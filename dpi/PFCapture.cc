//
// Created by root on 17-5-20.
//

#include <muduo/base/Logging.h>
#include "PFCapture.h"
#include <pcap.h>

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

PFCapture::PFCapture(const char *dev)
        :running_(0),
         name_(dev)
{
    ring_ = pfring_open(name_.c_str(), 65700, PF_RING_PROMISC);
    if (ring_ == nullptr) {
        LOG_SYSFATAL << "pfring_open()";
    }

    if (pfring_enable_ring(ring_) != 0) {
       LOG_SYSFATAL << "pfring_enable_ring()";
    }
}

void PFCapture::startLoop()
{
    assert(!running_);
    running_ = 1;
    pfring_loop()
    u_char          *buffer;
    pfring_pkthdr   hdr;
    while (running_) {
        int ret = pfring_recv_parsed(ring_, &buffer, 65660, &hdr, 1, 4, 0, 0);
        if (ret != 1) {
            LOG_SYSERR << "pfring_recv_parsed()";
            continue;
        }

        for (auto& func : packetCallbacks_)
            func(&hdr, buffer, hdr.ts);

        if (hdr.extended_hdr.parsed_pkt.l3_proto == IPPROTO_IP) {
            size_t offset = hdr.extended_hdr.parsed_pkt.offset.
            for (auto &func : ipFragmentCallbacks_) {
                func((ip *) (buffer + hdr.extended_hdr.parsed_pkt.),
                     hdr->caplen - linkOffset_,
                     timeStamp);
            }
        }
    }
}

void PFCapture::setFilter(const char *filter)
{
    if (strlen(filter) >= 32) {
        LOG_FATAL << "filer string too long";
    }

    char data[32];
    strcpy(data, filter);
    if (pfring_set_bpf_filter(ring_, data) != 0) {
        LOG_FATAL << "set filter failed";
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
    pfring_stat stat;
    pfring_stats(ring_, &stat);
    LOG_INFO << name_
             << ": receive packet " << stat.recv
             << ", drop by kernel " << stat.drop;
}