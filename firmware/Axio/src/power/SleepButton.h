#pragma once
#include <stdint.h>
namespace axio {
// A held wake button must first be released before it can request sleep.
// Emit sleep at the hold threshold; a short press calibrates on release.
class SleepButton {
  bool initialized=false,rawPressed=true,stablePressed=true,armed=false,tracking=false;
  uint32_t rawSince=0,pressedSince=0;
 public:
  enum Event {None,Calibrate,Sleep};
  static constexpr uint32_t HoldMs=5000,DebounceMs=30;
  Event tick(uint32_t now,bool pressed,bool enabled){
    if(!initialized){initialized=true;rawPressed=pressed;rawSince=now;}
    if(pressed!=rawPressed){rawPressed=pressed;rawSince=now;}
    if(!enabled){armed=false;tracking=false;}
    if(rawPressed!=stablePressed&&uint32_t(now-rawSince)>=DebounceMs){
      stablePressed=rawPressed;
      if(stablePressed){
        if(enabled&&armed){tracking=true;pressedSince=rawSince;}
        armed=false;
      }else{
        const Event event=enabled&&tracking?(uint32_t(rawSince-pressedSince)>=HoldMs?Sleep:Calibrate):None;
        tracking=false;armed=enabled;return event;
      }
    }
    if(enabled&&tracking&&stablePressed&&rawPressed&&uint32_t(now-pressedSince)>=HoldMs){
      tracking=false;return Sleep;
    }
    if(enabled&&!stablePressed&&!rawPressed&&uint32_t(now-rawSince)>=DebounceMs)armed=true;
    return None;
  }
};
}
