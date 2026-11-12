#include <Arduino.h>
#include "../../hardware_config.h"
#include "FeedbackLed.h"
#include "FeedbackLedRuntime.h"
#if FEEDBACK_LED_PIN >= 0
#include <driver/gpio.h>
namespace {
axio::FeedbackLed feedback;
bool lit=false;
void output(){
  const bool next=feedback.on();
  if(next!=lit){digitalWrite(FEEDBACK_LED_PIN,next?FEEDBACK_LED_ACTIVE_LEVEL:!FEEDBACK_LED_ACTIVE_LEVEL);lit=next;}
}
}
#endif

void setupFeedbackLed(){
#if FEEDBACK_LED_PIN >= 0
  feedback.reset();lit=false;
  // Restore the inactive output before releasing the previous sleep hold.
  gpio_set_level(static_cast<gpio_num_t>(FEEDBACK_LED_PIN),!FEEDBACK_LED_ACTIVE_LEVEL);
  pinMode(FEEDBACK_LED_PIN,OUTPUT);
  gpio_hold_dis(static_cast<gpio_num_t>(FEEDBACK_LED_PIN));
  gpio_deep_sleep_hold_dis();
  digitalWrite(FEEDBACK_LED_PIN,!FEEDBACK_LED_ACTIVE_LEVEL);
#endif
}
void startFeedbackLed(uint32_t now){
#if FEEDBACK_LED_PIN >= 0
  feedback.startup(now);output();
#else
  (void)now;
#endif
}
void calibratedFeedbackLed(uint32_t now){
#if FEEDBACK_LED_PIN >= 0
  feedback.calibrated(now);output();
#else
  (void)now;
#endif
}
void tickFeedbackLed(uint32_t now){
#if FEEDBACK_LED_PIN >= 0
  feedback.tick(now);output();
#else
  (void)now;
#endif
}
void beginSleepFeedbackLed(uint32_t now){
#if FEEDBACK_LED_PIN >= 0
  feedback.shutdown(now);output();
#else
  (void)now;
#endif
}
bool sleepFeedbackLedReady(){
#if FEEDBACK_LED_PIN >= 0
  return feedback.sleepReady();
#else
  return true;
#endif
}
void cancelSleepFeedbackLed(){
#if FEEDBACK_LED_PIN >= 0
  feedback.reset();output();
#endif
}
void holdFeedbackLedOff(){
#if FEEDBACK_LED_PIN >= 0
  cancelSleepFeedbackLed();
  gpio_hold_en(static_cast<gpio_num_t>(FEEDBACK_LED_PIN));
  gpio_deep_sleep_hold_en();
#endif
}
