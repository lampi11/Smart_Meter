#pragma once
#include <Arduino.h>

void initOTA();
void checkOTA();
bool isOTASafe();
void markOTAValid();
