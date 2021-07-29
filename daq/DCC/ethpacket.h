/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        ethpacket.h

 Description: This module gives basic definitions to interpret the content of
 the messages sent by the DCC to the client PC over an Ethernet link.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  December 2009: created from content extracted in gblink.h

*******************************************************************************/
#ifndef ETHPACKET_H
#define ETHPACKET_H

#define MAX_ETH_PACKET_DATA_SIZE 4096

// New structure of DataPacket. Aliased to DataPacketV2
typedef struct _DataPacket {
	unsigned short size;     // size in bytes including the size of the size field itself
	unsigned short dcchdr;   // specific header added by the DCC
	unsigned short hdr;      // specific header put by the FEM
	unsigned short args;     // read-back arguments echoed by FEM
	unsigned short ts_h;     // local FEM timestamp MSB
	unsigned short ts_l;     // local FEM timestamp LSB
	unsigned short ecnt;     // local FEM event type/count
	unsigned short scnt;     // sample count in first pulse over thr or total sample count in packet
	unsigned short samp[MAX_ETH_PACKET_DATA_SIZE/(sizeof(unsigned short))]; // data storage area
} DataPacket;

// Macros to manipulate DataPacket DCC header
#define FRAME_TYPE_FEM_DATA   0x0000
#define FRAME_TYPE_DCC_DATA   0x0001
#define FRAME_FLAG_EORQ       0x0004
#define FRAME_FLAG_EOEV       0x0008
#define GET_FEM_INDEX(word)      (((word) & 0x000F)>>0)
#define GET_DCC_INDEX(word)      (((word) & 0x03F0)>>4)
#define GET_FRAME_TY_V2(word)    (((word) & 0x3C00)>>10)
#define PUT_FEM_INDEX(word, ix)  (((word) & 0xFFF0) | (((ix) & 0x000F) << 0))
#define PUT_DCC_INDEX(word, ix)  (((word) & 0xFC0F) | (((ix) & 0x003F) << 4))
#define PUT_FRAME_TY_V2(word, ty) (((word) & 0x03FF) | FRAME_HDR_V2 | (((ty) & 0x000F) <<10))

// We now support the old and new version of the data packet structure
#define FRAME_HDR_V2_FIELD 0xC000
#define FRAME_HDR_V2       0x4000
#define IS_DATA_PACKET_V2(word)     (((word) & FRAME_HDR_V2_FIELD) == FRAME_HDR_V2)

void DataPacket_Print( DataPacket *pck);

#endif
