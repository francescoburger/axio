#include <iostream>
#include <string>
#include <assert.h>
#include "WiFi.h"
#include "WiFiUdp.h"
#include "Preferences.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "mdns.h"
#include "src/connectivity/ConnectivityRuntime.h"
#include "src/transport/OscRuntime.h"
FakeWiFi WiFi;
std::map<std::string,std::vector<uint8_t>> fakeNvs;
bool failNvs=false,failListener=false;
axio::RuntimeStatus legacyRuntimeStatus(){axio::RuntimeStatus s;s.wifi=WiFi.status()==WL_CONNECTED;return s;}
bool tare(){return true;}
struct Platform {
  uint64_t nowUs(){return uint64_t(millis())*1000;}
  axio::RuntimeStatus status(){return oscRuntimeStatus();}
  bool tare(){return tareOscRuntime();}
  bool configCommand(const char *id,char *cmd,char *out){return connectivityCommand(id,cmd,out)||oscConfigCommand(id,cmd,out);}
  void resetMetrics(){resetOscMetrics();}
  void metricsJson(char *out,size_t cap){oscMetricsJson(out,cap);}
  void connectivityJson(char *out,size_t cap){::connectivityJson(out,cap);}
};
int main(){
  strcpy(reinterpret_cast<char *>(fakeSdk().legacy.sta.ssid),"old-ssid");
  strcpy(reinterpret_cast<char *>(fakeSdk().legacy.sta.password),"secret-migration");
  setupConnectivity();setupOscRuntime(tare);
  Platform p;axio::TrackerState tracker;axio::DiagnosticService<Platform> service(tracker,p);
  axio::SerialProtocol protocol;char out[axio::SerialProtocol::MaxReply];std::string line;
  axio::Sample sample;sample.valid=true;
  while(std::getline(std::cin,line)) {
    if(line=="@DOWN"){WiFi.link=0;tickConnectivity();pollOscRuntime();assert(fakeUdp().bound.empty());continue;}
    if(line=="@UP"){WiFi.link=WL_CONNECTED;tickConnectivity();pollOscRuntime();assert(fakeUdp().bound.size()==(fakeUdp().failAll?0:1));continue;}
    if(line=="@IP"){WiFi.ip=IPAddress(192,168,1,20);tickConnectivity();pollOscRuntime();assert(fakeUdp().bound.size()==1);assert(fakeUdp().bound.begin()->second==uint32_t(WiFi.ip));continue;}
    if(line=="@DISC_ADVANCE"){fakeMillis()+=5000;tickConnectivity();continue;}
    if(line=="@DISC_PEER"){fakeMdns().peer=true;fakeMdns().ready=true;tickConnectivity();continue;}
    if(line=="@DISC_BAD"){fakeMdns().badSchema=true;continue;}
    if(line=="@DISC_AP"){fakeMdns().fromAp=true;continue;}
    if(line=="@DISC_FAIL"){fakeMdns().failInit=true;continue;}
    if(line=="@DISC_OK"){fakeMdns().failInit=false;continue;}
    if(line=="@DISC_SERVICE_FAIL"){fakeMdns().failService=true;continue;}
    if(line=="@DISC_SERVICE_OK"){fakeMdns().failService=false;continue;}
    if(line=="@DISC_PORT_FAIL"){fakeMdns().failPort=true;continue;}
    if(line=="@DISC_QUERY_FAIL"){fakeMdns().failQuery=true;continue;}
    if(line=="@DISC_RENAME"){fakeMdns().hostname="axio-123456789abc-2";fakeMillis()+=1000;tickConnectivity();continue;}
    if(line=="@AP_ADVANCE"){fakeMillis()+=15000;tickConnectivity();pollOscRuntime();assert(fakeSdk().ap.ap.authmode==WIFI_AUTH_WPA2_PSK);assert(fakeSdk().ap.ap.password[0]);continue;}
    if(line=="@APSTOPFAIL"){WiFi.failApStop=true;continue;}
    if(line=="@APSTOPOK"){WiFi.failApStop=false;continue;}
    if(line=="@APSTA"){WiFi.link=WL_CONNECTED;continue;}
    if(line=="@CORRUPT_AP"){fakeNvs["axio-ap-v1/secret"][0]^=1;continue;}
    if(line=="@WEBSET"){assert(!updateConnectivityNetwork("web-network","web-password",222,false));continue;}
    if(line=="@WEBDELETE"){assert(!updateConnectivityNetwork("web-network","",0,true));continue;}
    if(line=="@AP_PROFILE_FAIL"){fakeSdk().failApConfig=true;continue;}
    if(line=="@AP_RADIO_FAIL"){fakeSdk().failRadioStart=true;continue;}
    if(line=="@BINDFAIL"){fakeUdp().failAll=true;continue;}
    if(line=="@BINDOK"){fakeUdp().failAll=false;continue;}
    if(line=="@TIMEOUT"){fakeMillis()+=1000;pollOscRuntime();continue;}
    if(line=="@FAIL_NVS"){failNvs=true;continue;}
    if(line=="@OK_NVS"){failNvs=false;continue;}
    if(line=="@TICK"){
      ++sample.sequence;sample.timestampUs=uint64_t(fakeMillis())*1000;unsigned sends=fakeUdp().sends,polls=fakeUdp().polls;
      tickConnectivity();tickOscRuntime(sample,sample.timestampUs);pollOscRuntime();fakeMillis()+=10;
      if(WiFi.link!=WL_CONNECTED){assert(sends==fakeUdp().sends);assert(polls==fakeUdp().polls);}
      continue;
    }
    if(line=="@REBOOT"){
      // Fresh process objects are tested in the policy harness; here reload the production adapters.
      setupConnectivity();setupOscRuntime(tare);continue;
    }
    for(char c:line)protocol.feed(c,millis(),service,out);
    if(protocol.feed('\n',millis(),service,out))std::cout<<out;
  }
  assert(!fakeSdk().flashWrites);assert(!fakeSdk().openStarts);
  assert(!strcmp(reinterpret_cast<char *>(fakeSdk().legacy.sta.password),"secret-migration"));
}
