#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include "ConnectivityManager.h"
#include "ConnectivityRuntime.h"
#include "ApRuntime.h"
#include "DiscoveryRuntime.h"
#include "../diagnostics/SerialProtocol.h"
namespace {
using namespace axio::connectivity;
struct Store {
  Preferences prefs;bool ready=false;
  void begin(){ready=prefs.begin("axio-wifi-v1",false);}
  bool exists(){return ready&&(prefs.isKey("slot0")||prefs.isKey("slot1")||prefs.isKey("active"));}
  size_t read(int slot,uint8_t *out,size_t cap){if(!ready)return 0;const char *k=slot?"slot1":"slot0";return prefs.getBytesLength(k)<=cap?prefs.getBytes(k,out,cap):0;}
  bool write(int slot,const uint8_t *data,size_t size){return ready&&prefs.putBytes(slot?"slot1":"slot0",data,size)==size;}
  uint64_t committed(){return ready?prefs.getULong64("active",~uint64_t(0)):~uint64_t(0);}
  bool commit(uint64_t v){return ready&&prefs.putULong64("active",v)==8;}
} store;
struct Driver {
  bool ready=false;
  bool connected()const{return ready&&WiFi.status()==WL_CONNECTED&&uint32_t(WiFi.localIP())!=0;}
  void disconnect(){if(ready)esp_wifi_disconnect();}
  bool connect(const WifiNetwork &n) {
    if(!ready)return false;
    wifi_config_t c={};memcpy(c.sta.ssid,n.ssid,strlen(n.ssid));memcpy(c.sta.password,n.password,strlen(n.password));
    c.sta.scan_method=WIFI_FAST_SCAN;c.sta.threshold.authmode=n.password[0]?WIFI_AUTH_WPA2_PSK:WIFI_AUTH_OPEN;c.sta.pmf_cfg.capable=true;
    return esp_wifi_set_config(WIFI_IF_STA,&c)==ESP_OK&&esp_wifi_connect()==ESP_OK;
  }
} driver;
Journal<Store> journal(store);
ConnectivityManager<Driver,decltype(journal)> manager(driver,journal);
Config candidate;
char response[1800];
int digit(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
bool unhex(const char *p,char *out,size_t cap) {
  size_t n=strlen(p);if(n%2||n/2>=cap)return false;
  memset(out,0,cap);
  for(size_t i=0;i<n;i+=2){int a=digit(p[i]),b=digit(p[i+1]);if(a<0||b<0||!(a*16+b))return false;out[i/2]=char(a*16+b);}
  return true;
}
void hex(const char *p,char *out){const char *digits="0123456789abcdef";while(*p){unsigned char c=*p++;*out++=digits[c>>4];*out++=digits[c&15];}*out=0;}
// Escape arbitrary SSID bytes into ASCII JSON, without touching credentials.
void quoteSsid(const char *p,char *out){while(*p){unsigned char c=*p++;if(c>=32&&c<127&&c!='"'&&c!='\\')*out++=char(c);else {snprintf(out,7,"\\u%04x",c);out+=6;}}*out=0;}
}
void setupConnectivity() {
  // Read SDK flash config before selecting RAM storage. Never erase or replace it.
  setupDiscoveryIdentity();
  WiFi.setAutoReconnect(false);
  bool started=WiFi.STA.begin(false);
  wifi_config_t old={};WifiNetwork legacy;
  bool read=started&&esp_wifi_get_config(WIFI_IF_STA,&old)==ESP_OK;
  if(read){memcpy(legacy.ssid,old.sta.ssid,32);memcpy(legacy.password,old.sta.password,64);}
  driver.ready=started&&esp_wifi_set_storage(WIFI_STORAGE_RAM)==ESP_OK&&WiFi.STA.config();
  if(driver.ready)esp_wifi_disconnect();
  store.begin();manager.begin(read?&legacy:nullptr,uint32_t(millis()));
  if(!store.ready)manager.storageError="nvs_unavailable";
  if(driver.ready)setupApRuntime();
}
void tickConnectivity(){manager.tick(uint32_t(millis()));tickApRuntime(driver.connected(),manager.offlineMs(uint32_t(millis())));tickDiscovery(driver.connected());}
bool connectivityCommand(const char *id,char *command,char *out) {
  if(discoveryCommand(id,command,out))return true;
  if(apRuntimeCommand(id,command,out))return true;
  if(strncmp(command,"WIFI ",5))return false;
  const char *error=nullptr;
  if(!strcmp(command,"WIFI GET")) {
    connectivityNetworksJson(response,sizeof(response));
    axio::SerialProtocol::reply(out,id,nullptr,response);return true;
  }
  candidate=manager.get();
  if(!strncmp(command,"WIFI SET ",9)) {
    char *priority=command+9,*ssid=strchr(priority,' '),*password=ssid?strchr(ssid+1,' '):nullptr;
    WifiNetwork n;
    if(!ssid||!password)error="invalid_wifi_command";
    else {
      *ssid++=0;*password++=0;unsigned value=0;
      if(!*priority||strlen(priority)>3)error="invalid_priority";
      for(const char *p=priority;*p;++p){if(*p<'0'||*p>'9')error="invalid_priority";else value=value*10+unsigned(*p-'0');}
      if(value>255)error="invalid_priority";
      n.priority=uint8_t(value);
      if(!unhex(ssid,n.ssid,sizeof(n.ssid))||(!strcmp(password,"-")?false:!unhex(password,n.password,sizeof(n.password))))error="invalid_wifi_encoding";
      unsigned index=0;while(index<candidate.count&&strcmp(candidate.networks[index].ssid,n.ssid))++index;
      if(index==MaxNetworks)error="too_many_networks";
      if(!error){candidate.networks[index]=n;if(index==candidate.count)++candidate.count;}
    }
  } else if(!strncmp(command,"WIFI DELETE ",12)) {
    char ssid[33];if(!unhex(command+12,ssid,sizeof(ssid)))error="invalid_wifi_encoding";
    else {unsigned i=0;while(i<candidate.count&&strcmp(candidate.networks[i].ssid,ssid))++i;
      if(i==candidate.count)error="network_not_found";
      else {for(unsigned k=i+1;k<candidate.count;++k)candidate.networks[k-1]=candidate.networks[k];candidate.networks[--candidate.count]=WifiNetwork();}}
  } else error="unknown_wifi_command";
  if(!error)error=manager.apply(candidate,uint32_t(millis()));
  axio::SerialProtocol::reply(out,id,error,"{\"applied\":true,\"persistent\":true}");return true;
}
bool connectivityNetworksJson(char *out,size_t cap) {
  size_t used=snprintf(out,cap,"{\"networks\":[");char encoded[65];const auto &c=manager.get();
  for(unsigned i=0;i<c.count&&used<cap;++i){hex(c.networks[i].ssid,encoded);used+=snprintf(out+used,cap-used,"%s{\"ssid_hex\":\"%s\",\"priority\":%u}",i?",":"",encoded,c.networks[i].priority);}
  if(used+3>cap)return false;strcpy(out+used,"]}");return true;
}
const char *updateConnectivityNetwork(const char *ssid,const char *password,unsigned priority,bool remove) {
  if(!ssid||!password||strlen(ssid)>32||strlen(password)>64||priority>255)return "invalid_network";
  candidate=manager.get();unsigned i=0;while(i<candidate.count&&strcmp(candidate.networks[i].ssid,ssid))++i;
  if(remove){if(i==candidate.count)return "network_not_found";for(unsigned k=i+1;k<candidate.count;++k)candidate.networks[k-1]=candidate.networks[k];candidate.networks[--candidate.count]=WifiNetwork();}
  else {if(i==MaxNetworks)return "too_many_networks";WifiNetwork n;strcpy(n.ssid,ssid);strcpy(n.password,password);n.priority=uint8_t(priority);candidate.networks[i]=n;if(i==candidate.count)++candidate.count;}
  return manager.apply(candidate,uint32_t(millis()));
}
void connectivityJson(char *out,size_t cap) {
  char ssid[193]={};if(driver.connected())quoteSsid(WiFi.SSID().c_str(),ssid);
  auto ip=WiFi.localIP();bool ap=(WiFi.getMode()&WIFI_AP)&&WiFi.AP.started();auto apIp=ap?WiFi.softAPIP():IPAddress(0,0,0,0);
  snprintf(out,cap,"{\"sta\":\"%s\",\"sta_connected\":%s,\"ssid\":\"%s\",\"sta_ip\":\"%u.%u.%u.%u\",\"ap_active\":%s,\"ap_ip\":\"%u.%u.%u.%u\",\"mdns_hostname\":\"%s\",\"discovery\":\"%s\",\"networks\":%u,\"attempts\":%llu,\"disconnects\":%llu,\"reconnects\":%llu,\"transitions\":%llu,\"offline_ms\":%u,\"fallback_ms\":15000,\"last_transition\":\"%s\",\"config_source\":\"%s\",\"storage_error\":\"%s\",\"driver_ready\":%s}",
    name(manager.state),driver.connected()?"true":"false",ssid,ip[0],ip[1],ip[2],ip[3],ap?"true":"false",apIp[0],apIp[1],apIp[2],apIp[3],discoveryHostname(),discoveryState(),manager.get().count,
    (unsigned long long)manager.attempts,(unsigned long long)manager.disconnects,(unsigned long long)manager.reconnects,(unsigned long long)manager.transitions,
    unsigned(manager.offlineMs(uint32_t(millis()))),manager.reason,manager.source,manager.storageError?manager.storageError:"",driver.ready?"true":"false");
  size_t n=strlen(out);if(n+8<cap){strcpy(out+n-1,",\"ap\":");n=strlen(out);apRuntimeJson(out+n,cap-n-1);strcat(out,"}");}
  n=strlen(out);if(n+22<cap){strcpy(out+n-1,",\"discovery_details\":");n=strlen(out);discoveryJson(out+n,cap-n-1);strcat(out,"}");}
}
