/*********************************************************************************
TRESTDAQT2K.cxx

Implement data acquisition for DCC + FEM + FEC based readout

Author: JuanAn Garcia 18/05/2021

*********************************************************************************/

#include "TRESTDAQDCC.h"

TRESTDAQDCC::TRESTDAQDCC(TRestRun* rR, TRestRawDAQMetadata* dM) : TRESTDAQ(rR, dM) { initialize(); }

void TRESTDAQDCC::initialize() {
  dcc_socket.Open(GetDAQMetadata()->GetBaseIp(),GetDAQMetadata()->GetLocalIp(), REMOTE_DST_PORT);

    for(auto fec : GetDAQMetadata()->GetFECs()){
      FECMask |= (1 << fec.id);
        if(nFECs==0)startFEC = fec.id;
        endFEC = fec.id;
      nFECs++;
    }

}

void TRESTDAQDCC::configure() {
    std::cout << "Configuring readout" << std::endl;

    char cmd[200];
    if( SendCommand("fem 0") == DCCPacket::packetReply::ERROR)//Check first command, thow exception if DCC doesn't reply
      throw (TRESTDAQException("I didn't get reply from the DCC, please check the network"));

    // SendCommand("triglat 3 2000");//Trigger latency
    SendCommand("pokes 0x8 0x0000");                                // Disable pulser
    SendCommand("sca cnt 0x1ff");                                   // Set 511 bin count, make configurable?
    sprintf(cmd, "sca wck 0x%X", GetDAQMetadata()->GetClockDiv());  // Clock div
    SendCommand(cmd);
    SendCommand("pokes 0x16 0x0");    // Set trigger delay
    SendCommand("pokes 0x1A 0x1ff");  // SCA delay and trigger
      if (GetDAQMetadata()->GetAcquisitionType() == "pedestal"){
        SendCommand("pokes 0x14 0x400");  // Set SCA readback offset
      } else {
        SendCommand("pokes 0x14 0xc00");  // Set SCA readback offset
      }
    int pk = 0xffc0 | FECMask;
    sprintf(cmd, "pokeb 0x4 0x%X", pk);  // Set FEC mask, note that is inverted
    SendCommand(cmd);
    // SendCommand("pokeb 0x5 0x0",-1);//Deplecated?

    // Configure AFTER chip
    for(auto fec : GetDAQMetadata()->GetFECs()) {
        sprintf(cmd, "fec %d", fec.id);
        SendCommand(cmd);
          for (int a = 0; a < 4; a++) {
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

void TRESTDAQDCC::pedestal() {
    std::cout << "Starting pedestal run" << std::endl;

    char cmd[200];
    SendCommand("fem 0");
    SendCommand("pokes 0x14 0x0");  // Set SCA readback offset
    SendCommand("hbusy clr");
    SendCommand("fem 0");
    sprintf(cmd, "hped clr %d:%d * *", startFEC, endFEC);
    SendCommand(cmd);            ////Clear pedestals
    SendCommand("isobus 0x4F");  // Reset event counter, timestamp for type 10

    int loopN = 0;
    while (!abrt && (loopN++ < 100)) {  // 100 loops hardcoded TODO change
        SendCommand("isobus 0x6C");     // SCA start
        SendCommand("isobus 0x1C");     // SCA stop
        waitForTrigger();
        SendCommand("fem 0");
          for(auto fec : GetDAQMetadata()->GetFECs()) {
            for (int a = 0; a < 4; a++) {
              if(!fec.asic_isActive[a])continue;
              sprintf(cmd, "hped acc %d %d %d:%d", fec.id, a, fec.asic_channelStart[a], fec.asic_channelEnd[a]);
              SendCommand(cmd);  // Get pedestals
            }
          }   
    }

    SendCommand("fem 0");
      for(auto fec : GetDAQMetadata()->GetFECs()) {
            for (int a = 0; a < 4; a++) {
              if(!fec.asic_isActive[a])continue;
              sprintf(cmd, "hped getsummary %d %d %d:%d", fec.id, a, fec.asic_channelStart[a], fec.asic_channelEnd[a]);
              SendCommand(cmd, DCCPacket::packetType::BINARY, 1);  // Get summary
              sprintf(cmd, "hped centermean %d %d %d:%d %d", fec.id, a, fec.asic_channelStart[a], fec.asic_channelEnd[a], fec.asic_pedCenter[a]);
              SendCommand(cmd);  // Set mean
              sprintf(cmd, "hped setthr %d %d %d:%d %d %.1f", fec.id, a, fec.asic_channelStart[a], fec.asic_channelEnd[a], fec.asic_pedCenter[a], fec.asic_pedThr[a]);
              SendCommand(cmd);  // Set threshold
            }
          }
}

void TRESTDAQDCC::dataTaking() {
    std::cout << "Starting data taking run" << std::endl;

    char cmd[200];
    SendCommand("fem 0");  // Needed?
    // if(comp)SendCommand("skipempty 1", -1);//Skip empty frames in compress mode
    // else SendCommand("skipempty 0", -1);//Save empty frames if not
    SendCommand("isobus 0x4F");  // Reset event counter, timestamp for type 11

    while (!abrt && (GetDAQMetadata()->GetNEvents() == 0 || event_cnt < GetDAQMetadata()->GetNEvents())) {
        SendCommand("fem 0");

        SendCommand("isobus 0x6C");// SCA start
        if (GetDAQMetadata()->GetTriggerType() == "internal") SendCommand("isobus 0x1C");  // SCA stop case of internal trigger
        GetSignalEvent()->Initialize();
        GetSignalEvent()->SetID(event_cnt);
        waitForTrigger();
        // Perform data acquisition phase, compress, accept size
        GetSignalEvent()->SetTime(getCurrentTime());
          for(auto fec : GetDAQMetadata()->GetFECs()) {
            for (int a = 0; a < 4; a++) {
              if(!fec.asic_isActive[a])continue;
              sprintf(cmd, "areq %d %d %d %d %d", GetDAQMetadata()->GetCompressMode(), fec.id, a, fec.asic_channelStart[a], fec.asic_channelEnd[a]);
              SendCommand(cmd, DCCPacket::packetType::BINARY);
            }
          }

        FillTree(GetRestRun(),GetSignalEvent());
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

    unsigned short fec, asic, channel;
    unsigned short arg1 = GET_RB_ARG1(ntohs(dp->args));
    unsigned short arg2 = GET_RB_ARG2(ntohs(dp->args));

    int physChannel = DCCPacket::Arg12ToFecAsicChannel(arg1, arg2, fec, asic, channel);

    if (physChannel < 0) return;

    if (verboseLevel >= TRestStringOutput::REST_Verbose_Level::REST_Debug)
        std::cout << "FEC " << fec << " asic " << asic << " channel " << channel << " physChann " << physChannel << "\n";

   //bool compress = GET_RB_COMPRESS(ntohs(dp->args) );
   std::vector<Short_t> sData(512, 0);
   Short_t *sDataPtr = sData.data();

   unsigned short timeBin = 0;

      for (int i = 0; i < scnt && i<511; i++) {
        Short_t data = ntohs(dp->samp[i]);
          if (((data & 0xFE00) >> 9) == 8) {
              timeBin = GET_CELL_INDEX(data);
          } else if ((((data & 0xF000) >> 12) == 0)) {//Check fastest method
              if (timeBin < 512) sDataPtr[timeBin] = std::move(data);
              //if (timeBin < 512) sDataPtr[timeBin] = data;
              //if (timeBin < 512)memcpy(&sData[timeBin],&data,sizeof(Short_t));
              //if (timeBin < 512)std::copy(&sData[timeBin],&sData[timeBin], &data);
              timeBin++;
          }
      }

    TRestRawSignal rawSignal(physChannel, sData);
    GetSignalEvent()->AddSignal(rawSignal);
}

DCCPacket::packetReply TRESTDAQDCC::SendCommand(const char* cmd, DCCPacket::packetType pckType, size_t nPackets) {
    if (sendto(dcc_socket.client, cmd, strlen(cmd), 0, (struct sockaddr*)&(dcc_socket.target), sizeof(struct sockaddr)) == -1) {
        std::string error ="sendto failed: " + std::string(strerror(errno));
        throw (TRESTDAQException(error));
        //return DCCPacket::packetReply::ERROR;
    }

      if (verboseLevel >= TRestStringOutput::REST_Verbose_Level::REST_Debug)std::cout<<"Command sent "<<cmd<<std::endl;

    // wait for incoming messages
    bool done = false;
    int pckCnt = 1;
    uint8_t buf_rcv[8192];
    uint8_t* buf_ual;
    DCCPacket::packetReply errRep = DCCPacket::packetReply::OK;

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

            if(nPackets==0)saveEvent(buf_ual, length);

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

