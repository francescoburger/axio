#pragma once
#include "../core/Sample.h"
#include "../diagnostics/DiagnosticService.h"
axio::RuntimeStatus legacyRuntimeStatus();
void setupLegacyTransport(void (*tare)());
void checkIncomingOSC();
void sendLegacySample(const axio::Sample &sample);
