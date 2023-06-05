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
    fSignalEvent.Initialize();

     if(restRun)
      restRun->AddEventBranch(&fSignalEvent);

    auto tT = daq_metadata_types::triggerTypes_map.find(daqMetadata->GetTriggerType().Data());
    if(tT == daq_metadata_types::triggerTypes_map.end() ){
      std::cerr << "Unknown trigger type "<< daqMetadata->GetTriggerType() << std::endl;
      std::cerr << "Valid trigger types "<< std::endl;
        for(const auto &[type, trigger] : daq_metadata_types::triggerTypes_map){
          std::cerr << type <<" ["<< (int)trigger <<"], \t";
        }
      std::cerr << std::endl;
      throw (TRESTDAQException("Unknown trigger type, please check RML"));
    } else {
      triggerType = tT->second;
    }

  auto cM = daq_metadata_types::compressMode_map.find(daqMetadata->GetCompressMode().Data());

    if(cM == daq_metadata_types::compressMode_map.end() ){
      std::cerr << "Unknown compress mode "<< daqMetadata->GetCompressMode() << std::endl;
      std::cerr << "Valid compress modes "<< std::endl;
        for(const auto &[type, mode] : daq_metadata_types::compressMode_map){
          std::cerr << type <<" ["<< (int)mode <<"], \t";
        }
      std::cerr << std::endl;
      throw (TRESTDAQException("Unknown compress mode, please check RML"));
    } else {
      compressMode = cM->second;
    }

    auto rT = daq_metadata_types::acqTypes_map.find(std::string(daqMetadata->GetAcquisitionType()));
    if (rT == daq_metadata_types::acqTypes_map.end()) {
        std::cerr << "Unknown acquisition type " << daqMetadata->GetAcquisitionType() << " skipping" << std::endl;
        std::cerr << "Valid acq types:" << std::endl;
        for (auto& [name, t] : daq_metadata_types::acqTypes_map) std::cout << (int)t << " " << name << std::endl;
        throw (TRESTDAQException("Unsupported acquisition type, please check RML"));
    } else {
      acqType = rT->second;
    }

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
    if (event_cnt % 1000 == 0 || (evTime - lastEvTime) > 10 ) {
        rR->GetEventTree()->AutoSave("SaveSelf");
        lastEvTime = evTime;
    }
  }
  event_cnt++;
}
