#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace axio { namespace connectivity {
constexpr unsigned MaxNetworks=8;
struct WifiNetwork { char ssid[33]={}; char password[65]={}; uint8_t priority=0; };
struct Config { uint8_t count=0; WifiNetwork networks[MaxNetworks]; };
inline const char *validate(const Config &c) {
  if(c.count>MaxNetworks)return "too_many_networks";
  for(unsigned i=0;i<c.count;++i) {
    const auto &n=c.networks[i];
    size_t s=strnlen(n.ssid,sizeof(n.ssid)),p=strnlen(n.password,sizeof(n.password));
    if(!s||s>32)return "invalid_ssid";
    if(p>64||(p&&p<8))return "invalid_password";
    for(size_t k=0;k<p;++k) {
      unsigned char ch=n.password[k];
      if(ch<32||ch>126||(p==64&&!((ch>='0'&&ch<='9')||(ch>='a'&&ch<='f')||(ch>='A'&&ch<='F'))))return "invalid_password";
    }
    for(unsigned k=0;k<i;++k)if(!strcmp(n.ssid,c.networks[k].ssid))return "duplicate_ssid";
  }
  return nullptr;
}
inline uint32_t crc(const uint8_t *p,size_t n) {
  uint32_t c=~0u;
  while(n--){c^=*p++;for(unsigned i=0;i<8;++i)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}
  return ~c;
}
// Independent namespace and format; never writes the OSC journal or SDK credentials.
template<class Store> class Journal {
  Store &store;
  static constexpr size_t Size=16+1+MaxNetworks*99;
  uint8_t bytes[Size]={},expected[Size]={};
  int active=-1; uint32_t generation=0;
  static void put(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=uint8_t(v>>(8*i));}
  static uint32_t get(const uint8_t *p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
  bool read(int slot,Config &c,uint32_t &g) {
    if(store.read(slot,bytes,Size)!=Size||get(bytes)!=0x31575841||get(bytes+8)!=Size-16||get(bytes+12)!=crc(bytes+16,Size-16))return false;
    c=Config();c.count=bytes[16];
    for(unsigned i=0;i<MaxNetworks;++i){auto &n=c.networks[i];const uint8_t *p=bytes+17+i*99;memcpy(n.ssid,p,33);memcpy(n.password,p+33,65);n.priority=p[98];}
    g=get(bytes+4);return !validate(c);
  }
 public:
  bool present=false;
  explicit Journal(Store &s):store(s){}
  bool load(Config &out) {
    uint64_t selector=store.committed();
    present=selector!=~uint64_t(0)||store.exists();
    if(selector==~uint64_t(0)||(selector>>1)>0xffffffffu)return false;
    uint32_t g=0;Config c;int slot=selector&1;generation=selector>>1;
    if(read(slot,c,g)&&g==generation){out=c;active=slot;return true;}
    if(read(1-slot,c,g)&&g<generation){out=c;active=1-slot;return true;}
    return false;
  }
  bool save(const Config &c) {
    if(validate(c)||generation==0xffffffffu)return false;
    memset(expected,0,Size);put(expected,0x31575841);put(expected+4,generation+1);put(expected+8,Size-16);expected[16]=c.count;
    for(unsigned i=0;i<c.count;++i){const auto &n=c.networks[i];uint8_t *p=expected+17+i*99;memcpy(p,n.ssid,strlen(n.ssid));memcpy(p+33,n.password,strlen(n.password));p[98]=n.priority;}
    put(expected+12,crc(expected+16,Size-16));int next=active==0?1:0;
    if(!store.write(next,expected,Size)||store.read(next,bytes,Size)!=Size||memcmp(bytes,expected,Size)||!store.commit(uint64_t(generation+1)*2+next))return false;
    active=next;++generation;present=true;return true;
  }
};
enum class State { Empty, Connecting, Disconnecting, Backoff, Connected };
inline const char *name(State s){const char *names[]={"empty","connecting","disconnecting","backoff","connected"};return names[unsigned(s)];}
// Driver operations initiate work only. No scans, polling waits or catch-up loops.
template<class Driver,class Persistence> class ConnectivityManager {
  Driver &driver;Persistence &journal;Config config;
  unsigned order[MaxNetworks]={},cursor=0;
  uint32_t since=0,offlineSince=0,backoff=1000,wait=0;
  bool everConnected=false,online=false;
  void sort(){for(unsigned i=0;i<config.count;++i){order[i]=i;for(unsigned k=i;k>0&&config.networks[order[k]].priority>config.networks[order[k-1]].priority;--k){unsigned t=order[k];order[k]=order[k-1];order[k-1]=t;}}}
  void transition(State s,const char *why,uint32_t now){state=s;reason=why;since=now;++transitions;}
  void stop(const char *why,uint32_t now){driver.disconnect();transition(State::Disconnecting,why,now);}
 public:
  State state=State::Empty;const char *reason="boot",*source="empty",*storageError=nullptr;
  uint64_t attempts=0,disconnects=0,reconnects=0,transitions=0;
  static constexpr uint32_t AttemptMs=10000,SettleMs=200,MaxBackoffMs=30000;
  ConnectivityManager(Driver &d,Persistence &p):driver(d),journal(p){}
  const Config &get()const{return config;}
  bool connected()const{return online&&driver.connected();}
  uint32_t offlineMs(uint32_t now)const{return connected()?0:uint32_t(now-offlineSince);}
  void begin(const WifiNetwork *legacy,uint32_t now) {
    offlineSince=now;
    if(journal.load(config))source="saved";
    else if(journal.present){
      storageError="journal_invalid";source="recovery_required";
      if(legacy&&legacy->ssid[0]){
        Config fallback;fallback.count=1;fallback.networks[0]=*legacy;
        if(!validate(fallback)){config=fallback;source="recovery_legacy_volatile";}
      }
    }
    else if(legacy&&legacy->ssid[0]) {
      config.count=1;config.networks[0]=*legacy;
      if(validate(config)){config=Config();storageError="legacy_invalid";source="legacy_invalid";}
      else if(journal.save(config))source="migrated";
      else {storageError="migration_failed";source="legacy_volatile";}
    }
    sort();transition(config.count?State::Backoff:State::Empty,"boot",now);wait=0;
  }
  const char *apply(const Config &c,uint32_t now) {
    if(const char *e=validate(c))return e;
    if(!journal.save(c)){storageError="persistence_failed";return storageError;}
    config=c;source="applied";storageError=nullptr;sort();cursor=0;backoff=1000;wait=0;
    if(online){online=false;++disconnects;offlineSince=now;}
    stop("configuration_changed",now);return nullptr;
  }
  void tick(uint32_t now) {
    bool up=driver.connected();
    // During an intentional disconnect, wait for acknowledgement before accepting a link.
    if(state==State::Disconnecting) {
      if(!up&&uint32_t(now-since)>=SettleMs)transition(config.count?State::Backoff:State::Empty,"disconnected",now);
      return;
    }
    if(up) {
      if(!online){online=true;if(everConnected)++reconnects;everConnected=true;backoff=1000;transition(State::Connected,"sta_connected",now);}
      return;
    }
    if(online){online=false;++disconnects;offlineSince=now;cursor=0;wait=1000;stop("sta_lost",now);return;}
    if(state==State::Connecting&&uint32_t(now-since)>=AttemptMs) {
      ++cursor;wait=0;
      if(cursor>=config.count){cursor=0;wait=backoff;backoff=backoff<MaxBackoffMs/2?backoff*2:MaxBackoffMs;}
      stop("attempt_timeout",now);return;
    }
    if(state==State::Backoff&&uint32_t(now-since)>=wait&&config.count) {
      ++attempts;bool started=driver.connect(config.networks[order[cursor]]);
      transition(State::Connecting,started?"attempt_started":"driver_error",now);
    }
  }
};
}}
