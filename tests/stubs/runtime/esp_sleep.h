#pragma once
#include "Arduino.h"
#include "esp_wifi.h"
constexpr int ESP_GPIO_WAKEUP_GPIO_LOW=0,ESP_GPIO_WAKEUP_GPIO_HIGH=1,ESP_SLEEP_WAKEUP_GPIO=7;
struct FakeSleep {};
inline int esp_deep_sleep_enable_gpio_wakeup(uint64_t mask,int mode){
  if(mask!=(uint64_t(1)<<5)||(mode!=0&&mode!=1)||fakeDeviceControl().failWake)return -1;
  fakeDeviceControl().wakeMode=mode;return 0;
}
inline void esp_deep_sleep_start(){++fakeDeviceControl().sleeps;if(fakeDeviceControl().throwOnSleep)throw FakeSleep();}
inline int esp_sleep_get_wakeup_cause(){return fakeDeviceControl().wakeCause;}
inline void esp_default_wake_deep_sleep(){}
