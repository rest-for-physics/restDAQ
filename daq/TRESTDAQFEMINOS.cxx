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

    FEMArray.reserve(GetDAQMetadata()->GetFECs().size() );//Reserve space for all the feminos inside the FEC

    int *baseIp = GetDAQMetadata()->GetBaseIp();

    for(auto fec : GetDAQMetadata()->GetFECs()){
        FEMProxy FEM;
        FEM.Open(fec.ip, GetDAQMetadata()->GetLocalIp(), REMOTE_DST_PORT);
        FEM.buffer.reserve(MAX_BUFFER_SIZE);
        FEM.fecMetadata = fec;
        FEMArray.emplace_back(std::move(FEM));
    }

  //Start receive and event builder threads
  receiveThread = std::thread( TRESTDAQFEMINOS::ReceiveThread, &FEMArray);
  eventBuilderThread = std::thread( TRESTDAQFEMINOS::EventBuilderThread, &FEMArray, GetRestRun(),GetSignalEvent());
}

void TRESTDAQFEMINOS::startUp(){
  std::cout << "Starting up readout" << std::endl;
  //FEC Power-down
  BroadcastCommand("power_inv 0",FEMArray);
  BroadcastCommand("fec_enable 0",FEMArray);
  //Ring Buffer and DAQ cleanup
  BroadcastCommand("daq 0x000000 F",FEMArray);
  BroadcastCommand("daq 0xFFFFFF F",FEMArray);
  BroadcastCommand("sca enable 0",FEMArray);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  BroadcastCommand("serve_target 0",FEMArray);
  std::this_thread::sleep_for(std::chrono::seconds(4));
  BroadcastCommand("rbf getpnd",FEMArray);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  //Ring Buffer startup
  BroadcastCommand("rbf timed 1",FEMArray);
  BroadcastCommand("rbf timeval 2",FEMArray);
  BroadcastCommand("rbf resume",FEMArray);
    //Selection of operating mode
    std::unique_lock<std::mutex> lock(mutex);
    for (auto &FEM : FEMArray){
      char cmd[200];
      sprintf(cmd, "mode %s", FEM.fecMetadata.chipType);
      SendCommand(cmd,FEM);
      uint8_t asicMask =0;
        for(int a=0;a<4;a++){
          if(!FEM.fecMetadata.asic[a].isActive)continue;
          asicMask |= (1 << a);
          sprintf(cmd, "polarity %d %d", a, FEM.fecMetadata.asic[a].polarity);
          SendCommand(cmd,FEM);
        }
      //FEC Power-up and ASIC activation
      SendCommand("fec_enable 1",FEM);
      sprintf(cmd, "asic mask 0x%X", asicMask);
      SendCommand(cmd,FEM);
    }
    lock.unlock();

}

