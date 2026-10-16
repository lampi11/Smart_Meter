#include "billing.h"
#include "config.h"
#include "time_provider.h"
#include <Preferences.h>
#include "meter.h"
#include <PZEM004Tv30.h>

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
    float cur = getPzemCurrent();
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

    // First reading initialization
    if (lastEnergyKwh < 0) {
        lastEnergyKwh = reading.energy;
        return;
    }

    // --- energy-delta billing with plausibility safeguard ---
    if (reading.energy >= lastEnergyKwh) {
        float deltaKwh = reading.energy - lastEnergyKwh;

        // Reject physically impossible jumps (e.g. UART noise or corrupted PZEM registers)
        if (deltaKwh > MAX_ENERGY_DELTA_KWH) {
            Serial.printf("[BILLING] Corrupt energy jump rejected! delta=%.3f kWh (last=%.3f, now=%.3f)\n",
                          deltaKwh, lastEnergyKwh, reading.energy);
            // Resync baseline to new energy without deducting
            lastEnergyKwh = reading.energy;
            return;
        }

        float cost = deltaKwh * DEFAULT_TARIFF_GRID;
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
    saveBalanceNow(amount >= 0 ? "recharge" : "deduction");

    if (!relayOn && currentBalance > 0) {
        setRelay(true);
        saveBalanceNow("restored_after_recharge");
    } else if (relayOn && currentBalance <= EMERGENCY_CREDIT_LIMIT && !inNoCutWindow()) {
        setRelay(false);
        saveBalanceNow("cutoff_after_deduction");
        verifyRelay();
    }
    Serial.printf("[BILLING] Fund adjustment: %+.2f → Balance Rs %.2f (Relay %s)\n", 
                  amount, currentBalance, relayOn ? "ON" : "OFF");
}

void setBalanceDirect(float newBal) {
    currentBalance = newBal;
    saveBalanceNow("direct_set");

    if (!relayOn && currentBalance > 0) {
        setRelay(true);
        saveBalanceNow("restored_after_set");
    } else if (relayOn && currentBalance <= EMERGENCY_CREDIT_LIMIT && !inNoCutWindow()) {
        setRelay(false);
        saveBalanceNow("cutoff_after_set");
        verifyRelay();
    }
    Serial.printf("[BILLING] Direct balance set to: Rs %.2f (Relay %s)\n", 
                  currentBalance, relayOn ? "ON" : "OFF");
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
