#pragma once
#include <stdint.h>
namespace axio {
// Elapsed time is supplied by the RTC, starting in the deep-sleep wake stub.
class WakeHold {
  bool releasing=false;
  uint64_t releasedAt=0;
 public:
  enum Result {Waiting,Accepted,Rejected};
  static constexpr uint64_t HoldUs=1600000,DebounceUs=30000;
  Result tick(uint64_t elapsedUs,bool pressed){
    if(!pressed){
      if(!releasing){releasing=true;releasedAt=elapsedUs;}
      if(elapsedUs-releasedAt>=DebounceUs)return releasedAt>=HoldUs?Accepted:Rejected;
      return Waiting;
    }
    releasing=false;
    return elapsedUs>=HoldUs?Accepted:Waiting;
  }
};
}
void qualifyButtonWake();
bool prepareButtonSleep();
