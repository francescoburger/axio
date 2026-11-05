#pragma once
#include <stdint.h>
#include <utility/imumaths.h>
class Adafruit_BNO055 {
 public:
  enum { BNO055_QUATERNION_DATA_W_LSB_ADDR=0x20,
    VECTOR_EULER=0x1a, VECTOR_LINEARACCEL=0x28, VECTOR_GYROSCOPE=0x14,
    REMAP_CONFIG_P0=0x21, REMAP_SIGN_P0=0x04 };
  static bool present;
  static int remap, sign, crystal;
  Adafruit_BNO055(int, uint8_t) {}
  bool begin() { return present; }
  void setAxisRemap(int v) { remap=v; }
  void setAxisSign(int v) { sign=v; }
  void setExtCrystalUse(bool v) { crystal=v; }
};
