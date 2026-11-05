#pragma once
#include <stdint.h>
namespace axio {
struct Diagnostics {
  uint64_t attempts=0, acquired=0, readErrors=0, initErrors=0;
  uint64_t tareRequests=0, tareErrors=0, sendAttempts=0;
  uint64_t lastReadUs=0, maxReadUs=0, lastSendUs=0, maxSendUs=0;
  uint64_t lastStartUs=0, lastIntervalUs=0, maxIntervalUs=0;
  uint64_t lastProcessUs=0, maxProcessUs=0;
  double intervalMeanUs=0, intervalM2Us=0;
  void beginRead(uint64_t now) {
    if (attempts) {
      lastIntervalUs = now - lastStartUs;
      double delta=double(lastIntervalUs)-intervalMeanUs;
      intervalMeanUs+=delta/double(attempts);
      intervalM2Us+=delta*(double(lastIntervalUs)-intervalMeanUs);
      if (lastIntervalUs > maxIntervalUs) maxIntervalUs = lastIntervalUs;
    }
    lastStartUs = now;
    ++attempts;
  }
  void endRead(uint64_t elapsed) {
    lastReadUs = elapsed;
    if (elapsed > maxReadUs) maxReadUs = elapsed;
  }
  void sent(uint64_t elapsed) {
    ++sendAttempts;
    lastSendUs = elapsed;
    if (elapsed > maxSendUs) maxSendUs = elapsed;
  }
};
}
