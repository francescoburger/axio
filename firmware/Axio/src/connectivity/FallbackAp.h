#pragma once
#include "ConnectivityManager.h"
namespace axio { namespace connectivity {
// User-selected fixed WPA2 password. AXP1 random secrets migrate to AXP2.
constexpr char ApPassword[]="12345678";
template<class Storage> class ApCredentials {
  Storage &store;
  uint8_t bytes[40]={};
  bool valid(bool legacy=false)const {
    if(memcmp(bytes,legacy?"AXP1":"AXP2",4))return false;
    uint32_t sum=crc(bytes,36);
    for(unsigned i=0;i<4;++i)if(bytes[36+i]!=uint8_t(sum>>(8*i)))return false;
    if(legacy){
      for(unsigned i=4;i<36;++i)if(!((bytes[i]>='0'&&bytes[i]<='9')||(bytes[i]>='a'&&bytes[i]<='f')))return false;
    }else {
      if(memcmp(bytes+4,ApPassword,sizeof(ApPassword)-1))return false;
      for(unsigned i=4+sizeof(ApPassword)-1;i<36;++i)if(bytes[i])return false;
    }
    return true;
  }
 public:
  char password[sizeof(ApPassword)]={};bool ready=false;const char *error=nullptr;
  explicit ApCredentials(Storage &s):store(s){}
  bool replace() {
    ready=false;memset(password,0,sizeof(password));memset(bytes,0,sizeof(bytes));
    memcpy(bytes,"AXP2",4);memcpy(bytes+4,ApPassword,sizeof(ApPassword)-1);
    uint32_t sum=crc(bytes,36);
    for(unsigned i=0;i<4;++i)bytes[36+i]=uint8_t(sum>>(8*i));
    uint8_t verify[40];
    if(!store.write(bytes,sizeof(bytes))||store.read(verify,sizeof(verify))!=sizeof(verify)||memcmp(bytes,verify,sizeof(bytes))){error="ap_secret_persistence_failed";return false;}
    memcpy(password,ApPassword,sizeof(password));ready=true;error=nullptr;return true;
  }
  void begin() {
    ready=false;memset(password,0,sizeof(password));
    if(!store.available()){error="ap_nvs_unavailable";return;}
    if(!store.exists()){replace();return;}
    if(store.read(bytes,sizeof(bytes))!=sizeof(bytes)){error="ap_secret_invalid";return;}
    if(valid(true)){replace();return;}
    if(!valid()){error="ap_secret_invalid";return;}
    memcpy(password,ApPassword,sizeof(password));ready=true;error=nullptr;
  }
};
// Driver: start(), started(), enabled(), stop(), closePortal(), pollPortal(now).
template<class Driver> class FallbackAp {
  Driver &driver;
  uint32_t attempted=0,stopping=0;bool tried=false,stopTried=false,pending=false;
 public:
  static constexpr uint32_t FallbackMs=15000,RetryMs=10000,StartMs=3000;
  bool provisioned=false;uint64_t starts=0,failures=0,stops=0;
  uint32_t lastFallbackMs=0;const char *reason="boot";
  explicit FallbackAp(Driver &d):driver(d){}
  void close(uint32_t now) {
    driver.closePortal();pending=false;
    if(driver.enabled()&&(!stopTried||uint32_t(now-stopping)>=1000)){
      stopping=now;stopTried=true;
      if(driver.stop()){++stops;reason="sta_connected";}else {++failures;reason="ap_stop_failed";}
    }
    if(!driver.enabled())stopTried=false;
  }
  void tick(bool sta,uint32_t offlineMs,uint32_t now) {
    if(sta||!provisioned){close(now);return;}
    if(driver.started()) {pending=false;driver.pollPortal(now);return;}
    if(pending){
      if(uint32_t(now-attempted)<StartMs)return;
      driver.closePortal();driver.stop();pending=false;++failures;reason="ap_start_timeout";return;
    }
    if(offlineMs<FallbackMs||(tried&&uint32_t(now-attempted)<RetryMs))return;
    attempted=now;tried=true;lastFallbackMs=offlineMs;
    if(driver.start()){pending=true;++starts;reason="fallback_timeout";}
    else {++failures;reason="ap_start_failed";driver.closePortal();driver.stop();}
  }
};
}}
