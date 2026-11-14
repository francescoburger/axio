#include <Arduino.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_private/esp_clk.h>
#include <soc/rtc.h>
#include <soc/rtc_cntl_reg.h>
#include "../../hardware_config.h"
#include "WakeHold.h"
#include "FeedbackLedRuntime.h"
#if defined(ARDUINO_XIAO_ESP32S3)
#include <driver/rtc_io.h>
#endif

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
  // Every off path must arm only the physical button, never a timer or USB.
  if(esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL)!=ESP_OK)return false;
  pinMode(BUTTON_PIN,BUTTON_USE_PULLUP?INPUT_PULLUP:INPUT);
#if defined(ARDUINO_XIAO_ESP32S3)
  // EXT0 leaves the digital GPIO usable until sleep starts. Keep the RTC
  // pull-up active during sleep for the same button-to-GND circuit.
  const auto pin=static_cast<gpio_num_t>(BUTTON_PIN);
  if(rtc_gpio_pullup_en(pin)!=ESP_OK||rtc_gpio_pulldown_dis(pin)!=ESP_OK)return false;
  return esp_sleep_enable_ext0_wakeup(pin,BUTTON_ACTIVE_LEVEL)==ESP_OK;
#else
  // Keep LOW wake: the SDK automatically uses a pull-up for this mode.
  // HIGH wake would install a pull-down, so our button-to-GND circuit could
  // never signal release. PowerController waits for release after shutdown.
  return esp_deep_sleep_enable_gpio_wakeup(uint64_t(1)<<BUTTON_PIN,ESP_GPIO_WAKEUP_GPIO_LOW)==ESP_OK;
#endif
}

namespace {
[[noreturn]] void remainOff(){
  holdFeedbackLedOff();
  // LOW wake is level-triggered. A button held across power restoration (or
  // pressed again after a rejected wake) must be released before sleep.
  bool releaseSeen=false;
  uint64_t releasedAt=0;
  for(;;){
    const uint64_t now=uint64_t(esp_timer_get_time());
    if(digitalRead(BUTTON_PIN)==BUTTON_ACTIVE_LEVEL)releaseSeen=false;
    else {
      if(!releaseSeen){releaseSeen=true;releasedAt=now;}
      if(now-releasedAt>=100000)break;
    }
    delay(1);
  }
  if(prepareButtonSleep())esp_deep_sleep_start();
  // Configuration failure must never fall through into application setup or
  // repeatedly reset. Recovery requires an external reset after fixing it.
  for(;;)delay(1000);
}
}

void qualifyButtonWake(){
  const bool fromSleep=esp_reset_reason()==ESP_RST_DEEPSLEEP;
#if defined(ARDUINO_XIAO_ESP32S3)
  const bool fromButton=esp_sleep_get_wakeup_cause()==ESP_SLEEP_WAKEUP_EXT0;
  rtc_gpio_deinit(static_cast<gpio_num_t>(BUTTON_PIN));
#else
  const bool fromButton=esp_sleep_get_wakeup_cause()==ESP_SLEEP_WAKEUP_GPIO;
#endif
  pinMode(BUTTON_PIN,BUTTON_USE_PULLUP?INPUT_PULLUP:INPUT);
  // Off is the default after power loss, brownout, USB, software or watchdog
  // reset. RTC ticks are used only after a genuine button deep-sleep wake;
  // no persisted ON flag can accidentally restart the device after discharge.
  if(!fromSleep||!fromButton)remainOff();
  axio::WakeHold hold;
  const uint32_t period=esp_clk_slowclk_cal_get();
  for(;;){
    const uint64_t elapsed=rtc_time_slowclk_to_us(rtc_time_get()-buttonWakeTicks,period);
    const auto result=hold.tick(elapsed,digitalRead(BUTTON_PIN)==BUTTON_ACTIVE_LEVEL);
    if(result==axio::WakeHold::Accepted)return;
    if(result==axio::WakeHold::Rejected)remainOff();
    delay(1);
  }
}
