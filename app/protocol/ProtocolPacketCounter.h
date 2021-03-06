//
// Created by root on 17-5-8.
//

#ifndef NOFF_PROTOCOLPACKETCOUNTER_H
#define NOFF_PROTOCOLPACKETCOUNTER_H

#include <functional>
#include <array>

#include <muduo/base/noncopyable.h>
#include <muduo/base/Atomic.h>
#include <muduo/base/Mutex.h>

#include "TcpFragment.h"
#include "Timer.h"

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


class ProtocolPacketCounter : muduo::noncopyable
{
public:

    typedef std::function<void(const CounterDetail&)> CounterCallback;

    void onTcpData(TcpStream *stream, timeval timeStamp);

    void onUdpData(tuple4, char*, int, timeval timeStamp);

    void setCounterCallback(const CounterCallback& cb)
    {
        counterCallback_ = cb;
    }

private:

    CounterCallback counterCallback_;

    Timer timer;

    std::array<muduo::AtomicInt32, N_PROTOCOLS>
            counter;
};


#endif //NOFF_PROTOCOLPACKETCOUNTER_H
