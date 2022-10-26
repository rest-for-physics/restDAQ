
#ifndef __TREST_DAQ_SOCKET__
#define __TREST_DAQ_SOCKET__

#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <iostream>
#include <mutex>

#include "TRESTDAQException.h"

#define REMOTE_DST_PORT 1122

class TRESTDAQSocket {
   public:
    TRESTDAQSocket(){};
    int client;
    struct sockaddr_in target;
    unsigned char* target_adr;

    struct sockaddr_in remote;
    unsigned int remote_size;
    int rem_port;

    void Close();
    void Clear();
    void Open(int* rem_ip_base, int* loc_ip, int rpt);
};

#endif
