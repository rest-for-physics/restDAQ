/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        clientUDP.c

 Description: A simple UDP client application for testing data acquisition
  from the reduced DCC through Fast Ethernet.


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
  February 2007: created
  July 2008 : added Major/Minor version and compilation date/time information
  August 2008: changed way to count events by detecting "isobus" commands that
   were recently added to replace "sca start"
  October 2008: added support for wreq command
  November 2008: added support for daq command
  June 2009: when the first 2 bytes of the UDP datagram received are null,
  these two bytes will be skipped. This allows to have the UDP payload aligned
  on 32-bit boundaries on the sender side. The client program support
  transparently the 2 types of servers: the ones that do not align UDP payload
  on 32-bit boundaries (Internet standard), and the customized version where
  the usable application data is offset by 2 bytes in a standard UDP datagram.

  September 2009: added capability to produce output data file in binary format
  compatible with DaqT2K

  October 2009: added capability to dump histogram of DCC busy time 

  December 2009: changed command "verbose" so that it can be executed inside
  a script and the initial value is restored when the scripts ends.
  Changed socket read timeout by time measurement instead of number of
  iterations. This makes timeout independant of platform speed

  January 2010: added option to suppress empty packets in file saved

  January 2010: implemented option to detect end of areq command by EORQ flag
  in DCC header

  January 2010: changed to version 2.0: support multiple DCC servers

  February 2010: data can be saved in Midas format

  March 2010: added comparison of data size received from each FEM at the end
  of data acquisition of the current event to the size given in the end of
  event packet. The verifications is needed to detect any missing packets
  when "daq" commands are used

  November 2010: increased UDP buffer size to receive 8K Ethernet frames

  March 2011: added Fec_Per_Fem global variable and corresponding command line
  option. This value must be passed to the function that prints the content
  of received data to convert properly {Arg1;Arg2} to {FEC; ASIC; Channel}

*******************************************************************************/
extern "C" { 
#include "platform_spec.h"
#include "sock_util.h"
#include "gblink.h"
#include "after.h"
#include "fem.h"
#include "ethpacket.h"
#include "endofevent_packet.h"
}

#include "TRestRawSignalEvent.h"
#include "TRestRawDAQMetadata.h"
#include "TRestRun.h"

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>

// For shared memory
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <chrono>

/*******************************************************************************
 Constants types and global variables
*******************************************************************************/

////////////////////////////////////////////////
// Manage Major/Minor version numbering manually
#define CLIENT_VERSION_MAJOR 2
#define CLIENT_VERSION_MINOR 3
char client_date[] = __DATE__;
char client_time[] = __TIME__;
////////////////////////////////////////////////


#define REMOTE_DST_PORT   1122
#define CMD_ARRAY_SIZE   25000
#define CMD_LINE_SIZE      120

#define SOCK_REV_SIZE   200*1024

#define RETRY_TIME_TO_TIMEOUT 10000000

#define MAX_FEM_EVENT_SIZE (6*4*79*2*(512+32))
#define MAX_EVENT_SIZE (12*MAX_FEM_EVENT_SIZE)

#define MAX_NUMBER_OF_DCCS    18
#define MAX_NB_OF_FEM         72
#define MAX_NB_OF_FEM_PER_DCC 12

char res_file[80] = {"dummyRun.acq"};

int rem_ip_beg[4] = {192, 168, 10, 13};
int rem_port      = REMOTE_DST_PORT;
int use_stdin     = 1;
int probe_ct_pkt  = 0;
int format_ver    = 2;
int event_cnt     = 0;
bool fileOpen     = false;

unsigned short Fec_Per_Fem   = 4;

unsigned char buf_rcv[8192];
unsigned char *buf_ual;

int startFEC=0;
int endFEC=5;
int nFECs=0;
int nASICs=4;
int chStart=3;
int chEnd =78;

char inputConfigFile[256];
TRestRun* restRun;
TRestRawDAQMetadata *daqMetadata;
TRestRawSignalEvent *fSignalEvent;
//TRestRawSignalEvent *fAnaEvent;

