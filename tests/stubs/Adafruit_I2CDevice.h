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
  bool write(const uint8_t *data,size_t size) {
    assert(size==2 && begun);lastRegister=data[0];lastSize=size;
    if(ok&&(data[0]==0x3d||data[0]==0x3e))powerRegister(data[0])=data[1];
    return ok;
  }
  bool write_then_read(const uint8_t *reg, size_t n, uint8_t *out, size_t size) {
    assert(n==1 && begun);
    lastRegister=*reg; lastSize=size;
    if (!ok) return false;
    if(size==1&&(*reg==0x3d||*reg==0x3e)){*out=powerRegister(*reg);return true;}
    std::memcpy(out,payload,size); return true;
  }
 private:
  static uint8_t &powerRegister(uint8_t reg){static uint8_t op=12,power=0;return reg==0x3d?op:power;}
};
