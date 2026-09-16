#include "refresh_test.h"
#include "refresh_test_page.h"
#include "reader.h"
#include "ota_power.h"
#include "feedback.h"
#include "refresh_palette.h"
#include "refresh_patterns.h"
#include <lgfx/v1/Bus.hpp>

namespace RefreshTest {
namespace {
constexpr uint8_t normalClock=0x08, fastClock=0x07;
constexpr uint32_t timeoutMs=120000;
portMUX_TYPE mutex=portMUX_INITIALIZER_UNLOCKED;
struct Result { uint32_t total=0,busy=0; uint8_t pattern=0; bool fast=false; };
struct State { bool fault=false,restored=true; uint32_t baseline=0,fast=0,busyMs=0; const char* mode="none"; const char* message="Ready. Start with a normal test."; uint8_t pattern=0,used=0; Result history[8]{}; } state;
State copy(){portENTER_CRITICAL(&mutex);State result=state;portEXIT_CRITICAL(&mutex);return result;}
void fail(const char* message){portENTER_CRITICAL(&mutex);state.fault=true;state.restored=false;state.message=message;portEXIT_CRITICAL(&mutex);Feedback::play(Feedback::Cue::Error);}
bool waitReady(int pin){uint32_t start=millis();while(!digitalRead(pin)){if(millis()-start>=timeoutMs)return false;delay(10);}return true;}
void chart(M5Canvas& c,bool fast,bool alternate){
    c.fillSprite(WHITE);c.setTextSize(1);c.setTextDatum(top_left);c.setTextColor(BLACK,WHITE);
    c.setFont(&fonts::FreeSerif18pt7b);c.drawString("Refresh comparison",20,20);
    c.setFont(&fonts::Font2);c.drawString(fast?"Accelerated full refresh / PLL 07":"Normal full refresh / PLL 08",20,72);
    c.setFont(&fonts::FreeSerif12pt7b);c.drawString("Book serif, twelve points.",20,120);c.drawString("The quiet light of morning.",20,155);
    c.setFont(&fonts::FreeSans12pt7b);c.drawString("Clean sans, twelve points.",20,215);c.drawString("The quiet light of morning.",20,250);
    c.drawRect(20,315,360,64,BLACK);c.fillCircle(alternate?330:70,347,18,BLACK);
    c.setFont(&fonts::Font2);c.drawString(alternate?"Variant 2: right dot. Check for ghosts.":"Variant 1: left dot. Check for ghosts.",20,390);
    const uint32_t colors[]={0x000000,0xff0000,0xffff00,0x0000ff,0x00ff00,0xffffff};
    for(int i=0;i<6;++i)c.fillRect(20+i*60,430,60,65,colors[i]);
    c.drawRect(20,430,360,65,BLACK);
    c.drawString("One shot only. Normal timing restored after test.",20,535);
    c.drawString("Compare results in /refresh-test",20,562);
}
void testPage(M5Canvas& c,bool fast,uint32_t encoded){
    unsigned pattern=encoded/2;bool alternate=encoded&1;
    if(pattern==0){chart(c,fast,alternate);return;}
    bool inverse=pattern==3;
    uint16_t fg=inverse?WHITE:BLACK,bg=inverse?BLACK:WHITE;
    c.fillSprite(bg);c.setTextSize(1);c.setTextDatum(top_left);c.setTextColor(fg,bg);c.setFont(&fonts::Font2);
    if(pattern!=6)c.drawString(String(RefreshPatterns::names[pattern])+" / variant "+(alternate?"2":"1")+" / "+(fast?"accelerated":"normal"),20,16);
    if(pattern>=1&&pattern<=3){
        c.setFont(pattern==2?static_cast<const lgfx::IFont*>(&fonts::FreeSans12pt7b):static_cast<const lgfx::IFont*>(&fonts::FreeSerif12pt7b));
        const char* first[]={"The morning light arrived","across the quiet garden.","A book lay open on the desk.","There was time for one page,","and then perhaps another.","Nothing here needed haste."};
        const char* second[]={"Rain tapped on the window.","Beyond it, trees were moving","against a pale evening sky.","We turned to a fresh page","and followed the words","until the room grew quiet."};
        int step=c.fontHeight()+4;
        for(int y=55,i=0;y+step<=540;y+=step,++i)c.drawString((alternate?second:first)[i%6],24,y);
    }else if(pattern==4){
        for(int y=55;y<535;y+=40)for(int x=0;x<400;x+=40)c.fillRect(x,y,40,40,((x/40+(y-55)/40+alternate)&1)?BLACK:WHITE);
    }else if(pattern==5){
        const uint32_t palette[]={0xffffff,0x000000,0xff0000,0xffff00,0x0000ff,0x00ff00};
        for(int y=0;y<6;++y)for(int x=0;x<4;++x)c.fillRect(x*100,55+y*80,100,80,palette[(x+y+(alternate?3:0))%6]);
    }
    c.setFont(&fonts::Font2);c.setTextColor(fg,bg);
    c.drawString(pattern==6?"White cleanup: check for residual marks.":"Use the browser for tests; buttons leave this page.",20,560);
}
}
bool faulted(){return copy().fault;}

void run(M5Canvas& canvas,bool accelerated,uint32_t pattern){
    if(faulted())return;
    if(!RefreshPatterns::valid(pattern))return;
    auto panel=M5.Display.panel();auto cfg=panel->config();auto bus=panel->getBus();
    if(M5.getBoard()!=m5::board_t::board_M5PaperColor || cfg.panel_width!=400 || cfg.panel_height!=600 || cfg.pin_cs<0 || cfg.pin_busy<0 || !bus){fail("Wrong panel configuration. Experiment refused.");return;}
    testPage(canvas,accelerated,pattern);
    portENTER_CRITICAL(&mutex);state.pattern=pattern;state.mode=accelerated?"accelerated":"normal";state.message="Refreshing test page. Do not interrupt power.";state.restored=false;state.busyMs=0;portEXIT_CRITICAL(&mutex);
    if(!waitReady(cfg.pin_busy)){fail("Panel was already stuck busy. No commands sent; restart required.");return;}
    // Use the configured display SPI bus under pictureBus. This isolated test
    // avoids the stock driver's 20 s BUSY timeout and does not modify its source.
    bus->beginTransaction();digitalWrite(cfg.pin_cs,LOW);
    struct Transaction { lgfx::IBus* bus; int cs; ~Transaction(){bus->wait();digitalWrite(cs,HIGH);bus->endTransaction();} } transaction{bus,cfg.pin_cs};
    auto command=[&](uint8_t c){bus->writeCommand(c,8);};
    auto data=[&](uint8_t d){bus->writeData(d,8);};
    uint32_t started=millis();
    command(0x30);data(accelerated?fastClock:normalClock);
    command(0x10);
    uint8_t row[200];
    for(int y=0;y<600;++y){
        for(int x=0;x<400;x+=2)row[x/2]=(RefreshPalette::native(canvas.readPixel(x,y))<<4)|RefreshPalette::native(canvas.readPixel(x+1,y));
        bus->writeBytes(row,sizeof(row),true,false);bus->wait(); // D/C=data; stack row, no DMA
        if((y&15)==0)delay(1);
    }
    command(0x04);bus->wait();delay(20);
    if(!waitReady(cfg.pin_busy)){fail("Power-on timeout. Display blocked; no further commands sent.");return;}
    delay(200);
    command(0x06);data(0x6f);data(0x1f);data(0x17);data(0x27);bus->wait();delay(200);
    command(0x12);data(0x00);bus->wait();
    uint32_t refreshStart=millis();
    while(digitalRead(cfg.pin_busy)&&millis()-refreshStart<1000)delay(1);
    if(digitalRead(cfg.pin_busy)){fail("Refresh BUSY did not assert. Display blocked; restart required.");return;}
    uint32_t busyStart=millis();
    if(!waitReady(cfg.pin_busy)){fail("Refresh timeout. Display blocked; do not repeatedly retry.");return;}
    uint32_t busyDuration=millis()-busyStart;
    delay(200);command(0x02);data(0x00);bus->wait();delay(20);
    if(!waitReady(cfg.pin_busy)){fail("Power-off timeout. Clock restoration not confirmed; restart required.");return;}
    delay(200);command(0x30);data(normalClock);bus->wait();
    uint32_t elapsed=millis()-started;
    portENTER_CRITICAL(&mutex);
    if(accelerated)state.fast=elapsed;else state.baseline=elapsed;
    state.busyMs=busyDuration;state.restored=true;state.message="Complete. Normal timing restored. Inspect the physical chart.";
    if(state.used==8){for(unsigned i=1;i<8;++i)state.history[i-1]=state.history[i];--state.used;}
    Result& result=state.history[state.used++];result.total=elapsed;result.busy=busyDuration;result.pattern=pattern;result.fast=accelerated;
    portEXIT_CRITICAL(&mutex);Feedback::play(Feedback::Cue::Saved);
}

void routes(WebServer& server,const String& token){
    server.on("/refresh-test",HTTP_GET,[&server,&token]{String page(REFRESH_TEST_HTML);page.replace("{{TOKEN}}",token);server.sendHeader("Cache-Control","no-store");server.sendHeader("Content-Security-Policy","default-src 'none'; script-src 'self'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'");server.send(200,"text/html; charset=utf-8",page);});
    server.on("/refresh-test.js",HTTP_GET,[&server]{server.send_P(200,"text/javascript",REFRESH_TEST_JS);});
    server.on("/api/refresh-test",HTTP_GET,[&server]{
        State s=copy();String body=String("{\"faulted\":")+(s.fault?"true":"false")+",\"restored\":"+(s.restored?"true":"false")+",\"busy\":"+(Reader::busy()?"true":"false");
        body+=",\"baselineMs\":"+String(s.baseline)+",\"fastMs\":"+String(s.fast)+",\"busyMs\":"+String(s.busyMs)+",\"mode\":\""+s.mode+"\",\"message\":\""+s.message+"\"";
        body+=",\"pattern\":\""+String(RefreshPatterns::names[s.pattern/2])+"\",\"variant\":"+String(1+(s.pattern&1))+",\"history\":[";
        for(unsigned i=0;i<s.used;++i){const auto& r=s.history[i];if(i)body+=',';body+="{\"pattern\":\""+String(RefreshPatterns::names[r.pattern/2])+"\",\"variant\":"+String(1+(r.pattern&1))+",\"mode\":\""+(r.fast?"accelerated":"normal")+"\",\"totalMs\":"+String(r.total)+",\"busyMs\":"+String(r.busy)+"}";}
        body+="]}";
        server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",body);
    });
    server.on("/api/refresh-test",HTTP_POST,[&server,&token]{
        auto reject=[&](int code,const char* message){server.send(code,"application/json",String("{\"error\":\"")+message+"\"}");};
        if(server.header("X-Paper-Token")!=token){reject(403,"Reload this page.");return;}
        if(faulted()){reject(409,"Display fault latched. Restart required before further display commands.");return;}
        String mode=server.arg("mode");if(mode!="normal"&&mode!="accelerated"){reject(400,"Unknown test mode.");return;}
        int pattern=RefreshPatterns::index(server.hasArg("pattern")?server.arg("pattern").c_str():"chart");
        String variant=server.hasArg("variant")?server.arg("variant"):(mode=="normal"?"1":"2");
        if(pattern<0||(variant!="1"&&variant!="2")){reject(400,"Unknown pattern or variant.");return;}
        if(mode=="accelerated"&&(server.arg("ack")!="timing-experiment"||!copy().baseline)){reject(400,"Run the normal chart first and acknowledge experimental timing.");return;}
        if(!OtaPower::allowed(M5.Power.getVBUSVoltage(),M5.Power.getBatteryVoltage(),M5.Power.getBatteryLevel())){reject(409,"Connect USB power or charge the battery before testing.");return;}
        if(!Reader::request(mode=="normal"?Reader::Action::TestNormal:Reader::Action::TestAccelerated,"",pattern*2+(variant=="2"))){reject(409,"Reader is busy. Wait for it to finish.");return;}
        server.send(202,"application/json","{\"queued\":true}");
    });
}
}
