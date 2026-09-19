#include "status_light.h"
#include "menu_input.h"
#include "voice.h"
#include <M5Unified.h>
#include <esp32-hal-rmt.h>
namespace StatusLight {
namespace {
rmt_obj_t* output=nullptr;
MenuInput::LightTimer menuTimer;
uint32_t colour=0,until=0,last[2]={UINT32_MAX,UINT32_MAX};int led=-1;
void write(uint32_t a,uint32_t b){
    if(!output||(last[0]==a&&last[1]==b))return;
    rmt_data_t data[48]{};unsigned n=0;
    for(auto c:{a,b})for(unsigned shift:{8u,16u,0u}){
        uint8_t value=((c>>shift)&255)*40/255; // bounded brightness; GRB order
        for(int bit=7;bit>=0;--bit){bool one=value&(1<<bit);data[n].level0=1;data[n].duration0=one?8:4;data[n].level1=0;data[n++].duration1=one?4:8;}
    }
    if(!rmtWriteBlocking(output,data,48)){output=nullptr;return;}
    delayMicroseconds(300);
    last[0]=a;last[1]=b;
}
}
bool begin(){
    if(M5.getBoard()!=m5::board_t::board_M5PaperColor)return false;
    // M5Unified's pinned IDF4 LED encoder is unimplemented. Use Arduino's
    // working legacy RMT API (same timings as its neopixelWrite), for two LEDs.
    output=rmtInit(21,RMT_TX_MODE,RMT_MEM_64);
    if(!output)return false;
    rmtSetTick(output,100);write(0,0);return true;
}
bool ready(){return output!=nullptr;}
void flash(uint32_t c,uint32_t duration,int index){colour=c;until=millis()+duration;led=index;}
void menuActivity(){menuTimer.touch(millis());}
void tick(int selection){
    if(Voice::active()){write(millis()%1000<140?0x400000:0,0);return;}
    bool lit=menuTimer.visible(selection,millis());
    if(selection>=0&&selection<5){auto c=lit?MenuInput::colours[selection]:0;write(c,c);return;}
    if(until&&int32_t(millis()-until)<0)write(led==1?0:colour,led==0?0:colour);
    else {until=0;write(0,0);}
}
}
