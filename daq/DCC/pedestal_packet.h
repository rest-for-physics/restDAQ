/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        pedestal_packet.h

 Description: Definitions related to pedestal packets


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  November 2009: created

*******************************************************************************/
#ifndef _PEDESTAL_PACKET_H
#define _PEDESTAL_PACKET_H

#include "after.h"

#define PHISTO_SIZE 512

typedef struct _channel_stat {
    unsigned short mean;   // mean of channel pedestal
    unsigned short stdev;  // standard deviation of channel pedestal
} channel_stat;

// Histogram data packet for pedestals
typedef struct _PedestalHistoSummaryPacket {
    unsigned short size;                         // size in bytes including the size of the size field itself
    unsigned short dcchdr;                       // specific header added by the DCC
    unsigned short hdr;                          // specific header inherited from FEM
    unsigned short args;                         // read-back arguments as encoded by FEM
    unsigned short scnt;                         // number of unsigned short filled in following array
    channel_stat stat[ASIC_MAX_CHAN_INDEX + 1];  // summary of statistics for current ASIC
} PedestalHistoSummaryPacket;

// Histogram data packet for pedestals
typedef struct _PedestalHistoMathPacket {
    unsigned short size;     // size in bytes including the size of the size field itself
    unsigned short dcchdr;   // specific header added by the DCC
    unsigned short hdr;      // specific header put by the FEM
    unsigned short args;     // read-back arguments echoed by FEM
    unsigned short min_bin;  // range lower bound
    unsigned short max_bin;  // range higher bound
    unsigned short bin_wid;  // width of one bin in number of units
    unsigned short bin_cnt;  // number of bins
    unsigned short min_val;  // lowest bin with at least one entry
    unsigned short max_val;  // highest bin with at least one entry
    unsigned short mean;     // mean
    unsigned short stddev;   // standard deviation
    unsigned int entries;    // number of entries
    unsigned short bin_sat;  // number of bins saturated
    unsigned short align;    // alignment
} PedestalHistoMathPacket;

// New structure of DataPacket. Aliased to DataPacketV2
typedef struct _PedestalHistoBinPacket {
    unsigned short size;               // size in bytes including the size of the size field itself
    unsigned short dcchdr;             // specific header added by the DCC
    unsigned short hdr;                // specific header put by the FEM
    unsigned short args;               // read-back arguments echoed by FEM
    unsigned short ts_h;               // local FEM timestamp MSB
    unsigned short ts_l;               // local FEM timestamp LSB
    unsigned short ecnt;               // local FEM event type/count
    unsigned short scnt;               // sample count in first pulse over thr or total sample count in packet
    unsigned short samp[PHISTO_SIZE];  // data storage area
} PedestalHistoBinPacket;

void Pedestal_PrintHistoMathPacket(PedestalHistoMathPacket* phm);
void Pedestal_PrintHistoBinPacket(PedestalHistoBinPacket* pck);
void Pedestal_PrintHistoSummaryPacket(PedestalHistoSummaryPacket* pck);

#endif
