#pragma once
#include <stdint.h>
constexpr int WL_CONNECTED=3;
struct FakeWiFi {int status(){return WL_CONNECTED;}};
extern FakeWiFi WiFi;
struct IPAddress {uint8_t bytes[4];IPAddress(uint8_t a,uint8_t b,uint8_t c,uint8_t d):bytes{a,b,c,d}{}};
