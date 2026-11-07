#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
namespace axio {
// One request at a time, absolute timeout and bounded RX/TX work per tick.
template<class Socket,class Service> class DashboardHttp {
  Socket &socket;Service &service;
  char request[4097]={},header[512]={};
  size_t used=0,headerSize=0,offset=0,bodySize=0,bodyOffset=0;
  const char *body=nullptr;bool client=false;uint32_t since=0;
  template<class P> static auto finish(P &p,bool delivered,int)->decltype(p.finished(delivered),void()){p.finished(delivered);}
  template<class P> static void finish(P &,bool,long){}
  void drop(bool delivered=false){
    if(client)finish(service,delivered,0);
    socket.closeClient();client=false;used=headerSize=offset=bodySize=bodyOffset=0;body=nullptr;
    memset(request,0,sizeof(request));
  }
  void reply(unsigned code,const char *type,const char *data,size_t size,bool gzip=false){
    int n=snprintf(header,sizeof(header),"HTTP/1.1 %u %s\r\nContent-Type: %s\r\nContent-Length: %u\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nContent-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; frame-ancestors 'none'\r\n%s\r\n",code,code==200?"OK":"Error",type,unsigned(size),gzip?"Content-Encoding: gzip\r\n":"");
    if(n<0||size_t(n)>=sizeof(header)){drop();return;}headerSize=n;body=data;bodySize=size;offset=bodyOffset=0;
  }
  void error(unsigned code,const char *msg){reply(code,"application/json",msg,strlen(msg));}
  void parse(){
    char *end=strstr(request,"\r\n\r\n");if(!end){if(used>1024)error(431,"{\"error\":\"headers_too_large\"}");return;}
    size_t head=end-request+4;if(head>1024){error(431,"{\"error\":\"headers_too_large\"}");return;}
    char headers[1025];memcpy(headers,request,head);headers[head]=0;
    char method[8],path[100],version[16],extra;
    char *line=strstr(headers,"\r\n");if(!line){error(400,"{}");return;}*line=0;
    if(sscanf(headers,"%7s %99s %15s%c",method,path,version,&extra)!=3||strcmp(version,"HTTP/1.1")){error(400,"{}");return;}
    bool post=!strcmp(method,"POST");if(!post&&strcmp(method,"GET")){error(405,"{}");return;}
    bool host=false,lengthSeen=false,type=false,token=false;size_t length=0;char hostValue[100]={},origin[108]={};bool originSeen=false;
    for(char *p=line+2;*p&&*p!='\r';){char *next=strstr(p,"\r\n"),*value=strchr(p,':');if(!next||!value||value>next||*p==' '||*p=='\t'){error(400,"{}");return;}*next=0;*value++=0;while(*value==' '||*value=='\t')++value;
      if(!strcasecmp(p,"Transfer-Encoding")){error(400,"{}");return;}
      if(!strcasecmp(p,"Host")){if(host||!service.host(value)){error(403,"{}");return;}host=true;snprintf(hostValue,sizeof(hostValue),"%s",value);}
      if(!strcasecmp(p,"Origin")){if(originSeen||strlen(value)>=sizeof(origin)){error(403,"{}");return;}originSeen=true;strcpy(origin,value);}
      if(!strcasecmp(p,"Content-Length")){if(lengthSeen||!*value||strlen(value)>4){error(400,"{}");return;}lengthSeen=true;for(;*value;++value){if(*value<'0'||*value>'9'){error(400,"{}");return;}length=length*10+unsigned(*value-'0');}}
      if(!strcasecmp(p,"Content-Type")){if(type||strcmp(value,"application/json")){error(400,"{}");return;}type=true;}
      if(!strcasecmp(p,"X-Axio-Token")){if(token||!service.token(value)){error(403,"{}");return;}token=true;}
      p=next+2;
    }
    char expected[108];snprintf(expected,sizeof(expected),"http://%s",hostValue);
    if(!host||(originSeen&&strcmp(origin,expected))||(post&&(!lengthSeen||!type||!token))||(!post&&length)){error(403,"{\"error\":\"invalid_request\"}");return;}
    if(length>3072||head+length>=sizeof(request)){error(413,"{}");return;}
    if(used<head+length)return;if(used!=head+length){error(400,"{}");return;}
    auto result=service.handle(post,path,request+head);
    reply(result.code,result.type,result.body,result.size,result.gzip);
  }
 public:
  struct Reply {unsigned code;const char *type,*body;size_t size;bool gzip;};
  DashboardHttp(Socket &s,Service &p):socket(s),service(p){}
  bool idle()const{return !client;}
  void close(){drop();socket.closeServer();}
  void tick(uint32_t now){
    if(!client){if(!socket.acceptClient())return;client=true;since=now;}
    if(uint32_t(now-since)>=5000){drop();return;}
    if(headerSize){
      const char *data;size_t remaining;
      if(offset<headerSize){data=header+offset;remaining=headerSize-offset;}
      else {data=body+bodyOffset;remaining=bodySize-bodyOffset;}
      if(!remaining){drop();return;}if(remaining>512)remaining=512;
      int n=socket.write(data,remaining);if(n<0){drop();return;}
      if(offset<headerSize)offset+=n;else bodyOffset+=n;
      if(offset==headerSize&&bodyOffset==bodySize)drop(true);return;
    }
    size_t cap=sizeof(request)-used-1;if(cap>256)cap=256;if(!cap){error(413,"{}");return;}
    int n=socket.read(request+used,cap);if(n<0){drop();return;}if(!n)return;
    if(memchr(request+used,0,n)){error(400,"{}");return;}used+=n;request[used]=0;parse();
  }
};
}
