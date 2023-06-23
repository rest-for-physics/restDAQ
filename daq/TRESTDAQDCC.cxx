/*********************************************************************************
TRESTDAQT2K.cxx

Implement data acquisition for DCC + FEM + FEC based readout

Author: JuanAn Garcia 18/05/2021

*********************************************************************************/

#include "TRESTDAQDCC.h"

TRESTDAQDCC::TRESTDAQDCC(TRestRun* rR, TRestRawDAQMetadata* dM) : TRESTDAQ(rR, dM) { initialize(); }

void TRESTDAQDCC::initialize() {

    //Int_t baseIp[4];
    Int_t *baseIp = daqMetadata->GetFECs().front().ip;

    for(auto fec : daqMetadata->GetFECs()){
      FECMask |= (1 << fec.id);

        for (int i=0;i<4;i++){
          if (baseIp[i] != fec.ip[i] ){
            throw (TRESTDAQException("Inconsistent IP between FECs, please check RML"));
          }
        }
    }

    if (triggerType != daq_metadata_types::triggerTypes::INTERNAL && triggerType != daq_metadata_types::triggerTypes::EXTERNAL){
      std::cerr << "DCC only supports internal or external trigger" << std::endl;
      throw (TRESTDAQException("Unsupported trigger type, please check RML"));
    }

     if (compressMode != daq_metadata_types::compressModeTypes::ALLCHANNELS && compressMode != daq_metadata_types::compressModeTypes::ZEROSUPPRESSION){
      std::cerr << "DCC only supports allchannels or zerosuppression compress mode" << std::endl;
      throw (TRESTDAQException("Unsupported compress mode, please check RML"));
    }

    dcc_socket.Open(daqMetadata->GetFECs().front().ip, REMOTE_DST_PORT);

}

void TRESTDAQDCC::configure() {
    std::cout << "Configuring readout" << std::endl;

    char cmd[200];
    if( SendCommand("fem 0") == DCCPacket::packetReply::ERROR)//Check first command, thow exception if DCC doesn't reply
      throw (TRESTDAQException("I didn't get reply from the DCC, please check the network"));

    int pk = 0xffc0 | ~FECMask;
    sprintf(cmd, "pokeb 0x4 0x%X", pk);  // Set FEC mask, note that is inverted
    SendCommand(cmd);

    // SendCommand("triglat 3 2000"); //Trigger latency
    SendCommand("pokes 0x8 0x0000");  // Disable pulser
    SendCommand("sca cnt 0x1ff");     // Set 511 bin count, make configurable?

    SendCommand("pokes 0x16 0x0");    // Set trigger delay
    SendCommand("pokes 0x1A 0x1ff");  // SCA delay and trigger
      if ( acqType == daq_metadata_types::acqTypes::PEDESTAL) {
        SendCommand("pokes 0x14 0x400");  // Set SCA readback offset
      } else {
        SendCommand("pokes 0x14 0xc00");  // Set SCA readback offset
      }

      for(auto fec : daqMetadata->GetFECs()) {
        sprintf(cmd, "fec %d", fec.id);
        SendCommand(cmd);
        sprintf(cmd, "sca wck 0x%X", fec.clockDiv);  // Clock div
        SendCommand(cmd);
      }

    // Configure AFTER chip
    for(auto fec : daqMetadata->GetFECs()) {
        sprintf(cmd, "fec %d", fec.id);
        SendCommand(cmd);
          for (int a = 0; a < TRestRawDAQMetadata::nAsics; a++) {
            if(!fec.asic_isActive[a])continue;
            unsigned int reg = ( (fec.asic_shappingTime[a] & 0xF) << 3) | ( (fec.asic_gain[a] & 0x3) << 1) ;
            sprintf(cmd, "asic %d write 1 0x%X", a, reg);  // Gain and shapping time
            SendCommand(cmd);
            sprintf(cmd, "asic %d write 2 0xA000", a);  // Output buffers
            SendCommand(cmd);
            sprintf(cmd, "asic %d write 3 0x3f 0xffff 0xffff", a);
            SendCommand(cmd);
            sprintf(cmd, "asic %d write 4 0x3f 0xffff 0xffff", a);
            SendCommand(cmd);
          }
    }
    // SendCommand("isobus 0x0F", -1);//Reset event counter, timestamp set eventType to test
}

void TRESTDAQDCC::startDAQ() {

    if ( acqType == daq_metadata_types::acqTypes::PEDESTAL) {
        pedestal();
    } else {
        dataTaking();
    }
}

