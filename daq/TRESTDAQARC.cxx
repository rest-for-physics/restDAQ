/*********************************************************************************
TRESTDAQARC.cxx

Implement data acquisition for ARC based readout

Author: JuanAn Garcia 15/04/2024

*********************************************************************************/

#include "TRESTDAQARC.h"
#include "ARCPacket.h"


std::atomic<bool> TRESTDAQARC::stopReceiver(false);
std::atomic<bool> TRESTDAQARC::isPed(false);

TRESTDAQARC::TRESTDAQARC(TRestRun* rR, TRestRawDAQMetadata* dM) : TRESTDAQ(rR, dM) { initialize(); }

void TRESTDAQARC::initialize() {

    for(auto fec : daqMetadata->GetFECs()){
        FEMProxy FEM;
        FEM.Open(fec.ip, REMOTE_DST_PORT);
        FEM.fecMetadata = fec;
        FEMArray.emplace_back(std::move(FEM));
    }

    for (auto &FEM : FEMArray){
      auto cT = daq_metadata_types::chipTypes_map.find(FEM.fecMetadata.chipType.Data());
        if(cT == daq_metadata_types::chipTypes_map.end() ){
          std::cerr << "Unknown chip type for FEC id "<< FEM.fecMetadata.id <<" " << FEM.fecMetadata.chipType.Data() << std::endl;
          std::cerr << "Valid chip types "<< std::endl;
            for(const auto &[type, chip] : daq_metadata_types::chipTypes_map){
              std::cerr << type <<" ["<< (int)chip <<"], \t";
            }
          std::cerr << std::endl;
          throw (TRESTDAQException("Unknown chip type, please check RML"));
       } else if (cT->second != daq_metadata_types::chipTypes::AGET){
         std::cerr << "Unsupported chip type for FEC id "<< FEM.fecMetadata.id <<" " << FEM.fecMetadata.chipType.Data() << std::endl;
         throw (TRESTDAQException("Unsupported chip type, please check RML"));
       }
    }

  //Start receive and event builder threads
  stopReceiver=false;
  receiveThread = std::thread( TRESTDAQARC::ReceiveThread, &FEMArray);
  eventBuilderThread = std::thread( TRESTDAQARC::EventBuilderThread, &FEMArray, restRun, &fSignalEvent);
}

void TRESTDAQARC::startUp(){
  std::cout << "Starting up readout" << std::endl;
  //FEC Power-down
  //BroadcastCommand("power_inv 0",FEMArray);
  BroadcastCommand("fec_enable 0",FEMArray);
  //Ring Buffer and DAQ cleanup
  BroadcastCommand("DAQ 0",FEMArray,false);
  BroadcastCommand("daq 0xFFFFFF B",FEMArray);
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
    for (auto &FEM : FEMArray){
      char cmd[200];
      sprintf(cmd, "mode %s", FEM.fecMetadata.chipType.Data());
      SendCommand(cmd,FEM);
      uint8_t asicMask =0;
        for(int a=0;a<TRestRawDAQMetadata::nAsics;a++){
            if(!FEM.fecMetadata.asic_isActive[a]){
              asicMask |= (1 << a);
              continue;
            }
          sprintf(cmd, "polarity %d %d", a, FEM.fecMetadata.asic_polarity[a]);
          SendCommand(cmd,FEM);
        }
      //FEC Power-up and ASIC activation
      SendCommand("fec_enable 1",FEM);
      sprintf(cmd, "asic_mask 0x%X", asicMask);
      SendCommand(cmd,FEM);
    }

}

