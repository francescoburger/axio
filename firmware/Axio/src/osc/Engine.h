#pragma once
#include "../core/Sample.h"
#include "Codec.h"
namespace axio { namespace osc {
struct EngineMetrics {
  uint64_t udpAttempts=0, udpQueued=0, messages=0, malformed=0;
  uint64_t messageAttempts[MaxMessages]={}, messageQueued[MaxMessages]={};
  uint64_t schedules[MaxMessages]={};
  uint64_t maxSendUs=0, maxReceiveUs=0, maxApplyUs=0;
};
template<class Network> class Engine {
  Config config;
  Network &network;
  uint64_t last[MaxMessages]={};
  Packet packet;
 public:
  EngineMetrics metrics;
  explicit Engine(Network &n):network(n) {}
  const Config &get() const {return config;}
  void activate(const Config &c) {config=c;memset(last,0,sizeof(last));}
  void resetMetrics() {metrics=EngineMetrics();}
  bool emit(unsigned route,const uint8_t *data,size_t size) {
    ++metrics.udpAttempts;
    bool ok=network.send(config.destinations[route],data,size);
    if(ok)++metrics.udpQueued;
    return ok;
  }
  // Caller passes only a fresh complete snapshot. A repeated sequence is never sent.
  uint64_t sequence=0;
  void tick(const Sample &sample,uint64_t now) {
    if(!sample.valid||sample.sequence==sequence)return;
    sequence=sample.sequence;
    bool due[MaxMessages]={};
    for(unsigned i=0;i<config.messageCount;++i) {
      uint64_t period=config.schema==2 ? uint64_t(config.messages[i].intervalMs)*1000u :
        (1000000u+config.messages[i].hz-1)/config.messages[i].hz;
      // At the core's nominal 100 Hz ceiling, every fresh snapshot is due.
      // Gating completion timestamps again can discard samples on tiny I2C
      // jitter even though their acquisition starts obey the 10 ms scheduler.
      if((config.schema==1 && config.messages[i].hz==100) || now>=last[i]) {
        last[i]=(last[i] && now-last[i]<period)?last[i]+period:now+period;
        due[i]=true;
        ++metrics.schedules[i];
      }
    }
    for(unsigned r=0;r<config.destCount;++r) {
      if(config.schema==2) {
        unsigned count=0;
        for(unsigned i=0;i<config.messageCount;++i)if(due[i])++count;
        if(!count)continue;
        packet.size=0;if(count>1)packet.bundle();
        for(unsigned i=0;i<config.messageCount;++i)if(due[i]) {
          if(count>1)packet.integer(messageSize(config.messages[i],config.id));
          packet.message(config.messages[i],config,sample);
        }
        bool ok=emit(r,packet.data,packet.size);
        for(unsigned i=0;i<config.messageCount;++i)if(due[i]) {
          ++metrics.messageAttempts[i];if(ok)++metrics.messageQueued[i];++metrics.messages;
        }
        continue;
      }
      packet.size=0; bool bundled=false;
      for(unsigned i=0;i<config.messageCount;++i)if(due[i] && config.messages[i].bundle && (config.messages[i].routes&(1u<<r))) {
        if(!bundled) {packet.bundle();bundled=true;}
        packet.integer(messageSize(config.messages[i],config.id));
        packet.message(config.messages[i],config,sample);
      }
      if(bundled) {
        bool ok=emit(r,packet.data,packet.size);
        for(unsigned i=0;i<config.messageCount;++i)if(due[i] && config.messages[i].bundle && (config.messages[i].routes&(1u<<r))) {
          ++metrics.messageAttempts[i];if(ok)++metrics.messageQueued[i];++metrics.messages;
        }
      }
      for(unsigned i=0;i<config.messageCount;++i)if(due[i] && !config.messages[i].bundle && (config.messages[i].routes&(1u<<r))) {
        packet.size=0;packet.message(config.messages[i],config,sample);
        bool ok=emit(r,packet.data,packet.size);
        ++metrics.messageAttempts[i];if(ok)++metrics.messageQueued[i];++metrics.messages;
      }
    }
  }
  bool calibrate() {
    if(!network.calibrate())return false;
    if(config.ackEnabled) {
      char address[AddressSize];expand(config.ackAddress,config.id,address,sizeof(address));
      packet.size=0;packet.string(address);packet.string(",i");packet.integer(1);
      emit(config.ackDestination,packet.data,packet.size);
    }
    return true;
  }
  bool receive(const uint8_t *data,size_t size) {
    char address[AddressSize];expand(config.calibrationAddress,config.id,address,sizeof(address));
    if(!calibrationPacket(data,size,address) &&
       !(calibrationAlias(config,address,sizeof(address))&&calibrationPacket(data,size,address))){++metrics.malformed;return false;}
    return calibrate();
  }
};
}}
