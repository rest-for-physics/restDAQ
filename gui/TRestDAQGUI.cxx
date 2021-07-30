
#include "TRestDAQGUI.h"

#include <iostream>

#include "TRESTDAQManager.h"

ClassImp(TRestDAQGUI);

TRestDAQGUI::TRestDAQGUI(const int& p, const int& q, std::string decodingFile) {
    fMain = new TGMainFrame(gClient->GetRoot(), p, q, kHorizontalFrame);

    fMain->SetCleanup(kDeepCleanup);

    LoadLastSettings();
    UpdateInputs();
    LoadDecodingFile(decodingFile);

    fVLeft = new TGVerticalFrame(fMain, 100, 200, kLHintsLeft);
    std::cout << "Vertical frame" << std::endl;
    SetInputs();
    std::cout << "Inputs Added" << std::endl;
    SetOutputs();
    std::cout << "Outputs Added" << std::endl;
    SetButtons();
    std::cout << "Buttons Added" << std::endl;

    fMain->AddFrame(fVLeft, new TGLayoutHints(kLHintsLeft));

    fECanvas = new TRootEmbeddedCanvas("Ecanvas", fMain, 500, 200);
    fECanvas->GetCanvas()->Divide(2, 2);
    fMain->AddFrame(fECanvas, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 10, 10, 10, 1));

    meanRateGraph = new TGraph();
    meanRateGraph->SetLineColor(kBlue);
    meanRateGraph->SetMarkerColor(kBlue);
    meanRateGraph->SetMarkerStyle(20);

    instantRateGraph = new TGraph();
    instantRateGraph->GetXaxis()->SetTimeDisplay(1);
    instantRateGraph->GetXaxis()->SetTimeFormat("%m-%d %H:%M");
    instantRateGraph->GetXaxis()->SetTimeOffset(0);
    instantRateGraph->GetXaxis()->SetTitle("Date");
    instantRateGraph->GetYaxis()->SetTitle("Rate (Hz)");
    instantRateGraph->SetTitle("Rate");
    instantRateGraph->SetLineColor(kRed);
    instantRateGraph->SetMarkerColor(kRed);
    instantRateGraph->SetMarkerStyle(22);
    instantRateGraph->SetPoint(0, tNow, 0);
    fECanvas->GetCanvas()->cd(1);
    instantRateGraph->Draw();

    spectrum = new TH1I("Spectrum", "Spectrum", 1000, 0, 100000);
    spectrum->GetXaxis()->SetTitle("Amplitude");
    spectrum->GetYaxis()->SetTitle("Counts");
    fECanvas->GetCanvas()->cd(2);
    spectrum->Draw();

    fECanvas->GetCanvas()->cd(3);
    pulses = fECanvas->GetCanvas()->DrawFrame(0, 0, 512, 4000);
    pulses->SetTitle("Pulses");
    pulses->GetXaxis()->SetTitle("TimeBin");
    pulses->GetYaxis()->SetTitle("ADC");

    fECanvas->GetCanvas()->cd(4);
    if (nStrips)
        hitmap = new TH2I("HitMap", "HitMap", (nStrips / 2) / +1, 0, nStrips, (nStrips / 2) + 1, 0, nStrips);
    else
        hitmap = new TH2I();

    hitmap->Draw("COLZ");

    fECanvas->GetCanvas()->Update();

    fMain->SetWindowName("TRestDAQGUI");
    fMain->MapSubwindows();
    fMain->Resize();
    fMain->Layout();
    fMain->DontCallClose();
    fMain->MapWindow();

    tNow = TRESTDAQ::getCurrentTime();

    pthread_create(&updateT, NULL, UpdateThread, (void*)this);
    pthread_create(&readerT, NULL, ReaderThread, (void*)this);
}

TRestDAQGUI::~TRestDAQGUI() {
    exitGUI = true;
    pthread_join(updateT, NULL);
    pthread_join(readerT, NULL);

    if (cfgFileName != "none") SaveLastSettings();
    fMain->Cleanup();
    std::cout << "Destroying" << std::endl;
    gApplication->Terminate(0);
    delete fMain;
}

