#pragma once
#include <Arduino.h>
#include "meter.h"

enum BalanceState { 
    BAL_NORMAL, 
    BAL_SURVIVAL, 
    BAL_LOW, 
    BAL_URGENT, 
    BAL_EMERGENCY, 
    BAL_CUTOFF 
};

void         initBilling();
void         updateBilling(const MeterReading &reading);
float        getBalance();
BalanceState getBalanceState();
bool         isRelayOn();
bool         isSurvivalMode();
bool         isSurvivalTripped();
void         applyRecharge(float amount);
void         setBalanceDirect(float newBal);
float        getUnsavedConsumption();
void         saveBalanceNow(const char* reason);
bool         shouldSaveBalance();
bool         inNoCutWindow();
void         setRelayPolarity(bool activeLow);
bool         isRelayActiveLow();
