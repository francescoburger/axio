// ISOLATED API PROBE, not Axio firmware and not an election implementation.
// Credentials arrive as USB hex input, live only in RAM, and are never echoed.
// Do not flash over an active tracker without arranging its restoration first.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <mdns.h>
#include <Preferences.h>
#include "../../../firmware/Axio/src/connectivity/ConnectivityManager.h"
WiFiServer server(80);WiFiClient client;char headers[512]={};size_t headerSize=0;uint32_t clientSince=0;
struct ReadOnlyStore {
 Preferences prefs;
 bool exists(){return prefs.isKey("slot0")||prefs.isKey("slot1");}
 size_t read(int slot,uint8_t *p,size_t n){return prefs.getBytesLength(slot?"slot1":"slot0")<=n?prefs.getBytes(slot?"slot1":"slot0",p,n):0;}
 uint64_t committed(){return prefs.getULong64("active",~uint64_t(0));}
} store;
char input[240]={},hostname[32]={};size_t used=0;bool running=false,claimed=false;
int digit(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1;}
bool decode(const char *p,char *out,size_t cap){
  size_t n=strlen(p);if(n%2||n/2>=cap)return false;
  for(size_t i=0;i<n;i+=2){int a=digit(p[i]),b=digit(p[i+1]);if(a<0||b<0||!(a*16+b))return false;out[i/2]=char(a*16+b);}out[n/2]=0;return true;
}
void result(const char *action,esp_err_t error){Serial.printf("{\"action\":\"%s\",\"result\":%d,\"uptime_ms\":%lu}\n",action,int(error),(unsigned long)millis());}
void command(char *line){
  if(!strncmp(line,"SAVED ",6)){
    char requested[33]={};axio::connectivity::Config saved;
    if(!decode(line+6,requested,sizeof(requested))||!store.prefs.begin("axio-wifi-v1",true)){result("saved",ESP_ERR_INVALID_ARG);return;}
    axio::connectivity::Journal<ReadOnlyStore> journal(store);bool loaded=journal.load(saved);store.prefs.end();
    if(!loaded){result("saved",ESP_ERR_INVALID_STATE);return;}
    for(unsigned i=0;i<saved.count;++i)if(!strcmp(saved.networks[i].ssid,requested)){
      if(running)mdns_free();running=claimed=false;esp_wifi_disconnect();
      wifi_config_t config={};const auto &n=saved.networks[i];memcpy(config.sta.ssid,n.ssid,strlen(n.ssid));memcpy(config.sta.password,n.password,strlen(n.password));
      config.sta.threshold.authmode=*n.password?WIFI_AUTH_WPA2_PSK:WIFI_AUTH_OPEN;
      auto error=esp_wifi_set_config(WIFI_IF_STA,&config);if(error==ESP_OK)error=esp_wifi_connect();
      saved=axio::connectivity::Config();memset(&config,0,sizeof(config));result("saved",error);return;
    }
    saved=axio::connectivity::Config();result("saved",ESP_ERR_NOT_FOUND);return;
  }
  if(!strncmp(line,"JOIN ",5)){
    char *password=strchr(line+5,' ');char ssid[33]={},secret[65]={};
    if(!password){result("join",ESP_ERR_INVALID_ARG);return;}*password++=0;
    if(!decode(line+5,ssid,sizeof(ssid))||(!strcmp(password,"-")?false:!decode(password,secret,sizeof(secret)))){result("join",ESP_ERR_INVALID_ARG);return;}
    if(running)mdns_free();running=claimed=false;esp_wifi_disconnect();
    wifi_config_t config={};memcpy(config.sta.ssid,ssid,strlen(ssid));memcpy(config.sta.password,secret,strlen(secret));
    config.sta.threshold.authmode=*secret?WIFI_AUTH_WPA2_PSK:WIFI_AUTH_OPEN;
    esp_err_t error=esp_wifi_set_config(WIFI_IF_STA,&config);if(error==ESP_OK)error=esp_wifi_connect();
    memset(&config,0,sizeof(config));memset(secret,0,sizeof(secret));result("join",error);return;
  }
  if(!strcmp(line,"STATUS")){
    char actual[64]={};if(running)mdns_hostname_get(actual);
    Serial.printf("{\"actual_hostname\":\"%s\",\"sta\":%s,\"ip\":\"%s\",\"individual\":\"%s.local\",\"claimed\":%s,\"heap\":%u,\"heap_min\":%u}\n",actual,WiFi.status()==WL_CONNECTED?"true":"false",WiFi.localIP().toString().c_str(),hostname,claimed?"true":"false",unsigned(ESP.getFreeHeap()),unsigned(ESP.getMinFreeHeap()));return;
  }
  if(!running){result("mdns_not_ready",ESP_ERR_INVALID_STATE);return;}
  if(!strcmp(line,"CLAIM")){
    mdns_ip_addr_t address={};address.addr.type=ESP_IPADDR_TYPE_V4;address.addr.u_addr.ip4.addr=uint32_t(WiFi.localIP());
    auto error=mdns_delegate_hostname_add("axio",&address);if(error==ESP_OK)claimed=true;result("claim",error);return;
  }
  if(!strcmp(line,"RELEASE")){auto error=mdns_delegate_hostname_remove("axio");if(error==ESP_OK)claimed=false;result("release",error);return;}
  if(!strncmp(line,"QUERY ",6)){
    esp_ip4_addr_t address={};auto error=mdns_query_a(line+6,300,&address);
    Serial.printf("{\"query_result\":%d,\"ipv4_raw\":%lu}\n",int(error),(unsigned long)address.addr);return;
  }
  result("unknown_command",ESP_ERR_INVALID_ARG);
}
void setup(){
  Serial.begin(115200);uint8_t mac[6]={};esp_read_mac(mac,ESP_MAC_WIFI_STA);
  snprintf(hostname,sizeof(hostname),"axio-%02x%02x%02x%02x%02x%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  WiFi.setHostname(hostname);WiFi.setAutoReconnect(false);WiFi.STA.begin(false);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);WiFi.STA.config();esp_wifi_disconnect();
}
void loop(){
  if(WiFi.status()==WL_CONNECTED&&!running){
    esp_err_t error=mdns_init();if(error==ESP_OK)error=mdns_hostname_set(hostname);
    if(error==ESP_OK)error=mdns_service_add(nullptr,"_http","_tcp",80,nullptr,0);
    running=error==ESP_OK;if(running)server.begin();result("mdns_start",error);if(!running)mdns_free();
  }
  if(running){
    if(!client){client=server.accept();headerSize=0;clientSince=millis();}
    if(client){
      for(unsigned budget=0;budget<128&&client.available()&&headerSize+1<sizeof(headers);++budget){headers[headerSize++]=char(client.read());headers[headerSize]=0;}
      if(strstr(headers,"\r\n\r\n")){
        char body[128];snprintf(body,sizeof(body),"Axio mDNS isolated probe: %s",hostname);
        client.printf("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %u\r\nConnection: close\r\n\r\n%s",unsigned(strlen(body)),body);client.stop();
      }else if(uint32_t(millis()-clientSince)>1000)client.stop();
    }
  }
  for(unsigned budget=0;budget<64&&Serial.available();++budget){char c=Serial.read();
    if(c=='\n'){input[used]=0;command(input);memset(input,0,sizeof(input));used=0;}
    else if(c!='\r'){if(used+1<sizeof(input))input[used++]=c;else {memset(input,0,sizeof(input));used=0;result("line_too_large",ESP_ERR_INVALID_ARG);}}
  }
}
