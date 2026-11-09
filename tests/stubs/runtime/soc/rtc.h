#pragma once
#include "Arduino.h"
inline uint64_t rtc_time_get(){return fakeDeviceControl().rtcUs;}
inline uint64_t rtc_time_slowclk_to_us(uint64_t ticks,unsigned){return ticks;}
