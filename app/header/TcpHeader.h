//
// Created by root on 17-5-12.
//

#ifndef NOFFDEBUG_TCPHEADER_H
#define NOFFDEBUG_TCPHEADER_H

#include <netinet/ip.h>
#include <functional>
#include <vector>
#include <time.h>

#include "util/TopicServer.h"

using muduo::net::EventLoop;
using muduo::net::InetAddress;

struct tcpheader
{
    timeval         timeStamp;
    unsigned int    srcIP;
    unsigned int    dstIP;
    unsigned short  srcPort;
    unsigned short  dstPort;
    char            flag;
    int             len;
};

class TcpHeader
{
public:
    typedef std::function<void(tcpheader&)> TcpHeaderCallback;

public:
    TcpHeader(EventLoop* loop, const InetAddress& listenAddr);

    void start()
    { server_.start(); }

    void addTcpHeaderCallback(const TcpHeaderCallback& cb)
    { tcpHeaderCallback_.push_back(cb); }

    void processTcpHeader(ip * data,int skblen, timeval timeStamp);

private:
    std::vector<TcpHeaderCallback> tcpHeaderCallback_;

    EventLoop* loop_;
    TopicServer server_;
};

std::string to_string(const tcpheader&);

#endif //NOFFDEBUG_TCPHEADER_H
