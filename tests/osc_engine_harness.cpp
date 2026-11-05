#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include "src/osc/Engine.h"
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
  unsigned tares=0;
  bool send(const Destination &d,const uint8_t *p,size_t n){sent.push_back({d,std::vector<uint8_t>(p,p+n)});return queue;}
  bool tare(){++tares;return valid;}
  bool prepare(uint16_t,uint16_t){staged=prepareOk;return prepareOk;}
  void cancel(){staged=false;}
  void activate(){staged=false;}
};
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
  if(argc>1 && std::string(argv[1])=="--validate") {
    std::string json((std::istreambuf_iterator<char>(std::cin)),{});Config c;const char *error=parse(json.c_str(),c);
    if(error){std::cout<<error<<'\n';return 2;}
    char out[MaxConfig+1];assert(serialize(c,out,sizeof(out)));std::cout<<out<<'\n';return 0;
  }
  if(argc>1 && std::string(argv[1])=="--preset") {
    Config c=preset(argc>2?argv[2]:"Orientation");char out[MaxConfig+1];assert(!validate(c));assert(serialize(c,out,sizeof(out)));std::cout<<out<<'\n';return 0;
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
  c.ackEnabled=true;strcpy(c.tare,"/tare/{id}");strcpy(c.id,"test");engine.activate(c);net.sent.clear();net.queue=true;
  Packet command;command.string("/tare/test");command.string(",");
  assert(engine.receive(command.data,command.size));assert(net.sent.size()==1 && net.tares==1);
  Packet ack;ack.string("/tared/test");ack.string(",i");ack.integer(1);
  assert(net.sent[0].packet==std::vector<uint8_t>(ack.data,ack.data+ack.size));
  net.valid=false;assert(!engine.receive(command.data,command.size));assert(net.sent.size()==1 && net.tares==2);
  assert(!engine.receive(command.data,command.size-1));assert(net.tares==2);
  command.data[command.size-1]=1;assert(!engine.receive(command.data,command.size));assert(net.tares==2);
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
