#include "wifi_profiles.h"
#include <cassert>
#include <cstdio>
using namespace WifiProfiles;
int main(){
    Store s;assert(valid(s));assert(s.count==0&&s.last==255);
    Credentials legacy{};std::strcpy(legacy.ssid,"home");std::strcpy(legacy.password,"password");
    static_assert(sizeof(Credentials)==97,"Legacy NVS layout must be preserved");
    assert(save(s,legacy));assert(s.count==1&&s.last==0&&valid(s));
    Store reloaded;std::memcpy(&reloaded,&s,sizeof(s));assert(valid(reloaded));
    for(unsigned i=1;i<5;++i){Credentials c{};std::snprintf(c.ssid,33,"network%u",i);assert(save(s,c));}
    assert(s.count==5&&s.last==4&&valid(s));
    Credentials extra{};std::strcpy(extra.ssid,"sixth");
    Store before=s;assert(!save(s,extra));assert(!std::memcmp(&s,&before,sizeof(s)));
    assert(!save(s,extra,5));assert(!std::memcmp(&s,&before,sizeof(s)));
    std::strcpy(s.entries[0].name,"Home label");
    std::strcpy(legacy.password,"updated-password");assert(save(s,legacy));
    assert(s.count==5&&s.last==0&&!std::strcmp(s.entries[0].name,"Home label"));
    int rssi[]={-70,-50,-30,-1000,-40};
    assert(strongest(s,rssi,1)==2);assert(strongest(s,rssi,1|4)==4);
    assert(strongest(s,rssi,31)==-1);
    int none[]={-1000,-1000,-1000,-1000,-1000};assert(strongest(s,none,0)==-1);
    assert(save(s,extra,2));assert(s.count==5&&s.last==2&&!s.entries[2].name[0]);
    assert(forget(s,0));assert(s.last==1&&s.count==4&&valid(s));
    assert(forget(s,1));assert(s.last==255&&valid(s));
    while(s.count)assert(forget(s,0));assert(valid(s));assert(!forget(s,0));
    Store bad;bad.count=6;assert(!valid(bad));bad=Store{};bad.last=0;assert(!valid(bad));
    bad=Store{};bad.magic[0]='X';assert(!valid(bad));
    bad=before;bad.entries[1]=bad.entries[0];assert(!valid(bad));
    bad=before;std::memset(bad.entries[0].credentials.password,'x',64);assert(!valid(bad));
    Credentials invalid=legacy;std::strcpy(invalid.password,"short");assert(!save(s,invalid));
    std::memset(invalid.ssid,'x',33);assert(!validCredentials(invalid));
    puts("Wi-Fi profiles: migration layout, five-slot capacity, explicit replacement, updates, deletion, validation and signal ranking passed.");
}
