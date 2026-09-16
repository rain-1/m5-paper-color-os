#include "pictures.h"
#include "picture_format.h"
#include "gallery.h"
#include "device.h"
#include "feedback.h"
#include "reader.h"
#include "refresh_test.h"
#include "book_storage.h"
#include <SD.h>
#include <SPI.h>
#include <esp_system.h>

SemaphoreHandle_t pictureBus;
namespace {
bool mounted = false;
String sessionToken;
uint8_t* uploadBytes = nullptr;
size_t received = 0;
bool completed = false;
String uploadError;
String uploadDate;
uint32_t uploadTouched = 0;

struct Lock {
    bool held;
    Lock() : held(xSemaphoreTake(pictureBus, 0) == pdTRUE) {}
    ~Lock() { if (held) xSemaphoreGive(pictureBus); }
};
void json(WebServer& s, int status, const String& body) {
    s.sendHeader("Cache-Control", "no-store");
    s.send(status, "application/json", body);
}
void error(WebServer& s, int status, const char* message) {
    // Polling/listing failures stay silent. User actions receive concise feedback.
    if(s.method()==HTTP_POST)Feedback::play(status==409?Feedback::Cue::Busy:Feedback::Cue::Error);
    json(s, status, String("{\"error\":\"") + message + "\"}");
}
bool authorized(WebServer& s) {
    return sessionToken.length() && s.header("X-Paper-Token") == sessionToken;
}
void resetUpload() {
    free(uploadBytes); uploadBytes = nullptr; received = 0; completed = false;
}
}

bool picturesBegin() {
    pictureBus = xSemaphoreCreateMutex();
    if (!pictureBus) return false;
    char value[33];
    snprintf(value, sizeof(value), "%08lx%08lx%08lx%08lx", (unsigned long)esp_random(),
        (unsigned long)esp_random(), (unsigned long)esp_random(), (unsigned long)esp_random());
    sessionToken = value;
    // M5Unified already enables the card's PMIC rail. These pins share SPI2
    // with the panel, so all SD and display operations use pictureBus.
    SPI.begin(15, 14, 13, 47);
    mounted = SD.begin(47, SPI, 10000000) && SD.cardType() != CARD_NONE;
    if (mounted) mounted = SD.exists("/pictures") || SD.mkdir("/pictures");
    Serial.printf("SD: %s\n", mounted ? "mounted; /pictures ready" : "mount failed (use FAT32; restart after inserting)");
    if (mounted) Serial.printf("SD capacity: %llu MB\n", SD.cardSize() / (1024ULL*1024));
    return mounted;
}

void picturesPage(WebServer& s) {
    String page(GALLERY_HTML);
    page.replace("{{TOKEN}}", sessionToken);
    s.sendHeader("Cache-Control", "no-store");
    s.sendHeader("Content-Security-Policy", "default-src 'none'; script-src 'self'; style-src 'unsafe-inline'; img-src blob:; connect-src 'self'; frame-ancestors 'none'");
    s.send(200, "text/html; charset=utf-8", page);
}

