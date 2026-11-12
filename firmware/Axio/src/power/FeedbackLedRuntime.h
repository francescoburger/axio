#pragma once
#include <stdint.h>

void setupFeedbackLed();
void startFeedbackLed(uint32_t now);
void calibratedFeedbackLed(uint32_t now);
void tickFeedbackLed(uint32_t now);
void beginSleepFeedbackLed(uint32_t now);
bool sleepFeedbackLedReady();
void cancelSleepFeedbackLed();
void holdFeedbackLedOff();
