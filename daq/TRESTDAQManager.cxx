/*********************************************************************************
Manager for TRESTDAQ

Author: JuanAn Garcia 14/07/2021

Control data acquisition via shared memory, should be always running

*********************************************************************************/

#include "TRESTDAQManager.h"

#include <chrono>
#include <thread>

#include "TRESTDAQDCC.h"
#include "TRESTDAQDummy.h"
#include "TRESTDAQFEMINOS.h"

TRESTDAQManager::TRESTDAQManager() {
    int shmid;
    sharedMemoryStruct* sharedMemory;
    if (!GetSharedMemory(shmid, &sharedMemory, IPC_CREAT | 0666)) exit(1);

    InitializeSharedMemory(sharedMemory);
    PrintSharedMemory(sharedMemory);
    DetachSharedMemory(&sharedMemory);
}

TRESTDAQManager::~TRESTDAQManager() {
    int shmid;
    sharedMemoryStruct* sharedMemory;
    if (GetSharedMemory(shmid, &sharedMemory)) {
        std::cout << "Destroying shared memory" << std::endl;
        DetachSharedMemory(&sharedMemory);
        shmctl(shmid, IPC_RMID, NULL);
    }
}

void TRESTDAQManager::startUp() {

  std::cout<<__PRETTY_FUNCTION__<<std::endl;

  int shmid;
  sharedMemoryStruct* sM;
    if (!GetSharedMemory(shmid, &sM)) return;

    if (!TRestTools::fileExists(sM->cfgFile)) {
        std::cout << "File " << sM->cfgFile << " not found, please provide existing config file" << std::endl;
        DetachSharedMemory(&sM);
        return;
    }

  TRestRawDAQMetadata daqMetadata(sM->cfgFile);

  sM->status = 1;
  DetachSharedMemory(&sM);

    try{
      auto daq = GetTRESTDAQ(nullptr,&daqMetadata);
      if(daq){ 
        daq->startUp();
        daq->stopDAQ();
      }
    } catch(const TRESTDAQException& e) {
      std::cerr<<"TRESTDAQException was thrown: "<<e.what()<<std::endl;
    } catch (const std::exception& e) {
        std::cerr<<"std::exception was thrown: "<<e.what()<<std::endl;
    }

}

std::unique_ptr<TRESTDAQ> TRESTDAQManager::GetTRESTDAQ (TRestRun* rR, TRestRawDAQMetadata* dM){

  std::unique_ptr<TRESTDAQ> daq(nullptr);

  std::string electronicsType (dM->GetElectronicsType());
  auto eT = daq_metadata_types::electronicsTypes_map.find(electronicsType);
    if (eT == daq_metadata_types::electronicsTypes_map.end()) {
        std::cout << "Electronics type " << electronicsType << " not found, skipping " << std::endl;
        std::cout << "Valid electronics types:" << std::endl;
        for (const auto& [name, t] : daq_metadata_types::electronicsTypes_map) std::cout << (int)t << " " << name << std::endl;
      return daq;
    }

    if (eT->second == daq_metadata_types::electronicsTypes::DUMMY) {
        daq = std::make_unique<TRESTDAQDummy>(rR, dM);
    } else if (eT->second == daq_metadata_types::electronicsTypes::DCC) {
        daq = std::make_unique<TRESTDAQDCC>(rR, dM);
    } else if (eT->second == daq_metadata_types::electronicsTypes::FEMINOS) {
        daq = std::make_unique<TRESTDAQFEMINOS>(rR, dM);
    } else {
        std::cout << electronicsType << " not implemented, skipping..." << std::endl;
    }

return daq;

}

