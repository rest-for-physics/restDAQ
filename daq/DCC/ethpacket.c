/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        ethpacket.c

 Description: This module gives basic functions to interpret the content of
 the messages sent by the DCC to the client PC over an Ethernet link.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
  December 2009: created from content extracted in gblink.c
  July 2010: corrected bug due to incorrect test order that caused a Cell
  index to be printed instead of a read-back argument in some cases

  March 2011: added external variable Fec_Per_Fem to pass the correct
  argument to Arg12ToFecAsicChannel()
  
*******************************************************************************/
#include "gblink.h"
#include "err_codes.h"
#include "busymeter.h"
#include "pedestal_packet.h"
#include "endofevent_packet.h"
#include "ethpacket.h"

#include <stdio.h>

extern unsigned short Fec_Per_Fem;

/*******************************************************************************
 Arg12ToFecAsicChannel 
*******************************************************************************/
int Arg12ToFecAsicChannel(
								unsigned short fec_per_fem,
								unsigned short arg1,
								unsigned short arg2,
								unsigned short *fec,
								unsigned short *asic,
								unsigned short *channel
								)
{
	if (fec_per_fem == 6)
	{
		*fec     = (10*(arg1%6)/2 + arg2)/4;
		*asic    = (10*(arg1%6)/2 + arg2)%4;
		*channel = arg1/6;
		if ((*fec > 5) || (*asic > 3))
		{
			*fec = arg2 - 4;
			*asic = 4;
			*channel = (arg1 - 4) / 6;
		}
	}
	else if (fec_per_fem == 1)
	{
		*fec     = 0;
		*asic    = (arg1%6) + arg2;
		*channel = arg1/6;
		
	}
	else if (fec_per_fem == 4)
	{
		*fec     = (6*(arg1%6)/2 + arg2)/4;
		*asic    = (6*(arg1%6)/2 + arg2)%4;
		*channel = arg1/6;
	}
	else
	{
		return(-1);
	}
	
	return (0);
}

/*******************************************************************************
 DCC_Histogram_Print 
*******************************************************************************/
void DCC_Histogram_Print(HistogramPacket *pck)
{
	unsigned short i;
	unsigned short min_val;
	unsigned short max_val;
	unsigned short min_bin;
	unsigned short max_bin;
	unsigned short bin_cnt;
	unsigned short bin_wid;
	unsigned short samp;

	min_val = ntohs(pck->min_val);
	max_val = ntohs(pck->max_val);
	min_bin = ntohs(pck->min_bin);
	max_bin = ntohs(pck->max_bin);
	bin_wid = ntohs(pck->bin_wid);
	bin_cnt = ntohs(pck->bin_cnt);

	xil_printf("------------------------------------\n");
	xil_printf("DCC Header: Type:0x%x DCC_index:0x%x\n",
		GET_FRAME_TY_V2(ntohs(pck->dcchdr)),
		GET_DCC_INDEX(ntohs(pck->dcchdr)));
	xil_printf("Bin min   : %d ms\n", min_bin);
	xil_printf("Bin max   : %d ms\n", max_bin);
	xil_printf("Bin width : %d ms\n", bin_wid);
	xil_printf("Bin count : %d\n", bin_cnt);
	xil_printf("Min val   : %d ms\n", min_val);
	xil_printf("Max val   : %d ms\n", max_val);
	xil_printf("Mean      : %.2f ms\n", ntohs(pck->mean) / 100.0);
	xil_printf("StdDev    : %.2f ms\n", ntohs(pck->stddev) / 100.0);
	xil_printf("Entries   : %d\n",    ntohl(pck->entries));
	for (i=0; i<bin_cnt; i++)
	{
		samp = ntohs(pck->samp[i]);
		xil_printf("Bin(%3d) = %4d\n", i, samp);
	}
	xil_printf("------------------------------------\n");
}

/*******************************************************************************
 DCC_Data_Print 
*******************************************************************************/
void DCC_Data_Print(EndOfEventPacket *pck)
{
	// DCC end of event packet
	if (GET_FRAME_TY_V2(ntohs(pck->dcchdr)) & FRAME_FLAG_EOEV)
	{
		EndOfEvent_PrintPacket(pck);
		return;
	}
	// DCC histogram packet
	else if (GET_TYPE(ntohs(pck->hdr)) == RESP_TYPE_HISTOGRAM)
	{
		DCC_Histogram_Print((HistogramPacket *) pck);
		return;
	}

	
	else
	{
		xil_printf("DCC_Data_Print: unknown frame type 0x%x\n",
			GET_FRAME_TY_V2(ntohs(pck->dcchdr)));
	}
}

