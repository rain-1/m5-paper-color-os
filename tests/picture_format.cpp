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
}