void TRestDAQGUI::LoadDecodingFile(const std::string& decodingFile) {
    std::ifstream dec(decodingFile);
    if (!dec.is_open()) return;
    std::string line;

    while (getline(dec, line)) {
        istringstream ss(line);
        int id, chan;
        if (ss >> id >> chan) decoding[id] = chan;
        if (chan > nStrips) nStrips = chan;
        // std::cout<<id<<" "<<chan<<std::endl;
    }
    nStrips++;
    nStrips /= 2;
    std::cout << "NStrips " << nStrips << std::endl;
}

void TRestDAQGUI::SetInputs() {
    // 2 Acquitision intpus
    TGHorizontalFrame* fInputFrame = new TGHorizontalFrame(fVLeft, 200, 100, kLHintsRight);
    TGGroupFrame* fInputParam = new TGGroupFrame(fInputFrame, "Input parameters", kVerticalFrame);

    // 2.1 Type Frame
    TGHorizontalFrame* typeFrame = new TGHorizontalFrame(fInputParam, 200, 20, kLHintsCenterX);

    typeLabel = new TGLabel(typeFrame, "Run type");
    typeCombo = new TGComboBox(typeFrame);
    typeCombo->Resize(100, 20);
    for (const auto& [name, t] : daq_metadata_types::acqTypes_map) typeCombo->AddEntry(name.c_str(), (int)t);
    typeCombo->Select(type);
    typeCombo->SetEnabled(kFALSE);

    typeFrame->AddFrame(typeLabel, new TGLayoutHints(kLHintsLeft, 1, 1, 1, 1));
    typeFrame->AddFrame(typeCombo, new TGLayoutHints(kLHintsRight, 1, 1, 1, 1));

    fInputParam->AddFrame(typeFrame, new TGLayoutHints(kLHintsCenterX, 1, 1, 1, 1));

    // 2.2 Config Frame
    TGHorizontalFrame* cfgFrame = new TGHorizontalFrame(fInputParam, 200, 20, kLHintsCenterX);
    cfgLabel = new TGLabel(cfgFrame, "CfgFile");
    cfgName = new TGTextEntry(cfgFrame, "                 ");
    cfgName->SetText(cfgFileName.c_str());
    cfgName->SetToolTipText(cfgFileName.c_str());
    cfgName->SetEnabled(kFALSE);

    cfgButton = new TGTextButton(cfgFrame);
    cfgButton->SetText("...");
    cfgButton->Connect("Clicked()", "TRestDAQGUI", this, "cfgButtonPressed()");
    cfgButton->SetToolTipText("Click to select config file");
    cfgButton->SetEnabled(kFALSE);

    cfgFrame->AddFrame(cfgLabel, new TGLayoutHints(kLHintsLeft, 1, 1, 1, 1));
    cfgFrame->AddFrame(cfgName, new TGLayoutHints(kLHintsCenterX, 1, 1, 1, 1));
    cfgFrame->AddFrame(cfgButton, new TGLayoutHints(kLHintsCenterX, 1, 1, 1, 1));

    fInputParam->AddFrame(cfgFrame, new TGLayoutHints(kLHintsCenterX, 1, 1, 1, 1));

    // 2.3 NEvents
    TGHorizontalFrame* nEventsFrame = new TGHorizontalFrame(fInputParam, 200, 20, kLHintsCenterX);
    nEventsLabel = new TGLabel(nEventsFrame, "NEvents:");
    nEventsEntry = new TGTextEntry(nEventsFrame, "          0");
    nEventsEntry->SetText(std::to_string(nEvents).c_str());

    nEventsEntry->SetToolTipText("Number of events to be acquired, use 0 for infinite loop");
    nEventsEntry->SetEnabled(kFALSE);

    nEventsFrame->AddFrame(nEventsLabel, new TGLayoutHints(kLHintsLeft, 1, 1, 1, 1));
    nEventsFrame->AddFrame(nEventsEntry, new TGLayoutHints(kLHintsCenterX, 1, 1, 1, 1));

    fInputParam->AddFrame(nEventsFrame, new TGLayoutHints(kLHintsCenterX, 1, 1, 1, 1));

    fInputFrame->AddFrame(fInputParam, new TGLayoutHints(kLHintsCenterX, 1, 1, 1, 1));

    fVLeft->AddFrame(fInputFrame, new TGLayoutHints(kLHintsCenterX, 1, 1, 1, 1));
}

