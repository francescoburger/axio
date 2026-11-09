#include <Arduino.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <mdns.h>
#include "DiscoveryRuntime.h"
#include "DiscoveryManager.h"
#include "EntryElection.h"
#include "../transport/OscRuntime.h"
namespace {
using namespace axio::connectivity;
char hardware[13]={},hostname[64]={},reported[64]={};
char trackerId[17]={},announcedId[17]={};bool dashboardEnabled=false;
bool identityReady=false;const char *identityError=nullptr;
EntryElection election;
bool commonMode=false,reconfigure=false,commonObserved=false;
uint32_t ownerSeen=0;
uint32_t entryIp=0;
EntryElection::Action entryAction=EntryElection::Action::None;
struct Driver {
  mdns_search_once_t *search=nullptr;esp_netif_t *sta=nullptr;
  mdns_search_once_t *entrySearch=nullptr;bool censusQuery=false;
  char actualName[65]={};uint64_t entryQueries=0;
  uint64_t announcements=0,announceFailures=0;
  uint64_t resultRecords=0,emptyResults=0,incompleteResults=0,acceptedResults=0;
  bool initialized=false;const char *error=nullptr;
  uint32_t checkedName=0;uint64_t nameChanges=0,filtered=0;
  esp_err_t txt(const char *service,const char *proto,const char *key,const char *value){
    return commonMode?mdns_service_txt_item_set_for_host(hostname,service,proto,hostname,key,value):mdns_service_txt_item_set(service,proto,key,value);
  }
  bool pending()const{return search!=nullptr||entrySearch!=nullptr;}
  void announce(){
    if(sta&&mdns_netif_action(sta,MDNS_EVENT_ANNOUNCE_IP4)==ESP_OK)++announcements;
    else {++announceFailures;error="mdns_announce_failed";}
  }
  bool start(uint16_t port){
    if(mdns_init()!=ESP_OK){error="mdns_init_failed";return false;}
    initialized=true;sta=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    auto ap=esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    auto removed=ap?mdns_unregister_netif(ap):ESP_OK;
    mdns_txt_item_t txt[]={{"model","Axio"},{"schema","1"},{"hw",hardware}};
    if(!sta||(removed!=ESP_OK&&removed!=ESP_ERR_NOT_FOUND)||mdns_hostname_set(commonMode?"axio":hostname)!=ESP_OK||
       mdns_instance_name_set(hostname)!=ESP_OK){
      error="mdns_service_failed";stop();return false;
    }
    if(commonMode){
      mdns_ip_addr_t address={};address.addr.type=ESP_IPADDR_TYPE_V4;address.addr.u_addr.ip4.addr=uint32_t(WiFi.localIP());
      if(mdns_delegate_hostname_add(hostname,&address)!=ESP_OK||mdns_service_add_for_host(hostname,"_axio","_udp",hostname,port,txt,3)!=ESP_OK){error="mdns_alias_failed";stop();return false;}
    }else if(mdns_service_add(nullptr,"_axio","_udp",port,txt,3)!=ESP_OK){error="mdns_service_failed";stop();return false;}
    if(dashboardEnabled){
      mdns_txt_item_t item={"id",trackerId};
      if(this->txt("_axio","_udp",item.key,item.value)!=ESP_OK||
         (commonMode?mdns_service_add_for_host(hostname,"_http","_tcp",hostname,80,&item,1):mdns_service_add(nullptr,"_http","_tcp",80,&item,1))!=ESP_OK||
         mdns_service_add(nullptr,"_axio-entry","_udp",port,txt,3)!=ESP_OK){error="mdns_http_failed";stop();return false;}
      strcpy(announcedId,trackerId);
    }
    if(mdns_netif_action(sta,MDNS_EVENT_ENABLE_IP4)!=ESP_OK){error="mdns_sta_enable_failed";stop();return false;}
    memcpy(reported,hostname,sizeof(reported));strcpy(actualName,commonMode?"axio":hostname);
    checkedName=uint32_t(millis());if(commonMode)election.promoted(checkedName);error=nullptr;return true;
  }
  bool port(uint16_t value){
    auto result=commonMode?mdns_service_port_set_for_host(hostname,"_axio","_udp",hostname,value):mdns_service_port_set("_axio","_udp",value);
    if(result==ESP_OK&&(!dashboardEnabled||mdns_service_port_set("_axio-entry","_udp",value)==ESP_OK)){error=nullptr;return true;}error="mdns_port_failed";return false;
  }
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
  bool queryEntry(bool census){
    censusQuery=census;
    entrySearch=mdns_query_async_new(census?nullptr:"axio",census?"_axio-entry":nullptr,census?"_udp":nullptr,census?MDNS_TYPE_PTR:MDNS_TYPE_A,census?1200:600,MaxPeers,nullptr);
    if(entrySearch){++entryQueries;return true;}error="entry_query_failed";return false;
  }
  bool metadata(const mdns_result_t &r,char hw[13],char id[17]){
    if(!r.txt||!r.txt_value_len||r.txt_count>16)return false;
    unsigned model=0,schema=0,ids=0;
    for(size_t i=0;i<r.txt_count;++i){
      const auto &t=r.txt[i];if(!t.key||!t.value)continue;
      if(!strcmp(t.key,"model")){if(r.txt_value_len[i]!=4||memcmp(t.value,"Axio",4))return false;++model;}
      if(!strcmp(t.key,"schema")){if(r.txt_value_len[i]!=1||t.value[0]!='1')return false;++schema;}
      if(!strcmp(t.key,"id")){
        size_t len=r.txt_value_len[i];if(!len||len>16||*id)return false;
        for(size_t j=0;j<len;++j){char c=t.value[j];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'))return false;}
        memcpy(id,t.value,len);id[len]=0;
      }
      if(!strcmp(t.key,"hw")){if(r.txt_value_len[i]!=12)return false;memcpy(hw,t.value,12);hw[12]=0;++ids;}
    }
    return model==1&&schema==1&&ids==1&&hardwareId(hw);
  }
  bool poll(PeerCache &cache,bool accept,uint32_t now){
    if(entrySearch){
      mdns_result_t *results=nullptr;
      if(!mdns_query_async_get_results(entrySearch,0,&results,nullptr))return false;
      bool found=false;unsigned visited=0;
      if(accept)for(auto r=results;r&&visited<MaxPeers;r=r->next,++visited){
        if(r->esp_netif!=sta||r->ip_protocol!=MDNS_IP_PROTOCOL_V4||!r->ttl)continue;
        if(censusQuery){
          char hw[13]={},id[17]={};
          if(r->service_type&&r->proto&&!strcmp(r->service_type,"_axio-entry")&&!strcmp(r->proto,"_udp")&&metadata(*r,hw,id)&&strcmp(hw,hardware)<0)found=true;
        }else if(r->hostname&&!strcasecmp(r->hostname,"axio")){
          unsigned addresses=0;for(auto a=r->addr;a&&addresses<8;a=a->next,++addresses)
            if(a->addr.type==ESP_IPADDR_TYPE_V4&&a->addr.u_addr.ip4.addr&&a->addr.u_addr.ip4.addr!=uint32_t(WiFi.localIP()))found=true;
        }
      }
      if(results)mdns_query_results_free(results);
      if(mdns_query_async_delete(entrySearch)!=ESP_OK){error="entry_query_delete_failed";return false;}
      entrySearch=nullptr;
      if(accept){if(censusQuery)election.census(found,now);else {election.lookup(found,now);commonObserved=found;if(found)ownerSeen=now;}}
      return false; // Only the ordinary discovery query increments completed.
    }
    if(!search)return false;
    mdns_result_t *results=nullptr;
    // A zero wait budget is essential: queryService/query_ptr are synchronous.
    if(!mdns_query_async_get_results(search,0,&results,nullptr))return false;
    if(accept){unsigned visited=0;
      if(!results)++emptyResults;
      for(auto r=results;r&&visited<MaxPeers;r=r->next,++visited){
        ++resultRecords;
        if(!r->hostname||!r->service_type||!r->proto||!r->txt||!r->txt_value_len||!r->addr)++incompleteResults;
        char hw[13]={},id[17]={};
        if(r->esp_netif!=sta||r->ip_protocol!=MDNS_IP_PROTOCOL_V4||!r->hostname||!r->service_type||!r->proto||
           strcmp(r->service_type,"_axio")||strcmp(r->proto,"_udp")||!metadata(*r,hw,id)){++filtered;continue;}
        unsigned addresses=0;
        for(auto a=r->addr;a&&addresses<8;a=a->next,++addresses){
          if(a->addr.type!=ESP_IPADDR_TYPE_V4)continue;
          uint8_t ip[4];memcpy(ip,&a->addr.u_addr.ip4.addr,4);
          if(a->addr.u_addr.ip4.addr==uint32_t(WiFi.localIP()))continue;
          if(cache.observe(hw,r->hostname,ip,r->port,r->ttl,now,hardware)){cache.trackerId(hw,id);++acceptedResults;break;}
        }
      }
    }
    if(results)mdns_query_results_free(results);
    if(mdns_query_async_delete(search)!=ESP_OK){error="mdns_query_delete_failed";return false;}
    search=nullptr;error=nullptr;return true;
  }
  void updateName(uint32_t now){
    if(uint32_t(now-checkedName)<100)return;checkedName=now;char actual[65]={};
    if(mdns_hostname_get(actual)!=ESP_OK)return;
    snprintf(actualName,sizeof(actualName),"%s",actual);
    if(!commonMode&&hostLabel(actual,hardware)&&strcmp(actual,reported)){
      memcpy(reported,actual,strlen(actual)+1);++nameChanges;
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
  uint32_t now=uint32_t(millis());
  uint32_t currentIp=uint32_t(WiFi.localIP());
  if(!allowed||currentIp!=entryIp){election.tick(false,commonMode,"",now);commonMode=false;commonObserved=false;entryAction=EntryElection::Action::None;reconfigure=false;}
  entryIp=allowed?currentIp:0;
  manager.tick(allowed&&!reconfigure,uint32_t(WiFi.localIP()),oscListenPort(),now);
  if(reconfigure&&!manager.running)reconfigure=false;
  if(manager.active){
    driver.updateName(now);
    if(dashboardEnabled){
      auto action=election.tick(true,commonMode,driver.actualName,now);
      if(action!=EntryElection::Action::None)entryAction=action;
      if(!driver.pending()&&entryAction!=EntryElection::Action::None){
        action=entryAction;entryAction=EntryElection::Action::None;
        if(action==EntryElection::Action::Lookup||action==EntryElection::Action::Census){
          if(!driver.queryEntry(action==EntryElection::Action::Census))election.failed(now);
        }else if(action==EntryElection::Action::Announce){driver.announce();
        }else{
          commonMode=action==EntryElection::Action::Promote;reconfigure=true;
          if(!commonMode)election.demoted(now);
        }
      }
    }
    if(dashboardEnabled&&strcmp(announcedId,trackerId)){
      if(driver.txt("_axio","_udp","id",trackerId)==ESP_OK&&driver.txt("_http","_tcp","id",trackerId)==ESP_OK)strcpy(announcedId,trackerId);
    }
  }
}
const char *discoveryHostname(){return identityReady?reported:"";}
const char *discoveryState(){return !identityReady?"identity_error":manager.draining?"draining":!manager.active?"offline":driver.pending()?"querying":"ready";}
const char *discoveryEntryState(){return election.name();}
bool discoveryEntryOwned(){return manager.active&&commonMode&&!strcmp(driver.actualName,"axio")&&election.leader();}
bool discoveryEntryAvailable(){return discoveryEntryOwned()||(manager.active&&commonObserved&&uint32_t(uint32_t(millis())-ownerSeen)<7000);}
void discoveryJson(char *out,size_t cap){
  snprintf(out,cap,"{\"state\":\"%s\",\"active\":%s,\"service\":\"_axio._udp\",\"service_port\":%u,\"peers\":%u,\"query_pending\":%s,\"starts\":%llu,\"failures\":%llu,\"queries\":%llu,\"completed\":%llu,\"query_failures\":%llu,\"filtered\":%llu,\"rejected\":%llu,\"overflow\":%llu,\"name_changes\":%llu,\"last_transition\":\"%s\",\"error\":\"%s\"}",
    discoveryState(),manager.active?"true":"false",oscListenPort(),manager.cache.count(),driver.pending()?"true":"false",
    (unsigned long long)manager.starts,(unsigned long long)manager.failures,(unsigned long long)manager.queries,(unsigned long long)manager.completed,
    (unsigned long long)manager.queryFailures,(unsigned long long)driver.filtered,(unsigned long long)manager.cache.rejected,(unsigned long long)manager.cache.overflow,
    (unsigned long long)driver.nameChanges,manager.reason,identityError?identityError:(driver.error?driver.error:""));
  size_t n=strlen(out);
  if(n&&n<cap)snprintf(out+n-1,cap-n+1,",\"entry_state\":\"%s\",\"entry_owned\":%s,\"entry_queries\":%llu,\"entry_conflicts\":%llu,\"entry_failures\":%llu,\"announcements\":%llu,\"announce_failures\":%llu,\"result_records\":%llu,\"empty_results\":%llu,\"incomplete_results\":%llu,\"accepted_results\":%llu}",election.name(),discoveryEntryOwned()?"true":"false",(unsigned long long)driver.entryQueries,(unsigned long long)election.conflicts,(unsigned long long)election.failures,(unsigned long long)driver.announcements,(unsigned long long)driver.announceFailures,(unsigned long long)driver.resultRecords,(unsigned long long)driver.emptyResults,(unsigned long long)driver.incompleteResults,(unsigned long long)driver.acceptedResults);
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

void setDiscoveryTrackerId(const char *id){snprintf(trackerId,sizeof(trackerId),"%s",id);dashboardEnabled=true;}
const char *discoveryHardware(){return hardware;}
bool stopDiscoveryRuntime(){tickDiscovery(false);return !manager.running;}
bool discoveryPeersJson(char *out,size_t cap){
  manager.cache.expire(uint32_t(millis()));size_t used=0;bool comma=false;
  if(!cap)return false;out[used++]='[';
  for(unsigned i=0;i<MaxPeers;++i){const auto &p=manager.cache.get(i);if(!p.port)continue;
    if(used>=cap)return false;
    int n=snprintf(out+used,cap-used,"%s{\"id\":\"%s\",\"hw\":\"%s\",\"ip\":\"%u.%u.%u.%u\"}",comma?",":"",*p.id?p.id:p.hw,p.hw,p.ip[0],p.ip[1],p.ip[2],p.ip[3]);
    if(n<0||size_t(n)>=cap-used)return false;used+=n;comma=true;
  }
  if(used+2>cap)return false;strcpy(out+used,"]");return true;
}
