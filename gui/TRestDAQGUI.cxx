
#include <iostream>
#include "TGMsgBox.h"

#include "TRestDAQGUI.h"
#include "TRESTDAQManager.h"

ClassImp(TRestDAQGUI);

std::atomic<bool> TRestDAQGUI::exitGUI(false);
std::atomic<int> TRestDAQGUI::status(-1);


TRestDAQGUI::TRestDAQGUI(const int& p, const int& q, TRestDAQGUIMetadata *mt) {
    fMain = new TGMainFrame(gClient->GetRoot(), p, q, kHorizontalFrame);

    fMain->SetCleanup(kDeepCleanup);

    guiMetadata = mt;
    
    if(guiMetadata==nullptr) guiMetadata = new TRestDAQGUIMetadata();
    
    LoadLastSettings();
    UpdateInputs();

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

    fECanvas->GetCanvas()->cd(1);
    meanRateGraph = new TGraph();
    meanRateGraph->SetLineColor(kBlue);
    meanRateGraph->SetMarkerColor(kBlue);
    meanRateGraph->SetMarkerStyle(20);

    tNow = TRESTDAQ::getCurrentTime();

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
    instantRateGraph->Draw();

    fECanvas->GetCanvas()->cd(2);
    spectrum = new TH1I("Spectra", "Spectra", guiMetadata->GetBinsSpectra(), 0, guiMetadata->GetSpetraMax());
    spectrum->GetXaxis()->SetTitle("Amplitude");
    spectrum->GetYaxis()->SetTitle("Counts");
    spectrum->Draw();

    fECanvas->GetCanvas()->cd(3);
    pulses = fECanvas->GetCanvas()->DrawFrame(0, 0, 512, 4100);
    pulses->SetTitle("Pulses");
    pulses->GetXaxis()->SetTitle("TimeBin");
    pulses->GetYaxis()->SetTitle("ADC");

    hitmap = new TH2Poly("Hitmap empty", "Hitmap empty",0,1,0,1);
    #ifdef REST_DetectorLib
      TFile *file = TFile::Open(guiMetadata->GetReadoutFile());
        if(file == nullptr || file->IsZombie() ) {
          std::cout<<"Not valid file "<<guiMetadata->GetReadoutFile()<<std::endl;
        } else if (file->GetKey(guiMetadata->GetReadoutName()) == nullptr ) {
          std::cout<<"Key "<< guiMetadata->GetReadoutName()<<" not found "<<std::endl;
          file->Close();
        } else {
          fReadout = (TRestDetectorReadout *)file->Get(guiMetadata->GetReadoutName());
            if(fReadout){
              fReadout->PrintMetadata();
              TRestDetectorReadoutPlane* plane = &(*fReadout)[0];
              hitmap = plane->GetReadoutHistogram();
            }
          file->Close();
        }
    #endif

    fECanvas->GetCanvas()->cd(4);
    hitmap->Draw("COLZ0");

    fECanvas->GetCanvas()->Update();

    fMain->SetWindowName("TRestDAQGUI");
    fMain->MapSubwindows();
    fMain->Resize();
    fMain->Layout();
    fMain->DontCallClose();
    fMain->MapWindow();

    tNow = TRESTDAQ::getCurrentTime();

    /// Launch threads
    updateT = std::thread(UpdateParams);
    readerT = std::thread(READ);
}

