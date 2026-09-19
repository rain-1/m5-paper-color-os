#pragma once
#include "picture_format.h"
namespace VoiceFormat {
constexpr uint32_t rate=16000, maxBytes=rate*2*300;
inline void put32(uint8_t* p,uint32_t v){for(int i=0;i<4;++i)p[i]=v>>(i*8);}
inline void header(uint8_t* h,uint32_t bytes){
    memset(h,0,44);memcpy(h,"RIFF",4);put32(h+4,bytes+36);memcpy(h+8,"WAVEfmt ",8);
    put32(h+16,16);h[20]=1;h[22]=1;put32(h+24,rate);put32(h+28,rate*2);
    h[32]=2;h[34]=16;memcpy(h+36,"data",4);put32(h+40,bytes);
}
inline bool path(const char* s){
    // /recordings/YYYY/MM/YYYYMMDD_HHMMSS_12345678.wav
    if(!s||strlen(s)!=48||strncmp(s,"/recordings/",12)||s[16]!='/'||s[19]!='/'||s[35]!='_'||strcmp(s+44,".wav"))return false;
    char stamp[16];memcpy(stamp,s+20,15);stamp[15]=0;
    if(!picture::date(stamp)||memcmp(s+12,stamp,4)||memcmp(s+17,stamp+4,2))return false;
    for(int i=36;i<44;++i)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;
    return true;
}
inline bool validHeader(const uint8_t* h,size_t size){
    if(size<46||size>maxBytes+44||(size-44)%2)return false;
    uint8_t expected[44];header(expected,size-44);return !memcmp(h,expected,44);
}
}
