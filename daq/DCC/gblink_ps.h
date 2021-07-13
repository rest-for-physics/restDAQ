/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        gblink_ps.h

 Description: This module provides the definition of the API for sending
receiving packets over a (Gigabit) link (empty under Linux).

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
  September 2007: created

*******************************************************************************/
#ifndef GBLINK_PS_H
#define GBLINK_PS_H

typedef struct _Gblink {
	int id;
} Gblink;

typedef struct _GblinkParam {
	int id;
} GblinkParam;

#endif
