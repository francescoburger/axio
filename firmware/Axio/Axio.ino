#include <Arduino.h>
#include <esp_timer.h>
#include "src/core/BnoDevice.h"
#include "src/core/ImuManager.h"
#include "src/core/Scheduler.h"
#include "src/transport/LegacyTransport.h"

struct MonotonicClock {
  uint64_t nowUs() const { return uint64_t(esp_timer_get_time()); }
};
MonotonicClock monotonicClock;
axio::BnoDevice sensor;
axio::TrackerState tracker;
axio::Calibration calibration;
axio::ImuManager<axio::BnoDevice, MonotonicClock> imuManager(sensor, monotonicClock, tracker, calibration);
axio::Scheduler scheduler;

void calibrateReference() { imuManager.calibrateReference(); }

void setup() {
  setCpuFrequencyMhz(160);
  Serial.begin(115200);
  setupLegacyTransport(calibrateReference);
  imuManager.begin();
}

void loop() {
  processLegacyPortal();
  if (scheduler.due(uint32_t(millis())) && imuManager.acquire()) {
    const uint64_t start = monotonicClock.nowUs();
    sendLegacySample(tracker.latest);
    tracker.diagnostics.sent(monotonicClock.nowUs() - start);
  }
  checkIncomingOSC();
}
