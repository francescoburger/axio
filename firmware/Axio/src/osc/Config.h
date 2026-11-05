#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../core/Sample.h"
#include "../diagnostics/SerialProtocol.h"
namespace axio { namespace osc {
// Provisional resource bounds, not product limits. Revisit with hardware benchmarks.
constexpr unsigned MaxDest=8, MaxMessages=16, MaxArgs=16, AddressSize=80;
constexpr size_t MaxConfig=7168, MaxPacket=1400;
struct Destination { uint8_t ip[4]={127,0,0,1}; uint16_t port=16888; };
struct Field { const char *name; float Sample::*member; };
inline const Field *fields() {
  static const Field table[]={
    {"q0",&Sample::q0},{"q1",&Sample::q1},{"q2",&Sample::q2},{"q3",&Sample::q3},
    {"yaw",&Sample::yaw},{"pitch",&Sample::pitch},{"roll",&Sample::roll},
    {"axReal",&Sample::axReal},{"ayReal",&Sample::ayReal},{"azReal",&Sample::azReal},
    {"axWorld",&Sample::axWorld},{"ayWorld",&Sample::ayWorld},{"azWorld",&Sample::azWorld},
    {"gx",&Sample::gx},{"gy",&Sample::gy},{"gz",&Sample::gz}};
  return table;
}
struct Message {
  char address[AddressSize]="/orientation/{id}";
  uint8_t args[MaxArgs]={4,5,6};
  uint8_t argCount=3, routes=1;
  uint16_t hz=100;
  bool bundle=true;
};
struct Config {
  char id[17]="0", preset[12]="Orientation", tare[AddressSize]="/tare";
  uint16_t listenPort=16889;
  Destination destinations[MaxDest];
  Message messages[MaxMessages];
  uint8_t destCount=1, messageCount=1, ackDestination=0;
  bool ackEnabled=false;
  char ackAddress[AddressSize]="/tared/{id}";
};
inline bool expand(const char *source,const char *id,char *out,size_t cap) {
  size_t n=0;
  for (size_t i=0; source[i];) {
    if (!strncmp(source+i,"{id}",4)) {
      size_t k=strlen(id); if(n+k>=cap) return false;
      memcpy(out+n,id,k); n+=k; i+=4;
    } else {
      unsigned char c=source[i++];
      if(c<=32 || c>=127 || strchr("#*,?[]{}\\\"",c) || n+1>=cap) return false;
      out[n++]=c;
    }
  }
  out[n]=0;
  return n>1 && out[0]=='/' && out[n-1]!='/' && !strstr(out,"//");
}
inline size_t padded(size_t n) { return (n+3)&~size_t(3); }
inline size_t messageSize(const Message &m,const char *id) {
  char address[AddressSize];
  if(!expand(m.address,id,address,sizeof(address))) return MaxPacket+1;
  return padded(strlen(address)+1)+padded(m.argCount+2)+4*m.argCount;
}
inline const char *validate(const Config &c) {
  if(!c.destCount || c.destCount>MaxDest || !c.messageCount || c.messageCount>MaxMessages) return "resource_limit";
  if(!c.listenPort || !*c.id || strlen(c.id)>16) return "invalid_config";
  for(const char *p=c.id;*p;++p) if(!((*p>='0'&&*p<='9')||(*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||*p=='_'||*p=='-')) return "invalid_id";
  if(strcmp(c.preset,"Orientation") && strcmp(c.preset,"Motion") && strcmp(c.preset,"Custom")) return "invalid_preset";
  char address[AddressSize],other[AddressSize],tare[AddressSize],ack[AddressSize];
  if(!expand(c.tare,c.id,tare,sizeof(tare)) || !expand(c.ackAddress,c.id,ack,sizeof(ack))) return "invalid_address";
  if(c.ackDestination>=c.destCount || !strcmp(tare,ack)) return "address_conflict";
  for(unsigned i=0;i<c.destCount;++i) {
    const auto &d=c.destinations[i];
    if(!d.port || !d.ip[0] || d.ip[0]>=224 || d.ip[3]==255) return "invalid_destination";
    for(unsigned j=0;j<i;++j) if(!memcmp(d.ip,c.destinations[j].ip,4) && d.port==c.destinations[j].port) return "duplicate_destination";
  }
  for(unsigned i=0;i<c.messageCount;++i) {
    const auto &m=c.messages[i];
    if(!m.hz || m.hz>100) return "invalid_frequency";
    if(!m.argCount || m.argCount>MaxArgs || !m.routes || (unsigned(m.routes)>>c.destCount)) return "invalid_arguments_or_routes";
    if(!expand(m.address,c.id,address,sizeof(address))) return "invalid_address";
    if(!strcmp(address,tare)||!strcmp(address,ack)) return "address_conflict";
    if(messageSize(m,c.id)>MaxPacket) return "packet_too_large";
    for(unsigned k=0;k<m.argCount;++k) if(m.args[k]>=16) return "unknown_field";
    for(unsigned j=0;j<i;++j) {
      expand(c.messages[j].address,c.id,other,sizeof(other));
      if(!strcmp(address,other)) return "duplicate_address";
    }
  }
  // Worst case: all bundled messages due at once, per destination.
  for(unsigned r=0;r<c.destCount;++r) {
    size_t n=16;
    for(unsigned i=0;i<c.messageCount;++i) if(c.messages[i].bundle && (c.messages[i].routes&(1u<<r))) n+=4+messageSize(c.messages[i],c.id);
    if(n>MaxPacket) return "packet_too_large";
  }
  return nullptr;
}
inline Config preset(const char *name) {
  Config c;
  if(!strcmp(name,"Motion")) {
    strcpy(c.preset,"Motion"); c.messageCount=3;
    const char *addresses[]={"/motion/linear/{id}","/motion/world/{id}","/motion/gyro/{id}"};
    for(unsigned i=0;i<3;++i) { strcpy(c.messages[i].address,addresses[i]); for(unsigned k=0;k<3;++k) c.messages[i].args[k]=7+3*i+k; }
  } else if(!strcmp(name,"Custom")) {
    strcpy(c.preset,"Custom");strcpy(c.messages[0].address,"/custom/{id}");c.messages[0].argCount=1;
  }
  else {
    c.messageCount=2; strcpy(c.messages[1].address,"/quaternion/{id}");
    c.messages[1].argCount=4; for(unsigned i=0;i<4;++i)c.messages[1].args[i]=i;
  }
  return c;
}
// Schema-specific streaming reader: bounded destination arrays, no JSON DOM/heap.
class Reader {
  const char *p;
 public:
  explicit Reader(const char *s):p(s) {}
  void ws() { while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n')++p; }
  bool take(char c) { ws(); if(*p!=c)return false; ++p;return true; }
  bool text(char *out,size_t cap) {
    if(!take('"'))return false;
    size_t n=0;
    while(*p && *p!='"') { if(*p=='\\' || n+1>=cap)return false; out[n++]=*p++; }
    if(*p!='"')return false;
    ++p;out[n]=0;return true;
  }
  bool number(unsigned &v) {
    ws(); if(*p<'0'||*p>'9')return false;
    v=0; do { if(v>65535)return false; v=v*10+unsigned(*p++-'0'); }while(*p>='0'&&*p<='9');
    return v<=65535;
  }
  bool boolean(bool &v) {
    ws(); if(!strncmp(p,"true",4)){v=true;p+=4;return true;}
    if(!strncmp(p,"false",5)){v=false;p+=5;return true;}return false;
  }
  bool end() { ws();return !*p; }
  template<class Fn> bool object(Fn fn) {
    if(!take('{'))return false;
    unsigned seen=0;
    if(take('}'))return false;
    do {
      char key[32]; if(!text(key,sizeof(key))||!take(':'))return false;
      unsigned bit=fn(key); if(!bit || (seen&bit))return false; seen|=bit;
      if(take('}'))return true;
    }while(take(','));
    return false;
  }
};
inline const char *parse(const char *json,Config &c) {
  if(strlen(json)>MaxConfig)return "config_too_large";
  if(!JsonSyntax(json).object())return "invalid_json";
  c=Config(); Reader r(json); unsigned top=0;
  bool ok=r.object([&](const char *key)->unsigned {
    unsigned n=0,bit=0;
    if(!strcmp(key,"schema")) {if(!r.number(n)||n!=1)return 0;bit=1;}
    else if(!strcmp(key,"preset")){if(!r.text(c.preset,sizeof(c.preset)))return 0;bit=2;}
    else if(!strcmp(key,"id")){if(!r.text(c.id,sizeof(c.id)))return 0;bit=4;}
    else if(!strcmp(key,"listen_port")){if(!r.number(n))return 0;c.listenPort=n;bit=8;}
    else if(!strcmp(key,"tare_address")){if(!r.text(c.tare,sizeof(c.tare)))return 0;bit=16;}
    else if(!strcmp(key,"ack")) {
      unsigned mask=0;
      if(!r.object([&](const char *k)->unsigned {
        unsigned b=0,v;
        if(!strcmp(k,"enabled")){if(!r.boolean(c.ackEnabled))return 0;b=1;}
        else if(!strcmp(k,"address")){if(!r.text(c.ackAddress,sizeof(c.ackAddress)))return 0;b=2;}
        else if(!strcmp(k,"destination")){if(!r.number(v)||v>=MaxDest)return 0;c.ackDestination=v;b=4;}
        mask|=b;return b;
      })||mask!=7)return 0;
      bit=32;
    } else if(!strcmp(key,"destinations")) {
      if(!r.take('['))return 0;c.destCount=0;
      do {
        if(c.destCount>=MaxDest)return 0;auto &d=c.destinations[c.destCount++]; unsigned mask=0;
        if(!r.object([&](const char *k)->unsigned {
          unsigned b=0,v;
          if(!strcmp(k,"ip")) {
            char ip[20],tail; unsigned a[4]; if(!r.text(ip,sizeof(ip)))return 0;
            for(const char *p=ip;*p;++p)if((*p<'0'||*p>'9')&&*p!='.')return 0;
            if(sscanf(ip,"%u.%u.%u.%u%c",a,a+1,a+2,a+3,&tail)!=4)return 0;
            for(unsigned j=0;j<4;++j){if(a[j]>255)return 0;d.ip[j]=a[j];}b=1;
          } else if(!strcmp(k,"port")){if(!r.number(v))return 0;d.port=v;b=2;}
          mask|=b;return b;
        })||mask!=3)return 0;
      }while(r.take(','));
      if(!r.take(']'))return 0;bit=64;
    } else if(!strcmp(key,"messages")) {
      if(!r.take('['))return 0;c.messageCount=0;
      do {
        if(c.messageCount>=MaxMessages)return 0;auto &m=c.messages[c.messageCount++];unsigned mask=0;
        if(!r.object([&](const char *k)->unsigned {
          unsigned b=0,v;
          if(!strcmp(k,"address")){if(!r.text(m.address,sizeof(m.address)))return 0;b=1;}
          else if(!strcmp(k,"hz")){if(!r.number(v))return 0;m.hz=v;b=2;}
          else if(!strcmp(k,"bundle")){if(!r.boolean(m.bundle))return 0;b=4;}
          else if(!strcmp(k,"fields")) {
            if(!r.take('['))return 0;m.argCount=0;
            do {
              char field[20];if(m.argCount>=MaxArgs||!r.text(field,sizeof(field)))return 0;
              unsigned j=0;for(;j<16;++j)if(!strcmp(field,fields()[j].name))break;
              if(j==16)return 0;m.args[m.argCount++]=j;
            }while(r.take(','));
            if(!r.take(']'))return 0;b=8;
          } else if(!strcmp(k,"destinations")) {
            if(!r.take('['))return 0;m.routes=0;
            do {if(!r.number(v)||v>=MaxDest||(m.routes&(1u<<v)))return 0;m.routes|=1u<<v;}while(r.take(','));
            if(!r.take(']'))return 0;b=16;
          }
          mask|=b;return b;
        })||mask!=31)return 0;
      }while(r.take(','));
      if(!r.take(']'))return 0;bit=128;
    }
    top|=bit;return bit;
  });
  if(!ok||!r.end()||top!=255)return "invalid_config";
  return validate(c);
}
// JSON strings here have already passed the restricted ASCII schema validator.
inline bool serialize(const Config &c,char *out,size_t cap) {
  size_t used=0;
  auto append=[&](const char *s)->bool {size_t n=strlen(s);if(used+n>=cap)return false;memcpy(out+used,s,n+1);used+=n;return true;};
  char b[512];
  snprintf(b,sizeof(b),"{\"schema\":1,\"preset\":\"%s\",\"id\":\"%s\",\"listen_port\":%u,\"tare_address\":\"%s\",\"ack\":{\"enabled\":%s,\"address\":\"%s\",\"destination\":%u},\"destinations\":[",c.preset,c.id,c.listenPort,c.tare,c.ackEnabled?"true":"false",c.ackAddress,c.ackDestination);
  if(!append(b))return false;
  for(unsigned i=0;i<c.destCount;++i){auto &d=c.destinations[i];snprintf(b,sizeof(b),"%s{\"ip\":\"%u.%u.%u.%u\",\"port\":%u}",i?",":"",d.ip[0],d.ip[1],d.ip[2],d.ip[3],d.port);if(!append(b))return false;}
  if(!append("],\"messages\":["))return false;
  for(unsigned i=0;i<c.messageCount;++i){auto &m=c.messages[i];snprintf(b,sizeof(b),"%s{\"address\":\"%s\",\"hz\":%u,\"bundle\":%s,\"fields\":[",i?",":"",m.address,m.hz,m.bundle?"true":"false");if(!append(b))return false;
    for(unsigned j=0;j<m.argCount;++j){snprintf(b,sizeof(b),"%s\"%s\"",j?",":"",fields()[m.args[j]].name);if(!append(b))return false;}
    if(!append("],\"destinations\":["))return false;bool first=true;
    for(unsigned j=0;j<c.destCount;++j)if(m.routes&(1u<<j)){snprintf(b,sizeof(b),"%s%u",first?"":",",j);first=false;if(!append(b))return false;}
    if(!append("]}"))return false;
  }
  return append("]}");
}
}}
