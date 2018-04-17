//
// Created by root on 17-5-25.
//

#include <array>
#include <boost/any.hpp>

#include "TcpSession.h"

enum {
    upFlowSize = 0,
    upPacketNum,
    upSyn, upFin, upPsh, upRst,
    upSmallPacketNum,
    upRxmit,
    upRtt,

    downFlowSize,
    downPacketNum,
    downSyn, downFin, downPsh, downRst,
    downSmallPacketNum,
    downRxmit,
    downRtt,

    totalFieldNum
};

struct Seq
{
    uint32_t data_ = 0;
    bool valid_ = false;

    Seq(uint32_t data)
            : data_(data)
            , valid_(true)
    {}

    Seq() {}

    bool valid() const { return valid_; }
    void set(uint32_t data)
    {
        data_ = data;
        valid_ = true;
    }
};

bool operator==(Seq lhs, Seq rhs) { return lhs.data_ == rhs.data_; }
bool operator< (Seq lhs, Seq rhs) { return (int32_t)(lhs.data_ - rhs.data_) <  0; }
bool operator<=(Seq lhs, Seq rhs) { return (int32_t)(lhs.data_ - rhs.data_) <= 0; }
bool operator> (Seq lhs, Seq rhs) { return (int32_t)(lhs.data_ - rhs.data_) >  0; }
bool operator>=(Seq lhs, Seq rhs) { return (int32_t)(lhs.data_ - rhs.data_) >= 0; }

struct SessionData
{
    tuple4  t4_;
    timeval startTime_;
    timeval endTime_;
    Seq     upSeq_;
    Seq     downSeq_;

    // for rtt
    timeval upTimestamp_;
    timeval downTimestamp_;
    Seq oldUpSeq;
    Seq oldDownSeq;
    bool waitingUpAck = false;
    bool waitingDownAck = false;

    std::array<int, totalFieldNum> info = {};

    bool normallyClosed_ = true;

    boost::any context_;

    SessionData(tuple4 t4, timeval timeStamp) :
            t4_(t4), startTime_(timeStamp)
    {
        std::fill(info.begin(), info.end(), 0);
    }
};

std::string to_string(const SessionData& data)
{
    using std::string;
    using std::to_string;

    string ret = to_string(data.t4_);
    for (int c : data.info)
    {
        ret.append("\t");
        ret.append(std::to_string(c));
    }
    ret.append("\t");
    ret.append(to_string(data.endTime_.tv_sec - data.startTime_.tv_sec));
    ret.append("\t");
    ret.append(to_string(data.normallyClosed_));

    return ret.append("\n");
}

void TcpSession::onTcpData(ip *iphdr, int, timeval timestamp)
{
    int len = ntohs(iphdr->ip_len);

    len -= 4 * iphdr->ip_hl;
    if (len < (int)sizeof(tcphdr)) {
        return;
    }

    tcphdr *tcp = (tcphdr *)((u_char*)iphdr + 4 * iphdr->ip_hl);
    if (len < 4 * tcp->th_off) {
        len = 0;
    }
    else {
        len -= 4 * tcp->th_off;
    }


    tuple4 t4(htons(tcp->source),
              htons(tcp->dest),
              iphdr->ip_src.s_addr,
              iphdr->ip_dst.s_addr);

    /* check timeout */
    if (timer_.checkTime(timestamp)) {
        onTimer();
    }

    // printf("%lu\n", sessionDataMap_.size());

    u_int8_t flag = tcp->th_flags;
    auto it = sessionDataMap_.find(t4);

    if (it == sessionDataMap_.end()) {
        /* first packet */
        if (flag & TH_SYN) {
            auto p = sessionDataMap_.insert({t4, std::make_shared<SessionData>(t4, timestamp)});
            onConnection(t4, p.first->second);
            updateSession(t4, *tcp, timestamp, p.first->second, len);
        }
        return;
    }

    SessionDataPtr& ptr = it->second;
    EntryPtr entry = getEntryPtr(ptr);

    /* dead connection */
    if (entry == NULL) {
        /* another connection */
        if (flag & TH_SYN) {
            *ptr = SessionData(t4, timestamp);
            onConnection(t4, ptr);
            updateSession(t4, *tcp, timestamp, ptr, len);
        }
        else {
            /* keep size small */
            sessionDataMap_.erase(it);
        }
        return;
    }

    /* close connection */
    if ((flag & TH_FIN) || (flag & TH_RST)) {
        ptr->endTime_ = timestamp;
        updateSession(t4, *tcp, timestamp, ptr, len);
        server_.send(to_string(*ptr));
        sessionDataMap_.erase(it);
        return;
    }

    /* message, may have ACK+SYN */
    sessionBuckets_.back().insert(entry);
    updateSession(t4, *tcp, timestamp, ptr, len);
}

void TcpSession::onTimer()
{
    sessionBuckets_.push_back(Bucket());
}

