#include "paper_clock.h"
#include "voice.h"
#include <M5Unified.h>
#include <Preferences.h>
#include <sys/time.h>
#include <limits>
namespace PaperClock {
namespace {Preferences prefs;bool valid=false,storage=false;}
void begin(){
    storage=prefs.begin("paper-clock",false);
    m5::rtc_datetime_t rtc{};
    valid=storage&&prefs.getBool("set",false)&&M5.Rtc.getDateTime(&rtc)&&rtc.date.year>=2024&&rtc.date.year<=2099;
    if(valid)M5.Rtc.setSystemTimeFromRtc();
}
bool ready(){return valid;}
void routes(WebServer& s,const String& token){
    s.on("/api/clock",HTTP_POST,[&s,&token]{
        auto error=[&s](int status,const char* why){s.send(status,"application/json",String("{\"error\":\"")+why+"\"}");};
        if(s.header("X-Paper-Token")!=token){error(403,"Reload the page.");return;}
        if(Voice::active()){error(409,"Stop recording before changing the clock.");return;}
        String input=s.arg("epoch");uint64_t epoch=0;
        if(input.length()!=10){error(400,"Expected UTC epoch seconds, 2024 through 2099.");return;}
        for(char c:input){if(c<'0'||c>'9'){error(400,"Invalid clock value.");return;}epoch=epoch*10+c-'0';}
        if(epoch<1704067200ULL||epoch>=4102444800ULL||epoch>uint64_t(std::numeric_limits<time_t>::max())){error(400,"Clock date outside supported range.");return;}
        time_t now=epoch;tm utc{};gmtime_r(&now,&utc);M5.Rtc.setDateTime(&utc);
        m5::rtc_datetime_t actual{};
        if(!M5.Rtc.getDateTime(&actual)||actual.date.year!=utc.tm_year+1900||actual.date.month!=utc.tm_mon+1||actual.date.date!=utc.tm_mday||actual.time.hours!=utc.tm_hour||actual.time.minutes!=utc.tm_min){error(503,"RTC verification failed. Try again.");return;}
        timeval tv{now,0};settimeofday(&tv,nullptr);
        valid=storage&&prefs.putBool("set",true)>0;
        if(!valid){error(503,"Cannot persist clock setup.");return;}
        s.send(200,"application/json","{\"ok\":true,\"timezone\":\"UTC\"}");
    });
}
}
