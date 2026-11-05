#include <cassert>
#include <cmath>
#include <vector>
#include "src/core/ImuManager.h"
#include "src/core/Scheduler.h"
struct Clock {
  uint64_t t = (uint64_t(1)<<32) + 123;
  uint64_t nowUs() { t += 100; return t; }
};
struct Device {
  bool present=true;
  int fail=-1, calls=0;
  std::vector<int> order;
  bool begin() { return present; }
  bool read(int kind) { order.push_back(kind); return calls++ != fail; }
  bool readQuaternion(imu::Quaternion &v) { v=imu::Quaternion(2,0,0,0); return read(0); }
  bool readEuler(imu::Vector<3> &v) { v=imu::Vector<3>(359.9375,45.5,-90.5); return read(1); }
  bool readLinear(imu::Vector<3> &v) { v=imu::Vector<3>(35,-70,17.5); return read(2); }
  bool readGyro(imu::Vector<3> &v) { v=imu::Vector<3>(1.99,-1.99,.99); return read(3); }
};
int main() {
  axio::Calibration calibration;
  assert(calibration.quatOffset.w()==1 && calibration.yawOffset==0);
  Clock clock;
  Device device;
  axio::TrackerState state;
  axio::ImuManager<Device,Clock> manager(device,clock,state,calibration);
  assert(!state.latest.valid && state.sensor==axio::SensorStatus::Uninitialized);
  assert(!manager.acquire());
  assert(!manager.calibrateReference() && device.calls==0);
  // A first failed acquisition must not create any apparently valid sample.
  Device broken;
  broken.fail=0;
  axio::TrackerState empty;
  axio::ImuManager<Device,Clock> brokenManager(broken,clock,empty,calibration);
  brokenManager.begin();
  assert(!brokenManager.acquire() && !empty.latest.valid && empty.latest.sequence==0);
  manager.begin();
  assert(state.sensor==axio::SensorStatus::Ready);
  assert(manager.acquire());
  assert((device.order==std::vector<int>{0,1,2,3}));
  const auto first=state.latest;
  assert(first.valid && first.sequence==1 && first.timestampUs==clock.t);
  assert(first.timestampUs>(uint64_t(1)<<32));
  assert(first.raw.linear.y()==-70 && first.axReal==1 && first.ayReal==-2);
  assert(first.pitch==.5f && first.roll==.5f && first.q0==2);
  assert(first.gx==float(1/1500.0) && first.gy==float(-1/1500.0) && first.gz==0);
  assert(state.diagnostics.lastReadUs==100 && state.diagnostics.acquired==1);
  assert(manager.calibrateReference());
  assert(calibration.quatOffset.w()==2 && calibration.yawOffset==359);
  assert(manager.acquire() && state.latest.q0==4 && state.latest.yaw==0);
  // Fail every position of the four-block acquisition. Never publish partial data.
  for (int k=0;k<4;++k) {
    const auto previous=state.latest;
    device.fail=device.calls+k;
    assert(!manager.acquire());
    assert(state.sensor==axio::SensorStatus::ReadError);
    assert(state.latest.sequence==previous.sequence);
    assert(state.latest.timestampUs==previous.timestampUs);
    assert(state.latest.raw.gyro.x()==previous.raw.gyro.x());
    assert(state.latest.valid); // explicitly LAST valid, current status is error
  }
  assert(state.diagnostics.readErrors==4 && state.diagnostics.acquired==2);
  device.fail=-1;
  assert(manager.acquire() && state.latest.sequence==3);
  assert(state.sensor==axio::SensorStatus::Ready);
  for (int k=0;k<2;++k) {
    device.fail=device.calls+k;
    assert(!manager.calibrateReference());
    assert(calibration.quatOffset.w()==2 && calibration.yawOffset==359);
  }
  calibration.calibrateReference(imu::Quaternion(.5,.1,.2,.3),imu::Vector<3>(-.9375,0,0));
  assert(calibration.quatOffset.w()==.5 && calibration.quatOffset.x()==-.1);
  assert(calibration.yawOffset==0);
  Device absent; absent.present=false;
  axio::TrackerState failed;
  axio::ImuManager<Device,Clock> missing(absent,clock,failed,calibration);
  missing.begin();
  assert(!missing.acquire() && !missing.calibrateReference() && absent.calls==0);
  assert(failed.sensor==axio::SensorStatus::InitFailed && failed.diagnostics.initErrors==1);
  axio::Scheduler scheduler;
  assert(!scheduler.due(0) && !scheduler.due(9));
  assert(scheduler.due(10) && !scheduler.due(19) && scheduler.due(20));
  assert(scheduler.due(105) && !scheduler.due(105) && !scheduler.due(114));
  assert(scheduler.due(0xfffffff8u));
  assert(!scheduler.due(1) && scheduler.due(2) && !scheduler.due(3));
  state.diagnostics.sent(50); state.diagnostics.sent(20);
  assert(state.diagnostics.sendAttempts==2 && state.diagnostics.maxSendUs==50);
}
