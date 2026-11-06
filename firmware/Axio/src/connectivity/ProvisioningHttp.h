#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
namespace axio { namespace connectivity {
// A single short-lived request. Transport calls are nonblocking, with fixed budgets.
template<class Socket,class Service> class ProvisioningHttp {
  Socket &socket;Service &service;
  char request[1537]={},output[4096]={},token[33]={};
  size_t used=0,pending=0,offset=0;uint32_t since=0;
  bool client=false;char origin[40]={};
  static bool equal(const char *a,const char *b){while(*a&&*b){char x=*a++,y=*b++;if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return false;}return !*a&&!*b;}
  static int hex(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
  static bool decode(char *p){char *out=p;while(*p){unsigned char c=*p++;if(c=='+')c=' ';else if(c=='%'){if(!p[0]||!p[1]||hex(p[0])<0||hex(p[1])<0)return false;c=uint8_t(hex(p[0])*16+hex(p[1]));p+=2;}if(!c)return false;*out++=char(c);}*out=0;return true;}
  void reply(unsigned code,const char *type,const char *body){
    // body may point into output's reserved payload region.
    size_t len=strlen(body);char header[384];
    int n=snprintf(header,sizeof(header),"HTTP/1.1 %u %s\r\nContent-Type: %s\r\nContent-Length: %u\r\nCache-Control: no-store\r\nConnection: close\r\nX-Content-Type-Options: nosniff\r\nContent-Security-Policy: default-src 'none'; form-action 'self'; frame-ancestors 'none'\r\n\r\n",code,code==200?"OK":"Error",type,unsigned(len));
    if(n<0||size_t(n)>=sizeof(header)||size_t(n)+len>=sizeof(output)){drop();return;}
    memmove(output+n,body,len);memcpy(output,header,n);pending=size_t(n)+len;offset=0;
  }
  void error(unsigned code,const char *message){reply(code,"text/plain; charset=utf-8",message);}
  void form(char *body){
    char *fields[5]={};const char *names[]={"ssid","password","priority","action","token"};
    while(*body){char *next=strchr(body,'&');if(next)*next++=0;char *value=strchr(body,'=');
      if(!value){error(400,"invalid_form");return;}*value++=0;
      if(!decode(body)||!decode(value)){error(400,"invalid_encoding");return;}
      unsigned i=0;while(i<5&&strcmp(body,names[i]))++i;
      if(i==5||fields[i]){error(400,"invalid_field");return;}fields[i]=value;if(!next)break;body=next;
    }
    if(!fields[4]||strlen(fields[4])!=32||strcmp(fields[4],token)){error(403,"invalid_token");return;}
    if(!fields[0]||!fields[1]||!fields[2]||!fields[3]){error(400,"missing_field");return;}
    bool erase=!strcmp(fields[3],"delete");
    if(!erase&&strcmp(fields[3],"save")){error(400,"invalid_action");return;}
    unsigned priority=0;const char *p=fields[2];if(!*p||strlen(p)>3){error(400,"invalid_priority");return;}
    while(*p){if(*p<'0'||*p>'9'){error(400,"invalid_priority");return;}priority=priority*10+unsigned(*p++-'0');}
    if(priority>255){error(400,"invalid_priority");return;}
    const char *failure=service.update(fields[0],fields[1],priority,erase);
    if(failure){error(400,failure);return;}
    reply(200,"text/plain; charset=utf-8","Saved. STA connection will be attempted; the AP closes after success.");
  }
  void parse(){
    char *end=strstr(request,"\r\n\r\n");if(!end){if(used>1024)error(431,"headers_too_large");return;}
    size_t head=size_t(end-request)+4;if(head>1024){error(431,"headers_too_large");return;}
    char *line=strstr(request,"\r\n");if(!line){error(400,"invalid_request");return;}
    // Parse a copy of the headers only once the bounded body has arrived.
    char headers[1025];memcpy(headers,request,head);headers[head]=0;
    char *first=strstr(headers,"\r\n");*first=0;
    bool post=!strcmp(headers,"POST /network HTTP/1.1");
    bool root=!strcmp(headers,"GET / HTTP/1.1"),status=!strcmp(headers,"GET /status HTTP/1.1"),networks=!strcmp(headers,"GET /networks HTTP/1.1");
    if(!post&&!root&&!status&&!networks){error(404,"not_found");return;}
    bool lengthSeen=false,hostSeen=false,typeSeen=false,originSeen=false;size_t length=0;
    char *p=first+2;
    while(*p&&p[0]!='\r'){
      char *next=strstr(p,"\r\n");if(!next){error(400,"invalid_headers");return;}*next=0;
      char *value=strchr(p,':');if(!value||p[0]==' '||p[0]=='\t'){error(400,"invalid_header");return;}*value++=0;while(*value==' '||*value=='\t')++value;
      if(equal(p,"Transfer-Encoding")){error(400,"transfer_encoding_unsupported");return;}
      if(equal(p,"Content-Length")){
        if(lengthSeen||!*value||strlen(value)>4){error(400,"invalid_length");return;}lengthSeen=true;
        while(*value){if(*value<'0'||*value>'9'){error(400,"invalid_length");return;}length=length*10+unsigned(*value++-'0');}
      }else if(equal(p,"Host")){
        if(hostSeen||(strcmp(value,origin+7)&&strncmp(value,origin+7,strlen(origin+7)))){error(403,"invalid_host");return;}
        // Only the literal AP address, optionally :80, is accepted.
        size_t n=strlen(origin+7);if(strcmp(value,origin+7)&&(strlen(value)!=n+3||strcmp(value+n,":80"))){error(403,"invalid_host");return;}hostSeen=true;
      }else if(equal(p,"Origin")){
        if(originSeen||strcmp(value,origin)){error(403,"invalid_origin");return;}originSeen=true;
      }else if(equal(p,"Content-Type")){
        if(typeSeen||strcmp(value,"application/x-www-form-urlencoded")){error(400,"invalid_content_type");return;}typeSeen=true;
      }
      p=next+2;
    }
    if(!hostSeen||(post&&(!lengthSeen||!typeSeen))||(!post&&length)){error(400,"invalid_headers");return;}
    if(length>512){error(413,"body_too_large");return;}
    if(used<head+length)return;
    if(used!=head+length){error(400,"extra_data");return;}
    if(post){form(request+head);return;}
    if(root){
      char *body=output+512;
      snprintf(body,sizeof(output)-512,"<!doctype html><meta charset=utf-8><title>Axio Wi-Fi</title><h1>Axio Wi-Fi</h1><p>Temporary AP provisioning. <a href=/status>Status</a> | <a href=/networks>Saved networks (SSID hex)</a></p><form method=post action=/network><input type=hidden name=token value=%s><p><label>SSID <input name=ssid maxlength=32 required></label></p><p><label>Password <input name=password type=password maxlength=64 autocomplete=new-password></label> (empty: open STA)</p><p><label>Priority <input name=priority type=number min=0 max=255 value=0 required></label></p><button name=action value=save>Save network</button> <button name=action value=delete>Delete network</button></form><p>The AP closes after STA connects. Recover AP credentials via USB: AP CREDENTIALS.</p>",token);
      reply(200,"text/html; charset=utf-8",body);
    }else {char *body=output+512;bool ok=status?service.status(body,sizeof(output)-512):service.networks(body,sizeof(output)-512);if(ok)reply(200,"application/json",body);else error(500,"response_failed");}
  }
  void drop(){socket.closeClient();client=false;used=pending=offset=0;memset(request,0,sizeof(request));}
 public:
  ProvisioningHttp(Socket &s,Service &p):socket(s),service(p){}
  void begin(const char *ip,const char *sessionToken){close();snprintf(origin,sizeof(origin),"http://%s",ip);memcpy(token,sessionToken,32);token[32]=0;}
  void close(){drop();socket.closeServer();memset(token,0,sizeof(token));}
  void tick(uint32_t now){
    if(!client){if(!socket.acceptClient())return;client=true;since=now;}
    if(uint32_t(now-since)>=3000){drop();return;}
    if(pending){int n=socket.write(output+offset,pending-offset>256?256:pending-offset);if(n<0){drop();return;}offset+=size_t(n);if(offset==pending)drop();return;}
    size_t cap=sizeof(request)-1-used;if(cap>256)cap=256;
    if(!cap){error(413,"request_too_large");return;}
    int n=socket.read(request+used,cap);if(n<0){drop();return;}if(!n)return;
    for(int i=0;i<n;++i)if(!request[used+i]){error(400,"invalid_encoding");return;}
    used+=size_t(n);request[used]=0;parse();
  }
};
}}