key_t key;
int *abrt = NULL;
int id_abrt;

// For events in other formats
typedef struct _Event {
	int ev_sz;
	int ev_nb;
	unsigned char *cur_ptr;
	unsigned char data[MAX_EVENT_SIZE];
	FILE *fout;
	int format;
	int compress;
} Event;

Event event;

typedef struct _DccSocket {
	int    client;
	struct sockaddr_in target;
	unsigned char* target_adr;
	
	struct sockaddr_in remote;
	int    remote_size;
	int    rem_port;
} DccSocket;

DccSocket dcc_socket;

const Double_t getCurentTime(){

return std::chrono::duration_cast <std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count()/1000000.0;

}

/*******************************************************************************
 Signal_Handler
*******************************************************************************/
void sig_handler(int signum){
  printf("Executing signal handler on signal %d\n",signum);
  *abrt = 1;
}


/*******************************************************************************
 Event_Save
*******************************************************************************/
int Event_Save(unsigned char *buf, int size)
{
	DataPacket *dp;

	// If data supplied, copy to temporary buffer
	if (size > 0) {
	  dp = (DataPacket *) buf;
	  int scnt = ntohs(dp->scnt);
	  if ( (scnt <=8) && (ntohs(dp->samp[0]) == 0) && (ntohs(dp->samp[1]) == 0) )return (-1);//empty data

	  unsigned short fec, asic, channel;
	  unsigned short arg1 = GET_RB_ARG1(ntohs(dp->args));
	  unsigned short arg2 = GET_RB_ARG2(ntohs(dp->args));
	  fec     = (10*(arg1%6)/2 + arg2)/4;
	  asic    = (10*(arg1%6)/2 + arg2)%4;
	  channel = arg1/6;
	    if ((fec > 5) || (asic > 3)){
	      fec = arg2 - 4;
	      asic = 4;
	      channel = (arg1 - 4) / 6;
	   }
	  
	  std::vector<Short_t> sData(512,0);
	  
	  int physChannel = -10;
        if (channel > 2 && channel < 15) {
            physChannel = channel - 3;
        } else if (channel > 15 && channel < 28) {
            physChannel = channel - 4;
        } else if (channel > 28 && channel < 53) {
            physChannel = channel - 5;
        } else if (channel > 53 && channel < 66) {
            physChannel = channel - 6;
        } else if (channel > 66) {
            physChannel = channel - 7;
        }

        if (physChannel < 0)return -1;

        physChannel = fec * 72 * 4 + asic * 72 + physChannel;

	  if (daqMetadata->GetVerboseLevel() >= REST_Debug)std::cout<<"FEC "<<fec<<" asic "<<asic<<" channel "<<channel<<" physChann "<<physChannel<<"\n";

	      unsigned short timeBin=0;
	      for(int i=0;i<scnt;i++){
	        unsigned short data = ntohs(dp->samp[i]);
	          if (((data & 0xFE00) >> 9) == 8) {
                    timeBin = GET_CELL_INDEX(data);
                  } else if ((((data & 0xF000) >> 12) == 0)) {
                    if(timeBin<512)sData[timeBin] = data;//TODO optimize using memcpy
                    timeBin++;
                  }
	      }
	   TRestRawSignal rawSignal (physChannel,sData);
	   fSignalEvent->AddSignal(rawSignal);
	}
    return(0);
}

/*******************************************************************************
 DccSocket_Clear()
*******************************************************************************/
void DccSocket_Clear(DccSocket *dccs)
{
	dccs->client        = 0;
	dccs->rem_port      = 0;
	dccs->remote_size   = 0;
	dccs->target_adr    = (unsigned char*) 0;
}

