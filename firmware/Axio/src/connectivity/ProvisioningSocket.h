#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
namespace axio { namespace connectivity {
class ProvisioningSocket {
  int server=-1,client=-1;
  static bool nonblock(int fd){int flags=fcntl(fd,F_GETFL,0);return flags>=0&&fcntl(fd,F_SETFL,flags|O_NONBLOCK)>=0;}
 public:
  bool open(const uint8_t ip[4],uint16_t port=80){
    closeServer();server=::socket(AF_INET,SOCK_STREAM,0);if(server<0)return false;
    int yes=1;setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    sockaddr_in address={};address.sin_family=AF_INET;address.sin_port=htons(port);
    memcpy(&address.sin_addr.s_addr,ip,4);
    if(!nonblock(server)||::bind(server,reinterpret_cast<sockaddr *>(&address),sizeof(address))<0||::listen(server,1)<0){closeServer();return false;}return true;
  }
  bool acceptClient(){
    if(server<0)return false;client=::accept(server,nullptr,nullptr);if(client<0)return false;
    if(!nonblock(client)){closeClient();return false;}
#ifdef SO_NOSIGPIPE
    int yes=1;setsockopt(client,SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof(yes));
#endif
    return true;
  }
  int read(char *out,size_t size){int n=::recv(client,out,size,0);return n>0?n:(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK)?0:-1);}
  int write(const char *data,size_t size){
#ifdef MSG_NOSIGNAL
    int n=::send(client,data,size,MSG_NOSIGNAL);
#else
    int n=::send(client,data,size,0);
#endif
    return n>=0?n:((errno==EAGAIN||errno==EWOULDBLOCK)?0:-1);
  }
  void closeClient(){if(client>=0)::close(client);client=-1;}
  void closeServer(){closeClient();if(server>=0)::close(server);server=-1;}
};
}}
