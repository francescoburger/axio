#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

namespace axio {
// Strict bounded JSON syntax check. Configuration semantics belong to the engine.
class JsonSyntax {
  const char *p;
  void ws() { while (*p==' ' || *p=='\t' || *p=='\r' || *p=='\n') ++p; }
  bool string() {
    if (*p++ != '"') return false;
    while (*p && *p!='"') {
      unsigned char c=*p++;
      if (c<32 || c>126) return false; // ASCII wire encoding; Unicode via \uXXXX
      if (c=='\\') {
        if (*p=='u') {
          ++p;
          for (int i=0;i<4;++i) {
            if (!((*p>='0'&&*p<='9')||(*p>='a'&&*p<='f')||(*p>='A'&&*p<='F'))) return false;
            ++p;
          }
        } else { if (!*p || !strchr("\"\\/bfnrt",*p)) return false; ++p; }
      }
    }
    if (*p!='"') return false;
    ++p; return true;
  }
  bool value(unsigned depth) {
    if (depth>8) return false;
    ws();
    if (*p=='"') return string();
    if (*p=='{' || *p=='[') {
      bool object=*p++=='{'; char end=object?'}':']'; ws();
      if (*p==end) { ++p; return true; }
      do {
        ws();
        if (object) { if (*p!='"' || !string()) return false; ws(); if (*p++!=':') return false; }
        if (!value(depth+1)) return false;
        ws(); if (*p==end) { ++p; return true; }
        if (*p!=',') return false;
        ++p;
      } while (*p);
      return false;
    }
    const char *literals[]={"true", "false", "null"};
    for (const char *literal : literals) {
      size_t n=strlen(literal);
      if (strncmp(p,literal,n)==0) { p+=n; return true; }
    }
    if (*p=='-') ++p;
    if (*p=='0') ++p;
    else { if (*p<'1'||*p>'9') return false; do { ++p; } while (*p>='0'&&*p<='9'); }
    if (*p=='.') { ++p; if (*p<'0'||*p>'9') return false; do { ++p; } while (*p>='0'&&*p<='9'); }
    if (*p=='e'||*p=='E') {
      ++p; if (*p=='+'||*p=='-') ++p;
      if (*p<'0'||*p>'9') return false;
      do { ++p; } while (*p>='0'&&*p<='9');
    }
    return true;
  }
 public:
  explicit JsonSyntax(const char *s):p(s) {}
  bool object() { ws(); if (*p!='{' || !value(0)) return false; ws(); return !*p; }
};

class SerialProtocol {
 public:
  static constexpr size_t MaxLine=1024, MaxReply=2048;
  static constexpr uint32_t TimeoutMs=1000;
 private:
  char input[MaxLine+1] = {};
  size_t length=0;
  uint32_t started=0;
  bool overflow=false, invalid=false, discarding=false;
  void clear() { length=0; overflow=false; invalid=false; }
  const char *requestId() {
    // Recover correlation even for oversized, invalid or incomplete requests.
    if (strncmp(input,"AXIO ",5)!=0) return nullptr;
    char *id=input+5; size_t n=0;
    while (id[n] && id[n]!=' ' && n<=16) {
      char c=id[n];
      if (!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-')) return nullptr;
      ++n;
    }
    if (!n || n>16) return nullptr;
    id[n]=0; return id;
  }
 public:
  static void reply(char *out, const char *id, const char *error, const char *data="{}") {
    if (id) snprintf(out,MaxReply,"{\"axio\":1,\"id\":\"%s\",\"ok\":%s,\"%s\":%s%s%s}\n",
        id,error?"false":"true",error?"error":"data",error?"\"":"",error?error:data,error?"\"":"");
    else snprintf(out,MaxReply,"{\"axio\":1,\"id\":null,\"ok\":false,\"error\":\"%s\"}\n",error);
  }
  // Caller stops consuming RX while a reply is pending. One request in flight.
  template<class Service> bool feed(char c, uint32_t now, Service &service, char *out) {
    if (discarding) { if (c=='\n') discarding=false; return false; }
    if (c=='\n') {
      input[length]=0;
      if (length && input[length-1]=='\r') input[--length]=0;
      if (!length && !overflow && !invalid) return false;
      if (overflow || invalid) {
        reply(out,requestId(),overflow?"line_too_long":"invalid_encoding"); clear(); return true;
      }
      // Tokenize only prefix/id; command spaces remain intact.
      char *command=strchr(input+ (length>=5?5:length),' ');
      const char *id=requestId();
      if (!id || !command || !command[1]) reply(out,id,"invalid_request");
      else service.execute(id,command+1,out);
      clear(); return true;
    }
    if (!length) started=now;
    if (length<MaxLine) {
      input[length++]=c; input[length]=0;
      if ((static_cast<unsigned char>(c)<32 && c!='\r' && c!='\t') || static_cast<unsigned char>(c)>126) invalid=true;
    } else overflow=true;
    return false;
  }
  bool tick(uint32_t now, char *out) {
    if (length && uint32_t(now-started)>=TimeoutMs) {
      reply(out,requestId(),"incomplete_timeout"); clear(); discarding=true; return true;
    }
    return false;
  }
};
}
