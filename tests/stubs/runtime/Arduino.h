#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#ifndef INPUT
#define INPUT 0
#define INPUT_PULLUP 2
#endif
struct FakeDeviceControl {int button=1;unsigned adc=1950,restarts=0,sleeps=0;};
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
