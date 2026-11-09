#include <Arduino.h>
#include <esp_sleep.h>
#include <esp_private/esp_clk.h>
#include <soc/rtc.h>
#include <soc/rtc_cntl_reg.h>
#include "../../hardware_config.h"
#include "WakeHold.h"

// RTC memory survives deep sleep; no NVS writes. The stub runs before the
// bootloader, so bootloader/Arduino startup time contributes to the hold.
static RTC_DATA_ATTR uint64_t buttonWakeTicks=0;
extern "C" void RTC_IRAM_ATTR esp_wake_deep_sleep(){
  // Same hardware latch as rtc_time_get(), but no call into ordinary IRAM:
  // only RTC memory and ROM are available before esp_default_wake_deep_sleep.
  REG_SET_BIT(RTC_CNTL_TIME_UPDATE_REG,RTC_CNTL_TIME_UPDATE);
  const uint32_t low=REG_READ(RTC_CNTL_TIME0_REG);
  const uint32_t high=REG_READ(RTC_CNTL_TIME1_REG);
  buttonWakeTicks=(uint64_t(high)<<32)|low;
  esp_default_wake_deep_sleep();
}

bool prepareButtonSleep(){
  pinMode(BUTTON_PIN,BUTTON_USE_PULLUP?INPUT_PULLUP:INPUT);
  // Keep LOW wake: the SDK automatically uses a pull-up for this mode.
  // HIGH wake would install a pull-down, so our button-to-GND circuit could
  // never signal release. PowerController waits for release after shutdown.
  return esp_deep_sleep_enable_gpio_wakeup(uint64_t(1)<<BUTTON_PIN,ESP_GPIO_WAKEUP_GPIO_LOW)==ESP_OK;
}

void qualifyButtonWake(){
  if(esp_sleep_get_wakeup_cause()!=ESP_SLEEP_WAKEUP_GPIO)return;
  pinMode(BUTTON_PIN,BUTTON_USE_PULLUP?INPUT_PULLUP:INPUT);
  axio::WakeHold hold;
  const uint32_t period=esp_clk_slowclk_cal_get();
  for(;;){
    const uint64_t elapsed=rtc_time_slowclk_to_us(rtc_time_get()-buttonWakeTicks,period);
    const auto result=hold.tick(elapsed,digitalRead(BUTTON_PIN)==BUTTON_ACTIVE_LEVEL);
    if(result==axio::WakeHold::Accepted)return;
    if(result==axio::WakeHold::Rejected){
      // The BNO remains suspended: this path precedes IMU/Wi-Fi/USB setup.
      if(esp_deep_sleep_enable_gpio_wakeup(uint64_t(1)<<BUTTON_PIN,ESP_GPIO_WAKEUP_GPIO_LOW)==ESP_OK)
        esp_deep_sleep_start();
      // Fail closed if wake configuration fails: do not activate the device.
    }
    delay(1);
  }
}
