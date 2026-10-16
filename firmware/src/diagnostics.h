#pragma once
#include <Arduino.h>

void initDiagnostics();
void reportDiagnostics();
void incrementErrorCounter(const char* errorType);
