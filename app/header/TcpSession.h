//
// Created by root on 17-5-25.
//

#ifndef NOFF_TCPSESSION_H
#define NOFF_TCPSESSION_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <netinet/ip.h>

#include <util/noncopyable.h>
#include <boost/circular_buffer.hpp>

#include "dpi/Util.h"
#include "dpi/Sharding.h"
#include "util/Timer.h"
#include "util/TopicServer.h"

struct SessionData;
std::string to_string(const SessionData&);

using muduo::net::EventLoop;
using muduo::net::InetAddress;

class TcpSession: noncopyable
{
public:
    typedef std::function<void(SessionData&)> TcpSessionCallback;

    TcpSession(EventLoop* loop,
               const InetAddress& listenAddr,
               size_t timeoutSeconds = 60)
              : loop_(loop),
                server_(loop, listenAddr, "tcp session"),
                sessionBuckets_(timeoutSeconds)
    {}

    void addTcpSessionCallback(const TcpSessionCallback& cb)
    { callbacks_.push_back(cb); }

    void onTcpData(ip *iphdr, int len, timeval timeStamp);

    void start() { server_.start(); }

private:
    typedef std::vector<TcpSessionCallback> Callbacks;
    typedef std::shared_ptr<SessionData> SessionDataPtr;
    typedef std::weak_ptr<SessionData> WeakSessionDataPtr;

    struct Entry: noncopyable
    {
        explicit
        Entry(const WeakSessionDataPtr& weak,
              TcpSession& tcpSession) :
                weak_(weak), tcpSession_(tcpSession)
        {
        }

        ~Entry();

        WeakSessionDataPtr weak_;
        TcpSession& tcpSession_;
    };

    struct Tuple4HashEqual
    {
        bool operator()(const tuple4& lhs, const tuple4& rhs) const
        {
            if (lhs == rhs) {
                return true;
            }

            return lhs.source == rhs.dest &&
                   lhs.dest == rhs.source &&
                   lhs.saddr == rhs.daddr &&
                   lhs.daddr == rhs.saddr;
        }
    };

    typedef std::shared_ptr<Entry> EntryPtr;
    typedef std::weak_ptr<Entry> WeakEntryPtr;
    typedef std::unordered_set<EntryPtr> Bucket;
    typedef boost::circular_buffer<Bucket> WeakSessionDataList;

    typedef std::unordered_map<tuple4, SessionDataPtr,
            Sharding, Tuple4HashEqual> SessionDataMap;

private:
    Callbacks       callbacks_;
    SessionDataMap  sessionDataMap_;
    ::Timer           timer_;

    EventLoop* loop_;
    TopicServer server_;

    WeakSessionDataList sessionBuckets_;

private:
    void onTimer();
    void onConnection(const tuple4 &, SessionDataPtr&);
    void onTimeOut(const SessionDataPtr &);
    void updateSession(const tuple4 &, const tcphdr& hdr, SessionDataPtr &, int len);
    EntryPtr getEntryPtr(SessionDataPtr &);
};


#endif //NOFF_TCPSESSION_H
