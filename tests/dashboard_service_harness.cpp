#include <cassert>
#include <string>
#include "src/web/DashboardService.h"
#include "src/power/PowerController.h"
#include "src/power/SleepButton.h"
struct Platform {
  bool busyValue=false,canSleepValue=true;unsigned networkStage=0,activated=0,ids=0,restarts=0,sleeps=0,commands=0;
  bool host(const char *){return true;}bool token(const char *){return true;}
  void session(char *p,size_t n){snprintf(p,n,"{\"token\":\"fixture\"}");}
  bool snapshot(char *p,size_t n){snprintf(p,n,"{}");return true;}
  bool sample(char *p,size_t n){snprintf(p,n,"{\"valid\":false}");return true;}
  void battery(char *p,size_t n){snprintf(p,n,"{\"percent\":null}");}
  void powerStatus(char *p,size_t n){snprintf(p,n,"{\"state\":\"idle\"}");}
  bool busy(){return busyValue;}bool canSleep(){return canSleepValue;}
  const char *osc(const char *,bool){return nullptr;}
  const char *network(const char *,bool apply){if(apply)++networkStage;return nullptr;}
  const char *identity(const char *){++ids;return nullptr;}
  const char *command(const char *){++commands;return nullptr;}
  void activateNetwork(){++activated;}void activateIdentity(){++ids;}
  void power(bool sleep){sleep?++sleeps:++restarts;}
};
struct SensorPower {
  bool pressedValue=false,wake=true,failWrite=false,failRead=false,stopReady=true;
  uint8_t op=12,power=0;unsigned stopped=0,slept=0,restarted=0,resumed=0;
  bool pressed(){return pressedValue;}bool prepareWake(){return wake;}
  bool read(uint8_t reg,uint8_t &v){if(failRead)return false;v=reg==0x3d?op:power;return true;}
  bool write(uint8_t reg,uint8_t v){if(failWrite)return false;if(reg==0x3d)op=v;else power=v;return true;}
  bool stop(){++stopped;return stopReady;}void sleep(){++slept;}void restart(){++restarted;}void resume(){++resumed;}
};
int main(){
  Platform p;axio::DashboardService<Platform> service(p);
  auto request=[&](const char *path,const char *json){std::string body=json;return service.handle(true,path,&body[0]);};
  assert(service.handle(false,"/?page=osc",nullptr).gzip);
  assert(service.handle(false,"/app.js",nullptr).size>1000);
  assert(request("/api/network/validate","{}").code==200&&p.networkStage==0);
  assert(request("/api/network/apply","{}").code==200&&p.networkStage==1&&p.activated==0);
  service.finished(true);assert(p.activated==1);
  request("/api/network/apply","{}");service.finished(false);assert(p.activated==2);
  assert(request("/api/control","{\"action\":\"sleep\",\"confirm\":false}").code==400);
  assert(request("/api/control","{\"action\":\"sleep\",\"confirm\":true}").code==200&&p.sleeps==0);
  service.finished(false);assert(p.sleeps==0); // Browser vanished: do not sleep.
  request("/api/control","{\"action\":\"sleep\",\"confirm\":true}");service.finished(true);assert(p.sleeps==1);
  p.canSleepValue=false;assert(request("/api/control","{\"action\":\"sleep\",\"confirm\":true}").code==400);
  request("/api/control","{\"action\":\"restart\",\"confirm\":true}");service.finished(true);assert(p.restarts==1);
  assert(request("/api/command","{\"command\":\"AP CREDENTIALS\"}").code==403&&p.commands==0);
  assert(request("/api/command","{\"command\":\"CALIBRATION\"}").code==200&&p.commands==1);
  assert(request("/api/command","{\"command\":\"CALIBRATION\",\"command\":\"CALIBRATION\"}").code==400);
  p.busyValue=true;assert(request("/api/identity","{\"id\":\"x\"}").code==409);p.busyValue=false;
  assert(request("/api/identity","{\"id\":\"x\",\"unknown\":0}").code==400);
  assert(request("/api/identity","{\"id\":\"x\"}").code==200);service.finished(true);assert(p.ids==2);
  // Deep Sleep: stable button release -> CONFIG -> SUSPEND readback -> stop -> sleep.
  SensorPower hw;axio::PowerController<SensorPower> power(hw);
  assert(power.request(true,0));power.tick(0);power.tick(99);assert(hw.op==12&&power.imuAllowed());
  power.tick(100);assert(hw.op==0&&!power.imuAllowed());power.tick(124);assert(hw.power==0);
  power.tick(125);assert(hw.power==2&&hw.stopped==0);power.tick(144);assert(hw.stopped==0);
  power.tick(145);power.tick(146);assert(hw.stopped==1&&hw.slept==1);
  // Held button, unsupported wake, I2C errors and delayed shutdown never enter sleep.
  SensorPower held;held.pressedValue=true;axio::PowerController<SensorPower> a(held);
  a.request(true,0);a.tick(3000);assert(a.error&&held.op==12&&held.slept==0);
  SensorPower noWake;noWake.wake=false;axio::PowerController<SensorPower> b(noWake);
  b.request(true,0);b.tick(0);b.tick(100);assert(b.error&&noWake.slept==0);
  SensorPower bad;axio::PowerController<SensorPower> c(bad);
  c.request(true,0);c.tick(0);c.tick(100);bad.failRead=true;c.tick(125);assert(c.error&&bad.slept==0);
  bad.failRead=false;c.tick(150);c.tick(175);assert(c.imuAllowed()&&bad.op==12&&bad.power==0);
  SensorPower slow;slow.stopReady=false;axio::PowerController<SensorPower> d(slow);
  d.request(true,0xfffffff0);d.tick(0xfffffff0);d.tick(84);d.tick(109);d.tick(129);d.tick(3130);
  assert(d.error&&slow.slept==0);d.tick(3155);d.tick(3180);assert(d.imuAllowed());
  SensorPower fail;axio::PowerController<SensorPower> e(fail);e.request(true,0);e.tick(0);fail.failWrite=true;e.tick(100);
  assert(!e.imuAllowed()&&fail.slept==0&&!e.transitioning()); // Explicit restart remains possible.
  assert(e.request(false,200));e.tick(300);assert(fail.restarted==1);
  // The physical confirmation uses the same checked power sequence as HTTP.
  SensorPower physical;axio::SleepButton button;axio::PowerController<SensorPower> f(physical);
  auto tickButton=[&](uint32_t now,bool pressed){
    physical.pressedValue=pressed;
    if(button.tick(now,pressed,!f.busy()))assert(f.request(true,now));
    f.tick(now);
  };
  tickButton(0,false);tickButton(30,false);tickButton(100,true);tickButton(130,true);
  tickButton(3100,true);tickButton(10000,true);assert(!f.busy()&&physical.op==12);
  tickButton(10010,false);tickButton(10040,false);assert(f.busy()&&physical.slept==0);
  tickButton(10140,false);tickButton(10165,false);tickButton(10185,false);tickButton(10186,false);
  assert(physical.slept==1&&physical.power==2&&physical.stopped==1);
  puts("PASS");
}
