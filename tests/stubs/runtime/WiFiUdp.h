#pragma once
#include "WiFi.h"
#include <stddef.h>
extern bool failListener;
struct WiFiUDP {
  int begin(unsigned port){return failListener&&port==12345?0:1;}
  void stop(){}
  int beginPacket(IPAddress,unsigned){return 1;}
  size_t write(const uint8_t *,size_t size){return size;}
  int endPacket(){return 1;}
  int parsePacket(){return 0;}
  int read(uint8_t *,size_t){return 0;}
  void clear(){}
};
