/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        err_codes.h

 Description: Definition of various error codes.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  March 2006: created

*******************************************************************************/
#ifndef ERR_CODES_H
#define ERR_CODES_H

#define ERR_LINK_DCM_UNLOCKED -1
#define ERR_LINK_RX_INVALID -2
#define ERR_LINK_RX_ERROR_COUNT_NON_NULL -3
#define ERR_LINK_RX_TIMEOUT -4
#define ERR_RX_PACKET_OVERSIZE -5
#define ERR_TX_FAILED -6

#define ERR_ACTION_RESP_BAD_TYPE -7
#define ERR_ACTION_RESP_NOT_OK -8
#define ERR_ACTION_RESP_INDEX_MISMATCH -9
#define ERR_ACTION_RESP_ADR_MISMATCH -10
#define ERR_ACTION_WR_LB_FAILED -11
#define ERR_ACTION_WR_UB_FAILED -12
#define ERR_ACTION_RD_BB_FAILED -13
#define ERR_ACTION_RD_LB_FAILED -14
#define ERR_ACTION_RD_UB_FAILED -15
#define ERR_ACTION_ILLEGAL -16

#define ERR_ILLEGAL_PARAMETER -17
#define ERR_NULL_POINTER -18
#define ERR_DEVICE_DOES_NOT_EXIST -19

#define ERR_SYNTAX -20
#define ERR_VERIFY_MISMATCH -21

#define ERR_ILLEGAL_FEC_COUNT -30

#define ERR_UNKNOWN_COMMAND -50

#define ERR_NO_DATA -60
#define ERR_DATA_READ_INCOMPLETE -61

#define ERR_SCA_NOT_STARTED -100
#define ERR_SCA_STILL_WRITING -101
#define ERR_SCA_STILL_DIGITIZING -102

#define ERR_DCC_FEM_TS_MISMATCH -103

#include "bufpool_err.h"

#endif
