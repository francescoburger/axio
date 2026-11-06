#include <assert.h>
#include <iostream>
#include <iterator>
#include <string>
#include "src/connectivity/ProvisioningHttp.h"
#include "src/connectivity/ConnectivityManager.h"
struct Socket {
  std::string input,result;size_t offset=0;bool accepted=false,closed=false,stallRx=false,stallTx=false;
  bool acceptClient(){if(accepted)return false;accepted=true;return true;}
  int read(char *out,size_t n){if(stallRx)return 0;size_t count=input.size()-offset;if(count>n)count=n;if(count>13)count=13;memcpy(out,input.data()+offset,count);offset+=count;return int(count);}
  int write(const char *p,size_t n){if(stallTx)return 0;if(n>17)n=17;result.append(p,n);return int(n);}
  void closeClient(){if(accepted)closed=true;}void closeServer(){}
};
struct Service {
  unsigned updates=0;bool fail=false;
  const char *update(const char *ssid,const char *password,unsigned priority,bool){
    axio::connectivity::Config c;c.count=1;
    if(strlen(ssid)>32||strlen(password)>64)return "invalid_network";
    strcpy(c.networks[0].ssid,ssid);strcpy(c.networks[0].password,password);c.networks[0].priority=uint8_t(priority);
    if(auto error=axio::connectivity::validate(c))return error;
    if(fail)return "persistence_failed";++updates;return nullptr;
  }
  bool status(char *out,size_t){strcpy(out,"{\"ap_active\":true}");return true;}
  bool networks(char *out,size_t){strcpy(out,"{\"networks\":[]}");return true;}
};
int main(int argc,char **argv){
  Socket socket;Service service;socket.input=std::string(std::istreambuf_iterator<char>(std::cin),{});
  if(argc>1){std::string option=argv[1];socket.stallRx=option=="--stall-rx";socket.stallTx=option=="--stall-tx";service.fail=option=="--fail-store";}
  axio::connectivity::ProvisioningHttp<Socket,Service> http(socket,service);
  http.begin("192.168.4.1","0123456789abcdef0123456789abcdef");
  uint32_t start=0xffffff00;for(unsigned i=0;i<3100&&!socket.closed;++i)http.tick(start+i);
  assert(socket.closed);http.close();std::cout<<socket.result;std::cerr<<"updates="<<service.updates;
}
