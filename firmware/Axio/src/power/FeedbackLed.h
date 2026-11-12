#pragma once
#include <stdint.h>

namespace axio {
// One transition per tick, with unsigned millis arithmetic and no catch-up.
// Every pulse includes a final dark gap. Shutdown overrides other signals;
// calibration requests during an active signal coalesce into one pending cue.
class FeedbackLed {
  enum Signal {Idle,Startup,Calibration,Shutdown} signal=Idle;
  uint32_t since=0;
  uint8_t phase=0;
  bool pendingCalibration=false;
  uint8_t phases()const{return signal==Startup?2:signal==Calibration?4:6;}
  void start(Signal next,uint32_t now){signal=next;phase=0;since=now;}
 public:
  bool on()const{return signal!=Idle&&phase<phases()&&phase%2==0;}
  bool sleepReady()const{return signal==Shutdown&&phase==6;}
  void reset(){signal=Idle;phase=0;pendingCalibration=false;}
  void startup(uint32_t now){reset();start(Startup,now);}
  void calibrated(uint32_t now){
    if(signal==Shutdown)return;
    if(signal==Idle)start(Calibration,now);
    else pendingCalibration=true;
  }
  void shutdown(uint32_t now){pendingCalibration=false;start(Shutdown,now);}
  void tick(uint32_t now){
    if(signal==Idle||phase==phases())return;
    const uint32_t duration=signal==Startup&&phase==0?500:100;
    if(uint32_t(now-since)<duration)return;
    since=now;++phase;
    if(phase==phases()&&signal!=Shutdown){
      signal=Idle;
      if(pendingCalibration){pendingCalibration=false;start(Calibration,now);}
    }
  }
};
}
