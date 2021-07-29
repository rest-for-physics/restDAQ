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
    TRESTDAQDummy(TRestRun *rR, TRestRawDAQMetadata *dM);
   
    virtual void configure();
    virtual void startDAQ();
    virtual void stopDAQ();
    virtual void initialize();
    
};

#endif

