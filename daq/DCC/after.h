/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        after.h

 Description: Definition of constants relative to the ASIC AFTER and the FEC.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
  March 2007: created
  June 2008: extended addressable register range to 6
*******************************************************************************/
#ifndef AFTER_H
#define AFTER_H


#define ASIC_REGISTER_MIN_ADDRESS 1
#define ASIC_REGISTER_MAX_ADDRESS 6
#define ASIC_MIN_TIME_BIN_INDEX   0
#define ASIC_MAX_TIME_BIN_INDEX   511
#define ASIC_MIN_CHAN_INDEX       0
#define ASIC_MAX_CHAN_INDEX       78

#define MAX_NB_OF_ASIC_PER_FEC    4
#define MIN_ASIC_INDEX            0
#define MAX_ASIC_INDEX            (MIN_ASIC_INDEX+MAX_NB_OF_ASIC_PER_FEC-1)
#define ADC_FRAME_CHANNEL_INDEX   (MAX_ASIC_INDEX+1) 
#endif
