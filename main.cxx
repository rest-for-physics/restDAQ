#include <TApplication.h>
#include <TRestDAQGUI.h>
#include <dirent.h>

#include <csignal>
#include <iostream>

#include "CLI/App.hpp"
#include "CLI/Config.hpp"
#include "CLI/Formatter.hpp"
#include "TRESTDAQManager.h"

using namespace std;
namespace fs = std::filesystem;

constexpr char* thisProgram = "restDAQ";

void signal_handler(int signum) {
    cout << "Signal handler for '" << signum << "'" << endl;
    TRESTDAQManager::ExitManager();
}

bool checkRunning() {
    DIR* dir;
    struct dirent* ent;
    char* endptr;

    int nInstances = 0;

    if (!(dir = opendir("/proc"))) {
        cerr << "Cannot open /proc" << endl;
        return true;
    }

    while ((ent = readdir(dir)) != nullptr) {
        /* if endptr is not a null character, the directory is not
         * entirely numeric, so ignore it */
        long lpid = strtol(ent->d_name, &endptr, 10);
        if (*endptr != '\0') {
            continue;
        }
        /* try to open the cmdline file */
        string fName = "/proc/" + to_string(lpid) + "/status";
        ifstream fin(fName, ifstream::in);
        if (fin.fail()) {
            cerr << "Cannot open " + fName + " skipping..." << endl;
            continue;
        }

        string line;
        getline(fin, line);
        stringstream str;
        str << line;
        string head, binary;
        str >> head >> binary;
        if (string(thisProgram).substr(0, 15) == binary) {
            cout << binary << " pid: " << lpid << endl;
            nInstances++;
        }
        fin.close();
    }
    closedir(dir);

    return nInstances > 1;
}

void initializeSignalHandler() {
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
}

int main(int argc, char** argv) {
    CLI::App app{"REST DAQ"};

    fs::path configFilename;

    CLI::App* gui = app.add_subcommand("gui", "Open the GUI. It will automatically connect to the acquisition process if it is running");
    gui->add_option("-c,--config", configFilename, "Configuration RML file for the GUI")->expected(1);

    CLI::App* run = app.add_subcommand("run", "Start acquisition process");
    run->add_option("-c,--config", configFilename, "Configuration RML file for the acquisition process")->expected(1);
    string daqIP = "192.168.10.1";  // Default IP for 'feminos' card
    run->add_option("--source,--ip", daqIP, "Hostname (IP) of the DAQ")->expected(1);
    run->add_option("--stop", "Stop previous acquisition process if it exists")->expected(0);
    run->add_option("--startup", "Perform startup of electronics")->expected(0);
    run->add_option("--exit", "Perform startup of electronics")->expected(0);

    app.require_subcommand(1);
    CLI11_PARSE(app, argc, argv);

    const auto subcommand = app.get_subcommands().back();
    const string subcommandName = subcommand->get_name();
    if (subcommandName == "gui") {
        TApplication tApp("DAQ GUI", &argc, argv);
        TRestDAQGUIMetadata* daqGuiMetadata;
        if (configFilename.empty()) {
            daqGuiMetadata = new TRestDAQGUIMetadata();
        } else {
            daqGuiMetadata = new TRestDAQGUIMetadata(configFilename.string().c_str());
        }
        new TRestDAQGUI(1000, 700, daqGuiMetadata);

        tApp.Run();
    } else if (subcommandName == "run") {
        cout << "options: " << run->get_options().size() << endl;
        if (!run->get_option("--exit")->empty()) {
            cout << "Stopping existing acquisition process and exiting" << endl;
            TRESTDAQManager::ExitManager();
            return 0;
        }
        if (!run->get_option("--stop")->empty()) {
            cout << "Stopping existing acquisition process" << endl;
            TRESTDAQManager::StopRun();
        }
        if (checkRunning()) {
            cout << "Another acquisition process is running but only one instance is allowed. Please close it before starting a new acquisition."
                    " You can close the existing process via the --stop flag"
                 << endl;
            return 0;
        }

        const bool startupElectronics = !run->get_option("--startup")->empty();

        initializeSignalHandler();

        TRESTDAQManager daqManager;

        if (!configFilename.empty()) {
            int shareMemoryID;
            TRESTDAQManager::sharedMemoryStruct* sharedMemory;
            if (!TRESTDAQManager::GetSharedMemory(shareMemoryID, &sharedMemory)) {
                cerr << "Error accessing shared memory" << endl;
                return 1;
            }
            char* fullPath = realpath(configFilename.c_str(), nullptr);
            if (fullPath) {
                cout << "Full path: " << fullPath << endl;
                sprintf(sharedMemory->configFilename, "%s", fullPath);
            } else {
                sprintf(sharedMemory->configFilename, "%s", configFilename.c_str());
            }
            TRESTDAQManager::DetachSharedMemory(&sharedMemory);
            if (startupElectronics) {
                cout << "Attempting electronics startup" << endl;
                daqManager.startUp();
                return 1;
            } else {
                daqManager.dataTaking();
            }
        } else {
            cout << "Starting infinite loop" << endl;
            return 1;
            daqManager.run();
        }
    }
}
