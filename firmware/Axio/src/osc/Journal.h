#pragma once
#include "../core/Sample.h"
#include "Config.h"
namespace axio { namespace osc {
inline uint32_t crc32(const uint8_t *p,size_t n) {
  uint32_t c=~0u;while(n--){c^=*p++;for(int i=0;i<8;++i)c=(c>>1)^(0xedb88320u & (0u-(c&1)));}return ~c;
}
// Store: read/write(slot, bytes, size), committed()/commit(slot).
// Each slot is an atomic NVS blob. Separate atomic selector is the commit point.
template<class Store> class Journal {
  Store &store;
  uint8_t buffer[MaxConfig+16]={};
  char json[MaxConfig+1]={};
  int active=-1;
  uint32_t generation=0;
  static void put(uint8_t *b,uint32_t v){for(unsigned i=0;i<4;++i)b[i]=uint8_t(v>>(8*i));}
  static uint32_t get(const uint8_t *b){return uint32_t(b[0])|(uint32_t(b[1])<<8)|(uint32_t(b[2])<<16)|(uint32_t(b[3])<<24);}
  bool read(int slot,Config &out) {
    size_t n=store.read(slot,buffer,sizeof(buffer));
    if(n<17||get(buffer)!=0x314f5841||get(buffer+8)!=n-16||get(buffer+12)!=crc32(buffer+16,n-16))return false;
    size_t len=n-16;memcpy(json,buffer+16,len);json[len]=0;
    if(memchr(json,0,len)||parse(json,out))return false;
    return true;
  }
 public:
  explicit Journal(Store &s):store(s) {}
  bool load(Config &out) {
    Config candidate;
    uint64_t committed=store.committed();
    if(committed==~uint64_t(0) || (committed>>1)>0xffffffffu)return false;
    int selected=committed&1;generation=committed>>1;
    if(read(selected,candidate) && get(buffer+4)==generation) {active=selected;out=candidate;return true;}
    if(read(1-selected,candidate) && get(buffer+4)<generation) {active=1-selected;out=candidate;return true;}
    return false;
  }
  bool save(const Config &c) {
    if(validate(c)||!serialize(c,json,sizeof(json)))return false;
    size_t len=strlen(json);
    int next=active==0?1:0;
    if(generation==0xffffffffu)return false;
    put(buffer,0x314f5841);put(buffer+4,generation+1);put(buffer+8,len);
    memcpy(buffer+16,json,len);put(buffer+12,crc32(buffer+16,len));
    if(!store.write(next,buffer,len+16))return false;
    // Read back and verify canonical bytes as well as CRC and schema.
    size_t n=store.read(next,buffer,sizeof(buffer));
    if(n!=len+16||get(buffer)!=0x314f5841||get(buffer+4)!=generation+1||get(buffer+8)!=len||get(buffer+12)!=crc32(buffer+16,len)||memcmp(buffer+16,json,len))return false;
    if(!store.commit((uint64_t(generation+1)<<1)|unsigned(next)))return false;
    active=next;++generation;return true;
  }
};
}}
