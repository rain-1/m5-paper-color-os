#pragma once
#include <cstdint>
namespace MenuInput {
enum class Event { None, Up, Down, Choose, Back };
// Delay singles so the first tap of a double-tap never moves the selection.
struct Clicks {
    int pending=-1;uint32_t at=0;
    void clear(){pending=-1;}
    Event single(int button){return button?Event::Down:Event::Up;}
    Event release(int button,uint32_t now){
        if(pending==button&&uint32_t(now-at)<=350){clear();return button?Event::Back:Event::Choose;}
        Event prior=pending<0?Event::None:single(pending);
        pending=button;at=now;return prior;
    }
    Event tick(uint32_t now){if(pending<0||uint32_t(now-at)<=350)return Event::None;auto e=single(pending);clear();return e;}
};
struct LightTimer {
    int selected=-1;uint32_t touched=0;
    void touch(uint32_t now){touched=now;}
    bool visible(int selection,uint32_t now){
        if(selection<0){selected=-1;return false;}
        if(selection!=selected){selected=selection;touch(now);}
        return uint32_t(now-touched)<30000;
    }
};
constexpr uint32_t colours[]={0xff0000,0xffff00,0x00ff00,0x0000ff,0xffffff};
constexpr const char* names[]={"Red","Yellow","Green","Blue","White"};
inline unsigned count(unsigned rows,unsigned page){return rows>5?(rows-page*4<4?rows-page*4:4)+1:rows;}
inline unsigned move(unsigned selected,unsigned count,bool down){return count?(selected+count+(down?1:-1))%count:0;}
}
