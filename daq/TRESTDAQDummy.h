/*********************************************************************************
TRESTDAQT2K.h

Implement data acquisition for DCC + FEM + FEC based readout

Author: JuanAn Garcia 18/05/2021

*********************************************************************************/

#ifndef __TREST_DAQ_DUMMY__
#define __TREST_DAQ_DUMMY__

#include "TRESTDAQ.h"

class TRESTDAQDummy : public TRESTDAQ {
   public:
    TRESTDAQDummy(TRestRun* rR, TRestRawDAQMetadata* dM);

    void configure() override;
    void startDAQ(bool configure=true) override;
    void stopDAQ() override;
    void initialize() override;
};

#endif