void TRESTDAQDCC::pedestal() {
    std::cout << "Starting pedestal run" << std::endl;

    char cmd[200];
    SendCommand("fem 0");
    SendCommand("pokes 0x14 0x0");  // Set SCA readback offset
    SendCommand("hbusy clr");
    SendCommand("fem 0");
    for(auto fec : daqMetadata->GetFECs()) {
      sprintf(cmd, "hped clr %d * *", fec.id);
      SendCommand(cmd);            ////Clear pedestals
    }

    SendCommand("isobus 0x4F");  // Reset event counter, timestamp for type 10

    int loopN = 0;
      while (!abrt && (loopN++ < daqMetadata->GetNPedestalEvents())) {
        SendCommand("isobus 0x6C");     // SCA start
        SendCommand("isobus 0x1C");     // SCA stop
        waitForTrigger();
        SendCommand("fem 0");
          for(auto fec : daqMetadata->GetFECs()) {
            for (int a = 0; a < TRestRawDAQMetadata::nAsics; a++) {
              if(!fec.asic_isActive[a])continue;
              sprintf(cmd, "hped acc %d %d %d:%d", fec.id, a, fec.asic_channelStart[a], fec.asic_channelEnd[a]);
              SendCommand(cmd);  // Get pedestals
            }
          }
      }

    fSignalEvent.Initialize();
    fSignalEvent.SetID(0);
    fSignalEvent.SetTime(getCurrentTime());

    SendCommand("fem 0");
      for(auto fec : daqMetadata->GetFECs()) {
            for (int a = 0; a < TRestRawDAQMetadata::nAsics; a++) {
              if(!fec.asic_isActive[a])continue;
              sprintf(cmd, "hped getsummary %d %d %d:%d", fec.id, a, fec.asic_channelStart[a], fec.asic_channelEnd[a]);
              SendCommand(cmd, DCCPacket::packetType::BINARY, 1, DCCPacket::packetDataType::PEDESTAL);  // Get summary
              sprintf(cmd, "hped centermean %d %d %d:%d %d", fec.id, a, fec.asic_channelStart[a], fec.asic_channelEnd[a], fec.asic_pedCenter[a]);
              SendCommand(cmd);  // Set mean
              sprintf(cmd, "hped setthr %d %d %d:%d %d %.1f", fec.id, a, fec.asic_channelStart[a], fec.asic_channelEnd[a], fec.asic_pedCenter[a], fec.asic_pedThr[a]);
              SendCommand(cmd);  // Set threshold
            }
          }

    FillTree(restRun, &fSignalEvent);
}

void TRESTDAQDCC::dataTaking() {
    std::cout << "Starting data taking run" << std::endl;

    char cmd[200];
    SendCommand("fem 0");  // Needed?
    // if(comp)SendCommand("skipempty 1", -1);//Skip empty frames in compress mode
    // else SendCommand("skipempty 0", -1);//Save empty frames if not
    SendCommand("isobus 0x4F");  // Reset event counter, timestamp for type 11

    while ( !abrt && !nextFile && (daqMetadata->GetNEvents() == 0 || event_cnt < daqMetadata->GetNEvents())) {
        SendCommand("fem 0");

        SendCommand("isobus 0x6C");// SCA start
        if (triggerType ==  daq_metadata_types::triggerTypes::INTERNAL) SendCommand("isobus 0x1C");  // SCA stop case of internal trigger
        fSignalEvent.Initialize();
        fSignalEvent.SetID(event_cnt);
        waitForTrigger();
        // Perform data acquisition phase, compress, accept size
        fSignalEvent.SetTime(getCurrentTime());
        int mode = compressMode == daq_metadata_types::compressModeTypes::ZEROSUPPRESSION? 1 : 0;
          for(auto fec : daqMetadata->GetFECs()) {
            for (int a = 0; a < TRestRawDAQMetadata::nAsics; a++) {
              if(!fec.asic_isActive[a])continue;
              sprintf(cmd, "areq %d %d %d %d %d", mode, fec.id, a, fec.asic_channelStart[a], fec.asic_channelEnd[a]);
              SendCommand(cmd, DCCPacket::packetType::BINARY, 0, DCCPacket::packetDataType::EVENT);
            }
          }
        if(fSignalEvent.GetNumberOfSignals() >0 )FillTree(restRun, &fSignalEvent);
    }
}

