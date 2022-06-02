/*********************************************************************************
TRESTDAQ.cxx

Base class for different DAQs: T2K, FEMINOS or any other

Author: JuanAn Garcia 18/05/2021

*********************************************************************************/

#include "TRESTDAQ.h"

#include <chrono>

std::atomic<bool> TRESTDAQ::abrt(false);
std::atomic<int> TRESTDAQ::event_cnt(0);

TRESTDAQ::TRESTDAQ(TRestRun* rR, TRestRawDAQMetadata* dM) {
    restRun = rR;
    daqMetadata = dM;
    verboseLevel = daqMetadata->GetVerboseLevel();
    fSignalEvent = new TRestRawSignalEvent();
     if(restRun)
      restRun->AddEventBranch(fSignalEvent);
}

TRESTDAQ::~TRESTDAQ() {
    // Cleanup if any
    event_cnt=0;
}

Double_t TRESTDAQ::getCurrentTime() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count() / 1000000.0;
}

void TRESTDAQ::FillTree(TRestRun *rR, TRestRawSignalEvent* sEvent) {

  const double evTime = sEvent->GetTime();

  if(rR){
    rR->GetAnalysisTree()->SetEventInfo(sEvent);
    rR->GetEventTree()->Fill();
    rR->GetAnalysisTree()->Fill();
    // AutoSave is needed to read and write at the same time
    if (event_cnt % 100 == 0 || (evTime - lastEvTime) > 10 ) {
        rR->GetEventTree()->AutoSave("SaveSelf");
    }
  }
  event_cnt++;
  
  lastEvTime = evTime;
}
