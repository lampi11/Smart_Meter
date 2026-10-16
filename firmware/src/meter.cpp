#include "meter.h"
#include "config.h"
#include <PZEM004Tv30.h>

static PZEM004Tv30* pzem = nullptr;

void initMeter() {
    Serial2.begin(PZEM_BAUD_RATE, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);
    pzem = new PZEM004Tv30(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);
    Serial.printf("[METER] PZEM-004T initialised on UART2 (RX=%d, TX=%d)\n", PZEM_RX_PIN, PZEM_TX_PIN);
}

float getPzemCurrent() {
    if (!pzem) return NAN;
    return pzem->current();
}

bool readMeter(MeterReading &r) {
    if (!pzem) return false;

    float v  = pzem->voltage();
    float i  = pzem->current();
    float p  = pzem->power();
    float e  = pzem->energy();
    float f  = pzem->frequency();
    float pf = pzem->pf();

    // Check voltage validity
    if (isnan(v) || v < 0 || v > MAX_VOLTAGE) {
        r.voltage = 0.0f;
        r.valid = false;
    } else {
        r.voltage = v;
        r.valid = true;
    }

    // Populate other metrics independently so zero load / floating PF doesn't discard valid voltage
    r.current     = (!isnan(i) && i >= 0 && i <= MAX_CURRENT) ? i : 0.0f;
    r.power       = (!isnan(p) && p >= 0) ? p : 0.0f;
    r.energy      = (!isnan(e) && e >= 0) ? e : 0.0f;
    r.frequency   = (!isnan(f) && f >= 0) ? f : 50.0f;
    r.powerFactor = (!isnan(pf) && pf >= 0) ? pf : 1.0f;

    return r.valid;
}
