#include <dirent.h>
#include <signal.h>

#include <iostream>

#include "TRESTDAQManager.h"

const std::string thisProgram = "restDAQManager";

void signal_handler(int signum) {
    std::cout << " signal handler " << signum << std::endl;
    TRESTDAQManager::ExitManager();
}

void help() {
    std::cout << " Rest DAQ Manager options:" << std::endl;
    std::cout << "    --e       : Exit manager" << std::endl;
    std::cout << "    --c       : Set configFile (single run)" << std::endl;
    std::cout << "    --h       : Print this help" << std::endl;
    std::cout << "If no arguments are provided it starts at infinite loop which is controller via shared memory" << std::endl;
}

bool checkRunning() {
    DIR* dir;
    struct dirent* ent;
    char* endptr;

    int nInstances = 0;

    if (!(dir = opendir("/proc"))) {
        std::cerr << "Cannot open /proc" << std::endl;
        return true;
    }

    while ((ent = readdir(dir)) != NULL) {
        /* if endptr is not a null character, the directory is not
         * entirely numeric, so ignore it */
        long lpid = strtol(ent->d_name, &endptr, 10);
        if (*endptr != '\0') {
            continue;
        }
        /* try to open the cmdline file */
        std::string fName = "/proc/" + std::to_string(lpid) + "/status";
        std::ifstream fin(fName, std::ifstream::in);
        if (fin.fail()) {
            std::cerr << "Cannot open " + fName + " skipping..." << std::endl;
            continue;
        }

        std::string line;
        std::getline(fin, line);
        std::stringstream str;
        str << line;
        std::string head, binary;
        str >> head >> binary;
        if (thisProgram.substr(0, 15) == binary) {
            std::cout << binary << " pid: " << lpid << std::endl;
            nInstances++;
        }
        fin.close();
    }
    closedir(dir);

    if (nInstances > 1) return true;

    return false;
}

int main(int argc, char** argv) {
    std::string cfgFile = "";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--c") {
            i++;
            cfgFile = argv[i++];
        } else if (arg == "--e") {
            TRESTDAQManager::ExitManager();
            std::cout << "Exiting Rest DAQ Manager" << std::endl;
            return 0;
        } else if (arg == "--h") {
            help();
            return 0;
        } else {  // unmatched options
            std::cerr << "Warning argument " << arg << " not found" << std::endl;
            help();
            return -1;
        }
    }

    if (checkRunning()) {
        std::cout << "Another " << thisProgram << " is running, please close it before starting another one, only one instance is allowed"
                  << std::endl;
        return 0;
    }

    // nice(-20); //We should try to set highest priority

    TRESTDAQManager daqManager;

    std::cout << "Installing signal handler" << std::endl;
    // Install signal handler
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGILL, signal_handler);
    signal(SIGTRAP, signal_handler);
    signal(SIGABRT, signal_handler);
    signal(SIGIOT, signal_handler);
    signal(SIGFPE, signal_handler);
    signal(SIGBUS, signal_handler);
    signal(SIGSEGV, signal_handler);
    signal(SIGKILL, signal_handler);
    signal(SIGPIPE, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGSTKFLT, signal_handler);
    signal(SIGSYS, signal_handler);

    if (!cfgFile.empty()) {
        int shmid;
        TRESTDAQManager::sharedMemoryStruct* mem;
        if (!TRESTDAQManager::GetSharedMemory(shmid, &mem, 0)) {
            std::cerr << "Cannot get shared memory!!" << std::endl;
            return -1;
        }
        sprintf(mem->cfgFile, cfgFile.c_str());
        TRESTDAQManager::DetachSharedMemory(&mem);
        daqManager.dataTaking();
    } else {
        daqManager.run();
    }
}