void TRESTDAQFEMINOS::configure() {
  std::cout << "Configuring readout" << std::endl;
  char cmd[200];
  //Feminos settings
  //Selection of operating mode
  std::unique_lock<std::mutex> lock(mutex);
    for (auto &FEM : FEMArray){
      sprintf(cmd, "sca wckdiv 0x%X", FEM.fecMetadata.clockDiv);  // Clock div
      SendCommand(cmd,FEM);
    }
  lock.unlock();

  BroadcastCommand("sca cnt 0x200",FEMArray);
  BroadcastCommand("sca autostart 1",FEMArray);
  BroadcastCommand("rst_len 0",FEMArray);
    //AGET settings
    lock.lock();
    for (auto &FEM : FEMArray){
      if(FEM.fecMetadata.chipType != "aget"){
        std::string error = std::string(FEM.fecMetadata.chipType) +" mode not supported in FEMINOS cards";
        throw (TRESTDAQException(error));
      }

      SendCommand("aget * autoreset_bank 0x1",FEM);
        for(int a=0;a<4;a++){
          if(!FEM.fecMetadata.asic[a].isActive)continue;
            if(FEM.fecMetadata.asic[a].polarity == 0){
              sprintf(cmd, "aget %d vicm 0x1", a);
              SendCommand(cmd,FEM);
              sprintf(cmd, "aget %d polarity 0x0", a);
              SendCommand(cmd,FEM);
            } else {
              sprintf(cmd, "aget %d vicm 0x2", a);
              SendCommand(cmd,FEM);
              sprintf(cmd, "aget %d polarity 0x1", a);
              SendCommand(cmd,FEM);
            }
        }

      SendCommand("aget * en_mkr_rst 0x0",FEM);
      SendCommand("aget * rst_level 0x1",FEM);
      SendCommand("aget * short_read 0x1",FEM);
      SendCommand("aget * tst_digout 0x1",FEM);
      SendCommand("aget * mode 0x1",FEM);

        for(int a=0;a<4;a++){
          if(!FEM.fecMetadata.asic[a].isActive)continue;
          sprintf(cmd, "aget %d gain * 0x%X",a, (FEM.fecMetadata.asic[a].gain & 0x3) ); //Gain
          SendCommand(cmd,FEM);
          sprintf(cmd, "aget %d time 0x%X",a, (FEM.fecMetadata.asic[a].shappingTime & 0xF) );  //Shapping time
          SendCommand(cmd,FEM);
        }
      SendCommand("aget * dac 0x0",FEM);

      //# Channel ena/disable (AGET only)
      SendCommand("forceon_all 1",FEM);
      SendCommand("forceoff * * 0x1",FEM);
      SendCommand("forceon * * 0x0",FEM);
        for(int a=0;a<4;a++){
          if(!FEM.fecMetadata.asic[a].isActive)continue;
            sprintf(cmd,"forceoff %d * 0x0",a);
            SendCommand(cmd,FEM);
            sprintf(cmd,"forceon %d * 0x1",a);
            SendCommand(cmd,FEM);
             for(int c=0;c<78;c++){
               if(FEM.fecMetadata.asic[a].channelActive[c])continue;
               sprintf(cmd,"forceoff %d %d 0x1",a);
               SendCommand(cmd,FEM);
               sprintf(cmd,"forceon %d %d 0x0",a);
               SendCommand(cmd,FEM);
             }
        }
    }
  lock.unlock();

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
  BroadcastCommand("keep_fco 0",FEMArray);
  BroadcastCommand("test_zbt 0",FEMArray);
  BroadcastCommand("test_enable 0",FEMArray);
  BroadcastCommand("test_mode 0",FEMArray);
  BroadcastCommand("tdata A 0x40",FEMArray);
  //Readout settings
  BroadcastCommand("modify_hit_reg 0",FEMArray);
  BroadcastCommand("emit_hit_cnt 1",FEMArray);
  BroadcastCommand("emit_empty_ch 1",FEMArray);
  BroadcastCommand("keep_rst 1",FEMArray);
  BroadcastCommand("skip_rst 0",FEMArray);
  //Pedestal Thresholds and Zero-suppression
  BroadcastCommand("ped * * 0x0",FEMArray);
  BroadcastCommand("subtract_ped 0",FEMArray);
  BroadcastCommand("zero_suppress 0",FEMArray);
  BroadcastCommand("zs_pre_post 0 0",FEMArray);
  BroadcastCommand("thr * * 0x0",FEMArray);
  //Event generator
  BroadcastCommand("event_limit 0x3",FEMArray);//# Event limit: 0x0:infinite; 0x1:1; 0x2:10; 0x3:100; 0x4: 1000; 0x5:10000; 0x6:100000; 0x7:1000000
  BroadcastCommand("trig_rate 1 10",FEMArray);//# Range: 0:0.1Hz-10Hz 1:10Hz-1kHz 2:100Hz-10kHz 3:1kHz-100kHz
  BroadcastCommand("trig_enable 0x1",FEMArray);
  //Pedestal Histograms
  BroadcastCommand("hped offset * * 0",FEMArray);
  BroadcastCommand("hped clr * *",FEMArray);
  //Data server target: 0:drop data; 1:send to DAQ; 2:feed to pedestal histos; 3:feed to hit channel histos 
  BroadcastCommand("serve_target 2",FEMArray);
  BroadcastCommand("sca enable 1",FEMArray);
  std::this_thread::sleep_for(std::chrono::seconds(15));// Wait pedestal accumulation completion
  BroadcastCommand("sca enable 0",FEMArray);
  char cmd[200];
  std::unique_lock<std::mutex> lock(mutex);
    for (auto &FEM : FEMArray){
      for(int a=0;a<4;a++){
        if(!FEM.fecMetadata.asic[a].isActive)continue;
        sprintf(cmd, "hped getsummary %d *", a );
        SendCommand(cmd,FEM);
        //Set pedestal equalization
        sprintf(cmd, "hped centermean %d * %d", a , FEM.fecMetadata.asic[a].pedCenter );
        SendCommand(cmd,FEM);
      }
    }
  lock.unlock();

  BroadcastCommand("subtract_ped 1",FEMArray);
  BroadcastCommand("hped clr * *",FEMArray);
  BroadcastCommand("sca enable 1",FEMArray);
  std::this_thread::sleep_for(std::chrono::seconds(15));// Wait pedestal accumulation completion
  BroadcastCommand("sca enable 0",FEMArray);
    lock.lock();
    for (auto &FEM : FEMArray){
      for(int a=0;a<4;a++){
        if(!FEM.fecMetadata.asic[a].isActive)continue;
        sprintf(cmd, "hped getsummary %d *", a );
        SendCommand(cmd,FEM);
        //Set threshold
        sprintf(cmd, "hped setthr %d * %d %.1f", a , FEM.fecMetadata.asic[a].pedCenter, FEM.fecMetadata.asic[a].pedThr );
        SendCommand(cmd,FEM);
      }
    }
  lock.unlock();

  //Set Data server target to DAQ
  BroadcastCommand("serve_target 1",FEMArray);
}

