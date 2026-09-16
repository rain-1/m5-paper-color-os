#pragma once
#include <M5Unified.h>
#include <WebServer.h>
namespace RefreshTest {
bool faulted();
// Display task only; caller holds pictureBus. Never writes NVS or SD.
void run(M5Canvas& canvas, bool accelerated);
void routes(WebServer& server, const String& token);
}
