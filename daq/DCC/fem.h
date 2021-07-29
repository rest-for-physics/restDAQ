/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        fem.h

 Description: Definition of the API for the Front-End Mezzanine card.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  March 2006: created

*******************************************************************************/
#ifndef FEM_H
#define FEM_H

#include "gblink.h"

// Determine if write requests should be acknowledged or not
#ifdef ACK_ALL_WR
#define ACK_POL ACT_RQ_ACK
#else
#define ACK_POL 0
#endif

// FEC per FEM
#define DEFAULT_FEC_PER_FEM 1
#define MAX_NB_OF_FEC_PER_FEM 6
#define MIN_FEC_INDEX 0
#define MAX_FEC_INDEX (MAX_NB_OF_FEC_PER_FEM - 1)

#define MODE_MC_FLAG 0x2

typedef struct _Fem {
    Gblink gbl;
    int fem_id;
    short tx_ix;
    short fec_per_fem;
} Fem;

// Functions to get a handle to a FEM card
int Fem_Open(Fem* fem, int id);
int Fem_Close(Fem* fem);

// Basic actions to read/write to FEM registers
int Fem_Action(Fem* fem, unsigned short adr, void* dat, unsigned short action);
int Fem_RegisterWrite(Fem* fem, unsigned short adr, void* dat, unsigned short msk, unsigned short sz);
int Fem_RegisterRead(Fem* fem, unsigned short adr, void* dat, unsigned short msk, unsigned short sz);

// Functions to program external pulser DAC
int Fem_PulserDacWrite(Fem* fem, unsigned short fec, unsigned short* val);
int Fem_PulserDacRead(Fem* fem, unsigned short fec, unsigned short* val);

// Functions to program registers of the front-end ASICs
int Fem_AsicWrite(Fem* fem, unsigned short fec, unsigned short asic, unsigned short adr, unsigned short* val);
int Fem_AsicRead(Fem* fem, unsigned short fec, unsigned short asic, unsigned short adr, unsigned short* val);

// Function to request and gather event data
int Fem_RequestData(Fem* fem, unsigned short mode, unsigned short compress, unsigned short fec, unsigned short asic, unsigned short channel,
                    unsigned short cell, unsigned short target);
int Fem_ReceiveData(Fem* fem, PacketRx* prx);

// Functions to map detector geometry
int Fem_GetChannelByCoordinates(unsigned short x, unsigned short y, unsigned short* fec, unsigned short* asic, unsigned short* channel);

unsigned short Fem_GetPedThrAddr(Fem* fem, unsigned short fec, unsigned short asic, unsigned short channel, unsigned short base);

#endif