void TRESTDAQManager::dataTaking() {
    int shmid;
    sharedMemoryStruct* sM;
    if (!GetSharedMemory(shmid, &sM)) return;

    if (!TRestTools::fileExists(sM->cfgFile)) {
        std::cout << "File " << sM->cfgFile << " not found, please provide existing config file" << std::endl;
        DetachSharedMemory(&sM);
        return;
    }

    TRestRawDAQMetadata daqMetadata(sM->cfgFile);

    auto rT = daq_metadata_types::acqTypes_map.find(sM->runType);
    if (rT != daq_metadata_types::acqTypes_map.end()) {
        daqMetadata.SetAcquisitionType(sM->runType);
    } else if ((rT = daq_metadata_types::acqTypes_map.find(daqMetadata.GetAcquisitionType().Data())) != daq_metadata_types::acqTypes_map.end()) {
        std::cout << "Warning: Acquisition type not found in shared memory, assuming the " << daqMetadata.GetAcquisitionType().Data()
                  << " from config file" << std::endl;
        sprintf(sM->runType, "%s", daqMetadata.GetAcquisitionType().Data());
    } else {
        std::cout << "Acquisition type " << sM->runType << " not found, skipping " << std::endl;
        std::cout << "Valid acquisition types:" << std::endl;
        for (const auto& [name, t] : daq_metadata_types::acqTypes_map) std::cout << (int)t << " " << name << std::endl;
        DetachSharedMemory(&sM);
        return;
    }

    std::string runTag = sM->runTag;

    if (sM->nEvents > -1) {
        daqMetadata.SetNEvents(sM->nEvents);
        std::cout << "Setting " << sM->nEvents << " events to be acquired" << std::endl;
    } else {
        std::cout << "Setting " << daqMetadata.GetNEvents() << " events to be acquired from config file" << std::endl;
        sM->nEvents = daqMetadata.GetNEvents();
    }

    daqMetadata.PrintMetadata();
    maxFileSize = daqMetadata.GetMaxFileSize();
    const std::string cfgFile = std::string(sM->cfgFile);
    sM->status = 1;
    DetachSharedMemory(&sM);
    TRESTDAQ::abrt = false;
    TRESTDAQ::event_cnt = 0;
    std::thread abrtT(AbortThread);
    int parentRunNumber = 0;
    TRestDataBase *db = gDataBase;
    int runNumber = db->get_lastrun() + 1;
    DBEntry entry;
    entry.runNr = runNumber;
    entry.description = "TRESTDAQ";
    entry.tag = runTag;
    entry.type = daqMetadata.GetAcquisitionType();
    entry.version = REST_RELEASE;
    db->set_run(entry);

    do {
      TRestRun restRun;
      restRun.LoadConfigFromFile(cfgFile);
      restRun.SetRunNumber(runNumber);

        if(!runTag.empty() && runTag != "none"){
          restRun.SetRunTag(runTag);
        }

      restRun.SetRunType(daqMetadata.GetAcquisitionType());
      restRun.AddMetadata(&daqMetadata);
      restRun.SetParentRunNumber(parentRunNumber);
      restRun.FormOutputFile();

      if (!GetSharedMemory(shmid, &sM)) break;
      sprintf(sM->runName, "%s", restRun.GetOutputFileName().Data());
      DetachSharedMemory(&sM);
      std::cout << "Run " << " " << restRun.GetOutputFileName() << std::endl;

      TRESTDAQ::nextFile = false;
      std::thread fileSizeT(FileSizeThread);

      restRun.SetStartTimeStamp(TRESTDAQ::getCurrentTime());
      restRun.PrintMetadata();

      try{
        auto daq = GetTRESTDAQ(&restRun, &daqMetadata);
          if(daq){
            if(parentRunNumber == 0){
              daq->configure();
              std::cout << "Electronics configured, starting data taking run type " << std::endl;
            }
            daq->startDAQ();  // Should wait till completion or stopped
            daq->stopDAQ();
          }
      } catch(const TRESTDAQException& e) {
        std::cerr<<"TRESTDAQException was thrown: "<<e.what()<<std::endl;
      } catch (const std::exception& e) {
        std::cerr<<"std::exception was thrown: "<<e.what()<<std::endl;
      }

      restRun.SetEndTimeStamp(TRESTDAQ::getCurrentTime());
      restRun.UpdateOutputFile();
      restRun.CloseFile();
      restRun.PrintMetadata();

        if( (daqMetadata.GetNEvents() != 0 && TRESTDAQ::event_cnt >= daqMetadata.GetNEvents()) || daqMetadata.GetAcquisitionType() =="pedestal" ){
          StopRun();
        }

      fileSizeT.join();
      parentRunNumber++;
    } while (!TRESTDAQ::abrt && TRESTDAQ::nextFile );

    abrtT.join();
    std::cout << "Data taking stopped " << std::endl;

}

void TRESTDAQManager::StopRun() {
    int shmid;
    sharedMemoryStruct* sharedMemory;
    if (!GetSharedMemory(shmid, &sharedMemory)) return;
    sharedMemory->abortRun = 1;
    shmdt((sharedMemoryStruct*)sharedMemory);
}

void TRESTDAQManager::ExitManager() {
    int shmid;
    sharedMemoryStruct* sharedMemory;
    if (!GetSharedMemory(shmid, &sharedMemory)) return;
    sharedMemory->abortRun = 1;
    sharedMemory->exitManager = 1;
    PrintSharedMemory(sharedMemory);
    shmdt((sharedMemoryStruct*)sharedMemory);
}

