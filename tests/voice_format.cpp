#include "voice_format.h"
#include <cassert>
#include <cstdio>
#include <string>
uint32_t get(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
int main(){
    for(uint32_t n:{0u,32000u,VoiceFormat::maxBytes}){
        uint8_t h[44];VoiceFormat::header(h,n);
        assert(!memcmp(h,"RIFF",4)&&!memcmp(h+8,"WAVEfmt ",8)&&!memcmp(h+36,"data",4));
        assert(get(h+4)==n+36&&get(h+40)==n&&get(h+16)==16);
        assert(h[20]==1&&h[22]==1&&get(h+24)==16000&&get(h+28)==32000&&h[32]==2&&h[34]==16);
        assert(VoiceFormat::validHeader(h,n+44)==(n!=0));
        if(n)for(int i=0;i<44;++i){h[i]^=1;assert(!VoiceFormat::validHeader(h,n+44));h[i]^=1;}
        assert(!VoiceFormat::validHeader(h,n+45));
    }
    const std::string valid="/recordings/2026/09/20260919_134500_1234abcd.wav";
    assert(VoiceFormat::path(valid.c_str()));
    assert(!VoiceFormat::path(nullptr));
    for(size_t n=0;n<valid.size();++n)assert(!VoiceFormat::path(valid.substr(0,n).c_str()));
    for(size_t n=0;n<valid.size();++n){auto bad=valid;bad[n]='.';if(valid[n]!='.')assert(!VoiceFormat::path(bad.c_str()));}
    assert(!VoiceFormat::path((valid+".part").c_str()));
    assert(!VoiceFormat::path("/recordings/2026/08/20260919_134500_1234abcd.wav"));
    assert(!VoiceFormat::path("/recordings/2026/02/20260230_134500_1234abcd.wav"));
    puts("Voice WAV format, exact path allowlist, truncation, traversal and date matching passed.");
}
