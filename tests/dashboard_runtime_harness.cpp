// Real dashboard/runtime/socket code with fake radio, NVS, BNO and ADC.
#include <iostream>
#include <chrono>
#include <thread>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include "Arduino.h"
#include "WiFi.h"
#include "WiFiUdp.h"
#include "Preferences.h"
#include "esp_wifi.h"
#include "mdns.h"
#include "src/core/BnoDevice.h"
#include "src/core/TrackerState.h"
#include "src/connectivity/ConnectivityRuntime.h"
#include "src/connectivity/ConnectivityManager.h"
#include "src/web/DashboardRuntime.h"
#include "src/transport/OscRuntime.h"
FakeWiFi WiFi;FakeESP ESP;FakeWire Wire;FakeSerial Serial;unsigned long waited=0;
bool Adafruit_BNO055::present=true;int Adafruit_BNO055::remap=0,Adafruit_BNO055::sign=0,Adafruit_BNO055::crystal=0;
uint8_t Adafruit_I2CDevice::payload[8]={},Adafruit_I2CDevice::lastRegister=0;
size_t Adafruit_I2CDevice::lastSize=0;bool Adafruit_I2CDevice::ok=true,Adafruit_I2CDevice::begun=false;
std::map<std::string,std::vector<uint8_t>> fakeNvs;
bool failNvs=false,failListener=false;
axio::TrackerState tracker;uint64_t epoch=10000000;
axio::RuntimeStatus legacyRuntimeStatus(){axio::RuntimeStatus s;s.wifi=WiFi.status()==WL_CONNECTED;s.ip[0]=127;s.ip[3]=1;s.outPort=16888;return s;}
bool calibrate(){++tracker.diagnostics.calibrationRequests;return true;}
void reset(){resetOscMetrics();tracker.diagnostics=axio::Diagnostics();epoch=uint64_t(millis())*1000;}
uint64_t window(){return uint64_t(millis())*1000-epoch;}
int main(int argc,char **argv){
  const bool testButton=argc==2&&!strcmp(argv[1],"--sleep-button-offline");
  bool apMode=argc==2&&!strcmp(argv[1],"--ap");
  uint32_t clockOffset=apMode?16000:0;
  WiFi.apIp=IPAddress(127,0,0,1); // Same real loopback socket, simulated AP interface.
  fcntl(STDIN_FILENO,F_SETFL,O_NONBLOCK);
  std::string commands;
  strcpy(reinterpret_cast<char *>(fakeSdk().legacy.sta.ssid),"old-ssid");
  strcpy(reinterpret_cast<char *>(fakeSdk().legacy.sta.password),"secret-migration");
  WiFi.ip=IPAddress(127,0,0,1);
  prepareDashboardFlashInfo();setupConnectivity();setupOscRuntime(calibrate);
  axio::BnoDevice sensor;assert(sensor.begin());tracker.sensor=axio::SensorStatus::Ready;
  setupDashboard(tracker,sensor,reset,window);
  auto start=std::chrono::steady_clock::now();uint32_t last=0,virtualMs=0;bool ready=false;unsigned connected=0;
  for(;;){
    auto elapsed=testButton?virtualMs++:std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
    fakeMillis()=10000+uint32_t(elapsed)+clockOffset;
    char input[128];int received=read(STDIN_FILENO,input,sizeof(input));
    if(received>0)commands.append(input,received);
    size_t newline;
    while((newline=commands.find('\n'))!=std::string::npos){
      auto command=commands.substr(0,newline);commands.erase(0,newline+1);
      if(command=="STA"){apMode=false;WiFi.link=WL_CONNECTED;}
      if(command=="AP"){apMode=true;WiFi.link=0;tickConnectivity();clockOffset+=16000;}
      if(command=="FAIL_NVS")failNvs=true;
      if(command=="OK_NVS")failNvs=false;
      if(command=="FAIL_AP_STOP")WiFi.failApStop=true;
      if(command=="OK_AP_STOP")WiFi.failApStop=false;
      if(command=="ENTRY_RUN")for(unsigned i=0;i<2000;++i){clockOffset+=10;fakeMillis()+=10;tickConnectivity();}
      if(command=="ENTRY_CONFLICT")fakeMdns().hostname="axio-2";
    }
    if(testButton){fakeDeviceControl().button=((elapsed>=100&&elapsed<200)||(elapsed>=500&&elapsed<6000))?0:1;WiFi.link=0;}
    else if(apMode)WiFi.link=0;
    else if(fakeSdk().connects>connected){connected=fakeSdk().connects;WiFi.link=WL_CONNECTED;}
    if(dashboardNetworkAllowed())tickConnectivity();
    const auto &nets=dashboardNetworks();
    if(nets.count&&!strcmp(nets.networks[0].ssid,"renamed"))assert(!strcmp(nets.networks[0].password,"secret-migration"));
    if(dashboardImuAllowed()&&uint32_t(fakeMillis()-last)>=10){
      last=fakeMillis();axio::Sample sample;sample.yaw=.624;sample.pitch=-.173;sample.roll=.286;sample.q0=1;sample.axWorld=-1.4;
      tracker.diagnostics.beginRead(uint64_t(millis())*1000);tracker.publish(sample,uint64_t(millis())*1000);
      tickOscRuntime(tracker.latest,uint64_t(millis())*1000);
    }
    tickDashboard();
    if(testButton){
      if(elapsed>=5600&&elapsed<6000){
        uint8_t power=0;assert(sensor.powerRead(0x3e,power)&&power==2);
        assert(!dashboardImuAllowed()&&!dashboardNetworkAllowed());
        assert(fakeDeviceControl().sleeps==0&&fakeDeviceControl().wakeMode==0);
      }
      if(fakeDeviceControl().sleeps){
        assert(tracker.diagnostics.calibrationRequests==1);
        uint8_t power=0;assert(sensor.powerRead(0x3e,power)&&power==2);
        assert(elapsed>=6100&&elapsed<6200&&!dashboardImuAllowed());
        assert(fakeDeviceControl().button==1&&fakeDeviceControl().wakeMode==0);
        std::cout<<"PASS: physical long press, release, BNO suspend and sleep without Wi-Fi/browser"<<std::endl;return 0;
      }
      assert(elapsed<6200);continue;
    }
    if(!ready&&elapsed>100){std::cout<<"READY"<<std::endl;ready=true;}
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}
