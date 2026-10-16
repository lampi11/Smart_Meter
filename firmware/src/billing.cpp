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
static bool  relayActiveLow       = (RELAY_ON_STATE == LOW);

// Anti-Blackout Survival Mode state
static bool  survivalTripped      = false;
static unsigned long survivalTripTime = 0;

// ── helpers ──────────────────────────────────────────────
static void setRelay(bool on) {
    relayOn = on;
    int pinState = on ? (relayActiveLow ? LOW : HIGH) : (relayActiveLow ? HIGH : LOW);
    digitalWrite(RELAY_PIN, pinState);
    Serial.printf("[BILLING] Relay %s (GPIO %d = %s, Active-%s)\n", 
                  on ? "ON" : "OFF", RELAY_PIN, 
                  pinState == HIGH ? "HIGH" : "LOW",
                  relayActiveLow ? "LOW" : "HIGH");
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
    relayActiveLow = billingPrefs.getBool("actLow", (RELAY_ON_STATE == LOW));
    lastSaveTime   = millis();

    pinMode(RELAY_PIN, OUTPUT);
    // Immediately energize relay if balance is good (> emergency limit or in grace period)
    setRelay(currentBalance > EMERGENCY_CREDIT_LIMIT || inNoCutWindow());
    Serial.printf("[BILLING] Boot balance: Rs %.2f | Relay: %s (Active-%s)\n", 
                  currentBalance, relayOn ? "ON" : "OFF", relayActiveLow ? "LOW" : "HIGH");
}

void setRelayPolarity(bool activeLow) {
    if (relayActiveLow != activeLow) {
        relayActiveLow = activeLow;
        billingPrefs.putBool("actLow", activeLow);
        Serial.printf("[BILLING] Relay polarity switched to Active-%s\n", activeLow ? "LOW" : "HIGH");
        setRelay(relayOn);
    }
}

bool isRelayActiveLow() {
    return relayActiveLow;
}

bool isSurvivalMode() {
    // Anti-Blackout Survival Mode: when balance <= 10 Rs, or <= 0 during night hours
    return (currentBalance <= SURVIVAL_BALANCE_THRESHOLD && currentBalance > EMERGENCY_CREDIT_LIMIT) ||
           (currentBalance <= 0 && inNoCutWindow());
}

bool isSurvivalTripped() {
    return survivalTripped;
}

void updateBilling(const MeterReading &reading) {
    bool inSurvival = isSurvivalMode();

    if (inSurvival) {
        // --- Anti-Blackout Survival Mode Throttling (0.5A / 110W) ---
        if (survivalTripped) {
            // Check if cooldown expired to allow retry if tenant disconnected heavy load
            if (millis() - survivalTripTime >= SURVIVAL_TRIP_COOLDOWN_MS) {
                Serial.println("[SURVIVAL] Trip cooldown expired. Restoring power for fan/light...");
                survivalTripped = false;
                setRelay(true);
            } else {
                if (relayOn) setRelay(false);
            }
        } else {
            // In survival mode: if load exceeds 0.5A (or 120W), trip the relay!
            if (reading.valid && (reading.current > SURVIVAL_CURRENT_LIMIT_A || reading.power > SURVIVAL_POWER_LIMIT_W)) {
                Serial.printf("[SURVIVAL] Overload trip! Current=%.2fA (limit %.2fA), Power=%.1fW\n",
                              reading.current, SURVIVAL_CURRENT_LIMIT_A, reading.power);
                survivalTripped = true;
                survivalTripTime = millis();
                setRelay(false);
                saveBalanceNow("survival_overload_trip");
            } else if (!relayOn) {
                // Low power load allowed (fan & light)
                setRelay(true);
            }
        }
    } else {
        // --- Normal Operation (Balance > 10 Rs): Full power forwarding ---
        survivalTripped = false;

        if (currentBalance > SURVIVAL_BALANCE_THRESHOLD) {
            if (!relayOn) {
                setRelay(true);
                saveBalanceNow("full_power_restored");
            }
        } else if (currentBalance <= EMERGENCY_CREDIT_LIMIT && !inNoCutWindow()) {
            if (relayOn) {
                setRelay(false);
                saveBalanceNow("cutoff");
                verifyRelay();
            }
        }
    }

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
            lastEnergyKwh = reading.energy;
            return;
        }

        float cost = deltaKwh * DEFAULT_TARIFF_GRID;
        currentBalance       -= cost;
        unsavedConsumptionRs += cost;
    }
    lastEnergyKwh = reading.energy;

    // Check cutoff again if consumption dropped balance below emergency limit
    if (currentBalance <= EMERGENCY_CREDIT_LIMIT && relayOn && !inNoCutWindow()) {
        setRelay(false);
        saveBalanceNow("cutoff");
        verifyRelay();
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
    survivalTripped = false; // Reset overload trip on recharge
    saveBalanceNow(amount >= 0 ? "recharge" : "deduction");

    if (currentBalance > SURVIVAL_BALANCE_THRESHOLD) {
        setRelay(true);
    } else if (currentBalance <= EMERGENCY_CREDIT_LIMIT && !inNoCutWindow()) {
        setRelay(false);
        verifyRelay();
    } else {
        setRelay(true); // Survival mode allows fan/light
    }
    Serial.printf("[BILLING] Fund adjustment: %+.2f → Balance Rs %.2f (Relay %s)\n", 
                  amount, currentBalance, relayOn ? "ON" : "OFF");
}

void setBalanceDirect(float newBal) {
    currentBalance = newBal;
    survivalTripped = false; // Reset overload trip on balance set
    saveBalanceNow("direct_set");

    if (currentBalance > SURVIVAL_BALANCE_THRESHOLD) {
        setRelay(true);
    } else if (currentBalance <= EMERGENCY_CREDIT_LIMIT && !inNoCutWindow()) {
        setRelay(false);
        verifyRelay();
    } else {
        setRelay(true);
    }
    Serial.printf("[BILLING] Direct balance set to: Rs %.2f (Relay %s)\n", 
                  currentBalance, relayOn ? "ON" : "OFF");
}

float        getBalance()            { return currentBalance; }
float        getUnsavedConsumption() { return unsavedConsumptionRs; }
bool         isRelayOn()             { return relayOn; }

BalanceState getBalanceState() {
    if (isSurvivalMode()) {
        return BAL_SURVIVAL;
    }
    if (currentBalance > LOW_BALANCE_WARN)    return BAL_NORMAL;
    if (currentBalance > URGENT_BALANCE_WARN) return BAL_LOW;
    if (currentBalance > SURVIVAL_BALANCE_THRESHOLD) return BAL_URGENT;
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
