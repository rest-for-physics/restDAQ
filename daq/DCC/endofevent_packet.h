/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        endofevent_packet.h

 Description: Definitions related to end of event packet


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  December 2009: created

*******************************************************************************/
#ifndef _ENDOFEVENT_PACKET_H
#define _ENDOFEVENT_PACKET_H

#include "dcc.h"

// End of Event Packet
typedef struct _EndOfEventPacket {
    unsigned short size;                           // size in bytes including the size of the size field itself
    unsigned short dcchdr;                         // specific header added by the DCC
    unsigned short hdr;                            // specific header similar to what is put by the FEM
    unsigned short pad;                            // padding for 32-bit alignement
    unsigned int byte_rcv[MAX_NB_OF_FEM_PER_DCC];  // data received per FEM
    unsigned int byte_snd[MAX_NB_OF_FEM_PER_DCC];  // data sent per FEM
    unsigned int tot_byte_rcv;
    unsigned int tot_byte_snd;
} EndOfEventPacket;

void EndOfEvent_PrintPacket(EndOfEventPacket* eop);

#endif
