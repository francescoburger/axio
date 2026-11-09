#pragma once
#include <stdint.h>
namespace axio {
// Starts after HTTP acknowledgement, or after a confirmed physical long press.
// Both paths share the sensor suspend, shutdown and GPIO wake sequence.
template<class Platform> class PowerController {
  Platform &platform;uint32_t since=0,released=0;bool releaseSeen=false,requireRelease=true;
  uint8_t operation=0;bool sensorChanged=false;
  enum State {Idle,Restart,WaitRelease,ConfigWait,SuspendWait,Stopping,SleepReleaseWait,RestoreWait,RestoreModeWait,Failed} state=Idle;
  void fail(const char *why,uint32_t now){
    error=why;platform.resume();
    if(sensorChanged){
      bool ok=platform.write(0x3e,0); // NORMAL, then restore the original operation mode.
      state=ok?RestoreWait:Failed;since=now;
    }else state=Idle;
  }
 public:
  const char *error=nullptr;
  explicit PowerController(Platform &p):platform(p){}
  bool transitioning()const{return state!=Idle&&state!=Failed;}
  bool busy()const{return state!=Idle;}
  bool imuAllowed()const{return state==Idle||state==WaitRelease||state==Restart;}
  const char *status()const{const char *s[]={"idle","restarting","waiting_button_release","sensor_config","sensor_suspend","stopping_communications","waiting_sleep_release","restoring_sensor","restoring_mode","failed"};return s[state];}
  bool request(bool sleep,uint32_t now,bool waitForRelease=true){
    if(transitioning()||(sleep&&state==Failed))return false;
    error=nullptr;since=now;requireRelease=waitForRelease;releaseSeen=false;sensorChanged=false;state=sleep?WaitRelease:Restart;return true;
  }
  void tick(uint32_t now){
    if(state==Idle||state==Failed)return;
    if(state==Restart){if(uint32_t(now-since)>=100)platform.restart();return;}
    if(state==WaitRelease){
      if(requireRelease){
      if(uint32_t(now-since)>=3000){fail("button_held",now);return;}
      if(platform.pressed()){releaseSeen=false;return;}
      if(!releaseSeen){releaseSeen=true;released=now;return;}
      if(uint32_t(now-released)<100)return;
      }
      if(!platform.prepareWake()){fail("wake_configuration_failed",now);return;}
      if(!platform.read(0x3d,operation)){fail("sensor_mode_read_failed",now);return;}
      // Set sensorChanged before the write: an I2C failure can leave write outcome uncertain.
      sensorChanged=true;
      if(!platform.write(0x3d,0)){fail("sensor_config_failed",now);return;}
      state=ConfigWait;since=now;return;
    }
    if(state==ConfigWait){
      if(uint32_t(now-since)<25)return;
      uint8_t actual=255;
      if(!platform.read(0x3d,actual)||actual!=0){fail("sensor_config_unverified",now);return;}
      if(!platform.write(0x3e,2)){fail("sensor_suspend_failed",now);return;}
      state=SuspendWait;since=now;return;
    }
    if(state==SuspendWait){
      if(uint32_t(now-since)<20)return;
      uint8_t actual=255;
      if(!platform.read(0x3e,actual)||actual!=2){fail("sensor_suspend_unverified",now);return;}
      if(requireRelease&&platform.pressed()){fail("button_pressed_before_sleep",now);return;}
      state=Stopping;since=now;return;
    }
    if(state==Stopping){
      if(uint32_t(now-since)>=3000){fail("communications_stop_failed",now);return;}
      if(platform.stop()){
        // Sensor and communications are already off. LOW wake would wake
        // immediately while held; wait here without restarting any service.
        state=SleepReleaseWait;releaseSeen=false;
      }return;
    }
    if(state==SleepReleaseWait){
      if(platform.pressed()){releaseSeen=false;return;}
      if(!releaseSeen){releaseSeen=true;released=now;return;}
      if(uint32_t(now-released)<100)return;
      if(!platform.prepareWake()){fail("wake_configuration_failed",now);return;}
      platform.sleep();return;
    }
    if(state==RestoreWait){
      if(uint32_t(now-since)<25)return;
      uint8_t power=255;
      if(!platform.read(0x3e,power)||power!=0||!platform.write(0x3d,operation)){state=Failed;return;}
      state=RestoreModeWait;since=now;return;
    }
    if(state==RestoreModeWait){
      if(uint32_t(now-since)<25)return;
      uint8_t actual=255;
      if(!platform.read(0x3d,actual)||actual!=operation){state=Failed;return;}
      state=Idle;sensorChanged=false;
    }
  }
};
}
