/*********************************************************************************
TRESTDAQARC.h

Implement data acquisition for ARC based readout

Author: JuanAn Garcia 15/04/2024

Based on pclient program from Denis Calvet

*********************************************************************************/

#ifndef __TREST_DAQ_ARC__
#define __TREST_DAQ_ARC__

#include "TRESTDAQ.h"
#include "FEMProxy.h"

#include <iostream>
#include <thread>
#include <memory>

class TRESTDAQARC : public TRESTDAQ {
  public:
    TRESTDAQARC(TRestRun* rR, TRestRawDAQMetadata* dM);

    void configure() override;
    void startDAQ(bool configure=true) override;
    void stopDAQ() override;
    void initialize() override;
    void startUp() override;

    static void ReceiveThread(std::vector<FEMProxy> *FEMA);
    static void ReceiveBuffer(FEMProxy &FEM);
    static void EventBuilderThread(std::vector<FEMProxy> *FEMA, TRestRun *rR, TRestRawSignalEvent* sEvent);
    static void waitForCmd(FEMProxy &FEM, const char* cmd);
    static std::atomic<bool> stopReceiver;
    static std::atomic<bool> isPed;

  private:
    void pedestal();
    void dataTaking(bool configure=true);
    void BroadcastCommand(const char* cmd, std::vector<FEMProxy> &FEMA, bool wait=true);
    void SendCommand(const char* cmd, FEMProxy &FEM, bool wait=true);

    std::vector<FEMProxy> FEMArray;//Vector of ARC

    std::thread receiveThread, eventBuilderThread;

};

#endif
