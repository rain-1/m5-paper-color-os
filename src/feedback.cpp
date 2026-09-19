#include "feedback.h"
#include <M5Unified.h>
#include <Preferences.h>
#include "status_light.h"

namespace Feedback {
namespace {
struct Note { uint16_t frequency, duration; };
struct Motif { Note notes[4]; uint8_t count, red, green, blue; };
const Motif motifs[] = {
    {{{523,90},{659,120}},2,0,0,160},           // Connected: gently rising
    {{{659,90},{880,140}},2,0,160,0},           // Saved: rising confirmation
    {{{659,100},{440,100},{330,180}},3,160,0,0},// Error: descending, no alarm loop
    {{{440,70},{440,70}},2,120,80,0},          // Busy: two equal short notes
    {{{523,80},{659,80},{1047,160}},3,0,160,0},// Unlocked: three rising steps
    {{{523,80},{659,80},{784,80},{1047,180}},4,0,160,0},
    {{{880,200}},1,0,0,0}
};
Preferences settings;
QueueHandle_t queue=nullptr;
bool audible=true, storage=false, active=false;
Cue current=Cue::Connected;
uint8_t nextNote=0;
uint32_t nextAt=0;
}
void begin(){
    StatusLight::begin();
    queue=xQueueCreate(1,sizeof(Cue));
    storage=settings.begin("paper-ui",false);
    if(storage)audible=settings.getBool("sounds",true);
    M5.Speaker.setVolume(48);
}
bool enabled(){return audible;}
void setEnabled(bool value){
    audible=value;
    if(!value)M5.Speaker.stop();
    if(storage)settings.putBool("sounds",value);
}
void play(Cue cue){if(queue)xQueueOverwrite(queue,&cue);}
void tick(){
    uint32_t now=millis();
    Cue incoming;
    if(queue && xQueueReceive(queue,&incoming,0)==pdTRUE){
        current=incoming;nextNote=0;nextAt=now;active=true;
        M5.Speaker.stop();
        const auto& motif=motifs[static_cast<unsigned>(current)];
        StatusLight::flash((uint32_t(motif.red)<<16)|(uint32_t(motif.green)<<8)|motif.blue,1000);
    }
    if(active && int32_t(now-nextAt)>=0){
        const auto& motif=motifs[static_cast<unsigned>(current)];
        const auto& note=motif.notes[nextNote++];
        if(audible)M5.Speaker.tone(note.frequency,note.duration,0,true);
        nextAt=now+note.duration+65;
        active=nextNote<motif.count;
    }
}
}
