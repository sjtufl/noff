//
// Created by root on 17-5-8.
//

#ifndef NOFF_PROTOCOLPACKETCOUNTER_H
#define NOFF_PROTOCOLPACKETCOUNTER_H

#include <functional>
#include <array>

#include <util/noncopyable.h>
#include <muduo/base/Atomic.h>
#include <muduo/base/Mutex.h>

#include "TcpFragment.h"
#include "Timer.h"
#include "util/TopicServer.h"

#define DNS_PORT        53
#define SMTP_PORT       25
#define POP3_PORT       110
#define HTTP_PORT       80
#define HTTPS_PORT      443
#define TELNET_PORT     23
#define FTP_PORT        21

#define N_PROTOCOLS     7

typedef std::array<int, N_PROTOCOLS> CounterDetail;
std::string to_string(const CounterDetail&);

using muduo::net::EventLoop;
using muduo::net::InetAddress;

class ProtocolPacketCounter : noncopyable
{
public:
    ProtocolPacketCounter(EventLoop* loop, const InetAddress& listenAddr);

    void onTcpData(TcpStream *stream, timeval timeStamp);
    void onUdpData(tuple4, char*, int, timeval timeStamp);
    void start() { server_.start(); }

private:
    void sendAndClearCounter();

private:
    std::array<muduo::AtomicInt32, N_PROTOCOLS>
            counter_;

    EventLoop* loop_;
    TopicServer server_;
};


#endif //NOFF_PROTOCOLPACKETCOUNTER_H
