/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        dcc.h

 Description: Definitions related to the Data Concentrator Card (DCC).

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  March 2007: created

*******************************************************************************/
#ifndef DCC_H
#define DCC_H

// The number of FEM per DCC is passed by the preprocessor
// but a default value is set if not provided
#ifndef FEM_PER_DCC
#define FEM_PER_DCC 1
#endif
#define MAX_NB_OF_FEM_PER_DCC 12
#define MIN_FEM_INDEX 0
#define MAX_FEM_INDEX ((FEM_PER_DCC < MAX_NB_OF_FEM_PER_DCC) ? (MIN_FEM_INDEX + FEM_PER_DCC - 1) : (MAX_NB_OF_FEM_PER_DCC - 1))

#endif
