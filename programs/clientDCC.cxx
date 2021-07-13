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
extern "C"{
#include "platform_spec.h"
#include "sock_util.h"
#include "gblink.h"
#include "after.h"
#include "fem.h"
#include "ethpacket.h"
#include "endofevent_packet.h"
}

#include <stdio.h>
#include <time.h>
#include <sys/time.h>

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

#define RETRY_TIME_TO_TIMEOUT 8000000

#define MAX_FEM_EVENT_SIZE (6*4*79*2*(512+32))
#define MAX_EVENT_SIZE (12*MAX_FEM_EVENT_SIZE)

#define MAX_NUMBER_OF_DCCS    18
#define MAX_NB_OF_FEM         72
#define MAX_NB_OF_FEM_PER_DCC 12

char snd[CMD_ARRAY_SIZE][CMD_LINE_SIZE] = {
	"LOOP 10000\n",
	"sca start\n", 
	"sca stop\n",
	"wait\n",
	"dreq 0 0 0 0 0 0\n",
	"dreq 0 0 0 0 1 0\n",
	"dreq 0 0 0 0 2 0\n",
	"dreq 0 0 0 0 3 0\n",
	"dreq 0 0 0 0 4 0\n",
	"dreq 0 0 0 0 5 0\n",
	"dreq 0 0 0 0 6 0\n",
	"dreq 0 0 0 0 7 0\n",
	"NEXT\n",
	"END\n"
}; 
int  cmd_cnt  = 14;

char cmd_file[80]  = {"\0"};
char res_file[80] = {"\0"};
int  save_res     = 0;
int rem_ip_beg[4] = {192, 168, 10, 12};
int rem_port      = REMOTE_DST_PORT;
int verbose       = 4;
int use_stdin     = 1;
int probe_ct_pkt  = 0;
int format_ver    = 2;
int pending_event = 0;
int comp          = 0;
int echo_cmd      = 1;
int fem_per_dcc   = 4;
unsigned short Fec_Per_Fem   = 6;

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

enum Event_Phase {EVENT_BEGIN, EVENT_CONT, EVENT_END};

typedef struct _DccSocket {
	int    dcc_id;
	int    client;
	struct sockaddr_in target;
	unsigned char* target_adr;
	
	struct sockaddr_in remote;
	socklen_t    remote_size;
	int    rem_port;
} DccSocket;

int server_set = 0x1; // Only DCC0 is active by default
DccSocket dcc_socket[MAX_NUMBER_OF_DCCS];

/*******************************************************************************
 Event_Initialize
*******************************************************************************/
int Event_Initialize(Event *ev, char *file_out, int format, int compress)
{
	struct tm *now;
	char   run_str[21];
	time_t start_time;
	char str_res[4];

	ev->format   = format;
	ev->compress = compress;

	// ASCII format
	if (ev->format == 0)
	{
		sprintf(str_res, "w");
	}
	// Binary format
	else
	{
		sprintf(str_res, "wb");
	}

	// Open result file
	if (!(ev->fout = fopen(file_out, str_res)))
	{
		printf("Event_Initialize: could not open file %s.\n", file_out);
		return(-1);
	}

	// ASCII format
	if (ev->format == 0)
	{

	}
	// DaqT2q format
	else if ((ev->format == 1) || (ev->format == 2))
	{
		// Get the time of start
		time(&start_time);
		now = localtime(&start_time);
		sprintf(run_str, "R%4d.%02d.%02d-%02d:%02d:%02d",
			((now->tm_year)+1900),
			((now->tm_mon)+1),
			now->tm_mday,
			now->tm_hour,
			now->tm_min,
			now->tm_sec);

		fwrite(run_str, 20, 1, ev->fout);
	}
	else
	{
		printf("Event_Initialize: unknown save format %d.\n", ev->format);
		return(-1);
	}

	return(0);
}

/*******************************************************************************
 Event_Save
*******************************************************************************/
int Event_Save(Event *ev, char *buf, int size, int type, int phase, int evnb)
{
	int i, j;
	int phase_match;
	int nb;
	unsigned short *us;
	DataPacket *dp;
	int keep_data;

	unsigned int t2ksubhdr;
	unsigned short dcchdr;
	unsigned short ix;

	phase_match = 0;

	// Save format is ASCII
	if (ev->format == 0)
	{
		// ASCII message
		if (type == 0)
		{
			fprintf(ev->fout, buf);
			return (0);
		}
		// Binary data is converted to ASCII
		else
		{
			
			us = (unsigned short *) buf;
			for (j=0; j<(size/2); j++)
			{
				fprintf(ev->fout, "%4x ", ntohs(*us));
				us++;
			}
			fprintf(ev->fout, "\n");
		}
		return(0);
	}

	// Skip ASCII messages when data are saved in a binary format (format 1, 2 and 3)
	if (type == 0)
	{
		return (0);
	}

	// Binary DaqT2k format
	if ((ev->format == 1) || (ev->format == 2))
	{
		// On start of event, initialize various fields
		if (phase == EVENT_BEGIN)
		{
			ev->ev_nb = evnb;
			ev->ev_sz = 8;
			ev->cur_ptr = &(ev->data[0]);
			phase_match = 1;
		}
	
		// If data supplied, copy to temporary buffer
		if (size > 0)
		{
			keep_data = 1;
			if (ev->compress)
			{
				dp = (DataPacket *) buf;
				if (ntohs(dp->samp[0]) == (CELL_INDEX_FLAG | 0x1FF))
				{
					keep_data = 0;
				}
				if ( (ntohs(dp->scnt) <=8) && (ntohs(dp->samp[0]) == 0) && (ntohs(dp->samp[1]) == 0) )
				{
					keep_data = 0;
				}
			}
			
			if (keep_data)
			{
				if ((ev->ev_sz+size) < MAX_EVENT_SIZE)
				{
					memcpy(ev->cur_ptr, buf, size);
					ev->cur_ptr+=size;
					ev->ev_sz+=size;
				}
				else
				{
					printf("Event_Save: event %d too large size=%d\n", ev->ev_nb, ev->ev_sz+size);
					return (-1);
				}
			}

			if (phase == EVENT_CONT)
			{
				phase_match = 1;
			}
		}
	}

	// When end of event, write complete event to file
	if (phase == EVENT_END)
	{
		// Binary DaqT2k format
		if ((ev->format == 1) || (ev->format == 2))
		{
			ev->ev_nb = evnb;
		
			// write size
			nb = htonl(ev->ev_sz);
			fwrite(&nb, 4, 1, ev->fout);

			// write event_number
			nb = htonl(ev->ev_nb);
			fwrite(&nb, 4, 1, ev->fout);

			// write data
			fwrite(ev->data, (ev->ev_sz -8), 1, ev->fout);

			//printf("Wrote event:%d size:%d\n", ev->ev_nb, ev->ev_sz);
		}

		phase_match = 1;
	}
				
	if (!phase_match)
	{
		printf("Event_Save: illegal argument phase: %d\n", phase);
		return (-1);
	}

	return(0);
}

