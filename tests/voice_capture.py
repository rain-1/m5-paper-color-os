"""Exercise the actual recording worker using deterministic microphone/SD fakes."""
from pathlib import Path
import subprocess
import tempfile

worker=Path('src/voice.cpp').read_text().split('void captureWork(){',1)[1].split('\nvoid played(',1)[0]
source=r'''
#include "voice_format.h"
#include <atomic>
#include <algorithm>
#include <vector>
#include <string>
#include <cassert>
#include <cstdio>
struct String:std::string {
 using std::string::string;String(const std::string& s):std::string(s){}
 String substring(size_t a,size_t b)const{return substr(a,b-a);}
};
enum Scenario {Normal,Limit,NoLock,NoCard,Full,CreateFail,MicFail,QueueFail,Timeout,Overrun,WriteFail,HeaderFail,RenameFail};
Scenario scenario;int chunks=0,recordCalls=0,locks=0;bool micEnded=false,renamed=false,closed=false,railOff=false;
std::vector<uint8_t> disk;
struct File {
 bool opened=false;size_t pos=0;
 explicit operator bool()const{return opened;}
 size_t write(const uint8_t* p,size_t n){
   if((scenario==WriteFail&&n>44)||(scenario==HeaderFail&&chunks&&n==44))return 0;
   disk.resize(std::max(disk.size(),pos+n));memcpy(disk.data()+pos,p,n);pos+=n;return n;
 }
 bool seek(size_t p){pos=p;return true;}void flush(){}size_t size(){return disk.size();}void close(){opened=false;closed=true;}
};
struct SDClass {
 int cardType(){return scenario==NoCard?0:1;}
 uint64_t totalBytes(){return 100000000;}uint64_t usedBytes(){return scenario==Full?100000000:0;}
 bool exists(const String&){return false;}bool mkdir(const String&){return scenario!=CreateFail;}
 File open(const String&,int){File f;f.opened=true;return f;}
 bool rename(const String&,const String&){renamed=scenario!=RenameFail;return renamed;}
}SD;
constexpr size_t samples=8192;
int16_t sample0[samples]{},sample1[samples]{};int16_t* buffers[]={sample0,sample1};
int filled=1,pictureBus=1;constexpr int pdTRUE=1,FILE_WRITE=1,CARD_NONE=0,LOW=0;
std::atomic<bool> stopping{false},finished{false},overflow{false};
std::atomic<uint32_t> bytes{0},peak{0};std::atomic<const char*> message{""};
String path="/recordings/2026/09/20260919_134500_1234abcd.wav";bool success=false;
void released(void*,void*,size_t){}
struct Mic {
 bool begin(){return scenario!=MicFail;}void end(){micEnded=true;}
 void setBufferReleaseCallback(void*,void(*)(void*,void*,size_t)){}
 bool record(int16_t*,size_t){++recordCalls;return scenario!=QueueFail;}
};
struct {struct Mic Mic;}M5;
int xSemaphoreTake(int,int){if(scenario==NoLock)return 0;++locks;return 1;}
void xSemaphoreGive(int){--locks;}void xQueueReset(int){}
int pdMS_TO_TICKS(int n){return n;}void digitalWrite(int,int){railOff=true;}void vTaskDelete(void*){}
int xQueueReceive(int,int* index,int){
 if(scenario==Timeout)return 0;
 *index=chunks++%2;if(scenario!=Limit&&chunks==3)stopping=true;
 if(scenario==Overrun)overflow=true;return 1;
}
void captureWork(){'''+worker+r'''
int main(){
 for(int s=Normal;s<=RenameFail;++s){
   scenario=Scenario(s);chunks=recordCalls=locks=0;micEnded=renamed=closed=railOff=false;
   disk.clear();stopping=false;finished=false;overflow=false;bytes=0;success=false;
   capture(nullptr);assert(finished&&micEnded&&railOff&&locks==0);
   if(s==Normal||s==Limit){
     assert(success&&renamed&&closed);assert(bytes==(s==Normal?samples*2*3:VoiceFormat::maxBytes));
     uint8_t expected[44];VoiceFormat::header(expected,bytes);assert(disk.size()==bytes+44&&!memcmp(disk.data(),expected,44));
   }else assert(!success&&!renamed);
 }
 puts("Actual capture worker: stop, five-minute limit, SD/queue/mic failures, partial retention and cleanup passed.");
}
'''
with tempfile.TemporaryDirectory(prefix='paper-voice-') as directory:
    exe=str(Path(directory)/'test')
    subprocess.run(['g++','-std=c++11','-fsanitize=address,undefined','-Iinclude','-x','c++','-','-o',exe],input=source,text=True,check=True)
    subprocess.run([exe],check=True)
