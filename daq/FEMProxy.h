/*********************************************************************************
FEMProxy.h

Author: JuanAn Garcia 18/08/2021

Based on mclient program from Denis Calvet

*********************************************************************************/

#ifndef __FEM_PROXY__
#define __FEM_PROXY__

#include <deque>

#include "TRestRawDAQMetadata.h"
#include "TRESTDAQSocket.h"

class FEMProxy : public TRESTDAQSocket {
  
  public:
    FEMProxy(){ }
    bool pendingEvent=true;
    TRestRawDAQMetadata::FECMetadata fecMetadata;
    //std::atomic_int
    int cmd_sent=0;
    //std::atomic_int
    int cmd_rcv=0;

    std::deque <uint16_t> buffer;

    inline static std::mutex mutex_socket;
    inline static std::mutex mutex_mem;
};

#endif
