#include "device.h"
#include "device_page.h"
#include "pictures.h"
#include "feedback.h"
#include "version.h"
#include "ota_power.h"
#include <M5Unified.h>
#include <WiFi.h>
#include <Update.h>
#include <esp_ota_ops.h>

namespace {
uint32_t counts[3]{}, unlockedUntil=0, ledUntil=0, restartAt=0;
bool cHandled=false, updateStarted=false, updateComplete=false, updateLock=false;
size_t expected=0, received=0;
String updateError;
uint32_t lastChunk=0;
uint32_t powerCheckedAt=0;
bool powerRejected=false;
bool powerAllowed(){
    return OtaPower::allowed(M5.Power.getVBUSVoltage(),M5.Power.getBatteryVoltage(),M5.Power.getBatteryLevel());
}
bool stablePower(){
    for(int i=0;i<3;++i){
        if(!powerAllowed())return false;
        if(i<2)delay(20);
    }
    powerCheckedAt=millis();
    return true;
}
float temperature=NAN, humidity=NAN;
uint32_t sensorAt=0;
const char* resetReason(){
    switch(esp_reset_reason()){
        case ESP_RST_POWERON:return "power-on / hardware reset";
        case ESP_RST_SW:return "software restart";
        case ESP_RST_PANIC:return "firmware crash";
        case ESP_RST_INT_WDT:return "interrupt watchdog";
        case ESP_RST_TASK_WDT:return "task watchdog";
        case ESP_RST_WDT:return "watchdog";
        case ESP_RST_BROWNOUT:return "low-voltage reset";
        case ESP_RST_DEEPSLEEP:return "wake from deep sleep";
        case ESP_RST_EXT:return "external reset";
        default:return "unknown";
    }
}
int otaSeconds(){return unlockedUntil && int32_t(unlockedUntil-millis())>0 ? (unlockedUntil-millis())/1000+1 : 0;}
void abortUpdate(){if(updateStarted)Update.abort();updateStarted=false;if(updateLock){xSemaphoreGive(pictureBus);updateLock=false;}}
uint8_t crc(const uint8_t* b){uint8_t c=255;for(int i=0;i<2;i++){c^=b[i];for(int j=0;j<8;j++)c=(c&128)?(c<<1)^0x31:c<<1;}return c;}
void readEnvironment(){
    bool ok=M5.In_I2C.start(0x44,false,100000);
    if(ok)ok=M5.In_I2C.write(0xfd);
    M5.In_I2C.stop();
    if(!ok){temperature=humidity=NAN;return;}
    delay(10);
    uint8_t b[6];ok=M5.In_I2C.start(0x44,true,100000);
    if(ok)ok=M5.In_I2C.read(b,6,true);
    M5.In_I2C.stop();
    if(!ok || crc(b)!=b[2] || crc(b+3)!=b[5]){temperature=humidity=NAN;return;}
    temperature=-45+175*((b[0]<<8)|b[1])/65535.0f;
    humidity=std::max(0.0f,std::min(100.0f,-6+125*((b[3]<<8)|b[4])/65535.0f));
}
}

void deviceTick(){
    if(M5.BtnA.wasPressed())counts[0]++;
    if(M5.BtnB.wasPressed())counts[1]++;
    if(M5.BtnC.wasPressed())counts[2]++;
    if(!M5.BtnC.isPressed())cHandled=false;
    if(M5.BtnC.pressedFor(2500)&&!cHandled){
        unlockedUntil=millis()+120000;cHandled=true;
        Feedback::play(Feedback::Cue::Unlocked);
        Serial.println("OTA unlocked for 120 seconds");
    }
    if(ledUntil && int32_t(millis()-ledUntil)>=0){M5.Led.setAllColor(0,0,0);ledUntil=0;}
    if(updateStarted && millis()-lastChunk>15000){abortUpdate();updateError="Upload timed out";}
    if(restartAt && int32_t(millis()-restartAt)>=0)ESP.restart();
    if(!updateStarted && millis()-sensorAt>10000){sensorAt=millis();readEnvironment();}
}

