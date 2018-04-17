//
// Created by frank on 17-9-25.
//

#ifndef NOFF_RING_H
#define NOFF_RING_H

#include <muduo/net/Channel.h>
#include <pfring.h>

#include "IpFragment.h"
#include "TcpFragment.h"
#include "Udp.h"

using muduo::net::EventLoop;
using muduo::net::Channel;
using muduo::Timestamp;

class Ring
{
public:
    Ring(EventLoop* loop, pfring* ring);

    IpFragment& ipFragment()
    { return ip_; }
    TcpFragment& tcpFragment()
    { return tcp_; }
    Udp& udp()
    { return udp_; }

private:
    void handleRead(Timestamp);
    void handleError();

    EventLoop* loop_;
    pfring* ring_;
    int fd_;
    Channel channel_;

    // dpi
    IpFragment ip_;
    TcpFragment tcp_;
    Udp udp_;
};


#endif //NOFF_RING_H
