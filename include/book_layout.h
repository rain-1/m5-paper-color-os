#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace BookLayout {
constexpr size_t maxBytes = 1024 * 1024;
constexpr size_t maxPages = 16000;
struct Line { uint32_t begin, end; };
struct Metrics { uint16_t widths[128]{}; uint16_t width=352, rows=15; };

inline bool validText(const char* text, size_t size) {
    if (!size || size > maxBytes) return false;
    bool printable=false;
    for(size_t i=0;i<size;++i) {
        unsigned char c=text[i];
        if(c!='\n' && (c<32 || c>126)) return false;
        if(c>32)printable=true;
    }
    return printable;
}

// Byte offsets are stable across font changes. Every iteration consumes input,
// including a word wider than the entire screen and runs of whitespace.
inline uint32_t page(const char* text, size_t size, uint32_t offset,
                     const Metrics& m, std::vector<Line>* lines=nullptr) {
    uint32_t p=std::min<size_t>(offset,size);
    for(unsigned row=0;row<m.rows && p<size;++row) {
        while(p<size && text[p]==' ')++p;
        if(p==size)break;
        uint32_t start=p, j=p, lastSpace=UINT32_MAX;
        unsigned width=0;
        while(j<size && text[j]!='\n') {
            unsigned char c=text[j];
            unsigned advance=c<128?m.widths[c]:m.widths['?'];
            if(width+advance>m.width)break;
            width+=advance;
            if(c==' ')lastSpace=j;
            ++j;
        }
        uint32_t end=j;
        if(j==size)p=j;
        else if(text[j]=='\n')p=j+1;
        else if(lastSpace!=UINT32_MAX){end=lastSpace;p=lastSpace+1;}
        else {if(j==start)++j;end=j;p=j;}
        while(end>start && text[end-1]==' ')--end;
        if(lines)lines->push_back({start,end});
    }
    return p;
}
inline size_t pageAt(const std::vector<uint32_t>& starts,uint32_t offset) {
    if(starts.empty())return 0;
    auto it=std::upper_bound(starts.begin(),starts.end(),offset);
    return it==starts.begin()?0:size_t(it-starts.begin()-1);
}
inline bool validId(const char* id) {
    if(!id || std::strlen(id)!=8)return false;
    for(int i=0;i<8;++i)if(!((id[i]>='0'&&id[i]<='9')||(id[i]>='a'&&id[i]<='f')))return false;
    return id[8]==0;
}
}