TRestDAQGUI::~TRestDAQGUI() {
    exitGUI = true;
    updateT.join();
    readerT.join();

    if (cfgFileName != "none") SaveLastSettings();
    fMain->Cleanup();
    std::cout << "Destroying" << std::endl;
    gApplication->Terminate(0);
    delete fMain;
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

    nEventsEntry->Connect("TextChanged(char *)", "TRestDAQGUI", this, "VerifyEventsEntry()");

    nEventsFrame->AddFrame(nEventsLabel, new TGLayoutHints(kLHintsLeft, 1, 1, 1, 1));
    nEventsFrame->AddFrame(nEventsEntry, new TGLayoutHints(kLHintsCenterX, 1, 1, 1, 1));

    fInputParam->AddFrame(nEventsFrame, new TGLayoutHints(kLHintsCenterX, 1, 1, 1, 1));

    // 2.4 Drift field
    TGHorizontalFrame* driftFieldFrame = new TGHorizontalFrame(fInputParam, 200, 20, kLHintsCenterX);
    driftFieldLabel = new TGLabel(driftFieldFrame, "Drift Field:");
    driftFieldEntry = new TGTextEntry(driftFieldFrame, "        0");
    driftFieldEntry->SetText(std::to_string(drift).c_str());

    driftFieldEntry->Connect("TextChanged(char *)", "TRestDAQGUI", this, "UpdateDrift()");

    driftFieldEntry->SetToolTipText("Dift field in V");
    driftFieldEntry->SetEnabled(kFALSE);

    driftFieldFrame->AddFrame(driftFieldLabel, new TGLayoutHints(kLHintsLeft, 1, 1, 1, 1));
    driftFieldFrame->AddFrame(driftFieldEntry, new TGLayoutHints(kLHintsCenterX, 1, 1, 1, 1));

    fInputParam->AddFrame(driftFieldFrame, new TGLayoutHints(kLHintsCenterX, 1, 1, 1, 1));

    // 2.5 Mesh Voltage
    TGHorizontalFrame* meshVoltageFrame = new TGHorizontalFrame(fInputParam, 200, 20, kLHintsCenterX);
    meshVoltageLabel = new TGLabel(meshVoltageFrame, "Mesh Voltage:");
    meshVoltageEntry = new TGTextEntry(meshVoltageFrame, "        0");
    meshVoltageEntry->SetText(std::to_string(mesh).c_str());

    meshVoltageEntry->Connect("TextChanged(char *)", "TRestDAQGUI", this, "UpdateMesh()");

    meshVoltageEntry->SetToolTipText("Mesh voltage in V");
    meshVoltageEntry->SetEnabled(kFALSE);

    meshVoltageFrame->AddFrame(meshVoltageLabel, new TGLayoutHints(kLHintsLeft, 1, 1, 1, 1));
    meshVoltageFrame->AddFrame(meshVoltageEntry, new TGLayoutHints(kLHintsCenterX, 1, 1, 1, 1));

    fInputParam->AddFrame(meshVoltageFrame, new TGLayoutHints(kLHintsCenterX, 1, 1, 1, 1));

    // 2.6 Pressure
    TGHorizontalFrame* pressureFrame = new TGHorizontalFrame(fInputParam, 200, 20, kLHintsCenterX);
    pressureLabel = new TGLabel(pressureFrame, "Pressure:");
    pressureEntry = new TGTextEntry(pressureFrame, "        0");
    pressureEntry->SetText(std::to_string(pressure).c_str());

    pressureEntry->Connect("TextChanged(char *)", "TRestDAQGUI", this, "UpdatePressure()");

    pressureEntry->SetToolTipText("Pressure in bar");
    pressureEntry->SetEnabled(kFALSE);

    pressureFrame->AddFrame(pressureLabel, new TGLayoutHints(kLHintsLeft, 1, 1, 1, 1));
    pressureFrame->AddFrame(pressureEntry, new TGLayoutHints(kLHintsCenterX, 1, 1, 1, 1));

    fInputParam->AddFrame(pressureFrame, new TGLayoutHints(kLHintsCenterX, 1, 1, 1, 1));

    // 2.7 Run tag
    TGHorizontalFrame* runTagFrame = new TGHorizontalFrame(fInputParam, 200, 20, kLHintsCenterX);
    runTagLabel = new TGLabel(runTagFrame, "Run Tag:");
    runTagEntry = new TGTextEntry(runTagFrame, "______________");
    runTagEntry->SetText(runTag.c_str());

    runTagEntry->SetToolTipText("Run Tag");
    runTagEntry->SetEnabled(kFALSE);

    runTagEntry->Connect("TextChanged(char *)", "TRestDAQGUI", this, "UpdateRunTag()");

    runTagFrame->AddFrame(runTagLabel, new TGLayoutHints(kLHintsLeft, 1, 1, 1, 1));
    runTagFrame->AddFrame(runTagEntry, new TGLayoutHints(kLHintsCenterX, 1, 1, 1, 1));

    fInputParam->AddFrame(runTagFrame, new TGLayoutHints(kLHintsCenterX, 1, 1, 1, 1));

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
    stopButton->Connect("Clicked()", "TRestDAQGUI", this, "StopPressed()");
    stopButton->SetToolTipText("Stop data taking");
    stopButton->SetEnabled(kFALSE);
    stopFrame->AddFrame(stopButton, new TGLayoutHints(kLHintsCenterX | kLHintsCenterY | kLHintsExpandX, 1, 1, 1, 1));
    fVLeft->AddFrame(stopFrame, new TGLayoutHints(kLHintsCenterX | kFixedWidth, 1, 1, 1, 1));

    // 4.4 startUp button
    TGHorizontalFrame* startUpFrame = new TGHorizontalFrame(fVLeft, 100, 30, kLHintsCenterX | kFixedWidth);
    startUpButton = new TGTextButton(startUpFrame, "START &UP");
    startUpButton->Connect("Clicked()", "TRestDAQGUI", this, "StartUpPressed()");
    startUpButton->SetToolTipText("Start up electronics (e.g. after power up)");
    startUpButton->SetEnabled(kFALSE);
    startUpFrame->AddFrame(startUpButton, new TGLayoutHints(kLHintsCenterX | kLHintsCenterY | kLHintsExpandX, 1, 1, 1, 1));
    fVLeft->AddFrame(startUpFrame, new TGLayoutHints(kLHintsCenterX | kFixedWidth, 1, 1, 1, 1));

    // 4.3 Quit button
    TGHorizontalFrame* quitFrame = new TGHorizontalFrame(fVLeft, 100, 30, kLHintsCenterX | kFixedWidth);
    quitButton = new TGTextButton(quitFrame, "&QUIT");
    quitButton->Connect("Clicked()", "TRestDAQGUI", this, "~TRestDAQGUI()");
    quitButton->SetToolTipText("Quit GUI (doesn't stop the run)");
    quitFrame->AddFrame(quitButton, new TGLayoutHints(kLHintsCenterX | kLHintsCenterY | kLHintsExpandX, 1, 1, 1, 1));
    fVLeft->AddFrame(quitFrame, new TGLayoutHints(kLHintsCenterX | kFixedWidth, 1, 1, 1, 1));
}

