/*********************************************************************************
TRESTDAQFEMINOS.cxx

Implement data acquisition for FEMINOS + FEC based readout

Author: JuanAn Garcia 18/08/2021

*********************************************************************************/

#include "TRESTDAQFEMINOS.h"
#include "FEMINOSPacket.h"

std::atomic<bool> TRESTDAQFEMINOS::stopReceiver(false);

TRESTDAQFEMINOS::TRESTDAQFEMINOS(TRestRun* rR, TRestRawDAQMetadata* dM) : TRESTDAQ(rR, dM) { initialize(); }

void TRESTDAQFEMINOS::initialize() {

    FEMArray.reserve(__builtin_popcount(GetDAQMetadata()->GetFECMask()) );//Reserve space for all the feminos inside the FECMASK

    int *baseIp = GetDAQMetadata()->GetBaseIp();

    for (int fecN = 0; fecN < 32; fecN++) {
        if (GetDAQMetadata()->GetFECMask() & (1 << fecN) == 0) continue;
        FEMProxy FEM;
        FEM.Open(baseIp, GetDAQMetadata()->GetLocalIp(), REMOTE_DST_PORT);
        FEM.FEMIndex = fecN;
        FEM.buffer.reserve(MAX_BUFFER_SIZE);
        FEMArray.emplace_back(std::move(FEM));
        baseIp[3]++;//Increase IP for the next FEMINOS
    }

  //Start receive and event builder threads
  receiveThread = std::thread( TRESTDAQFEMINOS::ReceiveThread, &FEMArray);
  eventBuilderThread = std::thread( TRESTDAQFEMINOS::EventBuilderThread, &FEMArray,GetRestRun(),GetSignalEvent());
}

void TRESTDAQFEMINOS::startUp(){
  std::cout << "Starting up readout" << std::endl;
  //FEC Power-down
  SendCommand("power_inv 0",FEMArray);
  SendCommand("fec_enable 0",FEMArray);
  //Ring Buffer and DAQ cleanup
  SendCommand("daq 0x000000 F",FEMArray);
  SendCommand("daq 0xFFFFFF F",FEMArray);
  SendCommand("sca enable 0",FEMArray);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  SendCommand("serve_target 0",FEMArray);
  std::this_thread::sleep_for(std::chrono::seconds(4));
  SendCommand("rbf getpnd",FEMArray);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  //Ring Buffer startup
  SendCommand("rbf timed 1",FEMArray);
  SendCommand("rbf timeval 2",FEMArray);
  SendCommand("rbf resume",FEMArray);
  //Selection of operating mode
  std::string cmd = "mode " +std::string(GetDAQMetadata()->GetChipType());
  SendCommand(cmd.c_str(),FEMArray);
  cmd = "polarity " +std::to_string( GetDAQMetadata()->GetPolarity() & 0x1);
  SendCommand(cmd.c_str(),FEMArray);
  //FEC Power-up and ASIC activation
  SendCommand("fec_enable 1",FEMArray);
  SendCommand("asic mask 0x0",FEMArray);
}

void TRESTDAQFEMINOS::configure() {
  std::cout << "Configuring readout" << std::endl;
  char cmd[200];
  //Feminos settings
  sprintf(cmd, "sca wckdiv 0x%X", GetDAQMetadata()->GetClockDiv());  // Clock div
  SendCommand(cmd,FEMArray);
  SendCommand("sca cnt 0x200",FEMArray);
  SendCommand("sca autostart 1",FEMArray);
  SendCommand("rst_len 0",FEMArray);
    //AGET settings
    if( GetDAQMetadata()->GetChipType() == "aget"){
      SendCommand("aget * autoreset_bank 0x1",FEMArray);
        if(GetDAQMetadata()->GetPolarity() & 0x1 == 0){//Negative polarity
          SendCommand("aget * vicm 0x1",FEMArray);
          SendCommand("aget * polarity 0x0",FEMArray);
        } else {//Positive polarity
          SendCommand("aget * vicm 0x2",FEMArray);
          SendCommand("aget * polarity 0x1",FEMArray);
        }
      SendCommand("aget * en_mkr_rst 0x0",FEMArray);
      SendCommand("aget * rst_level 0x1",FEMArray);
      SendCommand("aget * short_read 0x1",FEMArray);
      SendCommand("aget * tst_digout 0x1",FEMArray);
      SendCommand("aget * mode 0x1",FEMArray);
      sprintf(cmd, "aget * gain 0x%X", (GetDAQMetadata()->GetShappingTime() & 0xF) );  // Gain
      SendCommand(cmd,FEMArray);
      sprintf(cmd, "aget * time 0x%X", (GetDAQMetadata()->GetGain() & 0x3) );  // Shapping time
      SendCommand(cmd,FEMArray);
      SendCommand("aget * dac 0x0",FEMArray);
    } else {
      std::string error = std::string(GetDAQMetadata()->GetChipType())  +" mode not supported in FEMINOS cards";
      throw (TRESTDAQException(error));
    }

}

