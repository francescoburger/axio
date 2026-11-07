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
namespace axio { namespace osc {struct Config;} }
const axio::osc::Config &oscConfiguration();
const char *applyDashboardOsc(const char *json,bool apply);

const char *initializeDashboardOsc(const char *hardware);
const char *changeTrackerId(const char *id);
void stopOscRuntime();
uint64_t oscDatagrams();
uint64_t oscErrors();
uint64_t oscWindowUs();