void TRestDAQGUI::SetOutputs() {
    // 3 Acquisition outpus
    TGVerticalFrame* fOutputFrame = new TGVerticalFrame(fVLeft, 300, 80, kLHintsCenterX);
    TGGroupFrame* fOutputParam = new TGGroupFrame(fOutputFrame, "Output parameters");

    // 3.1 Run Frame
    TGHorizontalFrame* runFrame = new TGHorizontalFrame(fOutputParam, 200, 20, kLHintsCenterX);
    runLabel = new TGLabel(runFrame, "Run");
    runName = new TGTextEntry(runFrame, "                 ");
    runName->SetText(runN.c_str());
    runName->SetToolTipText(cfgFileName.c_str());
    runName->SetEnabled(kFALSE);
    runName->SetToolTipText(runN.c_str());

    runFrame->AddFrame(runLabel, new TGLayoutHints(kLHintsCenterX | kLHintsCenterY | kLHintsExpandX, 1, 1, 1, 1));
    runFrame->AddFrame(runName, new TGLayoutHints(kLHintsCenterX | kLHintsCenterY | kLHintsExpandX, 1, 1, 1, 1));

    fOutputParam->AddFrame(runFrame, new TGLayoutHints(kLHintsCenterX | kLHintsExpandX, 1, 1, 1, 1));

    // 3.2 Rate Frame
    TGHorizontalFrame* rateFrame = new TGHorizontalFrame(fOutputParam, 200, 20, kLHintsCenterX);
    rateLabel = new TGLabel(rateFrame, "Rate 0.00 Hz");
    rateFrame->AddFrame(rateLabel, new TGLayoutHints(kLHintsCenterX | kLHintsCenterY | kLHintsExpandX, 1, 1, 1, 1));
    fOutputParam->AddFrame(rateFrame, new TGLayoutHints(kLHintsCenterX | kLHintsExpandX, 1, 1, 1, 1));

    // 3.3 Counter Frame
    TGHorizontalFrame* counterFrame = new TGHorizontalFrame(fOutputParam, 200, 20, kLHintsCenterX);

    counterLabel = new TGLabel(counterFrame, "Events: 0         ");

    counterFrame->AddFrame(counterLabel, new TGLayoutHints(kLHintsCenterX | kLHintsCenterY | kLHintsExpandX, 1, 1, 1, 1));

    fOutputParam->AddFrame(counterFrame, new TGLayoutHints(kLHintsCenterX | kLHintsExpandX, 1, 1, 1, 1));

    // 3.4 Status Frame
    TGHorizontalFrame* statusFrame = new TGHorizontalFrame(fOutputParam, 200, 20, kLHintsCenterX);

    std::string st = "STATUS: UNKNOWN";
    statusLabel = new TGLabel(statusFrame, st.c_str());

    statusFrame->AddFrame(statusLabel, new TGLayoutHints(kLHintsCenterX | kLHintsCenterY | kLHintsExpandX, 1, 1, 1, 1));

    fOutputParam->AddFrame(statusFrame, new TGLayoutHints(kLHintsCenterX | kLHintsExpandX, 1, 1, 1, 1));

    fOutputFrame->AddFrame(fOutputParam, new TGLayoutHints(kLHintsCenterX | kLHintsExpandX, 1, 1, 1, 1));

    fVLeft->AddFrame(fOutputFrame, new TGLayoutHints(kLHintsCenterX, 1, 1, 1, 1));
}

