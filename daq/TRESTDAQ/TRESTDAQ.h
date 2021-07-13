/*********************************************************************************
TRESTDAQ.h

Base class for different DAQs: T2K, FEMINOS or any other

Author: JuanAn Garcia 18/05/2021

To implement generic methods here

*********************************************************************************/

#ifndef __TREST_DAQ__
#define __TREST_DAQ__

#include <iostream>
#include <string>

class TRESTDAQ {
   public:
    TRESTDAQ(const std::string& cfgFileName);
    ~TRESTDAQ();

    // Pure virtual methods to start, stop and configure the DAQ
    virtual void configure() = 0;
    virtual void startDAQ() = 0;
    virtual void stopDAQ() = 0;
    virtual void initialize() = 0;

    void readConfig();

    const std::string getCfgFile() { return m_cfgFileName; }
    void setCfgFile(const std::string& cfgFileName) { m_cfgFileName = cfgFileName; }

    bool abrt = false;

   private:
    std::string m_cfgFileName;
};

#endif

