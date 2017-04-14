//
// Created by frabk on 17-4-10.
//

#ifndef NOFF_CAPTURE_H
#define NOFF_CAPTURE_H

#include <pcap.h>
#include <functional>

#include <muduo/base/noncopyable.h>
#include <vector>
#include <netinet/ip.h>

class Capture: muduo::noncopyable {

public:
    typedef std::function<void(const pcap_pkthdr*, const u_char*)> PacketCallback;
    typedef std::function<void(const ip*, int)> IpFragmentCallback;

    explicit
    Capture(const char *device, int snaplen, bool promisc, int msTimeout);
    ~Capture();

    void addPacketCallBack(const PacketCallback& cb)
    {
        packetCallbacks_.push_back(cb);
    }

    void addIpFragmentCallback(const IpFragmentCallback& cb)
    {
        ipFragmentCallbacks_.push_back(cb);
    }

    void startLoop(int packetCount);

    // not thread safe, just call in signal handler
    void breakLoop();

    void setFilter(const char *str);

private:
    pcap_t          *pcap_;

    std::string     device_;
    bpf_program     filter_;

    int             linkType;
    const char      *linkTypeStr;
    uint32_t        linkOffset;

    char            errBuf_[PCAP_ERRBUF_SIZE];

    bool            running_;


    std::vector<PacketCallback>      packetCallbacks_;
    std::vector<IpFragmentCallback>  ipFragmentCallbacks_;

    void onPacket(const pcap_pkthdr *, const u_char *);

    void logCaptureStats();

    static void internalCallBack(u_char *user, const struct pcap_pkthdr *hdr, const u_char *data);
};


#endif //NOFF_CAPTURE_H
