
#include <TRESTDAQSocket.h>

void TRESTDAQSocket::Open(int* rem_ip_base, int* loc_ip, int rpt) {
    // Initialize socket
    if ((client = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        std::string error = "Socket open failed: " + std::string(strerror(errno));
        throw(TRESTDAQException(error));
    }

    // Set socket in non-blocking mode
    int nb = 1;
    if (ioctl(client, FIONBIO, &nb) != 0) {
        std::string error = "ioctl socket failed: " + std::string(strerror(errno));
        throw(TRESTDAQException(error));
    }

    socklen_t optlen = sizeof(int);
    int rcvsz_req = 200 * 1024;
    // Set receive socket size
    if (setsockopt(client, SOL_SOCKET, SO_RCVBUF, &rcvsz_req, optlen) != 0) {
        std::string error = "setsockopt failed: " + std::string(strerror(errno));
        throw(TRESTDAQException(error));
    }

    int rcvsz_done;
    // Get receive socket size
    if (getsockopt(client, SOL_SOCKET, SO_RCVBUF, &rcvsz_done, &optlen) != 0) {
        std::string error = "getsockopt failed: " + std::string(strerror(errno));
        throw(TRESTDAQException(error));
    }

    // Bind the socket to the local IP address
    struct sockaddr_in src;
    src.sin_family = PF_INET;
    //  if ( (*(loc_ip+0) == 0) && (*(loc_ip+1) == 0) &&(*(loc_ip+2) == 0) &&(*(loc_ip+3) == 0) ){
    src.sin_addr.s_addr = htonl(INADDR_ANY);
    //}	else {
    //  src.sin_addr.s_addr = htonl(((*(loc_ip+0)&0xFF)<<24) | ((*(loc_ip+1)&0xFF)<<16) | ((*(loc_ip+2)&0xFF)<<8) | ((*(loc_ip+3)&0xFF)<<0));
    //  }
    src.sin_port = 0;
    if (bind(client, (struct sockaddr*)&src, sizeof(struct sockaddr_in)) != 0) {
        std::string error = "socket bind failed: " + std::string(strerror(errno));
        throw(TRESTDAQException(error));
    }

    // Check receive socket size
    if (rcvsz_done < rcvsz_req) {
        std::cout << "Warning in socket: recv buffer size set to " << rcvsz_done << " bytes while " << rcvsz_req
                  << " bytes were requested. Data losses may occur" << std::endl;
    }

    // Init target address
    rem_port = rpt;
    target.sin_family = PF_INET;
    target.sin_port = htons((unsigned short)rem_port);
    target_adr = (unsigned char*)&(target.sin_addr.s_addr);
    target_adr[0] = *(rem_ip_base + 0);
    target_adr[1] = *(rem_ip_base + 1);
    target_adr[2] = *(rem_ip_base + 2);
    target_adr[3] = *(rem_ip_base + 3);
    remote_size = sizeof(remote);
}

void TRESTDAQSocket::Close() {
    close(client);
    client = 0;
}

void TRESTDAQSocket::Clear() {
    client = 0;
    rem_port = 0;
    remote_size = 0;
    target_adr = (unsigned char*)0;
}