void picturesRoutes(WebServer& s, void (*display)(const char*)) {
    const char* headers[] = {"X-Paper-Token", "Content-Type"};
    s.collectHeaders(headers, 2);
    deviceRoutes(s,sessionToken);
    Reader::routes(s,sessionToken);
    bookStorageRoutes(s,sessionToken);
    s.on("/gallery", HTTP_GET, [&s] { picturesPage(s); });
    s.on("/converter.js", HTTP_GET, [&s] { s.send_P(200, "text/javascript", CONVERTER_JS); });
    s.on("/api/card", HTTP_GET, [&s] {
        json(s, 200, String("{\"mounted\":") + (mounted ? "true" : "false") + "}");
    });
    s.on("/api/images", HTTP_GET, [&s] {
        String month = s.arg("month");
        if (!picture::month(month.c_str())) { error(s,400,"Choose a valid month."); return; }
        if (!mounted) { error(s,503,"SD card is not mounted. Insert a FAT32 card and restart."); return; }
        Lock lock;
        if (!lock.held) { error(s,409,"Screen is refreshing. Try again in 30 seconds."); return; }
        String directory = "/pictures/" + month.substring(0,4) + "/" + month.substring(5,7);
        String body = "{\"files\":[";
        File dir = SD.open(directory);
        unsigned count=0;
        if (dir && dir.isDirectory()) {
            while (File file = dir.openNextFile()) {
                String path = directory + "/" + file.name();
                if (!file.isDirectory() && picture::path(path.c_str())) {
                    if (count == 100) break;
                    if (count++) body += ',';
                    body += '"' + path + '"';
                }
            }
        }
        body += "],\"limit\":100}";
        json(s,200,body);
    });
    s.on("/api/display", HTTP_POST, [&s,display] {
        if (!authorized(s)) { error(s,403,"Reload the page before trying again."); return; }
        if (RefreshTest::faulted()) { error(s,409,"Display fault latched. Restart required."); return; }
        if (Reader::busy()) { error(s,409,"Reader is refreshing. Please wait."); return; }
        String path=s.arg("path");
        if (!picture::path(path.c_str())) { error(s,400,"Invalid picture path."); return; }
        Lock lock;
        if (!lock.held) { error(s,409,"Screen is refreshing. Try again in 30 seconds."); return; }
        if (!mounted || !SD.exists(path)) { error(s,404,"Picture not found."); return; }
        display(path.c_str());
        json(s,202,"{\"queued\":true}");
    });
    s.on("/api/upload", HTTP_POST, [&s] {
        if (!authorized(s)) { resetUpload(); error(s,403,"Reload the page before trying again."); return; }
        if (!completed || !uploadBytes || !uploadError.isEmpty() || !picture::valid(uploadBytes,received)) {
            resetUpload(); error(s,400,"Upload rejected: expected one valid 400 x 600 Paper OS image."); return;
        }
        Lock lock;
        if (!lock.held) { resetUpload(); error(s,409,"Screen is refreshing. Try uploading again in 30 seconds."); return; }
        if (!mounted) { resetUpload(); error(s,503,"SD card is not mounted."); return; }
        String year="/pictures/"+uploadDate.substring(0,4);
        String month=year+"/"+uploadDate.substring(4,6);
        if ((!SD.exists(year) && !SD.mkdir(year)) || (!SD.exists(month) && !SD.mkdir(month))) {
            resetUpload(); error(s,507,"Could not create SD directories."); return;
        }
        char suffix[10]; snprintf(suffix,sizeof(suffix),"_%08lx",(unsigned long)esp_random());
        String path=month+"/"+uploadDate+suffix+".p6";
        String temp=path+".tmp";
        if (SD.exists(path) || SD.exists(temp)) { resetUpload(); error(s,409,"Filename collision. Retry upload."); return; }
        File file=SD.open(temp,FILE_WRITE);
        bool ok=file && file.write(uploadBytes,received)==received;
        if (file) { file.flush(); file.close(); }
        // Read back before publishing the file, detecting short writes/card errors.
        if (ok) {
            file=SD.open(temp);
            ok=file && file.size()==picture::fileSize;
            uint8_t check[512]; size_t offset=0;
            while (ok && offset<received) {
                size_t n=std::min(sizeof(check),received-offset);
                ok=file.read(check,n)==n && memcmp(check,uploadBytes+offset,n)==0;
                offset+=n;
            }
            file.close();
        }
        if (ok) ok=SD.rename(temp,path);
        if (!ok) SD.remove(temp);
        resetUpload();
        if (!ok) { error(s,507,"SD write verification failed. Check card space and try again."); return; }
        Serial.printf("Picture saved: %s\n",path.c_str());
        Feedback::play(Feedback::Cue::Saved);
        json(s,201,"{\"path\":\""+path+"\"}");
    }, [&s] {
        // Arduino invokes this callback for raw bodies too, without HTTPUpload.
        if (!s.header("Content-Type").startsWith("multipart/")) {
            resetUpload(); uploadError="Multipart required";
            if (s.raw().status == RAW_START) {
                error(s,400,"Multipart file upload required.");
                s.client().stop();
            }
            return;
        }
        HTTPUpload& u=s.upload();
        if (u.status==UPLOAD_FILE_START) {
            // Only one file per request; stale disconnected requests can be discarded.
            bool duplicate=uploadBytes && millis()-uploadTouched < 15000;
            resetUpload(); uploadError=""; uploadDate=s.arg("date");
            if (duplicate || !authorized(s) || !mounted || !picture::date(uploadDate.c_str())) uploadError="Rejected";
            else uploadBytes=static_cast<uint8_t*>(ps_malloc(picture::fileSize));
            if (!uploadBytes) uploadError="Rejected";
        } else if (u.status==UPLOAD_FILE_WRITE && uploadBytes && uploadError.isEmpty()) {
            if (u.currentSize > picture::fileSize-received) uploadError="Too large";
            else { memcpy(uploadBytes+received,u.buf,u.currentSize); received+=u.currentSize; }
        } else if (u.status==UPLOAD_FILE_END) completed=true;
        else if (u.status==UPLOAD_FILE_ABORTED) resetUpload();
        uploadTouched=millis();
    });
}

bool picturesDraw(M5Canvas& canvas, const char* path) {
    if (!mounted || !picture::path(path)) return false;
    File file=SD.open(path);
    if (!file || file.size()!=picture::fileSize) return false;
    uint8_t* bytes=static_cast<uint8_t*>(ps_malloc(picture::fileSize));
    if (!bytes) return false;
    bool ok=file.read(bytes,picture::fileSize)==picture::fileSize && picture::valid(bytes,picture::fileSize);
    file.close();
    if (ok) {
        const uint16_t colors[]={BLACK,WHITE,YELLOW,RED,BLUE,GREEN};
        for (size_t i=0;i<picture::width*picture::height;++i) {
            uint8_t packed=bytes[picture::headerSize+i/2];
            canvas.drawPixel(i%picture::width,i/picture::width,colors[(i&1) ? (packed&15) : (packed>>4)]);
        }
    }
    free(bytes);
    Serial.printf("Picture load: %s\n",ok ? "OK" : "invalid file");
    return ok;
}
