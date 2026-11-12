#include <cassert>
#include "src/power/FeedbackLed.h"

int main(){
  axio::FeedbackLed led;
  assert(!led.on()&&!led.sleepReady());
  led.startup(0);assert(led.on());
  led.tick(499);assert(led.on());
  led.tick(500);assert(!led.on());
  led.tick(600);assert(!led.on()&&!led.sleepReady());
  // Each successful calibration is a double flash, then the idle level.
  led.calibrated(1000);assert(led.on());
  for(unsigned phase=1;phase<=4;++phase){
    led.tick(1000+phase*100-1);assert(led.on()==(phase%2==1));
    led.tick(1000+phase*100);assert(led.on()==(phase<4&&phase%2==0));
  }
  // A calibration during startup waits for the complete long pulse and gap.
  led.startup(2000);led.calibrated(2100);led.calibrated(2200);
  led.tick(2499);assert(led.on());led.tick(2500);assert(!led.on());
  led.tick(2600);assert(led.on());
  for(unsigned t=2700;t<=3000;t+=100)led.tick(t);
  assert(!led.on());led.tick(3100);assert(!led.on());
  // Shutdown cancels pending calibration, ignores new requests and stays off
  // after exactly three pulses until explicitly reset by recovery/startup.
  led.calibrated(4000);led.calibrated(4001);led.shutdown(4010);
  led.calibrated(4011);
  for(unsigned phase=1;phase<=6;++phase){
    assert(!led.sleepReady());
    led.tick(4010+phase*100);
    assert(led.on()==(phase<6&&phase%2==0));
  }
  assert(!led.on()&&led.sleepReady());
  led.calibrated(5000);led.tick(6000);assert(!led.on()&&led.sleepReady());
  led.reset();assert(!led.on()&&!led.sleepReady());
  led.calibrated(6100);assert(led.on());
  // Delayed servicing never replays missed edges in a tight loop.
  led.shutdown(10000);led.tick(20000);assert(!led.on()&&!led.sleepReady());
  led.tick(20000);assert(!led.on());led.tick(20100);assert(led.on());
  // millis rollover works for both the long pulse and the short pulses.
  led.startup(0xffffff00u);led.tick(243);assert(led.on());
  led.tick(244);assert(!led.on());led.tick(344);assert(!led.on());
  led.shutdown(0xffffffd0u);
  for(unsigned phase=1;phase<=6;++phase)led.tick(uint32_t(0xffffffd0u+phase*100));
  assert(led.sleepReady()&&!led.on());
}
