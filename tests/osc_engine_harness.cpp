#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include "src/osc/Engine.h"
#include "src/osc/Editor.h"
#include "src/osc/Journal.h"
#include "src/osc/Transaction.h"
#include "src/core/Diagnostics.h"
using namespace axio::osc;
struct Store {
  std::vector<uint8_t> slots[2];uint64_t selected=~uint64_t(0);
  bool failWrite=false,failCommit=false,corruptWrite=false;
  size_t read(int slot,uint8_t *out,size_t cap){auto &s=slots[slot];if(s.size()>cap)return 0;memcpy(out,s.data(),s.size());return s.size();}
  bool write(int slot,const uint8_t *data,size_t size){if(failWrite)return false;slots[slot].assign(data,data+size);if(corruptWrite)slots[slot].back()^=1;return true;}
  uint64_t committed(){return selected;}
  bool commit(uint64_t n){if(failCommit)return false;selected=n;return true;}
};
struct Net {
  struct Sent {Destination d;std::vector<uint8_t> packet;};
  std::vector<Sent> sent;
  bool valid=true,queue=true,prepareOk=true,staged=false;
  unsigned calibrations=0;
  bool send(const Destination &d,const uint8_t *p,size_t n){sent.push_back({d,std::vector<uint8_t>(p,p+n)});return queue;}
  bool calibrate(){++calibrations;return valid;}
  bool prepare(uint16_t,uint16_t){staged=prepareOk;return prepareOk;}
  void cancel(){staged=false;}
  void activate(){staged=false;}
};
void numericCalibrationPackets(){
  Net n;Engine<Net> engine(n);Config c=preset("Custom");strcpy(c.calibrationAddress,"/cal");
  c.ackEnabled=true;engine.activate(c);
  for(const char *tag:{",i",",f"}){
    for(uint32_t value:{0u,1u,0xffffffffu,0x3f800000u,0xbf800000u}){
      if(tag[1]=='f'&&(value&0x7f800000u)==0x7f800000u)continue;
      Packet command;command.string("/cal");command.string(tag);command.integer(value);
      unsigned before=n.calibrations;assert(engine.receive(command.data,command.size));
      assert(n.calibrations==before+1);
      for(size_t length=0;length<command.size;++length)assert(!engine.receive(command.data,length));
      command.data[5]=1;assert(!engine.receive(command.data,command.size));command.data[5]=0;
      command.data[11]=1;assert(!engine.receive(command.data,command.size));
      assert(n.calibrations==before+1);
    }
  }
  unsigned before=n.calibrations;
  for(uint32_t value:{0x7f800000u,0xff800000u,0x7fc00000u}){
    Packet command;command.string("/cal");command.string(",f");command.integer(value);
    assert(!engine.receive(command.data,command.size));
  }
  for(const char *tag:{",s",",ff",",d",",T"}){
    Packet command;command.string("/cal");command.string(tag);command.integer(1);
    assert(!engine.receive(command.data,command.size));
  }
  Packet bundle,inner;inner.string("/cal");inner.string(",f");inner.integer(0x3f800000u);
  bundle.bundle();bundle.integer(inner.size);bundle.bytes(inner.data,inner.size);
  assert(!engine.receive(bundle.data,bundle.size));assert(n.calibrations==before);
}
void calibrationCompatibility() {
  Net n;Engine<Net> engine(n);Config c=preset("Custom");strcpy(c.id,"test");
  c.ackEnabled=true;strcpy(c.ackAddress,"/saved-ack/{id}");
  for(const char *address:{"/calibration","/calibration/{id}","/tare","/tare/{id}"}) {
    strcpy(c.calibrationAddress,address);assert(!validate(c));engine.activate(c);n.sent.clear();
    char canonical[AddressSize],alias[AddressSize];expand(address,c.id,canonical,sizeof(canonical));
    assert(calibrationAlias(c,alias,sizeof(alias)));
    unsigned before=n.calibrations;
    for(const char *endpoint:{canonical,alias}){
      Packet command;command.string(endpoint);command.string(",");
      assert(engine.receive(command.data,command.size));
      assert(!engine.receive(command.data,command.size-1));
    }
    assert(n.calibrations==before+2&&n.sent.size()==2);
    Packet ack;ack.string("/saved-ack/test");ack.string(",i");ack.integer(1);
    for(const auto &sent:n.sent)assert(sent.packet==std::vector<uint8_t>(ack.data,ack.data+ack.size));
  }
  strcpy(c.calibrationAddress,"/calibration");strcpy(c.messages[0].address,"/tare");
  assert(!validate(c));engine.activate(c);
  Packet old;old.string("/tare");old.string(",");unsigned before=n.calibrations;
  assert(!engine.receive(old.data,old.size)&&n.calibrations==before); // Output address wins.
  strcpy(c.messages[0].address,"/custom/{id}");strcpy(c.ackAddress,"/tare");
  assert(!validate(c));engine.activate(c);assert(!engine.receive(old.data,old.size));
  strcpy(c.calibrationAddress,"/custom-zero");strcpy(c.ackAddress,"/tared/{id}");
  assert(!validate(c));engine.activate(c);assert(!engine.receive(old.data,old.size));
  // Genuine pre-update journal bytes, including the old key and ACK value.
  char json[MaxConfig+1];assert(serialize(c,json,sizeof(json)));std::string legacy=json;
  legacy.replace(legacy.find("calibration_address"),strlen("calibration_address"),"tare_address");
  Store store;auto &record=store.slots[0];record.resize(16+legacy.size());
  auto put=[&](unsigned offset,uint32_t value){for(unsigned i=0;i<4;++i)record[offset+i]=uint8_t(value>>(8*i));};
  memcpy(record.data()+16,legacy.data(),legacy.size());put(0,0x314f5841);put(4,1);put(8,legacy.size());
  put(12,crc32(record.data()+16,legacy.size()));store.selected=2;
  auto original=record;Journal<Store> journal(store);Config loaded;
  assert(journal.load(loaded)&&!strcmp(loaded.calibrationAddress,"/custom-zero")&&!strcmp(loaded.ackAddress,"/tared/{id}"));
  assert(store.slots[0]==original&&store.slots[1].empty()&&store.selected==2); // Read-only migration.
  store.failCommit=true;assert(!journal.save(loaded));
  {Journal<Store> reboot(store);assert(reboot.load(loaded)&&!strcmp(loaded.calibrationAddress,"/custom-zero"));}
  store.failCommit=false;assert(journal.save(loaded));
  {Journal<Store> reboot(store);assert(reboot.load(loaded)&&!strcmp(loaded.ackAddress,"/tared/{id}"));}
  auto &saved=store.slots[store.selected&1];std::string savedJson(saved.begin()+16,saved.end());
  assert(savedJson.find("calibration_address")!=std::string::npos&&savedJson.find("tare_address")==std::string::npos);
}
int main(int argc,char **argv) {
  axio::Diagnostics diagnostic;
  diagnostic.beginRead(1000);diagnostic.beginRead(11000);diagnostic.beginRead(31000);
  assert(diagnostic.intervalMeanUs==15000 && diagnostic.intervalM2Us==50000000);
  Net jitterNet;Engine<Net> jitterEngine(jitterNet);
  Config jitterConfig=preset("Custom");jitterConfig.messages[0].hz=100;
  jitterEngine.activate(jitterConfig);
  axio::Sample jitterSample;jitterSample.valid=true;
  for(unsigned i=0;i<101;++i) {
    jitterSample.sequence=i+1;
    uint64_t timestamp=100000+i*10000+(i%2?0:400);
    jitterEngine.tick(jitterSample,timestamp);
    jitterEngine.tick(jitterSample,timestamp); // same sequence is still discarded
  }
  assert(jitterNet.sent.size()==101 && jitterEngine.metrics.schedules[0]==101);
  if(argc>1 && std::string(argv[1])=="--calibration"){
    calibrationCompatibility();numericCalibrationPackets();std::cout<<"PASS calibration compatibility\n";return 0;
  }
  if(argc>1 && std::string(argv[1])=="--validate") {
    std::string json((std::istreambuf_iterator<char>(std::cin)),{});Config c;const char *error=parse(json.c_str(),c);
    if(error){std::cout<<error<<'\n';return 2;}
    char out[MaxConfig+1];assert(serialize(c,out,sizeof(out)));std::cout<<out<<'\n';return 0;
  }
  if(argc>1 && std::string(argv[1])=="--preset") {
    Config c=preset(argc>2?argv[2]:"Orientation");char out[MaxConfig+1];assert(!validate(c));assert(serialize(c,out,sizeof(out)));std::cout<<out<<'\n';return 0;
  }
  if(argc>1 && std::string(argv[1])=="--phase4") {
    Editor draft;Config generated;
    strcpy(draft.suffix[1],"real");strcpy(draft.suffix[2],"world");strcpy(draft.suffix[3],"gyro");
    assert(!compileEditor(draft,Config(),generated));assert(generated.messageCount==4);
    assert(generated.messages[0].argCount==7);
    const uint8_t expected[]={4,5,6,0,1,2,3};
    assert(!memcmp(generated.messages[0].args,expected,sizeof(expected)));
    assert(!strcmp(generated.messages[1].address,"/motion/{id}/real"));
    for(unsigned g=0;g<5;++g)draft.suffix[g][0]=0;
    assert(!compileEditor(draft,Config(),generated));
    assert(generated.messageCount==1 && generated.messages[0].argCount==16);
    for(unsigned g=0;g<5;++g)draft.selected[g]=0;
    assert(compileEditor(draft,Config(),generated)); // no empty OSC messages
    draft.selected[0]=1;strcpy(draft.suffix[0],"/bad");
    assert(compileEditor(draft,Config(),generated)); // fixed slash is not user input
    Config legacy=preset("Motion"), modern;
    legacy.destCount=2;legacy.destinations[1].port=19000;
    legacy.messages[0].hz=30;legacy.messages[0].bundle=false;
    assert(!migrateV2(legacy,modern));
    assert(modern.schema==2 && modern.messages[0].intervalMs==34);
    assert(modern.messages[0].routes==3);
    char json[MaxConfig+1];assert(serialize(modern,json,sizeof(json)));
    Config round;assert(!parse(json,round));assert(round.schema==2);
    assert(round.messages[0].intervalMs==34);
    Net n;Engine<Net> e(n);axio::Sample v;v.valid=true;
    for(unsigned period: {10u,11u,13u,30u,65535u}) {
      Config c=modern;c.messageCount=1;c.messages[0].intervalMs=period;
      e.activate(c);n.sent.clear();
      v.sequence++;e.tick(v,100000);assert(n.sent.size()==2);
      assert(n.sent[0].packet[0]=='/'); // exactly one due: never a bundle
      v.sequence++;e.tick(v,100000+period*1000-1);assert(n.sent.size()==2);
      v.sequence++;e.tick(v,100000+period*1000);assert(n.sent.size()==4);
      e.tick(v,100000+period*2000);assert(n.sent.size()==4); // same sequence
      v.sequence++;e.tick(v,1000000000);assert(n.sent.size()==6); // one send, no catch-up
      v.sequence++;e.tick(v,1000000000);assert(n.sent.size()==6);
    }
    modern.messageCount=2;modern.messages[0].intervalMs=11;modern.messages[1].intervalMs=13;
    e.activate(modern);n.sent.clear();v.sequence++;e.tick(v,100000);
    assert(n.sent.size()==2 && n.sent[0].packet[0]=='#');
    v.sequence++;e.tick(v,111000);assert(n.sent.size()==4 && n.sent[2].packet[0]=='/');
    v.sequence++;e.tick(v,113000);assert(n.sent.size()==6 && n.sent[4].packet[0]=='/');
    v.sequence++;e.tick(v,243000);assert(n.sent.size()==8 && n.sent[6].packet[0]=='#');
    // Upgrade shares the same two-slot journal; reboot and failure paths stay transactional.
    Store store;Journal<Store> journal(store);Net txn;Engine<Net> active(txn);
    assert(journal.save(legacy));Config before;
    {Journal<Store> reboot(store);assert(reboot.load(before));assert(before.schema==1);}
    store.failCommit=true;
    assert(!strcmp(apply(modern,txn,journal,active,true),"persistence_failed"));
    {Journal<Store> reboot(store);assert(reboot.load(before));assert(before.schema==1);}
    store.failCommit=false;assert(!apply(modern,txn,journal,active,true));
    {Journal<Store> reboot(store);assert(reboot.load(before));assert(before.schema==2);}
    modern.messages[0].intervalMs=9;assert(validate(modern));
    Config unchanged=legacy;assert(migrateV2(modern,unchanged));assert(unchanged.schema==1);
    std::cout<<"PASS phase4"<<'\n';return 0;
  }
  Net net;Engine<Net> engine(net);Config c=preset("Motion");
  c.destCount=2;c.destinations[1].port=16890;
  for(unsigned i=0;i<c.messageCount;++i)c.messages[i].routes=3;
  assert(!validate(c));engine.activate(c);
  axio::Sample sample;sample.valid=true;sample.sequence=1;sample.axReal=.25f;sample.ayWorld=-2;sample.gz=3;
  engine.tick(sample,10000);assert(net.sent.size()==2 && engine.metrics.udpQueued==2);
  engine.tick(sample,20000);assert(net.sent.size()==2); // no stale duplicates
  sample.sequence++;engine.tick(sample,20000);assert(net.sent.size()==4);
  net.queue=false;sample.sequence++;engine.tick(sample,30000);assert(engine.metrics.udpAttempts==6 && engine.metrics.udpQueued==4);
  if(argc>1 && std::string(argv[1])=="--packets") {
    for(const auto &sent:net.sent){for(auto b:sent.packet)printf("%02x",b);puts("");}return 0;
  }
  c=preset("Custom");c.messages[0].hz=25;c.messages[0].bundle=false;
  c.messages[0].argCount=3;c.messages[0].args[0]=15;c.messages[0].args[1]=11;c.messages[0].args[2]=7;
  engine.activate(c);net.sent.clear();sample.sequence++;engine.tick(sample,100000);
  for(unsigned i=1;i<10;++i){sample.sequence++;engine.tick(sample,100000+i*10000);}
  assert(net.sent.size()==3); // 100,140,180 ms
  Packet ordered;ordered.string("/custom/0");ordered.string(",fff");
  ordered.integer(0x40400000);ordered.integer(0xc0000000);ordered.integer(0x3e800000);
  assert(net.sent[0].packet==std::vector<uint8_t>(ordered.data,ordered.data+ordered.size));
  sample.sequence++;engine.tick(sample,1000000);assert(net.sent.size()==4); // no burst catch-up
  sample.sequence++;engine.tick(sample,1000000);assert(net.sent.size()==4);
  c.ackEnabled=true;strcpy(c.calibrationAddress,"/calibration/{id}");strcpy(c.id,"test");engine.activate(c);net.sent.clear();net.queue=true;
  Packet command;command.string("/calibration/test");command.string(",");
  assert(engine.receive(command.data,command.size));assert(net.sent.size()==1 && net.calibrations==1);
  Packet ack;ack.string("/calibrated/test");ack.string(",i");ack.integer(1);
  assert(net.sent[0].packet==std::vector<uint8_t>(ack.data,ack.data+ack.size));
  net.valid=false;assert(!engine.receive(command.data,command.size));assert(net.sent.size()==1 && net.calibrations==2);
  assert(!engine.receive(command.data,command.size-1));assert(net.calibrations==2);
  command.data[command.size-1]=1;assert(!engine.receive(command.data,command.size));assert(net.calibrations==2);
  engine.resetMetrics();assert(engine.metrics.udpAttempts==0);
  // Failure injection uses the production transaction and journal.
  Store store;Journal<Store> journal(store);Config loaded;
  assert(!journal.load(loaded));assert(journal.save(c));
  strcpy(c.id,"second");assert(journal.save(c));
  {Journal<Store> reboot(store);assert(reboot.load(loaded));assert(!strcmp(loaded.id,"second"));}
  auto committed=store.selected;
  strcpy(c.id,"third");store.failCommit=true;assert(!journal.save(c));store.failCommit=false;
  {Journal<Store> reboot(store);assert(reboot.load(loaded));assert(!strcmp(loaded.id,"second"));}
  assert(store.selected==committed);
  store.corruptWrite=true;assert(!journal.save(c));store.corruptWrite=false;
  assert(journal.save(c)); // committed third, previous second preserved
  unsigned active=store.selected&1;store.slots[active].back()^=1;
  {Journal<Store> reboot(store);assert(reboot.load(loaded));assert(!strcmp(loaded.id,"second"));}
  // Uncommitted newer record must never be used as fallback.
  Store guardStore;Journal<Store> guardJournal(guardStore);
  assert(guardJournal.save(c));
  guardStore.failCommit=true;strcpy(c.id,"uncommitted");assert(!guardJournal.save(c));
  guardStore.slots[guardStore.selected&1].back()^=1;
  {Journal<Store> reboot(guardStore);Config untouched;assert(!reboot.load(untouched));assert(!strcmp(untouched.id,"0"));}
  store.slots[0].clear();store.slots[1].clear();store.selected=~uint64_t(0);
  Store atomicStore;Journal<Store> atomicJournal(atomicStore);Net atomicNet;Engine<Net> atomicEngine(atomicNet);
  c=preset("Orientation");strcpy(c.id,"new");
  atomicNet.prepareOk=false;assert(!strcmp(apply(c,atomicNet,atomicJournal,atomicEngine,true),"listen_failed"));
  assert(!strcmp(atomicEngine.get().id,"0"));
  atomicNet.prepareOk=true;atomicStore.failWrite=true;
  assert(!strcmp(apply(c,atomicNet,atomicJournal,atomicEngine,true),"persistence_failed"));assert(!atomicNet.staged);
  assert(!strcmp(atomicEngine.get().id,"0"));
  atomicStore.failWrite=false;assert(!apply(c,atomicNet,atomicJournal,atomicEngine,true));assert(!strcmp(atomicEngine.get().id,"new"));
  strcpy(c.id,"failed_commit");atomicStore.failCommit=true;
  assert(!strcmp(apply(c,atomicNet,atomicJournal,atomicEngine,true),"persistence_failed"));
  assert(!strcmp(atomicEngine.get().id,"new")&&!atomicNet.staged);
  std::cout<<"PASS\n";
}
