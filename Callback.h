//
// Created by root on 17-7-28.
//

#ifndef NOFF_CALLBACK_H
#define NOFF_CALLBACK_H

#include <functional>
#include <dpi/Util.h>

struct pfring_pkthdr;
typedef std::function<void(const pfring_pkthdr*, const u_char*, timeval)> PacketCallback;
typedef std::function<void(ip *, int, timeval)> IpFragmentCallback;

typedef std::function<void(ip*,int,timeval)>    IpCallback;
typedef std::function<void(ip*,int,timeval)>    IpUdpCallback;
typedef std::function<void(ip*, int,timeval)>   IpTcpCallback;
typedef std::function<void(ip*,int,timeval)>    IcmpCallback;

struct TcpStream;
typedef std::function<void(TcpStream*,timeval)> TcpCallback;
typedef std::function<void(TcpStream*,timeval,u_char*,int,int)> DataCallback;

typedef std::function<void(tuple4,char*,int,timeval)> UdpCallback;

#endif //NOFF_CALLBACK_H
