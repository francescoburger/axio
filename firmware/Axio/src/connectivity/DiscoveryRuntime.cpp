#include <Arduino.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <mdns.h>
#include "DiscoveryRuntime.h"
#include "DiscoveryManager.h"
#include "../transport/OscRuntime.h"
namespace {
using namespace axio::connectivity;
char hardware[13]={},hostname[64]={},reported[64]={};
bool identityReady=false;const char *identityError=nullptr;
struct Driver {
  mdns_search_once_t *search=nullptr;esp_netif_t *sta=nullptr;
  bool initialized=false;const char *error=nullptr;
  uint32_t checkedName=0;uint64_t nameChanges=0,filtered=0;
  bool pending()const{return search!=nullptr;}
  bool start(uint16_t port){
    if(mdns_init()!=ESP_OK){error="mdns_init_failed";return false;}
    initialized=true;sta=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    auto ap=esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    auto removed=ap?mdns_unregister_netif(ap):ESP_OK;
    mdns_txt_item_t txt[]={{"model","Axio"},{"schema","1"},{"hw",hardware}};
    if(!sta||(removed!=ESP_OK&&removed!=ESP_ERR_NOT_FOUND)||mdns_hostname_set(hostname)!=ESP_OK||
       mdns_instance_name_set(hostname)!=ESP_OK||mdns_service_add(nullptr,"_axio","_udp",port,txt,3)!=ESP_OK){
      error="mdns_service_failed";stop();return false;
    }
    if(mdns_netif_action(sta,MDNS_EVENT_ENABLE_IP4)!=ESP_OK){error="mdns_sta_enable_failed";stop();return false;}
    memcpy(reported,hostname,sizeof(reported));checkedName=uint32_t(millis());error=nullptr;return true;
  }
  bool port(uint16_t value){if(mdns_service_port_set("_axio","_udp",value)==ESP_OK){error=nullptr;return true;}error="mdns_port_failed";return false;}
  void disable(){
    if(sta){
      auto action=mdns_event_actions_t(MDNS_EVENT_DISABLE_IP4|MDNS_EVENT_DISABLE_IP6);
      if(mdns_netif_action(sta,action)!=ESP_OK)error="mdns_disable_failed";
      auto removed=mdns_unregister_netif(sta);
      if(removed!=ESP_OK&&removed!=ESP_ERR_NOT_FOUND)error="mdns_detach_failed";
      sta=nullptr;
    }
  }
  void stop(){if(initialized)mdns_free();initialized=false;sta=nullptr;}
  bool query(){search=mdns_query_async_new(nullptr,"_axio","_udp",MDNS_TYPE_PTR,2000,MaxPeers,nullptr);error=search?nullptr:"mdns_query_failed";return pending();}
  bool metadata(const mdns_result_t &r,char hw[13]){
    if(!r.txt||!r.txt_value_len||r.txt_count>16)return false;
    unsigned model=0,schema=0,ids=0;
    for(size_t i=0;i<r.txt_count;++i){
      const auto &t=r.txt[i];if(!t.key||!t.value)continue;
      if(!strcmp(t.key,"model")){if(r.txt_value_len[i]!=4||memcmp(t.value,"Axio",4))return false;++model;}
      if(!strcmp(t.key,"schema")){if(r.txt_value_len[i]!=1||t.value[0]!='1')return false;++schema;}
      if(!strcmp(t.key,"hw")){if(r.txt_value_len[i]!=12)return false;memcpy(hw,t.value,12);hw[12]=0;++ids;}
    }
    return model==1&&schema==1&&ids==1&&hardwareId(hw);
  }
  bool poll(PeerCache &cache,bool accept,uint32_t now){
    if(!search)return false;
    mdns_result_t *results=nullptr;
    // A zero wait budget is essential: queryService/query_ptr are synchronous.
    if(!mdns_query_async_get_results(search,0,&results,nullptr))return false;
    if(accept){unsigned visited=0;
      for(auto r=results;r&&visited<MaxPeers;r=r->next,++visited){
        char hw[13]={};
        if(r->esp_netif!=sta||r->ip_protocol!=MDNS_IP_PROTOCOL_V4||!r->hostname||!r->service_type||!r->proto||
           strcmp(r->service_type,"_axio")||strcmp(r->proto,"_udp")||!metadata(*r,hw)){++filtered;continue;}
        unsigned addresses=0;
        for(auto a=r->addr;a&&addresses<8;a=a->next,++addresses){
          if(a->addr.type!=ESP_IPADDR_TYPE_V4)continue;
          uint8_t ip[4];memcpy(ip,&a->addr.u_addr.ip4.addr,4);
          if(a->addr.u_addr.ip4.addr==uint32_t(WiFi.localIP()))continue;
          if(cache.observe(hw,r->hostname,ip,r->port,r->ttl,now,hardware))break;
        }
      }
    }
    if(results)mdns_query_results_free(results);
    if(mdns_query_async_delete(search)!=ESP_OK){error="mdns_query_delete_failed";return false;}
    search=nullptr;error=nullptr;return true;
  }
  void updateName(uint32_t now){
    if(uint32_t(now-checkedName)<1000)return;checkedName=now;char actual[64]={};
    if(mdns_hostname_get(actual)==ESP_OK&&hostLabel(actual,hardware)&&strcmp(actual,reported)){
      memcpy(reported,actual,sizeof(reported));++nameChanges;
    }
  }
} driver;
DiscoveryManager<Driver> manager(driver);
char response[4096];
}
void setupDiscoveryIdentity(){
  uint8_t mac[6]={};identityReady=false;
  if(esp_read_mac(mac,ESP_MAC_WIFI_STA)!=ESP_OK){identityError="hardware_identity_failed";return;}
  identity(mac,hardware,hostname);memcpy(reported,hostname,sizeof(reported));
  manager.jitter=uint32_t(mac[4])*256+mac[5];
  identityReady=WiFi.setHostname(hostname);identityError=identityReady?nullptr:"hostname_failed";
}
void tickDiscovery(bool sta){
  bool allowed=identityReady&&sta&&!(WiFi.getMode()&WIFI_AP);
  manager.tick(allowed,uint32_t(WiFi.localIP()),oscListenPort(),uint32_t(millis()));
  if(manager.active)driver.updateName(uint32_t(millis()));
}
const char *discoveryHostname(){return identityReady?reported:"";}
const char *discoveryState(){return !identityReady?"identity_error":manager.draining?"draining":!manager.active?"offline":driver.pending()?"querying":"ready";}
void discoveryJson(char *out,size_t cap){
  snprintf(out,cap,"{\"state\":\"%s\",\"active\":%s,\"service\":\"_axio._udp\",\"service_port\":%u,\"peers\":%u,\"query_pending\":%s,\"starts\":%llu,\"failures\":%llu,\"queries\":%llu,\"completed\":%llu,\"query_failures\":%llu,\"filtered\":%llu,\"rejected\":%llu,\"overflow\":%llu,\"name_changes\":%llu,\"last_transition\":\"%s\",\"error\":\"%s\"}",
    discoveryState(),manager.active?"true":"false",oscListenPort(),manager.cache.count(),driver.pending()?"true":"false",
    (unsigned long long)manager.starts,(unsigned long long)manager.failures,(unsigned long long)manager.queries,(unsigned long long)manager.completed,
    (unsigned long long)manager.queryFailures,(unsigned long long)driver.filtered,(unsigned long long)manager.cache.rejected,(unsigned long long)manager.cache.overflow,
    (unsigned long long)driver.nameChanges,manager.reason,identityError?identityError:(driver.error?driver.error:""));
}
bool discoveryCommand(const char *id,char *command,char *out){
  if(strcmp(command,"DISCOVERY GET"))return false;
  manager.cache.expire(uint32_t(millis()));
  size_t used=snprintf(response,sizeof(response),"{\"state\":\"%s\",\"peers\":[",discoveryState());bool comma=false;
  for(unsigned i=0;i<MaxPeers&&used<sizeof(response);++i){
    const auto &p=manager.cache.get(i);if(!p.port)continue;
    used+=snprintf(response+used,sizeof(response)-used,"%s{\"hw\":\"%s\",\"hostname\":\"%s.local\",\"ip\":\"%u.%u.%u.%u\",\"port\":%u,\"age_ms\":%u}",
      comma?",":"",p.hw,p.host,p.ip[0],p.ip[1],p.ip[2],p.ip[3],p.port,unsigned(uint32_t(millis())-p.seen));comma=true;
  }
  if(used+3>sizeof(response)){axio::SerialProtocol::reply(out,id,"response_too_large");return true;}
  strcpy(response+used,"]}");axio::SerialProtocol::reply(out,id,nullptr,response);return true;
}
