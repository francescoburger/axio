#pragma once
#include <stddef.h>
#include <stdint.h>
void setupDiscoveryIdentity();
void tickDiscovery(bool sta);
const char *discoveryHostname();
const char *discoveryState();
void discoveryJson(char *out,size_t cap);
bool discoveryCommand(const char *id,char *command,char *out);
