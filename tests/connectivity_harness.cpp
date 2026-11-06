#include <assert.h>
#include <stdio.h>
#include <string>
#include <vector>
#include "src/connectivity/ConnectivityManager.h"
using namespace axio::connectivity;
struct Store {
  std::vector<uint8_t> slots[2];uint64_t selector=~uint64_t(0);
  bool failWrite=false,failCommit=false,corruptRead=false;
  bool exists(){return !slots[0].empty()||!slots[1].empty();}
  size_t read(int i,uint8_t *out,size_t cap){auto &s=slots[i];if(s.size()>cap)return 0;memcpy(out,s.data(),s.size());if(corruptRead&&s.size())out[0]^=1;return s.size();}
  bool write(int i,const uint8_t *p,size_t n){if(failWrite)return false;slots[i].assign(p,p+n);return true;}
  uint64_t committed(){return selector;}
  bool commit(uint64_t v){if(failCommit)return false;selector=v;return true;}
};
struct Driver {
  bool up=false,accept=true;unsigned stops=0;std::vector<std::string> tries;
  bool connected()const{return up;}
  bool connect(const WifiNetwork &n){tries.emplace_back(n.ssid);return accept;}
  void disconnect(){++stops;up=false;}
};
WifiNetwork net(const char *ssid,unsigned p=0){WifiNetwork n;strcpy(n.ssid,ssid);strcpy(n.password,"password");n.priority=p;return n;}
Config config(){Config c;c.count=3;c.networks[0]=net("low",1);c.networks[1]=net("high",20);c.networks[2]=net("tie",20);return c;}
void priority(){
  Store s;Journal<Store> j(s);Driver d;ConnectivityManager<Driver,decltype(j)> m(d,j);
  assert(j.save(config()));m.begin(nullptr,0);m.tick(0);assert(d.tries.back()=="high");
  m.tick(9999);assert(d.tries.size()==1);m.tick(10000);m.tick(10200);m.tick(10201);assert(d.tries.back()=="tie");
  m.tick(20201);m.tick(20401);m.tick(20402);assert(d.tries.back()=="low");
  Config c=config();c.networks[1]=c.networks[0];assert(!strcmp(validate(c),"duplicate_ssid"));
}
void timing(){
  Store s;Journal<Store> j(s);Driver d;ConnectivityManager<Driver,decltype(j)> m(d,j);
  WifiNetwork n=net("one");uint32_t t=0xfffffff0;m.begin(&n,t);m.tick(t);
  m.tick(t+9999);assert(d.tries.size()==1);m.tick(t+10000);assert(m.state==State::Disconnecting);
  m.tick(t+10200);m.tick(t+11199);assert(d.tries.size()==1);m.tick(t+11200);assert(d.tries.size()==2);
  m.tick(t+21200);m.tick(t+21400);m.tick(t+23399);assert(d.tries.size()==2);m.tick(t+23400);assert(d.tries.size()==3);
  Store capStore;Journal<Store> capJournal(capStore);Driver capDriver;
  ConnectivityManager<Driver,decltype(capJournal)> capped(capDriver,capJournal);
  uint32_t now=0;capped.begin(&n,now);capped.tick(now);
  const unsigned waits[]={1000,2000,4000,8000,16000,30000,30000};
  for(unsigned w:waits){
    now+=10000;capped.tick(now);now+=200;capped.tick(now);
    auto count=capDriver.tries.size();capped.tick(now+w-1);assert(capDriver.tries.size()==count);
    now+=w;capped.tick(now);assert(capDriver.tries.size()==count+1);
  }
  // Large time jumps still produce at most one attempt per tick.
  auto before=d.tries.size();m.tick(t+1000000);assert(d.tries.size()<=before+1);
  // A real association needing more than the old 5 s budget keeps its priority.
  Store slowStore;Journal<Store> slowJournal(slowStore);Driver slowDriver;
  ConnectivityManager<Driver,decltype(slowJournal)> slow(slowDriver,slowJournal);
  assert(slowJournal.save(config()));slow.begin(nullptr,0);slow.tick(0);slow.tick(6500);
  assert(slowDriver.stops==0&&slowDriver.tries.size()==1&&slowDriver.tries[0]=="high");
  slowDriver.up=true;slow.tick(6501);assert(slow.connected()&&slow.attempts==1);
}
void reconnect(){
  Store s;Journal<Store> j(s);Driver d;ConnectivityManager<Driver,decltype(j)> m(d,j);
  WifiNetwork n=net("one");m.begin(&n,0);m.tick(0);d.up=true;m.tick(1);assert(m.connected());
  assert(m.reconnects==0);d.up=false;m.tick(100);assert(m.disconnects==1&&!m.connected());
  m.tick(300);m.tick(1300);assert(d.tries.size()==2);d.up=true;m.tick(1301);assert(m.reconnects==1);
  auto attempts=m.attempts;for(unsigned i=0;i<10000;++i)m.tick(1302+i);assert(m.attempts==attempts);
}
void invalid(){
  Config c=config();c.count=9;assert(validate(c));c=config();c.networks[0].ssid[0]=0;assert(validate(c));
  c=config();memset(c.networks[0].ssid,1,33);assert(validate(c));c=config();strcpy(c.networks[0].password,"short");assert(validate(c));
  c=config();memset(c.networks[0].password,122,64);c.networks[0].password[64]=0;assert(validate(c));
  memset(c.networks[0].password,97,64);assert(!validate(c));c.networks[0].password[0]=0;assert(!validate(c));
  Store s;Journal<Store> j(s);Driver d;ConnectivityManager<Driver,decltype(j)> m(d,j);m.begin(nullptr,0);
  c.count=9;assert(m.apply(c,1));assert(!s.exists());
  c=config();assert(!m.apply(c,2));d.accept=false;m.tick(202);m.tick(203);
  assert(!strcmp(m.reason,"driver_error"));assert(m.attempts==1);
  m.tick(204);assert(m.attempts==1);
}
void persistence(){
  Store s;Journal<Store> j(s);Config a=config(),out;assert(j.save(a));
  a.networks[0].priority=99;assert(j.save(a));Journal<Store> reboot(s);assert(reboot.load(out));assert(out.networks[0].priority==99);
  for(unsigned failure=0;failure<3;++failure){s.failWrite=failure==0;s.failCommit=failure==1;s.corruptRead=failure==2;
    Config b=a;b.networks[0].priority=100;assert(!reboot.save(b));s.failWrite=s.failCommit=s.corruptRead=false;
    Journal<Store> retry(s);assert(retry.load(out));assert(out.networks[0].priority==99);}
  Store fresh;Journal<Store> freshJournal(fresh);assert(freshJournal.save(config()));assert(freshJournal.save(a));s=fresh;
  s.slots[s.selector&1][20]^=1;Journal<Store> fallback(s);assert(fallback.load(out));assert(out.networks[0].priority==1);
}
void migration(){
  Store s;Driver d;Journal<Store> j(s);ConnectivityManager<Driver,decltype(j)> m(d,j);WifiNetwork n=net("old");
  s.failCommit=true;m.begin(&n,0);assert(!strcmp(m.source,"legacy_volatile"));m.tick(0);assert(d.tries.back()=="old");
  s.failCommit=false;Journal<Store> jr(s);ConnectivityManager<Driver,decltype(jr)> r(d,jr);r.begin(&n,10);
  assert(!strcmp(r.source,"recovery_legacy_volatile"));assert(r.get().count==1);
  Config c;c.count=1;c.networks[0]=net("new");assert(!r.apply(c,20));
  Journal<Store> jj(s);ConnectivityManager<Driver,decltype(jj)> rr(d,jj);rr.begin(&n,30);assert(!strcmp(rr.get().networks[0].ssid,"new"));
  Config empty;assert(!rr.apply(empty,40));Journal<Store> je(s);ConnectivityManager<Driver,decltype(je)> e(d,je);e.begin(&n,50);assert(e.get().count==0);
}
int main(int argc,char **argv){assert(argc==2);std::string s=argv[1];
  if(s=="priority")priority();else if(s=="timing")timing();else if(s=="reconnect")reconnect();else if(s=="invalid")invalid();else if(s=="persistence")persistence();else if(s=="migration")migration();else return 2;
  puts("PASS");
}
