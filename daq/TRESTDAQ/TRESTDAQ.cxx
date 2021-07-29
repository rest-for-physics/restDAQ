/*********************************************************************************
TRESTDAQ.cxx

Base class for different DAQs: T2K, FEMINOS or any other

Author: JuanAn Garcia 18/05/2021

*********************************************************************************/

#include <chrono>

#include "TRESTDAQ.h"

TRESTDAQ::TRESTDAQ(TRestRun *rR, TRestRawDAQMetadata *dM) {
  restRun = rR;
  daqMetadata = dM;
  fSignalEvent = new TRestRawSignalEvent();
  restRun->AddEventBranch(fSignalEvent);
  abrt = false;
  event_cnt = 0;
}

TRESTDAQ::~TRESTDAQ() {
    // Cleanup if any
}

Double_t TRESTDAQ::getCurrentTime(){

return std::chrono::duration_cast <std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count()/1000000.0;

}

void TRESTDAQ::FillTree(){

  restRun->GetAnalysisTree()->SetEventInfo( GetSignalEvent());
  restRun->GetEventTree()->Fill();
  restRun->GetAnalysisTree()->Fill();
    //AutoSave is needed to read and write at the same time
    if(event_cnt%100 ==0){
      restRun->GetEventTree()->AutoSave("SaveSelf");
    }

}