/*******************************************************************************
 DccSocket_Clear()
*******************************************************************************/
void DccSocket_Clear(DccSocket *dccs)
{
	dccs->client        = 0;
	dccs->dcc_id        = -1;
	dccs->rem_port      = 0;
	dccs->remote_size   = 0;
	dccs->target_adr    = (unsigned char*) 0;
}

/*******************************************************************************
 DccSocket_Open()
*******************************************************************************/
int DccSocket_Open(DccSocket *dccs, int *rem_ip_base, int ix, int rpt)
{
	
	int err;
	int nb;
	int  rcvsz_req  = SOCK_REV_SIZE;
	int  rcvsz_done;
	socklen_t  optlen = sizeof(int);

	// Check arguments
	if ((ix <0) || (ix >= MAX_NUMBER_OF_DCCS))
	{
		printf("DccSocket_Open: invalid argument %d\n", ix);
		return(-1);
	}
	
	// Create client socket
	if ((dccs->client = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		err = socket_get_error();
		printf("DccSocket_Open(%d): socket failed: error %d\n", ix, err);
		return(-1);	
	}

	// Set socket in non-blocking mode
	nb = 1;
	if ((err = ioctlsocket(dccs->client, FIONBIO, &nb)) != 0)
	{
		err = socket_get_error();
		printf("DccSocket_Open(%d): ioctlsocket failed: error %d\n", ix, err);
		return(-1);
	}

	// Set receive socket size
	if ((err = setsockopt(dccs->client, SOL_SOCKET, SO_RCVBUF, (char *)&rcvsz_req, optlen)) != 0)
	{
		err = socket_get_error();
		printf("DccSocket_Open(%d): setsockopt failed: error %d\n", ix, err);
		return(-1);
	}

	// Get receive socket size
	if ((err = getsockopt(dccs->client, SOL_SOCKET, SO_RCVBUF, (char *)&rcvsz_done, &optlen)) != 0)
	{
		err = socket_get_error();
		printf("DccSocket_Open(%d): getsockopt failed: error %d\n", ix, err);
		return(-1);
	}
	
	// Check receive socket size
	if (rcvsz_done < rcvsz_req)
	{
		printf("DccSocket_Open(%d): Warning: recv buffer size set to %d bytes while %d bytes were requested. Data losses may occur\n", rcvsz_done, rcvsz_req);
	}

	// Init target address
	dccs->rem_port          = rpt;
	dccs->target.sin_family = PF_INET;
	dccs->target.sin_port   = htons((unsigned short)dccs->rem_port);
	dccs->target_adr        = (unsigned char*) &(dccs->target.sin_addr.s_addr);
	dccs->target_adr[0]     = *(rem_ip_base+0);
	dccs->target_adr[1]     = *(rem_ip_base+1);
	dccs->target_adr[2]     = *(rem_ip_base+2);
	dccs->target_adr[3]     = *(rem_ip_base+3) + ix;
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

/*******************************************************************************
 parse_cmd_file() to get commands from a file
*******************************************************************************/
int parse_cmd_file()
{
	FILE *fptr;
	int ix = 0;
	int err = 0;
	int sl_comment, ml_comment, ml_cnext;

	if (!(fptr = fopen(cmd_file, "r")))
	{
		printf("could not open file %s.\n", cmd_file);
		return(-1);
	}

	// scan all lines
	cmd_cnt    = 0;
	ml_comment = 0;
	ml_cnext   = 0;
	while (fgets(snd[ix], CMD_LINE_SIZE, fptr))
	{
		// check for single or multi-line comments
		sl_comment = 0;
		ml_cnext   = ml_comment;
		if (snd[ix][0] == '/')
		{
			if (snd[ix][1] == '/')
			{
				sl_comment = 1;
			}
			if (snd[ix][1] == '*')
			{
				ml_comment = 1;
			}
		}
		if ( (snd[ix][0] == '*') && (snd[ix][1] == '/') )
		{
			ml_comment = 0;
		}
		
		// accept command line unless commented, empty line, etc
		if ( (snd[ix][0] != '\n') &&
			(snd[ix][0] != ' ') &&
			(snd[ix][0] != '\t') &&
			(snd[ix][0] != '#') &&
			(sl_comment == 0) &&
			(ml_comment == 0) &&
			(ml_cnext == 0)
			)
		{
			if (ix < (CMD_ARRAY_SIZE-2))
			{
				cmd_cnt++;
				ix++;
			}
			else
			{
				printf("maximum command count (%d) reached.\n", cmd_cnt);
				err = -1;
				break;
			}
		}
	}

	// Check for unterminated commented section
	if (ml_comment)
	{
		printf("unterminated commented section. Use \"*/\" at beginning of line.\n");
		err = -1;
	}

	// Always append a end command in case the user forgot it
	sprintf(snd[ix], "END\n");
	cmd_cnt++;
	fclose(fptr);
	return (err);
}

/*******************************************************************************
 help() to display usage
*******************************************************************************/
void help()
{
	printf("clientUDP <options>\n");
	printf("   -h               : print this message help\n");
	printf("   -s <xx.yy.zz.ww> : base IP address of remote server(s) in dotted decimal\n");
	printf("   -p <port>        : remote UDP target port\n");
	printf("   -S <0xServer_set>: hexadecimal pattern to tell which server(s) to connect to\n");
	printf("   -T               : Tokai setup, equivalent to -s 192.168.10.10 -S 0x3FFFF\n");
	printf("   -f <file>        : read commands from file specified\n");
	printf("   -F <version>     : Format: 0:ASCII  1:binary V1.x  2:binary V2.x \n");
	printf("   -Z               : Suppress packets that have zero data\n");
	printf("   -o <file>        : save results in file specified\n");
	printf("   -W <cnt>         : show data packets if more than <cnt> samples are identical (for debug)\n");
	printf("   -D <fem_per_dcc> : Number of FEMs per DCC\n");
	printf("   -E <fec_per_fem> : Number of FECs per FEM\n");
	printf("   -v <level>       : verbose\n");
	printf("   -ne              : no-echo of commands\n");
}

/*******************************************************************************
 parse_cmd_args() to parse command line arguments
*******************************************************************************/
int parse_cmd_args(int argc, char **argv)
{
	int i;
	int match;
	int err = 0;

	for (i=1;i<argc; i++)
	{
		match = 0;
		// remote host IP address
		if (strncmp(argv[i], "-s", 2) == 0)
		{
			match = 1;
			if ((i+1) < argc)
			{
				i++;
				if (sscanf(argv[i],"%d.%d.%d.%d", &rem_ip_beg[0], &rem_ip_beg[1], &rem_ip_beg[2], &rem_ip_beg[3]) == 4)
				{

				}
				else
				{
					printf("illegal argument %s\n", argv[i]);
					return (-1);
				}
			}
			else
			{
				printf("missing argument after %s\n", argv[i]);
				return (-1);
			}
		}
		// remote port number
		else if (strncmp(argv[i], "-p", 2) == 0)
		{
			match = 1;
			if ((i+1) < argc)
			{
				i++;
				if (sscanf(argv[i],"%d", &rem_port) == 1)
				{

				}
				else
				{
					printf("illegal argument %s\n", argv[i]);
					return (-1);
				}
			}
			else
			{
				printf("missing argument after %s\n", argv[i]);
				return (-1);
			}
		}
		// Server pattern
		else if (strncmp(argv[i], "-S", 2) == 0)
		{
			match = 1;
			if ((i+1) < argc)
			{
				i++;
				if (sscanf(argv[i],"0x%x", &server_set) == 1)
				{
					server_set &= ((1<<MAX_NUMBER_OF_DCCS)-1);
				}
				else
				{
					printf("illegal argument %s\n", argv[i]);
					return (-1);
				}
			}
			else
			{
				printf("missing argument after %s\n", argv[i]);
				return (-1);
			}
		}
		// Tokai setup
		else if (strncmp(argv[i], "-T", 2) == 0)
		{
			match = 1;
			server_set = 0x3FFFF;
			rem_ip_beg[0] = 192;
			rem_ip_beg[1] = 168;
			rem_ip_beg[2] = 10;
			rem_ip_beg[3] = 10;	
		}
		// get file name for commands 
		else if (strncmp(argv[i], "-f", 2) == 0)
		{
			match = 1;
			if ((i+1) < argc)
			{
				i++;
				strcpy(cmd_file, argv[i]);
				if ((err = parse_cmd_file()) < 0)
				{
					return (err);
				}
				use_stdin = 0;
			}
			else
			{
				printf("missing argument after %s\n", argv[i]);
				return (-1);
			}
		}
		// result file name
		else if (strncmp(argv[i], "-o", 2) == 0)
		{
			match = 1;
			if ((i+1) < argc)
			{
				i++;
				strcpy(res_file, argv[i]);
				save_res = 1;
			}
			else
			{
				printf("missing argument after %s\n", argv[i]);
				return (-1);
			}
		}
		// probe packets that contain constant data
		else if (strncmp(argv[i], "-W", 2) == 0)
		{
			match = 1;
			if ((i+1) < argc)
			{
				if (sscanf(argv[i+1],"%d", &probe_ct_pkt) == 1)
				{
					i++;
				}
				else
				{
					probe_ct_pkt = 0;
				}
			}
			else
			{
				probe_ct_pkt = 0;
			}
			
		}
		// format for saving data
		else if (strncmp(argv[i], "-F", 2) == 0)
		{
			match    = 1;
			if ((i+1) < argc)
			{
				if (sscanf(argv[i+1],"%d", &format_ver) == 1)
				{
					i++;
				}
				else
				{
					format_ver = 2;
				}
			}
			else
			{
				format_ver = 2;
			}
		}
		// FEM count per DCC
		else if (strncmp(argv[i], "-D", 2) == 0)
		{
			match    = 1;
			if ((i+1) < argc)
			{
				if (sscanf(argv[i+1],"%d", &fem_per_dcc) == 1)
				{
					i++;
				}
				else
				{
					fem_per_dcc = 4;
				}
			}
			else
			{
				fem_per_dcc = 4;
			}
		}
		// FEC count per FEM
		else if (strncmp(argv[i], "-E", 2) == 0)
		{
			match    = 1;
			if ((i+1) < argc)
			{
				if (sscanf(argv[i+1],"%d", &Fec_Per_Fem) == 1)
				{
					i++;
				}
				else
				{
					Fec_Per_Fem = 6;
				}
			}
			else
			{
				Fec_Per_Fem = 6;
			}
		}
		// verbose
		else if (strncmp(argv[i], "-v", 2) == 0)
		{
			match = 1;
			if ((i+1) < argc)
			{
				if (sscanf(argv[i+1],"%d", &verbose) == 1)
				{
					i++;
				}
				else
				{
					verbose = 1;
				}
			}
			else
			{
				verbose = 1;
			}
			
		}
		// suppress packets with zero data
		else if (strncmp(argv[i], "-Z", 2) == 0)
		{
			match = 1;
			comp  = 1;
		}
		// no-echo of commands
		else if (strncmp(argv[i], "-ne", 3) == 0)
		{
			match    = 1;
			echo_cmd = 0;	
		}
		// help
		else if (strncmp(argv[i], "-h", 2) == 0)
		{
			match = 1;
			help();
			return (-1); // force an error to exit
		}
		// unmatched options
		if (match == 0)
		{
			printf("Warning: unsupported option %s\n", argv[i]);
		}
	}
	return (0);

}

/*******************************************************************************
 Main
*******************************************************************************/
int main(int argc, char **argv)
{
	int err;
	int length;
	int rd_timeout;

	int i,j;
	int cmd_ix;
	int cmd_retry;
	int rep_lost;

	int dcc_cmd_cnt[MAX_NUMBER_OF_DCCS];

	char buf_rcv[8192];
	char *buf_ual;

	int done;

	int loop_max;
	int loop_ix;
	int cmd_loop_ix;
	int cmd_sub_ix;

	int nxt_is_bin;
	int is_bin;
	int is_daq;
	int is_daq_first_req;
	int is_areq;
	int exp_pkt;
	int no_err;

	char exp_pkt_str[40];

	int cmd_failed;

	int param[10];

	char *cmd;
	char cmd_stdin[80];
	int alldone;
	int cmd_stdin_done;
	int event_cnt;

	DataPacket *data_pkt;
	int synch_fail_cnt;
	int los_cnt;

	int ct_fnd;
	int ct_loop;
	int ct_cnt;

	unsigned int fec_span, asic_span, cha_span;

	char msg[120];
	char str[4][16];

	int saved_verbose = 4;
	struct timeval t_start, t_now;
	int tnts;

	int mask;
	int cur_dcc;

	EndOfEventPacket *eop;
	unsigned int daq_data_size[MAX_NB_OF_FEM_PER_DCC];

	// Parse command line arguments
	if (parse_cmd_args(argc, argv) < 0)
	{
		return (-1);	
	}

	// Initialize socket
	if ((err = socket_init()) < 0)
	{
		printf("sock_init failed: error %d\n", err);
		return (err);
	}

	// Open socket for each DCC present
	mask    = 0x1;
	done    = 0;
	cur_dcc = 0;
	for (i=0; i<MAX_NUMBER_OF_DCCS; i++)
	{
		DccSocket_Clear(&dcc_socket[i]);
		if (server_set & mask)
		{
			if ((err = DccSocket_Open(&dcc_socket[i], &rem_ip_beg[0], i, rem_port)) < 0)
			{
				printf("DccSocket_Open failed for DCC %d error %d\n", i, err);
				goto cleanup;
			}
			else
			{
				if (!done)
				{
					cur_dcc = i;
					done = 1;
				}
			}
		}
		mask <<=1;

		dcc_cmd_cnt[i] = 0;
	}

	// Print setup
	if (verbose)
	{
		printf("---------------------------------\n");
		printf("Client version    : %d.%d\n", CLIENT_VERSION_MAJOR, CLIENT_VERSION_MINOR);
		printf("Compiled          : %s at %s\n", client_date, client_time);
		mask = 0x1;
		for (i=0; i<MAX_NUMBER_OF_DCCS; i++)
		{
			if (server_set & mask)
			{
				printf("Remote server %2d  : %d.%d.%d.%d:%d\n",
					i,
					dcc_socket[i].target_adr[0],
					dcc_socket[i].target_adr[1],
					dcc_socket[i].target_adr[2],
					dcc_socket[i].target_adr[3],
					dcc_socket[i].rem_port);
			}
			mask <<=1;
		}

		if (use_stdin)
		{
			printf("Commands          : from stdin\n");
		}
		else
		{
			if (cmd_file[0] != '\0')
			{
				printf("Command file name : %s\n", cmd_file);
			}
			printf("Command count     : %d\n", cmd_cnt);
			if (echo_cmd)
			{
				printf("Command list      :\n");
				for (i=0;i<cmd_cnt; i++)
				{
					printf("   %s", snd[i]);
				}
			}
		}
		
		printf("---------------------------------\n");
	}

	// various init
	cmd         = &snd[0][0];
	cmd_ix      = 0;
	cmd_sub_ix  = 0;
	cmd_retry   = 0;
	rep_lost    = 0;
	loop_max    = 0;
	loop_ix     = 0;
	cmd_loop_ix = 0;
	i           = 0;
	nxt_is_bin  = 0;
	is_bin      = 0;
	is_daq      = 0;
	is_areq     = 0;
	cmd_failed  = 0;
	exp_pkt     = 1;
	alldone     = 0;
	event_cnt   = 0;
	synch_fail_cnt = 0;
	los_cnt        = 0;
	ct_cnt         = 0;

	for (j=0; j<MAX_NB_OF_FEM_PER_DCC; j++)
	{
		daq_data_size[j] = 0;
	}

	// Save result if desired
	if (save_res)
	{
		Event_Initialize(&event, res_file, format_ver, comp);
	}
	else
	{
		event.fout = (FILE*) 0;
	}

	// Execute command script
	while (!alldone)
	{
		// prepare index of command
		if (!use_stdin)
		{
			if (strncmp(&snd[cmd_ix][0], "LOOP", 4) == 0)
			{
				if (loop_max == 0)
				{
					if (sscanf(&snd[cmd_ix][0], "LOOP %d", &loop_max) == 1)
					{
						loop_ix = 1;
						cmd_loop_ix = cmd_ix+1;
					}
					else
					{
						printf("syntax error in command[%d].\n", cmd_ix);
						err = -1;
						break;
					}
				}
				cmd_ix++;
			}
			else if (strncmp(&snd[cmd_ix][0], "NEXT", 4) == 0)
			{
				if (loop_ix < loop_max)
				{
					cmd_ix = cmd_loop_ix;
					loop_ix++;
				}
				else
				{
					loop_max = 0;
					cmd_ix++;
				}
			}

			// Check for end of sequence
			if (strncmp(&snd[cmd_ix][0], "END", 3) == 0)
			{
				use_stdin = 1;
				verbose = saved_verbose;
			}

			// Do not exceed size of command list
			if (cmd_ix >= cmd_cnt)
			{
				cmd_ix = (cmd_cnt-1);
			}
			cmd = &snd[cmd_ix][0];
		}

		// use stdin for commands
		if (use_stdin)
		{
			cmd_stdin_done = 0;
			while (!cmd_stdin_done)
			{
				printf("(%d) >", i);
				if(fgets(&cmd_stdin[0],80, stdin) < 0)
				{
					printf("failed to get input from stdin\n", err);
					goto cleanup;
				}
				// check for exec command
				if (strncmp(cmd_stdin, "exec ", 5) == 0)
				{
					sscanf(&cmd_stdin[5], "%s", cmd_file);
					if (parse_cmd_file() < 0)
					{
						printf("command %s failed\n", &cmd_stdin[0]);
						cmd_ix++;
					}
					else
					{
						cmd = &snd[0][0];
						cmd_ix = 0;
						cmd_stdin_done = 1;
						use_stdin = 0;
						saved_verbose = verbose;
					}
					
				}
				else
				{
					cmd = &cmd_stdin[0];
					cmd_stdin_done = 1;
				}
			}
		}

		if ((verbose > 1) && echo_cmd)
		{
			printf("dcc(%02d).cmd(%d): %s", cur_dcc, dcc_cmd_cnt[cur_dcc], cmd);
		}

		// save command if needed
		if (save_res)
		{
			sprintf(msg, "dcc(%02d).cmd(%d): %s", cur_dcc, dcc_cmd_cnt[cur_dcc], cmd);
			Event_Save(&event, &msg[0], strlen(msg), 0, EVENT_CONT, event_cnt);
		}

		// check for commands that apply to local client
		if (strncmp(cmd, "exit", 4) == 0)
		{
			goto cleanup;
		}
		else if (strncmp(cmd, "quit", 4) == 0)
		{
			alldone = 1;
		}
		else if (strncmp(cmd, "version", 7) == 0)
		{
			printf("Client Version %d.%d Compiled %s at %s\n", CLIENT_VERSION_MAJOR, CLIENT_VERSION_MINOR, client_date, client_time);
		}

		done       = 0;
		// see how many response packets should come back
		if (strncmp(cmd, "dreq", 4) == 0)
		{
			exp_pkt = 1;
			is_bin  = 1;
			is_daq  = 0;
			is_areq = 0;
			sprintf(exp_pkt_str, "%d", exp_pkt);
		}
		else if (strncmp(cmd, "areq", 4) == 0)
		{
			if ((err=sscanf(cmd, "areq %d %d %d %d %d", &param[0], &param[1], &param[2], &param[3] , &param[4])) == 5)
			{
				exp_pkt = 1*(param[4] - param[3] + 1);
				sprintf(exp_pkt_str, "%d", exp_pkt);
			}
			else
			{
				printf("Syntax error in command %s. Expected 5 arguments but got %d\n", cmd, err);
				exp_pkt = 1;
				sprintf(exp_pkt_str, "%d", exp_pkt);
			}
			is_bin  = 1;
			is_daq  = 0;
			is_areq = 1;
		}
		else if (strncmp(cmd, "wreq", 4) == 0)
		{
			if ((err=sscanf(cmd, "wreq %d %d %d %d %d", &param[0], &param[1], &param[2], &param[3] , &param[4])) == 5)
			{
				// Prevent negative size window
				if (param[1] > param[3])
				{
					param[3] = param[1];
				}
				if (param[2] > param[4])
				{
					param[4] = param[2];
				}
				// Compute size of window
				exp_pkt = (param[3] - param[1] + 1) * (param[4] - param[2] + 1);
				sprintf(exp_pkt_str, "%d", exp_pkt);
			}
			else
			{
				printf("Syntax error in command %s. Expected 5 arguments but got %d\n", cmd, err);
				exp_pkt = 1;
				sprintf(exp_pkt_str, "%d", exp_pkt);
			}
			is_bin  = 1;
			is_daq  = 0;
			is_areq = 0;
		}
		else if (strncmp(cmd, "daq 0", 5) == 0)
		{
			exp_pkt = 1;
			is_bin  = 1;
			is_daq  = 1;
			is_areq = 0;
			is_daq_first_req = 1;
			sprintf(exp_pkt_str, "?");
			for (j=0; j<MAX_NB_OF_FEM_PER_DCC; j++)
			{
				daq_data_size[j] = 0;
			}
		}
		else if (strncmp(cmd, "daq 1", 5) == 0)
		{
			exp_pkt = 1;
			is_bin  = 1;
			is_daq  = 1;
			is_areq = 0;
			is_daq_first_req = 0;
			sprintf(exp_pkt_str, "?");
		}
		else if (strncmp(cmd, "hbusy get", 9) == 0)
		{
			exp_pkt = 1;
			is_bin  = 1;
			is_daq  = 0;
			is_areq = 0;
			is_daq_first_req = 0;
			sprintf(exp_pkt_str, "%d", exp_pkt);
		}
		else if (strncmp(cmd, "hped", 4) == 0)
		{
			// Scan arguments
			if (sscanf(cmd, "hped %s %s %s %s", &str[0][0], &str[1][0], &str[2][0], &str[3][0]) == 4)
			{
				// Wildcard on FEC?
				if (str[1][0] == '*')
				{
					fec_span = MAX_NB_OF_FEC_PER_FEM;
				}
				else
				{
					// Scan FEC index range
					if (sscanf(&str[1][0], "%d:%d", &param[0], &param[1]) == 2)
					{
						if (param[0] < param[1])
						{
							fec_span = param[1] - param[0] + 1;
						}
						else
						{
							fec_span = param[0] - param[1] + 1;
						}
					}
					else
					{
						fec_span = 1;
					}
				}
		
				// Wildcard on ASIC?
				if (str[2][0] == '*')
				{
					asic_span = MAX_NB_OF_ASIC_PER_FEC;
				}
				else
				{
					// Scan ASIC index range
					if (sscanf(&str[2][0], "%d:%d", &param[0], &param[1]) == 2)
					{
						if (param[0] < param[1])
						{
							asic_span = param[1] - param[0] + 1;
						}
						else
						{
							asic_span = param[0] - param[1] + 1;
						}
					}
					else
					{
						asic_span = 1;
					}
				}
					
		
				// Wildcard on Channel?
				if (str[3][0] == '*')
				{
					cha_span = ASIC_MAX_CHAN_INDEX+1;
				}
				else
				{
					// Scan Channel index range
					if (sscanf(&str[3][0], "%d:%d", &param[0], &param[1]) == 2)
					{
						if (param[0] < param[1])
						{
							cha_span = param[1] - param[0] + 1;
						}
						else
						{
							cha_span = param[0] - param[1] + 1;
						}
					}
					else
					{
						cha_span = 1;
					}
				}
			}
			else
			{
				
			}

			// Command hped getsummary produces binary packets
			if (strncmp(&(str[0][0]), "getsummary", 10) == 0)
			{
				exp_pkt = fec_span * asic_span;
				is_bin  = 1;
				is_daq  = 0;
				is_daq_first_req = 0;
				sprintf(exp_pkt_str, "%d", exp_pkt);
			}
			// Command hped getbins and getmath produce binary packets
			else if (strncmp(&(str[0][0]), "get", 3) == 0)
			{
				exp_pkt = fec_span * asic_span * cha_span;
				is_bin  = 1;
				is_daq  = 0;
				is_daq_first_req = 0;
				sprintf(exp_pkt_str, "%d", exp_pkt);
			}
			// Command hped acc and clr return single ASCII response 
			else
			{
				exp_pkt = 1;
				is_bin  = 0;
				is_daq  = 0;
				is_areq = 0;
				is_daq_first_req = 0;
				sprintf(exp_pkt_str, "%d", exp_pkt);
			}
		}
		else if (strncmp(cmd, "verbose", 7) == 0)
		{
			done = 1;
			cmd_ix++;
			if (sscanf(cmd, "verbose %d\n", &verbose) == 1)
			{

			}
			if (verbose > 1)
			{
				printf("dcc(%02d).rep(%d): verbose = %d\n", cur_dcc, dcc_cmd_cnt[cur_dcc], verbose);
			}

		}
		else if ((strncmp(cmd, "dcc ", 4) == 0) || (strncmp(cmd, "dcc\n", 4) == 0))
		{
			done = 1;
			cmd_ix++;
			if (sscanf(cmd, "dcc %d\n", &mask) == 1)
			{
				if (server_set & (1<<mask))
				{
					cur_dcc = mask;
				}
				else
				{
					printf("dcc(%02d).rep(%d): Warning DCC %d does not exist.\n", cur_dcc, dcc_cmd_cnt[cur_dcc], mask);
				}
			}
			if (verbose > 1)
			{
				printf("dcc(%02d).rep(%d): command apply to DCC %d\n", cur_dcc, dcc_cmd_cnt[cur_dcc], cur_dcc);
			}

		}
		else
		{
			exp_pkt = 1;
			is_bin  = 0;
			is_daq  = 0;
			is_areq = 0;
			sprintf(exp_pkt_str, "%d", exp_pkt);
		}

		// Make event count (accurate to + or - 1 event) by counting "sca start" commands or the equivalent "isobus" type 
		// the stop may come by external hardware, so it cannot be counted easily
		if ((strncmp(cmd, "sca start", 8) == 0) || (strncmp(cmd, "isobus 0x20", 11) == 0) || (strncmp(cmd, "isobus 0x60", 11) == 0) || (strncmp(cmd, "pulser go", 9) == 0))
		{
			// When results are saved to a file in binary format, put event count and size for compatibility with DaqT2k format
			if (save_res)
			{
				// Save previous event if any
				if (pending_event)
				{
					Event_Save(&event, (char *) 0, 0, 1, EVENT_END, event_cnt);
				}
				// Begin new event
				Event_Save(&event, (char *) 0, 0, 1, EVENT_BEGIN, event_cnt);
				pending_event = 1;
			}
			event_cnt++;
		}

		// send current command if needed
		if (!done)
		{
			if (cmd_retry < 10)
			{
				if ((err = sendto(dcc_socket[cur_dcc].client, cmd, strlen(cmd),
					0, (struct sockaddr*)&(dcc_socket[cur_dcc].target), sizeof(struct sockaddr))) == -1)
				{
					err = socket_get_error();
					printf("dcc(%02d): sendto failed: error %d\n", cur_dcc, err);
					goto cleanup;
				}
			}
			else
			{
				printf("dcc(%02d): failed to receive reply for command %d after %d retry.\n", cur_dcc, dcc_cmd_cnt[cur_dcc], cmd_retry);
				err = -1;
				break;
			}
		}

		// wait for incoming messages
		rd_timeout = 0;
		cmd_sub_ix = 0;
		while(!done)
		{
			length = recvfrom(dcc_socket[cur_dcc].client, &buf_rcv[0], 8192, 0, (struct sockaddr*)&(dcc_socket[cur_dcc].remote), &(dcc_socket[cur_dcc].remote_size));
			if (length < 0)
			{
				err = socket_get_error();
				if (err == EWOULDBLOCK)
				{
					tnts = 0;
					if (rd_timeout == 0)
					{
						gettimeofday(&t_start, NULL);	
					}
					else if ((rd_timeout%1000) == 0)
					{
						gettimeofday(&t_now, NULL);
						tnts = (t_now.tv_sec - t_start.tv_sec)*1000000 + (t_now.tv_usec - t_start.tv_usec);
					}
					rd_timeout++;
					yield();
					if (tnts >= RETRY_TIME_TO_TIMEOUT)
					{
						done = 1;
						cmd_retry++;
						rep_lost++;
						if (verbose)
						{
							printf("dcc(%02d).cmd(%d): %s had no reply after %d ms (missing %s packets). retry %d\n", cur_dcc, dcc_cmd_cnt[cur_dcc], cmd, (tnts/1000), exp_pkt_str, cmd_retry);
						}
					}
				}
				else
				{
					printf("dcc(%02d): recvfrom failed: error %d\n", cur_dcc, err);
					goto cleanup;
				}
			}
			else
			{
				// if the first 2 bytes are null, UDP datagram is aligned on next 32-bit boundary, so skip these first two bytes
				if ((buf_rcv[0] == 0) && (buf_rcv[1] == 0))
				{
					buf_ual = &buf_rcv[2];
					length -=2;
				}
				else
				{
					buf_ual = &buf_rcv[0];
				}
				
				cmd_retry = 0;
				rd_timeout  = 0;

				if (is_bin)
				{
					if (*buf_ual == '-')
					{
						cmd_failed++;
						no_err = 0;
						*(buf_ual+length) = '\0';
						printf("dcc(%02d): Warning: cmd(%d) %s failed: %s\n", cur_dcc, dcc_cmd_cnt[cur_dcc], cmd, buf_ual);
					}
					else
					{
						no_err = 1;
					}
				}
				else
				{
					// check command error status
					if (strncmp(buf_ual, "-", 1) == 0)
					{
						cmd_failed++;
						no_err = 0;
						*(buf_ual+length) = '\0';
						printf("dcc(%02d): Warning: cmd(%d) %s failed: %s\n", cur_dcc, dcc_cmd_cnt[cur_dcc], cmd, buf_ual);
					}
					else if (strncmp(buf_ual, "0", 1) == 0)
					{
						no_err = 1;
					}
					else
					{
						printf("dcc(%02d): Warning: cmd(%d) %s failed: received non ASCII packet.\n", cur_dcc, dcc_cmd_cnt[cur_dcc], cmd);
						DataPacket_Print((DataPacket*) buf_ual);
						printf("Flushing UDP queue:\n");
						do
						{
							length = recvfrom(dcc_socket[cur_dcc].client, &buf_rcv[0], 4096, 0, (struct sockaddr*)&(dcc_socket[cur_dcc].remote), &(dcc_socket[cur_dcc].remote_size));
							if (length > 0)
							{
								// if the first 2 bytes are null, UDP datagram is aligned on next 32-bit boundary, so skip these first two bytes
								if ((buf_rcv[0] == 0) && (buf_rcv[1] == 0))
								{
									buf_ual = &buf_rcv[2];
									length -=2;
								}
								else
								{
									buf_ual = &buf_rcv[0];
								}

								if  ((*buf_ual == '0') || (*buf_ual == '-'))
								{
									*(buf_ual+length) = '\0';
									printf("ASCII packet: %s\n", buf_ual);
								}
								else
								{
									printf("Binary packet:\n");
									DataPacket_Print((DataPacket*) buf_ual);
								}
							}
						} while (length > 0);
						goto cleanup;
					}
				}

				
				// do we expect more packets for that command?
				if (is_daq)
				{
					if ((is_bin) && (no_err))
					{
						// Check End Of Event
						data_pkt = (DataPacket*) buf_ual;
						if (GET_FRAME_TY_V2(ntohs(data_pkt->dcchdr)) & FRAME_FLAG_EOEV)
						{
							// Ckeck event size
							eop = (EndOfEventPacket *) buf_ual;
							for (j=0; j<MAX_NB_OF_FEM_PER_DCC; j++)
							{
								if (ntohl(eop->byte_snd[j]) != daq_data_size[j])
								{
									printf("dcc(%02d): Warning: DCC sent %d bytes for FEM(%d) but %d were received by PC.\n",
									cur_dcc,
									ntohl(eop->byte_snd[j]),
									j,
									daq_data_size[j]);
								}
							}
							done = 1;
							cmd_ix++;
							if (is_daq_first_req)
							{
								cmd_ix++;
							}
						}
						// Check End Of Request
						else if (GET_FRAME_TY_V2(ntohs(data_pkt->dcchdr)) & FRAME_FLAG_EORQ)
						{
							done = 1;
							if (is_daq_first_req)
							{
								cmd_ix++;
							}
						}
						// Compute size of data received from this FEM for current event
						daq_data_size[GET_FEM_INDEX(ntohs(data_pkt->dcchdr))] += length;
					}
					else
					{
						printf("dcc(%02d): Warning: daq received non binary packet\n", cur_dcc);
						done = 1;
						cmd_ix++;
					}
				}
				else
				{
					exp_pkt--;
					
					if ((is_areq) &&(is_bin) && (no_err) && (exp_pkt))
					{
						data_pkt = (DataPacket*) buf_ual;
						// Check End Of Request
						if (GET_FRAME_TY_V2(ntohs(data_pkt->dcchdr)) & FRAME_FLAG_EORQ)
						{
							sprintf(exp_pkt_str, "0 (skipped %d)", exp_pkt);
							exp_pkt = 0;
						}
						else
						{
							sprintf(exp_pkt_str, "%d", exp_pkt);
						}
					}
					else
					{	
						sprintf(exp_pkt_str, "%d", exp_pkt);
					}

					if (exp_pkt == 0)
					{
						done = 1;
						cmd_ix++;
					}
					else
					{
						done = 0;
					}
				}

				// Check for errors in data packet
				if ((is_bin) && (no_err))
				{
					// Check SYNCH_FAIL and LOS_FLAG
					data_pkt = (DataPacket*) buf_ual;
					if (GET_SYNCH_FAIL(ntohs(data_pkt->hdr)))
					{
						synch_fail_cnt++;
					}
					if (GET_LOS_FLAG(ntohs(data_pkt->hdr)))
					{
						los_cnt++;
					}
					// See if any unexpected constant data is in packet
					if (probe_ct_pkt)
					{
						ct_fnd = 1;
						for (ct_loop=0; ct_loop<probe_ct_pkt; ct_loop++)
						{
							if (data_pkt->samp[3] != data_pkt->samp[4+ct_loop])
							{
								ct_fnd = 0;
							}
						}
						if (ct_fnd)
						{
							DataPacket_Print((DataPacket*) buf_ual);
							ct_cnt++;
							printf("dcc(%02d): Warning: %d data packets with sample 3 to %d = %d (0x%x)\n",
								cur_dcc,
								ct_cnt,
								(3+probe_ct_pkt),
								htons(data_pkt->samp[3]),
								htons(data_pkt->samp[3])
							);
						}
					}
				}

				// show packet if desired
				if (verbose > 2)
				{
					if ((is_bin) && (no_err))
					{
						printf("dcc(%02d).rep(%d.%d): %d bytes of data (expecting %s more packets)\n",
							cur_dcc,
							dcc_cmd_cnt[cur_dcc],
							cmd_sub_ix,
							length,
							exp_pkt_str);
						if (verbose > 3)
						{
							DataPacket_Print((DataPacket*) buf_ual);
						}
					}
					else
					{
						*(buf_ual+length) = '\0';
						printf("dcc(%02d).rep(%d): %s", cur_dcc, dcc_cmd_cnt[cur_dcc], buf_ual);
					}
				}
				// save packet if needed
				if (save_res)
				{
					if ((is_bin) && (no_err))
					{
						// Add ASCII header
						sprintf(msg, "dcc(%02d).rep(%d.%d): %d bytes of data\n", cur_dcc, dcc_cmd_cnt[cur_dcc], cmd_sub_ix, length);
						Event_Save(&event, &msg[0], strlen(msg), 0, EVENT_CONT, event_cnt);
						
						// In cmd_server V1.x format DCC header is not present
						if (format_ver == 1)
						{
							buf_ual+=2;
							length-=2;
							*(buf_ual+0) = (length >> 8) & 0xFF;
							*(buf_ual+1) = (length >> 0) & 0xFF;
						}

						// Save binary data
						Event_Save(&event, buf_ual, length, 1, EVENT_CONT, event_cnt);
					}
					else
					{
						*(buf_ual+length) = '\0';
						sprintf(msg, "dcc(%02d).rep(%d.%d): %s\n", cur_dcc, dcc_cmd_cnt[cur_dcc], cmd_sub_ix, buf_ual);
						Event_Save(&event, &msg[0], strlen(msg), 0, EVENT_CONT, event_cnt);
					}
				}

				cmd_sub_ix++;
			}
		}

		// periodic print
		if (verbose > 0)
		{
			if (i && ((i%1000) == 0))
			{
				printf("  Sent %d cmd. Retry: %d. Failed: %d Event: %d LOS: %d SYNCH_FAIL: %d\n",
					i, rep_lost, cmd_failed, event_cnt, los_cnt, synch_fail_cnt);
			}
		}

		i++;
		dcc_cmd_cnt[cur_dcc]++;
	}

	// Final stats
	printf("---------------------------------\n");
	printf("Command sent       = %d\n", i);
	printf("Command retry      = %d\n", rep_lost);
	printf("Command failed     = %d\n", cmd_failed);
	printf("Events             = %d\n", event_cnt);
	printf("LOS error          = %d\n", los_cnt);
	printf("SYNCH error        = %d\n", synch_fail_cnt);
	printf("---------------------------------\n");
	

cleanup:

	for (i=0; i<MAX_NUMBER_OF_DCCS; i++)
	{
		if (dcc_socket[i].client)
		{
			DccSocket_Close(&dcc_socket[i]);
		}
	}

	// There might still be the last event to save
	if (save_res)
	{
		if (pending_event)
		{
			Event_Save(&event, (char *) 0, 0, 1, EVENT_END, event_cnt);
		}

		fclose(event.fout);
	}

	socket_cleanup();
	return (err);
}


