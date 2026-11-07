#pragma once
#include "../osc/Config.h"
#include "WebAssets.h"
namespace axio {
// Platform keeps hardware, persistent state and formatting outside HTTP parsing.
template<class Platform> class DashboardService {
  Platform &platform;
  char output[8192]={};
  enum Action {None,Network,Identity,Restart,Sleep} pending=None;
 public:
  struct Result {unsigned code;const char *type,*body;size_t size;bool gzip;};
  explicit DashboardService(Platform &p):platform(p){}
  bool host(const char *s){return platform.host(s);}
  bool token(const char *s){return platform.token(s);}
  Result json(unsigned code,const char *body){return {code,"application/json",body,strlen(body),false};}
  Result fail(const char *error,unsigned code=400){snprintf(output,sizeof(output),"{\"ok\":false,\"error\":\"%s\"}",error);return json(code,output);}
  void finished(bool delivered){
    Action action=pending;pending=None;
    // A saved network/identity must converge even if the browser vanished.
    if(action==Network)platform.activateNetwork();
    if(action==Identity)platform.activateIdentity();
    if(delivered&&(action==Restart||action==Sleep))platform.power(action==Sleep);
  }
  Result handle(bool post,const char *path,char *body){
    pending=None;
    if(!post){
      if(!strncmp(path,"/?",2)||!strcmp(path,"/"))return {200,"text/html; charset=utf-8",reinterpret_cast<const char *>(web::index),sizeof(web::index),true};
      if(!strcmp(path,"/style.css"))return {200,"text/css",reinterpret_cast<const char *>(web::style),sizeof(web::style),true};
      if(!strcmp(path,"/app.js"))return {200,"application/javascript",reinterpret_cast<const char *>(web::app),sizeof(web::app),true};
      if(!strcmp(path,"/api/session")){platform.session(output,sizeof(output));return json(200,output);}
      if(!strcmp(path,"/api/snapshot")){if(!platform.snapshot(output,sizeof(output)))return fail("snapshot_too_large",500);return json(200,output);}
      if(!strcmp(path,"/api/sample")){
        if(!platform.sample(output,sizeof(output)))return fail("telemetry_rate_limited",429);
        return json(200,output);
      }
      if(!strcmp(path,"/api/battery")){platform.battery(output,sizeof(output));return json(200,output);}
      if(!strcmp(path,"/api/power")){platform.powerStatus(output,sizeof(output));return json(200,output);}
      return fail("not_found",404);
    }
    if(platform.busy())return fail("device_busy",409);
    if(!JsonSyntax(body).object())return fail("invalid_json");
    if(!strcmp(path,"/api/osc/validate")||!strcmp(path,"/api/osc/apply")){
      const char *error=platform.osc(body,!strcmp(path,"/api/osc/apply"));
      return error?fail(error):json(200,"{\"ok\":true}");
    }
    if(!strcmp(path,"/api/network/validate")||!strcmp(path,"/api/network/apply")){
      bool apply=!strcmp(path,"/api/network/apply");const char *error=platform.network(body,apply);
      if(error)return fail(error);
      if(apply)pending=Network;
      return json(200,apply?"{\"ok\":true,\"persistent\":true,\"activation_pending\":true}":"{\"ok\":true,\"valid\":true}");
    }
    osc::Reader reader(body);
    if(!strcmp(path,"/api/identity")){
      char id[17]={};
      if(!reader.object([&](const char *key)->unsigned {return !strcmp(key,"id")&&reader.text(id,sizeof(id))?1:0;})||!reader.end())return fail("invalid_id");
      const char *error=platform.identity(id);if(error)return fail(error);
      pending=Identity;return json(200,"{\"ok\":true,\"persistent\":true}");
    }
    if(!strcmp(path,"/api/command")){
      char cmd[24]={};
      if(!reader.object([&](const char *key)->unsigned {return !strcmp(key,"command")&&reader.text(cmd,sizeof(cmd))?1:0;})||!reader.end())return fail("invalid_command");
      // Do not expose raw USB configuration, AP secrets or arbitrary commands.
      if(strcmp(cmd,"TARE")&&strcmp(cmd,"DIAG RESET"))return fail("command_not_allowed",403);
      const char *error=platform.command(cmd);return error?fail(error):json(200,"{\"ok\":true}");
    }
    if(!strcmp(path,"/api/control")){
      char action[12]={};bool confirmed=false;unsigned mask=0;
      bool valid=reader.object([&](const char *key)->unsigned {
        unsigned bit=0;
        if(!strcmp(key,"action")&&reader.text(action,sizeof(action)))bit=1;
        if(!strcmp(key,"confirm")&&reader.boolean(confirmed))bit=2;
        mask|=bit;return bit;
      });
      if(!valid||!reader.end()||mask!=3||!confirmed)return fail("confirmation_required");
      if(strcmp(action,"restart")&&strcmp(action,"sleep"))return fail("invalid_action");
      if(!strcmp(action,"sleep")&&!platform.canSleep())return fail("imu_unavailable");
      pending=!strcmp(action,"sleep")?Sleep:Restart;
      return json(200,"{\"ok\":true,\"acknowledged\":true,\"completion_verified\":false}");
    }
    return fail("not_found",404);
  }
};
}
