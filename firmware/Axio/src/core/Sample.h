#pragma once
#include <stdint.h>
#include <utility/imumaths.h>

namespace axio {
struct RawSample {
  imu::Quaternion quat;
  imu::Vector<3> euler, linear, gyro;
};
// Complete sequential read, not an atomic BNO055 fusion snapshot.
struct Sample {
  RawSample raw;
  imu::Quaternion quatRelative;
  float q0=0, q1=0, q2=0, q3=0;
  float yaw=0, pitch=0, roll=0;
  float axReal=0, ayReal=0, azReal=0;
  float axWorld=0, ayWorld=0, azWorld=0;
  float gx=0, gy=0, gz=0;
  uint64_t timestampUs=0, sequence=0;
  bool valid=false;
};
}
