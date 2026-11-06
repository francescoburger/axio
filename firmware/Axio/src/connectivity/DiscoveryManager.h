#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>
namespace axio { namespace connectivity {
constexpr unsigned MaxPeers=16;
inline bool hardwareId(const char *s){if(!s||strlen(s)!=12)return false;for(unsigned i=0;i<12;++i)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;return true;}
inline void identity(const uint8_t mac[6],char hw[13],char host[64]){
  snprintf(hw,13,"%02x%02x%02x%02x%02x%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  snprintf(host,64,"axio-%s",hw);
}
inline bool hostLabel(const char *s,const char *hw){
  if(!s||!hardwareId(hw))return false;size_t n=strnlen(s,64);if(n<17||n>63||strncmp(s,"axio-",5)||strncmp(s+5,hw,12))return false;
  if(n==17)return true;if(s[17]!='-'||n==18)return false;
  for(size_t i=18;i<n;++i)if(s[i]<'0'||s[i]>'9')return false;return true;
}
struct Peer {char hw[13]={},host[64]={};uint8_t ip[4]={};uint16_t port=0;uint32_t seen=0,lifetime=0;};
class PeerCache {
  Peer peers[MaxPeers];
 public:
  uint64_t rejected=0,overflow=0;
  void clear(){for(auto &p:peers)p=Peer();}
  void expire(uint32_t now){for(auto &p:peers)if(p.port&&uint32_t(now-p.seen)>=p.lifetime)p=Peer();}
  const Peer &get(unsigned i)const{return peers[i];}
  unsigned count()const{unsigned n=0;for(const auto &p:peers)if(p.port)++n;return n;}
  bool observe(const char *hw,const char *host,const uint8_t ip[4],uint16_t port,uint32_t ttl,uint32_t now,const char *self){
    if(!hardwareId(hw)||!hostLabel(host,hw)||!port||!ip[0]||ip[0]==127||ip[0]>=224||ip[3]==255){++rejected;return false;}
    if(!strcmp(hw,self))return false;
    unsigned index=MaxPeers,empty=MaxPeers;
    for(unsigned i=0;i<MaxPeers;++i){if(peers[i].port&&!strcmp(peers[i].hw,hw)){index=i;break;}if(!peers[i].port&&empty==MaxPeers)empty=i;}
    if(!ttl){if(index<MaxPeers)peers[index]=Peer();return true;}
    if(index==MaxPeers)index=empty;
    if(index==MaxPeers){++overflow;return false;}
    auto &p=peers[index];strcpy(p.hw,hw);strcpy(p.host,host);memcpy(p.ip,ip,4);p.port=port;p.seen=now;p.lifetime=(ttl>90?90:ttl)*1000;return true;
  }
};
// Driver owns the SDK query. No callbacks mutate this policy or cache.
template<class Driver> class DiscoveryManager {
  Driver &driver;uint32_t ip=0,lastStart=0,lastQuery=0;uint16_t port=0;
  bool tried=false;uint32_t queryDelay=2000;
  void drain(const char *why){driver.disable();active=false;draining=true;cache.clear();reason=why;}
 public:
  static constexpr uint32_t RetryMs=5000,QueryMs=2000;
  PeerCache cache;bool running=false,active=false,draining=false;
  uint64_t starts=0,failures=0,queries=0,completed=0,queryFailures=0;
  uint32_t jitter=0;const char *reason="offline";
  explicit DiscoveryManager(Driver &d):driver(d){}
  void tick(bool eligible,uint32_t address,uint16_t listenPort,uint32_t now){
    if(running&&active&&(!eligible||address!=ip))drain(eligible?"ip_changed":"sta_unavailable");
    if(draining){
      driver.poll(cache,false,now);
      if(driver.pending())return;
      driver.stop();running=draining=false;tried=false;return;
    }
    if(!eligible)return;
    cache.expire(now);
    if(!running){
      if(tried&&uint32_t(now-lastStart)<RetryMs)return;
      lastStart=now;tried=true;
      if(!driver.start(listenPort)){++failures;reason="mdns_start_failed";return;}
      running=active=true;ip=address;port=listenPort;++starts;reason="sta_ready";lastQuery=now;queryDelay=2000+jitter%3000;return;
    }
    if(port!=listenPort){
      if(!driver.port(listenPort)){++failures;drain("service_update_failed");return;}
      port=listenPort;
    }
    if(driver.pending()){
      if(driver.poll(cache,true,now)){++completed;reason="query_complete";}
      return;
    }
    if(uint32_t(now-lastQuery)>=queryDelay){
      lastQuery=now;queryDelay=30000+jitter%5000;
      if(driver.query()){++queries;reason="query_pending";}
      else {++queryFailures;reason="query_start_failed";}
    }
  }
};
}}
