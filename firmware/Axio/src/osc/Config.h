#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <new>
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
  uint16_t hz=100, intervalMs=10;
  bool bundle=true;
};
struct Editor {
  char mainAddress[AddressSize]="/motion/{id}";
  char suffix[5][AddressSize]={{0},{0},{0},{0},{0}};
  uint8_t selected[5]={7,7,7,7,15};
  uint16_t intervalMs=20;
};
struct Config {
  Editor editor;bool hasEditor=false;
  uint8_t schema=1;
  char id[17]="0", preset[12]="Orientation", calibrationAddress[AddressSize]="/calibration";
  uint16_t listenPort=16889;
  Destination destinations[MaxDest];
  Message messages[MaxMessages];
  uint8_t destCount=1, messageCount=1, ackDestination=0;
  bool ackEnabled=false;
  char ackAddress[AddressSize]="/calibrated/{id}";
};
inline const char *compileEditor(const Editor &,const Config &,Config &);
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
inline const char *validateParts(const Config &c,const Message *messages,unsigned messageCount,unsigned schema) {
  if(schema!=1 && schema!=2) return "invalid_schema";
  if(!c.destCount || c.destCount>MaxDest || !messageCount || messageCount>MaxMessages) return "resource_limit";
  if(!c.listenPort || !*c.id || strlen(c.id)>16) return "invalid_config";
  for(const char *p=c.id;*p;++p) if(!((*p>='0'&&*p<='9')||(*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||*p=='_'||*p=='-')) return "invalid_id";
  if(schema==1 && strcmp(c.preset,"Orientation") && strcmp(c.preset,"Motion") && strcmp(c.preset,"Custom")) return "invalid_preset";
  char address[AddressSize],other[AddressSize],calibrationAddress[AddressSize],ack[AddressSize];
  if(!expand(c.calibrationAddress,c.id,calibrationAddress,sizeof(calibrationAddress)) || !expand(c.ackAddress,c.id,ack,sizeof(ack))) return "invalid_address";
  if(c.ackDestination>=c.destCount || !strcmp(calibrationAddress,ack)) return "address_conflict";
  for(unsigned i=0;i<c.destCount;++i) {
    const auto &d=c.destinations[i];
    if(!d.port || !d.ip[0] || d.ip[0]>=224 || d.ip[3]==255) return "invalid_destination";
    for(unsigned j=0;j<i;++j) if(!memcmp(d.ip,c.destinations[j].ip,4) && d.port==c.destinations[j].port) return "duplicate_destination";
  }
  for(unsigned i=0;i<messageCount;++i) {
    const auto &m=messages[i];
    if(schema==1 && (!m.hz || m.hz>100)) return "invalid_frequency";
    if(schema==2 && m.intervalMs<10) return "invalid_interval";
    if(!m.argCount || m.argCount>MaxArgs || !m.routes || (unsigned(m.routes)>>c.destCount)) return "invalid_arguments_or_routes";
    if(!expand(m.address,c.id,address,sizeof(address))) return "invalid_address";
    if(!strcmp(address,calibrationAddress)||!strcmp(address,ack)) return "address_conflict";
    if(messageSize(m,c.id)>MaxPacket) return "packet_too_large";
    for(unsigned k=0;k<m.argCount;++k) if(m.args[k]>=16) return "unknown_field";
    for(unsigned j=0;j<i;++j) {
      expand(messages[j].address,c.id,other,sizeof(other));
      if(!strcmp(address,other)) return "duplicate_address";
    }
  }
  // Worst case: all bundled messages due at once, per destination.
  for(unsigned r=0;r<c.destCount;++r) {
    size_t n=16;
    for(unsigned i=0;i<messageCount;++i) if((schema==2 || messages[i].bundle) && (schema==2 || (messages[i].routes&(1u<<r)))) n+=4+messageSize(messages[i],c.id);
    if(n>MaxPacket) return "packet_too_large";
  }
  return nullptr;
}
inline const char *validate(const Config &c){return validateParts(c,c.messages,c.messageCount,c.schema);}
// Default endpoint aliases preserve existing clients in either direction.
// Never steal a configured output/ACK address or alias a custom endpoint.
inline bool calibrationAlias(const Config &c,char *out,size_t cap) {
  const char *alias=nullptr;
  if(!strcmp(c.calibrationAddress,"/calibration"))alias="/tare";
  else if(!strcmp(c.calibrationAddress,"/calibration/{id}"))alias="/tare/{id}";
  else if(!strcmp(c.calibrationAddress,"/tare"))alias="/calibration";
  else if(!strcmp(c.calibrationAddress,"/tare/{id}"))alias="/calibration/{id}";
  if(!alias||!expand(alias,c.id,out,cap))return false;
  char other[AddressSize];
  expand(c.ackAddress,c.id,other,sizeof(other));if(!strcmp(out,other))return false;
  for(unsigned i=0;i<c.messageCount;++i){expand(c.messages[i].address,c.id,other,sizeof(other));if(!strcmp(out,other))return false;}
  return true;
}
inline const char *compileEditor(const Editor &editor,const Config &base,Config &out) {
  if(editor.intervalMs<10)return "invalid_interval";
  Message messages[5];unsigned messageCount=0;
  const uint8_t first[]={4,7,10,13,0},count[]={3,3,3,3,4};
  char expanded[AddressSize];
  if(!expand(editor.mainAddress,base.id,expanded,sizeof(expanded)))return "invalid_address";
  for(unsigned g=0;g<5;++g) {
    if(editor.selected[g]>>count[g])return "unknown_field";
    // Validate suffixes even for a group whose chips are deselected.
    char address[AddressSize];
    int n=snprintf(address,sizeof(address),"%s%s%s",editor.mainAddress,*editor.suffix[g]?"/":"",editor.suffix[g]);
    if(n<0 || size_t(n)>=sizeof(address) || !expand(address,base.id,expanded,sizeof(expanded)))return "invalid_address";
    if(!editor.selected[g])continue;
    unsigned index=0;
    for(;index<messageCount;++index) {
      char previous[AddressSize];expand(messages[index].address,base.id,previous,sizeof(previous));
      if(!strcmp(expanded,previous))break;
    }
    if(index==messageCount) {
      messages[index]=Message();auto &m=messages[index];
      strcpy(m.address,address);m.argCount=0;m.intervalMs=editor.intervalMs;
      m.routes=uint8_t((1u<<base.destCount)-1);++messageCount;
    }
    auto &m=messages[index];
    for(unsigned k=0;k<count[g];++k)if(editor.selected[g]&(1u<<k)) {
      if(m.argCount>=MaxArgs)return "invalid_arguments_or_routes";
      m.args[m.argCount++]=first[g]+k;
    }
  }
  const char *error=validateParts(base,messages,messageCount,2);
  if(error)return error;
  if(&out!=&base)out=base;
  out.schema=2;out.editor=editor;out.hasEditor=true;strcpy(out.preset,"Custom");out.messageCount=messageCount;
  for(unsigned i=0;i<messageCount;++i)out.messages[i]=messages[i];
  return nullptr;
}
inline void setPreset(const char *name,Config &c) {
  new(&c) Config();
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
}
inline Config preset(const char *name){Config c;setPreset(name,c);return c;}
// Explicit conversion only: never rewrite a saved configuration during boot.
// Schema 2 broadcasts every message and chooses bundles automatically.
// Ceil avoids requesting an interval shorter than the legacy nominal period.
inline const char *migrateV2(const Config &old, Config &out) {
  const char *error=validate(old);if(error)return error;
  Config next=old;next.schema=2;strcpy(next.preset,"Custom");
  for(unsigned i=0;i<next.messageCount;++i) {
    auto &m=next.messages[i];
    if(old.schema==1)m.intervalMs=uint16_t((1000u+m.hz-1)/m.hz);
    m.routes=uint8_t((1u<<next.destCount)-1);m.bundle=true;
  }
  error=validate(next);if(!error)out=next;return error;
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
  new(&c) Config(); Reader r(json); unsigned top=0, legacyMessages=0, modernMessages=0;
  bool ok=r.object([&](const char *key)->unsigned {
    unsigned n=0,bit=0;
    if(!strcmp(key,"schema")) {if(!r.number(n)||(n!=1 && n!=2))return 0;c.schema=n;bit=1;}
    else if(!strcmp(key,"preset")){if(!r.text(c.preset,sizeof(c.preset)))return 0;bit=2;}
    else if(!strcmp(key,"id")){if(!r.text(c.id,sizeof(c.id)))return 0;bit=4;}
    else if(!strcmp(key,"listen_port")){if(!r.number(n))return 0;c.listenPort=n;bit=8;}
    // Same seen bit rejects duplicates, including both old and new spellings.
    else if(!strcmp(key,"calibration_address")||!strcmp(key,"tare_address")){if(!r.text(c.calibrationAddress,sizeof(c.calibrationAddress)))return 0;bit=16;}
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
    } else if(!strcmp(key,"editor")) {
      unsigned mask=0;
      if(!r.object([&](const char *k)->unsigned {
        unsigned bit=0,v=0;
        if(!strcmp(k,"main_address")){if(!r.text(c.editor.mainAddress,AddressSize))return 0;bit=1;}
        else if(!strcmp(k,"interval_ms")){if(!r.number(v))return 0;c.editor.intervalMs=v;bit=2;}
        else if(!strcmp(k,"suffixes")){
          if(!r.take('['))return 0;
          for(unsigned g=0;g<5;++g){if(g&&!r.take(','))return 0;if(!r.text(c.editor.suffix[g],AddressSize))return 0;}
          if(!r.take(']'))return 0;bit=4;
        } else if(!strcmp(k,"selected")){
          if(!r.take('['))return 0;
          for(unsigned g=0;g<5;++g){if(g&&!r.take(','))return 0;if(!r.number(v)||v>255)return 0;c.editor.selected[g]=v;}
          if(!r.take(']'))return 0;bit=8;
        }
        mask|=bit;return bit;
      })||mask!=15)return 0;
      c.hasEditor=true;bit=128;
    } else if(!strcmp(key,"messages")) {
      if(!r.take('['))return 0;c.messageCount=0;
      do {
        if(c.messageCount>=MaxMessages)return 0;auto &m=c.messages[c.messageCount++];unsigned mask=0;
        if(!r.object([&](const char *k)->unsigned {
          unsigned b=0,v;
          if(!strcmp(k,"address")){if(!r.text(m.address,sizeof(m.address)))return 0;b=1;}
          else if(!strcmp(k,"hz")){if(!r.number(v))return 0;m.hz=v;b=2;}
          else if(!strcmp(k,"interval_ms")){if(!r.number(v))return 0;m.intervalMs=v;b=32;}
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
        }))return 0;
        if(mask==31)++legacyMessages;
        else if(mask==41)++modernMessages;
        else return 0;
      }while(r.take(','));
      if(!r.take(']'))return 0;bit=128;
    }
    top|=bit;return bit;
  });
  if(!ok||!r.end())return "invalid_config";
  if(c.schema==1 && (c.hasEditor || top!=255 || legacyMessages!=c.messageCount))return "invalid_config";
  if(c.schema==2 && (top!=253 || (!c.hasEditor && modernMessages!=c.messageCount)))return "invalid_config";
  if(c.schema==2) {strcpy(c.preset,"Custom");for(unsigned i=0;i<c.messageCount;++i){c.messages[i].routes=uint8_t((1u<<c.destCount)-1);c.messages[i].bundle=true;}}
  if(c.hasEditor)return compileEditor(c.editor,c,c);
  return validate(c);
}
// JSON strings here have already passed the restricted ASCII schema validator.
inline bool serialize(const Config &c,char *out,size_t cap) {
  size_t used=0;
  auto append=[&](const char *s)->bool {size_t n=strlen(s);if(used+n>=cap)return false;memcpy(out+used,s,n+1);used+=n;return true;};
  char b[512];
  snprintf(b,sizeof(b),"{\"schema\":1,\"preset\":\"%s\",\"id\":\"%s\",\"listen_port\":%u,\"calibration_address\":\"%s\",\"ack\":{\"enabled\":%s,\"address\":\"%s\",\"destination\":%u},\"destinations\":[",c.preset,c.id,c.listenPort,c.calibrationAddress,c.ackEnabled?"true":"false",c.ackAddress,c.ackDestination);
  if(c.schema==2) snprintf(b,sizeof(b),"{\"schema\":2,\"id\":\"%s\",\"listen_port\":%u,\"calibration_address\":\"%s\",\"ack\":{\"enabled\":%s,\"address\":\"%s\",\"destination\":%u},\"destinations\":[",c.id,c.listenPort,c.calibrationAddress,c.ackEnabled?"true":"false",c.ackAddress,c.ackDestination);
  if(!append(b))return false;
  for(unsigned i=0;i<c.destCount;++i){auto &d=c.destinations[i];snprintf(b,sizeof(b),"%s{\"ip\":\"%u.%u.%u.%u\",\"port\":%u}",i?",":"",d.ip[0],d.ip[1],d.ip[2],d.ip[3],d.port);if(!append(b))return false;}
  if(c.schema==2 && c.hasEditor) {
    const auto &e=c.editor;
    snprintf(b,sizeof(b),"],\"editor\":{\"main_address\":\"%s\",\"interval_ms\":%u,\"suffixes\":[",e.mainAddress,e.intervalMs);
    if(!append(b))return false;
    for(unsigned g=0;g<5;++g){snprintf(b,sizeof(b),"%s\"%s\"",g?",":"",e.suffix[g]);if(!append(b))return false;}
    snprintf(b,sizeof(b),"],\"selected\":[%u,%u,%u,%u,%u]}}",e.selected[0],e.selected[1],e.selected[2],e.selected[3],e.selected[4]);
    return append(b);
  }
  if(!append("],\"messages\":["))return false;
  for(unsigned i=0;i<c.messageCount;++i){auto &m=c.messages[i];snprintf(b,sizeof(b),"%s{\"address\":\"%s\",\"hz\":%u,\"bundle\":%s,\"fields\":[",i?",":"",m.address,m.hz,m.bundle?"true":"false");if(c.schema==2) snprintf(b,sizeof(b),"%s{\"address\":\"%s\",\"interval_ms\":%u,\"fields\":[",i?",":"",m.address,m.intervalMs);
    if(!append(b))return false;
    for(unsigned j=0;j<m.argCount;++j){snprintf(b,sizeof(b),"%s\"%s\"",j?",":"",fields()[m.args[j]].name);if(!append(b))return false;}
    if(c.schema==2){if(!append("]}"))return false;continue;}
    if(!append("],\"destinations\":["))return false;bool first=true;
    for(unsigned j=0;j<c.destCount;++j)if(m.routes&(1u<<j)){snprintf(b,sizeof(b),"%s%u",first?"":",",j);first=false;if(!append(b))return false;}
    if(!append("]}"))return false;
  }
  return append("]}");
}
}}
