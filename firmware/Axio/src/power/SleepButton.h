#pragma once
#include <stdint.h>
namespace axio {
// A held wake button must first be released before it can request sleep.
// The long press confirms the command; emit it only after debounced release.
class SleepButton {
  bool initialized=false,rawPressed=true,stablePressed=true,armed=false,tracking=false;
  uint32_t rawSince=0,pressedSince=0;
 public:
  static constexpr uint32_t HoldMs=3000,DebounceMs=30;
  bool tick(uint32_t now,bool pressed,bool enabled){
    if(!initialized){initialized=true;rawPressed=pressed;rawSince=now;}
    if(pressed!=rawPressed){rawPressed=pressed;rawSince=now;}
    if(!enabled){armed=false;tracking=false;}
    if(rawPressed!=stablePressed&&uint32_t(now-rawSince)>=DebounceMs){
      stablePressed=rawPressed;
      if(stablePressed){
        if(enabled&&armed){tracking=true;pressedSince=rawSince;}
        armed=false;
      }else{
        const bool confirmed=enabled&&tracking&&uint32_t(rawSince-pressedSince)>=HoldMs;
        tracking=false;armed=enabled;return confirmed;
      }
    }
    if(enabled&&!stablePressed&&!rawPressed&&uint32_t(now-rawSince)>=DebounceMs)armed=true;
    return false;
  }
};
}
