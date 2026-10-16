#pragma once
#include <Arduino.h>

struct MeterReading {
    float voltage;
    float current;
    float power;
    float powerFactor;
    float frequency;
    float energy;   // cumulative kWh from PZEM
    bool  valid;    // false if NaN or out-of-bounds
};

void initMeter();
bool readMeter(MeterReading &reading);
