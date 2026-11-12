#include <cassert>
#include "Arduino.h"
#include "hardware_config.h"
#include "src/power/FeedbackLedRuntime.h"

int main(){
  auto &hw=fakeDeviceControl();
  setupFeedbackLed();
  assert(hw.ledOutput&&hw.ledLevel==!FEEDBACK_LED_ACTIVE_LEVEL);
  assert(!hw.ledHeld&&!hw.deepSleepHold&&hw.ledOnTransitions==0);
  startFeedbackLed(0);assert(hw.ledLevel==FEEDBACK_LED_ACTIVE_LEVEL);
  tickFeedbackLed(499);assert(hw.ledOnTransitions==1);
  tickFeedbackLed(500);assert(hw.ledLevel==!FEEDBACK_LED_ACTIVE_LEVEL);
  tickFeedbackLed(600);
  calibratedFeedbackLed(1000);
  for(unsigned t=1100;t<=1400;t+=100)tickFeedbackLed(t);
  assert(hw.ledOnTransitions==3&&hw.ledLevel==!FEEDBACK_LED_ACTIVE_LEVEL);
  beginSleepFeedbackLed(2000);assert(!sleepFeedbackLedReady());
  for(unsigned t=2100;t<=2600;t+=100)tickFeedbackLed(t);
  assert(sleepFeedbackLedReady()&&hw.ledOnTransitions==6);
  holdFeedbackLedOff();assert(hw.ledLevel==!FEEDBACK_LED_ACTIVE_LEVEL&&hw.ledHeld&&hw.deepSleepHold);
  // The next boot restores the inactive output before releasing the hold.
  setupFeedbackLed();assert(hw.ledOnTransitions==6&&!hw.ledHeld&&!hw.deepSleepHold);
  // Recovery clears a shutdown signal and allows later calibration feedback.
  beginSleepFeedbackLed(3000);cancelSleepFeedbackLed();
  tickFeedbackLed(4000);assert(hw.ledLevel==!FEEDBACK_LED_ACTIVE_LEVEL);
  assert(!sleepFeedbackLedReady());
  calibratedFeedbackLed(4100);assert(hw.ledLevel==FEEDBACK_LED_ACTIVE_LEVEL);
}
