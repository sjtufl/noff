//
// Created by root on 17-5-8.
//

#include <muduo/net/EventLoop.h>

#include "ProtocolPacketCounter.h"

using std::string;
string to_string(const CounterDetail &d)
{
    using std::to_string;

    string buffer;
    int total = 0;

    for (int x : d) {
        buffer.append(std::to_string(x));
        buffer.append("\t");
        total += x;
    }

    buffer.append(std::to_string(total));

    return buffer.append("\n");
}

ProtocolPacketCounter::ProtocolPacketCounter(EventLoop* loop,
                                             const InetAddress& listenAddr)
        : counter_(),
          loop_(loop),
          server_(loop, listenAddr, "packet counter_")
{
    loop_->runEvery(1, [this](){sendAndClearCounter();});
}


void ProtocolPacketCounter::onTcpData(TcpStream *stream, timeval timeStamp)
{
    switch (stream->addr.dest) {
        case SMTP_PORT:
            counter_[1].increment();
            break;
        case POP3_PORT:
            counter_[2].increment();
            break;
        case HTTP_PORT:
            counter_[3].increment();
        case HTTPS_PORT:
            counter_[4].increment();
            break;
        case TELNET_PORT:
            counter_[5].increment();
            break;
        case FTP_PORT:
            counter_[6].increment();
            break;
        default:
            return;
    }
}

void ProtocolPacketCounter::onUdpData(tuple4 t4, char *, int, timeval timeStamp)
{
    if (t4.dest == DNS_PORT || t4.source == DNS_PORT)
        counter_[0].increment();
}

void ProtocolPacketCounter::sendAndClearCounter()
{
    server_.send(to_string({counter_[0].getAndSet(0), counter_[1].getAndSet(0),
                            counter_[2].getAndSet(0), counter_[3].getAndSet(0),
                            counter_[4].getAndSet(0), counter_[5].getAndSet(0),
                            counter_[6].getAndSet(0)}));
}