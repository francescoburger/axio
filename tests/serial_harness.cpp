#include <cassert>
#include <iostream>
#include <string>
#include <algorithm>
#include "src/diagnostics/DiagnosticService.h"
#include "src/diagnostics/UsbDiagnostics.h"
#include "src/core/ImuManager.h"
struct Clock { uint64_t nowUs() { return 1000000; } };
struct Device {
  bool valid=true;
  bool begin() { return valid; }
  bool readQuaternion(imu::Quaternion &v) { v=imu::Quaternion(.5,.5,.5,.5); return valid; }
  bool readEuler(imu::Vector<3> &v) { v=imu::Vector<3>(123.75,0,0); return valid; }
  bool readLinear(imu::Vector<3> &) { return valid; }
  bool readGyro(imu::Vector<3> &) { return valid; }
};
struct Platform {
  Clock clock; Device device;
  axio::TrackerState tracker;
  axio::Calibration calibration;
  axio::ImuManager<Device,Clock> manager;
  Platform():manager(device,clock,tracker,calibration) { manager.begin(); manager.acquire(); }
  uint64_t nowUs() { return clock.nowUs(); }
  axio::RuntimeStatus status() { axio::RuntimeStatus s; s.heap=12345; return s; }
  bool tare() { return manager.calibrateReference(); }
};
struct Port {
  std::string input,output;
  size_t position=0;
  int space=0;
  int available() { return int(input.size()-position); }
  int read() { return input[position++]; }
  int availableForWrite() { return space; }
  size_t write(const uint8_t *data,size_t n) {
    assert(n<=64 && n<=size_t(space));
    // Exercise short writes even with reported free space.
    n=std::min(n,size_t(3));
    output.append(reinterpret_cast<const char *>(data),n); return n;
  }
};
int main(int argc,char **) {
  Platform platform;
  axio::DiagnosticService<Platform> service(platform.tracker,platform);
  axio::SerialProtocol parser;
  char out[axio::SerialProtocol::MaxReply];
  if (argc>1) {
    for (char c:std::string("AXIO t PING")) assert(!parser.feed(c,0xfffffff0u,service,out));
    assert(!parser.tick(0x3d7u,out)); // 999 ms, unsigned rollover
    assert(parser.tick(0x3d8u,out));
    assert(strstr(out,"\"id\":\"t\"") && strstr(out,"incomplete_timeout"));
    for (char c:std::string("tail\n")) assert(!parser.feed(c,0x3d9u,service,out));
    for (char c:std::string("AXIO ok PING\n")) parser.feed(c,1100,service,out);
    assert(strstr(out,"pong"));
    axio::UsbDiagnostics<decltype(service)> usb(service);
    Port port; port.input="AXIO 1 PING\nAXIO 2 PING\n";
    usb.poll(port,0);
    size_t consumed=port.position;
    for (int i=0;i<100;++i) usb.poll(port,0);
    assert(port.position==consumed && port.output.empty()); // backpressure
    port.space=64;
    for (int i=0;i<300;++i) usb.poll(port,0);
    assert(port.position==port.input.size());
    assert(std::count(port.output.begin(),port.output.end(),'\n')==2);
    assert(port.output.find("\"id\":\"1\"")<port.output.find("\"id\":\"2\""));
    // Queue a complete maximum-size burst before any poll (I2C occupied).
    Port burst;burst.space=64;
    burst.input="AXIO burst "+std::string(axio::SerialProtocol::MaxLine-11,'x')+"\n";
    assert(burst.input.size()==decltype(usb)::RxCapacity);
    axio::UsbDiagnostics<decltype(service)> burstUsb(service);
    for(int i=0;i<500;++i)burstUsb.poll(burst,0);
    assert(burst.position==burst.input.size());
    assert(burst.output.find("unknown_command")!=std::string::npos);
    assert(burst.output.find("\"id\":\"burst\"")!=std::string::npos);
    assert(platform.tare());
    assert(platform.calibration.quatOffset.x()==-.5 && platform.calibration.yawOffset==123);
    auto before=platform.calibration.quatOffset;
    platform.device.valid=false;
    char command[]="TARE"; service.execute("failed",command,out);
    assert(strstr(out,"imu_unavailable"));
    assert(platform.calibration.quatOffset.x()==before.x() && platform.calibration.yawOffset==123);
    assert(platform.tracker.diagnostics.tareErrors==1);
    std::cout << "PASS\n"; return 0;
  }
  char c;
  while (std::cin.get(c)) if (parser.feed(c,0,service,out)) std::cout << out;
}
