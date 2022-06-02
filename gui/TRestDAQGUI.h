
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
#include <TH2Poly.h>
#include <TObject.h>
#include <TPaveText.h>
#include <TRootEmbeddedCanvas.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TSystemDirectory.h>
#include <TTimeStamp.h>
#include <TTree.h>
#include <TVirtualX.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "TRestRawSignalEvent.h"
#include "TRestDAQGUIMetadata.h"
#ifdef REST_DetectorLib
#include "TRestDetectorReadout.h"
#endif


constexpr int PLOTS_UPDATE_TIME = 5;//Seconds to update the plots
constexpr int SLEEP_TIME = 500;//Miliseconds to sleep

class TRestDAQGUI {
   public:
    TGMainFrame* fMain;
    TGVerticalFrame* fVLeft;
    static inline TGTextButton *startButton, *stopButton, *quitButton, *cfgButton, *startUpButton;
    TGLabel *runLabel, *cfgLabel, *typeLabel, *nEventsLabel;
    static inline TGLabel *statusLabel,*counterLabel, *rateLabel;
    static inline TGTextEntry* runName, *cfgName, *nEventsEntry;

    static inline TGComboBox* typeCombo;

    static inline TRestDAQGUIMetadata *guiMetadata;

    TGFileContainer* fContents;
    TGTransientFrame* cfgMain;
    TGTransientFrame* startUpMain;

    std::string cfgFileName = "none";
    static inline std::string runN = "none";

    static inline double oldTime = 0;
    static inline double tNow, startTimeEvent;
    static inline int rateGraphCounter = 0;

    static inline Int_t eventCount = 0, oldEvents = 0, nEvents = 0;
    static inline Int_t type = 0;

    static inline TH1* pulses = nullptr;
    static inline TH1I* spectrum = nullptr;
    static inline TH2Poly* hitmap = nullptr;
    static inline TRootEmbeddedCanvas* fECanvas = nullptr;

    static inline TGraph *meanRateGraph = nullptr, *instantRateGraph = nullptr;
    static inline std::vector<TGraph*> pulsesGraph;

    std::thread updateT, readerT;

    TRestDAQGUI(const int& p, const int& q, TRestDAQGUIMetadata *mt);
    ~TRestDAQGUI();  // need to delete here created widgets

    void SetInputs();
    void SetOutputs();
    void SetButtons();
    void StartPressed();
    void ReadInputs();
    void DisableInputs();
    void ReadCfgFileName();
    void VerifyEventsEntry();

    void CloseCfgWindow(){
      if(cfgMain)delete cfgMain;
    }

    void CloseStartUpWindow(){
      if(startUpMain)delete startUpMain;
    }

    void LoadLastSettings();
    void SaveLastSettings();

    void cfgButtonPressed();
    void cfgButtonDoubleClick(TGLVEntry* f, Int_t btn);
    void StopPressed();
    void StartUpPressed();
    static void UpdateParams();
    void UpdateInputs();
    static void UpdateOutputs();
    void UpdateCfg(const std::string& cN);

    static void SetRunningState();
    static void SetStoppedState();
    static void SetUnknownState();

    static void READ();
    static void AnalyzeEvent(TRestRawSignalEvent* fEvent, double& oldTimeUpdate);
    static void UpdateRate(const double& currentTimeEv, double& oldTimeEv, const int& currentEventCount, int& oldEventCount);

    static bool GetDAQManagerParams(double &lastTimeUpdate);

    const std::string lastSetFile = "DAQLastSettings.txt";
    static std::atomic<bool> exitGUI;
    static std::atomic<int> status;

    static void FillHitmap(const std::map<int, int>& hmap);

    #ifdef REST_DetectorLib
    static inline TRestDetectorReadout* fReadout = nullptr;
    #endif

    ClassDef(TRestDAQGUI, 1)
};

#endif
