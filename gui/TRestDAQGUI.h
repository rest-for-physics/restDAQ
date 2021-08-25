
#ifndef __TREST_DAQ_GUI__
#define __TREST_DAQ_GUI__

#include <TApplication.h>
#include <TArray.h>
#include <TCanvas.h>
#include <TError.h>
#include <TF1.h>
#include <TFile.h>
#include <TGButton.h>
#include <TGClient.h>
#include <TGComboBox.h>
#include <TGFSContainer.h>
#include <TGFileBrowser.h>
#include <TGFrame.h>
#include <TGLabel.h>
#include <TGListTree.h>
#include <TGMenu.h>
#include <TGNumberEntry.h>
#include <TGTextEntry.h>
#include <TGraph.h>
#include <TH1.h>
#include <TH2.h>
#include <TObject.h>
#include <TPaveText.h>
#include <TRootEmbeddedCanvas.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TSystemDirectory.h>
#include <TTimeStamp.h>
#include <TTree.h>
#include <TVirtualX.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "TRestRawSignalEvent.h"

class TRestDAQGUI {
   public:
    TGMainFrame* fMain;
    TGVerticalFrame* fVLeft;
    TGTextButton *startButton, *stopButton, *quitButton, *cfgButton;
    TGLabel *statusLabel, *runLabel, *rateLabel, *counterLabel, *cfgLabel, *typeLabel, *nEventsLabel;
    TGTextEntry* runName;
    TGTextEntry* nEventsEntry;

    TGTextEntry* cfgName;

    TGComboBox* typeCombo;

    TGFileContainer* fContents;
    TGTransientFrame* cfgMain;

    std::string cfgFileName = "none", runN = "none";
    int status = -1;

    double startTimeEvent;
    double oldTime = 0, tNow;
    int rateGraphCounter = 0;
    int nStrips = 0;

    Int_t eventCount = 0, oldEvents = 0, nEvents = 0;
    Int_t type = 0;

    TH1* pulses;
    TH1I* spectrum;
    TH2I* hitmap;
    TRootEmbeddedCanvas* fECanvas;

    TGraph *meanRateGraph, *instantRateGraph;
    std::vector<TGraph*> pulsesGraph;

    pthread_t updateT, readerT;

    TRestDAQGUI(const int& p, const int& q, std::string decodingFile = "");
    ~TRestDAQGUI();  // need to delete here created widgets

    void SetInputs();
    void SetOutputs();
    void SetButtons();
    void StartPressed();
    void ReadInputs();
    void DisableInputs();
    void ReadCfgFileName();
    void VerifyEventsEntry();


    void LoadLastSettings();
    void SaveLastSettings();

    void cfgButtonPressed();
    void cfgButtonDoubleClick(TGLVEntry* f, Int_t btn);
    void stopPressed();
    void UpdateParams();
    void UpdateInputs();
    void UpdateOutputs();
    void UpdateCfg(const std::string& cN);

    void SetRunningState();
    void SetStoppedState();
    void SetUnknownState();

    void READ();
    void AnalyzeEvent(TRestRawSignalEvent* fEvent, double& oldTimeUpdate);
    void UpdateRate(const double& currentTimeEv, double& oldTimeEv, const int& currentEventCount, int& oldEventCount);

    bool GetDAQManagerParams();

    static void* UpdateThread(void* arg);
    static void* ReaderThread(void* arg);
    const std::string lastSetFile = "DAQLastSettings.txt";
    static std::atomic<bool> exitGUI;

    const std::vector<double> initPX() {
        std::vector<double> v(512);
        for (int i = 0; i < v.size(); i++) v[i] = i;
        return v;
    }
    const std::vector<double> pX = initPX();

    std::map<int, int> decoding;

    void LoadDecodingFile(const std::string& decodingFile);
    void FillHitmap(const std::map<int, int>& hmap);

    const int PLOTS_UPDATE_TIME = 5;//Seconds to update the plots
    const int SLEEP_TIME = 500;//Miliseconds to sleep
    const int N_SIGMA_THRESHOLD = 5;//Sigma threshold to apply to the pulses to take into accout in the writting
    const int MIN_FILE_SIZE = 15*1024;//Minimum file size for a file to be opened

    ClassDef(TRestDAQGUI, 1)
};

#endif
