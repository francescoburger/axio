#pragma once
#include "SerialProtocol.h"
#include "../core/TrackerState.h"

namespace axio {
struct RuntimeStatus {
  bool wifi=false;
  uint32_t heap=0, minHeap=0;
  int deviceId=0;
  unsigned outPort=0, listenPort=0;
  unsigned ip[4]={0,0,0,0};
};
// Platform seam: runtime status, monotonic time and the sole common tare entry.
template<class Platform> class DiagnosticService {
  TrackerState &tracker;
  Platform &platform;
  uint64_t epochUs=0;
 public:
  DiagnosticService(TrackerState &t, Platform &p):tracker(t),platform(p) {}
  void execute(const char *id, char *command, char *out) {
    const char *error=nullptr;
    char data[1600]={};
    auto &d=tracker.diagnostics;
    uint64_t now=platform.nowUs();
    if (!strcmp(command,"PING")) strcpy(data,"{\"pong\":true}");
    else if (!strcmp(command,"INFO")) snprintf(data,sizeof(data),
      "{\"firmware\":\"Axio\",\"version\":\"2.0.0-checkpoint-a\",\"protocol\":1,\"board\":\"XIAO_ESP32C3\",\"build\":\"%s %s\",\"cdc_on_boot\":true,\"config_engine\":false}",__DATE__,__TIME__);
    else if (!strcmp(command,"STATUS")) {
      RuntimeStatus s=platform.status();
      const char *states[]={"uninitialized","ready","init_failed","read_error"};
      snprintf(data,sizeof(data),"{\"imu\":\"%s\",\"sample_valid\":%s,\"sequence\":%llu,\"sample_timestamp_us\":%llu,\"wifi_connected\":%s,\"osc\":\"transitional\",\"heap_free\":%u,\"heap_min\":%u,\"uptime_us\":%llu}",
        states[unsigned(tracker.sensor)],tracker.latest.valid?"true":"false",
        (unsigned long long)tracker.latest.sequence,(unsigned long long)tracker.latest.timestampUs,
        s.wifi?"true":"false",unsigned(s.heap),unsigned(s.minHeap),(unsigned long long)now);
    } else if (!strcmp(command,"METRICS")) {
      double seconds=double(now-epochUs)/1e6;
      snprintf(data,sizeof(data),"{\"window_us\":%llu,\"acquisition_attempts\":%llu,\"acquired\":%llu,\"snapshots\":%llu,\"read_errors\":%llu,\"init_errors\":%llu,\"tare_requests\":%llu,\"tare_errors\":%llu,\"osc_attempts\":%llu,\"acquisition_hz\":%.6f,\"snapshot_hz\":%.6f,\"osc_attempt_hz\":%.6f,\"osc_requested_hz\":100,\"udp_sent\":null,\"host_received\":null,\"max_read_us\":%llu,\"max_processing_us\":%llu,\"max_send_us\":%llu,\"last_interval_us\":%llu,\"max_interval_us\":%llu}",
        (unsigned long long)(now-epochUs),(unsigned long long)d.attempts,(unsigned long long)d.acquired,
        (unsigned long long)d.acquired,(unsigned long long)d.readErrors,(unsigned long long)d.initErrors,
        (unsigned long long)d.tareRequests,(unsigned long long)d.tareErrors,(unsigned long long)d.sendAttempts,
        seconds>0?d.acquired/seconds:0,seconds>0?d.acquired/seconds:0,seconds>0?d.sendAttempts/seconds:0,
        (unsigned long long)d.maxReadUs,(unsigned long long)d.maxProcessUs,(unsigned long long)d.maxSendUs,
        (unsigned long long)d.lastIntervalUs,(unsigned long long)d.maxIntervalUs);
    } else if (!strcmp(command,"TARE")) {
      if (!platform.tare()) error="imu_unavailable";
      else strcpy(data,"{\"tared\":true,\"persistent\":false}");
    } else if (!strcmp(command,"DIAG RESET")) {
      // Reset measurement window only. Keep initialization history and lifetime sequence.
      uint64_t initErrors=d.initErrors;
      d=Diagnostics(); d.initErrors=initErrors; epochUs=now;
      strcpy(data,"{\"reset\":true,\"preserved\":[\"init_errors\",\"sequence\",\"heap_min\"]}");
    } else if (!strcmp(command,"CONFIG GET")) {
      RuntimeStatus s=platform.status();
      snprintf(data,sizeof(data),"{\"schema\":0,\"engine\":\"transitional\",\"editable\":false,\"id\":%d,\"destination\":{\"ip\":\"%u.%u.%u.%u\",\"port\":%u},\"listen_port\":%u,\"tare_address\":\"/tare\"}",s.deviceId,s.ip[0],s.ip[1],s.ip[2],s.ip[3],s.outPort,s.listenPort);
    } else if ((!strncmp(command,"CONFIG VALIDATE",15) && (command[15]==' ' || !command[15])) ||
               (!strncmp(command,"CONFIG APPLY",12) && (command[12]==' ' || !command[12]))) {
      size_t n=!strncmp(command,"CONFIG VALIDATE",15)?15:12;
      if (command[n]!=' ' || !JsonSyntax(command+n+1).object()) error="invalid_json";
      else error="config_engine_unavailable";
    } else error="unknown_command";
    SerialProtocol::reply(out,id,error,data);
  }
};
}
