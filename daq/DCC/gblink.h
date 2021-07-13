/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        gblink.h

 Description: This module provides the definition of the API for sending
 receiving packets over a (Gigabit) link.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
  March 2006: created
  November 2009: addded new types of messages for histograms
  
*******************************************************************************/
#ifndef GBLINK_H
#define GBLINK_H

#include "platform_spec.h"
#include "gblink_ps.h"

#define MAX_PACKETRX_DATA_SIZE 4096


typedef struct _PacketTx {
	unsigned short dst;
	unsigned short cmd;
	unsigned short adr;
	unsigned short dat;
} PacketTx;

typedef struct _PacketRx {
	unsigned short size;
	unsigned short placeholder[1]; // must match that of DataPacket and must guarantee 32-bit alignment;
	unsigned short data[MAX_PACKETRX_DATA_SIZE/(sizeof(unsigned short))];
} PacketRx;

// Header structure common to old and new structure of DataPacket
typedef struct _PacketRxCommonHdr {
	unsigned short size;
	unsigned short hdr;
} PacketRxCommonHdr;

// Macros to put header information in request packet
#define PACKTX_MAX_IX     0x0F
#define ACT_ILL           0x00 
#define ACT_WR_LB         0x10
#define ACT_WR_UB         0x20
#define ACT_WR_BB         0x30
#define ACT_RQ_ACK        0x40
#define ACT_SET_REQ       0x80
#define ACT_MASK          0xF0 
#define PACKTX_PUT_ACT(act,ix) ( ((act) & ACT_MASK)| ((ix) & PACKTX_MAX_IX) )
// Supplementary actions not written in header
#define ACT_RD_LB         0x100
#define ACT_RD_UB         0x200
#define ACT_RD_BB         0x300

// Response Packet Header Decode Macros
#define GET_REQ_INDEX(word)   ((word) & 0x000F)
#define GET_WR_ACK(word)      ((word) & 0x0030)
#define GET_RD_ACK(word)      ((word) & 0x0040)
#define GET_TR_OK(word)       ((word) & 0x0080)
#define GET_TYPE(word)        ((word) & 0xF000)
#define PUT_TYPE(word, ty)    (((word) & 0x0FFF) | ((ty) & 0xF000) )

// Response Packet Types from FEM (see t2k_pkg.vhd for coherence)
#define RESP_TYPE_ADC_DATA    0x0000
#define RESP_TYPE_SLOW_CTRL   0x1000
// These types are not among those returned by the FEM but by the DCC
#define RESP_TYPE_HISTOGRAM    0x2000
#define RESP_TYPE_HISTOSTAT    0x3000
#define RESP_TYPE_HISTOSUMMARY 0x4000
#define GET_RESP_TYPE(word)   (((word) & 0xF000)>>12)
#define GET_RESP_INDEX(word)  ((word) & 0x000F)

// Macros to interpret DataPacket header
#define GET_FEC_ERROR(word)  (((word) & 0x03F0)>>4)
#define GET_LOS_FLAG(word)   (((word) & 0x0400)>>10)
#define GET_SYNCH_FAIL(word) (((word) & 0x0800)>>11)

// Macros to interpret DataPacket read back arguments
#define GET_RB_MODE(word)     (((word) & 0x4000)>>14)
#define GET_RB_COMPRESS(word) (((word) & 0x2000)>>13)
#define GET_RB_ARG2(word)     (((word) & 0x1E00)>>9)
#define GET_RB_ARG1(word)     (((word) & 0x01FF))

// Macros to interpret DataPacket event type / count
#define GET_EVENT_TYPE(word)  (((word) & 0xC000)>>14)
#define GET_EVENT_COUNT(word) (((word) & 0x3FFF))

// Macros to interpret DataPacket Samples
#define CELL_INDEX_FLAG        0x1000
#define GET_CELL_INDEX(word)   (((word) & 0x0FFF))
#define ARGUMENT_FLAG          0x2000
#define GET_ARGUMENTS(word)    (((word) & 0xDFFF))
#define SAMPLE_COUNT_FLAG      0x4000
#define GET_SAMPLE_COUNT(word) (((word) & 0x0FFF))

// Function prototypes
int Gblink_Open(Gblink *gbl, GblinkParam *gblp);
int Gblink_Close(Gblink *gbl);
int Gblink_Snd(Gblink *gbl, PacketTx *pck);
int Gblink_Rcv(Gblink *gbl, PacketRx *pck);
void Gblink_Stat_Print(Gblink *gbl);
void PacketRx_Print(PacketRx *pck);
int Gblink_RcvPendingCount(Gblink *gbl);
int Gblink_GetRxErrorStatus(Gblink *gbl, int verb, char *cmt);
int Gblink_Flush(Gblink *gbl);
int Gblink_ClearErrCnt(Gblink *gbl, int retry, int dur);

#endif
