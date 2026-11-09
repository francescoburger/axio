#pragma once
#include <stdint.h>
#include <string.h>

namespace axio { namespace connectivity {
// Policy only. DNS ownership/probing remains the native responder's job.
// No decision is based on the long-lived discovery cache.
class EntryElection {
 public:
  enum class Action { None, Lookup, Census, Promote, Demote };
  enum class State { Offline, Waiting, Lookup, CensusWait, Census, ConfirmWait, Confirm, Switching, Probing, Leader, Retreat };
 private:
  State state_=State::Offline;
  uint32_t since_=0,delay_=0;
  unsigned misses_=0;
  void wait(uint32_t now,uint32_t delay){state_=State::Waiting;since_=now;delay_=delay;}
 public:
  uint64_t elections=0,conflicts=0,failures=0;
  State state()const{return state_;}
  const char *name()const{
    switch(state_){
      case State::Offline:return "offline";
      case State::Leader:return "leader";
      case State::Probing:return "probing";
      case State::Switching:return "claiming";
      case State::Retreat:return "withdrawing";
      case State::CensusWait:case State::Census:case State::ConfirmWait:case State::Confirm:return "electing";
      default:return "standby";
    }
  }
  bool leader()const{return state_==State::Leader;}
  Action tick(bool online,bool common,const char *actual,uint32_t now){
    if(!online){state_=State::Offline;misses_=0;return common?Action::Demote:Action::None;}
    if(state_==State::Offline){misses_=0;wait(now,3000);}
    if((state_==State::Leader||state_==State::Probing)&&common&&strcmp(actual,"axio")){
      ++conflicts;state_=State::Retreat;
    }
    if(state_==State::Retreat)return Action::Demote;
    if(state_==State::Switching)return Action::Promote;
    if(state_==State::Probing&&uint32_t(now-since_)>=3000)state_=State::Leader;
    if(state_==State::Waiting&&uint32_t(now-since_)>=delay_){state_=State::Lookup;return Action::Lookup;}
    if(state_==State::CensusWait){state_=State::Census;return Action::Census;}
    if(state_==State::ConfirmWait){state_=State::Confirm;return Action::Lookup;}
    return Action::None;
  }
  void lookup(bool occupied,uint32_t now){
    if(state_!=State::Lookup&&state_!=State::Confirm)return;
    if(occupied){misses_=0;wait(now,3000);return;}
    if(state_==State::Confirm){state_=State::Switching;++elections;return;}
    if(++misses_<2)wait(now,500);
    else state_=State::CensusWait;
  }
  void census(bool smallerCandidate,uint32_t now){
    if(state_!=State::Census)return;
    if(smallerCandidate){misses_=0;wait(now,1500);}
    else state_=State::ConfirmWait;
  }
  void promoted(uint32_t now){state_=State::Probing;since_=now;misses_=0;}
  void demoted(uint32_t now){misses_=0;wait(now,5000);}
  void failed(uint32_t now){++failures;misses_=0;wait(now,5000);}
};
}}
