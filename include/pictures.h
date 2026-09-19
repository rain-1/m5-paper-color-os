#pragma once
#include <M5Unified.h>
#include <WebServer.h>
#include <vector>

extern SemaphoreHandle_t pictureBus;
bool picturesBegin();
void picturesRoutes(WebServer& server, void (*display)(const char*));
void picturesPage(WebServer& server);
// Caller holds pictureBus, shared with the panel's SPI operations.
bool picturesDraw(M5Canvas& canvas, const char* path);
// Display worker only: caller holds pictureBus. Bounded newest-first lists.
bool picturesMonths(std::vector<String>& months);
bool picturesList(const String& month,std::vector<String>& paths);
bool picturesPreview(M5Canvas& canvas,const char* path,int x,int y);
