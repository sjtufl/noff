//
// Created by root on 17-9-8.
//

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>

#include "IpFragment.h"
#include "TcpFragment.h"
#include "Udp.h"
#include "NoffServer.h"
#include "mac/MacCount.h"
#include "protocol/ProtocolPacketCounter.h"
#include "http/Http.h"
#include "https/Https.h"
#include "dns/Dnsparser.h"
#include "header/TcpHeader.h"
#include "header/TcpSession.h"
#include "Ring.h"

#include "Noff.h"

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;
using std::placeholders::_5;

using muduo::net::TcpServer;
using muduo::net::EventLoop;

/*namespace
{

void setHttpInThread()
{
    assert(httpRequestOutput != NULL);
    assert(httpResponseOutput != NULL);

    auto& tcp = threadInstance(TcpFragment);
    auto& http = threadInstance(Http);

    // tcp connection->http
    tcp.addConnectionCallback(bind(
            &Http::onTcpConnection, &http, _1, _2));

    // tcp data->http
    tcp.addDataCallback(bind(
            &Http::onTcpData, &http, _1, _2, _3, _4, _5));

    // tcp close->http
    tcp.addTcpcloseCallback(bind(
            &Http::onTcpClose, &http, _1, _2));

    // tcp rst->http
    tcp.addRstCallback(bind(
            &Http::onTcpRst, &http, _1, _2));

    // tcp timeout->http
    tcp.addTcptimeoutCallback(bind(
            &Http::onTcpTimeout, &http, _1, _2));

    // http request->udp client
    http.addHttpRequestCallback(bind(
            &NoffServer::onDataPointer<HttpRequest>, httpRequestOutput.get(), _1));

    // http response->udp client
    http.addHttpResponseCallback(bind(
            &NoffServer::onDataPointer<HttpResponse>, httpResponseOutput.get(), _1));
}

void setPacketCounterInThread()
{
    assert(packetCounterOutput != NULL);

    auto& udp = threadInstance(Udp);
    auto& tcp = threadInstance(TcpFragment);
    auto& counter_ = globalInstance(ProtocolPacketCounter);

    // tcp->packet counter_
    tcp.addDataCallback(bind(
            &ProtocolPacketCounter::onTcpData, &counter_, _1, _2));

    // udp->packet counter_
    udp.addUdpCallback(bind(
            &ProtocolPacketCounter::onUdpData, &counter_, _1, _2, _3, _4));
    // packet->udp output
    counter_.setCounterCallback(bind(
            &NoffServer::onData<CounterDetail>, packetCounterOutput.get(), _1));
}

void setDnsCounterInThread()
{
    assert(dnsRequestOutput != NULL);
    assert(dnsResponseOutput != NULL);

    auto& udp = threadInstance(Udp);
    auto& dns = threadInstance(DnsParser);

    udp.addUdpCallback(bind(
            &DnsParser::processDns, &dns, _1, _2, _3, _4));

    dns.addRequstecallback(bind(
            &NoffServer::onData<DnsRequest>,
            dnsRequestOutput.get(), _1));

    dns.addResponsecallback(bind(
            &NoffServer::onData<DnsResponse>,
            dnsResponseOutput.get(), _1));
}

void setMacCounterInThread()
{
    assert(cap != NULL);
    assert(macCounterOutput != NULL);

    auto& mac = globalInstance(MacCount);

    cap->addPacketCallBack(bind(
            &MacCount::processMac, &mac, _1, _2, _3));

//    cap2->addPacketCallBack(bind(
//            &MacCount::processMac, &mac, _1, _2, _3));

    mac.addEtherCallback(bind(
            &Client::onData<MacInfo>, macCounterOutput.get(), _1));
}

void setTcpHeaderInThread()
{
    assert(tcpHeaderOutput != NULL);

    auto& ip = threadInstance(IpFragment);
    auto& header = threadInstance(TcpHeader);

    ip.addTcpCallback(bind(
            &TcpHeader::processTcpHeader, &header, _1, _2, _3));

    header.addTcpHeaderCallback(bind(
            &Client::onData<tcpheader>, tcpHeaderOutput.get(), _1));
}

void setTcpSessionInThread()
{
    assert(tcpSessionOutPut != NULL);

    auto& ip = threadInstance(IpFragment);
    auto& session = threadInstance(TcpSession);

    ip.addTcpCallback(bind(
            &TcpSession::onTcpData, &session, _1, _2, _3));

    session.addTcpSessionCallback(bind(
            &Client::onData<SessionData>, tcpSessionOutPut.get(), _1));
}

void initInThread()
{
    assert(cap != NULL);

    auto& ip = threadInstance(IpFragment);
    auto& udp = threadInstance(Udp);
    auto& tcp = threadInstance(TcpFragment);

    // ip->tcp
    ip.addTcpCallback(bind(
            &TcpFragment::processTcp, &tcp, _1, _2, _3));

    // ip->udp
    ip.addUdpCallback(bind(
            &Udp::processUdp, &udp, _1, _2, _3));

    // tcp->http->udp output
    setHttpInThread();

    // tcp->packet counter_->udp output
    setPacketCounterInThread();

    // udp->dns>udp
    setDnsCounterInThread();

    // tcp->udp
    setTcpHeaderInThread();

    setTcpSessionInThread();
}

}*/

