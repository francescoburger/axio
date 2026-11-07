#include <cassert>
#include <string>
#include <algorithm>
#include "src/power/Battery.h"
#include "src/power/SleepButton.h"
#include "src/web/ImageSize.h"
#include "src/web/DashboardHttp.h"
struct Socket {
  std::string input,output;size_t offset=0;bool accepted=false,closed=false;
  bool acceptClient(){if(accepted)return false;accepted=true;return true;}
  int read(char *out,size_t cap){size_t n=std::min(cap,input.size()-offset);input.copy(out,n,offset);offset+=n;return n;}
  int write(const char *data,size_t n){n=std::min(n,size_t(17));output.append(data,n);return n;}
  void closeClient(){closed=true;}void closeServer(){}
};
struct Service {
  struct Result {unsigned code;const char *type,*body;size_t size;bool gzip;};
  unsigned calls=0;
  bool host(const char *p){return !strcmp(p,"192.168.1.15");}
  bool token(const char *p){return !strcmp(p,"test-token");}
  Result handle(bool,const char *,char *){++calls;return {200,"application/json","{\"ok\":true}",11,false};}
};
int main(){
  uint8_t image[128]={};image[0]=0xe9;image[1]=1;image[23]=1;image[28]=16;
  auto read=[&](uint32_t offset,uint8_t *out,size_t n){if(offset+n>sizeof(image))return false;memcpy(out,image+offset,n);return true;};
  assert(axio::imageSize(read,sizeof(image))==96);
  image[28]=255;assert(!axio::imageSize(read,sizeof(image)));
  image[28]=16;image[1]=17;assert(!axio::imageSize(read,sizeof(image)));
  axio::Battery battery;unsigned reads=0;
  auto adc=[&](){++reads;return 1950u;};
  for(unsigned i=0;i<16;++i){battery.tick(i*63,adc);battery.tick(i*63,adc);}
  assert(reads==16 && battery.available && battery.voltage>3.899 && battery.voltage<3.901);
  // Wraparound does not cause a burst of ADC reads.
  axio::Battery wrap;reads=0;wrap.tick(0xfffffff0,adc);wrap.tick(20,adc);assert(reads==1);wrap.tick(47,adc);assert(reads==2);
  // Boot/wake while held never requests sleep, regardless of hold duration.
  axio::SleepButton button;
  assert(!button.tick(0,true,true));assert(!button.tick(5000,true,true));
  assert(!button.tick(5010,false,true));assert(!button.tick(5040,false,true));
  // Short press and contact bounce do not confirm the command.
  assert(!button.tick(5100,true,true));assert(!button.tick(5110,false,true));
  assert(!button.tick(5140,false,true));assert(!button.tick(5200,true,true));
  assert(!button.tick(5230,true,true));assert(!button.tick(8199,false,true));
  assert(!button.tick(8229,false,true)); // 2999 ms, below threshold.
  assert(!button.tick(8300,true,true));assert(!button.tick(8330,true,true));
  assert(!button.tick(11300,true,true)); // Reaching 3 s while held does not sleep.
  assert(!button.tick(11300,false,true));assert(!button.tick(11329,false,true));
  assert(button.tick(11330,false,true));assert(!button.tick(11400,false,true));
  // A gesture spanning a busy/error state is discarded; require a new press.
  assert(!button.tick(12000,true,true));assert(!button.tick(12030,true,true));
  assert(!button.tick(13000,true,false));assert(!button.tick(15000,true,true));
  assert(!button.tick(15100,false,true));assert(!button.tick(15130,false,true));
  assert(!button.tick(15200,true,true));assert(!button.tick(15230,true,true));
  assert(!button.tick(18200,false,true));assert(button.tick(18230,false,true));
  // Unsigned time wrap preserves both debounce and exact long-press threshold.
  axio::SleepButton buttonWrap;
  assert(!buttonWrap.tick(0xffffff00,false,true));assert(!buttonWrap.tick(0xffffff1e,false,true));
  assert(!buttonWrap.tick(0xffffff40,true,true));assert(!buttonWrap.tick(0xffffff5e,true,true));
  assert(!buttonWrap.tick(2808,false,true));assert(buttonWrap.tick(2838,false,true));
  for(unsigned mode=0;mode<6;++mode) {
    Socket socket;Service service;axio::DashboardHttp<Socket,Service> http(socket,service);
    const char *host=mode==1?"evil.test":"192.168.1.15";
    socket.input="GET /api/status HTTP/1.1\r\nHost: "+std::string(host)+"\r\n\r\n";
    if(mode==2)socket.input="POST /api/control HTTP/1.1\r\nHost: 192.168.1.15\r\nContent-Length: 2\r\nContent-Type: application/json\r\nX-Axio-Token: test-token\r\n\r\n{}";
    if(mode==3)socket.input="POST /api/control HTTP/1.1\r\nHost: 192.168.1.15\r\nContent-Length: 2\r\nContent-Type: application/json\r\n\r\n{}";
    if(mode==4)socket.input="GET / HTTP/1.1\r\nHost: 192.168.1.15\r\nOrigin: http://evil.test\r\n\r\n";
    if(mode==5)socket.input="POST /api/control HTTP/1.1\r\nHost: 192.168.1.15\r\nContent-Length: 2\r\nContent-Length: 2\r\nContent-Type: application/json\r\nX-Axio-Token: test-token\r\n\r\n{}";
    for(unsigned t=0;t<1000&&!socket.closed;++t)http.tick(t);
    assert(socket.closed);assert(service.calls==unsigned(mode==0||mode==2));
    assert(socket.output.find(mode==0||mode==2?"200 OK":"Error")!=std::string::npos);
  }
  Socket slow;Service service;axio::DashboardHttp<Socket,Service> http(slow,service);http.tick(0xfffffff0);http.tick(4984);assert(slow.closed && service.calls==0);
}
