/*
//
// Created by root on 17-4-21.
//

#include <unistd.h>
#include <string.h>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <arpa/inet.h>
#include <signal.h>
#include <memory>

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Timestamp.h>
#include <muduo/base/Atomic.h>
#include <dpi/Udp.h>
#include <app/header/TcpSession.h>


#include "PFCapture.h"
#include "IpFragment.h"
#include "TcpFragment.h"
#include "mac/MacCount.h"
#include "protocol/ProtocolPacketCounter.h"
#include "http/Http.h"
#include "dns/Dnsparser.h"
#include "header/TcpHeader.h"
#include "UdpClient.h"
#include "NoffServer.h"
#include "header/TcpSession.h"

using namespace std;

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;
using std::placeholders::_5;

#define USE_TCP

#ifdef USE_TCP
typedef NoffServer Client;
#else
typedef UdpClient Client;
#endif

typedef PFCapture Cap;

unique_ptr<Cap>      cap;
unique_ptr<Client>   httpRequestOutput ;
unique_ptr<Client>   httpResponseOutput;
unique_ptr<Client>   dnsRequestOutput;
unique_ptr<Client>   dnsResponseOutput;
unique_ptr<Client>   tcpHeaderOutput;
unique_ptr<Client>   macCounterOutput;
unique_ptr<Client>   packetCounterOutput;
unique_ptr<Client>   tcpSessionOutPut;

void sigHandler(int)
{
    assert(cap != NULL);
    cap->breakLoop();
#ifdef USE_TCP
    NoffServer::breakLoop();
#endif
}

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
            &Client::onDataPointer<HttpRequest>, httpRequestOutput.get(), _1));

    // http response->udp client
    http.addHttpResponseCallback(bind(
            &Client::onDataPointer<HttpResponse>, httpResponseOutput.get(), _1));
}

void setPacketCounterInThread()
{
    assert(packetCounterOutput != NULL);

    auto& udp = threadInstance(Udp);
    auto& tcp = threadInstance(TcpFragment);
    auto& counter = globalInstance(ProtocolPacketCounter);

    // tcp->packet counter
     tcp.addDataCallback(bind(
            &ProtocolPacketCounter::onTcpData, &counter, _1, _2));

    // udp->packet counter
    udp.addUdpCallback(bind(
            &ProtocolPacketCounter::onUdpData, &counter, _1, _2, _3, _4));
    // packet->udp output
    counter.setCounterCallback(bind(
            &Client::onData<CounterDetail>, packetCounterOutput.get(), _1));
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
            &Client::onData<DnsRequest>,
            dnsRequestOutput.get(), _1));

    dns.addResponsecallback(bind(
            &Client::onData<DnsResponse>,
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

    // tcp->packet counter->udp output
    setPacketCounterInThread();

    // udp->dns>udp
    setDnsCounterInThread();

    // tcp->udp
    setTcpHeaderInThread();

    setTcpSessionInThread();
}

int main(int argc, char **argv)
{
    int opt;
    char dev[32] = "empty";
    char ip_addr[16] = "127.0.0.1";
    uint16_t port = 10666;
    uint16_t port2 = 30001;

    muduo::Logger::setLogLevel(muduo::Logger::INFO);

    while ((opt = getopt(argc, argv, "i:j:h:")) != -1) {
        switch (opt) {
            case 'i':
                if (strlen(optarg) >= 31) {
                    LOG_ERROR << "device name too long";
                    exit(1);
                }
                strcpy(dev, optarg);
                break;
            case 'h':
                if (strlen(optarg) >= 16) {
                    LOG_ERROR << "IP address too long";
                    exit(1);
                }
                strcpy(ip_addr, optarg);
                break;
            default:
                LOG_ERROR << "usage: [-i interface] [-h ip]";
                exit(1);
        }
    }

    //define the udp client
    httpRequestOutput.reset(new Client({ip_addr, port++}, "http request"));
    httpResponseOutput.reset(new Client({ip_addr, port++}, "http response"));
    dnsRequestOutput.reset(new Client({ip_addr, port++}, "dns request"));
    dnsResponseOutput.reset(new Client({ip_addr, port++}, "dns response"));

    tcpHeaderOutput.reset(new Client({ip_addr, port2++}, "tcp header"));
    macCounterOutput.reset(new Client({ip_addr, port2++}, "mac counter"));
    packetCounterOutput.reset(new Client({ip_addr, port2++}, "packet counter"));

    tcpSessionOutPut.reset(new Client({ip_addr, port2++}, "tcp session"));

    cap.reset(new Cap(dev));
    cap->setFilter("ip");


    signal(SIGINT, sigHandler);

//     pcap->mac counter->udp
     setMacCounterInThread();

    cap->setThreadInitCallback([](){
        initInThread();
    });
    cap->startLoop();

#ifdef USE_TCP
    NoffServer::startLoop();
#else
    pause();
#endif
}
*/

#include <muduo/net/EventLoop.h>
#include <signal.h>

#include "Noff.h"

using muduo::net::EventLoop;
using muduo::Logger;

EventLoop* g_loop;

void sigHandler(int)
{
    g_loop->quit();
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        printf("usage: ./noff interface");
        return 0;
    }

    muduo::Logger::setLogLevel(muduo::Logger::INFO);

    EventLoop loop;
    g_loop = &loop;

    InetAddress dnsRequestAddr(30100);
    InetAddress dnsResponseAddr(30200);
    InetAddress httpRequestAddr(30300);
    InetAddress httpResponseAddr(30400);
    InetAddress tcpHeaderAddr(30500);
    InetAddress tcpSessionAddr(30600);
    InetAddress packetCounterAddr(30700);
    InetAddress httpsAddr(30800);

    LOG_INFO << "dns request port " << dnsRequestAddr.toPort();
    LOG_INFO << "dns response port " << dnsResponseAddr.toPort();
    LOG_INFO << "http request port " << httpRequestAddr.toPort();
    LOG_INFO << "http response port " << httpResponseAddr.toPort();
    LOG_INFO << "tcp header port " << tcpHeaderAddr.toPort();
    LOG_INFO << "tcp session port " << tcpSessionAddr.toPort();
    LOG_INFO << "packet counter port " << packetCounterAddr.toPort();
    LOG_INFO << "https packet port " << httpsAddr.toPort();

    Noff noff(&loop, argv[1]);
    noff.setDnsTopic(dnsRequestAddr, dnsResponseAddr);
    noff.setHttpTopic(httpRequestAddr, httpResponseAddr);
    noff.setTcpHeaderTopic(tcpHeaderAddr);
    noff.setTcpSessionTopic(tcpSessionAddr);
    noff.setPacketCounterTopic(packetCounterAddr);
    noff.setHttpsTopic(httpsAddr);
    noff.start();

    signal(SIGINT, sigHandler);
    loop.loop();
}