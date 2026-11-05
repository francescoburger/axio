#include <Arduino.h>
#include <esp_timer.h>
#include "src/core/BnoDevice.h"
#include "src/core/ImuManager.h"
#include "src/core/Scheduler.h"
#include "src/transport/LegacyTransport.h"
#include "src/diagnostics/UsbDiagnostics.h"
#if !ARDUINO_USB_CDC_ON_BOOT
#error "Axio USB diagnostics require USB CDC On Boot: Enabled"
#endif

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
struct DiagnosticPlatform {
  uint64_t nowUs() { return monotonicClock.nowUs(); }
  axio::RuntimeStatus status() { return legacyRuntimeStatus(); }
  bool tare() { return imuManager.calibrateReference(); }
} diagnosticPlatform;
axio::DiagnosticService<DiagnosticPlatform> diagnosticService(tracker, diagnosticPlatform);
axio::UsbDiagnostics<decltype(diagnosticService)> usbDiagnostics(diagnosticService);

void setup() {
  setCpuFrequencyMhz(160);
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  setupLegacyTransport(calibrateReference);
  imuManager.begin();
}

void loop() {
  processLegacyPortal();
  if (scheduler.due(uint32_t(millis()))) {
    const uint64_t acquireStart = monotonicClock.nowUs();
    if (imuManager.acquire()) {
      const uint64_t start = monotonicClock.nowUs();
      auto &d = tracker.diagnostics;
      d.lastProcessUs = start - acquireStart - d.lastReadUs;
      if (d.lastProcessUs > d.maxProcessUs) d.maxProcessUs = d.lastProcessUs;
      sendLegacySample(tracker.latest);
      d.sent(monotonicClock.nowUs() - start);
    }
  }
  checkIncomingOSC();
  usbDiagnostics.poll(Serial, uint32_t(millis()));
}
