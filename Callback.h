//
// Created by root on 17-7-28.
//

#ifndef NOFF_CALLBACK_H
#define NOFF_CALLBACK_H

#include <functional>
#include <pfring.h>
#include <dpi/Util.h>
#include <muduo/base/ThreadLocalSingleton.h>
#include <muduo/base/Singleton.h>

#define threadInstance(Type) \
muduo::ThreadLocalSingleton<Type>::instance()

#define globalInstance(Type) \
muduo::Singleton<Type>::instance()

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

typedef std::function<void ()> ThreadFunc;

#endif //NOFF_CALLBACK_H
