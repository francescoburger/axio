#include <assert.h>
#include <string>
#include <vector>
#include "src/connectivity/FallbackAp.h"
using namespace axio::connectivity;
struct Store {
  std::vector<uint8_t> bytes;bool fail=false,badRead=false,availableFlag=true;
  bool available(){return availableFlag;}bool exists(){return !bytes.empty();}
  bool write(const uint8_t *p,size_t n){if(fail)return false;bytes.assign(p,p+n);return true;}
  size_t read(uint8_t *p,size_t n){if(bytes.size()>n)return 0;memcpy(p,bytes.data(),bytes.size());if(badRead&&bytes.size())p[0]^=1;return bytes.size();}
};
struct Driver {
  bool enabledFlag=false,startedFlag=false,failStart=false,failStop=false;
  unsigned starts=0,stops=0,polls=0,closes=0;
  bool enabled(){return enabledFlag;}bool started(){return startedFlag;}
  bool start(){++starts;if(failStart)return false;enabledFlag=true;return true;}
  bool stop(){++stops;if(failStop)return false;enabledFlag=startedFlag=false;return true;}
  void closePortal(){++closes;}void pollPortal(uint32_t){++polls;}
};
void credentials(){
  Store s;ApCredentials<Store> c(s);c.begin();assert(c.ready&&!strcmp(c.password,"12345678"));
  assert(s.bytes.size()==40&&!memcmp(s.bytes.data(),"AXP2",4));
  ApCredentials<Store> reboot(s);reboot.begin();assert(reboot.ready&&!strcmp(reboot.password,"12345678"));
  assert(reboot.replace());assert(!strcmp(reboot.password,"12345678"));
  // Valid old random record is migrated, without touching any other namespace.
  memcpy(s.bytes.data(),"AXP1",4);memset(s.bytes.data()+4,97,32);
  uint32_t checksum=crc(s.bytes.data(),36);for(unsigned i=0;i<4;++i)s.bytes[36+i]=uint8_t(checksum>>(8*i));
  ApCredentials<Store> migrated(s);migrated.begin();assert(migrated.ready&&!strcmp(migrated.password,"12345678"));assert(!memcmp(s.bytes.data(),"AXP2",4));
  s.bytes[10]^=1;ApCredentials<Store> broken(s);broken.begin();assert(!broken.ready);
  assert(!strcmp(broken.error,"ap_secret_invalid"));
}
void failures(){
  Store s;s.fail=true;ApCredentials<Store> c(s);c.begin();assert(!c.ready&&!c.password[0]);
  s.fail=false;s.badRead=true;assert(!c.replace()&&!c.ready);
  s.badRead=false;assert(c.replace()&&c.ready);
  Store absent;absent.availableFlag=false;ApCredentials<Store> a(absent);a.begin();assert(!a.ready&&absent.bytes.empty());
}
void lifecycle(){
  Driver d;FallbackAp<Driver> ap(d);ap.provisioned=true;uint32_t boot=0xfffffff0;
  ap.tick(false,14999,boot+14999);assert(!d.starts);
  ap.tick(false,15000,boot+15000);assert(d.starts==1&&ap.lastFallbackMs==15000);
  d.startedFlag=true;ap.tick(false,15001,boot+15001);assert(d.polls==1);
  ap.tick(true,0,boot+15002);assert(!d.enabled()&&d.stops==1&&d.closes>0);
  ap.tick(false,14999,boot+30001);assert(d.starts==1);
  ap.tick(false,15000,boot+30002);assert(d.starts==2);
  d.startedFlag=true;d.failStop=true;ap.tick(true,0,boot+30003);unsigned stops=d.stops;
  ap.tick(true,0,boot+30004);assert(d.stops==stops);
  d.failStop=false;ap.tick(true,0,boot+31003);assert(!d.enabled());
}
void retry(){
  Driver d;FallbackAp<Driver> ap(d);ap.tick(false,15000,0);assert(!d.starts);
  ap.provisioned=true;d.failStart=true;ap.tick(false,15000,0);assert(d.starts==1&&ap.failures==1);
  ap.tick(false,24999,9999);assert(d.starts==1);d.failStart=false;ap.tick(false,25000,10000);assert(d.starts==2);
  ap.tick(false,27999,12999);assert(d.stops==1);ap.tick(false,28000,13000);assert(d.stops==2);
  assert(ap.failures==2);ap.tick(false,34999,19999);assert(d.starts==2);ap.tick(false,35000,20000);assert(d.starts==3);
}
int main(int argc,char **argv){assert(argc==2);std::string s=argv[1];if(s=="credentials")credentials();else if(s=="failures")failures();else if(s=="lifecycle")lifecycle();else if(s=="retry")retry();else return 2;puts("PASS");}
