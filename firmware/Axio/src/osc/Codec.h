#pragma once
#include "Config.h"
namespace axio { namespace osc {
class Packet {
 public:
  uint8_t data[MaxPacket]={}; size_t size=0;
  bool bytes(const void *p,size_t n) {if(n>MaxPacket-size)return false;memcpy(data+size,p,n);size+=n;return true;}
  bool integer(uint32_t v) {uint8_t b[4]={uint8_t(v>>24),uint8_t(v>>16),uint8_t(v>>8),uint8_t(v)};return bytes(b,4);}
  bool string(const char *s) {size_t n=strlen(s)+1;const uint8_t zero[4]={};return bytes(s,n)&&bytes(zero,padded(n)-n);}
  bool message(const Message &m,const Config &c,const Sample &sample) {
    char address[AddressSize],tags[MaxArgs+2];
    if(!expand(m.address,c.id,address,sizeof(address)))return false;
    tags[0]=',';for(unsigned j=0;j<m.argCount;++j)tags[j+1]='f';tags[m.argCount+1]=0;
    if(!string(address)||!string(tags))return false;
    for(unsigned j=0;j<m.argCount;++j) {float v=sample.*(fields()[m.args[j]].member);uint32_t bits;memcpy(&bits,&v,4);if(!integer(bits))return false;}
    return true;
  }
  bool bundle() {return bytes("#bundle\0",8)&&integer(0)&&integer(1);}
};
inline bool calibrationPacket(const uint8_t *p,size_t n,const char *address) {
  size_t len=strlen(address)+1, offset=padded(len);
  if(n<offset+4 || memcmp(p,address,len))return false;
  for(size_t i=len;i<offset;++i)if(p[i])return false;
  if(p[offset]!=',' || p[offset+2] || p[offset+3])return false;
  if(!p[offset+1])return n==offset+4;
  // Pure Data's oscformat encodes a numeric trigger as ,f by default.
  // The argument triggers the command; it is never an orientation reference.
  if(n!=offset+8)return false;
  if(p[offset+1]=='i')return true;
  if(p[offset+1]!='f')return false;
  uint32_t bits=uint32_t(p[offset+4])<<24|uint32_t(p[offset+5])<<16|
                uint32_t(p[offset+6])<<8|uint32_t(p[offset+7]);
  return (bits&0x7f800000u)!=0x7f800000u; // Reject NaN/infinity.
}
}}
