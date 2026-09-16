#pragma once
#include <M5Unified.h>
#include <WebServer.h>

extern SemaphoreHandle_t pictureBus;
bool picturesBegin();
void picturesRoutes(WebServer& server, void (*display)(const char*));
void picturesPage(WebServer& server);
// Caller holds pictureBus, shared with the panel's SPI operations.
bool picturesDraw(M5Canvas& canvas, const char* path);