void TRestDAQGUI::SetButtons() {
    // 4 Buttons

    // 4.1 Start/Pause button
    TGHorizontalFrame* startFrame = new TGHorizontalFrame(fVLeft, 100, 30, kLHintsCenterX | kFixedWidth);
    startButton = new TGTextButton(startFrame);
    startButton->SetText("&START");
    startButton->Connect("Clicked()", "TRestDAQGUI", this, "StartPressed()");
    startButton->SetToolTipText("Start data taking");
    startButton->SetEnabled(kFALSE);
    startFrame->AddFrame(startButton, new TGLayoutHints(kLHintsCenterX | kLHintsCenterY | kLHintsExpandX, 1, 1, 1, 1));
    fVLeft->AddFrame(startFrame, new TGLayoutHints(kLHintsCenterX | kFixedWidth, 1, 1, 1, 1));

    // 4.2 stop button
    TGHorizontalFrame* stopFrame = new TGHorizontalFrame(fVLeft, 100, 30, kLHintsCenterX | kFixedWidth);
    stopButton = new TGTextButton(stopFrame, "S&TOP");
    stopButton->Connect("Clicked()", "TRestDAQGUI", this, "stopPressed()");
    stopButton->SetToolTipText("Stop data taking");
    stopButton->SetEnabled(kFALSE);
    stopFrame->AddFrame(stopButton, new TGLayoutHints(kLHintsCenterX | kLHintsCenterY | kLHintsExpandX, 1, 1, 1, 1));
    fVLeft->AddFrame(stopFrame, new TGLayoutHints(kLHintsCenterX | kFixedWidth, 1, 1, 1, 1));

    // 4.3 Quit button
    TGHorizontalFrame* quitFrame = new TGHorizontalFrame(fVLeft, 100, 30, kLHintsCenterX | kFixedWidth);
    quitButton = new TGTextButton(quitFrame, "&QUIT");
    quitButton->Connect("Clicked()", "TRestDAQGUI", this, "~TRestDAQGUI()");
    quitButton->SetToolTipText("Quit GUI (doesn't stop the run)");
    quitFrame->AddFrame(quitButton, new TGLayoutHints(kLHintsCenterX | kLHintsCenterY | kLHintsExpandX, 1, 1, 1, 1));
    fVLeft->AddFrame(quitFrame, new TGLayoutHints(kLHintsCenterX | kFixedWidth, 1, 1, 1, 1));
}

void TRestDAQGUI::StartPressed() {
    int shmid;
    TRESTDAQManager::sharedMemoryStruct* mem;
    if (!TRESTDAQManager::GetSharedMemory(shmid, &mem, 0)) {
        std::cerr << "Cannot get shared memory, please make sure that restDAQManager is running" << std::endl;
        return;
    }
    type = typeCombo->GetSelected();
    for (const auto& [name, t] : daq_metadata_types::acqTypes_map) {
        if (type == (int)t) {
            sprintf(mem->runType, name.c_str());
            break;
        }
    }

    cfgFileName = cfgName->GetText();
    sprintf(mem->cfgFile, cfgFileName.c_str());

    nEvents = std::atoi(nEventsEntry->GetText());
    mem->nEvents = nEvents;
    mem->startDAQ = 1;
    TRESTDAQManager::DetachSharedMemory(&mem);
}

void TRestDAQGUI::stopPressed() {
    int shmid;
    TRESTDAQManager::sharedMemoryStruct* mem;
    if (!TRESTDAQManager::GetSharedMemory(shmid, &mem, 0)) {
        std::cerr << "Cannot get shared memory, please make sure that restDAQManager is running" << std::endl;
        return;
    }

    mem->abortRun = 1;
    TRESTDAQManager::DetachSharedMemory(&mem);
}

