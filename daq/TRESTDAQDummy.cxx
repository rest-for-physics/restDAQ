/*********************************************************************************
TRESTDAQT2K.cxx

Implement data acquisition for DCC + FEM + FEC based readout

Author: JuanAn Garcia 18/05/2021

*********************************************************************************/

#include "TRESTDAQDummy.h"

const std::vector<double> pulseSample = {0,       0,       83,      115.333, 136,     154.4,   171.167, 188,     206.125, 223.333, 242.4,   262.727,
                                         283.833, 317,     352.833, 391.417, 431.167, 472.167, 513.667, 553.5,   594,     631.917, 667.583, 700.333,
                                         728.917, 754.083, 775.083, 791.583, 803.417, 810.333, 813.667, 811.833, 806.083, 795.583, 780.75,  764.5,
                                         745.5,   724,     701.583, 677.667, 653.917, 629.417, 604.667, 579.75,  555.333, 531.583, 507.167, 491.091,
                                         474.7,   457,     438.875, 418,     394.833, 368.2,   335,     288.333, 209.5,   0,       0,       0};

TRESTDAQDummy::TRESTDAQDummy(TRestRun* rR, TRestRawDAQMetadata* dM) : TRESTDAQ(rR, dM) { initialize(); }

void TRESTDAQDummy::initialize() {}

void TRESTDAQDummy::configure() { std::cout << "Configuring readout" << std::endl; }

void TRESTDAQDummy::startDAQ(bool configure) {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    while ( !abrt && !nextFile && (daqMetadata->GetNEvents() == 0 || event_cnt < daqMetadata->GetNEvents() ) ) {
        fSignalEvent.Initialize();
        fSignalEvent.SetID(event_cnt);
        fSignalEvent.SetTime(getCurrentTime());
        int physChannel = rand() * 140. / RAND_MAX;
        for (int s = 0; s < 10; s++) {
            std::vector<Short_t> sData(512, 250);
            double factor = rand() * 3. / RAND_MAX;
            for (size_t i = 0; i < pulseSample.size(); i++) {
                int timeBin = i + 200;
                sData[timeBin] += pulseSample[i] * factor;
            }
            // std::cout<<physChannel<<std::endl;
            if (s == 5) physChannel = rand() * 140. / RAND_MAX + 144;
            TRestRawSignal rawSignal(physChannel % 288, sData);
            fSignalEvent.AddSignal(rawSignal);
            physChannel++;
        }

        FillTree(restRun, &fSignalEvent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void TRESTDAQDummy::stopDAQ() { std::cout << "Run stopped" << std::endl; }
