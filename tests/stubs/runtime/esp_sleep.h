#pragma once
#include "Arduino.h"
#include "esp_wifi.h"
struct FakeSleep {};
constexpr int ESP_SLEEP_WAKEUP_ALL=1,ESP_SLEEP_WAKEUP_TIMER=4,ESP_SLEEP_WAKEUP_UNDEFINED=0;
inline int esp_sleep_disable_wakeup_source(int source){
  auto &hw=fakeDeviceControl();++hw.wakeDisables;
  if(source!=ESP_SLEEP_WAKEUP_ALL||hw.failDisableWake)return -1;
  hw.extraWake=false;hw.wakeMode=-1;return ESP_OK;
}
#if defined(ARDUINO_XIAO_ESP32S3)
constexpr int ESP_SLEEP_WAKEUP_EXT0=2;
inline int esp_sleep_enable_ext0_wakeup(int pin,int level){
  if(pin!=4||level!=0||fakeDeviceControl().failWake)return -1;
  fakeDeviceControl().wakeMode=level;return 0;
}
#else
constexpr int ESP_GPIO_WAKEUP_GPIO_LOW=0,ESP_GPIO_WAKEUP_GPIO_HIGH=1,ESP_SLEEP_WAKEUP_GPIO=7;
inline int esp_deep_sleep_enable_gpio_wakeup(uint64_t mask,int mode){
  if(mask!=(uint64_t(1)<<5)||(mode!=0&&mode!=1)||fakeDeviceControl().failWake)return -1;
  fakeDeviceControl().wakeMode=mode;return 0;
}
#endif
inline void esp_deep_sleep_start(){++fakeDeviceControl().sleeps;if(fakeDeviceControl().throwOnSleep)throw FakeSleep();}
inline int esp_sleep_get_wakeup_cause(){return fakeDeviceControl().wakeCause;}
inline void esp_default_wake_deep_sleep(){}
