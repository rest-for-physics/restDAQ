/*********************************************************************************
TRESTDAQT2K.cxx

Implement data acquisition for DCC + FEM + FEC based readout

Author: JuanAn Garcia 18/05/2021

*********************************************************************************/

#include <sys/time.h>
#include <chrono>

#include "TRESTDAQDCC.h"

const int REMOTE_DST_PORT = 1122;

DCCEvent::DCCEvent(bool comp, const std::string& fileName) : compress(comp) {
    struct tm* now;
    time_t start_time;
    char run_str[21];
    char str_res[4];

    // Open result file
    fileOpen = false;
    if (!(fout = fopen(fileName.c_str(), "wb"))) {
        std::cerr << "Event_Initialize: cannot open file " << fileName << std::endl;
        return;
    }
    fileOpen = true;
    // Get the time of start
    time(&start_time);
    now = localtime(&start_time);
    sprintf(run_str, "R%4d.%02d.%02d-%02d:%02d:%02d", ((now->tm_year) + 1900), ((now->tm_mon) + 1),
            now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec);

    fwrite(run_str, 20, 1, fout);
}

DCCEvent::~DCCEvent() {
    if (fileOpen) fclose(fout);
}

void DCCEvent::startEvent() {
    ev_sz = 8;
    cur_ptr = &(data[0]);
}

void DCCEvent::addData(uint8_t* buf, int size) {
    if (compress) {
        DataPacket* dp = (DataPacket*)buf;
        if (ntohs(dp->samp[0]) == (CELL_INDEX_FLAG | 0x1FF) ||
            (ntohs(dp->scnt) <= 8) && (ntohs(dp->samp[0]) == 0) && (ntohs(dp->samp[1]))) {
            return;
        }
    }

    if (size < DCCPacket::MAX_EVENT_SIZE) {
        memcpy(cur_ptr, buf, size);
        cur_ptr += size;
        ev_sz += size;
    }
}

void DCCEvent::writeEvent(int evnb) {
    if (!fileOpen) return;

    uint32_t nb = htonl(ev_sz);
    fwrite(&nb, 4, 1, fout);
    // write event_number
    nb = htonl(evnb);
    fwrite(&nb, 4, 1, fout);
    // write data
    fwrite(data, (ev_sz - 8), 1, fout);
}

void TRESTDAQDCC::initialize() {
    // Initialize socket
    if (m_sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP) == -1) {
        std::cerr << "Socket open failed: " << strerror(errno) << std::endl;
        // Throw?
    }

    // Set socket in non-blocking mode
    int nb = 1;
    if (ioctlsocket(m_sockfd, FIONBIO, &nb) != 0) {
        std::cerr << "ioctlsocket failed: " << strerror(errno) << std::endl;
    }

    socklen_t optlen = sizeof(int);
    int rcvsz_req = 200 * 1024;
    // Set receive socket size
    if (setsockopt(m_sockfd, SOL_SOCKET, SO_RCVBUF, &rcvsz_req, optlen) != 0) {
        std::cerr << "setsockopt failed: " << strerror(errno) << std::endl;
    }

    int rcvsz_done;
    // Get receive socket size
    if (getsockopt(m_sockfd, SOL_SOCKET, SO_RCVBUF, &rcvsz_done, &optlen) != 0) {
        std::cerr << "getsockopt failed: " << strerror(errno) << std::endl;
    }

    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    // Set rcv timeout
    if (setsockopt(m_sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
        std::cerr << "setsock timeout failed: " << strerror(errno) << std::endl;
    }
}

void TRESTDAQDCC::configure() {}

void TRESTDAQDCC::startDAQ() {}

void TRESTDAQDCC::stopDAQ() {}

DCCPacket::packetReply TRESTDAQDCC::SendCommand(const char* cmd, DCCPacket::packetType pckType,
                                                size_t nPackets) {
    if (sendto(m_sockfd, cmd, strlen(cmd), 0, (struct sockaddr*)&(m_target), sizeof(struct sockaddr)) == -1) {
        std::cerr << "sendto failed: " << strerror(errno) << std::endl;
        return DCCPacket::packetReply::ERROR;
    }

    // wait for incoming messages
    bool done = false;
    int pckCnt = 1;
    socklen_t remote_size = sizeof(m_remote);
    uint8_t buf_rcv[8192];
    uint8_t* buf_ual;
    DCCPacket::packetReply errRep = DCCPacket::packetReply::OK;

    while (!done) {
        int length;
        int cnt = 0;
        auto startTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::duration<int>>(
            std::chrono::steady_clock::now() - startTime);

        do {
            length = recvfrom(m_sockfd, buf_rcv, 8192, 0, (struct sockaddr*)&m_remote, &remote_size);

            if (length < 0) {
                int err = socket_get_error();
                if (err == EWOULDBLOCK || err == EAGAIN) {
                    if (cnt % 1000 == 0) {
                        duration = std::chrono::duration_cast<std::chrono::duration<int>>(
                            std::chrono::steady_clock::now() - startTime);
                    }
                } else {
                    std::cerr << "recvfrom failed: " << strerror(errno) << std::endl;
                    return DCCPacket::packetReply::ERROR;
                }
            }
            cnt++;
        } while (length < 0 && duration.count() < 10);

        if (duration.count() > 10) {
            std::cerr << "No reply after " << duration.count() << "seconds, missing packets are expected"
                      << std::endl;
            return DCCPacket::packetReply::ERROR;
        }

        // if the first 2 bytes are null, UDP datagram is aligned on next 32-bit boundary, so skip these first
        // two bytes
        if ((buf_rcv[0] == 0) && (buf_rcv[1] == 0)) {
            buf_ual = &buf_rcv[2];
            length -= 2;
        } else {
            buf_ual = &buf_rcv[0];
        }

        if ((*buf_ual == '-')) {  // ERROR ASCII packet
            return DCCPacket::packetReply::RETRY;
        }

        if (pckType == DCCPacket::packetType::BINARY) {  // DAQ packet

            event->addData(buf_ual, length);

            DataPacket* data_pkt = (DataPacket*)buf_ual;

            // Check End Of Event
            if (GET_FRAME_TY_V2(ntohs(data_pkt->dcchdr)) & FRAME_FLAG_EOEV ||
                GET_FRAME_TY_V2(ntohs(data_pkt->dcchdr)) & FRAME_FLAG_EORQ || pckCnt >= nPackets) {
                done = true;
            }

        } else {
            done = true;  // ASCII Packet, check response?
        }

        pckCnt++;
    }
    return DCCPacket::packetReply::OK;
}

void TRESTDAQDCC::waitForTrigger() {  // Wait till trigger is acquired
    DCCPacket::packetReply reply;
    do {
        reply = SendCommand("wait 1000000");                    // Wait for the event to be acquired
    } while (reply == DCCPacket::packetReply::RETRY && !abrt);  // Infinite loop till aborted or wait succeed
}

