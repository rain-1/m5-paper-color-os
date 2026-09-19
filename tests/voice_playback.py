"""Actual playback worker, with deterministic SD and speaker ownership fakes."""
from pathlib import Path
import subprocess
import tempfile
worker=Path('src/voice.cpp').read_text().split('void playbackWork(){',1)[1].split('\nvoid playbackTask(',1)[0]
source=r'''
#include "voice_format.h"
#include <atomic>
#include <algorithm>
#include <vector>
#include <deque>
#include <cassert>
#include <cstdio>
enum Scenario{Normal,Stop,BadHeader,Missing,StartFail,ReadFail,QueueFail,Timeout,NoLock};
Scenario scenario;constexpr size_t samples=8192;int16_t b0[samples]{},b1[samples]{};int16_t* buffers[]={b0,b1};
bool owned[2]{},ended=false,success=false;int locks=0,playedCount=0;unsigned now=0;
std::deque<int> queued;std::vector<uint8_t> disk;
std::atomic<bool> stopping{false},overflow{false};std::atomic<uint32_t> bytes{0};std::atomic<const char*> message{""};
int pictureBus=1,filled=1;constexpr int pdTRUE=1;const char* path="note.wav";
struct File {
 size_t pos=0;explicit operator bool()const{return scenario!=Missing;}bool isDirectory(){return false;}size_t size(){return disk.size();}
 size_t read(uint8_t* p,size_t n){
  for(int i=0;i<2;++i)if(p==reinterpret_cast<uint8_t*>(buffers[i]))assert(!owned[i]);
  if(scenario==ReadFail&&pos)return 0;
  assert(pos+n<=disk.size());memcpy(p,disk.data()+pos,n);pos+=n;return n;
 }
};
struct {File open(const char*){return File{};}}SD;
void played(void*,const void*,uint8_t){}
struct Speaker {
 bool begin(){return scenario!=StartFail;}void setVolume(int){}
 void setBufferReleaseCallback(void*,void(*)(void*,const void*,uint8_t)){}
 bool playRaw(int16_t* p,size_t n,uint32_t rate,bool stereo,int repeat,int channel,bool stop){
  assert(rate==16000&&!stereo&&repeat==1&&channel==0&&!stop&&n&&n<=samples);
  if(scenario==QueueFail)return false;
  int i=p==buffers[0]?0:1;assert(!owned[i]);owned[i]=true;queued.push_back(i);return true;
 }
 void end(){ended=true;owned[0]=owned[1]=false;queued.clear();}
};
struct {struct Speaker Speaker;}M5;
int xSemaphoreTake(int,int){if(scenario==NoLock)return 0;++locks;return 1;}void xSemaphoreGive(int){--locks;}
void xQueueReset(int){queued.clear();}int pdMS_TO_TICKS(int n){return n;}
unsigned millis(){return now;}void delay(int n){now+=n;}
int xQueueReceive(int,int* i,int wait){
 now+=wait;if(scenario==Timeout)return 0;assert(!queued.empty());*i=queued.front();queued.pop_front();owned[*i]=false;
 ++playedCount;if(scenario==Stop&&playedCount==2)stopping=true;return 1;
}
void playbackWork(){'''+worker+r'''
int main(){
 for(int s=Normal;s<=NoLock;++s){
  scenario=Scenario(s);locks=playedCount=now=0;ended=success=false;stopping=false;overflow=false;bytes=0;owned[0]=owned[1]=false;queued.clear();
  disk.assign(44+32000*3,0);VoiceFormat::header(disk.data(),disk.size()-44);if(s==BadHeader)disk[24]^=1;
  playbackWork();assert(ended&&locks==0&&!owned[0]&&!owned[1]);
  assert(success==(s==Normal||s==Stop));if(s==Normal)assert(bytes==disk.size()-44&&playedCount==6);
 }
 puts("Playback worker: buffer ownership, complete/stop, WAV validation, SD/speaker/timeout failures and cleanup passed.");
}
'''
with tempfile.TemporaryDirectory(prefix='paper-playback-') as directory:
    exe=str(Path(directory)/'test')
    subprocess.run(['g++','-std=c++11','-fsanitize=address,undefined','-Iinclude','-x','c++','-','-o',exe],input=source,text=True,check=True)
    subprocess.run([exe],check=True)
