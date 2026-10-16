#include "meter.h"
#include "config.h"
#include <PZEM004Tv30.h>

// Global PZEM object — also used by billing.cpp for relay verification
PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);

void initMeter() {
    Serial.println("[METER] PZEM-004T initialised on UART2");
}

bool readMeter(MeterReading &r) {
    r.voltage     = pzem.voltage();
    r.current     = pzem.current();
    r.power       = pzem.power();
    r.powerFactor = pzem.pf();
    r.frequency   = pzem.frequency();
    r.energy      = pzem.energy();

    // Reject NaN or out-of-bounds
    if (isnan(r.voltage) || isnan(r.current) || isnan(r.power) ||
        isnan(r.energy)  || isnan(r.powerFactor) || isnan(r.frequency) ||
        r.voltage < 0 || r.voltage > MAX_VOLTAGE ||
        r.current < 0 || r.current > MAX_CURRENT) {
        r.voltage = r.current = r.power = r.powerFactor = r.frequency = r.energy = 0;
        r.valid = false;
        return false;
    }
    r.valid = true;
    return true;
}
