#pragma once
#include <map>
#include <string>
#include <vector>
#include <stdint.h>
#include <string.h>
extern std::map<std::string,std::vector<uint8_t>> fakeNvs;
extern bool failNvs;
struct Preferences {
  std::string ns;
  bool begin(const char *name,bool){ns=std::string(name)+"/";return true;}
  bool isKey(const char *key){return fakeNvs.count(ns+key)!=0;}
  size_t getBytesLength(const char *key){return fakeNvs[ns+key].size();}
  size_t getBytes(const char *key,void *out,size_t cap){auto &v=fakeNvs[ns+key];if(v.size()>cap)return 0;memcpy(out,v.data(),v.size());return v.size();}
  size_t putBytes(const char *key,const void *data,size_t size){if(failNvs)return 0;const uint8_t *p=static_cast<const uint8_t *>(data);fakeNvs[ns+key].assign(p,p+size);return size;}
  uint64_t getULong64(const char *key,uint64_t fallback){uint64_t v;return getBytes(key,&v,8)==8?v:fallback;}
  size_t putULong64(const char *key,uint64_t v){return putBytes(key,&v,8);}
};
