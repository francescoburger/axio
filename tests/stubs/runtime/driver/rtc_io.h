#pragma once
#include "Arduino.h"
using gpio_num_t=int;
inline int rtc_gpio_pullup_en(gpio_num_t pin){
  if(pin!=4||fakeDeviceControl().failRtc)return -1;
  fakeDeviceControl().rtcPullup=true;return 0;
}
inline int rtc_gpio_pulldown_dis(gpio_num_t pin){
  if(pin!=4||fakeDeviceControl().failRtc)return -1;
  fakeDeviceControl().rtcPulldownDisabled=true;return 0;
}
inline int rtc_gpio_deinit(gpio_num_t pin){
  if(pin!=4)return -1;
  ++fakeDeviceControl().rtcDeinits;return 0;
}