void TRestDAQGUI::VerifyEventsEntry(){

  nEventsEntry->SetText( std::to_string(std::atoi(nEventsEntry->GetText() ) ).c_str() );

}

void TRestDAQGUI::UpdateDrift(){

  drift = std::atoi(driftFieldEntry->GetText());

}

void TRestDAQGUI::UpdateMesh(){

  mesh = std::atoi(meshVoltageEntry->GetText());

}

void TRestDAQGUI::UpdatePressure(){
  pressure = std::atof(pressureEntry->GetText());
}

void TRestDAQGUI::UpdateRunTag(){

runTag = runTagEntry->GetText();

}


void TRestDAQGUI::StartPressed() {
    int shmid;
    TRESTDAQManager::sharedMemoryStruct* mem;
    if (!TRESTDAQManager::GetSharedMemory(shmid, &mem)) {
        std::cerr << "Cannot get shared memory, please make sure that restDAQManager is running" << std::endl;
        return;
    }
    type = typeCombo->GetSelected();
    for (const auto& [name, t] : daq_metadata_types::acqTypes_map) {
        if (type == static_cast<int>(t)) {
            std::cout<<name<<std::endl;
            sprintf(mem->runType, name.c_str());
            break;
        }
    }

    cfgFileName = cfgName->GetText();
    std::cout<<cfgFileName<<std::endl;
    sprintf(mem->cfgFile, cfgFileName.c_str());

    nEvents = std::atoi(nEventsEntry->GetText());
    mem->nEvents = nEvents;

    std::string tag = runTag + "_Vm_"+std::to_string(mesh) + "_Vd_" + std::to_string(drift) + "_Pr_" +std::to_string(pressure);

    sprintf(mem->runTag, tag.c_str());

    mem->startDAQ = 1;
    TRESTDAQManager::DetachSharedMemory(&mem);
}

void TRestDAQGUI::StopPressed() {
    int shmid;
    TRESTDAQManager::sharedMemoryStruct* mem;
    if (!TRESTDAQManager::GetSharedMemory(shmid, &mem)) {
        std::cerr << "Cannot get shared memory, please make sure that restDAQManager is running" << std::endl;
        return;
    }

    mem->abortRun = 1;
    TRESTDAQManager::DetachSharedMemory(&mem);
}

