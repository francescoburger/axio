#pragma once
#include "WiFi.h"
#include <stddef.h>
#include <map>
extern bool failListener;
struct UdpState {bool failAll=false;unsigned sends=0,polls=0,begins=0;std::map<const void *,uint32_t> bound;};
inline UdpState &fakeUdp(){static UdpState s;return s;}
struct WiFiUDP {
  int begin(unsigned port){return begin(IPAddress(0,0,0,0),port);}
  int begin(IPAddress ip,unsigned port){if(fakeUdp().failAll||(failListener&&port==12345))return 0;fakeUdp().bound[this]=uint32_t(ip);++fakeUdp().begins;return 1;}
  void stop(){fakeUdp().bound.erase(this);}
  int beginPacket(IPAddress,unsigned){return 1;}
  size_t write(const uint8_t *,size_t size){return size;}
  int endPacket(){++fakeUdp().sends;return 1;}
  int parsePacket(){++fakeUdp().polls;return 0;}
  int read(uint8_t *,size_t){return 0;}
  void clear(){}
};
