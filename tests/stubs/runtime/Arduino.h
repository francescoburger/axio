#pragma once
#define AXIO_FAKE_RUNTIME_ARDUINO
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#ifndef INPUT
#define INPUT 0
#define INPUT_PULLUP 2
#endif
#define RTC_DATA_ATTR
#define RTC_IRAM_ATTR
struct FakeDeviceControl {
  int button=1,wakeMode=-1,wakeCause=0;unsigned adc=1950,restarts=0,sleeps=0;
  bool failWake=false,throwOnSleep=false;uint64_t rtcUs=0;
};
inline FakeDeviceControl &fakeDeviceControl(){static FakeDeviceControl c;return c;}
inline void pinMode(int,int){}
inline int digitalRead(int){return fakeDeviceControl().button;}
inline unsigned analogReadMilliVolts(int){return fakeDeviceControl().adc;}
struct FakeESP {
  unsigned getFreeHeap(){return 100000;}unsigned getMinFreeHeap(){return 50000;}
  unsigned getSketchSize(){return 1145629;}unsigned getFreeSketchSpace(){return 1310720;}
  void restart(){++fakeDeviceControl().restarts;}
};
extern FakeESP ESP;

inline void delay(unsigned long ms){fakeDeviceControl().rtcUs+=uint64_t(ms)*1000;}
