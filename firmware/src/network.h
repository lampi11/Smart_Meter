#pragma once
#include <Arduino.h>
#include "meter.h"

void initNetwork();
bool isWiFiConnected();
void maintainNetwork(const MeterReading &live);
void syncNTP();
String getDeviceMac();
String getSerialNumber();
String getClaimCode();
String getServerUrl();
void resetWiFiCredentials();
