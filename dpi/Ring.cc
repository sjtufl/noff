//
// Created by frank on 17-9-25.
//

#include <sys/eventfd.h>

#include "Ring.h"

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

Ring::Ring(EventLoop *loop, pfring *ring)
        : loop_(loop),
          ring_(ring),
          fd_(::eventfd(1, EFD_NONBLOCK | EFD_CLOEXEC)), // always readable
          channel_(loop, fd_)
{
    if (fd_ == -1)
        LOG_SYSFATAL << "eventfd()";

    channel_.setReadCallback(std::bind(&Ring::handleRead, this, _1));
    channel_.setErrorCallback(std::bind(&Ring::handleError, this));
    channel_.enableReading();

    ip_.addTcpCallback(std::bind(
            &TcpFragment::processTcp, &tcp_, _1, _2, _3));
    ip_.addUdpCallback(std::bind(
            &Udp::processUdp, &udp_, _1, _2, _3));
}

void Ring::handleRead(Timestamp)
{
    u_int len = 65660;
    u_char buffer_data[len];
    pfring_pkthdr hdr;

#ifndef NDEBUG
    const int loops = 1;
#else
    const int loops = 100000;
#endif

    for (int i = 0; i < loops; ++i) {
        u_char *buffer = buffer_data;
        int ret = pfring_recv(ring_, &buffer, len, &hdr, 1);
        if (ret != 1) {
            if (ret != 0)
                LOG_SYSERR << "pfring_recv()";
            break;
        }


        int offset;
        if (buffer[12] == 0x08 && buffer[13] == 0x00)
            offset = 14;
        else if (buffer[12] == 0x81 && buffer[13] == 0)
            offset = 18;
        else
            continue;

//        for (auto &func : packetCallbacks_)
//            func(&hdr, buffer, hdr.ts);

        ip_.startIpfragProc(((ip*)(buffer + offset)), hdr.caplen - offset, hdr.ts);
    }
}

void Ring::handleError()
{
    LOG_SYSERR << "pfring fd get error";
}