void TRESTDAQManager::run() {
    int shmid;
    sharedMemoryStruct* sharedMemory;
    bool exitMan = false;
    do {
        if (!GetSharedMemory(shmid, &sharedMemory)) break;
        exitMan = sharedMemory->exitManager;

        if(sharedMemory->startUp == 1){
          DetachSharedMemory(&sharedMemory);
          startUp();
          if (!GetSharedMemory(shmid, &sharedMemory)) break;
          sharedMemory->startUp = 0;
          sharedMemory->status = 0;
        }

        if (sharedMemory->startDAQ == 1) {
            std::cout << "DAQ started" << std::endl;
            // Make sure that abort flag is set to false
            sharedMemory->abortRun = 0;
            DetachSharedMemory(&sharedMemory);

            dataTaking();

            if (!GetSharedMemory(shmid, &sharedMemory)) break;
            sharedMemory->status = 0;
            sharedMemory->startDAQ = 0;
            std::cout << "DAQ stopped" << std::endl;
            TRESTDAQ::event_cnt = 0;
        }

        DetachSharedMemory(&sharedMemory);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while (!exitMan);
}

bool TRESTDAQManager::GetSharedMemory(int& sid, sharedMemoryStruct** sM, int flag, bool verbose) {
    if ((sid = shmget(TRESTDAQManager::key, sizeof(sharedMemoryStruct), flag)) == -1) {
        if(verbose)std::cerr << "Error while creating shared memory (shmget) " << std::strerror(errno) << std::endl;
        return false;
    }

    *sM = (sharedMemoryStruct*)shmat(sid, NULL, 0);
    if (*sM == (sharedMemoryStruct*)-1) {
        std::cerr << "Error while creating shared memory (shmat) " << std::strerror(errno) << std::endl;
        return false;
    }
    return true;
}

void TRESTDAQManager::DetachSharedMemory(sharedMemoryStruct** sM) { shmdt((sharedMemoryStruct*)*sM); }

void TRESTDAQManager::PrintSharedMemory(sharedMemoryStruct* sM) {
    std::cout << "Cfg File: " << sM->cfgFile << std::endl;
    std::cout << "Status: " << sM->status << std::endl;
    std::cout << "StartDAQ: " << sM->startDAQ << std::endl;
    std::cout << "RunType: " << sM->runType << std::endl;
    std::cout << "RunTag: " << sM->runTag << std::endl;
    std::cout << "AbortRun: " << sM->abortRun << std::endl;
    std::cout << "Event count: " << sM->eventCount << std::endl;
    std::cout << "Number of events to acquire: " << sM->nEvents << std::endl;
    std::cout << "RunName: " << sM->runName << std::endl;
    std::cout << "Exit Manager: " << sM->exitManager << std::endl;
}

void TRESTDAQManager::InitializeSharedMemory(sharedMemoryStruct* sM) {
    sprintf(sM->cfgFile, "none");
    sprintf(sM->runTag, "none");
    sprintf(sM->runName, "none");
    sM->startDAQ = 0;
    sM->startUp = 0;
    sM->status = 0;
    sM->eventCount = 0;
    sM->nEvents = -1;
    sM->exitManager = 0;
    sM->abortRun = 0;
}

int TRESTDAQManager::GetFileSize(const std::string &filename){
  struct stat stat_buf;
  int rc = stat(filename.c_str(), &stat_buf);
  return rc == 0 ? stat_buf.st_size : -1;
}

void TRESTDAQManager::AbortThread() {
    int shmid;
    sharedMemoryStruct* sharedMemory;

    bool abortRun = false;
      do {
        if (!GetSharedMemory(shmid, &sharedMemory)) break;
        abortRun = sharedMemory->abortRun;
        sharedMemory->eventCount = TRESTDAQ::event_cnt;
        DetachSharedMemory(&sharedMemory);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      } while (!abortRun);

    TRESTDAQ::abrt = true;
}


void TRESTDAQManager::FileSizeThread() {
    int shmid;
    sharedMemoryStruct* sharedMemory;

      do {
        if (!GetSharedMemory(shmid, &sharedMemory)) break;
        const std::string runN = std::string(sharedMemory->runName);
        const int fileSize = GetFileSize(runN);
        DetachSharedMemory(&sharedMemory);
          if(fileSize >= maxFileSize){
            std::cout << runN << " "<< fileSize << " "<<maxFileSize<< std::endl;
            TRESTDAQ::nextFile = true;
          }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      } while (!TRESTDAQ::abrt && !TRESTDAQ::nextFile);

}