void TRestDAQGUI::StartUpPressed() {

    startUpMain = new TGTransientFrame(gClient->GetRoot(), fMain, 400, 200);
    startUpMain->DontCallClose(); // to avoid double deletions.
    startUpMain->SetCleanup(kDeepCleanup);
    startUpMain->Resize();
     // position relative to the parent's window
    startUpMain->CenterOnParent();

    int retval=0;
    startUpMain->Disconnect("CloseWindow()");
    startUpMain->Connect("CloseWindow()", "TRestDAQGUI", this, "CloseStartUpWindow()");
    new TGMsgBox(gClient->GetRoot(), startUpMain,
                "START UP", "Start up is only needed after power-cycle, do you want to continue?",
                kMBIconExclamation, kMBYes| kMBNo, &retval);

    startUpMain->MapWindow();
    //gClient->WaitFor(startUpMain);

    if(retval == kMBYes){
      int shmid;
      TRESTDAQManager::sharedMemoryStruct* mem;
        if (!TRESTDAQManager::GetSharedMemory(shmid, &mem)) {
          std::cerr << "Cannot get shared memory, please make sure that restDAQManager is running" << std::endl;
          return;
        }

      cfgFileName = cfgName->GetText();
      std::cout<<cfgFileName<<std::endl;
      sprintf(mem->cfgFile, cfgFileName.c_str());
      mem->startUp = 1;
      TRESTDAQManager::DetachSharedMemory(&mem);
    }

    delete startUpMain;
}

void TRestDAQGUI::cfgButtonPressed() {
    cfgMain = new TGTransientFrame(gClient->GetRoot(), fMain, 400, 200);
    cfgMain->DontCallClose(); // to avoid double deletions.
    cfgMain->Disconnect("CloseWindow()");
    cfgMain->Connect("CloseWindow()", "TRestDAQGUI", this, "CloseCfgWindow()");
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
        file >> cfgFileName >> type >> nEvents >> runTag >> drift >> mesh >> pressure;
    else
        std::cout << "Cannot open " << fileN << std::endl;
}

void TRestDAQGUI::SaveLastSettings() {
    std::string fileN = REST_USER_PATH + "/" + lastSetFile;
    std::cout << "Saving last settings in " << fileN << std::endl;
    std::ofstream file(fileN);
    if (file.is_open())
        file << cfgFileName << "\t" << type << "\t" << nEvents << "\t" << runTag << "\t" << drift << "\t" << mesh << "\t" << pressure << "\n";
    else
        std::cout << "Cannot open " << fileN << std::endl;
}

