#pragma once
#include "esp_wifi.h"
#include <assert.h>
#include <string>
constexpr int ESP_ERR_NOT_FOUND=-3,ESP_IPADDR_TYPE_V4=0,MDNS_IP_PROTOCOL_V4=0,MDNS_TYPE_PTR=12,MDNS_TYPE_A=1;
using esp_err_t=int;
enum mdns_event_actions_t {MDNS_EVENT_ENABLE_IP4=2,MDNS_EVENT_ANNOUNCE_IP4=8,MDNS_EVENT_DISABLE_IP4=32,MDNS_EVENT_DISABLE_IP6=64};
struct esp_netif_t {bool station;};
inline esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key){static esp_netif_t sta{true},ap{false};return !strcmp(key,"WIFI_STA_DEF")?&sta:&ap;}
struct mdns_txt_item_t {const char *key,*value;};
struct mdns_ip_addr_t {struct {struct {struct {uint32_t addr=0;} ip4;} u_addr;int type=ESP_IPADDR_TYPE_V4;} addr;mdns_ip_addr_t *next=nullptr;};
struct mdns_result_t {
  mdns_result_t *next=nullptr;esp_netif_t *esp_netif=nullptr;uint32_t ttl=90;int ip_protocol=MDNS_IP_PROTOCOL_V4;
  char *hostname=nullptr,*service_type=nullptr,*proto=nullptr;uint16_t port=16889;
  mdns_txt_item_t *txt=nullptr;uint8_t *txt_value_len=nullptr;size_t txt_count=0;mdns_ip_addr_t *addr=nullptr;
};
struct mdns_search_once_t {};
struct FakeMdns {
  bool initialized=false,hasQuery=false,ready=false,failInit=false,failService=false,failQuery=false,failPort=false;
  bool apRemoved=false,staRemoved=false;unsigned starts=0,frees=0,queries=0,deleted=0,resultsFreed=0,portChanges=0;
  uint16_t port=0;uint32_t queryAt=0;bool peer=false,badSchema=false,fromAp=false;
  std::string hostname,instance,model,schema,hw;
  std::string queryKind,delegatedHost;
  uint32_t queryTimeout=2000;
  bool entryOwner=false,entrySmaller=false,failAlias=false,failAnnounce=false,noAddress=false;
  unsigned announcements=0;
  mdns_search_once_t handle;mdns_result_t result;mdns_ip_addr_t address;mdns_txt_item_t txt[3];uint8_t lengths[3]={4,1,12};
};
inline FakeMdns &fakeMdns(){static FakeMdns m;return m;}
inline int mdns_init(){auto &m=fakeMdns();if(m.failInit)return -1;assert(!m.initialized);m.delegatedHost.clear();m.initialized=true;m.staRemoved=m.apRemoved=false;++m.starts;return ESP_OK;}
inline void mdns_free(){auto &m=fakeMdns();assert(!m.hasQuery);m.initialized=false;++m.frees;}
inline int mdns_unregister_netif(esp_netif_t *n){if(n->station)fakeMdns().staRemoved=true;else fakeMdns().apRemoved=true;return ESP_OK;}
inline int mdns_netif_action(esp_netif_t *netif,mdns_event_actions_t action){
  if(action==MDNS_EVENT_ANNOUNCE_IP4){assert(netif->station&&fakeMdns().initialized&&!fakeMdns().staRemoved);++fakeMdns().announcements;if(fakeMdns().failAnnounce)return -1;}return ESP_OK;
}
inline int mdns_hostname_set(const char *name){fakeMdns().hostname=name;return ESP_OK;}
inline int mdns_hostname_get(char *out){strcpy(out,fakeMdns().hostname.c_str());return ESP_OK;}
inline int mdns_instance_name_set(const char *name){fakeMdns().instance=name;return ESP_OK;}
inline int mdns_service_add(const char *,const char *service,const char *proto,uint16_t port,mdns_txt_item_t *txt,size_t count){
  auto &m=fakeMdns();if(m.failService)return -1;if(!strcmp(service,"_http")){assert(!strcmp(proto,"_tcp")&&count==1);return ESP_OK;}if(!strcmp(service,"_axio-entry")){assert(!strcmp(proto,"_udp")&&count==3);return ESP_OK;}assert(!strcmp(service,"_axio")&&!strcmp(proto,"_udp")&&count==3);
  m.port=port;for(size_t i=0;i<count;++i){if(!strcmp(txt[i].key,"model"))m.model=txt[i].value;else if(!strcmp(txt[i].key,"schema"))m.schema=txt[i].value;else if(!strcmp(txt[i].key,"hw"))m.hw=txt[i].value;else assert(false);}return ESP_OK;
}
inline int mdns_service_port_set(const char *service,const char *,uint16_t port){auto &m=fakeMdns();if(m.failPort||(!m.delegatedHost.empty()&&!strcmp(service,"_axio")))return -1;m.port=port;++m.portChanges;return ESP_OK;}
inline mdns_search_once_t *mdns_query_async_new(const char *name,const char *service,const char *proto,int type,uint32_t timeout,size_t max,void *){
  auto &m=fakeMdns();if(m.failQuery)return nullptr;assert(!m.hasQuery&&m.apRemoved&&!m.staRemoved);
  if(type==MDNS_TYPE_A){assert(name&&!strcmp(name,"axio")&&!service&&!proto&&timeout==600);m.queryKind="a";}
  else {assert(!name&&service&&proto&&!strcmp(proto,"_udp")&&type==MDNS_TYPE_PTR&&max==16);m.queryKind=service;assert((m.queryKind=="_axio"&&timeout==2000)||(m.queryKind=="_axio-entry"&&timeout==1200));}
  m.queryTimeout=timeout;
  m.hasQuery=true;m.ready=false;m.queryAt=millis();++m.queries;return &m.handle;
}
inline bool mdns_query_async_get_results(mdns_search_once_t *,uint32_t timeout,mdns_result_t **out,uint8_t *){
  auto &m=fakeMdns();assert(timeout==0);if(!m.ready&&uint32_t(millis()-m.queryAt)<m.queryTimeout)return false;
  m.ready=true;*out=nullptr;
  if(m.peer&&m.queryKind=="_axio"){
    m.result=mdns_result_t();m.result.esp_netif=esp_netif_get_handle_from_ifkey(m.fromAp?"WIFI_AP_DEF":"WIFI_STA_DEF");
    m.result.hostname=const_cast<char *>("axio-aabbccddeeff");m.result.service_type=const_cast<char *>("_axio");m.result.proto=const_cast<char *>("_udp");
    m.txt[0]={"model","Axio"};m.txt[1]={"schema",m.badSchema?"2":"1"};m.txt[2]={"hw","aabbccddeeff"};
    m.result.txt=m.txt;m.result.txt_value_len=m.lengths;m.result.txt_count=3;
    uint8_t ip[4]={192,168,1,42};memcpy(&m.address.addr.u_addr.ip4.addr,ip,4);m.result.addr=m.noAddress?nullptr:&m.address;*out=&m.result;
  }
  if((m.entryOwner&&m.queryKind=="a")||(m.entrySmaller&&m.queryKind=="_axio-entry")){
    m.result=mdns_result_t();m.result.esp_netif=esp_netif_get_handle_from_ifkey(m.fromAp?"WIFI_AP_DEF":"WIFI_STA_DEF");
    m.result.hostname=const_cast<char *>(m.queryKind=="a"?"axio":"axio-000000000001");
    m.result.service_type=const_cast<char *>("_axio-entry");m.result.proto=const_cast<char *>("_udp");
    m.txt[0]={"model","Axio"};m.txt[1]={"schema",m.badSchema?"2":"1"};m.txt[2]={"hw","000000000001"};
    m.result.txt=m.txt;m.result.txt_value_len=m.lengths;m.result.txt_count=3;
    uint8_t ip[4]={192,168,1,42};memcpy(&m.address.addr.u_addr.ip4.addr,ip,4);m.result.addr=&m.address;*out=&m.result;
  }
  return true;
}
inline void mdns_query_results_free(mdns_result_t *){++fakeMdns().resultsFreed;}
inline int mdns_query_async_delete(mdns_search_once_t *){auto &m=fakeMdns();assert(m.ready);m.hasQuery=false;++m.deleted;return ESP_OK;}

inline int mdns_service_txt_item_set(const char *service,const char *,const char *,const char *){return !fakeMdns().delegatedHost.empty()&&(!strcmp(service,"_axio")||!strcmp(service,"_http"))?ESP_ERR_NOT_FOUND:ESP_OK;}
inline int mdns_delegate_hostname_add(const char *host,const mdns_ip_addr_t *){if(fakeMdns().failAlias)return -1;fakeMdns().delegatedHost=host;return ESP_OK;}
inline int mdns_service_add_for_host(const char *instance,const char *service,const char *proto,const char *,uint16_t port,mdns_txt_item_t *txt,size_t count){return mdns_service_add(instance,service,proto,port,txt,count);}
inline int mdns_service_txt_item_set_for_host(const char *instance,const char *,const char *,const char *host,const char *,const char *){assert(fakeMdns().delegatedHost==host&&fakeMdns().delegatedHost==instance);return ESP_OK;}
inline int mdns_service_port_set_for_host(const char *instance,const char *,const char *,const char *host,uint16_t port){auto &m=fakeMdns();assert(m.delegatedHost==host&&m.delegatedHost==instance);if(m.failPort)return -1;m.port=port;++m.portChanges;return ESP_OK;}