void TRESTDAQFEMINOS::startDAQ() {
    auto rT = daq_metadata_types::acqTypes_map.find(std::string(GetDAQMetadata()->GetAcquisitionType()));
    if (rT == daq_metadata_types::acqTypes_map.end()) {
        std::cout << "Unknown acquisition type " << GetDAQMetadata()->GetAcquisitionType() << " skipping" << std::endl;
        std::cout << "Valid acq types:" << std::endl;
        for (auto& [name, t] : daq_metadata_types::acqTypes_map) std::cout << (int)t << " " << name << std::endl;
        return;
    }

    if (rT->second == daq_metadata_types::acqTypes::PEDESTAL) {
        pedestal();
    } else {
        dataTaking();
    }
}

void TRESTDAQFEMINOS::pedestal() {
  std::cout << "Starting pedestal run" << std::endl;
  //Test mode settings
  SendCommand("keep_fco 0",FEMArray);
  SendCommand("test_zbt 0",FEMArray);
  SendCommand("test_enable 0",FEMArray);
  SendCommand("test_mode 0",FEMArray);
  SendCommand("tdata A 0x40",FEMArray);
  //Readout settings
  SendCommand("modify_hit_reg 0",FEMArray);
  SendCommand("emit_hit_cnt 1",FEMArray);
  SendCommand("emit_empty_ch 1",FEMArray);
  SendCommand("keep_rst 1",FEMArray);
  SendCommand("skip_rst 0",FEMArray);
  //# Channel ena/disable (AGET only)
    if( GetDAQMetadata()->GetChipType() == "aget"){
      SendCommand("forceon_all 1",FEMArray);
      SendCommand("forceoff * * 0x1",FEMArray);
      SendCommand("forceon * * 0x0",FEMArray);
      SendCommand("forceoff 0 * 0x0",FEMArray);
      SendCommand("forceon 0 * 0x1",FEMArray);
    }
  //Pedestal Thresholds and Zero-suppression
  SendCommand("ped * * 0x0",FEMArray);
  SendCommand("subtract_ped 0",FEMArray);
  SendCommand("zero_suppress 0",FEMArray);
  SendCommand("zs_pre_post 0 0",FEMArray);
  SendCommand("thr * * 0x0",FEMArray);
  //Event generator
  SendCommand("event_limit 0x2",FEMArray);//# Event limit: 0x0:infinite; 0x1:1; 0x2:10; 0x3:100; 0x4: 1000; 0x5:10000; 0x6:100000; 0x7:1000000
  SendCommand("trig_rate 1 10",FEMArray);//# Range: 0:0.1Hz-10Hz 1:10Hz-1kHz 2:100Hz-10kHz 3:1kHz-100kHz
  SendCommand("trig_enable 0x1",FEMArray);
  //Pedestal Histograms
  SendCommand("hped offset * * 0",FEMArray);
  SendCommand("hped clr * *",FEMArray);
  //Data server target: 0:drop data; 1:send to DAQ; 2:feed to pedestal histos; 3:feed to hit channel histos 
  SendCommand("serve_target 2",FEMArray);
  SendCommand("sca enable 1",FEMArray);
  std::this_thread::sleep_for(std::chrono::seconds(15));// Wait pedestal accumulation completion
  SendCommand("sca enable 0",FEMArray);
  char cmd[200];
    for(int i=0;i<4;i++){
      sprintf(cmd, "hped getsummary %d *", i );
      SendCommand(cmd,FEMArray);
    }
  //Set pedestal equalization constants TODO make configurable
  SendCommand("hped centermean * * 250",FEMArray);
  SendCommand("subtract_ped 1",FEMArray);
  SendCommand("hped clr * *",FEMArray);
  SendCommand("sca enable 1",FEMArray);
  std::this_thread::sleep_for(std::chrono::seconds(15));// Wait pedestal accumulation completion
  SendCommand("sca enable 0",FEMArray);
    for(int i=0;i<4;i++){
      sprintf(cmd, "hped getsummary %d *", i );
      SendCommand(cmd,FEMArray);
    }
  SendCommand("hped setthr * * 250 5.0",FEMArray);
  //Set Data server target to DAQ
  SendCommand("serve_target 1",FEMArray);
}

