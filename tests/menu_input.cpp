#include "menu_input.h"
#include <cassert>
#include <cstdio>
using namespace MenuInput;
int main(){
    LightTimer light;
    assert(light.visible(0,100));assert(light.visible(0,30099));assert(!light.visible(0,30100));
    light.touch(31000);assert(light.visible(0,31000)); // wake same choice, no navigation
    assert(light.visible(1,60000));assert(!light.visible(1,90000));
    assert(!light.visible(-1,90001));assert(light.visible(1,90002));
    light.touch(UINT32_MAX-100);assert(light.visible(1,100));assert(!light.visible(1,30000));
    Clicks c;
    assert(c.release(0,100)==Event::None);
    assert(c.tick(450)==Event::None);assert(c.tick(451)==Event::Up);
    assert(c.release(0,1000)==Event::None);assert(c.release(0,1200)==Event::Choose);
    assert(c.tick(2000)==Event::None); // no phantom single after double
    assert(c.release(1,2100)==Event::None);assert(c.release(1,2400)==Event::Back);
    assert(c.release(0,3000)==Event::None);assert(c.release(1,3100)==Event::Up);
    assert(c.tick(3451)==Event::Down);
    assert(c.release(1,4000)==Event::None);c.clear();assert(c.tick(5000)==Event::None);
    assert(c.release(0,UINT32_MAX-100)==Event::None);assert(c.release(0,100)==Event::Choose);
    assert(c.release(1,UINT32_MAX-100)==Event::None);assert(c.tick(300)==Event::Down);
    assert(move(0,5,false)==4);assert(move(4,5,true)==0);assert(move(0,0,true)==0);
    for(unsigned rows=0;rows<=64;++rows){
        unsigned pages=rows>5?(rows+3)/4:1,visited=0;
        for(unsigned p=0;p<pages;++p){unsigned n=count(rows,p);assert(n<=5);visited+=n-(rows>5?1:0);}
        assert(visited==rows);
    }
    puts("Menu input: delayed singles, double choose/back, cancellation, timer wrap, navigation and pagination passed.");
}