void TRestDAQGUI::cfgButtonPressed() {
    cfgMain = new TGTransientFrame(gClient->GetRoot(), NULL, 400, 200);
    // cfgMain->Connect("CloseWindow()", "cfgButtonPressed", this, "CloseWindow()");
    cfgMain->SetCleanup(kDeepCleanup);
    TGMenuBar* mb = new TGMenuBar(cfgMain);
    cfgMain->AddFrame(mb, new TGLayoutHints(kLHintsTop | kLHintsLeft | kLHintsExpandX, 0, 0, 1, 1));
    TGListView* lv = new TGListView(cfgMain, 400, 200);
    cfgMain->AddFrame(lv, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    Pixel_t white;
    gClient->GetColorByName("white", white);
    fContents = new TGFileContainer(lv, kSunkenFrame, white);
    fContents->Connect("DoubleClicked(TGFrame*,Int_t)", "TRestDAQGUI", this, "cfgButtonDoubleClick(TGLVEntry*,Int_t)");

    // position relative to the parent's window
    cfgMain->CenterOnParent();

    cfgMain->SetWindowName("File List");
    cfgMain->MapSubwindows();
    cfgMain->MapWindow();
    fContents->SetDefaultHeaders();
    fContents->DisplayDirectory();
    fContents->AddFile("..");  // up level directory
    fContents->Resize();
    fContents->StopRefreshTimer();  // stop refreshing
    cfgMain->Resize();
}

void TRestDAQGUI::cfgButtonDoubleClick(TGLVEntry* f, Int_t btn) {
    if (btn != kButton1) return;

    TString name(f->GetTitle());
    const char* fname = (const char*)f->GetUserData();

    // std::cout<<"Name "<<name<<std::endl;
    if (fname) {  // Do nothing

    } else if (name.EndsWith(".rml")) {  // Write rml file in cfgName tab and exit
        TString fullPath = gSystem->pwd() + TString("/") + name;
        UpdateCfg(fullPath.Data());
        std::cout << "Setting cfg file " << fullPath << std::endl;
        delete cfgMain;
    } else {  // Navigate through a directory
        fContents->SetDefaultHeaders();
        gSystem->ChangeDirectory(name);
        fContents->ChangeDirectory(name);
        fContents->DisplayDirectory();
        fContents->AddFile("..");  // up level directory
        fContents->Resize();
        fContents->StopRefreshTimer();  // stop refreshing
        cfgMain->Resize();
    }
}

void TRestDAQGUI::LoadLastSettings() {
    std::string fileN = REST_USER_PATH + "/" + lastSetFile;
    std::ifstream file(fileN);
    if (file.is_open())
        file >> cfgFileName >> type >> nEvents;
    else
        std::cout << "Cannot open " << fileN << std::endl;
}

void TRestDAQGUI::SaveLastSettings() {
    std::string fileN = REST_USER_PATH + "/" + lastSetFile;
    std::cout << "Saving last settings in " << fileN << std::endl;
    std::ofstream file(fileN);
    if (file.is_open())
        file << cfgFileName << "\t" << type << "\t" << nEvents << "\n";
    else
        std::cout << "Cannot open " << fileN << std::endl;
}

void TRestDAQGUI::UpdateInputs() {

  int shmid;
  TRESTDAQManager::sharedMemoryStruct* mem;
  if (!TRESTDAQManager::GetSharedMemory(shmid, &mem, 0)) {
        std::cerr << "Cannot get shared memory, please make sure that restDAQManager is running" << std::endl;
        return;
    }
    //Only update if is DAQ running
    if(mem->status != 1){
      TRESTDAQManager::DetachSharedMemory(&mem);
      return;
    }

    auto rT = daq_metadata_types::acqTypes_map.find(mem->runType);
    if (rT != daq_metadata_types::acqTypes_map.end()) type = (int)rT->second;

    cfgFileName = mem->cfgFile;
    nEvents = mem->nEvents;

    TRESTDAQManager::DetachSharedMemory(&mem);

}

void TRestDAQGUI::UpdateOutputs() {
    std::string tmp = "Events: " + std::to_string(eventCount);

    if (tmp != counterLabel->GetText()->Data()) counterLabel->SetText(tmp.c_str());

    double r = 0;
    double tNow = TRESTDAQ::getCurrentTime();
    if ((tNow - oldTime) > 0 && (eventCount - oldEvents) >= 0) r = (oldEvents - eventCount) / (oldTime - tNow);

    oldTime = tNow;
    oldEvents = eventCount;

    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << r;
    tmp = "Rate " + ss.str() + " Hz";

    if (tmp != rateLabel->GetText()->Data()) rateLabel->SetText(tmp.c_str());

    tmp = "STATUS: ";
    if (status == 0)
        tmp += "STOPPED";
    else if (status == 1)
        tmp += "RUNNING";
    else
        tmp += "UNKNOWN";

    if (tmp != statusLabel->GetText()->Data()) statusLabel->SetText(tmp.c_str());

    if (runN != runName->GetText()) {
        runName->SetText(runN.c_str());
        runName->SetToolTipText(runN.c_str());
    }
}

void TRestDAQGUI::UpdateCfg(const std::string& cN) {
    cfgFileName = cN;
    cfgName->SetText(cfgFileName.c_str());
    cfgName->SetToolTipText(cfgFileName.c_str());
}

void TRestDAQGUI::SetRunningState() {
    startButton->SetEnabled(kFALSE);
    stopButton->SetEnabled(kTRUE);
    typeCombo->SetEnabled(kFALSE);
    cfgName->SetEnabled(kFALSE);
    cfgButton->SetEnabled(kFALSE);
    nEventsEntry->SetEnabled(kFALSE);
}

void TRestDAQGUI::SetStoppedState() {
    startButton->SetEnabled(kTRUE);
    stopButton->SetEnabled(kFALSE);
    typeCombo->SetEnabled(kTRUE);
    cfgName->SetEnabled(kTRUE);
    cfgButton->SetEnabled(kTRUE);
    nEventsEntry->SetEnabled(kTRUE);
}

void TRestDAQGUI::SetUnknownState() {
    startButton->SetEnabled(kFALSE);
    stopButton->SetEnabled(kFALSE);
    typeCombo->SetEnabled(kFALSE);
    cfgName->SetEnabled(kFALSE);
    cfgButton->SetEnabled(kFALSE);
}

bool TRestDAQGUI::GetDAQManagerParams() {
    int shmid;
    TRESTDAQManager::sharedMemoryStruct* mem;
    if (!TRESTDAQManager::GetSharedMemory(shmid, &mem, 0)) {
        std::cerr << "Cannot get shared memory, please make sure that restDAQManager is running" << std::endl;
        return false;
    }

    status = mem->status;
    eventCount = mem->eventCount;
    runN = std::string(mem->runName);
    auto rT = daq_metadata_types::acqTypes_map.find(mem->runType);
    if (rT != daq_metadata_types::acqTypes_map.end()) type = (int)rT->second;

    TRESTDAQManager::DetachSharedMemory(&mem);

    return true;
}

void* TRestDAQGUI::UpdateThread(void* arg) {
    TRestDAQGUI* inst = (TRestDAQGUI*)arg;
    inst->UpdateParams();
    return NULL;
}

void TRestDAQGUI::UpdateParams() {
    while (!exitGUI) {
        tNow = TRESTDAQ::getCurrentTime();
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));
        int oldStatus = status;
        if (!GetDAQManagerParams()) {
            status = -1;
            if (status != oldStatus) {
                UpdateOutputs();
                SetUnknownState();
            }
            continue;
        }

        UpdateOutputs();
        if (status == oldStatus) continue;  // Check if STATUS has changed
        if (status) {
            SetRunningState();
        } else {
            SetStoppedState();
        }
    }
    std::cout << "Exiting update thread " << std::endl;
}

