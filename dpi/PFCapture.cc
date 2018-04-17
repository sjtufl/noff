//
// Created by root on 17-5-20.
//

#include <muduo/base/Logging.h>
#include "PFCapture.h"
#include "IpFragment.h"
#include <pcap.h>
#include <pfring.h>

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

PFCapture::PFCapture(const char *dev)
        :name_(dev),
        running_(0)

{
    pfring *ring[MAX_NUM_RX_CHANNELS];
    n_ring_ = pfring_open_multichannel(name_.c_str(),
                                       65700, PF_RING_PROMISC, ring);

    if (n_ring_ == 0) {
        LOG_SYSFATAL << "pfring_open_multichannel(" << name_ << ")";
    }

    char name[32];
    for (int i = 0; i < n_ring_; ++i) {
        snprintf(name, 32, "PF_RING %d", i);
        ring_.push_back(ring[i]);
        threads.emplace_back(new Thread([=](){
            threadRun(ring_[i]);
        }, name));
    }
    for (pfring *p : ring_) {
        if (pfring_enable_ring(p) != 0) {
            LOG_SYSFATAL << "pfring_enable_ring()";
        }
    }
    LOG_INFO << n_ring_ << " threads started";
}

void PFCapture::setThreadInitCallback(const ThreadFunc &func)
{
    initFunc_ = func;
}

void PFCapture::threadRun(pfring *ring)
{
    if (initFunc_ != NULL) {
        initFunc_();
    }

    auto &ipFrag = threadInstance(IpFragment);

    u_int len = 65660;
    u_char buffer_data[len];
    pfring_pkthdr hdr;
    while (running_) {
        u_char *buffer = buffer_data;
        int ret = pfring_recv(ring, &buffer, len, &hdr, 1);
        if (ret != 1) {
            LOG_SYSERR << "pfring_recv()";
            continue;
        }

        int offset;

        if (buffer[12] == 0x08 && buffer[13] == 0x00)
            offset = 14;
        else if (buffer[12] == 0x81 && buffer[13] == 0)
            offset = 18;
        else
            continue;

        for (auto &func : packetCallbacks_)
            func(&hdr, buffer, hdr.ts);

        ipFrag.startIpfragProc(((ip*)(buffer + offset)), hdr.caplen - offset, hdr.ts);
    }
}

void PFCapture::setFilter(const char *filter)
{
    if (strlen(filter) >= 32) {
        LOG_FATAL << "filer string too long";
    }

    char data[32];
    strcpy(data, filter);
    for (pfring *p : ring_) {
        if (pfring_set_bpf_filter(p, data) != 0) {
            LOG_SYSFATAL << "set filter failed";
        }
    }
}

//void PFCapture::onPacket(const pfring_pkthdr *hdr, const u_char *data, timeval timeStamp)
//{
//    if (hdr->caplen <= linkOffset_) {
//        LOG_WARN << "Capture: packet too short";
//        return;
//    }
//
//    switch (linkType_) {
//
//        case DLT_EN10MB:
//            if (data[12] == 0x81 && data[13] == 0) {
//                /* Skip 802.1Q VLAN and priority information */
//                linkOffset_ = 18;
//            }
//            else if (data[12] != 0x08 || data[13] != 0x00) {
//                LOG_DEBUG << "Capture: receive none IP packet";
//                return;
//            }
//            break;
//
//        case DLT_LINUX_SLL:
//            if (data[14] != 0x08 || data[15] != 0x00) {
//                LOG_TRACE << "Capture: receive none IP packet";
//                return;
//            }
//            break;
//
//        default:
//            // never happened
//            LOG_FATAL << "Capture: receive unsupported packet";
//            return;
//    }
//
//    for (auto& func : ipFragmentCallbacks_) {
//        func((ip*)(data + linkOffset_),
//             hdr->caplen - linkOffset_,
//             timeStamp);
//    }
//}

void PFCapture::logCaptureStats()
{
    pfring_stat stat, total = {0, 0};
    for (pfring *p : ring_) {
        pfring_stats(p, &stat);
        total.recv += stat.recv;
        total.drop += stat.drop;
    }
    LOG_INFO << name_
             << ": receive packet " << total.recv
             << ", drop by kernel " << total.drop;
}