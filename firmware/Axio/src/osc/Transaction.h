#pragma once
#include "Config.h"
namespace axio { namespace osc {
template<class Network,class Journal,class Engine>
const char *apply(const Config &candidate,Network &network,Journal &journal,Engine &engine,bool listening) {
  const char *error=validate(candidate);if(error)return error;
  if(!network.prepare(listening?engine.get().listenPort:0,candidate.listenPort))return "listen_failed";
  if(!journal.save(candidate)){network.cancel();return "persistence_failed";}
  network.activate();engine.activate(candidate);return nullptr;
}
}}
