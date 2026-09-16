#include "refresh_palette.h"
#include "refresh_patterns.h"
#include <cassert>
int main(){
    using RefreshPalette::native;
    assert(native(0)==0);assert(native(0xffff)==1);assert(native(0xffe0)==2);
    assert(native(0xf800)==3);assert(native(0x001f)==5);assert(native(0x07e0)==6);
    assert(((native(0xffff)<<4)|native(0))==0x10);
    for(unsigned i=0;i<65536;++i){auto c=native(i);assert(c==0||c==1||c==2||c==3||c==5||c==6);}
    for(unsigned i=0;i<RefreshPatterns::count;++i){assert(RefreshPatterns::index(RefreshPatterns::names[i])==int(i));assert(RefreshPatterns::valid(i*2));assert(RefreshPatterns::valid(i*2+1));}
    assert(RefreshPatterns::index("invalid")==-1);assert(RefreshPatterns::index("")==-1);assert(!RefreshPatterns::valid(14));assert(!RefreshPatterns::valid(UINT32_MAX));
}
