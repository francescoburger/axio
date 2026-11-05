#pragma once
#include <Adafruit_BNO055.h>
#include <Adafruit_I2CDevice.h>
#include <Wire.h>
#include "../../hardware_config.h"
namespace axio {
// Same register transactions / conversion factors as Adafruit BNO055 1.6.4,
// but propagate read failures instead of converting zero-filled buffers.
class BnoDevice {
  static constexpr uint8_t address = BNO055_I2C_ADDRESS;
  Adafruit_BNO055 bno = Adafruit_BNO055(55, address);
  Adafruit_I2CDevice bus = Adafruit_I2CDevice(address, &Wire);
  static int16_t word(const uint8_t *p) {
    return int16_t(uint16_t(p[0]) | (uint16_t(p[1]) << 8));
  }
  bool vector(uint8_t reg, double divisor, imu::Vector<3> &result) {
    uint8_t bytes[6];
    if (!bus.write_then_read(&reg, 1, bytes, sizeof(bytes))) return false;
    result = imu::Vector<3>(double(word(bytes))/divisor,
        double(word(bytes+2))/divisor, double(word(bytes+4))/divisor);
    return true;
  }
 public:
  bool begin() {
    Wire.begin(SDA_PIN, SCL_PIN);
    Serial.println("Initializing BNO055...");
    if (!bno.begin()) {
      Serial.print("BNO055 not detected! Check wiring or I2C address.");
      return false;
    }
    delay(1000);
    bno.setAxisRemap(Adafruit_BNO055::REMAP_CONFIG_P0);
    bno.setAxisSign(Adafruit_BNO055::REMAP_SIGN_P0);
    bno.setExtCrystalUse(true);
    // begin(false) initializes the BusIO handle without an extra probe.
    bus.begin(false);
    Serial.println("BNO055 initialized successfully.");
    return true;
  }
  bool readQuaternion(imu::Quaternion &result) {
    uint8_t reg = Adafruit_BNO055::BNO055_QUATERNION_DATA_W_LSB_ADDR;
    uint8_t bytes[8];
    if (!bus.write_then_read(&reg, 1, bytes, sizeof(bytes))) return false;
    const double scale = (1.0 / (1 << 14));
    result = imu::Quaternion(scale*word(bytes), scale*word(bytes+2),
        scale*word(bytes+4), scale*word(bytes+6));
    return true;
  }
  bool readEuler(imu::Vector<3> &v) { return vector(Adafruit_BNO055::VECTOR_EULER, 16.0, v); }
  bool readLinear(imu::Vector<3> &v) { return vector(Adafruit_BNO055::VECTOR_LINEARACCEL, 100.0, v); }
  bool readGyro(imu::Vector<3> &v) { return vector(Adafruit_BNO055::VECTOR_GYROSCOPE, 16.0, v); }
};
}
