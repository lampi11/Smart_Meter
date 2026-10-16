#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <PZEM004Tv30.h>
#include <Preferences.h>
#include "time_provider.h"

// --- Fixed Hardware Contract Pins ---
#define PZEM_RX_PIN 16
#define PZEM_TX_PIN 17
#define RELAY_PIN 4
#define I2C_SDA 21
#define I2C_SCL 22

// --- Settings ---
const float TARIFF_GRID = 8.0; // Rs/kWh
const float EMERGENCY_CREDIT_LIMIT = -30.0;
const int NO_CUT_START_HOUR = 22; // 10 PM
const int NO_CUT_END_HOUR = 7;    // 7 AM

// Objects
PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);
LiquidCrystal_I2C* lcd = nullptr; 
Preferences prefs;

// --- State ---
float currentBalance = 0.0;
float unsavedConsumptionRs = 0.0;
float lastEnergyKwh = -1.0;
bool relayState = true;
unsigned long lastReadTime = 0;
unsigned long lastSaveTime = 0;

void saveBalance(const char* reason) {
    prefs.putFloat("balance", currentBalance);
    unsavedConsumptionRs = 0.0;
    lastSaveTime = millis();
    Serial.printf("Saved balance: Rs %.2f (Reason: %s)\n", currentBalance, reason);
}

void setRelay(bool state, const char* reason) {
    if (relayState == state) return;
    relayState = state;
    digitalWrite(RELAY_PIN, state ? HIGH : LOW);
    saveBalance("Relay state changed");
    
    // Relay Verification (Wait 1 second for SSR to open, then verify)
    if (!state) {
        delay(1000);
        float current = pzem.current();
        if (!isnan(current) && current > 0.5) {
            Serial.println("DIAGNOSTIC: relay_verify_failed! Load is still drawing current.");
        }
    }
}

bool inNoCutWindow() {
    if (!isTimeSynced() && getTimeSource() == TIME_NONE) return false;
    
    uint32_t epoch = getEpoch();
    // Convert UTC epoch to IST hour (UTC + 5:30 = 19800 seconds)
    int hourIST = ((epoch + 19800) % 86400) / 3600;
    
    if (NO_CUT_START_HOUR > NO_CUT_END_HOUR) {
        return (hourIST >= NO_CUT_START_HOUR || hourIST < NO_CUT_END_HOUR);
    } else {
        return (hourIST >= NO_CUT_START_HOUR && hourIST < NO_CUT_END_HOUR);
    }
}

void setup() {
    Serial.begin(115200);
    Wire.begin(I2C_SDA, I2C_SCL);
    
    // Scan I2C for LCD
    Wire.beginTransmission(0x27);
    if (Wire.endTransmission() == 0) {
        lcd = new LiquidCrystal_I2C(0x27, 16, 2);
    } else {
        Wire.beginTransmission(0x3F);
        if (Wire.endTransmission() == 0) {
            lcd = new LiquidCrystal_I2C(0x3F, 16, 2);
        } else {
            Serial.println("LCD NOT FOUND on 0x27 or 0x3F!");
            lcd = new LiquidCrystal_I2C(0x27, 16, 2); // fallback
        }
    }
    lcd->init();
    lcd->backlight();
    
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, HIGH); // Default ON

    initTimeProvider();
    
    // Initialize NVS storage for billing
    prefs.begin("billing", false);
    
    // For testing purposes, we give a default balance of Rs 100 on the very first boot.
    currentBalance = prefs.getFloat("balance", 100.0); 
    
    Serial.println("\n--- Step 2: Time Provider & Billing Core ---");
    Serial.printf("Recovered Balance: Rs %.2f\n", currentBalance);
}

void loop() {
    if (millis() - lastReadTime >= 2000) {
        lastReadTime = millis();
        
        float v = pzem.voltage();
        float i = pzem.current();
        float p = pzem.power();
        float e = pzem.energy(); // cumulative kWh
        
        lcd->clear();
        lcd->setCursor(0, 0);
        

        
        
        if (isnan(v)) {

            // Hardware failure state (or no AC power to PZEM)
            v = 0.0; i = 0.0; p = 0.0;
        }
        
        // --- 1. Consumption Math ---
        if (lastEnergyKwh >= 0 && e > lastEnergyKwh) {
            float deltaKwh = e - lastEnergyKwh;
            float cost = deltaKwh * TARIFF_GRID;
            currentBalance -= cost;
            unsavedConsumptionRs += cost;
        }
        lastEnergyKwh = e;
        
        // --- 2. Flash Write Policy (Section 3.2) ---
        if (unsavedConsumptionRs >= 0.50) {
            saveBalance("Consumption crossed Rs 0.50");
        } else if (millis() - lastSaveTime >= 300000) {
            saveBalance("5min Heartbeat");
        }
        
        // --- 3. Cutoff Logic (Section 3.3) ---
        if (currentBalance <= EMERGENCY_CREDIT_LIMIT) {
            if (relayState && !inNoCutWindow()) {
                setRelay(false, "Cutoff triggered (Credit Exhausted)");
            }
        } else if (currentBalance > 0) {
            if (!relayState) {
                setRelay(true, "Power Restored (Positive Balance)");
            }
        }
        
        // --- 4. LCD Status States ---
        lcd->printf("V:%.0f I:%.1f P:%.0f", v, i, p);
        lcd->setCursor(0, 1);
        
        if (relayState) {
            if (currentBalance <= 20.0) {
                lcd->print("URGENT RCHG:");
                lcd->printf("%.0f", currentBalance);
            } else if (currentBalance <= 50.0) {
                lcd->print("LOW BAL: Rs ");
                lcd->printf("%.0f", currentBalance);
            } else {
                lcd->print("Bal: Rs ");
                lcd->printf("%.2f", currentBalance);
            }
        } else {
            if (inNoCutWindow()) {
                 lcd->print("NIGHT Rs ");
                 lcd->printf("%.1f", currentBalance);
            } else {
                 lcd->print("CUTOFF! Rchg Req");
            }
        }
    }
}
