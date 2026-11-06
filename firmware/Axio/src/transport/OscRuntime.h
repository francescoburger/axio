#pragma once
#include "../diagnostics/DiagnosticService.h"
void setupOscRuntime(bool (*tare)());
void tickOscRuntime(const axio::Sample &sample,uint64_t now);
void pollOscRuntime();
bool tareOscRuntime();
bool oscConfigCommand(const char *id,char *command,char *out);
void resetOscMetrics();
axio::RuntimeStatus oscRuntimeStatus();
void oscMetricsJson(char *out,size_t cap);
uint16_t oscListenPort();