void TRESTDAQARC::configure() {
  std::cout << "Configuring readout" << std::endl;
  char cmd[200];
  //ARC settings
  //Selection of operating mode
    for (auto &FEM : FEMArray){
      sprintf(cmd, "sca wckdiv 0x%X", FEM.fecMetadata.clockDiv);  // Clock div
      SendCommand(cmd,FEM);
    }

  BroadcastCommand("sca cnt 0x200",FEMArray);
  BroadcastCommand("sca autostart 1",FEMArray);
  BroadcastCommand("rst_len 1",FEMArray);
  BroadcastCommand("mmpol 0x3",FEMArray); //Needed?

  //Test mode settings
  BroadcastCommand("keep_fco 0",FEMArray);
  BroadcastCommand("test_zbt 0",FEMArray);
  BroadcastCommand("test_enable 0",FEMArray);
  BroadcastCommand("test_mode 0",FEMArray);
  BroadcastCommand("tdata A 0x40",FEMArray);

   //AGET settings
   for (auto &FEM : FEMArray){
      SendCommand("aget 3:0 autoreset_bank 0x1",FEM);
      SendCommand("aget 3:0 dis_multiplicity_out 0x0",FEM);
        for(int a=0;a<TRestRawDAQMetadata::nAsics;a++){
          if(!FEM.fecMetadata.asic_isActive[a])continue;
            if(FEM.fecMetadata.asic_polarity[a] == 0){
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

      SendCommand("aget 3:0 en_mkr_rst 0x0",FEM);
      SendCommand("aget 3:0 rst_level 0x1",FEM);
      SendCommand("aget 3:0 short_read 0x0",FEM);
      SendCommand("aget 3:0 tst_digout 0x0",FEM);
      SendCommand("aget 3:0 mode 0x1",FEM);

        for(int a=0;a<TRestRawDAQMetadata::nAsics;a++){
          if(!FEM.fecMetadata.asic_isActive[a])continue;
          sprintf(cmd, "aget %d gain * 0x%X",a, (FEM.fecMetadata.asic_gain[a] & 0x3) ); //Gain
          SendCommand(cmd,FEM);
          sprintf(cmd, "aget %d time 0x%X",a, (FEM.fecMetadata.asic_shappingTime[a] & 0xF) );  //Shapping time
          SendCommand(cmd,FEM);
        }
      SendCommand("aget 3:0 dac 0x0",FEM);
    }
}

void TRESTDAQARC::startDAQ(bool configure) {
    auto rT = daq_metadata_types::acqTypes_map.find(std::string(daqMetadata->GetAcquisitionType()));
    if (rT == daq_metadata_types::acqTypes_map.end()) {
        std::cout << "Unknown acquisition type " << daqMetadata->GetAcquisitionType() << " skipping" << std::endl;
        std::cout << "Valid acq types:" << std::endl;
        for (auto& [name, t] : daq_metadata_types::acqTypes_map) std::cout << (int)t << " " << name << std::endl;
        return;
    }

    if (rT->second == daq_metadata_types::acqTypes::PEDESTAL) {
        pedestal();
    } else {
        dataTaking(configure);
        isPed=false;
    }
}

void TRESTDAQARC::pedestal() {
  std::cout << "Starting pedestal run" << std::endl;
  //Readout settings
  BroadcastCommand("modify_hit_reg 0",FEMArray);
  BroadcastCommand("emit_hit_cnt 1",FEMArray);
  BroadcastCommand("emit_empty_ch 1",FEMArray);
  BroadcastCommand("keep_rst 1",FEMArray);
  BroadcastCommand("skip_rst 0",FEMArray);
  //# Channel ena/disable (AGET only)
  BroadcastCommand("forceon_all 1",FEMArray);
  BroadcastCommand("forceoff 3:0 * 0x0",FEMArray);
  BroadcastCommand("forceon 3:0 * 0x1",FEMArray);

  //Pedestal Thresholds and Zero-suppression
  BroadcastCommand("ped 3:0 * 0x0",FEMArray);
  BroadcastCommand("subtract_ped 0",FEMArray);
  BroadcastCommand("zero_suppress 0",FEMArray);
  BroadcastCommand("zs_pre_post 0 0",FEMArray);
  BroadcastCommand("thr * * 0x0",FEMArray);
  //Event generator
  BroadcastCommand("event_limit 0x2",FEMArray);//# Event limit: 0x0:infinite; 0x1:1; 0x2:10; 0x3:100; 0x4: 1000; 0x5:10000; 0x6:100000; 0x7:1000000
  BroadcastCommand("trig_rate 1 10",FEMArray);//# Range: 0:0.1Hz-10Hz 1:10Hz-1kHz 2:100Hz-10kHz 3:1kHz-100kHz
  BroadcastCommand("trig_ena 0x1",FEMArray);
  //Pedestal Histograms
  BroadcastCommand("hped 3:0 * offset 0",FEMArray);
  BroadcastCommand("hped 3:0 * clr",FEMArray);
  //Data server target: 0:drop data; 1:send to DAQ; 2:feed to pedestal histos; 3:feed to hit channel histos 
  BroadcastCommand("serve_target 2",FEMArray);
  BroadcastCommand("sca enable 1",FEMArray);
  std::this_thread::sleep_for(std::chrono::seconds(15));// Wait pedestal accumulation completion
  BroadcastCommand("sca enable 0",FEMArray);
  char cmd[200];
    for (auto &FEM : FEMArray){
      for(int a=0;a<4;a++){
        if(!FEM.fecMetadata.asic_isActive[a])continue;
        sprintf(cmd, "hped %d * getsummary", a );
        SendCommand(cmd,FEM);
      }
    }

  for (auto &FEM : FEMArray){
      for(int a=0;a<4;a++){
        if(!FEM.fecMetadata.asic_isActive[a])continue;
        //Set pedestal equalization
        sprintf(cmd, "hped %d * centermean %d", a , FEM.fecMetadata.asic_pedCenter[a] );
        SendCommand(cmd,FEM);
      }
    }

  isPed=true;

  BroadcastCommand("subtract_ped 1",FEMArray);
  BroadcastCommand("hped 3:0 * clr",FEMArray);
  BroadcastCommand("sca enable 1",FEMArray);
  std::this_thread::sleep_for(std::chrono::seconds(15));// Wait pedestal accumulation completion
  BroadcastCommand("sca enable 0",FEMArray);
  BroadcastCommand("trig_ena 0x0",FEMArray);
    for (auto &FEM : FEMArray){
      for(int a=0;a<TRestRawDAQMetadata::nAsics;a++){
        if(!FEM.fecMetadata.asic_isActive[a])continue;
        sprintf(cmd, "hped %d * getsummary", a );
        SendCommand(cmd,FEM);
        //Set threshold
        sprintf(cmd, "hped %d * setthr %d %.1f", a , FEM.fecMetadata.asic_pedCenter[a], FEM.fecMetadata.asic_pedThr[a] );
        SendCommand(cmd,FEM);
      }
    }

  //Set Data server target to DAQ
  BroadcastCommand("serve_target 1",FEMArray);
}

void TRESTDAQARC::dataTaking(bool configure) {
  std::cout << "Starting data taking run" << std::endl;
  if(configure){
    //BroadcastCommand("daq 0x000000 F",FEMArray,false);
    BroadcastCommand("DAQ 0",FEMArray);
    BroadcastCommand("daq 0xFFFFFF F",FEMArray);
    BroadcastCommand("sca enable 0",FEMArray);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    BroadcastCommand("serve_target 0",FEMArray);
    std::this_thread::sleep_for(std::chrono::seconds(4));
    //Readout settings
    BroadcastCommand("modify_hit_reg 0",FEMArray);
    BroadcastCommand("emit_hit_cnt 1",FEMArray);
    BroadcastCommand("emit_empty_ch 1",FEMArray);
    BroadcastCommand("emit_lst_cell_rd 1",FEMArray);
    BroadcastCommand("keep_rst 1",FEMArray);
    BroadcastCommand("skip_rst 0",FEMArray);
    //AGET settings
      if(compressMode == daq_metadata_types::compressModeTypes::ALLCHANNELS || compressMode == daq_metadata_types::compressModeTypes::ZEROSUPPRESSION){
        BroadcastCommand("aget 3:0 mode 0x1",FEMArray);//Mode: 0x0: hit/selected channels 0x1:all channels
      } else if(compressMode == daq_metadata_types::compressModeTypes::TRIGGEREDCHANNELS){
        BroadcastCommand("aget 3:0 mode 0x0",FEMArray);//Mode: 0x0: hit/selected channels 0x1:all channels    
      }
    //BroadcastCommand("aget * tst_digout 1",FEMArray);//??

      for (auto &FEM : FEMArray){
        char cmd[200];
          for(int a=0;a<TRestRawDAQMetadata::nAsics;a++){
            if(!FEM.fecMetadata.asic_isActive[a])continue;
            sprintf(cmd, "aget %d dac 0x%X",a, FEM.fecMetadata.asic_coarseThr[a]);
            SendCommand(cmd,FEM);
            sprintf(cmd, "aget %d threshold 0x%X",a, FEM.fecMetadata.asic_fineThr[a]);
            SendCommand(cmd,FEM);
            sprintf(cmd, "mult_thr %d 0x%X",a, FEM.fecMetadata.asic_multThr[a]);
            SendCommand(cmd,FEM);
            sprintf(cmd, "mult_limit %d 0x%X",a, FEM.fecMetadata.asic_multLimit[a]);
            SendCommand(cmd,FEM);
          }
      }

      if(compressMode == daq_metadata_types::compressModeTypes::ZEROSUPPRESSION){
        BroadcastCommand("zero_suppress 1",FEMArray);
        BroadcastCommand("zs_pre_post 8 4",FEMArray);
      } else {
        BroadcastCommand("zero_suppress 0",FEMArray);
        BroadcastCommand("zs_pre_post 0 0",FEMArray);
      }

      //Event generator
      BroadcastCommand("clr tstamp",FEMArray);
      BroadcastCommand("clr evcnt",FEMArray);
      BroadcastCommand("event_limit 0x0",FEMArray);//Infinite
        if (triggerType ==  daq_metadata_types::triggerTypes::INTERNAL){
          BroadcastCommand("trig_rate 1 10",FEMArray);
          BroadcastCommand("trig_ena 0x1",FEMArray);
        } else if (triggerType ==  daq_metadata_types::triggerTypes::TCM) {
          BroadcastCommand("trig_ena 0x8",FEMArray);//tcm???
        } else if (triggerType ==  daq_metadata_types::triggerTypes::AUTO) {
          BroadcastCommand("trig_ena 0x0",FEMArray);
        }
    }

    BroadcastCommand("serve_target 1",FEMArray);//1: send to DAQ
    BroadcastCommand("sca enable 1",FEMArray);//Enable data taking
    BroadcastCommand("daq 0xFFFFFE F",FEMArray, false);//DAQ request
      //Wait till DAQ completion
      while ( !abrt && !nextFile && (daqMetadata->GetNEvents() == 0 || event_cnt < daqMetadata->GetNEvents())) {
        //Do something here? E.g. send packet request
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }
    BroadcastCommand("sca enable 0",FEMArray);
    BroadcastCommand("serve_target 0",FEMArray);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    BroadcastCommand("DAQ 0",FEMArray);
    BroadcastCommand("daq 0xFFFFFF F",FEMArray);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));//Wait some time till DAQ command is propagated
}

