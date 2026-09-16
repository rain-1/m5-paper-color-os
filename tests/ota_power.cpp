#include "ota_power.h"
#include <cassert>
int main(){
    using OtaPower::allowed;
    assert(allowed(5000,3200,0)); // Stable external input permits charging a low battery.
    assert(allowed(5000,0,-1)); // One independently confirmed source is sufficient.
    assert(allowed(0,3600,30));
    assert(allowed(-1,3800,60)); // USB measurement unavailable, healthy battery.
    assert(!allowed(0,3599,80)); // Percentage alone cannot override low voltage.
    assert(!allowed(0,3800,29));
    assert(!allowed(0,3800,-1));
    assert(!allowed(0,0,100));
    assert(!allowed(4599,3300,0));
    assert(!allowed(5501,3300,0));
    assert(!allowed(65535,65535,100));
    assert(!allowed(0,4351,100));
    assert(!allowed(0,3800,101));
    assert(allowed(4600,0,-1));
    assert(allowed(5500,0,-1));
    // USB unplugged during upload: continue only with a healthy battery.
    assert(allowed(5000,3500,25) && !allowed(0,3500,25));
    assert(allowed(5000,3800,60) && allowed(0,3800,60));
}
