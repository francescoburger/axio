#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cstring>
#include <cassert>
#include "Wire.h"
class Adafruit_I2CDevice {
 public:
  static uint8_t payload[8], lastRegister;
  static size_t lastSize;
  static bool ok, begun;
  Adafruit_I2CDevice(uint8_t address, FakeWire *wire) { assert(address==0x29 && wire==&Wire); }
  bool begin(bool probe) { assert(!probe); begun=true; return true; }
  bool write_then_read(const uint8_t *reg, size_t n, uint8_t *out, size_t size) {
    assert(n==1 && begun);
    lastRegister=*reg; lastSize=size;
    if (!ok) return false;
    std::memcpy(out,payload,size); return true;
  }
};
