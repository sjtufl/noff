//
// Created by root on 17-9-8.
//

#ifndef NOFF_NOFF_H
#define NOFF_NOFF_H

#include <vector>
#include <pfring.h>

#include <util/noncopyable.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/Thread.h>

#include "app/protocol/ProtocolPacketCounter.h"

using muduo::net::InetAddress;
using muduo::net::EventLoop;
using muduo::Thread;

class Noff: noncopyable
{
public:
    Noff(EventLoop* loop, const std::string& devName);
    ~Noff();

    // bind callbacks and start loops
    void start();

    void setHttpTopic(const InetAddress& httpRequestAddr,
                      const InetAddress& httpResponseAddr);

    void setDnsTopic(const InetAddress& dnsRequestAddr,
                     const InetAddress& responseAddr);

    void setTcpSessionTopic(const InetAddress& addr);
    void setTcpHeaderTopic(const InetAddress& addr);
    void setPacketCounterTopic(const InetAddress& addr);
    void setHttpsTopic(const InetAddress& addr);

private:
    void runInThread(int index);
    static void setAddress(std::vector<InetAddress>& vec, const InetAddress& addr, size_t n);

private:
    EventLoop* loop_;
    std::vector<std::unique_ptr<Thread>> threads_;
    std::vector<pfring*> ring_;
    std::vector<EventLoop*> loops_;

    // protocol counter_
    std::unique_ptr<ProtocolPacketCounter> packetCounter_;
    InetAddress packetAddress_;


    // tcp server addresses
    std::vector<InetAddress> httpRequestAddress_;
    std::vector<InetAddress> httpResponseAddress_;
    std::vector<InetAddress> dnsRequestAddress_;
    std::vector<InetAddress> dnsResponseAddress_;
    std::vector<InetAddress> tcpHeaderAddress_;
    std::vector<InetAddress> tcpSessionAddress_;
    InetAddress httpsAddress_;
};


#endif //NOFF_NOFF_H