void TRESTDAQARC::stopDAQ() {
  
  stopReceiver = true;
  receiveThread.join();
  eventBuilderThread.join();

    for (auto &FEM : FEMArray)
      FEM.Close();
}

void TRESTDAQARC::BroadcastCommand(const char* cmd, std::vector<FEMProxy> &FEMA, bool wait){

    for (auto &FEM : FEMA){
      SendCommand(cmd,FEM,wait);
    }

  //if (verboseLevel >= REST_Debug)std::cout<<"Command sent "<<cmd<<std::endl;

}

void TRESTDAQARC::SendCommand(const char* cmd, FEMProxy &FEM, bool wait ){
   //if(abrt)return;
   std::unique_lock<std::mutex> lock(FEM.mutex_socket);
   if (sendto (FEM.client, cmd, strlen(cmd), 0, (struct sockaddr*)&(FEM.target), sizeof(struct sockaddr)) == -1) {
     std::string error ="sendto failed: " + std::string(strerror(errno));
     throw (TRESTDAQException(error));
   }

  lock.unlock();
  if (verboseLevel >= TRestStringOutput::REST_Verbose_Level::REST_Debug)std::cout<<"FEM "<<FEM.fecMetadata.id<<" Command sent "<<cmd<<std::endl;

    if(wait){
      //lock.lock();
      FEM.cmd_sent++;
      //lock.unlock();
      waitForCmd(FEM, cmd);
    }

}

