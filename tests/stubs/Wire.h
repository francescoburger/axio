#pragma once
struct FakeWire {
  int sda=-1, scl=-1;
  void begin(int a, int b) { sda=a; scl=b; }
};
extern FakeWire Wire;
struct FakeSerial {
  void print(const char*) {}
  void println(const char*) {}
};
extern FakeSerial Serial;
extern unsigned long waited;
#ifndef AXIO_FAKE_RUNTIME_ARDUINO
inline void delay(unsigned long ms) { waited+=ms; }
#endif
