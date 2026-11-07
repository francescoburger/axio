#pragma once
#include <stdint.h>
namespace axio {
// Same millis subtraction and no catch-up as V1.1, including 32-bit rollover.
class Scheduler {
  uint32_t last = 0;
 public:
  uint32_t remaining(uint32_t now)const{uint32_t elapsed=uint32_t(now-last);return elapsed>=10?0:10-elapsed;}
  bool due(uint32_t now) {
    if (uint32_t(now - last) < 10) return false;
    last = now;
    return true;
  }
};
}