void TRESTDAQARC::waitForCmd(FEMProxy &FEM, const char* cmd){

  int timeout = 0;
  bool condition;

    do {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      std::unique_lock<std::mutex> lock(FEM.mutex_socket);
      condition = (FEM.cmd_sent > FEM.cmd_rcv);
      lock.unlock();
      timeout++;
    } while ( condition && timeout <20);

  if (verboseLevel >= TRestStringOutput::REST_Verbose_Level::REST_Debug)std::cout<<"Cmd sent "<<FEM.cmd_sent<<" Cmd Received: "<<FEM.cmd_rcv<<std::endl;

  if(timeout>=20)std::cout<<"Cmd timeout "<<cmd<<std::endl;
}

void TRESTDAQARC::ReceiveThread( std::vector<FEMProxy> *FEMA ) {

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
  int err=0;
    while (!stopReceiver){

      // Copy the read fds from what we computed outside of the loop
      readfds_work = readfds;

        // Wait for any of these sockets to be ready
        if ((err = select(smax, &readfds_work, &writefds, &exceptfds, &t_timeout)) < 0){
           std::string error ="select failed: " + std::string(strerror(errno));
           throw (TRESTDAQException(error));
        }

        if(err == 0 )continue;//Nothing received

        for (auto &FEM : *FEMA){

          if (FD_ISSET(FEM.client, &readfds_work)){
            std::unique_lock<std::mutex> lock(FEM.mutex_socket);
            uint16_t buf_rcv[8192/(sizeof(uint16_t))];
            int length = recvfrom(FEM.client, buf_rcv, 8192, 0, (struct sockaddr*)&FEM.remote, &FEM.remote_size);
            lock.unlock();
              if (length < 0) {
                std::string error ="recvfrom failed: " + std::string(strerror(errno));
                throw (TRESTDAQException(error));
              }

            if (verboseLevel >= TRestStringOutput::REST_Verbose_Level::REST_Debug)std::cout<<"Packet received with length "<<length<<" bytes"<<std::endl;

            if(length>6){//empty frame?
              const size_t size = length/sizeof(uint16_t);//Note that length is in bytes while size is uint16_t

              if (verboseLevel >= TRestStringOutput::REST_Verbose_Level::REST_Debug)ARCPacket::DataPacket_Print(&buf_rcv[1], size-1);

                if(ARCPacket::isDataFrame(&buf_rcv[1])) {
                  std::unique_lock<std::mutex> lock_mem(FEM.mutex_mem);
                  //const std::deque<uint16_t> frame (&buf_rcv[1], &buf_rcv[size -1]);
                  FEM.buffer.insert(FEM.buffer.end(), &buf_rcv[1], &buf_rcv[size]);
                  const size_t bufferSize = FEM.buffer.size();
                  lock_mem.unlock();
                    if (verboseLevel >= TRestStringOutput::REST_Verbose_Level::REST_Debug)
                      std::cout<<"Packet buffered with size "<<(int)size-1<<" queue size: "<<bufferSize<<std::endl;
                    if( bufferSize > 1024*1024*1024){
                      std::string error ="Buffer FULL with size "+std::to_string(bufferSize/sizeof(uint16_t))+" bytes";
                      throw (TRESTDAQException(error));
                    }

               } else if (ARCPacket::isMFrame(&buf_rcv[1]) && isPed){
                  FEM.cmd_rcv++;
                  std::unique_lock<std::mutex> lock_mem(FEM.mutex_mem);
                  //const std::deque<uint16_t> frame (&buf_rcv[1], &buf_rcv[size -1]);
                  FEM.buffer.insert(FEM.buffer.end(), &buf_rcv[1], &buf_rcv[size]);
                  const size_t bufferSize = FEM.buffer.size();
                  lock_mem.unlock();
                  if (verboseLevel == TRestStringOutput::REST_Verbose_Level::REST_Info)ARCPacket::DataPacket_Print(&buf_rcv[1], size-1);
                } else {
                  //lock.lock();
                  FEM.cmd_rcv++;
                  //lock.unlock();
                }
            }
          }

        }
    }

  std::cout<<"End of receive Thread "<<err<<std::endl;

}

