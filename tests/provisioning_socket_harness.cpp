#include <chrono>
#include <thread>
#include <stdio.h>
#include <stdlib.h>
#include "src/connectivity/ProvisioningHttp.h"
#include "src/connectivity/ProvisioningSocket.h"
struct Service {
  const char *update(const char *,const char *,unsigned,bool){return nullptr;}
  bool status(char *out,size_t){strcpy(out,"{\"transport\":\"host_loopback\"}");return true;}
  bool networks(char *out,size_t){strcpy(out,"{\"networks\":[]}");return true;}
};
int main(int argc,char **argv){
  if(argc!=2)return 2;
  axio::connectivity::ProvisioningSocket socket;Service service;
  axio::connectivity::ProvisioningHttp<decltype(socket),Service> http(socket,service);
  http.begin("127.0.0.1","0123456789abcdef0123456789abcdef");
  const uint8_t address[4]={127,0,0,1};if(!socket.open(address,uint16_t(atoi(argv[1]))))return 3;
  puts("READY");fflush(stdout);auto started=std::chrono::steady_clock::now();
  for(;;){
    auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
    if(elapsed>=5000)break;
    http.tick(uint32_t(elapsed));std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  http.close();
}
