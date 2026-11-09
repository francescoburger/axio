#include <assert.h>
#include <string>
#include "src/connectivity/DiscoveryManager.h"
#include "src/connectivity/EntryElection.h"
using namespace axio::connectivity;
struct Driver {
  bool waiting=false,done=false,failStart=false,failPort=false,failQuery=false;
  unsigned starts=0,stops=0,disables=0,queries=0,ports=0,accepted=0;
  bool start(uint16_t){++starts;return !failStart;}
  void stop(){assert(!waiting);++stops;}
  void disable(){++disables;}
  bool port(uint16_t){++ports;return !failPort;}
  bool query(){++queries;waiting=!failQuery;return waiting;}
  bool pending(){return waiting;}
  bool poll(PeerCache &,bool accept,uint32_t){if(!waiting||!done)return false;if(accept)++accepted;waiting=false;return true;}
};
void names(){
  uint8_t mac[6]={0,1,2,3,4,5};char hw[13],host[64];identity(mac,hw,host);
  assert(!strcmp(hw,"000102030405")&&!strcmp(host,"axio-000102030405"));
  assert(hostLabel(host,hw));assert(hostLabel("axio-000102030405-2",hw));
  assert(!hostLabel("axio-000102030405-",hw));assert(!hostLabel("axio-000102030405.local",hw));
  assert(!hardwareId("00010203040g"));assert(!hostLabel("axio-000102030406",hw));
}
void cache(){
  PeerCache c;const char *self="111111111111";uint8_t ip[4]={192,168,1,2};
  uint32_t now=0xfffffff0;
  assert(c.observe("000102030405","axio-000102030405",ip,16889,5000,now,self));assert(c.count()==1);
  c.expire(now+89999);assert(c.count()==1);c.expire(now+90000);assert(!c.count());
  assert(c.observe("000102030405","axio-000102030405",ip,16889,2,10,self));
  ip[3]=3;assert(c.observe("000102030405","axio-000102030405-2",ip,16989,2,20,self));
  assert(c.count()==1&&c.get(0).ip[3]==3&&c.get(0).port==16989);
  assert(c.observe("000102030405","axio-000102030405",ip,16989,0,30,self));assert(!c.count());
  assert(!c.observe(self,"axio-111111111111",ip,1,30,0,self));
  assert(!c.observe("bad","bad",ip,1,30,0,self));ip[0]=224;
  assert(!c.observe("000102030405","axio-000102030405",ip,1,30,0,self));
}
void capacity(){
  PeerCache c;uint8_t ip[4]={192,168,1,2};char hw[13],host[64];
  for(unsigned i=0;i<MaxPeers;++i){snprintf(hw,sizeof(hw),"%012x",i);snprintf(host,sizeof(host),"axio-%s",hw);assert(c.observe(hw,host,ip,1,90,0,"ffffffffffff"));}
  assert(c.count()==16);assert(!c.observe("eeeeeeeeeeee","axio-eeeeeeeeeeee",ip,1,90,0,"ffffffffffff"));assert(c.overflow==1);
  c.clear();assert(!c.count());
}
void lifecycle(){
  Driver d;DiscoveryManager<Driver> m(d);uint32_t t=0xfffffff0;
  m.tick(false,1,16889,t);assert(!d.starts);m.tick(true,1,16889,t);assert(m.active&&d.starts==1);
  m.tick(true,1,16889,t+1999);assert(!d.queries);m.tick(true,1,16889,t+2000);assert(d.waiting&&d.queries==1);
  m.tick(false,1,16889,t+2001);assert(m.draining&&!m.active&&d.stops==0&&d.disables==1);
  m.tick(true,2,16889,t+2002);assert(m.draining&&d.starts==1);
  d.done=true;m.tick(true,2,16889,t+2003);assert(!m.running&&d.stops==1&&!d.accepted);
  m.tick(true,2,16889,t+2004);assert(m.active&&d.starts==2);
  m.tick(true,2,16989,t+2005);assert(d.ports==1&&d.starts==2);
  m.tick(true,3,16989,t+2006);assert(!m.running&&d.disables==2&&d.stops==2);
}
void pacing(){
  Driver d;DiscoveryManager<Driver> m(d);m.jitter=999;m.tick(true,1,1,0);
  m.tick(true,1,1,2998);assert(!d.queries);m.tick(true,1,1,2999);assert(d.queries==1);
  for(unsigned i=3000;i<4000;++i)m.tick(true,1,1,i);assert(d.queries==1);
  d.done=true;m.tick(true,1,1,4000);assert(m.completed==1);
  m.tick(true,1,1,33997);assert(d.queries==1);m.tick(true,1,1,33998);assert(d.queries==2);
}
void failures(){
  Driver d;DiscoveryManager<Driver> m(d);d.failStart=true;m.tick(true,1,1,0);assert(m.failures==1);
  m.tick(true,1,1,4999);assert(d.starts==1);d.failStart=false;m.tick(true,1,1,5000);assert(m.active&&d.starts==2);
  d.failQuery=true;m.tick(true,1,1,7000);assert(m.queryFailures==1&&!d.waiting);
  m.tick(true,1,1,7001);assert(d.queries==1);d.failPort=true;m.tick(true,1,2,7002);assert(m.draining&&!m.active);
  m.tick(true,1,2,7003);assert(!m.running);
}
void election(){
  using A=EntryElection::Action;EntryElection e;uint32_t t=0xfffff000;
  assert(e.tick(false,false,"",t)==A::None);
  assert(e.tick(true,false,"individual",t)==A::None);
  assert(e.tick(true,false,"individual",t+2999)==A::None);
  assert(e.tick(true,false,"individual",t+3000)==A::Lookup);
  e.lookup(true,t+3500); // A healthy owner is retained, irrespective of MAC.
  assert(e.tick(true,false,"individual",t+6499)==A::None);
  assert(e.tick(true,false,"individual",t+6500)==A::Lookup);
  e.lookup(false,t+7000);
  assert(e.tick(true,false,"individual",t+7500)==A::Lookup);
  e.lookup(false,t+8000);assert(e.tick(true,false,"individual",t+8001)==A::Census);
  e.census(true,t+9000); // A smaller live candidate goes first.
  assert(e.tick(true,false,"individual",t+10500)==A::Lookup);e.lookup(false,t+11000);
  assert(e.tick(true,false,"individual",t+11500)==A::Lookup);e.lookup(false,t+12000);
  assert(e.tick(true,false,"individual",t+12001)==A::Census);e.census(false,t+13000);
  assert(e.tick(true,false,"individual",t+13001)==A::Lookup);
  e.lookup(false,t+13500);assert(e.tick(true,false,"individual",t+13501)==A::Promote);
  e.promoted(t+14000);assert(!e.leader());
  assert(e.tick(true,true,"axio",t+16999)==A::None&&!e.leader());
  assert(e.tick(true,true,"axio",t+17000)==A::None&&e.leader());
  assert(e.tick(true,true,"axio-2",t+18000)==A::Demote&&e.conflicts==1&&!e.leader());
  e.demoted(t+18000);assert(e.tick(true,false,"individual",t+22999)==A::None);
  assert(e.tick(true,false,"individual",t+23000)==A::Lookup);
  e.failed(t+23001);assert(e.failures==1);
  assert(e.tick(false,true,"axio",t+23002)==A::Demote);
  // A result arriving after radio loss cannot promote.
  e.lookup(false,t+23003);assert(e.state()==EntryElection::State::Offline);
}
int main(int argc,char **argv){assert(argc==2);std::string name=argv[1];if(name=="names")names();else if(name=="cache")cache();else if(name=="capacity")capacity();else if(name=="lifecycle")lifecycle();else if(name=="pacing")pacing();else if(name=="failures")failures();else if(name=="election")election();else return 2;puts("PASS");}
