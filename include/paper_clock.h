#pragma once
#include <WebServer.h>
namespace PaperClock {
void begin();
bool ready();
void routes(WebServer& server,const String& token);
}
