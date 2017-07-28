//
// Created by root on 17-5-20.
//

#ifndef NOFF_PFCAPTURE_H
#define NOFF_PFCAPTURE_H

#include <vector>
#include <pfring.h>
#include <netinet/ip.h>

#include "Callback.h"

class PFCapture : muduo::noncopyable
{
public:
    PFCapture(const char *dev);
    ~PFCapture()
    {
        if (running_) {
            breakLoop();
        }
        logCaptureStats();
    }

    void addPacketCallBack(const PacketCallback& cb)
    {
        packetCallbacks_.push_back(cb);
    }

    void addIpFragmentCallback(const IpFragmentCallback& cb)
    {
        ipFragmentCallbacks_.push_back(cb);
    }

    void startLoop();

    // not thread safe, just call in signal handler
    void breakLoop()
    {
        assert(running_);
        running_ = 0;
        pfring_breakloop(ring_);
    }

    void setFilter(const char *filter);

private:
    pfring *ring_;

    std::string     name_;

    volatile sig_atomic_t running_;

    std::vector<PacketCallback>      packetCallbacks_;
    std::vector<IpFragmentCallback>  ipFragmentCallbacks_;

    void logCaptureStats();
};


#endif //NOFF_PFCAPTURE_H
