#pragma once
#include "WiFi.h"
#include <stddef.h>
#include <map>
#include <deque>
#include <vector>
#include <algorithm>
extern bool failListener;
struct UdpState {
  struct Incoming {uint16_t port;std::vector<uint8_t> bytes;};
  bool failAll=false;unsigned sends=0,polls=0,begins=0;
  std::map<const void *,uint32_t> bound;
  std::deque<Incoming> incoming;
  std::vector<uint8_t> lastSent;
};
inline UdpState &fakeUdp(){static UdpState s;return s;}
struct WiFiUDP {
  uint16_t localPort=0;std::vector<uint8_t> rx,tx;
  int begin(unsigned port){return begin(IPAddress(0,0,0,0),port);}
  int begin(IPAddress ip,unsigned port){if(fakeUdp().failAll||(failListener&&port==12345))return 0;localPort=port;fakeUdp().bound[this]=uint32_t(ip);++fakeUdp().begins;return 1;}
  void stop(){fakeUdp().bound.erase(this);localPort=0;rx.clear();}
  int beginPacket(IPAddress,unsigned){tx.clear();return 1;}
  size_t write(const uint8_t *data,size_t size){tx.insert(tx.end(),data,data+size);return size;}
  int endPacket(){++fakeUdp().sends;fakeUdp().lastSent=tx;return 1;}
  int parsePacket(){
    ++fakeUdp().polls;if(!rx.empty()||!localPort)return 0;
    for(auto it=fakeUdp().incoming.begin();it!=fakeUdp().incoming.end();++it){
      if(it->port==localPort){rx=it->bytes;fakeUdp().incoming.erase(it);return int(rx.size());}
    }
    return 0;
  }
  int read(uint8_t *out,size_t size){size=std::min(size,rx.size());std::copy(rx.begin(),rx.begin()+size,out);rx.erase(rx.begin(),rx.begin()+size);return int(size);}
  void clear(){rx.clear();}
};
