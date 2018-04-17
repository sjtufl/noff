//
// Created by root on 17-9-27.
//

#include <sys/socket.h>

#include "Https.h"

Https::Https(EventLoop *loop, const InetAddress& addr)
        : loop_(loop),
          addr_(addr),
          fd_(::socket(AF_INET, SOCK_DGRAM, 0))
{
    if (fd_ == -1)
        LOG_SYSFATAL << "socket()";
}

Https::~Https()
{ ::close(fd_); }

void Https::onTcpData(TcpStream *stream, timeval timeStamp,
                      u_char* data, int len, int flag)
{
    tuple4 t4 = stream->addr;
    if (t4.dest != 443)
        return;

    std::string res(std::to_string(timeStamp.tv_sec));

    if (flag == FROMSERVER) {
        std::swap(t4.saddr, t4.daddr);
        std::swap(t4.source, t4.dest);
    }

    res.append("\t");
    char src_ip[30];
    inet_ntop(AF_INET, &t4.saddr, src_ip, 30);
    res.append(src_ip);

    res.append("\t");
    char dst_ip[30];
    inet_ntop(AF_INET, &t4.daddr, dst_ip, 30);
    res.append(dst_ip);

    res.append("\t");
    res.append(std::to_string(t4.source));

    res.append("\t");
    res.append(std::to_string(t4.dest));

    res.append("\t");
    res.append(std::string(data, data+len));

    ssize_t n = sendto(fd_, res.c_str(), res.length(), 0,
                       addr_.getSockAddr(), sizeof(sockaddr_in));

    if (n != res.length())
        LOG_SYSERR << "tcp data too long";
}