/*********************************************************************************
TRESTDAQ.h

Base class for different DAQs: T2K, FEMINOS or any other

Author: JuanAn Garcia 18/05/2021

To implement generic methods here

*********************************************************************************/

#ifndef __TREST_DAQ__
#define __TREST_DAQ__

#include <iostream>
#include <string>

#include "TRestRawDAQMetadata.h"
#include "TRestRawSignalEvent.h"
#include "TRestRun.h"

class TRESTDAQException : public std::exception {
  public:
    TRESTDAQException(const string& msg) : m_msg(msg){
    }

   ~TRESTDAQException(){ }

   virtual const char* what() const throw () {
        return m_msg.c_str();
   }
  private:
   const string m_msg;
};

class TRESTDAQ {
   public:
    TRESTDAQ(TRestRun* rR, TRestRawDAQMetadata* dM);
    ~TRESTDAQ();

    // Pure virtual methods to start, stop and configure the DAQ
    virtual void configure() = 0;
    virtual void startDAQ() = 0;
    virtual void stopDAQ() = 0;
    virtual void initialize() = 0;

    inline static bool abrt = false;
    inline static int event_cnt = 0;

    static Double_t getCurrentTime();

    TRestRun* GetRestRun() { return restRun; }
    TRestRawSignalEvent* GetSignalEvent() { return fSignalEvent; }
    TRestRawDAQMetadata* GetDAQMetadata() { return daqMetadata; }
    void FillTree();

   private:
    TRestRun* restRun;
    TRestRawDAQMetadata* daqMetadata;
    TRestRawSignalEvent* fSignalEvent;
};

#endif