void TRestDAQGUI::UpdateInputs() {

  int shmid;
  TRESTDAQManager::sharedMemoryStruct* mem;
  if (!TRESTDAQManager::GetSharedMemory(shmid, &mem)) {
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
    startUpButton->SetEnabled(kFALSE);
    nEventsEntry->SetEnabled(kFALSE);
    driftFieldEntry->SetEnabled(kFALSE);
    meshVoltageEntry->SetEnabled(kFALSE);
    pressureEntry->SetEnabled(kFALSE);
    runTagEntry->SetEnabled(kFALSE);
}

void TRestDAQGUI::SetStoppedState() {
    startButton->SetEnabled(kTRUE);
    stopButton->SetEnabled(kFALSE);
    typeCombo->SetEnabled(kTRUE);
    cfgName->SetEnabled(kTRUE);
    cfgButton->SetEnabled(kTRUE);
    startUpButton->SetEnabled(kTRUE);
    nEventsEntry->SetEnabled(kTRUE);
    driftFieldEntry->SetEnabled(kTRUE);
    meshVoltageEntry->SetEnabled(kTRUE);
    pressureEntry->SetEnabled(kTRUE);
    runTagEntry->SetEnabled(kTRUE);
}

void TRestDAQGUI::SetUnknownState() {
    startButton->SetEnabled(kFALSE);
    stopButton->SetEnabled(kFALSE);
    startUpButton->SetEnabled(kFALSE);
    typeCombo->SetEnabled(kFALSE);
    cfgName->SetEnabled(kFALSE);
    cfgButton->SetEnabled(kFALSE);
}

bool TRestDAQGUI::GetDAQManagerParams(double &lastTimeUpdate) {
    int shmid;
    TRESTDAQManager::sharedMemoryStruct* mem;
    if (!TRESTDAQManager::GetSharedMemory(shmid, &mem, 0, false)) {
        if( (tNow - lastTimeUpdate) > 30 ){
          std::cerr << "Cannot get shared memory, please make sure that restDAQManager is running" << std::endl;
          lastTimeUpdate = tNow;
        }
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

void TRestDAQGUI::UpdateParams() {

  double lastTimeUpdate = 0;

    while (!exitGUI) {
        tNow = TRESTDAQ::getCurrentTime();
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));
        int oldStatus = status;
        if (!GetDAQManagerParams(lastTimeUpdate)) {
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

void TRestDAQGUI::READ() {

    std::string fName = "";
    bool resetPlots = true;
    double timeUpdate = 0;
    int currentEventCount =0;
    double oldTimeEvent = 0;
    int oldEventCount = 0;
    int parentRunNumber = 0;

    while (!exitGUI) {

        if (status == 1 && !exitGUI) {

            int fSize = TRESTDAQManager::GetFileSize(runN);
            //Wait till filesize is big enough
            while (fSize < guiMetadata->GetMinFileSize() && status == 1 && !exitGUI) {
              fSize = TRESTDAQManager::GetFileSize(runN);
              std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));
            }

            //Wait till filename is updated
            while (runN == fName && status == 1 && !exitGUI) {
              std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));
            }

            if (resetPlots){
              spectrum->Clear();
              spectrum->Reset();
              hitmap->Clear();
              hitmap->Reset("");
              instantRateGraph->Set(0);
              instantRateGraph->SetPoint(0, tNow, 0);
              meanRateGraph->Set(0);
              meanRateGraph->SetPoint(0, tNow, 0);
              rateGraphCounter = 0;
              timeUpdate = tNow;
              currentEventCount = 0;
              resetPlots = false;
              oldTimeEvent = 0;
              oldEventCount = 0;
              parentRunNumber = 0;
            }

            std::cout << "Checking input file " << runN << std::endl;

            TRestRun runInfo (runN);
            TRestRun restRun;
            restRun.LoadConfigFromFile(cfgFileName);
            restRun.SetRunType(runInfo.GetRunType());
            restRun.SetRunNumber(runInfo.GetRunNumber());
            restRun.SetRunTag(runInfo.GetRunTag());
            restRun.SetParentRunNumber(parentRunNumber);
            fName = realpath(restRun.FormFormat(restRun.GetOutputFileName()).Data(), NULL);

            std::cout << "Reading file " << fName << std::endl;

            TFile f(fName.c_str());
              while((f.IsZombie() || !f.ReadKeys()) && status == 1 && !exitGUI){
                f.Open(fName.c_str());
                std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));
              }

            std::cout << "Reading file " << fName << std::endl;

            TTree* tree = f.Get<TTree>("EventTree");
            TRestRawSignalEvent* fEvent = nullptr;
            tree->SetBranchAddress("TRestRawSignalEventBranch", &fEvent);

            int i = 0;
            int fCount = 0;
            while (fCount<2) {
                tree->Refresh();
                const int entries = tree->GetEntries();
                while (i < entries && status == 1 && !exitGUI) {
                    tree->GetEntry(i);
                    double timeEvent = fEvent->GetTime();
                      if (currentEventCount == 0) {
                        startTimeEvent = timeEvent;
                        oldTimeEvent = timeEvent;
                      }
                    UpdateRate(timeEvent, oldTimeEvent, currentEventCount, oldEventCount);
                    AnalyzeEvent(fEvent, timeUpdate);
                    i++;
                    currentEventCount++;
                }
                 if(fName != runN || status != 1 || exitGUI){
                   fCount++;
                 }
                std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME) );
            }
            f.Close();
            std::cout << "Closing file " << fName << std::endl;
            if(status != 1)resetPlots = true;
            parentRunNumber++;
        }
      std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));
    }

    std::cout << "Exiting reading thread " << std::endl;
}

