#include <Arduino.h>
#include <WiFi.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <esp_sleep.h>
#include <esp_heap_caps.h>
#include <esp_wifi.h>
#include <math.h>
#include "DashboardRuntime.h"
#include "DashboardHttp.h"
#include "DashboardService.h"
#include "JsonWriter.h"
#include "ImageSize.h"
#ifndef AXIO_DASHBOARD_TEST_CLOCK
#include <esp_ota_ops.h>
#include <esp_partition.h>
#endif
#include "../core/BnoDevice.h"
#include "../core/TrackerState.h"
#include "../power/Battery.h"
#include "../power/PowerController.h"
#include "../power/SleepButton.h"
#include "../connectivity/ProvisioningSocket.h"
#include "../connectivity/ConnectivityRuntime.h"
#include "../connectivity/ConnectivityManager.h"
#include "../connectivity/ApRuntime.h"
#include "../connectivity/DiscoveryRuntime.h"
#include "../transport/OscRuntime.h"
#ifndef AXIO_DASHBOARD_HTTP_PORT
#define AXIO_DASHBOARD_HTTP_PORT 80
#endif
namespace {
axio::TrackerState *tracker=nullptr;axio::BnoDevice *sensor=nullptr;
void (*resetMetricsCallback)()=nullptr;uint64_t (*windowCallback)()=nullptr;
axio::Battery measuredBattery;
axio::SleepButton sleepButton;
char sessionToken[33]={},appliedId[17]={};
const char *configurationError=nullptr;
bool serving=false,tried=false,communicationsOff=false,boundAp=false;
uint32_t cachedFlashSize=0,cachedFlashPartition=0;
uint32_t lastBind=0,boundIp=0;uint64_t httpFailures=0;
enum Deferred {Nothing,ApplyNetwork,ApplyIdentity};Deferred deferred=Nothing;uint32_t deferredAt=0;
void closeServer();
struct PowerPlatform {
  bool pressed(){return digitalRead(BUTTON_PIN)==BUTTON_ACTIVE_LEVEL;}
  bool prepareWake(){
    pinMode(BUTTON_PIN,INPUT_PULLUP);
    return esp_deep_sleep_enable_gpio_wakeup(uint64_t(1)<<BUTTON_PIN,ESP_GPIO_WAKEUP_GPIO_LOW)==ESP_OK;
  }
  bool read(uint8_t reg,uint8_t &value){return sensor->powerRead(reg,value);}
  bool write(uint8_t reg,uint8_t value){return sensor->powerWrite(reg,value);}
  bool stop(){
    stopOscRuntime();
    if(!stopDiscoveryRuntime())return false;
    closeServer();stopApRuntime();
    communicationsOff=WiFi.enableSTA(false);return communicationsOff;
  }
  void resume(){
    if(communicationsOff){WiFi.enableSTA(true);refreshApConfiguration();communicationsOff=false;}
  }
  void restart(){ESP.restart();}
  void sleep(){esp_deep_sleep_start();}
} powerPlatform;
axio::PowerController<PowerPlatform> powerController(powerPlatform);
void ip(axio::JsonWriter &w,IPAddress address){w.add("\"%u.%u.%u.%u\"",address[0],address[1],address[2],address[3]);}
const char *sensorStatus(){if(!powerController.imuAllowed())return powerController.status();const char *s[]={"uninitialized","ready","init_failed","read_error"};return s[unsigned(tracker->sensor)];}
struct Platform {
  bool host(const char *name){
    char literal[96],local[80];auto address=boundAp?WiFi.softAPIP():WiFi.localIP();
    snprintf(literal,sizeof(literal),"%u.%u.%u.%u",address[0],address[1],address[2],address[3]);
    snprintf(local,sizeof(local),"%s.local",discoveryHostname());
    for(const char *allowed:{literal,local}){
      if(boundAp&&allowed==local)continue; // mDNS is a STA service only.
      size_t n=strlen(allowed);if(!strcasecmp(name,allowed))return true;
      char port[12];snprintf(port,sizeof(port),":%u",unsigned(AXIO_DASHBOARD_HTTP_PORT));
      if(!strncasecmp(name,allowed,n)&&!strcmp(name+n,port))return true;
    }
    return false;
  }
  bool token(const char *value){return strlen(value)==32&&!strcmp(value,sessionToken);}
  void session(char *out,size_t cap){snprintf(out,cap,"{\"token\":\"%s\"}",sessionToken);}
  bool busy(){return powerController.transitioning()||deferred!=Nothing;}
  bool canSleep(){return powerController.imuAllowed()&&tracker->sensor==axio::SensorStatus::Ready;}
  const char *osc(const char *json,bool apply){const char *error=applyDashboardOsc(json,apply);if(apply&&!error)configurationError=nullptr;return error;}
  const char *network(const char *json,bool apply){return dashboardNetworkChange(json,apply);}
  const char *identity(const char *id){return changeTrackerId(id);}
  void activateNetwork(){deferred=ApplyNetwork;deferredAt=millis();}
  void activateIdentity(){deferred=ApplyIdentity;deferredAt=millis();}
  void power(bool sleep){pinMode(BUTTON_PIN,INPUT_PULLUP);powerController.request(sleep,millis());}
  void powerStatus(char *out,size_t cap){snprintf(out,cap,"{\"state\":\"%s\",\"wake_cause\":%u,\"error\":%s%s%s}",powerController.status(),unsigned(esp_sleep_get_wakeup_cause()),powerController.error?"\"":"",powerController.error?powerController.error:"null",powerController.error?"\"":"");}
  const char *command(const char *cmd){
    if(!strcmp(cmd,"CALIBRATION")||!strcmp(cmd,"TARE"))return powerController.imuAllowed()&&calibrateOscRuntime()?nullptr:"imu_unavailable";
    resetMetricsCallback();return nullptr;
  }
  void battery(char *out,size_t cap){
    if(measuredBattery.available)snprintf(out,cap,"{\"voltage\":%.3f,\"percent\":null}",double(measuredBattery.voltage));
    else snprintf(out,cap,"{\"voltage\":null,\"percent\":null}");
  }
  bool sample(char *out,size_t cap){
    static uint32_t last=0;static bool sent=false;static axio::Sample cached;
    uint32_t now=millis();if(!sent||uint32_t(now-last)>=100){cached=tracker->latest;sent=true;last=now;}
    const auto &s=cached;axio::JsonWriter w(out,cap);
    w.add("{\"valid\":%s,\"sensor\":\"%s\",\"sequence\":%llu,\"timestamp_us\":%llu",s.valid&&tracker->sensor==axio::SensorStatus::Ready&&powerController.imuAllowed()?"true":"false",sensorStatus(),(unsigned long long)s.sequence,(unsigned long long)s.timestampUs);
    for(unsigned i=0;i<16;++i){const auto &f=axio::osc::fields()[i];double value=s.*(f.member);if(isfinite(value))w.add(",\"%s\":%.7g",f.name,value);else w.add(",\"%s\":null",f.name);}w.add("}");return w.ok;
  }
  bool snapshot(char *out,size_t cap){
    axio::JsonWriter w(out,cap);const auto &c=oscConfiguration();const auto &d=tracker->diagnostics;
    bool connected=WiFi.status()==WL_CONNECTED&&uint32_t(WiFi.localIP())!=0;
    bool apActive=(WiFi.getMode()&WIFI_AP)&&WiFi.AP.started()&&uint32_t(WiFi.softAPIP())!=0;
    uint8_t mac[6]={};esp_wifi_get_mac(WIFI_IF_STA,mac);
    w.add("{\"device\":{\"id\":\"%s\",\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"ip\":",c.id,mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);ip(w,boundAp?WiFi.softAPIP():WiFi.localIP());
    w.add(",\"hostname\":\"%s.local\",\"ap_ssid\":\"%s\",\"version\":\"4.0.0-dev\",\"osc_status\":\"%s\"},",discoveryHostname(),apSsid(),configurationError?"Configuration error":connected&&!(WiFi.getMode()&WIFI_AP)?"Active":"Suspended");
    w.add("\"status\":{\"wifi_connected\":%s,\"imu\":\"%s\"},\"osc\":",connected?"true":"false",sensorStatus());
    w.raw([&](char *p,size_t n){return axio::osc::serialize(c,p,n);});
    w.add(",\"peers\":");w.raw(discoveryPeersJson);
    w.add(",\"network\":{\"connected\":%s,\"ap_active\":%s,\"revision\":%u,\"ssid\":",connected?"true":"false",apActive?"true":"false",dashboardNetworkRevision());
    // SSID is an arbitrary byte string: use hex in the saved editor for lossless round trips.
    wifi_config_t radio={};esp_wifi_get_config(WIFI_IF_STA,&radio);char ssid[33]={};memcpy(ssid,radio.sta.ssid,32);w.quote(connected?ssid:"");
    w.add(",\"ip\":");ip(w,boundAp?WiFi.softAPIP():WiFi.localIP());w.add(",\"gateway\":");if(connected)ip(w,WiFi.gatewayIP());else w.add("null");w.add(",\"subnet\":");if(connected)ip(w,WiFi.subnetMask());else w.add("null");
    if(connected)w.add(",\"rssi\":%d",WiFi.RSSI());else w.add(",\"rssi\":null");
    w.add(",\"entry_point\":null,\"entry_state\":\"unverified\",\"ap_ip\":");ip(w,apActive?WiFi.softAPIP():IPAddress(192,168,4,1));w.add(",\"networks\":[");
    const auto &networks=dashboardNetworks();unsigned order[8]={};
    for(unsigned i=0;i<networks.count;++i){order[i]=i;for(unsigned j=i;j&&networks.networks[order[j]].priority>networks.networks[order[j-1]].priority;--j){unsigned t=order[j];order[j]=order[j-1];order[j-1]=t;}}
    for(unsigned i=0;i<networks.count;++i){const auto &n=networks.networks[order[i]];w.add("%s{\"original\":%u,\"ssid_hex\":\"",i?",":"",order[i]);for(const unsigned char *p=reinterpret_cast<const unsigned char *>(n.ssid);*p;++p)w.add("%02x",*p);w.add("\"}");}
    const uint64_t window=windowCallback(),oscWindow=oscWindowUs();
    w.add("]},\"diagnostics\":{\"uptime\":\"%llu s\",\"heap_free\":\"%u B\",\"heap_min\":\"%u B\",\"flash_used\":\"%u B\",\"flash_partition\":%u,",(unsigned long long)(esp_timer_get_time()/1000000),ESP.getFreeHeap(),ESP.getMinFreeHeap(),cachedFlashSize,cachedFlashPartition);
    if(window)w.add("\"imu_hz\":%.4f,",double(d.acquired)*1e6/double(window));else w.add("\"imu_hz\":null,");
    if(oscWindow)w.add("\"osc_hz\":%.4f,",double(oscDatagrams())*1e6/double(oscWindow));else w.add("\"osc_hz\":null,");
    w.add("\"max_interval_us\":%llu,\"samples\":%llu,\"datagrams\":%llu,\"udp_errors\":%llu,\"destinations\":%u,\"wifi_mode\":\"%s\",\"mdns\":\"%s\",\"reconnects\":%llu,\"largest_block\":\"%u B\",\"http_bind_failures\":%llu},\"configuration_error\":",(unsigned long long)d.maxIntervalUs,(unsigned long long)d.acquired,(unsigned long long)oscDatagrams(),(unsigned long long)oscErrors(),c.destCount,WiFi.getMode()&WIFI_AP?"APSTA":"STA",discoveryState(),(unsigned long long)connectivityReconnects(),heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),(unsigned long long)httpFailures);
    if(configurationError)w.quote(configurationError);else w.add("null");w.add("}");return w.ok;
  }
} platform;
axio::DashboardService<Platform> service(platform);
axio::connectivity::ProvisioningSocket dashboardSocket;
axio::DashboardHttp<decltype(dashboardSocket),decltype(service)> http(dashboardSocket,service);
void closeServer(){http.close();serving=false;tried=false;setApHttpStatus(false);}
}
void setupDashboard(axio::TrackerState &state,axio::BnoDevice &device,void (*reset)(),uint64_t (*window)()){
  tracker=&state;sensor=&device;resetMetricsCallback=reset;windowCallback=window;
  setApHttpClose(closeServer);
  uint8_t entropy[16];esp_fill_random(entropy,sizeof(entropy));
  const char *digits="0123456789abcdef";for(unsigned i=0;i<16;++i){sessionToken[2*i]=digits[entropy[i]>>4];sessionToken[2*i+1]=digits[entropy[i]&15];}sessionToken[32]=0;
  memset(entropy,0,sizeof(entropy));pinMode(BATTERY_ADC_PIN,INPUT);
  pinMode(BUTTON_PIN,BUTTON_USE_PULLUP?INPUT_PULLUP:INPUT);
  configurationError=initializeDashboardOsc(discoveryHardware());
  setApTrackerId(oscConfiguration().id);setDiscoveryTrackerId(oscConfiguration().id);strcpy(appliedId,oscConfiguration().id);
}
bool dashboardImuAllowed(){return powerController.imuAllowed();}
void tickDashboard(bool allowWork){
  const uint32_t buttonNow=millis();
  if(sleepButton.tick(buttonNow,powerPlatform.pressed(),deferred==Nothing&&!powerController.busy()&&tracker->sensor==axio::SensorStatus::Ready))
    powerController.request(true,buttonNow);
  powerController.tick(millis());
  if(deferred!=Nothing&&uint32_t(millis()-deferredAt)>=150){
    Deferred action=deferred;deferred=Nothing;
    if(action==ApplyNetwork)activateDashboardNetwork();
    else {setApTrackerId(oscConfiguration().id);setDiscoveryTrackerId(oscConfiguration().id);strcpy(appliedId,oscConfiguration().id);}
  }
  if(deferred==Nothing&&http.idle()&&!powerController.transitioning()&&strcmp(appliedId,oscConfiguration().id)){deferred=ApplyIdentity;deferredAt=millis();}
  if(!powerController.imuAllowed())allowWork=true;
  if(allowWork)measuredBattery.tick(millis(),[](){return analogReadMilliVolts(BATTERY_ADC_PIN);});
  bool ap=(WiFi.getMode()&WIFI_AP)!=0;
  auto ipAddress=ap?WiFi.softAPIP():WiFi.localIP();
  bool eligible=communicationsOff?false:ap?(WiFi.AP.started()&&uint32_t(ipAddress)!=0):(WiFi.status()==WL_CONNECTED&&uint32_t(ipAddress)!=0);
  uint32_t address=uint32_t(ipAddress),now=millis();
  if(serving&&(!eligible||address!=boundIp||ap!=boundAp))closeServer();
  if(!eligible||!allowWork)return;
  if(!serving&&(!tried||uint32_t(now-lastBind)>=1000)){
    tried=true;lastBind=now;uint8_t bytes[]={ipAddress[0],ipAddress[1],ipAddress[2],ipAddress[3]};
    serving=dashboardSocket.open(bytes,AXIO_DASHBOARD_HTTP_PORT);boundIp=address;boundAp=ap;if(!serving)++httpFailures;
    setApHttpStatus(ap&&serving,ap&&!serving);
  }
  if(serving)http.tick(now);
}

bool dashboardNetworkAllowed(){return !powerController.transitioning();}

void prepareDashboardFlashInfo(){
#ifdef AXIO_DASHBOARD_TEST_CLOCK
  cachedFlashSize=ESP.getSketchSize();cachedFlashPartition=ESP.getFreeSketchSpace();
#else
  const esp_partition_t *partition=esp_ota_get_running_partition();
  if(!partition)return;cachedFlashPartition=partition->size;
  cachedFlashSize=axio::imageSize([&](uint32_t offset,uint8_t *out,size_t n){return esp_partition_read(partition,offset,out,n)==ESP_OK;},partition->size);
#endif
}
