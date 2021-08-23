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
    if (GetSharedMemory(shmid, &sharedMemory, 0)) {
        std::cout << "Destroying shared memory" << std::endl;
        DetachSharedMemory(&sharedMemory);
        shmctl(shmid, IPC_RMID, NULL);
    }
}

void TRESTDAQManager::dataTaking() {
    int shmid;
    sharedMemoryStruct* sM;
    if (!GetSharedMemory(shmid, &sM, 0)) return;

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

    if (sM->nEvents > -1) {
        daqMetadata.SetNEvents(sM->nEvents);
        std::cout << "Setting " << sM->nEvents << " events to be acquired" << std::endl;
    } else {
        std::cout << "Setting " << daqMetadata.GetNEvents() << " events to be acquired from config file" << std::endl;
        sM->nEvents = daqMetadata.GetNEvents();
    }

    TRestRun restRun;
    restRun.LoadConfigFromFile(sM->cfgFile);
    daqMetadata.PrintMetadata();
    TString rTag = restRun.GetRunTag();

    if (rTag == "Null" || rTag == "") restRun.SetRunTag(daqMetadata.GetTitle());

    restRun.SetRunType(daqMetadata.GetAcquisitionType());
    restRun.AddMetadata(&daqMetadata);
    restRun.FormOutputFile();
    sprintf(sM->runName, "%s", restRun.GetOutputFileName().Data());
    std::cout << "Run " << sM->runName << " " << restRun.GetOutputFileName() << std::endl;
    restRun.SetStartTimeStamp(TRESTDAQ::getCurrentTime());
    restRun.PrintMetadata();

    sM->status = 1;
    DetachSharedMemory(&sM);

    std::thread abrtT(AbortThread);

    std::string elType = daqMetadata.GetElectronicsType().Data();
    auto eT = daq_metadata_types::electronicsTypes_map.find(elType);
    if (eT == daq_metadata_types::electronicsTypes_map.end()) {
        std::cout << "Electronics type " << elType << " not found, skipping " << std::endl;
        std::cout << "Valid electronics types:" << std::endl;
        for (const auto& [name, t] : daq_metadata_types::electronicsTypes_map) std::cout << (int)t << " " << name << std::endl;
    } else {
      try{
        std::unique_ptr<TRESTDAQ> daq;
        bool impl = true;
        if (eT->second == daq_metadata_types::electronicsTypes::DUMMY) {
            daq = std::make_unique<TRESTDAQDummy>(&restRun, &daqMetadata);
        } else if (eT->second == daq_metadata_types::electronicsTypes::DCC) {
            daq = std::make_unique<TRESTDAQDCC>(&restRun, &daqMetadata);
        } else if (eT->second == daq_metadata_types::electronicsTypes::FEMINOS) {
            daq = std::make_unique<TRESTDAQFEMINOS>(&restRun, &daqMetadata);
        } else {
            std::cout << elType << " not implemented, skipping..." << std::endl;
            impl = false;
        }

        if (impl) {
            TRESTDAQ::abrt = false;
            daq->configure();
            std::cout << "Electronics configured, starting data taking run type " << std::endl;

              daq->startDAQ();  // Should wait till completion or stopped
              daq->stopDAQ();
        }
      } catch(const TRESTDAQException& e) {
        std::cerr<<"TRESTDAQException was thrown: "<<e.what()<<std::endl;
      } catch (const std::exception& e) {
        std::cerr<<"std::exception was thrown: "<<e.what()<<std::endl;
      }
    }

    StopRun();

    restRun.SetEndTimeStamp(TRESTDAQ::getCurrentTime());
    restRun.UpdateOutputFile();
    restRun.CloseFile();
    restRun.PrintMetadata();
    std::cout << "Data taking stopped" << std::endl;

    abrtT.join();
}

void TRESTDAQManager::StopRun() {
    int shmid;
    sharedMemoryStruct* sharedMemory;
    if (!GetSharedMemory(shmid, &sharedMemory, 0)) return;
    sharedMemory->abortRun = 1;
    shmdt((sharedMemoryStruct*)sharedMemory);
}

void TRESTDAQManager::ExitManager() {
    int shmid;
    sharedMemoryStruct* sharedMemory;
    if (!GetSharedMemory(shmid, &sharedMemory, 0)) return;
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
        if (!GetSharedMemory(shmid, &sharedMemory, 0)) break;
        exitMan = sharedMemory->exitManager;
        if (sharedMemory->startDAQ == 1) {
            std::cout << "DAQ started" << std::endl;
            // Make sure that abort flag is set to false
            sharedMemory->abortRun = 0;
            DetachSharedMemory(&sharedMemory);

            dataTaking();

            if (!GetSharedMemory(shmid, &sharedMemory, 0)) break;
            sharedMemory->status = 0;
            sharedMemory->startDAQ = 0;
        }

        DetachSharedMemory(&sharedMemory);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while (!exitMan);
}

bool TRESTDAQManager::GetSharedMemory(int& sid, sharedMemoryStruct** sM, int flag) {
    if ((sid = shmget(TRESTDAQManager::key, sizeof(sharedMemoryStruct), flag)) == -1) {
        std::cerr << "Error while creating shared memory (shmget) " << std::strerror(errno) << std::endl;
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
    std::cout << "AbortRun: " << sM->abortRun << std::endl;
    std::cout << "Event count: " << sM->eventCount << std::endl;
    std::cout << "Number of events to acquire: " << sM->nEvents << std::endl;
    std::cout << "RunName: " << sM->runName << std::endl;
    std::cout << "Exit Manager: " << sM->exitManager << std::endl;
}

void TRESTDAQManager::InitializeSharedMemory(sharedMemoryStruct* sM) {
    sprintf(sM->cfgFile, "none");
    sprintf(sM->runType, "none");
    sprintf(sM->runName, "none");
    sM->startDAQ = 0;
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

    bool abrt = 0;
    do {
        if (!GetSharedMemory(shmid, &sharedMemory, 0)) break;
        abrt = sharedMemory->abortRun;
        sharedMemory->eventCount = TRESTDAQ::event_cnt;
        DetachSharedMemory(&sharedMemory);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    } while (!abrt);

    TRESTDAQ::abrt = true;
}
