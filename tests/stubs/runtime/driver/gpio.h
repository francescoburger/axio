#pragma once
#include <cassert>
#include "Arduino.h"
using gpio_num_t=int;
inline int gpio_set_level(gpio_num_t pin,int level){assert(pin==FakeLedPin);digitalWrite(pin,level);return 0;}
inline int gpio_hold_en(gpio_num_t pin){
  auto &hw=fakeDeviceControl();assert(pin==FakeLedPin&&hw.ledOutput&&hw.ledLevel==!FakeLedActive);
  hw.ledHeld=true;return 0;
}
inline int gpio_hold_dis(gpio_num_t pin){
  auto &hw=fakeDeviceControl();assert(pin==FakeLedPin&&hw.ledOutput&&hw.ledLevel==!FakeLedActive);
  hw.ledHeld=false;return 0;
}
inline void gpio_deep_sleep_hold_en(){fakeDeviceControl().deepSleepHold=true;}
inline void gpio_deep_sleep_hold_dis(){fakeDeviceControl().deepSleepHold=false;}
