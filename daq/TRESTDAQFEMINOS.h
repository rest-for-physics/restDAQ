/*********************************************************************************
TRESTDAQFEMINOS.h

Implement data acquisition for FEMINOS + FEC based readout

Author: JuanAn Garcia 18/08/2021

Based on mclient program from Denis Calvet

*********************************************************************************/

#ifndef __TREST_DAQ_FEMINOS__
#define __TREST_DAQ_FEMINOS__

#include "TRESTDAQ.h"
#include "TRESTDAQSocket.h"

#include <iostream>
#include <thread>
#include <memory>

class FEMProxy : public TRESTDAQSocket {
  public:
    bool pendingEvent=false;
    FECMetadata fecMetadata;

    std::vector<std::vector<uint16_t>> buffer;
};

class TRESTDAQFEMINOS : public TRESTDAQ {
  public:
    TRESTDAQFEMINOS(TRestRun* rR, TRestRawDAQMetadata* dM);

    virtual void configure();
    virtual void startDAQ();
    virtual void stopDAQ();
    virtual void initialize();
    virtual void startUp();

    static void ReceiveThread(std::vector<FEMProxy> *FEMA);
    static void ReceiveBuffer(FEMProxy &FEM);
    static void EventBuilderThread(std::vector<FEMProxy> *FEMA, TRestRun *rR, TRestRawSignalEvent* sEvent);
    static std::atomic<bool> stopReceiver;

    inline static std::mutex mutex;
    inline static const uint16_t MAX_BUFFER_SIZE = 512;

  private:
    void pedestal();
    void dataTaking();
    void BroadcastCommand(const char* cmd, std::vector<FEMProxy> &FEMA);
    void SendCommand(const char* cmd, FEMProxy &FEM);

    std::vector<FEMProxy> FEMArray;//Vector of FEMINOS

    std::thread receiveThread, eventBuilderThread;

};

#endif
