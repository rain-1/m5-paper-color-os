#pragma once
#include <WebServer.h>
// Caller holds pictureBus. Reads at most 64 bytes of display metadata.
String bookStorageTitle(const String& id);
void bookStorageRoutes(WebServer& server,const String& token);
