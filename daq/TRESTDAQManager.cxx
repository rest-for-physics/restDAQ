/*********************************************************************************
Manager for TRESTDAQ

Author: JuanAn Garcia 14/07/2021

Control data acquisition via shared memory, should be always running

*********************************************************************************/

#include "TRESTDAQManager.h"

#include <sys/stat.h>

#include <chrono>
#include <thread>

#include "TRESTDAQDCC.h"
#include "TRESTDAQDummy.h"
#include "TRESTDAQFEMINOS.h"

using namespace std;

TRESTDAQManager::TRESTDAQManager() {
    int sharedMemoryID;
    sharedMemoryStruct* sharedMemory;
    if (!GetSharedMemory(sharedMemoryID, &sharedMemory, IPC_CREAT | 0666)) exit(1);

    InitializeSharedMemory(sharedMemory);
    PrintSharedMemory(sharedMemory);
    DetachSharedMemory(&sharedMemory);
}

TRESTDAQManager::~TRESTDAQManager() {
    int sharedMemoryID;
    sharedMemoryStruct* sharedMemory;
    if (GetSharedMemory(sharedMemoryID, &sharedMemory)) {
        std::cout << "Destroying shared memory" << std::endl;
        DetachSharedMemory(&sharedMemory);
        shmctl(sharedMemoryID, IPC_RMID, nullptr);
    }
}

void TRESTDAQManager::startUp() {
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    int shareMemoryID;
    sharedMemoryStruct* sharedMemory;
    if (!GetSharedMemory(shareMemoryID, &sharedMemory)) return;

    if (!TRestTools::fileExists(sharedMemory->configFilename)) {
        std::cout << "File '" << sharedMemory->configFilename << "' not found, please provide existing config file" << std::endl;
        DetachSharedMemory(&sharedMemory);
        return;
    }

    TRestRawDAQMetadata daqMetadata(sharedMemory->configFilename);

    sharedMemory->status = 1;
    DetachSharedMemory(&sharedMemory);

    try {
        auto daq = GetTRESTDAQ(nullptr, &daqMetadata);
        if (daq != nullptr) {
            daq->startUp();
            daq->stopDAQ();
        }
    } catch (const TRESTDAQException& e) {
        std::cerr << "TRESTDAQException was thrown: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "std::exception was thrown: " << e.what() << std::endl;
    }
}

std::unique_ptr<TRESTDAQ> TRESTDAQManager::GetTRESTDAQ(TRestRun* run, TRestRawDAQMetadata* metadata) {
    std::unique_ptr<TRESTDAQ> daq(nullptr);

    std::string electronicsType(metadata->GetElectronicsType());
    auto eT = daq_metadata_types::electronicsTypes_map.find(electronicsType);
    if (eT == daq_metadata_types::electronicsTypes_map.end()) {
        std::cout << "Electronics type " << electronicsType << " not found, skipping " << std::endl;
        std::cout << "Valid electronics types:" << std::endl;
        for (const auto& [name, t] : daq_metadata_types::electronicsTypes_map) std::cout << (int)t << " " << name << std::endl;
        return daq;
    }

    if (eT->second == daq_metadata_types::electronicsTypes::DUMMY) {
        daq = std::make_unique<TRESTDAQDummy>(run, metadata);
    } else if (eT->second == daq_metadata_types::electronicsTypes::DCC) {
        daq = std::make_unique<TRESTDAQDCC>(run, metadata);
    } else if (eT->second == daq_metadata_types::electronicsTypes::FEMINOS) {
        daq = std::make_unique<TRESTDAQFEMINOS>(run, metadata);
    } else {
        std::cout << electronicsType << " not implemented, skipping..." << std::endl;
        cerr << "Error: Invalid electronics type " << electronicsType << endl;
        return 1;
    }

    return daq;
}

