#include <iostream>
#include <iomanip>
#include "src/core/Calibration.h"
int main() {
  axio::Calibration calibration;
  double v[20];
  std::cout << std::setprecision(17);
  while (std::cin >> v[0]) {
    for (int i=1; i<20; ++i) if (!(std::cin >> v[i])) return 2;
    calibration.calibrateReference(imu::Quaternion(v[0],v[1],v[2],v[3]),
                                  imu::Vector<3>(v[4],v[5],v[6]));
    axio::RawSample raw;
    raw.quat = imu::Quaternion(v[7],v[8],v[9],v[10]);
    raw.euler = imu::Vector<3>(v[11],v[12],v[13]);
    raw.linear = imu::Vector<3>(v[14],v[15],v[16]);
    raw.gyro = imu::Vector<3>(v[17],v[18],v[19]);
    axio::Sample s;
    calibration.apply(raw, s);
    double result[] = {calibration.quatOffset.w(),calibration.quatOffset.x(),
      calibration.quatOffset.y(),calibration.quatOffset.z(),double(calibration.yawOffset),
      s.q0,s.q1,s.q2,s.q3,s.yaw,s.pitch,s.roll,s.axReal,s.ayReal,s.azReal,
      s.axWorld,s.ayWorld,s.azWorld,s.gx,s.gy,s.gz};
    for (double value:result) std::cout << value << ' ';
    std::cout << '\n';
  }
}
