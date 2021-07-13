/*********************************************************************************
TRESTDAQ.cxx

Base class for different DAQs: T2K, FEMINOS or any other

Author: JuanAn Garcia 18/05/2021

*********************************************************************************/

#include "TRESTDAQ.h"


TRESTDAQ::TRESTDAQ( const std::string  &cfgFileName ) : m_cfgFileName( cfgFileName ) {

readConfig();
initialize();
}

TRESTDAQ::~TRESTDAQ () {
//Cleanup if any
}

void TRESTDAQ::readConfig() {



}





