#pragma once
#include <stdint.h>
namespace axio {
// User-provided 1/2 divider formula; no voltage-to-SoC curve is assumed.
// One corrected ADC conversion per service interval, not sixteen in a burst.
class Battery {
  uint32_t sum=0,last=0;uint8_t count=0;bool started=false;
 public:
  bool available=false;float voltage=0;
  template<class Adc> void tick(uint32_t now,Adc readMilliVolts) {
    if(started && uint32_t(now-last)<63)return;
    started=true;last=now;sum+=readMilliVolts();
    if(++count==16){voltage=2*sum/16/1000.0;available=true;sum=0;count=0;}
  }
};
}