/*******************************************************************************
 DccSocket_Open()
*******************************************************************************/
int DccSocket_Open(DccSocket *dccs, int *rem_ip_base, int rpt)
{

	int err;
	int nb;
	int  rcvsz_req  = SOCK_REV_SIZE;
	int  rcvsz_done;
	unsigned int  optlen = sizeof(int);

	// Create client socket
	if ((dccs->client = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		err = socket_get_error();
		printf("DccSocket_Open: socket failed: error %d\n",err);
		fprintf(stderr, "socket() failed: %s\n", strerror(errno));
		return(-1);
	}

	// Set socket in non-blocking mode
	nb = 1;
	if ((err = ioctlsocket(dccs->client, FIONBIO, &nb)) != 0)
	{
		err = socket_get_error();
		printf("DccSocket_Open(): ioctlsocket failed: error %d\n", err);
		fprintf(stderr, "socket() failed: %s\n", strerror(errno));
		return(-1);
	}

	// Set receive socket size
	if ((err = setsockopt(dccs->client, SOL_SOCKET, SO_RCVBUF, (char *)&rcvsz_req, optlen)) != 0)
	{
		err = socket_get_error();
		printf("DccSocket_Open(): setsockopt failed: error %d\n", err);
		fprintf(stderr, "socket() failed: %s\n", strerror(errno));
		return(-1);
	}

       struct timeval timeout;      
       timeout.tv_sec = 10;
       timeout.tv_usec = 0;
    
       if (err = setsockopt (dccs->client, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0 ){
               err = socket_get_error();
		printf("DccSocket_Open(): setsockopt failed: error %d\n", err);
		fprintf(stderr, "socket() failed: %s\n", strerror(errno));
		return(-1);
	}

	// Get receive socket size
	if ((err = getsockopt(dccs->client, SOL_SOCKET, SO_RCVBUF, (char *)&rcvsz_done, &optlen)) != 0)
	{
		err = socket_get_error();
		printf("DccSocket_Open(): getsockopt failed: error %d\n", err);
		fprintf(stderr, "socket() failed: %s\n", strerror(errno));
		return(-1);
	}
	
	// Check receive socket size
	if (rcvsz_done < rcvsz_req)
	{
		printf("DccSocket_Open(): Warning: recv buffer size set to %d bytes while %d bytes were requested. Data losses may occur\n", rcvsz_done, rcvsz_req);
	}

	// Init target address
	dccs->rem_port          = rpt;
	dccs->target.sin_family = PF_INET;
	dccs->target.sin_port   = htons((unsigned short)dccs->rem_port);
	dccs->target_adr        = (unsigned char*) & (dccs->target.sin_addr.s_addr);
	dccs->target_adr[0]     = *(rem_ip_base+0);
	dccs->target_adr[1]     = *(rem_ip_base+1);
	dccs->target_adr[2]     = *(rem_ip_base+2);
	dccs->target_adr[3]     = *(rem_ip_base+3);
	dccs->remote_size       = sizeof(dccs->remote);

	return (0);
}

/*******************************************************************************
 DccSocket_Close()
*******************************************************************************/
void DccSocket_Close(DccSocket *dccs)
{
	if (dccs->client)
	{
		closesocket(dccs->client);
		dccs->client = 0;
	}
}

void getSharedMemory( ){

    // ftok to generate unique key
    if (key = ftok("/bin/ls",32) ==-1 ){
      perror("Error while creating shared memory (ftok)");
      exit(-1);
    }

    // shmget returns an identifier in shmid
    if (id_abrt = shmget(key, sizeof(int), 0777 | IPC_CREAT) == -1){
      perror("Error while creating shared memory (shmget)");
      exit(-1);
    }

    // shmat to attach to shared memory
    if( (abrt = (int *) shmat(id_abrt, NULL, 0) ) == (int *)(-1)){
      perror("Error while creating shared memory (shmat)");
      exit(-1);
    }

}

void cleanSharedMemory( ){

  printf("Cleaning shared memory...\n");
  //Write EXIT flag
  *abrt=1;
  //detach from shared memory
  shmdt((char *)abrt);
  //shmctl(id_abrt, IPC_STAT,(struct shmid_ds *)NULL);
  //shmctl(id_abrt, IPC_RMID,(struct shmid_ds *)NULL);
}

void abortRun(){
  printf("Stopping run\n");
  getSharedMemory();
  cleanSharedMemory();
}

/*******************************************************************************
 help() to display usage
*******************************************************************************/
void help()
{
	printf("clientUDP <options>\n");
	printf("   -h                : print this message help\n");
	printf("   -c <configFile>   : Set config file\n");
	printf("   -e                : Exit another running acquisition program\n");
}

/*******************************************************************************
 parse_cmd_args() to parse command line arguments
*******************************************************************************/
int parse_args(int argc, char **argv)
{
	for (int i=1;i<argc; i++)
	{
	        // remote host IP address
		if (strncmp(argv[i], "-c", 2) == 0)
		{       i++;
		        sscanf(argv[i],"%s",inputConfigFile);
		} else if (strncmp(argv[i], "-e", 2) == 0){
			abortRun();
			return (-1); // force an error to exit
		} else if (strncmp(argv[i], "-h", 2) == 0){
			help();
			return (-1); // force an error to exit
		}
		// unmatched options
		else {
			printf("Warning: unsupported option %s\n", argv[i]);
			help();
			return -1;
		}
	}
	return (0);
}

void cleanup(bool e){

  if (dcc_socket.client)
  DccSocket_Close(&dcc_socket);

  socket_cleanup();

  //Clean shared memory
  cleanSharedMemory();

    if(e){
      printf("Exiting...\n");
      exit(1);
    }
}

int GetPacket (int pckType) {

  // wait for incoming messages
  bool done = false;
  int cnt=0;
  int pckCnt=1;
  int errRep =1;
  struct timeval t_start, t_now;
  gettimeofday(&t_start, NULL);
  while (!done){
    int length = recvfrom(dcc_socket.client, &buf_rcv[0], 8192, 0, (struct sockaddr*)&(dcc_socket.remote), (unsigned int*)&(dcc_socket.remote_size));
    if (length < 0){
      int err = socket_get_error();
      if(cnt%1000==0)gettimeofday(&t_now, NULL);
      if (err == EWOULDBLOCK ||err == EAGAIN ){
        int tnts = (t_now.tv_sec - t_start.tv_sec)*1000000 + (t_now.tv_usec - t_start.tv_usec);
         if (daqMetadata->GetVerboseLevel() >= REST_Extreme)fprintf(stderr, "socket() failed: %s\n", strerror(errno));
          if(tnts < RETRY_TIME_TO_TIMEOUT ){
            continue;
          } else {
            printf("dcc().cmd(): no reply after %d us, missing packets are expected.\n", tnts);
            return -1;
          }
        cnt++;
      } else {
         fprintf(stderr, "socket() failed: %s\n", strerror(errno));
         cleanup(true);
         return -1;
      }
    } else {
      gettimeofday(&t_start, NULL);
    }

    // if the first 2 bytes are null, UDP datagram is aligned on next 32-bit boundary, so skip these first two bytes
      if ((buf_rcv[0] == 0) && (buf_rcv[1] == 0)) {
        buf_ual = &buf_rcv[2];
        length -=2;
      } else {
        buf_ual = &buf_rcv[0];
      }

      if ( (*buf_ual == '-') ) {//ERROR ASCII packet
        pckType = -1;
        if(daqMetadata->GetVerboseLevel() >= REST_Debug)printf("ERROR packet: %s\n", buf_ual);
        errRep = 0;
      }
      if(daqMetadata->GetVerboseLevel() >= REST_Debug){
        if ( pckType >=0 ) {
          printf("Binary packet: %X\n", buf_ual);
        } else {
          printf("ASCII packet: %s\n", buf_ual);
        }
      }
      
      if(pckType >= 0){//DAQ packet


        Event_Save(buf_ual, length);

        DataPacket *data_pkt = (DataPacket*) buf_ual;
        if(daqMetadata->GetVerboseLevel() >= REST_Debug)printf("Event saving packet %d for event %d, type %d\n", pckCnt, event_cnt, pckType);

        // Check End Of Event
          if (GET_FRAME_TY_V2(ntohs(data_pkt->dcchdr)) & FRAME_FLAG_EOEV ||
             GET_FRAME_TY_V2(ntohs(data_pkt->dcchdr)) & FRAME_FLAG_EORQ  ||
             ( pckType>0 && pckCnt>= pckType) ) {
             done = true;
           }

      } else {
        done = true;//ASCII Packet, check response?
      }

      pckCnt++;

      // show packet if desired
      if (daqMetadata->GetVerboseLevel() >= REST_Debug){
        printf("dcc().rep(): %d bytes of data \n",length);
	  if ( pckType >= 0 ){
	    DataPacket *data_pkt = (DataPacket*) buf_ual;
	    DataPacket_Print(data_pkt);
	  } else {
	    *(buf_ual+length) = '\0';
	    printf("dcc().rep(): %s", buf_ual);
	  }
      }
    }
  return errRep;
}

int sendCmd(const char *cmd, int pckType){

  if(daqMetadata->GetVerboseLevel() >= REST_Debug)printf("dcc().cmd(): command sent %s\n", cmd);

  if ( sendto(dcc_socket.client, cmd, strlen(cmd), 0, (struct sockaddr*)&(dcc_socket.target), sizeof(struct sockaddr) )  == -1){
	const int err = socket_get_error();
	fprintf(stderr, "socket() failed: %s\n", strerror(errno));
	cleanup(true);
	return -1;
  }

  return GetPacket(pckType);

}

void waitForTrigger(){//Wait till trigger is acquired
  int flag=0;
    do {
      flag = sendCmd("wait 1000000",-1);//Wait for the event to be acquired
    } while (flag ==0 && !(*abrt) ); //Infinite loop till aborted or wait succeed
}

void data_taking(  ){

printf("Starting data taking run\n");

  char cmd[200];
  sendCmd("fem 0",-1);//Needed?
  //if(comp)sendCmd("skipempty 1", -1);//Skip empty frames in compress mode
  //else sendCmd("skipempty 0", -1);//Save empty frames if not
  sendCmd("isobus 0x4F", -1);//Reset event counter, timestamp for type 11
 
    while ( !(*abrt) && (daqMetadata->GetNEvents() == 0 || event_cnt <daqMetadata->GetNEvents()) ){
      sendCmd("fem 0",-1);

      sendCmd("isobus 0x6C",-1);//SCA start
      if(daqMetadata->GetTriggerType()=="internal") sendCmd("isobus 0x1C",-1);//SCA stop case of internal trigger
      fSignalEvent->Initialize();
      fSignalEvent->SetID(event_cnt);
      waitForTrigger();
        //Perform data acquisition phase, compress, accept size
      fSignalEvent->SetTime(getCurentTime());
        for(int fecN=5;fecN>=0;fecN--){
          if(daqMetadata->GetFECMask() & (1 << fecN) )continue;
            for (int asic=0;asic<4;asic++){
              sprintf(cmd,"areq %d %d %d %d %d", daqMetadata->GetCompressMode(), fecN, asic,chStart, chEnd);
              sendCmd(cmd,0);
            }
        }
      
      restRun->GetEventTree()->Fill();
      restRun->GetAnalysisTree()->SetEventInfo( fSignalEvent);
      restRun->GetAnalysisTree()->Fill();
      event_cnt++;
    }

}


//Start a pedestal run
void pedestal( ){

  printf("Starting pedestal run\n");

  char cmd[200];
  sendCmd("fem 0",-1);
  sendCmd("pokes 0x14 0x0",-1);//Set SCA readback offset
  sendCmd("hbusy clr",-1);
  sendCmd("fem 0",-1);
  sprintf(cmd,"hped clr %d:%d * %d:%d",startFEC, endFEC, chStart, chEnd);
  sendCmd(cmd,-1);////Clear pedestals
  sendCmd("isobus 0x4F", -1);//Reset event counter, timestamp for type 10

  int loopN=0;
    while ( !(*abrt) && (loopN++ < 100) ){//100 loops hardcoded TODO change
      sendCmd("isobus 0x6C",-1);//SCA start
      sendCmd("isobus 0x1C",-1);//SCA stop
      waitForTrigger();
      sendCmd("fem 0",-1);
      sprintf(cmd,"hped acc %d:%d * %d:%d",startFEC, endFEC, chStart, chEnd);
      sendCmd(cmd,-1);//Get pedestals
    }

  sendCmd("fem 0",-1);
  sprintf(cmd,"hped getsummary %d:%d * %d:%d",startFEC, endFEC, chStart, chEnd);
  sendCmd(cmd,nFECs*nASICs);//Get summary
  sprintf(cmd,"hped centermean %d:%d * %d:%d 250",startFEC, endFEC, chStart, chEnd);
  sendCmd(cmd,-1);//Set mean
  sprintf(cmd,"hped setthr %d:%d * %d:%d 250 4.5",startFEC, endFEC, chStart, chEnd);
  sendCmd(cmd,-1);//Set threshold

}

void configure(){

  printf("Configuring readout\n");

  char cmd[200];
  sendCmd("fem 0",-1);//Needed?
  sendCmd("triglat 3 2000",-1);//Trigger latency
  sendCmd("pokes 0x8 0x0000",-1);//Disable pulser
  sendCmd("sca cnt 0x1ff",-1);//Set 511 bin count, make configurable?
  sprintf(cmd,"sca wck 0x%X",daqMetadata->GetClockDiv());//Clock div
  sendCmd(cmd,-1);  
  sendCmd("pokes 0x16 0x0",-1);//Set trigger delay
  sendCmd("pokes 0x1A 0x1ff",-1);//SCA delay and trigger
  if(daqMetadata->GetAcquisitionType() == "pedestal")sendCmd("pokes 0x14 0x400",-1);//Set SCA readback offset
  else sendCmd("pokes 0x14 0xc00",-1);//Set SCA readback offset
  int pk = 0xffc0 | daqMetadata->GetFECMask();
  sprintf(cmd,"pokeb 0x4 0x%X", pk);//Set FEC mask, note that is inverted
  sendCmd(cmd,-1);
  //sendCmd("pokeb 0x5 0x0",-1);//Deplecated?

  //Configure AFTER chip
  for(int fecN=5;fecN>=0;fecN--){
    if(daqMetadata->GetFECMask() & (1 << fecN) )continue;
    sprintf(cmd,"fec %d",fecN);
    sendCmd(cmd,-1);
      for (int asic=0;asic<4;asic++){
        unsigned int reg = ( (daqMetadata->GetShappingTime() & 0xF ) <<3) | ( (daqMetadata->GetGain() & 0x3) <<1 );
        sprintf(cmd,"asic %d write 1 0x%X",asic, reg);//Gain and shapping time
        sendCmd(cmd,-1);
        sprintf(cmd,"asic %d write 2 0xA000",asic);//Output buffers
        sendCmd(cmd,-1);
        sprintf(cmd,"asic %d write 3 0x3f 0xffff 0xffff",asic);
        sendCmd(cmd,-1);
        sprintf(cmd,"asic %d write 4 0x3f 0xffff 0xffff",asic);
        sendCmd(cmd,-1);
      }
  }
  //sendCmd("isobus 0x0F", -1);//Reset event counter, timestamp set eventType to test
}

/*******************************************************************************
 Main
*******************************************************************************/
int main(int argc, char **argv)
{
	int err;

	// Parse command line arguments
	if (parse_args(argc, argv) < 0)
	{
		return (-1);	
	}

    restRun = new TRestRun();
    restRun->LoadConfigFromFile(inputConfigFile);
    daqMetadata = new TRestRawDAQMetadata( inputConfigFile );
    daqMetadata->PrintMetadata();
    TString rTag = restRun->GetRunTag();
    
    if (rTag == "Null" || rTag == "") restRun->SetRunTag(daqMetadata->GetTitle());
    restRun->SetRunType(daqMetadata->GetAcquisitionType());

    restRun->AddMetadata(daqMetadata);
    //restRun->PrintMetadata();

    restRun->FormOutputFile();

    fSignalEvent = new TRestRawSignalEvent();
    restRun->AddEventBranch(fSignalEvent);

    //fAnaEvent = new TRestRawSignalEvent();
    restRun->SetStartTimeStamp(getCurentTime());

    restRun->PrintMetadata();
    daqMetadata->PrintMetadata();
    std::cout<<"Printed daq metadata"<<std::endl;

	// Initialize socket
	if ((err = socket_init()) < 0)
	{
		printf("sock_init failed: error %d\n", err);
		fprintf(stderr, "socket() failed: %s\n", strerror(errno));
		return (err);
	}

	// Open socket
	DccSocket_Clear(&dcc_socket);
	
	//for(int i=0;i<4;i++)rem_ip_beg[i] = daqMetadata->GetBaseIp()[i];
	std::cout<<rem_ip_beg[0]<<"."<<rem_ip_beg[1]<<"."<<rem_ip_beg[2]<<"."<<rem_ip_beg[3]<<std::endl;
	if ((err = DccSocket_Open(&dcc_socket, &rem_ip_beg[0], rem_port)) < 0)
	{
	        printf("DccSocket_Open failed, error %d\n", err);
	        fprintf(stderr, "socket() failed: %s\n", strerror(errno));
	        return -1;
	}

       std::cout<<"Socket opened"<<std::endl; 

	// Print setup
	if (daqMetadata->GetVerboseLevel() >= REST_Info)
	{
		printf("---------------------------------\n");
		printf("Client version    : %d.%d\n", CLIENT_VERSION_MAJOR, CLIENT_VERSION_MINOR);
		printf("Compiled          : %s at %s\n", client_date, client_time);
		printf("Remote server     : %d.%d.%d.%d:%d\n",
			dcc_socket.target_adr[0],
			dcc_socket.target_adr[1],
			dcc_socket.target_adr[2],
			dcc_socket.target_adr[3],
			dcc_socket.rem_port);
		printf("---------------------------------\n");
	}

        bool firstFEC=true;
          for(int fecN=5;fecN>=0;fecN--){
            if(daqMetadata->GetFECMask() & (1 << fecN) )continue;
              if(firstFEC)endFEC=fecN;
              firstFEC=false;
              startFEC = fecN;
              nFECs++;
          }

        printf("FECmask 0x%X, startFEC %d, endFEC %d, nFECs %d\n", daqMetadata->GetFECMask(), startFEC, endFEC, nFECs);

        //Install signal handler
        signal(SIGINT, sig_handler);
        signal(SIGQUIT, sig_handler);
        signal(SIGILL, sig_handler);
        signal(SIGTRAP, sig_handler);
        signal(SIGABRT, sig_handler);
        signal(SIGIOT, sig_handler);
        signal(SIGFPE, sig_handler);
        signal(SIGBUS, sig_handler);
        signal(SIGSEGV, sig_handler);
        signal(SIGKILL, sig_handler);
        signal(SIGPIPE, sig_handler);
        signal(SIGTERM, sig_handler);
        signal(SIGSTKFLT, sig_handler);
        signal(SIGSYS, sig_handler);

        getSharedMemory();
        *abrt =0;

        configure();

        if(daqMetadata->GetAcquisitionType() == "pedestal"){
          pedestal( );
        } else if (daqMetadata->GetAcquisitionType() == "background" || daqMetadata->GetAcquisitionType() == "calibration"){
          data_taking( );
        } else {
          std::cout<<"Unknown acquisition type "<<daqMetadata->GetAcquisitionType()<<" skipping"<<std::endl;
          std::cout<<"Valid acq types: pedestal, calibration or background"<<std::endl;
        }

        cleanup(false);
        restRun->SetEndTimeStamp(getCurentTime());
	TString Filename = restRun->GetOutputFileName();

        restRun->UpdateOutputFile();
        restRun->CloseFile();
        restRun->PrintMetadata();
        printf("Exiting...\n");
}


