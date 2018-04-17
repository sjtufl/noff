//
// Created by root on 17-5-20.
//

#ifndef NOFF_PFCAPTURE_H
#define NOFF_PFCAPTURE_H

#include <vector>
#include <signal.h>
#include <pfring.h>
#include <netinet/ip.h>
#include <muduo/base/Thread.h>

#include <util/noncopyable.h>

#include "Callback.h"

using muduo::Thread;

class PFCapture : noncopyable
{
public:
    PFCapture(const char *dev);
    ~PFCapture()
    {
        if (running_) {
            breakLoop();
        }
        logCaptureStats();
        for (pfring *p : ring_) {
            pfring_close(p);
        }
    }

    void setThreadInitCallback(const ThreadFunc &func);

    void addPacketCallBack(const PacketCallback& cb)
    {
        packetCallbacks_.push_back(cb);
    }

    void startLoop()
    {
        assert(!running_);
        running_ = 1;

        for (int i = 0; i < n_ring_; ++i) {
            threads[i]->start();
        }
    }

    // just call in signal handler
    void breakLoop()
    {
        assert(running_);
        running_ = 0;
        for (int i = 0; i < n_ring_; ++i) {
            pfring_breakloop(ring_[i]);
            threads[i]->join();
        }
    }

    void setFilter(const char *filter);

private:
    std::vector<std::unique_ptr<Thread>> threads;
    std::vector<pfring*> ring_;
    int n_ring_;
    std::string name_;

    ThreadFunc initFunc_;

    int  running_;

    std::vector<PacketCallback> packetCallbacks_;

    void logCaptureStats();
    void threadRun(pfring *ring);
};


#endif //NOFF_PFCAPTURE_H
