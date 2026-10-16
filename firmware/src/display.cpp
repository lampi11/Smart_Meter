#include "display.h"
#include "config.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

static LiquidCrystal_I2C* lcd = nullptr;
static unsigned long customMsgEnd = 0;

void initDisplay() {
    Wire.begin(I2C_SDA, I2C_SCL);

    // Auto-detect LCD address
    uint8_t addr = 0x27;
    Wire.beginTransmission(0x27);
    if (Wire.endTransmission() != 0) {
        Wire.beginTransmission(0x3F);
        if (Wire.endTransmission() == 0) addr = 0x3F;
        else Serial.println("[DISPLAY] LCD not found on 0x27 or 0x3F!");
    }
    lcd = new LiquidCrystal_I2C(addr, 16, 2);
    lcd->init();
    lcd->backlight();

    // Splash screen
    lcd->setCursor(0, 0);
    lcd->print("Lampitex  v");
    lcd->print(FW_VERSION);
    lcd->setCursor(0, 1);
    lcd->print("Starting...");
    delay(1500);
    lcd->clear();
    Serial.printf("[DISPLAY] LCD on 0x%02X\n", addr);
}

void showCustomMessage(const char* msg) {
    if (!lcd) return;
    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print("*** MESSAGE ***");
    lcd->setCursor(0, 1);
    
    // Print up to 16 chars
    char buf[17] = {0};
    strncpy(buf, msg, 16);
    lcd->print(buf);
    
    customMsgEnd = millis() + 3000; // Show for 3 seconds
}

void updateDisplay(const MeterReading &reading, float balance,
                   BalanceState state, bool relayOn) {
    if (!lcd) return;
    
    // Skip normal update if custom message is active
    if (millis() < customMsgEnd) return;

    lcd->clear();

    // Line 0: V / I / P
    lcd->setCursor(0, 0);
    lcd->printf("V:%.0f I:%.1f P:%.0f",
                reading.voltage, reading.current, reading.power);

    // Line 1: balance state
    lcd->setCursor(0, 1);
    switch (state) {
        case BAL_NORMAL:
            lcd->printf("Bal: Rs %.2f", balance);
            break;
        case BAL_LOW:
            lcd->printf("LOW BAL: Rs %.0f", balance);
            break;
        case BAL_URGENT:
            lcd->printf("URGENT RCHG:%.0f", balance);
            break;
        case BAL_EMERGENCY:
            lcd->printf("EMRG Rs %.1f", balance);
            break;
        case BAL_CUTOFF:
            if (!relayOn) {
                if (inNoCutWindow())
                    lcd->printf("NIGHT Rs %.1f", balance);
                else
                    lcd->print("CUTOFF! Rchg Req");
            } else {
                lcd->printf("EMRG Rs %.1f", balance);
            }
            break;
    }
}
