#pragma once
#include <WebServer.h>
#include <vector>
namespace Voice {
void begin();
bool active();
bool playing();
const char* status();
// Display worker may enqueue; main task starts audio after display/bus is idle.
bool enqueue(const char* path="");
// Caller holds pictureBus. Newest 100 completed notes across all months.
bool list(std::vector<String>& paths);
bool start();
void stop();
void tick();
void routes(WebServer& server,const String& token);
}
