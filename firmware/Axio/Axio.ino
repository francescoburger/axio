#include <Arduino.h>
#include <esp_timer.h>
#include "src/core/BnoDevice.h"
#include "src/core/ImuManager.h"
#include "src/core/Scheduler.h"
#include "src/transport/LegacyTransport.h"
#include "src/transport/OscRuntime.h"
#include "src/connectivity/ConnectivityRuntime.h"
#include "src/diagnostics/UsbDiagnostics.h"
#include "src/web/DashboardRuntime.h"
#include "src/power/WakeHold.h"
#include "src/power/FeedbackLedRuntime.h"
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
void referenceCalibrated(){calibratedFeedbackLed(uint32_t(millis()));}
axio::ImuManager<axio::BnoDevice, MonotonicClock> imuManager(sensor, monotonicClock, tracker, calibration,referenceCalibrated);
axio::Scheduler scheduler;

void calibrateReference() { imuManager.calibrateReference(); }
struct DiagnosticPlatform {
  uint64_t nowUs() { return monotonicClock.nowUs(); }
  axio::RuntimeStatus status() { return oscRuntimeStatus(); }
  bool configCommand(const char *id,char *command,char *out) {return connectivityCommand(id,command,out)||oscConfigCommand(id,command,out);}
  void connectivityJson(char *out,size_t cap) {::connectivityJson(out,cap);}
  void resetMetrics() {resetOscMetrics();}
  void metricsJson(char *out,size_t cap) {oscMetricsJson(out,cap);}
  bool calibrate() { return dashboardImuAllowed()&&calibrateOscRuntime(); }
} diagnosticPlatform;
axio::DiagnosticService<DiagnosticPlatform> diagnosticService(tracker, diagnosticPlatform);
axio::UsbDiagnostics<decltype(diagnosticService)> usbDiagnostics(diagnosticService);

void setup() {
  setupFeedbackLed();
  qualifyButtonWake();
  setCpuFrequencyMhz(160);
  prepareDashboardFlashInfo();
  Serial.setRxBufferSize(decltype(usbDiagnostics)::RxCapacity);
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  setupLegacyTransport(calibrateReference);
  imuManager.begin();
  setupConnectivity();
  setupOscRuntime([]()->bool { return imuManager.calibrateReference(); });
  setupDashboard(tracker,sensor,[](){diagnosticService.resetMetricsNow();},[]()->uint64_t{return diagnosticService.windowUs();});
  diagnosticService.startWindow();
  startFeedbackLed(uint32_t(millis()));
}

void loop() {
  if(dashboardNetworkAllowed())tickConnectivity();
  if (dashboardImuAllowed() && scheduler.due(uint32_t(millis()))) {
    const uint64_t acquireStart = monotonicClock.nowUs();
    if (imuManager.acquire()) {
      const uint64_t start = monotonicClock.nowUs();
      auto &d = tracker.diagnostics;
      d.lastProcessUs = start - acquireStart - d.lastReadUs;
      if (d.lastProcessUs > d.maxProcessUs) d.maxProcessUs = d.lastProcessUs;
      tickOscRuntime(tracker.latest, start);
      d.sent(monotonicClock.nowUs() - start);
    }
  }
  if(dashboardImuAllowed())pollOscRuntime();
  usbDiagnostics.poll(Serial, uint32_t(millis()));
  tickDashboard(scheduler.remaining(uint32_t(millis()))>=3);
}
