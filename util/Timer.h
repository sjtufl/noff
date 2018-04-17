//
// Created by root on 17-5-9.
//

#ifndef DNSPARSER_TIMER_H
#define DNSPARSER_TIMER_H

#include <pcap.h>
#include <muduo/base/Mutex.h>
#include <util/noncopyable.h>

class Timer: noncopyable
{
public:
    Timer():time(0)
    {}
    bool checkTime(timeval TimeStamp);

    timeval lastCheckTime() const
    {
        return { time - 1, 0 };
    }

private:
    __time_t time;

    muduo::MutexLock lock;
};
#endif //DNSPARSER_TIMER_H