void TRESTDAQFEMINOS::dataTaking() {
  std::cout << "Starting data taking run" << std::endl;
  BroadcastCommand("daq 0x000000 F",FEMArray);
  BroadcastCommand("daq 0xFFFFFF F",FEMArray);
  BroadcastCommand("sca enable 0",FEMArray);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  BroadcastCommand("serve_target 0",FEMArray);
  std::this_thread::sleep_for(std::chrono::seconds(4));
  //Readout settings
  BroadcastCommand("modify_hit_reg 0",FEMArray);
  BroadcastCommand("emit_hit_cnt 1",FEMArray);
  BroadcastCommand("emit_empty_ch 0",FEMArray);
  BroadcastCommand("emit_lst_cell_rd 1",FEMArray);
  BroadcastCommand("keep_rst 1",FEMArray);
  BroadcastCommand("skip_rst 0",FEMArray);
  //AGET settings
    if( GetDAQMetadata()->GetChipType() == "aget"){
      BroadcastCommand("aget * mode 0x1",FEMArray);//Mode: 0x0: hit/selected channels 0x1:all channels
      BroadcastCommand("aget * tst_digout 1",FEMArray);//??
    }
    BroadcastCommand("subtract_ped 1",FEMArray);
    if(GetDAQMetadata()->GetCompressMode()){
      BroadcastCommand("zero_suppress 1",FEMArray);
      BroadcastCommand("zs_pre_post 8 4",FEMArray);
    } else {
      BroadcastCommand("zero_suppress 0",FEMArray);
      BroadcastCommand("zs_pre_post 0 0",FEMArray);
    }
    //Event generator
    BroadcastCommand("event_limit 0x0",FEMArray);//Infinite
      if (GetDAQMetadata()->GetTriggerType() == "internal"){
        BroadcastCommand("trig_rate 1 50",FEMArray);
        BroadcastCommand("trig_enable 0x1",FEMArray);
      } else {
        BroadcastCommand("trig_enable 0x8",FEMArray);//tcm???
      }
    BroadcastCommand("serve_target 1",FEMArray);//1: send to DAQ
    BroadcastCommand("sca enable 1",FEMArray);//Enable data taking
    BroadcastCommand("daq 0x000000 F",FEMArray);//DAQ request
      //Wait till DAQ completion
      while (!abrt && (GetDAQMetadata()->GetNEvents() == 0 || event_cnt < GetDAQMetadata()->GetNEvents())) {
        //Do something here? E.g. send packet request
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    BroadcastCommand("daq 0xFFFFFF F",FEMArray);//Stop DAQ request

}

void TRESTDAQFEMINOS::stopDAQ() {
  stopReceiver = true;
  receiveThread.join();
  eventBuilderThread.join();

    for (auto &FEM : FEMArray)
      FEM.Close();
}

void TRESTDAQFEMINOS::BroadcastCommand(const char* cmd, std::vector<FEMProxy> &FEMA){

  std::unique_lock<std::mutex> lock(mutex);

    for (auto &FEM : FEMA){
      SendCommand(cmd,FEM);
    }

  lock.unlock();

    if (verboseLevel >= REST_Debug)std::cout<<"Command sent "<<cmd<<std::endl;

}

void TRESTDAQFEMINOS::SendCommand(const char* cmd, FEMProxy &FEM){

      if (sendto(FEM.client, cmd, strlen(cmd), 0, (struct sockaddr*)&(FEM.target), sizeof(struct sockaddr)) == -1) {
        std::string error ="sendto failed: " + std::string(strerror(errno));
        throw (TRESTDAQException(error));
      }

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
            uint16_t buf_rcv[8192/(sizeof(uint16_t))];
            int length = recvfrom(FEM.client, buf_rcv, 8192, 0, (struct sockaddr*)&FEM.remote, &FEM.remote_size);

              if (length < 0) {
                std::string error ="recvfrom failed: " + std::string(strerror(errno));
                throw (TRESTDAQException(error));
              }

            if(length<=2)continue;//empty frame?

            size_t size = length/sizeof(uint16_t);//Note that length is in bytes while size is uint16_t
            std::vector<uint16_t> frame(&buf_rcv[1],buf_rcv + size-1);//skipping first word after the UDP header
            FEM.buffer.emplace_back(std::move(frame));
              if(FEM.buffer.size() >= (MAX_BUFFER_SIZE -1) ){
                std::string error ="Buffer FULL on FEM: "+ std::to_string(FEM.fecMetadata.id);
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
        if(rR){
          sEvent->SetID(ev_count);
          sEvent->SetTime( rR->GetStartTimestamp() + (double) ts * 2E-8 );
          FillTree(rR,sEvent);
          sEvent->Initialize();
          newEvent =false;
        }
      }

  }

}


