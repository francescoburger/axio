#pragma once
#include "Arduino.h"
constexpr int ESP_GPIO_WAKEUP_GPIO_LOW=0;
inline int esp_deep_sleep_enable_gpio_wakeup(uint64_t mask,int mode){return mask==(uint64_t(1)<<5)&&mode==0?0:-1;}
inline void esp_deep_sleep_start(){++fakeDeviceControl().sleeps;}

inline int esp_sleep_get_wakeup_cause(){return 0;}
