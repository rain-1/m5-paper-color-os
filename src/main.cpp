#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_system.h>
#include "portal.h"
#include "pictures.h"
#include "device.h"
#include "feedback.h"
#include "version.h"
#include "reader.h"
#include "refresh_test.h"

namespace {
constexpr uint32_t CONNECT_TIMEOUT = 30000;
constexpr uint32_t PORTAL_GRACE = 20000;
const IPAddress PORTAL_IP(192, 168, 4, 1);
struct Credentials { char ssid[33]; char password[64]; };
struct Screen { bool setup; char network[33]; char password[17]; char ip[16]; bool boot; char picture[64]; int battery; bool reader; };
Preferences prefs;
WebServer server(80);
DNSServer dns;
QueueHandle_t screenQueue;
Credentials candidate{};
String apName, apPassword, token;
String result = "Ready to connect.";
bool portal = false, connecting = false, testing = false, online = false;
bool storageReady = false, pendingConnect = false, saved = false;
uint32_t connectStarted = 0, portalCloseAt = 0, disconnectedAt = 0;

String randomHex() {
    char value[17];
    snprintf(value, sizeof(value), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
    return String(value);
}

void showScreen(bool setup) {
    if (Reader::active()) return;
    Screen screen{};
    screen.setup = setup;
    screen.battery = M5.Power.getBatteryLevel();
    (setup ? apName : WiFi.SSID()).toCharArray(screen.network, sizeof(screen.network));
    apPassword.toCharArray(screen.password, sizeof(screen.password));
    (setup ? PORTAL_IP : WiFi.localIP()).toString().toCharArray(screen.ip, sizeof(screen.ip));
    xQueueOverwrite(screenQueue, &screen);
}

// The panel may block for tens of seconds. Only this task owns graphics after
// initialization; HTTP/DNS and button handling continue on the Arduino task.
void displayTask(void*) {
    M5Canvas canvas(&M5.Display);
    canvas.setColorDepth(16);
    if (!canvas.createSprite(M5.Display.width(), M5.Display.height())) {
        Serial.println("Display buffer allocation failed");
        vTaskDelete(nullptr);
        return;
    }
    Screen screen;
    while (true) {
        xQueueReceive(screenQueue, &screen, portMAX_DELAY);
        xSemaphoreTake(pictureBus, portMAX_DELAY);
        if (RefreshTest::faulted()) {
            if(screen.reader) Reader::displayed();
            xSemaphoreGive(pictureBus);
            continue;
        }
        if (screen.reader) {
            if (Reader::render(canvas, screen.battery)) {
                M5.Display.setEpdMode(epd_mode_t::epd_fastest);
                canvas.pushSprite(0,0);
                M5.Display.waitDisplay();
                M5.Display.setEpdMode(epd_mode_t::epd_quality);
            }
            Reader::displayed();
            xSemaphoreGive(pictureBus);
            continue;
        }
        if (screen.picture[0]) {
            if (picturesDraw(canvas, screen.picture)) {
                // Prepared .p6 pixels are already quantized/dithered. On the
                // pinned ED2208 driver, fastest selects NO dithering only;
                // it does not change panel clocks or refresh waveforms.
                M5.Display.setEpdMode(epd_mode_t::epd_fastest);
                canvas.pushSprite(0,0);
                M5.Display.waitDisplay();
                M5.Display.setEpdMode(epd_mode_t::epd_quality);
                Serial.println("Picture display complete");
            }
            xSemaphoreGive(pictureBus);
            continue;
        }
        canvas.fillSprite(WHITE);
        const uint16_t colors[] = {BLACK, BLUE, GREEN, RED, YELLOW};
        int width = canvas.width();
        for (int i = 0; i < 5; ++i) canvas.fillRect(i * width / 5, 0, width / 5 + 1, 16, colors[i]);
        canvas.setTextColor(BLACK, WHITE);
        canvas.setTextFont(4);
        canvas.setTextSize(2);
        canvas.drawString("Paper OS", 24, 52);
        canvas.setTextSize(1);
        canvas.drawString("A quiet place to begin.", 24, 120);
        canvas.drawFastHLine(24, 175, width - 48, BLACK);
        if (screen.boot) {
            canvas.drawString("Finding your Wi-Fi...", 24, 204);
            canvas.setTextFont(2);
            canvas.drawString("Setup will open if we cannot connect.", 24, 290);
            canvas.drawString("Colour e-paper takes a moment to wake.", 24, 320);
            canvas.pushSprite(0, 0);
            M5.Display.waitDisplay();
            xSemaphoreGive(pictureBus);
            continue;
        }
        canvas.drawString(screen.setup ? "Let's get connected." : "You're connected.", 24, 204);
        canvas.setTextFont(2);
        canvas.setTextSize(1);
        canvas.drawString(screen.setup ? "1. Join this Wi-Fi on your phone:" : "Wi-Fi network", 24, 265);
        canvas.drawString(screen.network, 24, 294);
        if (screen.setup) {
            canvas.drawString("Password", 24, 338);
            canvas.drawString(screen.password, 24, 362);
            canvas.drawString("2. Open the setup page:", 24, 415);
            canvas.drawString("http://192.168.4.1", 24, 444);
        } else {
            canvas.drawString("Device address", 24, 338);
            canvas.drawString(screen.ip, 24, 362);
            canvas.drawString("Open this address in your browser", 24, 430);
            canvas.drawString("for pictures and the device lab.", 24, 454);
        }
        canvas.drawString("Top C: reader / Hold left B: Wi-Fi", 24, 535);
        canvas.drawString(String("v") + PAPER_OS_VERSION + " / Battery: " + screen.battery + "%", 24, 565);
        canvas.pushSprite(0, 0);
        M5.Display.waitDisplay();
        xSemaphoreGive(pictureBus);
    }
}

void reply(int code, const String& body) {
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.sendHeader("Content-Security-Policy", "default-src 'none'; style-src 'unsafe-inline'; form-action 'self'; frame-ancestors 'none'");
    server.send(code, "text/html; charset=utf-8", body);
}

bool fromPortal() { return portal && server.client().localIP() == PORTAL_IP; }

void setupRoutes() {
    server.on("/", HTTP_GET, [] {
        if (!fromPortal()) { picturesPage(server); return; }
        String page(PORTAL_HTML);
        page.replace("{{TOKEN}}", token);
        reply(200, page);
    });
    picturesRoutes(server, [](const char* path) {
        Reader::leave();
        Screen screen{};
        strlcpy(screen.picture,path,sizeof(screen.picture));
        xQueueOverwrite(screenQueue,&screen);
    });
    server.on("/status", HTTP_GET, [] {
        if (!fromPortal()) { reply(403, "Setup Wi-Fi only."); return; }
        String page = "<!doctype html><meta name='viewport' content='width=device-width,initial-scale=1'>";
        if (connecting || pendingConnect) page += "<meta http-equiv='refresh' content='3'>";
        page += "<title>Paper OS</title><h1>Paper OS</h1><p>" + result + "</p>";
        if (!connecting && !pendingConnect && !saved) page += "<a href='/'>Try again</a>";
        reply(200, page);
    });
    server.on("/connect", HTTP_POST, [] {
        if (!fromPortal() || server.arg("token") != token) { reply(403, "Reopen the setup page and try again."); return; }
        if (connecting || pendingConnect || saved) { reply(409, "A connection is already in progress. Open /status."); return; }
        String ssid = server.arg("ssid"), password = server.arg("password");
        if (ssid.length() == 0 || ssid.length() > 32 || password.length() > 63 ||
            (password.length() > 0 && password.length() < 8)) {
            reply(400, "Use a 1-32 byte network name and an 8-63 byte password (or empty for open Wi-Fi). <a href='/'>Back</a>");
            return;
        }
        candidate = {};
        ssid.toCharArray(candidate.ssid, sizeof(candidate.ssid));
        password.toCharArray(candidate.password, sizeof(candidate.password));
        pendingConnect = true;
        result = "Connecting... This can take 30 seconds. Stay on the setup Wi-Fi.";
        server.sendHeader("Location", "/status");
        reply(303, "Connecting. <a href='/status'>Check progress</a>");
    });
    // Includes Android, Apple and Windows HTTP captive-portal probes.
    server.onNotFound([] {
        if (!fromPortal()) { reply(403, "Setup Wi-Fi only."); return; }
        server.sendHeader("Location", "http://192.168.4.1/");
        reply(302, "<a href='http://192.168.4.1/'>Open Paper OS setup</a>");
    });
}

void startPortal() {
    if (portal) return;
    WiFi.disconnect(false, false);
    connecting = testing = online = saved = false;
    portalCloseAt = 0;
    WiFi.mode(WIFI_AP_STA);
    apPassword = randomHex().substring(0, 12);
    token = randomHex();
    if (!WiFi.softAPConfig(PORTAL_IP, PORTAL_IP, IPAddress(255,255,255,0)) ||
        !WiFi.softAP(apName.c_str(), apPassword.c_str())) {
        Serial.println("Setup access point failed; restarting");
        delay(1000);
        ESP.restart();
    }
    dns.start(53, "*", PORTAL_IP);
    server.begin();
    portal = true;
    result = "Ready to connect.";
    showScreen(true);
    Serial.println("Setup portal started");
}

void beginConnection(bool isTest) {
    WiFi.disconnect(false, false);
    WiFi.begin(candidate.ssid, candidate.password);
    testing = isTest;
    connecting = true;
    online = false;
    connectStarted = millis();
}
} // namespace

void setup() {
    auto cfg = M5.config();
    cfg.clear_display = false;
    cfg.internal_spk = true;
    cfg.internal_mic = false;
    M5.begin(cfg);
    Serial.begin(115200);
    M5.Display.setRotation(0);
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    picturesBegin();
    Feedback::begin();
    if (!pictureBus) { Serial.println("SPI mutex allocation failed"); while(true) delay(1000); }
    screenQueue = xQueueCreate(1, sizeof(Screen));
    Reader::begin([] {
        Screen screen{};
        screen.reader = true;
        screen.battery = M5.Power.getBatteryLevel();
        xQueueOverwrite(screenQueue, &screen);
    });
    if (!screenQueue || xTaskCreate(displayTask, "paper-display", 8192, nullptr, 1, nullptr) != pdPASS) {
        Serial.println("Display task initialization failed");
        while (true) delay(1000);
    }
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    WiFi.setHostname("paper-os");
    String suffix = WiFi.macAddress();
    suffix.replace(":", "");
    apName = "PaperOS-" + suffix.substring(6);
    setupRoutes();
    server.begin();
    storageReady = prefs.begin("paper-os", false);
    bool haveCredentials = storageReady && prefs.getBytesLength("wifi") == sizeof(candidate) &&
                           prefs.getBytes("wifi", &candidate, sizeof(candidate)) == sizeof(candidate);
    haveCredentials = haveCredentials && candidate.ssid[0] && candidate.ssid[32] == '\0' && candidate.password[63] == '\0';
    M5.update();
    if (haveCredentials && !M5.BtnB.isPressed()) {
        Screen splash{};
        splash.boot = true;
        xQueueOverwrite(screenQueue, &splash);
        beginConnection(false);
        Reader::resumeLast();
    }
    else startPortal();
}

void loop() {
    M5.update();
    static bool aLong=false,bLong=false,cLong=false;
    auto readerAction=[](Reader::Action action) {
        if (!Reader::request(action)) Feedback::play(Feedback::Cue::Busy);
    };
    if (M5.BtnA.pressedFor(2500) && !aLong) {
        aLong=true;
        if (Reader::active()) readerAction(Reader::Action::Bookmark);
    }
    if (M5.BtnB.pressedFor(2500) && !bLong) {
        bLong=true;
        if (Reader::busy()) Feedback::play(Feedback::Cue::Busy);
        else { Reader::leave(); if (portal) showScreen(true); else startPortal(); }
    }
    if (M5.BtnC.pressedFor(2500)) cLong=true;
    deviceTick();
    Feedback::tick();
    if (M5.BtnA.wasReleased()) {
        if (!aLong) { if (Reader::active()) readerAction(Reader::Action::Previous); else showScreen(portal); }
        aLong=false;
    }
    if (M5.BtnB.wasReleased()) {
        if (!bLong) readerAction(Reader::active()?Reader::Action::Next:Reader::Action::Menu);
        bLong=false;
    }
    if (M5.BtnC.wasReleased()) {
        if (!cLong) readerAction(Reader::active()?Reader::Action::Select:Reader::Action::Menu);
        cLong=false;
    }
    if (portal) dns.processNextRequest();
    server.handleClient();
    if (pendingConnect) { pendingConnect = false; beginConnection(true); }
    uint32_t now = millis();
    if (connecting && WiFi.status() == WL_CONNECTED) {
        connecting = false;
        online = true;
        disconnectedAt = 0;
        if (testing) {
            saved = storageReady && prefs.putBytes("wifi", &candidate, sizeof(candidate)) == sizeof(candidate);
            memset(&candidate, 0, sizeof(candidate));
            if (saved) {
                result = "Connected and saved. You can return to your normal Wi-Fi. The setup network closes shortly.";
                portalCloseAt = now + PORTAL_GRACE;
                showScreen(false);
            } else {
                result = "Connected, but credentials could not be saved. Restart the device and try again.";
            }
        } else { memset(&candidate, 0, sizeof(candidate)); showScreen(false); }
        Serial.println("Wi-Fi connected");
        Feedback::play(Feedback::Cue::Connected);
        Serial.printf("Paper OS: http://%s/\n",WiFi.localIP().toString().c_str());
    } else if (connecting && now - connectStarted >= CONNECT_TIMEOUT) {
        connecting = false;
        WiFi.disconnect(false, false);
        memset(&candidate, 0, sizeof(candidate));
        if (!portal) startPortal();
        result = "Connection failed. Check the network name, password and 2.4 GHz signal, then try again. Previous saved credentials were kept.";
        Feedback::play(Feedback::Cue::Error);
    }
    if (portal && portalCloseAt && int32_t(now - portalCloseAt) >= 0) {
        dns.stop(); WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA);
        portal = false; portalCloseAt = 0;
    }
    if (online && WiFi.status() != WL_CONNECTED) {
        if (!disconnectedAt) disconnectedAt = now;
        if (now - disconnectedAt >= CONNECT_TIMEOUT) {
            if (portal) { online = saved = false; portalCloseAt = 0; showScreen(true); }
            else startPortal();
        }
    } else disconnectedAt = 0;
    delay(2);
}