void TRESTDAQFEMINOS::dataTaking() {
  std::cout << "Starting data taking run" << std::endl;
  SendCommand("daq 0x000000 F",FEMArray);
  SendCommand("daq 0xFFFFFF F",FEMArray);
  SendCommand("sca enable 0",FEMArray);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  SendCommand("serve_target 0",FEMArray);
  std::this_thread::sleep_for(std::chrono::seconds(4));
  //Readout settings
  SendCommand("modify_hit_reg 0",FEMArray);
  SendCommand("emit_hit_cnt 1",FEMArray);
  SendCommand("emit_empty_ch 0",FEMArray);
  SendCommand("emit_lst_cell_rd 1",FEMArray);
  SendCommand("keep_rst 1",FEMArray);
  SendCommand("skip_rst 0",FEMArray);
  //AGET settings
    if( GetDAQMetadata()->GetChipType() == "aget"){
      SendCommand("aget * mode 0x1",FEMArray);//Mode: 0x0: hit/selected channels 0x1:all channels
      SendCommand("aget * tst_digout 1",FEMArray);//??
      SendCommand("forceon_all 1",FEMArray);
    }
    SendCommand("subtract_ped 1",FEMArray);
    if(GetDAQMetadata()->GetCompressMode()){
      SendCommand("zero_suppress 1",FEMArray);
      SendCommand("zs_pre_post 8 4",FEMArray);
    } else {
      SendCommand("zero_suppress 0",FEMArray);
      SendCommand("zs_pre_post 0 0",FEMArray);
    }
    //Event generator
    SendCommand("event_limit 0x0",FEMArray);//Infinite
      if (GetDAQMetadata()->GetTriggerType() == "internal"){
        SendCommand("trig_rate 1 50",FEMArray);
        SendCommand("trig_enable 0x1",FEMArray);
      } else {
        SendCommand("trig_enable 0x8",FEMArray);//tcm???
      }
    SendCommand("serve_target 1",FEMArray);//1: send to DAQ
    SendCommand("sca enable 1",FEMArray);//Enable data taking
    SendCommand("daq 0x000000 F",FEMArray);//DAQ request
      //Wait till DAQ completion
      while (!abrt && (GetDAQMetadata()->GetNEvents() == 0 || event_cnt < GetDAQMetadata()->GetNEvents())) {
        //Do something here? E.g. send packet request
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    SendCommand("daq 0xFFFFFF F",FEMArray);//Stop DAQ request

}

void TRESTDAQFEMINOS::stopDAQ() {
  stopReceiver = true;
  receiveThread.join();
  eventBuilderThread.join();
}

void TRESTDAQFEMINOS::saveEvent(unsigned char* buf, int size) {
    // If data supplied, copy to temporary buffer
    if (size <= 0) return;

    int physChannel=0;
    std::vector<Short_t> sData(512,0);

    TRestRawSignal rawSignal(physChannel, sData);
    GetSignalEvent()->AddSignal(rawSignal);
}


void TRESTDAQFEMINOS::SendCommand(const char* cmd, std::vector<FEMProxy> &FEMA){

  std::unique_lock<std::mutex> lock(mutex);

    for (auto &FEM : FEMA){
      if (sendto(FEM.client, cmd, strlen(cmd), 0, (struct sockaddr*)&(FEM.target), sizeof(struct sockaddr)) == -1) {
        std::string error ="sendto failed: " + std::string(strerror(errno));
        throw (TRESTDAQException(error));
      }
    }

  lock.unlock();

    if (verboseLevel >= REST_Debug)std::cout<<"Command sent "<<cmd<<std::endl;

}

void TRESTDAQFEMINOS::SendCommand(const char* cmd, FEMProxy &FEM){

  std::unique_lock<std::mutex> lock(mutex);

      if (sendto(FEM.client, cmd, strlen(cmd), 0, (struct sockaddr*)&(FEM.target), sizeof(struct sockaddr)) == -1) {
        std::string error ="sendto failed: " + std::string(strerror(errno));
        throw (TRESTDAQException(error));
      }

  lock.unlock();

    if (verboseLevel >= REST_Debug)std::cout<<"Command sent "<<cmd<<std::endl;

}

void TRESTDAQFEMINOS::ReceiveThread( std::vector<FEMProxy> *FEMA ) {

  fd_set readfds, writefds, exceptfds, readfds_work;
  struct timeval t_timeout;
  t_timeout.tv_sec  = 5;
  t_timeout.tv_usec = 0;

  // Build the socket descriptor set from which we want to read
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);

  
  int smax  = 0;
    for (auto &FEM : *FEMA){
      FD_SET(FEM.client, &readfds);
        if (FEM.client > smax)
          smax = FEM.client;
    }
  smax++;

    while (!stopReceiver){

      // Copy the read fds from what we computed outside of the loop
      readfds_work = readfds;

        int err =0;
        // Wait for any of these sockets to be ready
        if ((err = select(smax, &readfds_work, &writefds, &exceptfds, &t_timeout)) < 0){
           std::string error ="select failed: " + std::string(strerror(errno));
           throw (TRESTDAQException(error));
        }

        if(err == 0 )continue;//Nothing received

      std::unique_lock<std::mutex> lock(mutex);

        for (auto &FEM : *FEMA){
          if (FD_ISSET(FEM.client, &readfds_work)){
            uint16_t *buf_rcv;
            int length = recvfrom(FEM.client, buf_rcv, 8192, 0, (struct sockaddr*)&FEM.remote, &FEM.remote_size);

              if (length < 0) {
                std::string error ="recvfrom failed: " + std::string(strerror(errno));
                throw (TRESTDAQException(error));
              }

            if(length<=2)continue;//empty frame?

            size_t size = length/sizeof(uint16_t);//Note that length is in bytes while size is uint16_t
            std::vector<uint16_t> frame(buf_rcv+1,buf_rcv + size-1);//skipping first word after the UDP header
            FEM.buffer.emplace_back(std::move(frame));
              if(FEM.buffer.size() >= (MAX_BUFFER_SIZE -1) ){
                std::string error ="Buffer FULL on FEM: "+ std::to_string(FEM.FEMIndex);
                throw (TRESTDAQException(error));
              }
          }
        }
      lock.unlock();
    }
}

