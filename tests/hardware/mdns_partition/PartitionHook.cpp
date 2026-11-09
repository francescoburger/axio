// Link-time receive gate for the native mDNS PCB. No second responder,
// framework patch, radio change or NVS write. Never part of firmware/Axio/.
#include <Arduino.h>
#include <atomic>
#include <mdns.h>
#include <lwip/udp.h>
#include "src/diagnostics/SerialProtocol.h"
#include "PartitionHook.h"

namespace {
std::atomic<uint32_t> deadline{0},dropped{0},forwarded{0},bindings{0};
std::atomic<bool> hold{false},bound{false};
udp_recv_fn receiver=nullptr;void *receiverArg=nullptr;
uint32_t remaining(){
  int32_t left=int32_t(deadline.load()-uint32_t(millis()));
  return hold.load()&&left>0?uint32_t(left):0;
}
void receive(void *,udp_pcb *pcb,pbuf *packet,const ip_addr_t *address,uint16_t port){
  if(remaining()){++dropped;pbuf_free(packet);return;}
  ++forwarded;
  if(receiver)receiver(receiverArg,pcb,packet,address,port);
  else pbuf_free(packet);
}
}
extern "C" void __real_udp_recv(udp_pcb *,udp_recv_fn,void *);
extern "C" void __wrap_udp_recv(udp_pcb *pcb,udp_recv_fn callback,void *arg){
  if(pcb->local_port==5353){
    receiver=callback;receiverArg=arg;bound.store(callback!=nullptr);
    if(callback){++bindings;__real_udp_recv(pcb,receive,nullptr);return;}
  }
  __real_udp_recv(pcb,callback,arg);
}
bool mdnsPartitionCommand(const char *id,char *command,char *out){
  if(strncmp(command,"TEST MDNS ",10))return false;
  if(!strcmp(command,"TEST MDNS HOLD")){
    deadline.store(uint32_t(millis())+60000);hold.store(true);
  }else if(!strcmp(command,"TEST MDNS JOIN"))hold.store(false);
  else if(strcmp(command,"TEST MDNS STATUS")){
    axio::SerialProtocol::reply(out,id,"unknown_test_command");return true;
  }
  char native[65]={},data[320];mdns_hostname_get(native);
  snprintf(data,sizeof(data),"{\"probe\":\"rx_partition_v1\",\"remaining_ms\":%lu,\"dropped\":%lu,\"forwarded\":%lu,\"bindings\":%lu,\"bound\":%s,\"native\":\"%s\"}",
    (unsigned long)remaining(),(unsigned long)dropped.load(),(unsigned long)forwarded.load(),
    (unsigned long)bindings.load(),bound.load()?"true":"false",native);
  axio::SerialProtocol::reply(out,id,nullptr,data);return true;
}
