#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include "ApRuntime.h"
#include "FallbackAp.h"
#include "ConnectivityRuntime.h"
#include "../diagnostics/SerialProtocol.h"
namespace {
using namespace axio::connectivity;
struct SecretStore {
  Preferences prefs;bool ready=false;
  void begin(){ready=prefs.begin("axio-ap-v1",false);}
  bool available(){return ready;}
  bool exists(){return ready&&prefs.isKey("secret");}
  size_t read(uint8_t *p,size_t n){return ready&&prefs.getBytesLength("secret")<=n?prefs.getBytes("secret",p,n):0;}
  bool write(const uint8_t *p,size_t n){return ready&&prefs.putBytes("secret",p,n)==n;}
} secretStore;
ApCredentials<SecretStore> credentials(secretStore);
void (*closeHttp)()=nullptr;
char ssid[38]={}; // Axio- plus all six MAC bytes.
struct Driver {
  bool armed=false,serving=false;
  uint64_t portalFailures=0;const char *error=nullptr;
  bool enabled(){return (WiFi.getMode()&WIFI_AP)!=0;}
  bool started(){return enabled()&&WiFi.AP.started()&&uint32_t(WiFi.softAPIP())!=0;}
  bool arm(){
    armed=false;
    // Configure while the radio is stopped: never advertise the SDK default/open AP.
    if(esp_wifi_stop()!=ESP_OK){error="ap_radio_stop_failed";return false;}
    wifi_config_t c={};memcpy(c.ap.ssid,ssid,strlen(ssid));c.ap.ssid_len=strlen(ssid);
    const char *selectedPassword=connectivityApPassword();
    if(!*selectedPassword)selectedPassword=credentials.password;
    memcpy(c.ap.password,selectedPassword,strlen(selectedPassword));c.ap.authmode=WIFI_AUTH_WPA2_PSK;
    c.ap.pairwise_cipher=WIFI_CIPHER_TYPE_CCMP;c.ap.max_connection=2;c.ap.channel=1;c.ap.beacon_interval=100;
    bool configured=esp_wifi_set_mode(WIFI_MODE_APSTA)==ESP_OK&&esp_wifi_set_config(WIFI_IF_AP,&c)==ESP_OK;
    bool station=esp_wifi_set_mode(WIFI_MODE_STA)==ESP_OK;
    // Restart only in STA mode, even if secure AP configuration failed.
    bool restarted=station&&esp_wifi_start()==ESP_OK;
    armed=configured&&restarted;error=armed?nullptr:"ap_profile_failed";return armed;
  }
  bool start(){if(!armed||!credentials.ready)return false;return WiFi.enableAP(true);}
  bool stop(){bool stopped=WiFi.enableAP(false);if(stopped)closeHttpNow();return stopped;}
  void closeHttpNow(){if(serving&&closeHttp)closeHttp();serving=false;}
  // A failed radio stop leaves AP reachable. Keep HTTP alive between retries,
  // instead of dropping every in-flight dashboard request on each STA tick.
  void closePortal(){if(!started())closeHttpNow();}
  void pollPortal(uint32_t){/* HTTP is serviced by tickDashboard after IMU work. */}
} driver;
FallbackAp<Driver> fallback(driver);
}
void setupApRuntime(){
  uint8_t mac[6]={};
  bool identity=esp_wifi_get_mac(WIFI_IF_STA,mac)==ESP_OK;
  if(identity)snprintf(ssid,sizeof(ssid),"Axio-%02x%02x%02x%02x%02x%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  secretStore.begin();credentials.begin();
  fallback.provisioned=identity&&credentials.ready&&driver.arm();
  if(!identity)driver.error="ap_identity_failed";
}
void tickApRuntime(bool sta,uint32_t offlineMs){fallback.tick(sta,offlineMs,uint32_t(millis()));}
bool apRuntimeCommand(const char *id,char *command,char *out){
  if(!strcmp(command,"AP CREDENTIALS")){
    if(!credentials.ready)axio::SerialProtocol::reply(out,id,credentials.error?credentials.error:"ap_credentials_unavailable");
    else {char response[192];snprintf(response,sizeof(response),"{\"ssid\":\"%s\",\"password\":\"%s\",\"security\":\"WPA2-PSK\",\"setup_url\":\"http://192.168.4.1/\"}",ssid,*connectivityApPassword()?connectivityApPassword():credentials.password);axio::SerialProtocol::reply(out,id,nullptr,response);}
    return true;
  }
  if(strcmp(command,"AP REKEY"))return false;
  if(*connectivityApPassword()){axio::SerialProtocol::reply(out,id,"ap_password_managed_by_network");return true;}
  // Explicit physical-USB recovery only; never invoked by web or normal transitions.
  driver.closeHttpNow();fallback.provisioned=false;
  if(driver.enabled()&&!driver.stop()){axio::SerialProtocol::reply(out,id,"ap_stop_failed");return true;}
  bool saved=credentials.replace();fallback.provisioned=saved&&ssid[0]&&driver.arm();
  axio::SerialProtocol::reply(out,id,fallback.provisioned?nullptr:(credentials.error?credentials.error:"ap_profile_failed"),"{\"restored\":true,\"persistent\":true}");return true;
}
void apRuntimeJson(char *out,size_t cap){
  auto ip=driver.enabled()?WiFi.softAPIP():IPAddress(0,0,0,0);
  snprintf(out,cap,"{\"enabled\":%s,\"ready\":%s,\"ssid\":\"%s\",\"ip\":\"%u.%u.%u.%u\",\"http_active\":%s,\"starts\":%llu,\"stops\":%llu,\"failures\":%llu,\"http_failures\":%llu,\"last_fallback_ms\":%u,\"last_transition\":\"%s\",\"error\":\"%s\"}",
    driver.enabled()?"true":"false",fallback.provisioned?"true":"false",ssid,ip[0],ip[1],ip[2],ip[3],driver.serving?"true":"false",
    (unsigned long long)fallback.starts,(unsigned long long)fallback.stops,(unsigned long long)fallback.failures,(unsigned long long)driver.portalFailures,
    unsigned(fallback.lastFallbackMs),fallback.reason,credentials.error?credentials.error:(driver.error?driver.error:""));
}

const char *apSsid(){return ssid;}
void refreshApConfiguration(){
  driver.closeHttpNow();fallback.provisioned=false;
  if(driver.enabled()&&!driver.stop()){driver.error="ap_stop_failed";return;}
  fallback.provisioned=credentials.ready&&ssid[0]&&driver.arm();
}
void setApTrackerId(const char *id){
  char next[38];snprintf(next,sizeof(next),"Axio-%s",id);
  if(!strcmp(next,ssid))return;
  snprintf(ssid,sizeof(ssid),"%s",next);refreshApConfiguration();
}
void stopApRuntime(){fallback.provisioned=false;driver.closeHttpNow();driver.stop();}
void setApHttpClose(void (*close)()){closeHttp=close;}
void setApHttpStatus(bool active,bool failed){
  driver.serving=active;
  if(failed){++driver.portalFailures;driver.error="ap_http_bind_failed";}
  else if(active&&driver.error&&!strcmp(driver.error,"ap_http_bind_failed"))driver.error=nullptr;
}
