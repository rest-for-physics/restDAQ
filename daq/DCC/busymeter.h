/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        busymeter.h

 Description: Definition of functions to measure performance of DCC


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  October 2009: created

*******************************************************************************/
#ifndef _BUSYMETER_H
#define _BUSYMETER_H

#include "histo.h"

#define BHISTO_SIZE 256

typedef struct _bhisto {
    histo busy_histo;
    unsigned short busy_bins[BHISTO_SIZE];
} bhisto;

// Histogram data packet
typedef struct _HistogramPacket {
    unsigned short size;               // size in bytes including the size of the size field itself
    unsigned short dcchdr;             // specific header added by the DCC
    unsigned short hdr;                // placeholder for header normally put by FEM
    unsigned short min_bin;            // range lower bound
    unsigned short max_bin;            // range higher bound
    unsigned short bin_wid;            // width of one bin in number of units
    unsigned short bin_cnt;            // number of bins
    unsigned short min_val;            // lowest bin with at least one entry
    unsigned short max_val;            // highest bin with at least one entry
    unsigned short mean;               // mean
    unsigned short stddev;             // standard deviation
    unsigned short pad;                // padding for 32-bit alignment
    unsigned int entries;              // number of entries
    unsigned short samp[BHISTO_SIZE];  // data storage area
} HistogramPacket;

extern bhisto busy_histogram;

unsigned char Busymeter_GetBusyDuration();
void Busymeter_HistoPacketize(bhisto* bh, HistogramPacket* hp);

#endif