void TRESTDAQFEMINOS::EventBuilderThread(std::vector<FEMProxy> *FEMA, TRestRun *rR, TRestRawSignalEvent* sEvent){

  sEvent->Initialize();

  uint32_t ev_count;
  uint64_t ts;

  bool pendingEvent = false;
  bool newEvent = false;

  while (!stopReceiver){
    
    //Lock the thread, this is needed to avoid writing the buffer while reading it
    std::unique_lock<std::mutex> lock(mutex);
      for (auto &FEM : *FEMA){
        if(FEM.buffer.empty())continue;//Do nothing, no frames left to read
        if(pendingEvent && !FEM.pendingEvent)continue;//Wait till we reach end of event for all the FEMINOS

        std::vector<uint16_t> frame = FEM.buffer.front();//Get first frame in the queue
          if (verboseLevel >= REST_Debug)FEMINOSPacket::DataPacket_Print(frame.data(), frame.size());

            if(FEMINOSPacket::isDataFrame(frame.data() ) ){
              int physChannel;
              bool endOfEvent;
              std::vector<Short_t> sData(512,0);
                if(!FEMINOSPacket::GetDataFrame( frame.data(), frame.size(), sData, physChannel, ev_count, ts, endOfEvent) ){
                  newEvent =true;
                  FEM.pendingEvent = !endOfEvent;
                  TRestRawSignal rawSignal(physChannel, sData);
                  sEvent->AddSignal(rawSignal);
                }
          }

          FEM.buffer.erase(FEM.buffer.begin());//Erase buffer which has been readout
      }

    lock.unlock();

      for (auto &FEM : *FEMA){
        pendingEvent |= FEM.pendingEvent;//Check if the event is pending
      }

      if(newEvent && !pendingEvent){//Save Event if closed
        sEvent->SetID(ev_count);
        sEvent->SetTime( rR->GetStartTimestamp() + (double) ts * 2E-8 );
        FillTree(rR,sEvent);
        sEvent->Initialize();
        newEvent =false;
      }

  }

}


