#include "picture_format.h"
#include <cassert>
#include <vector>
int main(){
    std::vector<uint8_t> image(picture::fileSize,0x15);
    memcpy(image.data(),picture::header,picture::headerSize);
    assert(picture::valid(image.data(),image.size()));
    assert(!picture::valid(image.data(),image.size()-1));
    image.back()=0x16;assert(!picture::valid(image.data(),image.size()));
    image.back()=0x15;image[4]=0;assert(!picture::valid(image.data(),image.size()));
    assert(picture::date("20260916_143025"));
    assert(picture::date("20240229_235959"));
    assert(!picture::date("20260229_235959"));
    assert(!picture::date("20260931_143025"));
    assert(!picture::date("20260916_246025"));
    assert(picture::month("2026-09"));assert(!picture::month("2026-13"));
    assert(picture::path("/pictures/2026/09/20260916_143025_deadbeef.p6"));
    assert(picture::path("/pictures/2026/09/20260916_143025_p_deadbeef.p6"));
    assert(picture::path("/pictures/2026/09/20260916_143025_l_deadbeef.p6"));
    assert(!picture::path("/pictures/2026/09/20260916_143025_x_deadbeef.p6"));
    assert(!picture::path("/pictures/2026/09/20260916_143025_l_deadbeeG.p6"));
    assert(!picture::path("/pictures/2026/08/20260916_143025_l_deadbeef.p6"));
    assert(!picture::path("/pictures/2026/09/20260916_143025_l_deadbeef.p6.tmp"));
    assert(picture::orientation("p")&&picture::orientation("l"));
    for(auto s:{"", "ll", "L", "../"})assert(!picture::orientation(s));
    assert(!picture::path("/pictures/2026/08/20260916_143025_deadbeef.p6"));
    assert(!picture::path("/pictures/../../secret"));
    memcpy(image.data(),picture::header,picture::headerSize);
    for(size_t i=0;i<picture::width*picture::height;i+=2)image[16+i/2]=((i%6)<<4)|((i+1)%6);
    assert(picture::valid(image.data(),image.size()));
    std::vector<uint8_t> thumb(picture::thumbSize);
    picture::thumbnail(image.data(),thumb.data());
    assert(thumb.size()==4816 && !memcmp(thumb.data(),"P6T1",4) && thumb[4]==80 && thumb[6]==120);
    for(size_t y=0;y<120;++y)for(size_t x=0;x<80;++x){size_t i=y*80+x;auto c=(i&1)?thumb[16+i/2]&15:thumb[16+i/2]>>4;assert(c==((y*5+2)*400+x*5+2)%6);}
}
