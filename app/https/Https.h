//
// Created by root on 17-9-27.
//

#ifndef NOFF_HTTPS_H
#define NOFF_HTTPS_H

#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>

#include "dpi/TcpFragment.h"

using muduo::net::EventLoop;
using muduo::net::InetAddress;

class Https
{
public:
    Https(EventLoop* loop, const InetAddress& addr);
    ~Https();
    void onTcpData(TcpStream* stream, timeval timeStamp, u_char* data, int len, int flag);

private:
    EventLoop* loop_;
    InetAddress addr_;
    int fd_;
};


#endif //NOFF_HTTPS_H
