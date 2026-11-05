#include <iostream>
#include <string>
#include "WiFi.h"
#include "Preferences.h"
#include "esp_timer.h"
#include "src/transport/OscRuntime.h"
#include "src/diagnostics/SerialProtocol.h"
FakeWiFi WiFi;
std::map<std::string,std::vector<uint8_t>> fakeNvs;
bool failNvs=false,failListener=false;
axio::RuntimeStatus legacyRuntimeStatus(){return axio::RuntimeStatus();}
bool tare(){return true;}
struct Platform {
  uint64_t nowUs(){return esp_timer_get_time();}
  axio::RuntimeStatus status(){return oscRuntimeStatus();}
  bool tare(){return tareOscRuntime();}
  bool configCommand(const char *id,char *cmd,char *out){return oscConfigCommand(id,cmd,out);}
  void resetMetrics(){resetOscMetrics();}
  void metricsJson(char *out,size_t cap){oscMetricsJson(out,cap);}
};
int main(int argc,char **argv) {
  setupOscRuntime(tare);
  if(argc>1){failNvs=std::string(argv[1])=="--fail-nvs";failListener=std::string(argv[1])=="--fail-listener";}
  Platform platform;axio::TrackerState tracker;
  axio::DiagnosticService<Platform> service(tracker,platform);
  axio::SerialProtocol protocol;char out[axio::SerialProtocol::MaxReply];
  std::string line;
  while(std::getline(std::cin,line)) {
    if(line=="@REBOOT"){setupOscRuntime(tare);continue;}
    for(char c:line)protocol.feed(c,0,service,out);
    if(protocol.feed('\n',0,service,out))std::cout<<out;
  }
}
