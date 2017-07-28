//
// Created by root on 17-7-28.
//

#include "TcpClient.h"

TcpClient::TcpClient(const muduo::net::InetAddress& srvaddr,
                     const std::string& name)
        : srvaddr_(srvaddr),
          name_(name)
{
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ == -1) {
        LOG_SYSFATAL << "socket()";
    }

    int ret = connect(sockfd_, srvaddr_.getSockAddr(), sizeof(sockaddr_in));
    if (ret == -1) {
        LOG_SYSFATAL << "connect()";
    }

    LOG_INFO << "["<< name << "] " << srvaddr.toIpPort();
}

void TcpClient::onByteStream(const char *data, size_t len)
{
    assert(data != NULL);

    again:
    ssize_t n = send(sockfd_, data, len, 0);
    if (n == -1) {
        if (errno == EINTR)
            goto again;
        else if (errno == EAGAIN)
            LOG_WARN << "["<< name_ << "] "
                     << srvaddr_.toIpPort() <<" receiver too slow";
        else
            LOG_SYSERR << "send()";
    }
    else if (send(sockfd_, data, len, 0) != len) {
        LOG_WARN << "["<< name_ << "] "
                 << srvaddr_.toIpPort() <<" receiver too slow";
    }
}