void* TRestDAQGUI::ReaderThread(void* arg) {
    TRestDAQGUI* inst = (TRestDAQGUI*)arg;
    inst->READ();
    return NULL;
}

void TRestDAQGUI::READ() {

    while (!exitGUI) {

       std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));

        if (status == 1) {
            int fSize = TRESTDAQManager::GetFileSize(runN);
            if(fSize < MIN_FILE_SIZE)continue;//Wait till filesize is big enough

            TFile f(runN.c_str());
            if (f.IsZombie()) continue;
            if (!f.ReadKeys()) continue;

            spectrum->Clear();
            spectrum->Reset();
            hitmap->Clear();
            hitmap->Reset();
            instantRateGraph->Set(0);
            instantRateGraph->SetPoint(0, tNow, 0);
            meanRateGraph->Set(0);
            meanRateGraph->SetPoint(0, tNow, 0);
            rateGraphCounter = 0;

            std::cout << "Reading file " << runN << std::endl;

            TTree* tree = (TTree*)f.Get("EventTree");
            TRestRawSignalEvent* fEvent;
            tree->SetBranchAddress("TRestRawSignalEventBranch", &fEvent);
            int i = 0;
            double oldTimeEvent = 0;
            int oldEventCount = 0;
            double timeUpdate = tNow;
            while (status == 1 && !exitGUI) {
                int entries = tree->GetEntries();
                while (i < entries && status == 1 && !exitGUI) {
                    tree->GetEntry(i);
                    double timeEvent = fEvent->GetTime();
                    if (i == 0) {
                        startTimeEvent = timeEvent;
                        oldTimeEvent = timeEvent;
                    }
                    UpdateRate(timeEvent, oldTimeEvent, i, oldEventCount);
                    AnalyzeEvent(fEvent, timeUpdate);
                    i++;
                }
                tree->Refresh();
                std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));
            }
            f.Close();
        }
    }

    std::cout << "Exiting reading thread " << std::endl;
}

