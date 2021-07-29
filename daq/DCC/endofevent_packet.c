/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        endofevent_packet.c

 Description: Implementation of functions related to end of event packet


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  December 2009: created

*******************************************************************************/
#include "endofevent_packet.h"

#include <stdio.h>

#include "ethpacket.h"
#include "platform_spec.h"

/*******************************************************************************
 EndOfEvent_PrintPacket
*******************************************************************************/
void EndOfEvent_PrintPacket(EndOfEventPacket* eop) {
    int i;

    xil_printf("------------------------------------\r\n");
    xil_printf("Message size : %d bytes\r\n", ntohs(eop->size));
    xil_printf("DCC Header: Type:0x%x DCC_index:0x%x FEM_index:0x%x\r\n", GET_FRAME_TY_V2(ntohs(eop->dcchdr)), GET_DCC_INDEX(ntohs(eop->dcchdr)),
               GET_FEM_INDEX(ntohs(eop->dcchdr)));
    xil_printf("FEM Header: 0x%x\r\n", ntohs(eop->hdr));
    for (i = 0; i < MAX_NB_OF_FEM_PER_DCC; i++) {
        xil_printf("Fem(%2d)   : recv: %d bytes sent: %d bytes\r\n", i, ntohl(eop->byte_rcv[i]), ntohl(eop->byte_snd[i]));
    }
    xil_printf("Total     : recv: %d bytes sent: %d bytes\r\n", ntohl(eop->tot_byte_rcv), ntohl(eop->tot_byte_snd));
}
