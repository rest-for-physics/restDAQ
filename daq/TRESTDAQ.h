/*********************************************************************************
TRESTDAQ.h

Base class for different DAQs: T2K, FEMINOS or any other

Author: JuanAn Garcia 18/05/2021

To implement generic methods here

*********************************************************************************/

#ifndef __TREST_DAQ__
#define __TREST_DAQ__

#include <TRestRawDAQMetadata.h>
#include <TRestRawSignalEvent.h>
#include <TRestRun.h>

#include <iostream>
#include <string>

#include "TRESTDAQException.h"

class TRESTDAQ {
   public:
    TRESTDAQ(TRestRun* run, TRestRawDAQMetadata* sharedMemory);
    ~TRESTDAQ();

    // Pure virtual methods to start, stop and configure the DAQ
    virtual void configure() = 0;
    virtual void startDAQ() = 0;
    virtual void stopDAQ() = 0;
    virtual void initialize() = 0;
    virtual void startUp(){};

    virtual bool Ping() const {
        // To be overridden
        return false;
    };

    static std::atomic<bool> abrt;
    static std::atomic<int> event_cnt;
    static inline double lastEvTime = 0;

    static inline TRestStringOutput::REST_Verbose_Level verboseLevel = TRestStringOutput::REST_Verbose_Level::REST_Info;

    static Double_t getCurrentTime();

    TRestRun* GetRestRun() const { return restRun; }
    TRestRawSignalEvent* GetSignalEvent() const { return fSignalEvent; }
    TRestRawDAQMetadata* GetDAQMetadata() const { return daqMetadata; }
    static void FillTree(TRestRun* run, TRestRawSignalEvent* event);

   private:
    TRestRun* restRun;
    TRestRawDAQMetadata* daqMetadata;
    TRestRawSignalEvent* fSignalEvent;
};

#endif
