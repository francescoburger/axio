#pragma once
#include "Calibration.h"
#include "TrackerState.h"
namespace axio {
// Device and clock are minimal compile-time seams, with no virtual dispatch.
template<class Device, class Clock> class ImuManager {
  Device &device;
  Clock &clock;
  TrackerState &state;
  Calibration &calibration;
  bool initialized = false;
  void (*onCalibrated)()=nullptr;
 public:
  ImuManager(Device &d, Clock &c, TrackerState &s, Calibration &t,void (*notify)()=nullptr)
      : device(d), clock(c), state(s), calibration(t),onCalibrated(notify) {}
  void begin() {
    initialized = device.begin();
    state.sensor = initialized ? SensorStatus::Ready : SensorStatus::InitFailed;
    if (!initialized) ++state.diagnostics.initErrors;
  }
  bool acquire() {
    if (!initialized) return false;
    const uint64_t start = clock.nowUs();
    state.diagnostics.beginRead(start);
    RawSample raw;
    const bool ok = device.readQuaternion(raw.quat) &&
        device.readEuler(raw.euler) && device.readLinear(raw.linear) &&
        device.readGyro(raw.gyro);
    const uint64_t end = clock.nowUs();
    state.diagnostics.endRead(end - start);
    if (!ok) { state.readFailed(); return false; }
    Sample sample;
    sample.raw = raw;
    calibration.apply(raw, sample);
    state.publish(sample, end);
    return true;
  }
  // Single entry point for all command sources. No automatic calibration.
  bool calibrateReference() {
    ++state.diagnostics.calibrationRequests;
    imu::Quaternion quat;
    imu::Vector<3> euler;
    if (!initialized || !device.readQuaternion(quat) || !device.readEuler(euler)) {
      ++state.diagnostics.calibrationErrors;
      if (initialized) state.readFailed();
      return false;
    }
    calibration.calibrateReference(quat, euler);
    if(onCalibrated)onCalibrated();
    return true;
  }
};
}