void TRESTDAQDCC::stopDAQ() {
    dcc_socket.Close();
    std::cout << "Run stopped" << std::endl;
}

void TRESTDAQDCC::saveEvent(unsigned char* buf, int size) {
    // If data supplied, copy to temporary buffer
    if (size <= 0) return;
    DCCPacket::DataPacket* dp = (DCCPacket::DataPacket*)buf;

    //Check if packet has ADC data
    if( GET_TYPE(ntohs(dp->hdr) ) != RESP_TYPE_ADC_DATA )return;

    const unsigned int scnt = ntohs(dp->scnt);
    if ((scnt <= 8) && (ntohs(dp->samp[0]) == 0) && (ntohs(dp->samp[1]) == 0)) return;  // empty data
    if ((scnt <= 12) && ((ntohs(dp->samp[0]) == 0x11ff) || (ntohs(dp->samp[1]) == 0x11ff) ) ) return;  // Data starting at 511 bin

    unsigned short fec, asic, channel;
    const unsigned short arg1 = GET_RB_ARG1(ntohs(dp->args));
    const unsigned short arg2 = GET_RB_ARG2(ntohs(dp->args));

    int physChannel = DCCPacket::Arg12ToFecAsicChannel(arg1, arg2, fec, asic, channel);

    if (physChannel < 0) return;

    if (verboseLevel >= TRestStringOutput::REST_Verbose_Level::REST_Debug)
        std::cout << "FEC " << fec << " asic " << asic << " channel " << channel << " physChann " << physChannel << "\n";

   //bool compress = GET_RB_COMPRESS(ntohs(dp->args) );
   std::vector<Short_t> sData(512, 0);
   Short_t *sDataPtr = sData.data();

   unsigned short timeBin = 0;

      for (unsigned int i = 0; i < scnt && i<511; i++) {
        Short_t data = ntohs(dp->samp[i]);
          if (((data & 0xFE00) >> 9) == 8) {
              timeBin = GET_CELL_INDEX(data);
          } else if ((((data & 0xF000) >> 12) == 0)) {//Check fastest method
              if (timeBin < 512) sDataPtr[timeBin] = std::move(data);
              //if (timeBin < 512) sDataPtr[timeBin] = data;
              //if (timeBin < 512) memcpy(&sData[timeBin],&data,sizeof(Short_t));
              //if (timeBin < 512) std::copy(&sData[timeBin],&sData[timeBin], &data);
              timeBin++;
          }
      }

    TRestRawSignal rawSignal(physChannel, sData);
    fSignalEvent.AddSignal(rawSignal);
}

void TRESTDAQDCC::savePedestals(unsigned char* buf, int size) {
    // If data supplied, copy to temporary buffer
    if (size <= 0) return;
    DCCPacket::DataPacket* dp = (DCCPacket::DataPacket*)buf;

    //Check if packet has ADC data
    if( GET_TYPE(ntohs(dp->hdr) ) != RESP_TYPE_HISTOSUMMARY )return;

    DCCPacket::PedestalHistoSummaryPacket* pck = (DCCPacket::PedestalHistoSummaryPacket*) dp;

    unsigned short fec, asic;
    const unsigned short arg1 = GET_RB_ARG1(ntohs(pck->args));
    const unsigned short arg2 = GET_RB_ARG2(ntohs(pck->args));

    const unsigned int nbsw = (ntohs(pck->size) - 2 - 6 - 2) / sizeof(short);

      for (unsigned short ch = 0; ch < (nbsw / 2); ch++) {

        const Short_t mean = ntohs(pck->stat[ch].mean);
        const Short_t stdev = ntohs(pck->stat[ch].stdev);

        const int physChannel = DCCPacket::Arg12ToFecAsic(arg1, arg2, fec, asic, ch);

        if (verboseLevel < TRestStringOutput::REST_Verbose_Level::REST_Debug){
          std::cout << "FEC " << fec << " asic " << asic << " channel " << ch << " physChann " << physChannel << " mean "<< (double)(mean) / 100.0 << " stddev " << (double)(stdev) / 100.0 << std::endl;
        }

        if(physChannel < 0 )continue;

        std::vector<Short_t> sData = {mean, stdev};

        TRestRawSignal rawSignal(physChannel, sData);
        fSignalEvent.AddSignal(rawSignal);
      }
}

