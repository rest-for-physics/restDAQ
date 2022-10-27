/*********************************************************************************
TRESTDAQFEMINOS.h

Implement data acquisition for FEMINOS + FEC based readout

Author: JuanAn Garcia 18/08/2021

Based on mclient program from Denis Calvet

*********************************************************************************/

#ifndef __TREST_DAQ_FEMINOS__
#define __TREST_DAQ_FEMINOS__

#include <deque>
#include <iostream>
#include <memory>
#include <thread>

#include "TRESTDAQ.h"
#include "TRESTDAQSocket.h"

class FEMProxy : public TRESTDAQSocket {
   public:
    FEMProxy() {}
    bool pendingEvent = false;
    FECMetadata fecMetadata;
    // std::atomic_int
    int cmd_sent = 0;
    // std::atomic_int
    int cmd_rcv = 0;

    std::deque<uint16_t> buffer;

    inline static std::mutex mutex_socket;
    inline static std::mutex mutex_mem;
};

class TRESTDAQFEMINOS : public TRESTDAQ {
   public:
    TRESTDAQFEMINOS(TRestRun* run, TRestRawDAQMetadata* metadata);

    void configure() override;
    void startDAQ() override;
    void stopDAQ() override;
    void initialize() override;
    void startUp() override;

    static void ReceiveThread(std::vector<FEMProxy>* FEMA);
    static void ReceiveBuffer(FEMProxy& FEM);
    static void EventBuilderThread(std::vector<FEMProxy>* FEMA, TRestRun* run, TRestRawSignalEvent* sEvent);
    static void waitForCmd(FEMProxy& FEM);
    static std::atomic<bool> stopReceiver;

    inline static std::mutex mutex;

   private:
    void pedestal();
    void dataTaking();
    void BroadcastCommand(const char* cmd, std::vector<FEMProxy>& FEMA, bool wait = true);
    void SendCommand(const char* cmd, FEMProxy& FEM, bool wait = true);

    std::vector<FEMProxy> FEMArray;  // Vector of FEMINOS

    std::thread receiveThread, eventBuilderThread;
};

#endif
