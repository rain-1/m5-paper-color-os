#pragma once
#include <WebServer.h>
namespace Voice {
void begin();
bool active();
bool start();
void stop();
void tick();
void routes(WebServer& server,const String& token);
}
