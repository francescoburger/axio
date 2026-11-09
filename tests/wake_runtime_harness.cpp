#include <cassert>
// Compile the production wake path against fake GPIO/RTC/sleep registers.
#include "src/power/WakeRuntime.cpp"
void expectSleep(){
  bool slept=false;
  try{qualifyButtonWake();}catch(const FakeSleep &){slept=true;}
  assert(slept);
}
int main(){
  auto &hw=fakeDeviceControl();hw.throwOnSleep=true;
  // LOW wake preserves the SDK pull-up for the button-to-GND circuit.
  // Even a still-held button must never select HIGH/pull-down wake.
  hw.button=0;assert(prepareButtonSleep());assert(hw.wakeMode==0);
  hw.button=1;assert(prepareButtonSleep());assert(hw.wakeMode==0);
  hw.failWake=true;assert(!prepareButtonSleep());hw.failWake=false;
  // The next deliberate hold starts the device, counting boot elapsed time.
  hw.wakeCause=ESP_SLEEP_WAKEUP_GPIO;hw.button=0;
  hw.rtcUs=1000000;esp_wake_deep_sleep();
  hw.rtcUs+=axio::WakeHold::HoldUs;qualifyButtonWake();
  // Released before threshold returns to sleep with the same LOW wake mode.
  hw.rtcUs=5000000;esp_wake_deep_sleep();hw.rtcUs+=100000;hw.button=1;
  expectSleep();assert(hw.wakeMode==0);
  // A reset/power-on does not run the hold gate.
  hw.wakeCause=0;hw.rtcUs=0;qualifyButtonWake();
}