DCCPacket::packetReply TRESTDAQDCC::SendCommand(const char* cmd, DCCPacket::packetType pckType, size_t nPackets, DCCPacket::packetDataType dataType) {
    if (sendto(dcc_socket.client, cmd, strlen(cmd), 0, (struct sockaddr*)&(dcc_socket.target), sizeof(struct sockaddr)) == -1) {
        std::string error ="sendto failed: " + std::string(strerror(errno));
        throw (TRESTDAQException(error));
    }

      if (verboseLevel >= TRestStringOutput::REST_Verbose_Level::REST_Debug)std::cout<<"Command sent "<<cmd<<std::endl;

    // wait for incoming messages
    bool done = false;
    size_t pckCnt = 1;
    uint8_t buf_rcv[8192];
    uint8_t* buf_ual;

    while (!done) {
        int length;
        int cnt = 0;
        auto startTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::duration<int>>(std::chrono::steady_clock::now() - startTime);

        do {
            length = recvfrom(dcc_socket.client, buf_rcv, 8192, 0, (struct sockaddr*)&dcc_socket.remote, &dcc_socket.remote_size);

            if (length < 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    if (cnt % 1000 == 0) {
                        duration = std::chrono::duration_cast<std::chrono::duration<int>>(std::chrono::steady_clock::now() - startTime);
                        if (verboseLevel >= TRestStringOutput::REST_Verbose_Level::REST_Extreme) fprintf(stderr, "socket() failed: %s\n", strerror(errno));
                    }
                } else {
                  std::string error ="recvfrom failed: " + std::string(strerror(errno));
                  throw (TRESTDAQException(error));
                }
            }
            cnt++;
        } while (length < 0 && duration.count() < 10 && !abrt);

        if(abrt){
          std::cerr << "Run aborted" << std::endl;
            return DCCPacket::packetReply::ERROR;
        }

        if (duration.count() >= 10 || length < 0) {
            std::cerr << "No reply after " << duration.count() << " seconds, missing packets are expected" << std::endl;
            return DCCPacket::packetReply::ERROR;
        }

        // if the first 2 bytes are null, UDP datagram is aligned on next 32-bit boundary, so skip these first
        // two bytes
        if ((buf_rcv[0] == 0) && (buf_rcv[1] == 0)) {
            buf_ual = &buf_rcv[2];
            length -= 2;
        } else {
            buf_ual = &buf_rcv[0];
        }

        // show packet if desired
        if (verboseLevel >= TRestStringOutput::REST_Verbose_Level::REST_Debug) {
            printf("dcc().rep(): %d bytes of data \n", length);
            if (pckType == DCCPacket::packetType::BINARY) {
                DCCPacket::DataPacket* data_pk = (DCCPacket::DataPacket*)buf_ual;
                DCCPacket::DataPacket_Print(data_pk);
            } else {
                *(buf_ual + length) = '\0';
                printf("dcc().rep(): %s", buf_ual);
            }
        }

        if ((*buf_ual == '-')) {  // ERROR ASCII packet
            if (verboseLevel >= TRestStringOutput::REST_Verbose_Level::REST_Debug) printf("ERROR packet: %s\n", buf_ual);
            return DCCPacket::packetReply::RETRY;
        }

        if (pckType == DCCPacket::packetType::BINARY) {  // DAQ packet

            DCCPacket::DataPacket* data_pkt = (DCCPacket::DataPacket*)buf_ual;

            if(dataType == DCCPacket::packetDataType::EVENT){
              saveEvent(buf_ual, length);
            } else if(dataType == DCCPacket::packetDataType::PEDESTAL) {
              savePedestals(buf_ual, length);
            }

            // Check End Of Event
            if (GET_FRAME_TY_V2(ntohs(data_pkt->dcchdr)) & FRAME_FLAG_EOEV || GET_FRAME_TY_V2(ntohs(data_pkt->dcchdr)) & FRAME_FLAG_EORQ ||
                (nPackets >0 && pckCnt >= nPackets) ) {
                done = true;
            }

        } else {
            done = true;  // ASCII Packet, check response?
        }

        pckCnt++;
    }
    return DCCPacket::packetReply::OK;
}

void TRESTDAQDCC::waitForTrigger() {  // Wait till trigger is acquired
    DCCPacket::packetReply reply;
    do {
        reply = SendCommand("wait 1000000");                   // Wait for the event to be acquired
    } while (reply == DCCPacket::packetReply::RETRY && !abrt);  // Infinite loop till aborted or wait succeed
}