void TRESTDAQARC::EventBuilderThread(std::vector<FEMProxy> *FEMA, TRestRun *rR, TRestRawSignalEvent* sEvent){

  sEvent->Initialize();

  uint32_t ev_count=0;
  uint64_t ts=0;

  bool newEvent = true;
  bool emptyBuffer = true;

  do {
    emptyBuffer=true;
      for (auto &FEM : *FEMA){
        std::unique_lock<std::mutex> lock(FEM.mutex_mem);
        emptyBuffer &= FEM.buffer.empty();
        if(!FEM.buffer.empty()){
          if(FEM.pendingEvent){//Wait till we reach end of event for all the ARC
            FEM.pendingEvent = !ARCPacket::GetNextEvent( FEM.buffer, sEvent, ts, ev_count);
          }
        }
        lock.unlock();
      }

        newEvent = true;
        for (const auto &FEM : *FEMA){
            newEvent &= !FEM.pendingEvent;//Check if the event is pending
        }

      if(newEvent){//Save Event if closed
        if(rR){
          sEvent->SetID(ev_count);
          sEvent->SetTime( rR->GetStartTimestamp() + (double) ts * 2E-8 );
          FillTree(rR, sEvent);
          sEvent->Initialize();
          if(event_cnt%100 == 0)std::cout<<"Events "<<event_cnt<<std::endl;
            for (auto &FEM : *FEMA)FEM.pendingEvent = true;
        }
      }

    if(emptyBuffer)std::this_thread::sleep_for(std::chrono::milliseconds(100));
  } while(!(emptyBuffer && stopReceiver));

  //Save pedestal event
  if(isPed && emptyBuffer){
        if(rR){
          sEvent->SetID(ev_count);
          sEvent->SetTime( rR->GetStartTimestamp() + (double) ts * 2E-8 );
          FillTree(rR, sEvent);
          sEvent->Initialize();
        }
      }

}