void deviceRoutes(WebServer& s,const String& token){
    s.on("/device",HTTP_GET,[&s,&token]{String page(DEVICE_HTML);page.replace("{{TOKEN}}",token);s.sendHeader("Cache-Control","no-store");s.sendHeader("Content-Security-Policy","default-src 'none'; script-src 'self'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'");s.send(200,"text/html",page);});
    s.on("/device.js",HTTP_GET,[&s]{s.send_P(200,"text/javascript",DEVICE_JS);});
    s.on("/api/device",HTTP_GET,[&s]{
        m5::rtc_datetime_t rtc{};bool rtcOk=M5.Rtc.getDateTime(&rtc);
        char date[32];snprintf(date,sizeof(date),"%04d-%02d-%02d %02d:%02d:%02d",rtc.date.year,rtc.date.month,rtc.date.date,rtc.time.hours,rtc.time.minutes,rtc.time.seconds);
        auto charging=M5.Power.isCharging();
        int battery=M5.Power.getBatteryLevel(), batteryMv=M5.Power.getBatteryVoltage(), inputMv=M5.Power.getVBUSVoltage();
        String body=String("{\"version\":\"")+PAPER_OS_VERSION+"\",\"battery\":"+String(battery)+",\"millivolts\":"+String(batteryMv);
        body+=",\"inputMillivolts\":"+String(inputMv)+",\"otaPowerOk\":"+(OtaPower::allowed(inputMv,batteryMv,battery)?String("true"):String("false"));
        body+=String(",\"sounds\":")+(Feedback::enabled()?"true":"false");
        body+=",\"charging\":\""+String(charging==m5::Power_Class::is_charging ? "yes" : charging==m5::Power_Class::is_discharging ? "no" : "unknown")+"\"";
        body+=",\"ip\":\""+WiFi.localIP().toString()+"\",\"rssi\":"+String(WiFi.RSSI());
        body+=",\"buttons\":["+String(counts[0])+","+String(counts[1])+","+String(counts[2])+"]";
        body+=",\"heap\":"+String(ESP.getFreeHeap())+",\"psram\":"+String(ESP.getFreePsram())+",\"uptime\":"+String(millis()/1000);
        body+=",\"resetReason\":\""+String(resetReason())+"\"";
        body+=",\"temperature\":"+(isnan(temperature)?String("null"):String(temperature,1))+",\"humidity\":"+(isnan(humidity)?String("null"):String(humidity,1));
        body+=",\"rtc\":\""+String(rtcOk?date:"unavailable")+"\",\"otaSeconds\":"+String(otaSeconds())+"}";
        s.sendHeader("Cache-Control","no-store");s.send(200,"application/json",body);
    });
    s.on("/api/feedback",HTTP_POST,[&s,&token]{
        if(s.header("X-Paper-Token")!=token){s.send(403,"application/json","{\"error\":\"Reload the page.\"}");return;}
        if(s.hasArg("enabled"))Feedback::setEnabled(s.arg("enabled")=="true");
        else{
            String cue=s.arg("cue");
            if(cue=="saved")Feedback::play(Feedback::Cue::Saved);
            else if(cue=="error")Feedback::play(Feedback::Cue::Error);
            else if(cue=="busy")Feedback::play(Feedback::Cue::Busy);
            else if(cue=="unlocked")Feedback::play(Feedback::Cue::Unlocked);
            else if(cue=="updated")Feedback::play(Feedback::Cue::Updated);
            else{s.send(400,"application/json","{\"error\":\"Unknown cue.\"}");return;}
        }
        s.send(200,"application/json","{\"ok\":true}");
    });
    s.on("/api/test",HTTP_POST,[&s,&token]{
        if(s.header("X-Paper-Token")!=token){s.send(403,"application/json","{\"error\":\"Reload the page.\"}");return;}
        String action=s.arg("action");
        if(action=="tone"){
            M5.Speaker.setVolume(48);
            if(!M5.Speaker.tone(880,200)){s.send(503,"application/json","{\"error\":\"Speaker test failed to start.\"}");return;}
        }else if(action=="led0"||action=="led1"||action=="off"){
            M5.Led.setBrightness(40);M5.Led.setAllColor(0,0,0);
            if(action=="led0")M5.Led.setColor(0,255,0,0);
            if(action=="led1")M5.Led.setColor(1,0,0,255);
            ledUntil=millis()+5000;
        }else{s.send(400,"application/json","{\"error\":\"Unknown test.\"}");return;}
        s.send(200,"application/json","{\"ok\":true}");
    });
    s.on("/api/update",HTTP_POST,[&s,&token]{
        bool ok=s.header("X-Paper-Token")==token && updateStarted && updateComplete && updateError.isEmpty() && received==expected;
        if(ok && !stablePower()){powerRejected=true;ok=false;}
        if(ok)ok=Update.end(false);
        if(ok){updateStarted=false;unlockedUntil=0;if(updateLock){xSemaphoreGive(pictureBus);updateLock=false;}s.send(200,"application/json","{\"ok\":true}");Feedback::play(Feedback::Cue::Updated);restartAt=millis()+1500;}
        else{
            abortUpdate();Feedback::play(Feedback::Cue::Error);
            if(powerRejected)s.send(409,"application/json","{\"error\":\"Update stopped: power is low or unavailable. Connect stable USB power, or charge the battery to at least 30% and 3.6 V. Current firmware was kept.\"}");
            else s.send(400,"application/json","{\"error\":\"Update rejected. Hold C to unlock; use a complete Paper OS firmware.bin and wait for the screen to finish refreshing.\"}");
        }
    },[&s,&token]{
        if(!s.header("Content-Type").startsWith("multipart/")){
            abortUpdate();updateComplete=false;powerRejected=false;updateError="Multipart required";
            if(s.raw().status==RAW_START){s.send(400,"application/json","{\"error\":\"Multipart file upload required.\"}");s.client().stop();}
            return;
        }
        auto& u=s.upload();lastChunk=millis();
        if(u.status==UPLOAD_FILE_START){
            bool duplicate=updateStarted;abortUpdate();updateComplete=false;powerRejected=false;updateError="";received=0;
            String size=s.arg("size");expected=0;
            for(size_t i=0;i<size.length();i++){if(size[i]<'0'||size[i]>'9'||expected>0x640000){expected=0;break;}expected=expected*10+size[i]-'0';}
            const esp_partition_t* next=esp_ota_get_next_update_partition(nullptr);
            if(duplicate||s.header("X-Paper-Token")!=token||!otaSeconds()||!next||expected<1024||expected>next->size){updateError="Rejected";return;}
            updateLock=xSemaphoreTake(pictureBus,0)==pdTRUE;
            if(!updateLock){updateError="Screen busy";return;}
            if(!stablePower()){powerRejected=true;updateError="Unsafe power";abortUpdate();return;}
            updateStarted=Update.begin(expected,U_FLASH);
            if(!updateStarted){updateError="Cannot start";abortUpdate();}
        }else if(u.status==UPLOAD_FILE_WRITE && updateStarted && updateError.isEmpty()){
            if(millis()-powerCheckedAt>=1000){
                powerCheckedAt=millis();
                if(!powerAllowed()){powerRejected=true;updateError="Unsafe power";abortUpdate();return;}
            }
            if(u.currentSize>expected-received || Update.write(u.buf,u.currentSize)!=u.currentSize){updateError="Write failed";abortUpdate();}
            else received+=u.currentSize;
        }else if(u.status==UPLOAD_FILE_END)updateComplete=true;
        else if(u.status==UPLOAD_FILE_ABORTED){updateError="Aborted";abortUpdate();}
    });
}