void TRestDAQGUI::UpdateRate(const double& currentTimeEv, double& oldTimeEv, const int& currentEventCount, int& oldEventCount) {
    // std::cout<<currentTimeEv<<" "<< oldTimeEv<<" "<<currentTimeEv - oldTimeEv<<std::endl;
    if (currentTimeEv - oldTimeEv < PLOTS_UPDATE_TIME) return;

    double meanRate = currentEventCount / (currentTimeEv - startTimeEvent);
    double instantRate = (currentEventCount - oldEventCount) / (currentTimeEv - oldTimeEv);

    // std::cout<<"Rate "<<meanRate<<" "<<instantRate<<" "<<currentEventCount<<" "<<oldEventCount<<std::endl;

    double rootTime = (currentTimeEv + oldTimeEv) / 2.;

    instantRateGraph->SetPoint(rateGraphCounter, rootTime, instantRate);
    meanRateGraph->SetPoint(rateGraphCounter, rootTime, meanRate);

    oldEventCount = currentEventCount;
    oldTimeEv = currentTimeEv;
    rateGraphCounter++;
}

void TRestDAQGUI::AnalyzeEvent(TRestRawSignalEvent* fEvent, double& oldTimeUpdate) {

    bool updatePlots = tNow > (oldTimeUpdate + PLOTS_UPDATE_TIME);

    if (updatePlots) {
        for (auto p : pulsesGraph) delete p;
        pulsesGraph.clear();
    }

    std::vector<double> pY(512);

    int evAmplitude = 0;
    std::map<int, int> hmap;
    for (int s = 0; s < fEvent->GetNumberOfSignals(); s++) {
        TRestRawSignal* signal = fEvent->GetSignal(s);
        int max = 0;
        double baseLine = 0, baseLineSigma = 0;
        int nPoints = 0;
        for (int i = 0; i < signal->GetNumberOfPoints(); i++) {
            int val = signal->GetData(i);
            if (val > max) max = val;
            pY[i] = val;

            if (i > 100 || val == 0) continue;
            baseLine += val;
            baseLineSigma += val * val;
            nPoints++;
        }

        if (nPoints > 0) {
            baseLine /= nPoints;
            baseLineSigma = TMath::Sqrt(baseLineSigma / nPoints - baseLine * baseLine);
        }

        if (max > baseLine + N_SIGMA_THRESHOLD * baseLineSigma) {  // Only pulses above certain threshold
            max -= baseLine;
            evAmplitude += max;
            hmap[signal->GetID()] = max;
            if (updatePlots) {
                TGraph* gr = new TGraph(signal->GetNumberOfPoints(), &(pX[0]), &(pY[0]));
                pulsesGraph.emplace_back(gr);
            }
        }
    }

    if (!decoding.empty() && !hmap.empty() && evAmplitude > 0) FillHitmap(hmap);

    spectrum->Fill(evAmplitude);

    if (updatePlots) {
        if (meanRateGraph->GetN() > 0 && instantRateGraph->GetN() > 0) {
            fECanvas->GetCanvas()->cd(1);
            instantRateGraph->Draw("ALP");
            meanRateGraph->Draw("LP");
        }

        fECanvas->GetCanvas()->cd(2);
        spectrum->Draw();

        int color = 1;
        fECanvas->GetCanvas()->cd(3);
        for (auto gr : pulsesGraph) {
            gr->SetLineColor(color);
            gr->Draw("SAME");
            color++;
        }

        if (!decoding.empty()) {
            fECanvas->GetCanvas()->cd(4);
            hitmap->Draw("COLZ");
        }

        fECanvas->GetCanvas()->Update();
        oldTimeUpdate = tNow;
    }
}

void TRestDAQGUI::FillHitmap(const std::map<int, int>& hmap) {
    double posX = 0, posY = 0;
    int ampX = 0, ampY = 0;
    for (const auto& [sID, ampl] : hmap) {
        if (decoding.find(sID) == decoding.end()) continue;
        int channel = decoding[sID] % (nStrips * 2);
        int rChannel = channel % nStrips;
        if (channel / nStrips == 0) {
            posX += ampl * rChannel;
            ampX += ampl;
        } else {
            posY += ampl * rChannel;
            ampY += ampl;
        }
    }

    if (ampX == 0 || ampY == 0) return;

    posX /= ampX;
    posY /= ampY;
    hitmap->Fill(posX, posY);
}
