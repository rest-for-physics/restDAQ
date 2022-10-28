/*********************************************************************************
TRESTDAQManager.h

Manager for TRESTDAQ

Author: JuanAn Garcia 14/07/2021

Control data acquisition via shared memory, should be always running

*********************************************************************************/

#ifndef __TREST_DAQ_MANAGER__
#define __TREST_DAQ_MANAGER__

#include <sys/ipc.h>
#include <sys/shm.h>

#include <iostream>
#include <string>

#include "TRESTDAQ.h"

class TRESTDAQManager {
   public:
    TRESTDAQManager();
    ~TRESTDAQManager();

    void run();

    // Shared memory
    struct sharedMemoryStruct {
        char configFilename[1024];
        char runType[256];
        char runName[1024];
        int startDAQ;
        int startUp;
        int status;
        int eventCount;
        int nEvents;
        int exitManager;
        int abortRun;
    };

    void dataTaking();
    void startUp();

    static std::unique_ptr<TRESTDAQ> GetTRESTDAQ(TRestRun* run, TRestRawDAQMetadata* metadata);

    bool Initialize() const;

    // Shared Memory
    static void InitializeSharedMemory(sharedMemoryStruct* sharedMemory);
    static void PrintSharedMemory(sharedMemoryStruct* sharedMemory);
    static bool GetSharedMemory(int& sharedMemoryID, sharedMemoryStruct** sharedMemory, int flag = 0, bool verbose = true);
    static void DetachSharedMemory(sharedMemoryStruct** sharedMemory);

    static int GetFileSize(const std::string& filename);

    // Control commands
    static void StopRun();
    static void ExitManager();
    static void AbortThread();

   private:
    inline static const key_t key{0x12367};
};

#endif
