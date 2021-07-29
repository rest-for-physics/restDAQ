/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        platform_spec.h

 Description: Specific include file and definitions for Linux PC

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  March 2006: created

*******************************************************************************/
#ifndef PLATFORM_SPEC_H
#define PLATFORM_SPEC_H

#include <errno.h>
#include <netinet/in.h>
#include <sched.h>
#include <string.h>
#include <stropts.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

extern int errno;

#define yield() sched_yield()
#define xil_printf printf
#define Sleep(ms) usleep(1000 * (ms))
#endif
