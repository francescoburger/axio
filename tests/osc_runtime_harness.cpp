#include <iostream>
#include <string>
#include <cassert>
#include "WiFi.h"
#include "WiFiUdp.h"
#include "Preferences.h"
#include "esp_timer.h"
#include "src/transport/OscRuntime.h"
#include "src/diagnostics/SerialProtocol.h"
#include "src/osc/Codec.h"
FakeWiFi WiFi;
std::map<std::string,std::vector<uint8_t>> fakeNvs;
bool failNvs=false,failListener=false;
axio::RuntimeStatus legacyRuntimeStatus(){return axio::RuntimeStatus();}
unsigned calibrationCalls=0;bool calibrationOk=true;
bool calibrate(){++calibrationCalls;return calibrationOk;}
struct Platform {
  uint64_t nowUs(){return esp_timer_get_time();}
  axio::RuntimeStatus status(){return oscRuntimeStatus();}
  bool calibrate(){return calibrateOscRuntime();}
  bool configCommand(const char *id,char *cmd,char *out){return oscConfigCommand(id,cmd,out);}
  void resetMetrics(){resetOscMetrics();}
  void metricsJson(char *out,size_t cap){oscMetricsJson(out,cap);}
};
int main(int argc,char **argv) {
  setupOscRuntime(calibrate);
  if(argc>1&&std::string(argv[1])=="--pd-calibration"){
    using namespace axio::osc;
    Config c;strcpy(c.calibrationAddress,"/cal");strcpy(c.ackAddress,"/calok");c.ackEnabled=true;
    Editor editor;assert(!compileEditor(editor,c,c));
    char json[MaxConfig+1],command[MaxConfig+20],out[axio::SerialProtocol::MaxReply];
    assert(serialize(c,json,sizeof(json)));snprintf(command,sizeof(command),"CONFIG APPLY %s",json);
    assert(oscConfigCommand("pd",command,out)&&strstr(out,"\"ok\":true"));
    assert(!strcmp(oscConfiguration().calibrationAddress,"/cal")&&oscConfiguration().ackEnabled);
    auto receive=[&](const Packet &packet){fakeUdp().incoming.push_back({16889,std::vector<uint8_t>(packet.data,packet.data+packet.size)});pollOscRuntime();};
    Packet ack;ack.string("/calok");ack.string(",i");ack.integer(1);
    for(const char *tag:{",f",",i",","}){
      Packet message;message.string("/cal");message.string(tag);
      if(tag[1])message.integer(tag[1]=='f'?0x3f800000u:1u);
      unsigned calls=calibrationCalls,sends=fakeUdp().sends;
      receive(message);assert(calibrationCalls==calls+1&&fakeUdp().sends==sends+1);
      assert(fakeUdp().lastSent==std::vector<uint8_t>(ack.data,ack.data+ack.size));
    }
    Packet message;message.string("/cal");message.string(",f");message.integer(0x3f800000u);
    unsigned calls=calibrationCalls,sends=fakeUdp().sends;
    calibrationOk=false;receive(message);assert(calibrationCalls==calls+1&&fakeUdp().sends==sends);calibrationOk=true;
    Packet wrong;wrong.string("/other");wrong.string(",f");wrong.integer(0x3f800000u);
    calls=calibrationCalls;receive(wrong);assert(calibrationCalls==calls);
    Packet truncated=message;--truncated.size;receive(truncated);assert(calibrationCalls==calls);
    fakeUdp().incoming.push_back({16889,std::vector<uint8_t>(MaxPacket+1,0)});pollOscRuntime();assert(calibrationCalls==calls);
    WiFi.enableAP(true);receive(message);assert(calibrationCalls==calls);fakeUdp().incoming.clear();WiFi.enableAP(false);
    receive(message);assert(calibrationCalls==calls+1);
    std::cout<<"PASS: Pure Data numeric calibration, persisted config, UDP poll, callback and acknowledgement\n";return 0;
  }
  if(argc>1){failNvs=std::string(argv[1])=="--fail-nvs";failListener=std::string(argv[1])=="--fail-listener";}
  Platform platform;axio::TrackerState tracker;
  axio::DiagnosticService<Platform> service(tracker,platform);
  axio::SerialProtocol protocol;char out[axio::SerialProtocol::MaxReply];
  std::string line;
  while(std::getline(std::cin,line)) {
    if(line=="@REBOOT"){setupOscRuntime(calibrate);continue;}
    for(char c:line)protocol.feed(c,0,service,out);
    if(protocol.feed('\n',0,service,out))std::cout<<out;
  }
}
