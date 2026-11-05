#include <cassert>
#include <cstring>
#include "src/core/BnoDevice.h"
FakeWire Wire;
FakeSerial Serial;
unsigned long waited=0;
bool Adafruit_BNO055::present=true;
int Adafruit_BNO055::remap=-1, Adafruit_BNO055::sign=-1, Adafruit_BNO055::crystal=-1;
uint8_t Adafruit_I2CDevice::payload[8]={}, Adafruit_I2CDevice::lastRegister=0;
size_t Adafruit_I2CDevice::lastSize=0;
bool Adafruit_I2CDevice::ok=true, Adafruit_I2CDevice::begun=false;
void payload(int a,int b,int c,int d=0) {
  int values[]={a,b,c,d};
  for (int i=0;i<4;++i) {
    const uint16_t bits=uint16_t(values[i]);
    Adafruit_I2CDevice::payload[2*i]=bits & 255;
    Adafruit_I2CDevice::payload[2*i+1]=bits >> 8;
  }
}
int main() {
  axio::BnoDevice device;
  assert(device.begin());
  assert(Wire.sda==SDA_PIN && Wire.scl==SCL_PIN && waited==1000);
  assert(Adafruit_BNO055::remap==0x21 && Adafruit_BNO055::sign==0x04 && Adafruit_BNO055::crystal==1);
  imu::Quaternion q;
  payload(16384,-16384,32767,-32768);
  assert(device.readQuaternion(q));
  assert(q.w()==1 && q.x()==-1 && q.y()==32767/16384.0 && q.z()==-2);
  assert(Adafruit_I2CDevice::lastRegister==0x20 && Adafruit_I2CDevice::lastSize==8);
  imu::Vector<3> v;
  payload(-32768,32767,-1);
  assert(device.readEuler(v) && v.x()==-2048 && v.y()==32767/16.0 && v.z()==-1/16.0);
  assert(Adafruit_I2CDevice::lastRegister==0x1a && Adafruit_I2CDevice::lastSize==6);
  assert(device.readLinear(v) && v.x()==-32768/100.0 && v.y()==32767/100.0 && v.z()==-.01);
  assert(Adafruit_I2CDevice::lastRegister==0x28);
  assert(device.readGyro(v) && v.x()==-2048 && v.z()==-1/16.0);
  assert(Adafruit_I2CDevice::lastRegister==0x14);
  const double previous=v.x();
  Adafruit_I2CDevice::ok=false;
  assert(!device.readQuaternion(q) && q.w()==1);
  assert(!device.readEuler(v) && v.x()==previous);
  assert(!device.readLinear(v) && v.x()==previous);
  assert(!device.readGyro(v) && v.x()==previous);
  Adafruit_BNO055::present=false;
  waited=0;
  assert(!device.begin() && waited==0);
}
