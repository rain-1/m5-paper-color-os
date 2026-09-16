#pragma once
#include <cstdint>
#include <cstring>
namespace RefreshPatterns {
constexpr const char* names[]={"chart","serif","sans","inverse","checker","colour","white"};
constexpr unsigned count=sizeof(names)/sizeof(names[0]);
inline int index(const char* name){for(unsigned i=0;i<count;++i)if(!std::strcmp(name,names[i]))return i;return -1;}
inline bool valid(unsigned value){return value<count*2;}
}
