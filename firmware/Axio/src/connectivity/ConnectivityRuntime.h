#pragma once
#include <stddef.h>
#include <stdint.h>
void setupConnectivity();
void tickConnectivity();
bool connectivityCommand(const char *id,char *command,char *out);
void connectivityJson(char *out,size_t cap);
bool connectivityNetworksJson(char *out,size_t cap);
const char *updateConnectivityNetwork(const char *ssid,const char *password,unsigned priority,bool remove);

const char *connectivityApPassword();
const char *dashboardNetworkChange(const char *json,bool apply);

namespace axio {namespace connectivity {struct Config;}}
void activateDashboardNetwork();
unsigned dashboardNetworkRevision();
const axio::connectivity::Config &dashboardNetworks();
uint64_t connectivityReconnects();
