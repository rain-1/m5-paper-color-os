#pragma once
#include <M5Unified.h>
#include <WebServer.h>

namespace Reader {
enum class Action { Menu, Previous, Next, Select, Back, Open, Font, Jump, Bookmark, Recall, Sampler, Resume, TestNormal, TestAccelerated, VoiceStatus };
void begin(void (*schedule)());
bool request(Action action, const char* id="", uint32_t value=0);
bool active();
bool busy();
// -1 outside an idle menu; otherwise fixed colour row 0..4.
int menuSelection();
bool menuPicking();
void leave();
void resumeLast();
// The display task owns reader rendering/state and holds pictureBus for both.
bool render(M5Canvas& canvas, int battery);
void displayed();
void routes(WebServer& server, const String& token);
}
