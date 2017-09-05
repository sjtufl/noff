//
// Created by root on 17-8-3.
//

#ifndef NOFF_PUBLISHER_H
#define NOFF_PUBLISHER_H

#include <muduo/net/TcpServer.h>
#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <set>

using namespace muduo;
using namespace muduo::net;

namespace
{
const int MAX_CONNECTIONS = 3;
const int MAX_BUFFERSIZE = 102400;
EventLoop loop;
}

class NoffServer
{
public:
    NoffServer(const InetAddress &listenAddr,
              const muduo::string &name)
            : server_(&loop, listenAddr, name),
              connections_(new ConnectionList)
    {
        server_.setConnectionCallback(
                std::bind(&NoffServer::onConnection, this, _1));
        server_.start();
        LOG_INFO << "["<< name << "] " << listenAddr.toIpPort();
    }

    template <typename T>
    void onData(const T &data)
    {
        using namespace std; // ??? wtf

        std::string buffer = to_string(data);
        buffer.append("\r\n");
        send(buffer);
    }

    template <typename T>
    void onDataPointer(const T *data)
    {
        onData(*data);
    }

    static void startLoop()
    {
        loop.loop();
    }

    static void breakLoop()
    {
        loop.quit();
    }

private:
    void onConnection(const TcpConnectionPtr& conn)
    {
        LOG_INFO << conn->localAddress().toIpPort() << " -> "
                 << conn->peerAddress().toIpPort() << " is "
                 << (conn->connected() ? "UP" : "DOWN");

        MutexLockGuard lock(mutex_);
        if (!connections_.unique())
            connections_.reset(new ConnectionList(*connections_));
        assert(connections_.unique());

        if (conn->connected()) {
            if (connections_->size() < MAX_CONNECTIONS)
                connections_->insert(conn);
            else
                conn->shutdown();
        }
        else
            connections_->erase(conn);
    }

    void send(const std::string &message)
    {
        ConnectionListPtr connections = getConnectionList();
        for (auto &conn : *connections) {
            if (conn->outputBuffer()->readableBytes() < MAX_BUFFERSIZE)
                conn->send(message);
            else
                LOG_WARN << conn->name() << " receiver too slow";
        }
    }

    typedef std::set<TcpConnectionPtr> ConnectionList;
    typedef std::shared_ptr<ConnectionList> ConnectionListPtr;

    ConnectionListPtr getConnectionList()
    {
        MutexLockGuard lock(mutex_);
        return connections_;
    }

    TcpServer server_;
    MutexLock mutex_;
    ConnectionListPtr connections_;
};

#endif //NOFF_PUBLISHER_H
