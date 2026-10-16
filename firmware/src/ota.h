#pragma once
#include <Arduino.h>

void initOTA();
void checkOTA();
void triggerOTA(const char* customUrl = nullptr);
bool isOTASafe();
void markOTAValid();
