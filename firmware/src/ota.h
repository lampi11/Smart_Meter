#pragma once
#include <Arduino.h>

void initOTA();
void checkOTA();
void triggerOTA(const char* customUrl = nullptr);
void handleCloudVersionCheck(const char* latestVersion, const char* otaUrl = nullptr);
int  compareVersions(const char* vA, const char* vB);
bool isOTASafe();
void markOTAValid();
