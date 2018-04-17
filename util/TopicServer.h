//
// Created by frank on 17-9-25.
//

#ifndef NOFF_TOPICSERVER_H
#define NOFF_TOPICSERVER_H

#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpServer.h>
#include <muduo/base/Logging.h>

#include <util/noncopyable.h>

using muduo::net::TcpServer;
using muduo::net::EventLoop;
using muduo::net::InetAddress;
using muduo::net::TcpConnectionPtr;

class TopicServer: noncopyable
{
public:
    TopicServer(EventLoop* loop,
                const InetAddress& listenAddr,
                const std::string& name);

    // thread safe
    void send(const std::string& message);
    void start() { server_.start(); }

    // NOT thread safe
    bool connected() const
    { return connected_; }
    bool congestion() const
    { return congestion_; }

private:
    void onConnection(const TcpConnectionPtr& conn);
    void onHighWaterMark(const TcpConnectionPtr& conn, size_t mark);
    void onWriteCompelte(const TcpConnectionPtr& conn);
    void onMessage(const TcpConnectionPtr& conn);

private:
    EventLoop* loop_;
    TcpServer server_;
    TcpConnectionPtr conn_;
    bool connected_;
    bool congestion_;
    static const size_t highWaterMark = 16 * 1024 * 1024;
};


#endif //NOFF_TOPICSERVER_H
