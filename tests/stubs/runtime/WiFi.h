#pragma once
#include <stdint.h>
#include <string>
constexpr int WL_CONNECTED=3,WIFI_STA=1,WIFI_AP=2;
struct IPAddress {
  uint8_t bytes[4];IPAddress(uint8_t a,uint8_t b,uint8_t c,uint8_t d):bytes{a,b,c,d}{}
  uint8_t operator[](unsigned i)const{return bytes[i];}
  operator uint32_t()const{return uint32_t(bytes[0])|uint32_t(bytes[1])<<8|uint32_t(bytes[2])<<16|uint32_t(bytes[3])<<24;}
};
struct FakeAP {bool up=false;bool started(){return up;}};
struct FakeSTA {bool begin(bool){return true;}bool config(){return true;}};
struct FakeWiFi {FakeAP AP;int mode=WIFI_STA;bool failApStart=false,failApStop=false;unsigned apStarts=0,apStops=0;
  bool enableSTA(bool on){mode=on?WIFI_STA:0;return true;}
  IPAddress gatewayIP(){return IPAddress(127,0,0,1);}IPAddress subnetMask(){return IPAddress(255,0,0,0);}int RSSI(){return -58;}
  int getMode(){return mode;}bool enableAP(bool enabled){if(enabled&&failApStart)return false;if(!enabled&&failApStop)return false;if(enabled){mode|=WIFI_AP;AP.up=true;++apStarts;}else {mode&=~WIFI_AP;AP.up=false;++apStops;}return true;}
  IPAddress softAPIP(){return AP.up?IPAddress(192,168,4,1):IPAddress(0,0,0,0);}
  std::string hostname;bool setHostname(const char *name){hostname=name;return true;}
  FakeSTA STA;bool setAutoReconnect(bool){return true;}std::string SSID(){return "test-ssid";}int link=WL_CONNECTED;IPAddress ip{192,168,1,15};int status(){return link;}IPAddress localIP(){return ip;}};
extern FakeWiFi WiFi;
inline uint32_t &fakeMillis(){static uint32_t now=10000;return now;}
inline uint32_t millis(){return fakeMillis();}
