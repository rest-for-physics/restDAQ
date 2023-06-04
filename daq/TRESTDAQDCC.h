/*********************************************************************************
TRESTDAQT2K.h

Implement data acquisition for DCC + FEM + FEC based readout

Author: JuanAn Garcia 18/05/2021

Based on mclient program from Denis Calvet

*********************************************************************************/

#ifndef __TREST_DAQ_DCC__
#define __TREST_DAQ_DCC__

#include "TRESTDAQ.h"
#include "DCCPacket.h"
#include "TRESTDAQSocket.h"

class TRESTDAQDCC : public TRESTDAQ {
   public:
    TRESTDAQDCC(TRestRun* rR, TRestRawDAQMetadata* dM);

    void configure() override;
    void startDAQ() override;
    void stopDAQ() override;
    void initialize() override;

   private:
    void pedestal();
    void dataTaking();
    DCCPacket::packetReply SendCommand(const char* cmd, DCCPacket::packetType type = DCCPacket::packetType::ASCII, size_t nPackets = 0, DCCPacket::packetDataType dataType = DCCPacket::packetDataType::NONE);

    void waitForTrigger();
    void saveEvent(unsigned char* buf, int size);
    void savePedestals(unsigned char* buf, int size);

    uint16_t FECMask = 0;

    // Socket
    TRESTDAQSocket dcc_socket;
};

#endif
