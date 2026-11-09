#pragma once
#include "Sample.h"
namespace axio {
class Calibration {
 public:
  imu::Quaternion quatOffset = imu::Quaternion(1, 0, 0, 0);
  int yawOffset = 0;
  void calibrateReference(const imu::Quaternion &quat, const imu::Vector<3> &euler) {
    quatOffset = quat.conjugate();
    yawOffset = euler.x();
  }
  void apply(const RawSample &raw, Sample &sample) const {
    float &q0 = sample.q0;
    float &q1 = sample.q1;
    float &q2 = sample.q2;
    float &q3 = sample.q3;
    float &yaw = sample.yaw;
    float &pitch = sample.pitch;
    float &roll = sample.roll;
    float &axReal = sample.axReal;
    float &ayReal = sample.ayReal;
    float &azReal = sample.azReal;
    float &axWorld = sample.axWorld;
    float &ayWorld = sample.ayWorld;
    float &azWorld = sample.azWorld;
    float &gx = sample.gx;
    float &gy = sample.gy;
    float &gz = sample.gz;
    imu::Quaternion &quatRelative = sample.quatRelative;
    // --- Quaternion (relative to calibration reference) ---

    imu::Quaternion quat = raw.quat;
    quatRelative = quatOffset * quat;
    q0 = quatRelative.w();
    q1 = quatRelative.x();
    q2 = quatRelative.y();
    q3 = quatRelative.z();

    // --- Euler Angles ---
    // Preserve V1.1 casts, remainder and operation order; no clamping.
    imu::Vector<3> euler = raw.euler;
    yaw   = ((((int)euler.x() - yawOffset + 360 + 180) % 360) / 360.0) * 2 - 1;
    roll  = -1 * (int)euler.z() / 180.0;
    pitch = (int)euler.y() / 90.0;

    // --- Linear Acceleration (sensor frame, gravity removed) ---
    imu::Vector<3> linear = raw.linear;
    axReal = linear.x();
    ayReal = linear.y();
    azReal = linear.z();

    // Rotate linear acceleration into the world frame
    computeWorldAccel(sample);

    // Historical divisor is 35.0, without clamping.
    axReal  /= 35.0;
    ayReal  /= 35.0;
    azReal  /= 35.0;
    axWorld /= 35.0;
    ayWorld /= 35.0;
    azWorld /= 35.0;

    // Raw getVector convention: degrees/s in Adafruit BNO055 1.6.4.
    imu::Vector<3> gyro = raw.gyro;
    gx = int(gyro.x()) / 1500.0;
    gy = int(gyro.y()) / 1500.0;
    gz = int(gyro.z()) / 1500.0;

  }
 private:
  static void computeWorldAccel(Sample &sample) {
    float &q0 = sample.q0;
    float &q1 = sample.q1;
    float &q2 = sample.q2;
    float &q3 = sample.q3;
    float &axReal = sample.axReal;
    float &ayReal = sample.ayReal;
    float &azReal = sample.azReal;
    float &axWorld = sample.axWorld;
    float &ayWorld = sample.ayWorld;
    float &azWorld = sample.azWorld;
    axWorld = (1 - 2 * q2 * q2 - 2 * q3 * q3) * axReal + (2 * q1 * q2 - 2 * q0 * q3) *
              ayReal + (2 * q1 * q3 + 2 * q0 * q2) * azReal;

    ayWorld = (2 * q1 * q2 + 2 * q0 * q3) * axReal + (1 - 2 * q1 * q1 - 2 * q3 * q3) *
              ayReal + (2 * q2 * q3 - 2 * q0 * q1) * azReal;

    azWorld = (2 * q1 * q3 - 2 * q0 * q2) * axReal + (2 * q2 * q3 + 2 * q0 * q1) *
              ayReal + (1 - 2 * q1 * q1 - 2 * q2 * q2) * azReal;
  }
};
}
