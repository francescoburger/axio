#pragma once
#include "WiFi.h"
#include <string.h>
constexpr int ESP_OK=0,WIFI_IF_STA=0,WIFI_IF_AP=1,WIFI_MODE_STA=1,WIFI_MODE_APSTA=3,WIFI_CIPHER_TYPE_CCMP=4,WIFI_STORAGE_RAM=1,WIFI_FAST_SCAN=0,WIFI_AUTH_WPA2_PSK=3,WIFI_AUTH_OPEN=0;
struct wifi_config_t {
  struct {uint8_t ssid[32]={},password[64]={};int scan_method=0;struct {int authmode=0;} threshold;struct {bool capable=false;} pmf_cfg;} sta;
  struct {uint8_t ssid[32]={},password[64]={};unsigned ssid_len=0;int authmode=0,pairwise_cipher=0,max_connection=0,channel=0,beacon_interval=0;} ap;
};
struct FakeSdk {
  wifi_config_t legacy,current,ap;bool running=true,failApConfig=false,failRadioStart=false;unsigned openStarts=0;bool ram=false;unsigned flashWrites=0,connects=0,disconnects=0;
};
inline FakeSdk &fakeSdk(){static FakeSdk sdk;return sdk;}
inline int esp_wifi_get_config(int,wifi_config_t *c){*c=fakeSdk().legacy;return ESP_OK;}
inline int esp_wifi_set_storage(int mode){fakeSdk().ram=mode==WIFI_STORAGE_RAM;return ESP_OK;}
inline int esp_wifi_set_config(int iface,const wifi_config_t *c){if(!fakeSdk().ram)++fakeSdk().flashWrites;if(iface==WIFI_IF_AP&&fakeSdk().failApConfig)return -1;if(iface==WIFI_IF_AP)fakeSdk().ap=*c;else fakeSdk().current=*c;return ESP_OK;}
inline int esp_wifi_connect(){++fakeSdk().connects;return ESP_OK;}
inline int esp_wifi_disconnect(){++fakeSdk().disconnects;WiFi.link=0;return ESP_OK;}

inline int esp_wifi_get_mac(int,uint8_t *p){const uint8_t mac[6]={0x12,0x34,0x56,0x78,0x9a,0xbc};memcpy(p,mac,6);return ESP_OK;}
inline int esp_wifi_stop(){fakeSdk().running=false;WiFi.link=0;return ESP_OK;}
inline int esp_wifi_set_mode(int mode){WiFi.mode=mode;return ESP_OK;}
inline int esp_wifi_start(){if(fakeSdk().failRadioStart)return -1;if((WiFi.mode&WIFI_AP)&&!fakeSdk().ap.ap.password[0])++fakeSdk().openStarts;fakeSdk().running=true;return ESP_OK;}
