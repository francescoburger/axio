#pragma once
#define AXIO_FAKE_RUNTIME_ARDUINO
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#ifndef INPUT
#define INPUT 0
#define INPUT_PULLUP 2
#define OUTPUT 1
#endif
#define RTC_DATA_ATTR
#define RTC_IRAM_ATTR
#if defined(ARDUINO_XIAO_ESP32S3)
constexpr int FakeLedPin=21,FakeLedActive=0;
#else
constexpr int FakeLedPin=3,FakeLedActive=1;
#endif
struct FakeDeviceControl {
  int button=1,wakeMode=-1,wakeCause=0;unsigned adc=1950,restarts=0,sleeps=0;
  bool failWake=false,throwOnSleep=false,failRtc=false,rtcPullup=false,rtcPulldownDisabled=false;
  unsigned rtcDeinits=0;uint64_t rtcUs=0;
  int ledLevel=!FakeLedActive;bool ledOutput=false,ledHeld=false,deepSleepHold=false;
  unsigned ledWrites=0,ledOnTransitions=0;
};
inline FakeDeviceControl &fakeDeviceControl(){static FakeDeviceControl c;return c;}
inline void pinMode(int pin,int mode){if(pin==FakeLedPin)fakeDeviceControl().ledOutput=mode==OUTPUT;}
inline void digitalWrite(int pin,int level){
  if(pin!=FakeLedPin)return;
  auto &hw=fakeDeviceControl();++hw.ledWrites;
  if(hw.ledLevel!=level&&level==FakeLedActive)++hw.ledOnTransitions;
  hw.ledLevel=level;
}
inline int digitalRead(int){return fakeDeviceControl().button;}
inline unsigned analogReadMilliVolts(int){return fakeDeviceControl().adc;}
struct FakeESP {
  unsigned getFreeHeap(){return 100000;}unsigned getMinFreeHeap(){return 50000;}
  unsigned getSketchSize(){return 1145629;}unsigned getFreeSketchSpace(){return 1310720;}
  void restart(){++fakeDeviceControl().restarts;}
};
extern FakeESP ESP;

inline void delay(unsigned long ms){fakeDeviceControl().rtcUs+=uint64_t(ms)*1000;}
