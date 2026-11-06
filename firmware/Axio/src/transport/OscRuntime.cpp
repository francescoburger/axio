#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <esp_timer.h>
#include "OscRuntime.h"
#include "LegacyTransport.h"
#include "../osc/Engine.h"
#include "../osc/Journal.h"
#include "../osc/Transaction.h"
namespace {
struct Storage {
  Preferences prefs;
  bool ready=false;
  void begin() {ready=prefs.begin("axio-osc-v1",false);}
  size_t read(int slot,uint8_t *out,size_t cap) {
    if(!ready)return 0;const char *key=slot?"slot1":"slot0";
    size_t n=prefs.getBytesLength(key);return n<=cap?prefs.getBytes(key,out,cap):0;
  }
  bool write(int slot,const uint8_t *data,size_t size) {return ready&&prefs.putBytes(slot?"slot1":"slot0",data,size)==size;}
  uint64_t committed() {return ready?prefs.getULong64("active",~uint64_t(0)):~uint64_t(0);}
  bool commit(uint64_t value) {return ready&&prefs.putULong64("active",value)==8;}
} storage;
struct Network {
  WiFiUDP ports[2]; unsigned active=0; bool staged=false;
  bool (*callback)()=nullptr;
  IPAddress boundIp{0,0,0,0};bool bound=false;
  uint32_t lastBind=0;bool triedBind=false;uint64_t bindFailures=0;
  bool sta()const{return WiFi.status()==WL_CONNECTED&&uint32_t(WiFi.localIP())!=0;}
  bool allowed()const{return sta()&&!(WiFi.getMode()&WIFI_AP);}
  void sync(uint16_t port) {
    auto ip=WiFi.localIP();
    if(bound&&(!allowed()||ip!=boundIp)){ports[active].stop();bound=false;triedBind=false;}
    if(allowed()&&!bound&&(!triedBind||uint32_t(millis()-lastBind)>=1000)){
      lastBind=millis();triedBind=true;bound=ports[active].begin(ip,port)==1;boundIp=ip;if(!bound)++bindFailures;
    }
  }
  bool send(const axio::osc::Destination &d,const uint8_t *data,size_t size) {
    if(!allowed()||!bound||WiFi.localIP()!=boundIp)return false;
    auto &udp=ports[active];
    if(!udp.beginPacket(IPAddress(d.ip[0],d.ip[1],d.ip[2],d.ip[3]),d.port))return false;
    if(udp.write(data,size)!=size) {udp.endPacket();return false;}
    return udp.endPacket()==1;
  }
  bool tare() {return callback&&callback();}
  bool prepare(uint16_t oldPort,uint16_t newPort) {
    if(!allowed())return true;
    if(oldPort==newPort&&bound)return true;
    staged=ports[1-active].begin(WiFi.localIP(),newPort)==1;return staged;
  }
  void cancel() {if(staged)ports[1-active].stop();staged=false;}
  void activate() {if(staged){ports[active].stop();active=1-active;bound=true;boundIp=WiFi.localIP();}staged=false;}
} network;
axio::osc::Engine<Network> engine(network);
axio::osc::Journal<Storage> journal(storage);
axio::osc::Config candidate;
char configJson[axio::osc::MaxConfig+1];
uint8_t received[axio::osc::MaxPacket];
const char *source="default";
bool listening=false;
uint64_t metricEpoch=0,suspendedNoSta=0,suspendedListener=0,suspendedAp=0;
}
void setupOscRuntime(bool (*tare)()) {
  network.callback=tare;storage.begin();
  candidate=axio::osc::preset("Orientation");
  auto old=legacyRuntimeStatus();
  snprintf(candidate.id,sizeof(candidate.id),"%d",old.deviceId);
  for(unsigned i=0;i<4;++i)candidate.destinations[0].ip[i]=old.ip[i];
  candidate.destinations[0].port=old.outPort;
  if(axio::osc::validate(candidate))candidate=axio::osc::preset("Orientation");
  if(journal.load(candidate))source="saved";
  engine.activate(candidate);
  network.ports[0].stop();network.ports[1].stop();network.bound=false;network.triedBind=false;
  network.sync(candidate.listenPort);listening=network.bound;
}
void tickOscRuntime(const axio::Sample &sample,uint64_t now) {
  network.sync(engine.get().listenPort);listening=network.bound;
  if(!network.sta()){++suspendedNoSta;return;}
  if(WiFi.getMode()&WIFI_AP){++suspendedAp;return;}
  if(!listening){++suspendedListener;return;}
  uint64_t start=esp_timer_get_time();engine.tick(sample,now);
  uint64_t elapsed=esp_timer_get_time()-start;
  if(elapsed>engine.metrics.maxSendUs)engine.metrics.maxSendUs=elapsed;
}
void pollOscRuntime() {
  network.sync(engine.get().listenPort);listening=network.bound;
  if(!network.allowed()||!listening)return;
  uint64_t start=esp_timer_get_time();
  auto &udp=network.ports[network.active];int size=udp.parsePacket();
  if(size>0) {
    if(size>int(sizeof(received))) {udp.clear();++engine.metrics.malformed;}
    else {int n=udp.read(received,size);if(n==size)engine.receive(received,size);else ++engine.metrics.malformed;}
  }
  uint64_t elapsed=esp_timer_get_time()-start;
  if(elapsed>engine.metrics.maxReceiveUs)engine.metrics.maxReceiveUs=elapsed;
}
bool tareOscRuntime(){return engine.tare();}
uint16_t oscListenPort(){return engine.get().listenPort;}
void resetOscMetrics(){suspendedNoSta=0;suspendedListener=0;suspendedAp=0;network.bindFailures=0;engine.resetMetrics();metricEpoch=esp_timer_get_time();}
axio::RuntimeStatus oscRuntimeStatus() {
  auto s=legacyRuntimeStatus();s.engine=true;s.listenPort=engine.get().listenPort;return s;
}
bool oscConfigCommand(const char *id,char *command,char *out) {
  if(!strcmp(command,"CONFIG GET")) {
    if(!axio::osc::serialize(engine.get(),configJson,sizeof(configJson)))axio::SerialProtocol::reply(out,id,"serialization_error");
    else axio::SerialProtocol::reply(out,id,nullptr,configJson);
    return true;
  }
  bool apply=!strncmp(command,"CONFIG APPLY ",13);
  if(!apply&&strncmp(command,"CONFIG VALIDATE ",16))return false;
  uint64_t start=esp_timer_get_time();
  const char *error=axio::osc::parse(command+(apply?13:16),candidate);
  if(!error&&apply) {
    error=axio::osc::apply(candidate,network,journal,engine,listening);
    if(!error){source="applied";listening=network.bound;resetOscMetrics();}
  }
  uint64_t elapsed=esp_timer_get_time()-start;
  if(elapsed>engine.metrics.maxApplyUs)engine.metrics.maxApplyUs=elapsed;
  axio::SerialProtocol::reply(out,id,error,apply?"{\"applied\":true,\"persistent\":true}":"{\"valid\":true}");
  return true;
}
void oscMetricsJson(char *out,size_t cap) {
  const auto &m=engine.metrics;
  uint64_t window=uint64_t(esp_timer_get_time())-metricEpoch;
  size_t n=snprintf(out,cap,"{\"window_us\":%llu,\"udp_attempts\":%llu,\"udp_queued\":%llu,\"delivery_verified\":false,\"suspended_no_sta\":%llu,\"suspended_listener\":%llu,\"suspended_ap\":%llu,\"listener_failures\":%llu,\"udp_errors\":%llu,\"malformed_commands\":%llu,\"max_send_us\":%llu,\"max_receive_us\":%llu,\"max_config_us\":%llu,\"config_source\":\"%s\",\"listening\":%s,\"messages\":[",
    (unsigned long long)window,(unsigned long long)m.udpAttempts,(unsigned long long)m.udpQueued,(unsigned long long)suspendedNoSta,(unsigned long long)suspendedListener,(unsigned long long)suspendedAp,(unsigned long long)network.bindFailures,(unsigned long long)(m.udpAttempts-m.udpQueued),(unsigned long long)m.malformed,
    (unsigned long long)m.maxSendUs,(unsigned long long)m.maxReceiveUs,(unsigned long long)m.maxApplyUs,source,listening?"true":"false");
  for(unsigned i=0;i<engine.get().messageCount&&n<cap;++i)n+=snprintf(out+n,cap-n,"%s{\"index\":%u,\"requested_hz\":%u,\"scheduled\":%llu,\"effective_hz\":%.6f,\"attempted\":%llu,\"queued\":%llu}",i?",":"",i,engine.get().messages[i].hz,(unsigned long long)m.schedules[i],window?m.schedules[i]*1e6/double(window):0,(unsigned long long)m.messageAttempts[i],(unsigned long long)m.messageQueued[i]);
  if(n<cap)snprintf(out+n,cap-n,"]}");
}
