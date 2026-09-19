#include "voice.h"
#include "voice_format.h"
#include "voice_page.h"
#include "pictures.h"
#include "reader.h"
#include "feedback.h"
#include "status_light.h"
#include "refresh_test.h"
#include "paper_clock.h"
#include <M5Unified.h>
#include <SD.h>
#include <atomic>
#include <algorithm>
#include <vector>
#include <time.h>

namespace Voice {
namespace {
constexpr size_t samples=8192;
QueueHandle_t filled=nullptr;
int16_t* buffers[2]{};
std::atomic<bool> running{false}, stopping{false}, finished{false}, overflow{false};
std::atomic<uint32_t> bytes{0}, peak{0};
std::atomic<const char*> message{"Ready. Set the clock before your first recording."};
String path;
bool success=false;
uint32_t powerAt=0;
void released(void*,void* data,size_t){
    int index=data==buffers[0]?0:1;
    if(!M5.Mic.isRecording()&&!stopping&&bytes<VoiceFormat::maxBytes-samples*4)overflow=true;
    if(xQueueSend(filled,&index,0)!=pdTRUE)overflow=true;
}
void captureWork(){
    bool locked=xSemaphoreTake(pictureBus,0)==pdTRUE;
    bool ok=false;
    File file;
    String temporary=path+".part";
    auto fail=[](const char* why){message=why;};
    if(!locked)fail("Device busy. Wait for display or SD work to finish.");
    else do {
        if(SD.cardType()==CARD_NONE){fail("Insert a FAT32 SD card and restart.");break;}
        uint64_t total=SD.totalBytes(),used=SD.usedBytes();
        if(total<used||total-used<VoiceFormat::maxBytes+65536){fail("Need at least 10 MB free on SD.");break;}
        String year=path.substring(0,16),month=path.substring(0,19);
        if((!SD.exists("/recordings")&&!SD.mkdir("/recordings"))||(!SD.exists(year)&&!SD.mkdir(year))||(!SD.exists(month)&&!SD.mkdir(month))){fail("Cannot create recordings directory.");break;}
        if(SD.exists(path)||SD.exists(temporary)){fail("Filename collision. Please try again.");break;}
        file=SD.open(temporary,FILE_WRITE);
        uint8_t h[44];VoiceFormat::header(h,0);
        if(!file||file.write(h,44)!=44){fail("Cannot create recording on SD.");break;}
        if(!M5.Mic.begin()){fail("Microphone could not start.");break;}
        M5.Mic.setBufferReleaseCallback(nullptr,released);
        xQueueReset(filled);
        if(!M5.Mic.record(buffers[0],samples)||!M5.Mic.record(buffers[1],samples)){fail("Microphone buffer setup failed.");break;}
        message="Recording. Release any user button to stop.";
        ok=true;
        while(bytes<VoiceFormat::maxBytes){
            int index;
            if(xQueueReceive(filled,&index,pdMS_TO_TICKS(2000))!=pdTRUE){ok=false;fail("Microphone timed out; partial file kept.");break;}
            if(overflow){ok=false;fail("Audio overrun; partial file kept.");break;}
            size_t n=std::min(size_t(VoiceFormat::maxBytes-bytes.load()),samples*2);
            uint32_t level=0;for(size_t i=0;i<n/2;++i)level=std::max(level,uint32_t(abs(int(buffers[index][i]))));peak=level;
            if(file.write(reinterpret_cast<uint8_t*>(buffers[index]),n)!=n){ok=false;fail("SD write failed; partial file kept.");break;}
            bytes.fetch_add(n);
            // Finish the next complete block on stop (at most ~0.52 seconds).
            if(stopping||bytes==VoiceFormat::maxBytes)break;
            if(!M5.Mic.record(buffers[index],samples)){ok=false;fail("Microphone queue failed; partial file kept.");break;}
        }
    }while(false);
    M5.Mic.end();
    M5.Mic.setBufferReleaseCallback(nullptr,nullptr);
    digitalWrite(45,LOW); // Speaker is stopped too; shared codec/mic rail off.
    if(file){
        uint8_t h[44];VoiceFormat::header(h,bytes);
        if(!file.seek(0)||file.write(h,44)!=44){ok=false;fail("WAV header write failed; partial file kept.");}
        file.flush();bool sizeOk=file.size()==bytes+44;file.close();
        if(!sizeOk){ok=false;fail("Recording length mismatch; partial file kept.");}
        if(ok){
            if(!SD.rename(temporary,path)){ok=false;fail("Final rename failed; partial file kept.");}
            else message=bytes==VoiceFormat::maxBytes?"Saved: five-minute limit reached.":"Saved to SD.";
        }
    }
    if(locked)xSemaphoreGive(pictureBus);
    success=ok;
}
void capture(void*){captureWork();finished.store(true,std::memory_order_release);vTaskDelete(nullptr);}
void json(WebServer& s,int code,const String& body){s.sendHeader("Cache-Control","no-store");s.send(code,"application/json",body);}
void error(WebServer& s,int code,const char* text){json(s,code,String("{\"error\":\"")+text+"\"}");}
struct Lock {bool held;Lock():held(xSemaphoreTake(pictureBus,0)==pdTRUE){}~Lock(){if(held)xSemaphoreGive(pictureBus);}};
}
void begin(){
    filled=xQueueCreate(2,sizeof(int));
    for(auto& buffer:buffers)buffer=static_cast<int16_t*>(ps_malloc(samples*sizeof(int16_t)));
}
bool active(){return running.load();}
bool start(){
    if(active()){message="Already recording. Stop and save first.";return false;}
    if(!filled||!buffers[0]||!buffers[1]||!StatusLight::ready()){message="Recording unavailable: memory or indicator failure.";return false;}
    if(Reader::busy()||RefreshTest::faulted()){message="Wait for the display to finish.";return false;}
    {Lock lock;if(!lock.held){message="Display or SD busy. Try again when finished.";return false;}}
    int level=M5.Power.getBatteryLevel(),mv=M5.Power.getBatteryVoltage();
    if(level<15||mv<3500){message="Charge to at least 15% and 3.5 V before recording.";return false;}
    time_t now=time(nullptr);struct tm utc{};gmtime_r(&now,&utc);
    if(!PaperClock::ready()||utc.tm_year<124||utc.tm_year>199){message="Set the clock at /voice before recording.";return false;}
    char name[64];snprintf(name,sizeof(name),"/recordings/%04d/%02d/%04d%02d%02d_%02d%02d%02d_%08lx.wav",utc.tm_year+1900,utc.tm_mon+1,utc.tm_year+1900,utc.tm_mon+1,utc.tm_mday,utc.tm_hour,utc.tm_min,utc.tm_sec,(unsigned long)esp_random());path=name;
    stopping=false;finished=false;overflow=false;bytes=0;peak=0;message="Starting microphone…";
    Feedback::suspend(true);
    if(Feedback::enabled()){M5.Speaker.tone(880,120);delay(160);}
    M5.Speaker.end();
    auto cfg=M5.Mic.config();cfg.sample_rate=VoiceFormat::rate;M5.Mic.config(cfg);
    running=true;powerAt=millis();
    if(xTaskCreate(capture,"voice-capture",6144,nullptr,2,nullptr)!=pdPASS){running=false;Feedback::suspend(false);message="Cannot start recording task.";return false;}
    return true;
}
void stop(){if(active()){stopping=true;message="Stopping and saving…";}}
void tick(){
    if(!active())return;
    if(finished.load(std::memory_order_acquire)){
        running=false;Feedback::suspend(false);Feedback::play(success?Feedback::Cue::Saved:Feedback::Cue::Error);return;
    }
    if(millis()-powerAt>2000){powerAt=millis();if(M5.Power.getBatteryVoltage()<3400||M5.Power.getBatteryLevel()<10)stop();}
}
void routes(WebServer& s,const String& token){
    s.on("/voice",HTTP_GET,[&s,&token]{String html(VOICE_HTML);html.replace("{{TOKEN}}",token);s.sendHeader("Cache-Control","no-store");s.sendHeader("Content-Security-Policy","default-src 'none'; script-src 'self'; style-src 'unsafe-inline'; connect-src 'self'; media-src 'self'; frame-ancestors 'none'");s.send(200,"text/html",html);});
    s.on("/voice.js",HTTP_GET,[&s]{s.send_P(200,"text/javascript",VOICE_JS);});
    s.on("/api/voice",HTTP_GET,[&s]{json(s,200,String("{\"active\":")+(active()?"true":"false")+",\"state\":\""+(active()?(stopping?"Saving":"Recording"):"Idle")+"\",\"seconds\":"+String(bytes.load()/32000)+",\"peak\":"+String(peak.load())+",\"message\":\""+message.load()+"\",\"path\":\""+path+"\"}");});
    s.on("/api/voice",HTTP_POST,[&s,&token]{
        if(s.header("X-Paper-Token")!=token){error(s,403,"Reload the page.");return;}
        if(s.arg("action")=="stop")stop();
        else if(s.arg("action")=="start"){if(!start()){error(s,409,message.load());return;}}
        else{error(s,400,"Unknown recording action.");return;}
        json(s,202,"{\"ok\":true}");
    });
    s.on("/api/recordings",HTTP_GET,[&s]{
        String month=s.arg("month");if(!picture::month(month.c_str())){error(s,400,"Choose a valid month.");return;}
        if(active()){error(s,409,"Stop recording before browsing SD.");return;}Lock lock;if(!lock.held){error(s,409,"Display or SD busy.");return;}
        if(SD.cardType()==CARD_NONE){error(s,503,"SD card unavailable.");return;}
        String dir="/recordings/"+month.substring(0,4)+"/"+month.substring(5,7);File folder=SD.open(dir);
        std::vector<String> files;
        if(folder&&folder.isDirectory())while(File file=folder.openNextFile()){
            String p=dir+"/"+file.name();if(file.isDirectory()||!VoiceFormat::path(p.c_str()))continue;
            auto at=std::lower_bound(files.begin(),files.end(),p,[](const String& a,const String& b){return a.compareTo(b)>0;});
            if(at!=files.end()||files.size()<100){files.insert(at,p);if(files.size()>100)files.pop_back();}delay(1);
        }
        String body="{\"files\":[";for(size_t i=0;i<files.size();++i){if(i)body+=',';body+='"'+files[i]+'"';}json(s,200,body+"]}");
    });
    s.on("/api/recording",HTTP_GET,[&s]{
        String p=s.arg("path");if(!VoiceFormat::path(p.c_str())){error(s,400,"Invalid recording path.");return;}
        if(active()){error(s,409,"Stop recording before playback.");return;}Lock lock;if(!lock.held){error(s,409,"Display or SD busy.");return;}
        File file=SD.open(p);if(!file||file.isDirectory()||file.size()<44||file.size()>VoiceFormat::maxBytes+44){error(s,404,"Recording unavailable.");return;}
        s.sendHeader("Cache-Control","no-store");s.streamFile(file,"audio/wav");
    });
}
}
