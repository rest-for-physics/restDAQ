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
        char cfgFile[1024];
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
    std::unique_ptr<TRESTDAQ> GetTRESTDAQ(TRestRun* rR, TRestRawDAQMetadata* dM);

    // Shared Memory
    static void InitializeSharedMemory(sharedMemoryStruct* sM);
    static void PrintSharedMemory(sharedMemoryStruct* sM);
    static bool GetSharedMemory(int& sid, sharedMemoryStruct** sM, int flag = 0, bool verbose = true);
    static void DetachSharedMemory(sharedMemoryStruct** sM);

    static int GetFileSize(const std::string& filename);

    // Control commands
    static void StopRun();
    static void ExitManager();
    static void AbortThread();

   private:
    inline static const key_t key{0x12367};
};

#endif
