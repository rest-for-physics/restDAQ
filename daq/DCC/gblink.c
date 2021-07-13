/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        gblink.c

 Description: This module provides part of the implementation of the API for
 sending receiving packets over a (Gigabit) link. This file contains only the
 functions that are platform independant. Refer to <arch>/gblink_ps.c for the
 platform specific part.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
  May 2006: created
  
  March 2007: added calls to network byte order conversions macros to run
  transparently on big endian architecture (e.g. PowerPC 405) and big endian
  hosts (e.g. Pentium PC)
  
  December 2007: corrected number of iterations in PacketRx_Print; the number
  of data words is decreased by one because pck->size includes the size of
  the "size" field (i.e. 2 bytes).
  November 2008: changed code to reflect data format change: 16-bit words can
  indicate either: the index of a SCA cell, a 12-bit ADC value, read-back
  arguments or partial word count.
  
  December 2008: corrected decoding error: the last two 16-bit words of a
  packet are the CRC32 of the packet received on the FEM-DCC link. It has never
  been used in the upper levels of software, but we try to keep it for
  compatibility with existing software. These last 2 words may be re-assigned
  to some other use.
  
  December 2008: changed format to DataPacket to support a header made by the
  DCC and placed in front of the data packet received from the FEM. Previous
  format is still supported for backward compatibility. The new format allows
  to identify from which FEM and DCC the packet comes from.
  The size of the new header also fixes the problem of 32-bit word mis-
  alignment that occurs in the old format.
  
  October 2009: added function to print DCC busy histogram
  December 2009: added function Arg12ToFecAsicChannel
   Note that functions to print packets only decode FEC, ASIC and Channel
   numbers proprerly for the 6 FEC version of the FEM. This limitation is 
   due to the fact that the FEC count of the FEM being readout must be
   known to decode these field properly and this argument is not passed to
   packet printout functions.

  December 2009: moved all the code not related to gblink (link between the 
  FEM and the DCC) to eth_packet.c (link between the DCC and the client PC)
  
*******************************************************************************/
#include "gblink.h"

#include <stdio.h>

/*******************************************************************************
 PacketRx_Print 
*******************************************************************************/
void PacketRx_Print(PacketRx *pck)
{
	unsigned int i;

	for (i=0;i<((ntohs(pck->size)/(sizeof (short)))-1);i++)
	{
		xil_printf("Data(%d)=0x%x\r\n", i, ntohs(pck->data[i]));	
	}
	xil_printf("Size:%d bytes\r\n\r\n", ntohs(pck->size));
}
