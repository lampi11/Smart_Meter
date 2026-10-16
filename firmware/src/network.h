#include "meter.h"
#pragma once
#include <Arduino.h>

void initNetwork();
bool isWiFiConnected();
void maintainNetwork(const MeterReading &live);
void syncNTP();
