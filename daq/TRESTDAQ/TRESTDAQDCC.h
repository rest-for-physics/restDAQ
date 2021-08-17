/*********************************************************************************
TRESTDAQT2K.h

Implement data acquisition for DCC + FEM + FEC based readout

Author: JuanAn Garcia 18/05/2021

Based on mclient program from Denis Calvet

*********************************************************************************/

#ifndef __TREST_DAQ_DCC__
#define __TREST_DAQ_DCC__

#include "TRESTDAQ.h"
#include "DCCPacket.h"

#include <sys/socket.h>

#include <iostream>

class DccSocket {
   public:
    int client;
    struct sockaddr_in target;
    unsigned char* target_adr;

    struct sockaddr_in remote;
    unsigned int remote_size;
    int rem_port;

    void Close();
    void Clear();
    void Open(int* rem_ip_base, int rpt);
};

class TRESTDAQDCC : public TRESTDAQ {
   public:
    TRESTDAQDCC(TRestRun* rR, TRestRawDAQMetadata* dM);

    virtual void configure();
    virtual void startDAQ();
    virtual void stopDAQ();
    virtual void initialize();

   private:
    void pedestal();
    void dataTaking();
    DCCPacket::packetReply SendCommand(const char* cmd, DCCPacket::packetType type = DCCPacket::packetType::ASCII, size_t nPackets = 0);

    void waitForTrigger();
    void saveEvent(unsigned char* buf, int size);

    // TODO GetChannels from decoding file?
    int startFEC;
    int endFEC;
    int chStart=3;
    int chEnd=78;
    int nFECs=0;
    int nASICs = 4;

    const int REMOTE_DST_PORT = 1122;

    // Socket
    DccSocket dcc_socket;
};

#endif
