#pragma once
#include <stdint.h>
#include <stddef.h>
namespace axio {
// Length of the ESP image framing, not a signature/hash verification. Reads only
// the image and segment headers; avoids esp_image_verify and its scratch buffers.
template<class Read> uint32_t imageSize(Read read,uint32_t capacity){
  uint8_t header[24];if(capacity<24||!read(0,header,sizeof(header))||header[0]!=0xe9||!header[1]||header[1]>16||header[23]>1)return 0;
  uint32_t offset=sizeof(header);
  for(unsigned i=0;i<header[1];++i){
    uint8_t segment[8];if(offset>capacity-8||!read(offset,segment,sizeof(segment)))return 0;
    uint32_t length=uint32_t(segment[4])|uint32_t(segment[5])<<8|uint32_t(segment[6])<<16|uint32_t(segment[7])<<24;
    offset+=8;if(length>capacity-offset)return 0;offset+=length;
  }
  if(offset>capacity-16)return 0;
  offset=(offset+16)&~uint32_t(15);
  if(header[23]){if(offset>capacity-32)return 0;offset+=32;}
  return offset<=capacity?offset:0;
}
}
