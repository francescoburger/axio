#pragma once
#include <stddef.h>
void setupConnectivity();
void tickConnectivity();
bool connectivityCommand(const char *id,char *command,char *out);
void connectivityJson(char *out,size_t cap);
bool connectivityNetworksJson(char *out,size_t cap);
const char *updateConnectivityNetwork(const char *ssid,const char *password,unsigned priority,bool remove);