void TRestDAQGUI::UpdateRate(const double& currentTimeEv, double& oldTimeEv, const int& currentEventCount, int& oldEventCount) {

    if (currentTimeEv - oldTimeEv < PLOTS_UPDATE_TIME) return;

    //std::cout<<currentTimeEv<<" "<< oldTimeEv<<" "<<currentTimeEv - oldTimeEv<<" "<<currentTimeEv - startTimeEvent <<std::endl;

    double meanRate = currentEventCount / (currentTimeEv - startTimeEvent);
    double instantRate = (currentEventCount - oldEventCount) / (currentTimeEv - oldTimeEv);

    //std::cout<<"Rate "<<meanRate<<" "<<instantRate<<" "<<currentEventCount<<" "<<oldEventCount<<std::endl;

    double rootTime = (currentTimeEv + oldTimeEv) / 2.;

    instantRateGraph->SetPoint(rateGraphCounter, rootTime, instantRate);
    meanRateGraph->SetPoint(rateGraphCounter, rootTime, meanRate);

    oldEventCount = currentEventCount;
    oldTimeEv = currentTimeEv;
    rateGraphCounter++;
}

void TRestDAQGUI::AnalyzeEvent(TRestRawSignalEvent* fEvent, double& oldTimeUpdate) {

    if(fEvent == nullptr) return;

    bool updatePlots = tNow > (oldTimeUpdate + PLOTS_UPDATE_TIME );

    if(updatePlots){
      for(auto &gr : pulsesGraph)delete gr;
      pulsesGraph.clear();
    }

    int evAmplitude = 0;
    std::map<int, int> hmap;
    int color =1;
    for (int s = 0; s < fEvent->GetNumberOfSignals(); s++) {
        TRestRawSignal* signal = fEvent->GetSignal(s);
        const double max = signal->GetAmplitudeFast(TVector2(guiMetadata->GetBaselineRange()), guiMetadata->GetSignalThreshold());

        if(max<=0)continue;

        evAmplitude +=max;
        hmap[signal->GetID()] = max;

          if (updatePlots){ 
            auto gr = signal->GetGraph(color);
            pulsesGraph.emplace_back((TGraph*)(gr->Clone()) );
          }
        color++;
    }

    if (!hmap.empty() && evAmplitude > 0) FillHitmap(hmap);

    if(evAmplitude > 0)spectrum->Fill(evAmplitude);

    if (updatePlots) {
        if (meanRateGraph->GetN() > 0 && instantRateGraph->GetN() > 0) {
            fECanvas->GetCanvas()->cd(1);
            instantRateGraph->Draw("ALP");
            meanRateGraph->Draw("LP");
        }

        fECanvas->GetCanvas()->cd(2);
        spectrum->Draw();

        fECanvas->GetCanvas()->cd(3);
        for (const auto& gr : pulsesGraph) {
            gr->Draw("SAME");
        }

        #ifdef REST_DetectorLib
          if(fReadout){
            fECanvas->GetCanvas()->cd(4);
            hitmap->Draw("COLZ0");
          }
        #endif

        fECanvas->GetCanvas()->Update();
        oldTimeUpdate = tNow;
    }
}

void TRestDAQGUI::FillHitmap(const std::map<int, int>& hmap) {
    #ifdef REST_DetectorLib
    if(!fReadout)return;

    double posX = 0, posY = 0;
    int ampX = 0, ampY = 0;
    Int_t readoutChannel = -1, readoutModule, planeID=0;
    TRestDetectorReadoutPlane* plane = &(*fReadout)[0];
    for (const auto& [sID, ampl] : hmap) {
      fReadout->GetPlaneModuleChannel(sID, planeID, readoutModule, readoutChannel);
        if (readoutChannel==-1) continue;
        Double_t x = plane->GetX(readoutModule, readoutChannel);
        Double_t y = plane->GetY(readoutModule, readoutChannel);

        if ( TMath::IsNaN(y) ) {
            posX += ampl * x;
            ampX += ampl;
        } else if ( TMath::IsNaN(x) ) {
            posY += ampl * y;
            ampY += ampl;
        }
    }

    if (ampX == 0 || ampY == 0) return;

    posX /= ampX;
    posY /= ampY;
    hitmap->Fill(posX, posY);
    #endif
}