void TRESTDAQManager::dataTaking() {
    int sharedMemoryID;
    sharedMemoryStruct* sharedMemory;
    if (!GetSharedMemory(sharedMemoryID, &sharedMemory)) return;

    if (!TRestTools::fileExists(sharedMemory->configFilename)) {
        std::cout << "File '" << sharedMemory->configFilename << "' not found, please provide existing config file" << std::endl;
        DetachSharedMemory(&sharedMemory);
        return;
    }

    TRestRawDAQMetadata daqMetadata(sharedMemory->configFilename);

    auto rT = daq_metadata_types::acqTypes_map.find(sharedMemory->runType);
    if (rT != daq_metadata_types::acqTypes_map.end()) {
        daqMetadata.SetAcquisitionType(sharedMemory->runType);
    } else if ((rT = daq_metadata_types::acqTypes_map.find(daqMetadata.GetAcquisitionType().Data())) != daq_metadata_types::acqTypes_map.end()) {
        std::cout << "Warning: Acquisition type not found in shared memory, assuming the " << daqMetadata.GetAcquisitionType().Data()
                  << " from config file" << std::endl;
        sprintf(sharedMemory->runType, "%s", daqMetadata.GetAcquisitionType().Data());
    } else {
        std::cout << "Acquisition type " << sharedMemory->runType << " not found, skipping " << std::endl;
        std::cout << "Valid acquisition types:" << std::endl;
        for (const auto& [name, t] : daq_metadata_types::acqTypes_map) std::cout << (int)t << " " << name << std::endl;
        DetachSharedMemory(&sharedMemory);
        return;
    }

    if (sharedMemory->nEvents > -1) {
        daqMetadata.SetNEvents(sharedMemory->nEvents);
        std::cout << "Setting " << sharedMemory->nEvents << " events to be acquired" << std::endl;
    } else {
        std::cout << "Setting " << daqMetadata.GetNEvents() << " events to be acquired from config file" << std::endl;
        sharedMemory->nEvents = daqMetadata.GetNEvents();
    }

    TRestRun restRun;
    restRun.LoadConfigFromFile(sharedMemory->configFilename);
    daqMetadata.PrintMetadata();
    TString rTag = restRun.GetRunTag();

    if (rTag == "Null" || rTag == "") restRun.SetRunTag(daqMetadata.GetTitle());

    restRun.SetRunType(daqMetadata.GetAcquisitionType());
    restRun.AddMetadata(&daqMetadata);
    restRun.FormOutputFile();
    sprintf(sharedMemory->runName, "%s", restRun.GetOutputFileName().Data());
    std::cout << "Run " << sharedMemory->runName << " " << restRun.GetOutputFileName() << std::endl;
    restRun.SetStartTimeStamp(TRESTDAQ::getCurrentTime());
    restRun.PrintMetadata();

    sharedMemory->status = 1;
    DetachSharedMemory(&sharedMemory);

    std::thread abrtT(AbortThread);

    try {
        auto daq = GetTRESTDAQ(&restRun, &daqMetadata);
        if (daq) {
            TRESTDAQ::abrt = false;
            daq->configure();
            std::cout << "Electronics configured, starting data taking run type " << std::endl;
            daq->startDAQ();  // Should wait till completion or stopped
            daq->stopDAQ();
        }
    } catch (const TRESTDAQException& e) {
        std::cerr << "TRESTDAQException was thrown: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "std::exception was thrown: " << e.what() << std::endl;
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
    int sharedMemoryID;
    sharedMemoryStruct* sharedMemory;
    if (!GetSharedMemory(sharedMemoryID, &sharedMemory)) return;
    sharedMemory->abortRun = 1;
    shmdt((sharedMemoryStruct*)sharedMemory);
}

void TRESTDAQManager::ExitManager() {
    int sharedMemoryID;
    sharedMemoryStruct* sharedMemory;
    if (!GetSharedMemory(sharedMemoryID, &sharedMemory)) return;
    sharedMemory->abortRun = 1;
    sharedMemory->exitManager = 1;
    PrintSharedMemory(sharedMemory);
    shmdt((sharedMemoryStruct*)sharedMemory);
}

void TRESTDAQManager::run() {
    int sharedMemoryID;
    sharedMemoryStruct* sharedMemory;
    bool exitMan = false;
    do {
        if (!GetSharedMemory(sharedMemoryID, &sharedMemory)) {
            break;
        }
        exitMan = sharedMemory->exitManager;

        if (sharedMemory->startUp == 1) {
            DetachSharedMemory(&sharedMemory);
            startUp();
            if (!GetSharedMemory(sharedMemoryID, &sharedMemory)) {
                break;
            }
            sharedMemory->startUp = 0;
            sharedMemory->status = 0;
        }

        if (sharedMemory->startDAQ == 1) {
            std::cout << "DAQ started" << std::endl;
            // Make sure that abort flag is set to false
            sharedMemory->abortRun = 0;
            DetachSharedMemory(&sharedMemory);

            dataTaking();

            if (!GetSharedMemory(sharedMemoryID, &sharedMemory)) {
                break;
            }
            sharedMemory->status = 0;
            sharedMemory->startDAQ = 0;
        }

        DetachSharedMemory(&sharedMemory);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while (!exitMan);
}

bool TRESTDAQManager::GetSharedMemory(int& sharedMemoryID, sharedMemoryStruct** sharedMemory, int flag, bool verbose) {
    if ((sharedMemoryID = shmget(TRESTDAQManager::key, sizeof(sharedMemoryStruct), flag)) == -1) {
        if (verbose) std::cerr << "Error while creating shared memory (shmget) " << std::strerror(errno) << std::endl;
        return false;
    }

    *sharedMemory = (sharedMemoryStruct*)shmat(sharedMemoryID, nullptr, 0);
    if (*sharedMemory == (sharedMemoryStruct*)-1) {
        std::cerr << "Error while creating shared memory (shmat) " << std::strerror(errno) << std::endl;
        return false;
    }
    return true;
}

void TRESTDAQManager::DetachSharedMemory(sharedMemoryStruct** sharedMemory) { shmdt((sharedMemoryStruct*)*sharedMemory); }

void TRESTDAQManager::PrintSharedMemory(sharedMemoryStruct* sharedMemory) {
    std::cout << "Config file: " << sharedMemory->configFilename << std::endl;
    std::cout << "Status: " << sharedMemory->status << std::endl;
    std::cout << "StartDAQ: " << sharedMemory->startDAQ << std::endl;
    std::cout << "RunType: " << sharedMemory->runType << std::endl;
    std::cout << "AbortRun: " << sharedMemory->abortRun << std::endl;
    std::cout << "Event count: " << sharedMemory->eventCount << std::endl;
    std::cout << "Number of events to acquire: " << sharedMemory->nEvents << std::endl;
    std::cout << "RunName: " << sharedMemory->runName << std::endl;
    std::cout << "Exit Manager: " << sharedMemory->exitManager << std::endl;
}

void TRESTDAQManager::InitializeSharedMemory(sharedMemoryStruct* sharedMemory) {
    sprintf(sharedMemory->configFilename, "none");
    sprintf(sharedMemory->runType, "none");
    sprintf(sharedMemory->runName, "none");
    sharedMemory->startDAQ = 0;
    sharedMemory->startUp = 0;
    sharedMemory->status = 0;
    sharedMemory->eventCount = 0;
    sharedMemory->nEvents = -1;
    sharedMemory->exitManager = 0;
    sharedMemory->abortRun = 0;
}

int TRESTDAQManager::GetFileSize(const std::string& filename) {
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

void TRESTDAQManager::AbortThread() {
    int sharedMemoryID;
    sharedMemoryStruct* sharedMemory;

    bool abrt = false;
    do {
        if (!GetSharedMemory(sharedMemoryID, &sharedMemory)) break;
        abrt = sharedMemory->abortRun;
        sharedMemory->eventCount = TRESTDAQ::event_cnt;
        DetachSharedMemory(&sharedMemory);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    } while (!abrt);

    TRESTDAQ::abrt = true;
}
