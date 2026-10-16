#pragma once
#include <Arduino.h>
#include "meter.h"
#include "billing.h"

void initDisplay();
void updateDisplay(const MeterReading &reading, float balance,
                   BalanceState state, bool relayOn);
void showCustomMessage(const char* msg);
void showSetupScreen(const char* apName, const char* code);
void showWiFiConnected(const char* ip);
