#pragma once
#include <cstdint>
#include <cstring>

namespace WifiProfiles {
constexpr unsigned capacity=5;
struct Credentials { char ssid[33]; char password[64]; };
struct Entry { Credentials credentials; char name[33]; };
struct Store {
    char magic[4]={'W','I','F','1'};
    uint8_t count=0,last=255,reserved[2]={};
    Entry entries[capacity]{};
};
inline bool validCredentials(const Credentials& c){
    if(!c.ssid[0]||!std::memchr(c.ssid,0,sizeof(c.ssid))||!std::memchr(c.password,0,sizeof(c.password)))return false;
    auto n=std::strlen(c.password);return n==0||(n>=8&&n<=63);
}
inline int find(const Store& s,const char* ssid){
    for(unsigned i=0;i<s.count;++i)if(!std::strcmp(s.entries[i].credentials.ssid,ssid))return i;
    return -1;
}
inline bool valid(const Store& s){
    if(std::memcmp(s.magic,"WIF1",4)||s.count>capacity||s.reserved[0]||s.reserved[1]||(s.last!=255&&s.last>=s.count))return false;
    for(unsigned i=0;i<s.count;++i){
        if(!validCredentials(s.entries[i].credentials)||!std::memchr(s.entries[i].name,0,33))return false;
        for(unsigned j=0;j<i;++j)if(!std::strcmp(s.entries[i].credentials.ssid,s.entries[j].credentials.ssid))return false;
    }
    return true;
}
// Only call after successful connection. Existing SSIDs update in place.
inline bool save(Store& s,const Credentials& c,int replace=-1){
    if(!validCredentials(c))return false;
    int i=find(s,c.ssid);
    if(i<0){
        if(s.count<capacity)i=s.count++;
        else if(replace>=0&&replace<s.count)i=replace;
        else return false;
        s.entries[i]={};
    }
    s.entries[i].credentials=c;s.last=i;return true;
}
inline bool forget(Store& s,unsigned i){
    if(i>=s.count)return false;
    if(s.last==i)s.last=255;else if(s.last!=255&&s.last>i)--s.last;
    for(unsigned j=i+1;j<s.count;++j)s.entries[j-1]=s.entries[j];
    s.entries[--s.count]={};return true;
}
inline int strongest(const Store& s,const int* rssi,uint8_t tried){
    int best=-1;
    for(unsigned i=0;i<s.count;++i)if(!(tried&(1<<i))&&rssi[i]>-1000&&(best<0||rssi[i]>rssi[best]))best=i;
    return best;
}
}
