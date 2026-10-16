#pragma once
#include <Arduino.h>
#include "meter.h"

enum BalanceState { BAL_NORMAL, BAL_LOW, BAL_URGENT, BAL_EMERGENCY, BAL_CUTOFF };

void         initBilling();
void         updateBilling(const MeterReading &reading);
float        getBalance();
BalanceState  getBalanceState();
bool         isRelayOn();
void         applyRecharge(float amount);
void         setBalanceDirect(float newBal);
float        getUnsavedConsumption();
void         saveBalanceNow(const char* reason);
bool         shouldSaveBalance();
bool         inNoCutWindow();
void         setRelayPolarity(bool activeLow);
bool         isRelayActiveLow();
