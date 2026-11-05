#pragma once
#include "SerialProtocol.h"
namespace axio {
// No periodic output, waits, flush, dynamic command buffers or network endpoint.
// Poll budgets apply to RX/TX, including a disconnected or stalled USB host.
template<class Service> class UsbDiagnostics {
  SerialProtocol protocol;
  Service &service;
  char output[SerialProtocol::MaxReply]={};
  size_t pending=0, offset=0;
 public:
  explicit UsbDiagnostics(Service &s):service(s) {}
  template<class Port> void poll(Port &port, uint32_t now) {
    if (pending) {
      int available=port.availableForWrite();
      if (available>0) {
        size_t n=pending-offset;
        if (n>64) n=64;
        if (n>size_t(available)) n=size_t(available);
        offset+=port.write(reinterpret_cast<const uint8_t *>(output+offset),n);
        if (offset==pending) { pending=0; offset=0; }
      }
      return;
    }
    if (protocol.tick(now,output)) { pending=strlen(output); return; }
    for (unsigned n=0;n<64 && port.available();++n) {
      if (protocol.feed(char(port.read()),now,service,output)) { pending=strlen(output); break; }
    }
  }
};
}
