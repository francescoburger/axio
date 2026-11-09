// Isolated native-ownership probe. Reads saved networks; writes no NVS.
// No IMU access, calibration, or automatic promotion. Restore Axio after use.
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <mdns.h>
#include "../../../firmware/Axio/src/connectivity/ConnectivityManager.h"
struct ReadOnlyStore {
  Preferences prefs;
  bool exists(){return prefs.isKey("slot0")||prefs.isKey("slot1");}
  size_t read(int slot,uint8_t *out,size_t cap){const char *key=slot?"slot1":"slot0";return prefs.getBytesLength(key)<=cap?prefs.getBytes(key,out,cap):0;}
  uint64_t committed(){return prefs.getULong64("active",~uint64_t(0));}
} store;
axio::connectivity::Config networks;
char hardware[13]={},individual[32]={},input[32]={};unsigned used=0,cursor=0;
bool running=false,lead=false;uint32_t attemptAt=0;
WiFiServer server(80);WiFiClient client;uint32_t clientAt=0;
esp_err_t start(bool leader){
  if(running){mdns_free();running=false;}
  esp_err_t result=mdns_init();if(result!=ESP_OK)return result;
  running=true;lead=leader;
  auto ap=esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");if(ap)mdns_unregister_netif(ap);
  result=mdns_hostname_set(leader?"axio":individual);
  if(result!=ESP_OK)return result;
  mdns_instance_name_set(individual);
  mdns_txt_item_t txt[]={{"model","Axio"},{"schema","1"},{"hw",hardware}};
  if(leader){
    mdns_ip_addr_t address={};address.addr.type=ESP_IPADDR_TYPE_V4;address.addr.u_addr.ip4.addr=uint32_t(WiFi.localIP());
    result=mdns_delegate_hostname_add(individual,&address);if(result!=ESP_OK)return result;
    result=mdns_service_add_for_host(individual,"_axio","_udp",individual,16889,txt,3);if(result!=ESP_OK)return result;
    result=mdns_service_add_for_host(individual,"_http","_tcp",individual,80,nullptr,0);if(result!=ESP_OK)return result;
    // A self-hosted service includes axio.local in native probes and goodbyes.
    result=mdns_service_add(nullptr,"_axio-entry","_udp",16889,txt,3);
  }else{
    result=mdns_service_add(nullptr,"_axio","_udp",16889,txt,3);if(result!=ESP_OK)return result;
    result=mdns_service_add(nullptr,"_http","_tcp",80,nullptr,0);
  }
  return result;
}
void status(){
  char actual[65]={};if(running)mdns_hostname_get(actual);
  Serial.printf("{\"sta\":%s,\"ip\":\"%s\",\"individual\":\"%s.local\",\"actual\":\"%s\",\"lead_requested\":%s,\"heap\":%u,\"heap_min\":%u,\"uptime_ms\":%lu,\"wifi_status\":%d,\"networks\":%u,\"cursor\":%u}\n",WiFi.status()==WL_CONNECTED?"true":"false",WiFi.localIP().toString().c_str(),individual,actual,lead?"true":"false",unsigned(ESP.getFreeHeap()),unsigned(ESP.getMinFreeHeap()),(unsigned long)millis(),int(WiFi.status()),networks.count,cursor);
}
void setup(){
  Serial.begin(115200);uint8_t mac[6]={};esp_read_mac(mac,ESP_MAC_WIFI_STA);
  snprintf(hardware,sizeof(hardware),"%02x%02x%02x%02x%02x%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);snprintf(individual,sizeof(individual),"axio-%s",hardware);
  WiFi.setHostname(individual);WiFi.setAutoReconnect(false);WiFi.STA.begin(false);esp_wifi_set_storage(WIFI_STORAGE_RAM);WiFi.STA.config();esp_wifi_disconnect();
  if(store.prefs.begin("axio-wifi-v1",true)){axio::connectivity::Journal<ReadOnlyStore> journal(store);journal.load(networks);store.prefs.end();}
  // Highest priority first, without persisting the reordered local copy.
  for(unsigned i=0;i<networks.count;++i)for(unsigned j=i;j&&networks.networks[j].priority>networks.networks[j-1].priority;--j){auto n=networks.networks[j];networks.networks[j]=networks.networks[j-1];networks.networks[j-1]=n;}
  attemptAt=millis()-12000;
}
void loop(){
  if(WiFi.status()!=WL_CONNECTED){
    if(running){mdns_free();running=false;}
    if(networks.count&&uint32_t(millis()-attemptAt)>=12000){
      attemptAt=millis();esp_wifi_disconnect();wifi_config_t config={};const auto &n=networks.networks[cursor];
      memcpy(config.sta.ssid,n.ssid,strlen(n.ssid));memcpy(config.sta.password,n.password,strlen(n.password));config.sta.scan_method=WIFI_FAST_SCAN;config.sta.threshold.authmode=*n.password?WIFI_AUTH_WPA2_PSK:WIFI_AUTH_OPEN;config.sta.pmf_cfg.capable=true;
      esp_wifi_set_config(WIFI_IF_STA,&config);esp_wifi_connect();memset(&config,0,sizeof(config));cursor=(cursor+1)%networks.count;
    }
  }else if(!running){Serial.printf("{\"start\":%d}\n",int(start(false)));server.begin();}
  if(running){
    if(!client){client=server.accept();clientAt=millis();}
    if(client){if(client.available()){char body[96];snprintf(body,sizeof(body),"Axio ownership probe %s",hardware);client.printf("HTTP/1.1 200 OK\r\nContent-Length: %u\r\nConnection: close\r\n\r\n%s",unsigned(strlen(body)),body);client.stop();}else if(uint32_t(millis()-clientAt)>1000)client.stop();}
  }
  for(unsigned budget=0;budget<64&&Serial.available();++budget){char ch=Serial.read();if(ch=='\n'){
    input[used]=0;if(!strcmp(input,"STATUS"))status();
    else if(running&&(!strcmp(input,"LEAD")||!strcmp(input,"FOLLOW")))Serial.printf("{\"switch\":%d}\n",int(start(!strcmp(input,"LEAD"))));
    used=0;
  }else if(ch!='\r'&&used+1<sizeof(input))input[used++]=ch;}
}
