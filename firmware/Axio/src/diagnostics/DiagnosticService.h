#pragma once
#include "SerialProtocol.h"
#include <math.h>
#include "../core/TrackerState.h"

namespace axio {
struct RuntimeStatus {
  bool wifi=false, engine=false;
  uint32_t heap=0, minHeap=0;
  int deviceId=0;
  unsigned outPort=0, listenPort=0;
  unsigned ip[4]={0,0,0,0};
};
// Platform seam: runtime status, monotonic time and the sole calibration entry.
template<class Platform> class DiagnosticService {
  TrackerState &tracker;
  Platform &platform;
  uint64_t epochUs=0;
  char data[5000]={};
  template<class P> static auto config(P &p,const char *id,char *cmd,char *out,int)
      -> decltype(p.configCommand(id,cmd,out),bool()) {return p.configCommand(id,cmd,out);}
  template<class P> static bool config(P &,const char *,char *,char *,long) {return false;}
  template<class P> static auto reset(P &p,int) -> decltype(p.resetMetrics(),void()) {p.resetMetrics();}
  template<class P> static void reset(P &,long) {}
  template<class P> static auto metrics(P &p,char *out,size_t cap,int) -> decltype(p.metricsJson(out,cap),void()) {p.metricsJson(out,cap);}
  template<class P> static void metrics(P &,char *out,size_t,long) {strcpy(out,"null");}
  template<class P> static auto connectivity(P &p,char *out,size_t cap,int) -> decltype(p.connectivityJson(out,cap),void()) {p.connectivityJson(out,cap);}
  template<class P> static void connectivity(P &,char *out,size_t,long) {strcpy(out,"null");}
 public:
  DiagnosticService(TrackerState &t, Platform &p):tracker(t),platform(p) {}
  void startWindow(){if(!tracker.diagnostics.attempts)epochUs=platform.nowUs();}
  uint64_t windowUs(){return platform.nowUs()-epochUs;}
  void resetMetricsNow(){
    reset(platform,0);auto &d=tracker.diagnostics;uint64_t errors=d.initErrors;
    d=Diagnostics();d.initErrors=errors;epochUs=platform.nowUs();
  }
  void execute(const char *id, char *command, char *out) {
    if(config(platform,id,command,out,0))return;
    const char *error=nullptr;
    data[0]=0;
    auto &d=tracker.diagnostics;
    uint64_t now=platform.nowUs();
    if (!strcmp(command,"PING")) strcpy(data,"{\"pong\":true}");
    else if (!strcmp(command,"INFO")) snprintf(data,sizeof(data),
      "{\"firmware\":\"Axio\",\"version\":\"4.0.0-dev\",\"protocol\":1,\"board\":\"XIAO_ESP32C3\",\"build\":\"%s %s\",\"cdc_on_boot\":true,\"config_engine\":%s}",__DATE__,__TIME__,platform.status().engine?"true":"false");
    else if (!strcmp(command,"STATUS")) {
      RuntimeStatus s=platform.status();
      const char *states[]={"uninitialized","ready","init_failed","read_error"};
      snprintf(data,sizeof(data),"{\"imu\":\"%s\",\"sample_valid\":%s,\"sequence\":%llu,\"sample_timestamp_us\":%llu,\"wifi_connected\":%s,\"osc\":\"%s\",\"heap_free\":%u,\"heap_min\":%u,\"uptime_us\":%llu}",
        states[unsigned(tracker.sensor)],tracker.latest.valid?"true":"false",
        (unsigned long long)tracker.latest.sequence,(unsigned long long)tracker.latest.timestampUs,
        s.wifi?"true":"false",s.engine?"configurable":"transitional",unsigned(s.heap),unsigned(s.minHeap),(unsigned long long)now);
    } else if (!strcmp(command,"METRICS")) {
      double seconds=double(now-epochUs)/1e6;
      snprintf(data,sizeof(data),"{\"window_us\":%llu,\"acquisition_attempts\":%llu,\"acquired\":%llu,\"snapshots\":%llu,\"read_errors\":%llu,\"init_errors\":%llu,\"calibration_requests\":%llu,\"calibration_errors\":%llu,\"osc_attempts\":%llu,\"acquisition_hz\":%.6f,\"snapshot_hz\":%.6f,\"osc_attempt_hz\":%.6f,\"osc_requested_hz\":%s,\"udp_sent\":null,\"host_received\":null,\"max_read_us\":%llu,\"max_processing_us\":%llu,\"max_send_us\":%llu,\"last_interval_us\":%llu,\"max_interval_us\":%llu,\"interval_mean_us\":%.3f,\"interval_stddev_us\":%.3f}",
        (unsigned long long)(now-epochUs),(unsigned long long)d.attempts,(unsigned long long)d.acquired,
        (unsigned long long)d.acquired,(unsigned long long)d.readErrors,(unsigned long long)d.initErrors,
        (unsigned long long)d.calibrationRequests,(unsigned long long)d.calibrationErrors,(unsigned long long)d.sendAttempts,
        seconds>0?d.acquired/seconds:0,seconds>0?d.acquired/seconds:0,seconds>0?d.sendAttempts/seconds:0,
        platform.status().engine?"null":"100",(unsigned long long)d.maxReadUs,(unsigned long long)d.maxProcessUs,(unsigned long long)d.maxSendUs,
        (unsigned long long)d.lastIntervalUs,(unsigned long long)d.maxIntervalUs,d.intervalMeanUs,d.attempts>1?sqrt(d.intervalM2Us/double(d.attempts-1)):0);
      if(platform.status().engine) {
        size_t n=strlen(data);
        strcpy(data+n-1,",\"engine\":");
        size_t offset=strlen(data);
        metrics(platform,data+offset,sizeof(data)-offset-1,0);
        strcat(data,"}");
      }
    } else if (!strcmp(command,"CALIBRATION")||!strcmp(command,"TARE")) {
      if (!platform.calibrate()) error="imu_unavailable";
      else strcpy(data,!strcmp(command,"TARE")?"{\"tared\":true,\"persistent\":false}":"{\"calibrated\":true,\"persistent\":false}");
    } else if (!strcmp(command,"DIAG RESET")) {
      resetMetricsNow();
      strcpy(data,"{\"reset\":true,\"preserved\":[\"init_errors\",\"sequence\",\"heap_min\"]}");
    } else if (!strcmp(command,"CONFIG GET")) {
      RuntimeStatus s=platform.status();
      snprintf(data,sizeof(data),"{\"schema\":0,\"engine\":\"transitional\",\"editable\":false,\"id\":%d,\"destination\":{\"ip\":\"%u.%u.%u.%u\",\"port\":%u},\"listen_port\":%u,\"calibration_address\":\"/calibration\"}",s.deviceId,s.ip[0],s.ip[1],s.ip[2],s.ip[3],s.outPort,s.listenPort);
    } else if ((!strncmp(command,"CONFIG VALIDATE",15) && (command[15]==' ' || !command[15])) ||
               (!strncmp(command,"CONFIG APPLY",12) && (command[12]==' ' || !command[12]))) {
      size_t n=!strncmp(command,"CONFIG VALIDATE",15)?15:12;
      if (command[n]!=' ' || !JsonSyntax(command+n+1).object()) error="invalid_json";
      else error="config_engine_unavailable";
    } else error="unknown_command";
    if(!error&&(!strcmp(command,"INFO")||!strcmp(command,"STATUS")||!strcmp(command,"METRICS"))) {
      size_t n=strlen(data);strcpy(data+n-1,",\"connectivity\":");n=strlen(data);
      connectivity(platform,data+n,sizeof(data)-n-1,0);strcat(data,"}");
    }
    SerialProtocol::reply(out,id,error,data);
  }
};
}