Noff::Noff(EventLoop *loop, const std::string& devName)
        : loop_(loop)
{
    ring_.resize(MAX_NUM_RX_CHANNELS);
    int n = pfring_open_multichannel(devName.c_str(), 65660,
                                     PF_RING_PROMISC, ring_.data());
    if (n == 0)
        LOG_SYSFATAL << "pfring_open_multichannel(" << devName << ")";

    ring_.resize(n);
    loops_.resize(n);

    for (pfring *p : ring_) {
        if (pfring_enable_ring(p) != 0) {
            LOG_SYSFATAL << "pfring_enable_ring()";
        }
    }
    if (n > 16)
        n = 16;

    char name[32];
    for (int i = 0; i < n; ++i) {
        snprintf(name, 32, "PF_RING %d", i);
        threads_.emplace_back(new Thread([=](){
            runInThread(i);
        }, name));
    }

    LOG_INFO << n << " threads started";
}

Noff::~Noff()
{
    for (auto loop: loops_) {
        if (loop != nullptr)
            loop->quit();
    }
    for (auto& thread: threads_)
        thread->join();

    pfring_stat stat, total = {0, 0, 0};
    for (pfring *p : ring_) {
        pfring_stats(p, &stat);
        total.recv += stat.recv;
        total.drop += stat.drop;
        pfring_close(p);
    }
    LOG_INFO << ": receive packet " << total.recv
             << ", drop by kernel " << total.drop;
}

void Noff::start()
{
    for (auto& thread: threads_)
        thread->start();
}

void Noff::setHttpTopic(const InetAddress& httpRequestAddr,
                        const InetAddress& httpResponseAddr)
{
    setAddress(httpRequestAddress_, httpRequestAddr, threads_.size());
    setAddress(httpResponseAddress_, httpResponseAddr, threads_.size());
}

void Noff::setDnsTopic(const InetAddress& requestAddr,
                       const InetAddress& responseAddr)
{
    setAddress(dnsRequestAddress_, requestAddr, threads_.size());
    setAddress(dnsResponseAddress_, responseAddr, threads_.size());
}

void Noff::setTcpHeaderTopic(const InetAddress &listenAddr)
{ setAddress(tcpHeaderAddress_, listenAddr, threads_.size()); }

void Noff::setTcpSessionTopic(const InetAddress &listenAddr)
{ setAddress(tcpSessionAddress_, listenAddr, threads_.size()); }

void Noff::setPacketCounterTopic(const InetAddress &addr)
{
    packetAddress_ = addr;
    packetCounter_.reset(new ProtocolPacketCounter(loop_, addr));
    packetCounter_->start();
}

void Noff::setHttpsTopic(const InetAddress& addr)
{ httpsAddress_ = addr; }

void Noff::runInThread(int index)
{
    assert(!loop_->isInLoopThread());
    assert(!tcpHeaderAddress_.empty());
    assert(packetCounter_ != nullptr);

    EventLoop loop;

    // the loop will be occupied by ring most of the time, so the connection may have long delay !!!
    Ring ring(&loop, ring_[index]);

    // tcp header
    TcpHeader tcpHeader(&loop, tcpHeaderAddress_[index]);
    ring.ipFragment().addTcpCallback(bind(
                &TcpHeader::processTcpHeader, &tcpHeader, _1, _2, _3));
    tcpHeader.start();

    //set dns
    DnsParser dns(&loop, dnsRequestAddress_[index], dnsResponseAddress_[index]);
    ring.udp().addUdpCallback(bind(
            &DnsParser::processDns, &dns, _1, _2, _3, _4));
    dns.start();

    //set http
    Http http(&loop, httpRequestAddress_[index], httpResponseAddress_[index]);
    ring.tcpFragment().addConnectionCallback(bind(
            &Http::onTcpConnection, &http, _1, _2));
    // tcp data->http
    ring.tcpFragment().addDataCallback(bind(
            &Http::onTcpData, &http, _1, _2, _3, _4, _5));
    // tcp close->http
    ring.tcpFragment().addTcpcloseCallback(bind(
            &Http::onTcpClose, &http, _1, _2));
    // tcp rst->http
    ring.tcpFragment().addRstCallback(bind(
            &Http::onTcpRst, &http, _1, _2));
    // tcp timeout->http
    ring.tcpFragment().addTcptimeoutCallback(bind(
            &Http::onTcpTimeout, &http, _1, _2));
    http.start();

    //set tcp session
    TcpSession tcpSession(&loop, tcpSessionAddress_[index]);
    ring.ipFragment().addTcpCallback(bind(
            &TcpSession::onTcpData, &tcpSession, _1, _2, _3));
    tcpSession.start();

    // tcp->packet counter
    ring.tcpFragment().addDataCallback(bind(
            &ProtocolPacketCounter::onTcpData, packetCounter_.get(), _1, _2));
    // udp->packet counter
    ring.udp().addUdpCallback(bind(
            &ProtocolPacketCounter::onUdpData, packetCounter_.get(), _1, _2, _3, _4));

    Https https(&loop, httpsAddress_);
    ring.tcpFragment().addDataCallback(std::bind(
            &Https::onTcpData, &https,  _1, _2, _3, _4, _5));

    loops_[index] = &loop;
    loop.loop();
    loops_[index] = nullptr;
}

void Noff::setAddress(std::vector<InetAddress>& vec, const InetAddress& addr, size_t n)
{
    auto ip = addr.toIp();
    uint16_t port = addr.toPort();

    for (size_t i = 0; i < n; ++i)
        vec.emplace_back(ip, port++);
}