void TcpSession::onConnection(const tuple4& t4, SessionDataPtr& dataPtr)
{
    EntryPtr entry(new Entry(dataPtr, *this));
    WeakEntryPtr weak(entry);

    sessionBuckets_.back().insert(std::move(entry));
    dataPtr->context_ = std::move(weak);
}

void TcpSession::onTimeOut(const SessionDataPtr& dataPtr)
{
    dataPtr->normallyClosed_ = false;
    dataPtr->endTime_ = timer_.lastCheckTime();
//    for (auto &cb : callbacks_) {
//        cb(*dataPtr);
//    }
    server_.send(to_string(*dataPtr));

    sessionDataMap_.erase(dataPtr->t4_);
}

void TcpSession::updateSession(const tuple4& t4, const tcphdr& hdr, timeval timestamp, SessionDataPtr& dataPtr, int len)
{
    auto flag = hdr.th_flags;
    Seq dataSeq(ntohl(hdr.seq));
    Seq ackSeq(ntohl(hdr.ack_seq));

    if (t4 == dataPtr->t4_) {
        /* up */

        /* update simple filed */
        dataPtr->info[upFlowSize] += len;
        dataPtr->info[upPacketNum] += 1;
        if (flag & TH_SYN)  ++dataPtr->info[upSyn];
        if (flag & TH_FIN)  ++dataPtr->info[upFin];
        if (flag & TH_PUSH) ++dataPtr->info[upPsh];
        if (flag & TH_RST)  ++dataPtr->info[upRst];
        dataPtr->info[upSmallPacketNum] += (len <= 64);

        /* update retransmit */
        if (!dataPtr->upSeq_.valid()) {
            dataPtr->upSeq_ = dataSeq;
        }
        else if (dataPtr->upSeq_ < dataSeq && len > 0) {
            dataPtr->upSeq_ = dataSeq;
        }
        else if (len > 0) {
            ++dataPtr->info[upRxmit];
        }

        /* update rtt */
        if (!dataPtr->waitingDownAck && (len > 0 || (flag & TH_SYN)))
        {
            dataPtr->upTimestamp_ = timestamp;
            dataPtr->oldUpSeq = dataSeq;
            dataPtr->waitingDownAck = true;
        }
        if (dataPtr->waitingUpAck && ackSeq > dataPtr->oldDownSeq)
        {
            auto seconds = timestamp.tv_sec - dataPtr->downTimestamp_.tv_sec;
            auto useconds = timestamp.tv_usec - dataPtr->downTimestamp_.tv_usec;

            auto& rtt = dataPtr->info[downRtt];
            if (rtt <= 0)
                rtt = seconds * 1000 + useconds / 1000;
            else
                rtt = rtt * 4 / 5 + (seconds * 1000 + useconds / 1000) / 5;
            dataPtr->waitingUpAck = false;
        }
    }
    else {
        /* down */
        dataPtr->info[downFlowSize] += len;
        dataPtr->info[downPacketNum] += 1;
        if (flag & TH_SYN)  ++dataPtr->info[downSyn];
        if (flag & TH_FIN)  ++dataPtr->info[downFin];
        if (flag & TH_PUSH) ++dataPtr->info[downPsh];
        if (flag & TH_RST)  ++dataPtr->info[downRst];
        dataPtr->info[downSmallPacketNum] += (len <= 64);

        // retransmit
        if (!dataPtr->downSeq_.valid()) {
            dataPtr->downSeq_ = dataSeq;
        }
        else if (dataPtr->downSeq_ < dataSeq && len > 0) {
            dataPtr->downSeq_ = dataSeq;
        }
        else if (len > 0) {
            ++dataPtr->info[downRxmit];
        }

        // rtt
        if (!dataPtr->waitingUpAck && (len > 0 || (flag & TH_SYN))) {
            dataPtr->downTimestamp_ = timestamp;
            dataPtr->oldDownSeq = dataSeq;
            dataPtr->waitingUpAck = true;
        }
        if (dataPtr->waitingDownAck && ackSeq > dataPtr->oldUpSeq) {
            auto seconds = timestamp.tv_sec - dataPtr->upTimestamp_.tv_sec;
            auto useconds = timestamp.tv_usec - dataPtr->upTimestamp_.tv_usec;

            auto& rtt = dataPtr->info[upRtt];
            if (rtt <= 0)
                rtt = seconds * 1000 + useconds / 1000;
            else
                rtt = rtt * 4 / 5 + (seconds * 1000 + useconds / 1000) / 5;
            dataPtr->waitingDownAck = false;
        }
    }
}

TcpSession::Entry::~Entry()
{
    SessionDataPtr ptr = weak_.lock();
    if (ptr != NULL) {
        tcpSession_.onTimeOut(ptr);
    }
}

TcpSession::EntryPtr TcpSession::getEntryPtr(SessionDataPtr &dataPtr)
{
    assert(!dataPtr->context_.empty());
    WeakEntryPtr weakEntry(boost::any_cast<WeakEntryPtr>(dataPtr->context_));
    return weakEntry.lock();
}