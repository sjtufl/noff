//
// Created by frank on 17-9-25.
//

#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>

#include "TopicServer.h"

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

TopicServer::TopicServer(EventLoop* loop,
                         const InetAddress& listenAddr,
                         const std::string& name)
        : loop_(loop),
          server_(loop, listenAddr, muduo::string(name.c_str())),
          connected_(false),
          congestion_(false)
{
    server_.setConnectionCallback(std::bind(
            &TopicServer::onConnection, this, _1));
    server_.setWriteCompleteCallback(std::bind(
            &TopicServer::onWriteCompelte, this, _1));
    server_.setMessageCallback(std::bind(
            &TopicServer::onMessage, this, _1));
}


void TopicServer::send(const std::string &message)
{
    assert(server_.getLoop() == loop_);

    if (!connected_ || congestion_)
        return;

    // ensure thread safety
    if (auto tie = conn_)
        tie->send(message);
}

void TopicServer::onConnection(const TcpConnectionPtr &conn)
{
    loop_->assertInLoopThread();

    LOG_INFO << conn->localAddress().toIpPort() << " -> "
             << conn->peerAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");

    if (conn->connected()) {
        if (conn_ == nullptr) {
            conn_ = conn;
            conn_->setHighWaterMarkCallback(std::bind(
                    &TopicServer::onHighWaterMark, this, _1, _2), highWaterMark);
            connected_ = true;
            congestion_ = false;
            LOG_INFO << conn->name() << " start send";
        }
        else {
            conn->send("already connected");
            conn->forceClose();
            LOG_INFO << conn->name() << " force close";
        }
    }
    else if (conn == conn_){
        conn_.reset();
        connected_ = false;
        LOG_INFO << conn->name() << " normal close";
    }
}

void TopicServer::onHighWaterMark(const TcpConnectionPtr &conn, size_t mark)
{
    loop_->assertInLoopThread();
    LOG_INFO << conn_->name() << " send buffer "
             << mark << " bytes long, stop send";
    congestion_ = true;
}

void TopicServer::onWriteCompelte(const TcpConnectionPtr &conn)
{
    loop_->assertInLoopThread();
    if (connected_ && congestion_) {
        LOG_INFO << conn_->name() << "send buffer empty, go on send";
        congestion_ = false;
    }
}

void TopicServer::onMessage(const TcpConnectionPtr& conn)
{
    LOG_INFO << "message!\n";
}