/*******************************************************************************
 FemAdcDataPrint 
*******************************************************************************/
void FemAdcDataPrint(DataPacket *pck)
{
	unsigned short i;
	unsigned int ts;
	unsigned short scnt;
	unsigned short samp;
	short nbsw;
	unsigned int   tmp;
	unsigned short fec, asic, channel;
	
	xil_printf("------------------------------------\n");
	xil_printf("Packet sz : %d bytes\n", (ntohs(pck->size)));
	xil_printf("DCC Header: Type:0x%x DCC_index:0x%x FEM_index:0x%x\n",
		GET_FRAME_TY_V2(ntohs(pck->dcchdr)),
		GET_DCC_INDEX(ntohs(pck->dcchdr)),
		GET_FEM_INDEX(ntohs(pck->dcchdr)));

	xil_printf("FEM Header: Msg_type:0x%x Index:0x%x\n",
		GET_RESP_TYPE(ntohs(pck->hdr)),
		GET_RESP_INDEX(ntohs(pck->hdr)));
	xil_printf("Errors    : FEC:0x%x Unable to SYNCH:%d Framing LOS:%d\n",
		GET_FEC_ERROR(ntohs(pck->hdr)),
		GET_SYNCH_FAIL(ntohs(pck->hdr)),
		GET_LOS_FLAG(ntohs(pck->hdr)));
	xil_printf("Read-back : Mode:%d Compress:%d Arg1:0x%x Arg2:0x%x\n",
		GET_RB_MODE(ntohs(pck->args)),
		GET_RB_COMPRESS(ntohs(pck->args)),
		GET_RB_ARG1(ntohs(pck->args)),
		GET_RB_ARG2(ntohs(pck->args)));
	
	Arg12ToFecAsicChannel(Fec_Per_Fem, (unsigned short) GET_RB_ARG1(ntohs(pck->args)), (unsigned short) GET_RB_ARG2(ntohs(pck->args)), &fec, &asic, &channel);
	xil_printf("            Fec:%d Asic:%d Channel:%d\n", fec, asic, channel);

	ts = (((int)ntohs((pck->ts_h)))<<16) | (int) ntohs((pck->ts_l));
	xil_printf("Event     : Type:%d Count:%d Time:0x%x NbOfWords:%d\n",
		GET_EVENT_TYPE(ntohs(pck->ecnt)),
		GET_EVENT_COUNT(ntohs(pck->ecnt)),
		ts,
		ntohs(pck->scnt));
	scnt = ntohs(pck->scnt) & 0x7FF;

	// Compute the number of short words in the packet
	// Must subtract:
	// .  2 bytes for DCC header,
	// . 12 bytes for FEM header,
	// .  2 bytes used for storing the size field itself,
	// .  4 bytes used in the trailer for CRC32 or debug info 
	nbsw = (ntohs(pck->size) - 2 - 12 - 2 - 4) / sizeof(short);

	for (i=0;i<nbsw;i++)
	{
		samp = ntohs(pck->samp[i]);
		if (samp & ARGUMENT_FLAG)
		{
			xil_printf("ArgRb (%3d)=0x%3x", i, GET_ARGUMENTS(samp));
			Arg12ToFecAsicChannel(Fec_Per_Fem, (unsigned short) GET_RB_ARG1(samp), (unsigned short) GET_RB_ARG2(samp), &fec, &asic, &channel);
			xil_printf(" (F:%d A:%d C:%d)\n", fec, asic, channel);
		}
		else if (samp & SAMPLE_COUNT_FLAG)
		{
			xil_printf("SamCnt(%3d)=0x%3x (%4d)\n", i, GET_SAMPLE_COUNT(samp), GET_SAMPLE_COUNT(samp));
		}
		else if (samp & CELL_INDEX_FLAG)
		{
			xil_printf("Cell  (%3d)=0x%3x (%4d)\n", i, GET_CELL_INDEX(samp), GET_CELL_INDEX(samp));
		}
		else
		{
			xil_printf("Sample(%3d)=0x%3x (%4d)\n", i, samp, samp);
		}
	}
	tmp = (((unsigned int) ntohs(pck->samp[i])) << 16) | ((unsigned int) ntohs(pck->samp[i+1]));
	xil_printf("Trailer    =0x%08x (%d)\n", tmp, tmp);
	xil_printf("------------------------------------\n");
}

/*******************************************************************************
 DataPacketV2_Print 
*******************************************************************************/
void DataPacket_Print( DataPacket *pck)
{
	// DCC data packet has a different structure
	if (GET_FRAME_TY_V2(ntohs(pck->dcchdr)) & FRAME_TYPE_DCC_DATA)
	{
		DCC_Data_Print((EndOfEventPacket*) pck);
		return;
	}
	
	// FEM data - Pedestal Histogram Mathematics
	else if (GET_TYPE(ntohs(pck->hdr)) == RESP_TYPE_HISTOSTAT)
	{
		Pedestal_PrintHistoMathPacket((PedestalHistoMathPacket *)pck);
		return;
	}

	// FEM data - Pedestal Histogram Bins
	else if (GET_TYPE(ntohs(pck->hdr)) == RESP_TYPE_HISTOGRAM)
	{
		Pedestal_PrintHistoBinPacket((PedestalHistoBinPacket *) pck);
		return;
	}
	
	// FEM data - Pedestal Histogram Summary
	else if (GET_TYPE(ntohs(pck->hdr)) == RESP_TYPE_HISTOSUMMARY)
	{
		Pedestal_PrintHistoSummaryPacket((PedestalHistoSummaryPacket *)pck);
		return;
	}
	
	// FEM data - ADC samples in zero-suppressed or non zero-suppressed mode
	else if (GET_TYPE(ntohs(pck->hdr)) == RESP_TYPE_ADC_DATA)
	{
		FemAdcDataPrint(pck);
		return;
	}
}



