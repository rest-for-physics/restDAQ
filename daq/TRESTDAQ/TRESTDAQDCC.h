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
#include "platform_spec.h"
#include "sock_util.h"
#include "gblink.h"
#include "after.h"
#include "fem.h"
#include "ethpacket.h"
#include "endofevent_packet.h"
}

#include <iostream>
#include <sys/socket.h>

namespace DCCPacket {
  enum class packetReply {ERROR = -1, RETRY = 0, OK = 1 };
  enum class packetType {ASCII =0, BINARY = 1};
  const uint32_t MAX_EVENT_SIZE = 6*4*79*2*(512+32)*12;
};

class DCCEvent {

  public:

  DCCEvent( bool comp, const std::string &fileName );
  ~DCCEvent( );

  int ev_sz;
  uint8_t *cur_ptr;
  uint8_t data[DCCPacket::MAX_EVENT_SIZE];
  FILE *fout;
  bool compress;
  bool fileOpen;

  void startEvent ( );
  void addData(uint8_t *buf, int size);
  void writeEvent( int evnb);

};

class TRESTDAQDCC : public TRESTDAQ {

  public:

    virtual void configure();
    virtual void startDAQ();
    virtual void stopDAQ();
    virtual void initialize();

  private:

  DCCPacket::packetReply SendCommand( const char* cmd, DCCPacket::packetType type = DCCPacket::packetType::ASCII, size_t nPackets = 1 );
  
  void waitForTrigger( );

  //Event
  DCCEvent *event;
  int m_event_cnt;

  //Socket
  int m_sockfd;
  struct sockaddr_in m_target;
  struct sockaddr_in m_remote;

};

#endif

