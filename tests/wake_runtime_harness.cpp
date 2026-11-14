#include <cassert>
#define AXIO_WAKE_TEST_CLOCK
// Compile the production wake path against fake GPIO/RTC/sleep registers.
#include "src/power/WakeRuntime.cpp"
void expectSleep(){
  bool slept=false;
  try{qualifyButtonWake();}catch(const FakeSleep &){slept=true;}
  assert(slept);
  auto &hw=fakeDeviceControl();
  assert(hw.button==1&&hw.wakeMode==0&&!hw.extraWake);
  assert(hw.ledOnTransitions==0&&hw.ledLevel==!FakeLedActive&&hw.ledHeld&&hw.deepSleepHold);
}
#if defined(ARDUINO_XIAO_ESP32S3)
constexpr int ButtonWake=ESP_SLEEP_WAKEUP_EXT0;
#else
constexpr int ButtonWake=ESP_SLEEP_WAKEUP_GPIO;
#endif
void resetHardware(){
  fakeDeviceControl()=FakeDeviceControl();
  fakeDeviceControl().throwOnSleep=true;
  setupFeedbackLed();
}
struct FailClosedWait {};
int main(){
  resetHardware();
  auto &hw=fakeDeviceControl();hw.throwOnSleep=true;
  // LOW wake preserves the SDK pull-up for the button-to-GND circuit.
  // Even a still-held button must never select HIGH/pull-down wake.
  hw.button=0;assert(prepareButtonSleep());assert(hw.wakeMode==0);
  hw.button=1;assert(prepareButtonSleep());assert(hw.wakeMode==0);
  hw.failWake=true;assert(!prepareButtonSleep());hw.failWake=false;
  hw.extraWake=true;assert(prepareButtonSleep());assert(!hw.extraWake);
  hw.failDisableWake=true;assert(!prepareButtonSleep());hw.failDisableWake=false;
  // The next deliberate hold starts the device, counting boot elapsed time.
#if defined(ARDUINO_XIAO_ESP32S3)
  assert(hw.rtcPullup&&hw.rtcPulldownDisabled);
  hw.failRtc=true;assert(!prepareButtonSleep());hw.failRtc=false;
#endif
  hw.wakeCause=ButtonWake;hw.resetReason=ESP_RST_DEEPSLEEP;
  hw.button=0;
  hw.rtcUs=1000000;esp_wake_deep_sleep();
  hw.rtcUs+=axio::WakeHold::HoldUs;qualifyButtonWake();
#if defined(ARDUINO_XIAO_ESP32S3)
  assert(hw.rtcDeinits==1);
  assert(hw.ledOnTransitions==0); // No startup cue during wake qualification.
#endif
  // Released before threshold returns to sleep with the same LOW wake mode.
  hw.rtcUs=5000000;esp_wake_deep_sleep();hw.rtcUs+=100000;hw.button=1;
  expectSleep();assert(hw.wakeMode==0);
  assert(hw.ledOnTransitions==0&&hw.ledLevel==!FakeLedActive&&hw.ledHeld&&hw.deepSleepHold);
  // All cold/reset boots stay off, even with stale RTC data or wake cause.
  // Power-on includes battery recovery and USB reconnect; no flash flag is
  // needed to distinguish previously running from deliberately sleeping.
  for(int reason: {ESP_RST_UNKNOWN,ESP_RST_POWERON,ESP_RST_EXT,ESP_RST_SW,
                  ESP_RST_PANIC,ESP_RST_INT_WDT,ESP_RST_TASK_WDT,ESP_RST_WDT,
                  ESP_RST_BROWNOUT,ESP_RST_SDIO,ESP_RST_USB,ESP_RST_JTAG}){
    for(int cause: {ESP_SLEEP_WAKEUP_UNDEFINED,ButtonWake,ESP_SLEEP_WAKEUP_TIMER}){
      resetHardware();hw.resetReason=reason;hw.wakeCause=cause;
      hw.rtcUs=0;buttonWakeTicks=UINT64_MAX;hw.extraWake=true;
      expectSleep();assert(hw.sleeps==1&&hw.restarts==0);
      // The next physical long press can wake after every reset/power loss.
      hw.resetReason=ESP_RST_DEEPSLEEP;hw.wakeCause=ButtonWake;hw.button=0;
      esp_wake_deep_sleep();hw.rtcUs+=axio::WakeHold::HoldUs;
      qualifyButtonWake();assert(hw.sleeps==1);
    }
  }
  // A different deep-sleep wake source must not start the application either.
  for(int cause: {ESP_SLEEP_WAKEUP_UNDEFINED,ESP_SLEEP_WAKEUP_TIMER}){
    resetHardware();hw.resetReason=ESP_RST_DEEPSLEEP;hw.wakeCause=cause;
    expectSleep();
  }
  // Held across power restoration: no immediate LOW-wake loop. Release,
  // including bounce, must be stable for 100 ms, then a fresh press is needed.
  resetHardware();hw.button=0;
  hw.onDelay=[](unsigned long){
    auto &c=fakeDeviceControl();
    c.button=c.rtcUs<200000||(c.rtcUs>=220000&&c.rtcUs<230000)?0:1;
  };
  expectSleep();assert(hw.rtcUs>=330000&&hw.sleeps==1);
  // Re-press during short-wake rejection also waits for release before sleep.
  resetHardware();hw.resetReason=ESP_RST_DEEPSLEEP;hw.wakeCause=ButtonWake;
  hw.rtcUs=5000000;esp_wake_deep_sleep();hw.rtcUs+=100000;
  hw.onDelay=[](unsigned long){
    auto &c=fakeDeviceControl();c.button=c.rtcUs>=5145000&&c.rtcUs<5200000?0:1;
  };
  expectSleep();assert(hw.rtcUs>=5300000&&hw.sleeps==1);
  // A qualification loop with a still-held button accepts at the old threshold.
  resetHardware();hw.resetReason=ESP_RST_DEEPSLEEP;hw.wakeCause=ButtonWake;
  hw.button=0;esp_wake_deep_sleep();hw.rtcUs=100000;
  qualifyButtonWake();assert(hw.rtcUs==axio::WakeHold::HoldUs&&hw.sleeps==0);
  // Wake configuration errors stay closed without retries, resets or fallthrough.
  for(int failure=0;failure<3;++failure){
    resetHardware();hw.failWake=failure==0;hw.failDisableWake=failure==1;
#if defined(ARDUINO_XIAO_ESP32S3)
    hw.failRtc=failure==2;
#else
    if(failure==2)continue;
#endif
    hw.onDelay=[](unsigned long ms){if(ms==1000)throw FailClosedWait();};
    bool closed=false;
    try{qualifyButtonWake();}catch(const FailClosedWait &){closed=true;}
    assert(closed&&hw.sleeps==0&&hw.restarts==0&&hw.wakeDisables==1);
    assert(hw.ledOnTransitions==0&&hw.ledHeld);
  }
}
