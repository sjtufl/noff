//
// Created by root on 17-7-28.
//

#ifndef NOFF_TCPCLIENT_H
#define NOFF_TCPCLIENT_H

#include <arpa/inet.h>

#include <muduo/base/noncopyable.h>
#include <muduo/base/Logging.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/InetAddress.h>

class TcpClient : muduo::noncopyable
{
public:
    TcpClient(const muduo::net::InetAddress& srvaddr, const std::string& name = "debug");
    ~TcpClient()
    {
        close(sockfd_);
    }

    void onString(const std::string& str)
    {
        onByteStream(str.data(), str.size());
    }

    template <typename T>
    void onData(const T &data)
    {
        using namespace std; // ??? wtf

        std::string buffer = to_string(data);
        buffer.append("\r\n");

        onByteStream(buffer.data(), buffer.length());
    }

    template <typename T>
    void onDataPointer(const T *data)
    {
        onData(*data);
    }

private:
    void onByteStream(const char *data, size_t len);

    muduo::net::InetAddress srvaddr_;
    std::string name_;
    int sockfd_;
};


#endif //NOFF_TCPCLIENT_H
