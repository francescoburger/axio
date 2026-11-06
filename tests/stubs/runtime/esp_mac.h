#pragma once
#include "esp_wifi.h"
constexpr int ESP_MAC_WIFI_STA=0;
inline int esp_read_mac(uint8_t *out,int){return esp_wifi_get_mac(WIFI_IF_STA,out);}
