#pragma once
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
namespace axio {
class JsonWriter {
  char *out;size_t cap,used=0;
 public:
  bool ok=true;
  JsonWriter(char *p,size_t n):out(p),cap(n){if(cap)*out=0;else ok=false;}
  void add(const char *format,...){
    if(!ok)return;va_list args;va_start(args,format);int n=vsnprintf(out+used,cap-used,format,args);va_end(args);
    if(n<0||size_t(n)>=cap-used){ok=false;return;}used+=n;
  }
  void quote(const char *value){
    add("\"");for(const unsigned char *p=reinterpret_cast<const unsigned char *>(value);*p;++p){
      if(*p=='"'||*p=='\\')add("\\%c",*p);
      else if(*p<32||*p>=127)add("\\u%04x",unsigned(*p));else add("%c",*p);
    }add("\"");
  }
  template<class Fn> void raw(Fn fn){
    if(!ok)return;if(!fn(out+used,cap-used)){ok=false;return;}
    size_t n=strnlen(out+used,cap-used);if(n>=cap-used){ok=false;return;}used+=n;
  }
  size_t size()const{return used;}
};
}
