#pragma once
#include <stddef.h>
#include <stdint.h>
void setupApRuntime();
void tickApRuntime(bool sta,uint32_t offlineMs);
bool apRuntimeCommand(const char *id,char *command,char *out);
void apRuntimeJson(char *out,size_t cap);

void refreshApConfiguration();
void setApTrackerId(const char *id);
const char *apSsid();
void stopApRuntime();
// The full dashboard owns HTTP in both radio modes. No second AP portal.
void setApHttpClose(void (*close)());
void setApHttpStatus(bool active,bool failed=false);
