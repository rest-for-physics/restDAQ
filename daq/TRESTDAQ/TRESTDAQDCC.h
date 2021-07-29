/*********************************************************************************
TRESTDAQT2K.h

Implement data acquisition for DCC + FEM + FEC based readout

Author: JuanAn Garcia 18/05/2021

Based on mclient program from Denis Calvet

*********************************************************************************/

#ifndef __TREST_DAQ_DCC__
#define __TREST_DAQ_DCC__

#include "TRESTDAQ.h"

extern "C" {
#include "after.h"
#include "endofevent_packet.h"
#include "ethpacket.h"
#include "fem.h"
#include "gblink.h"
#include "platform_spec.h"
#include "sock_util.h"
}

#include <sys/socket.h>
#include <iostream>

namespace DCCPacket {
enum class packetReply { ERROR = -1, RETRY = 0, OK = 1 };
enum class packetType { ASCII = 0, BINARY = 1 };
const uint32_t MAX_EVENT_SIZE = 6 * 4 * 79 * 2 * (512 + 32) * 12;
};  // namespace DCCPacket

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
  void Open(int *rem_ip_base, int rpt);
  
};

class TRESTDAQDCC : public TRESTDAQ {
   public:
    TRESTDAQDCC(TRestRun *rR, TRestRawDAQMetadata *dM);
   
    virtual void configure();
    virtual void startDAQ();
    virtual void stopDAQ();
    virtual void initialize();

   private:
    void pedestal();
    void dataTaking();
    DCCPacket::packetReply SendCommand(const char* cmd, DCCPacket::packetType type = DCCPacket::packetType::ASCII, size_t nPackets = 1);

    void waitForTrigger();
    void saveEvent(unsigned char *buf, int size);

    //TODO GetChannels from decoding file?
    int startFEC;
    int endFEC;
    int chStart;
    int chEnd;
    int nFECs;
    int nASICs =4;

    const int REMOTE_DST_PORT = 1122;

    // Socket
    DccSocket dcc_socket;
};

#endif

