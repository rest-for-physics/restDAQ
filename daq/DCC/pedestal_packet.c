/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        pedestal_packet.c

 Description: Implementation of functions related to pedestal packets


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  November 2009: created

*******************************************************************************/
#include "pedestal_packet.h"

#include <stdio.h>

#include "ethpacket.h"
#include "gblink.h"
#include "platform_spec.h"

/*******************************************************************************
 Pedestal_PrintHistoMathPacket
*******************************************************************************/
void Pedestal_PrintHistoMathPacket(PedestalHistoMathPacket* phm) {
    xil_printf("------------------------------------\n");
    xil_printf("Message size : %d bytes\n", ntohs(phm->size));
    xil_printf("DCC Header: Type:0x%x DCC_index:0x%x FEM_index:0x%x\n", GET_FRAME_TY_V2(ntohs(phm->dcchdr)), GET_DCC_INDEX(ntohs(phm->dcchdr)),
               GET_FEM_INDEX(ntohs(phm->dcchdr)));
    xil_printf("FEM Header: 0x%x\n", ntohs(phm->hdr)),
        xil_printf("Read-back : Mode:%d Compress:%d Arg1:0x%x Arg2:0x%x\n", GET_RB_MODE(ntohs(phm->args)), GET_RB_COMPRESS(ntohs(phm->args)),
                   GET_RB_ARG1(ntohs(phm->args)), GET_RB_ARG2(ntohs(phm->args)));

    xil_printf("Bin min   : %d\n", ntohs(phm->min_bin));
    xil_printf("Bin max   : %d\n", ntohs(phm->max_bin));
    xil_printf("Bin width : %d\n", ntohs(phm->bin_wid));
    xil_printf("Bin count : %d\n", ntohs(phm->bin_cnt));
    xil_printf("Min val   : %d\n", ntohs(phm->min_val));
    xil_printf("Max val   : %d\n", ntohs(phm->max_val));
    xil_printf("Mean      : %.2f\n", ((double)(ntohs(phm->mean))) / 100.0);
    xil_printf("StdDev    : %.2f\n", ((double)(ntohs(phm->stddev))) / 100.0);
    xil_printf("Entries   : %d\n", ntohl(phm->entries));
    xil_printf("Bin satur.: %d\n", ntohs(phm->bin_sat));
    xil_printf("Align     : %d\n", ntohs(phm->align));
    xil_printf("------------------------------------\n");
}

/*******************************************************************************
 Pedestal_PrintHistoBinPacket
*******************************************************************************/
void Pedestal_PrintHistoBinPacket(PedestalHistoBinPacket* pck) {
    unsigned short i;
    unsigned short nbsw;
    unsigned short samp;

    xil_printf("------------------------------------\n");
    xil_printf("Message size : %d bytes\n", ntohs(pck->size));
    xil_printf("DCC Header   : Type:0x%x DCC_index:0x%x FEM_index:0x%x\n", GET_FRAME_TY_V2(ntohs(pck->dcchdr)), GET_DCC_INDEX(ntohs(pck->dcchdr)),
               GET_FEM_INDEX(ntohs(pck->dcchdr)));

    xil_printf("FEM Header   : Msg_type:0x%x Index:0x%x\n", GET_RESP_TYPE(ntohs(pck->hdr)));
    xil_printf("Read-back    : Arg1:0x%x Arg2:0x%x\n", GET_RB_ARG1(ntohs(pck->args)), GET_RB_ARG2(ntohs(pck->args)));
    xil_printf("NbOfWords    : %d\n", ntohs(pck->scnt));

    // Compute the number of short words in the packet
    // Must subtract:
    // .  2 bytes for DCC header,
    // . 12 bytes for FEM header,
    // .  2 bytes used for storing the size field itself
    nbsw = (ntohs(pck->size) - 2 - 12 - 2) / sizeof(short);

    for (i = 0; i < nbsw; i++) {
        samp = ntohs(pck->samp[i]);
        xil_printf("Bin(%3d)= %5d\n", i, samp);
    }
    xil_printf("------------------------------------\n");
}

/*******************************************************************************
 Pedestal_PrintHistoSummaryPacket
*******************************************************************************/
void Pedestal_PrintHistoSummaryPacket(PedestalHistoSummaryPacket* pck) {
    unsigned short i;
    unsigned short nbsw;

    xil_printf("------------------------------------\n");
    xil_printf("Message size : %d bytes\n", ntohs(pck->size));
    xil_printf("DCC Header   : Type:0x%x DCC_index:0x%x FEM_index:0x%x\n", GET_FRAME_TY_V2(ntohs(pck->dcchdr)), GET_DCC_INDEX(ntohs(pck->dcchdr)),
               GET_FEM_INDEX(ntohs(pck->dcchdr)));

    xil_printf("FEM Header   : Msg_type:0x%x Index:0x%x\n", GET_RESP_TYPE(ntohs(pck->hdr)));
    xil_printf("Read-back    : Arg1:0x%x Arg2:0x%x\n", GET_RB_ARG1(ntohs(pck->args)), GET_RB_ARG2(ntohs(pck->args)));
    xil_printf("NbOfWords    : %d\n", ntohs(pck->scnt));

    // Compute the number of short words in the packet
    // Must subtract:
    // .  2 bytes for DCC header,
    // .  6 bytes for other header,
    // .  2 bytes used for storing the size field itself
    nbsw = (ntohs(pck->size) - 2 - 6 - 2) / sizeof(short);

    for (i = 0; i < (nbsw / 2); i++) {
        xil_printf("Stat   (%3d) : mean = %.2f  stdev = %.2f\n", i, ((double)(ntohs(pck->stat[i].mean))) / 100.0,
                   ((double)(ntohs(pck->stat[i].stdev))) / 100.0);
    }
    xil_printf("------------------------------------\n");
}
