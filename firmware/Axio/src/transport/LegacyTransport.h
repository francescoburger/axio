#pragma once
#include "../core/Sample.h"
void setupLegacyTransport(void (*tare)());
void processLegacyPortal();
void checkIncomingOSC();
void sendLegacySample(const axio::Sample &sample);
