#include "billing.h"
#include "config.h"
#include "time_provider.h"
#include <Preferences.h>
#include <PZEM004Tv30.h>

extern PZEM004Tv30 pzem;   // defined in meter.cpp

static Preferences billingPrefs;
static float currentBalance       = 0;
static float lastEnergyKwh        = -1.0f;
static float unsavedConsumptionRs = 0;
static unsigned long lastSaveTime = 0;
static bool  relayOn              = true;

// ── helpers ──────────────────────────────────────────────
static void setRelay(bool on) {
    relayOn = on;
    digitalWrite(RELAY_PIN, on ? RELAY_ON_STATE : RELAY_OFF_STATE);
    Serial.printf("[BILLING] Relay %s\n", on ? "ON" : "OFF");
}

static void verifyRelay() {
    delay(RELAY_VERIFY_DELAY_MS);
    float cur = pzem.current();
    if (!isnan(cur) && cur > RELAY_VERIFY_CURRENT_THRESHOLD) {
        Serial.println("[DIAG] relay_verify_failed — current still flowing after cut");
    }
}

// ── public API ───────────────────────────────────────────
void initBilling() {
    billingPrefs.begin("billing", false);
    currentBalance = billingPrefs.getFloat("balance", DEFAULT_BALANCE);
    lastEnergyKwh  = billingPrefs.getFloat("lastE", -1.0f);
    lastSaveTime   = millis();

    pinMode(RELAY_PIN, OUTPUT);
    setRelay(true);   // default power-on
    Serial.printf("[BILLING] Boot balance: Rs %.2f\n", currentBalance);
}

void updateBilling(const MeterReading &reading) {
    if (!reading.valid) return;

    // --- energy-delta billing ---
    if (lastEnergyKwh >= 0 && reading.energy >= lastEnergyKwh) {
        float deltaKwh = reading.energy - lastEnergyKwh;
        float cost     = deltaKwh * DEFAULT_TARIFF_GRID;
        currentBalance       -= cost;
        unsavedConsumptionRs += cost;
    }
    lastEnergyKwh = reading.energy;

    // --- cutoff / restore logic ---
    if (currentBalance <= EMERGENCY_CREDIT_LIMIT && relayOn && !inNoCutWindow()) {
        setRelay(false);
        saveBalanceNow("cutoff");
        verifyRelay();
    } else if (currentBalance > 0 && !relayOn) {
        setRelay(true);
        saveBalanceNow("restored");
    }
}

bool shouldSaveBalance() {
    return (unsavedConsumptionRs >= SAVE_THRESHOLD_RS) ||
           (millis() - lastSaveTime >= SAVE_HEARTBEAT_MS);
}

void saveBalanceNow(const char* reason) {
    billingPrefs.putFloat("balance", currentBalance);
    billingPrefs.putFloat("lastE",   lastEnergyKwh);
    unsavedConsumptionRs = 0;
    lastSaveTime = millis();
    Serial.printf("[BILLING] Saved Rs %.2f (%s)\n", currentBalance, reason);
}

void applyRecharge(float amount) {
    
    currentBalance += amount;
    saveBalanceNow("recharge");
    if (!relayOn && currentBalance > 0) {
        setRelay(true);
        saveBalanceNow("restored_after_recharge");
    }
    Serial.printf("[BILLING] Recharged +%.2f → Rs %.2f\n", amount, currentBalance);
}

float        getBalance()            { return currentBalance; }
float        getUnsavedConsumption() { return unsavedConsumptionRs; }
bool         isRelayOn()             { return relayOn; }

BalanceState getBalanceState() {
    if (currentBalance > LOW_BALANCE_WARN)    return BAL_NORMAL;
    if (currentBalance > URGENT_BALANCE_WARN) return BAL_LOW;
    if (currentBalance > 0)                   return BAL_URGENT;
    if (currentBalance > EMERGENCY_CREDIT_LIMIT) return BAL_EMERGENCY;
    return BAL_CUTOFF;
}

bool inNoCutWindow() {
    int h = getISTHour();
    if (h < 0) return false;   // time unknown → allow cut
    if (NO_CUT_START_HOUR > NO_CUT_END_HOUR)
        return (h >= NO_CUT_START_HOUR || h < NO_CUT_END_HOUR);
    return (h >= NO_CUT_START_HOUR && h < NO_CUT_END_HOUR);
}
