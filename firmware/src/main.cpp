// ─────────────────────────────────────────────────────────
//  Lampitex Prepaid Smart Meter — Production Firmware v1.0
// ─────────────────────────────────────────────────────────
#include <Arduino.h>
#include "config.h"
#include "time_provider.h"
#include "meter.h"
#include "billing.h"
#include "display.h"
#include "storage.h"
#include "network.h"
#include "ota.h"
#include "diagnostics.h"

// ── Timing ───────────────────────────────────────────────
static unsigned long lastRead      = 0;
static unsigned long lastAggregate = 0;
static bool otaMarkedValid         = false;
static bool firstSuccessfulRead    = false;

// ── Latest Reading ───────────────────────────────────────
static MeterReading latestReading;

// ── Aggregation accumulators ─────────────────────────────
static float aggPowerSum  = 0;
static float aggPFSum     = 0;
static int   aggSamples   = 0;
static float aggEnergyStart = -1;

// ── Serial command buffer ────────────────────────────────
static char cmdBuf[64];
static int  cmdPos = 0;

// ─────────────────────────────────────────────────────────
static void processSerialCommand(const char* cmd) {
    // RECHARGE <amount>
    if (strncmp(cmd, "RECHARGE ", 9) == 0) {
        float amt = atof(cmd + 9);
        if (amt > 0) {
            applyRecharge(amt);
            addLedgerEntry("recharge", amt, getBalance(), "serial");
            Serial.printf("OK: Recharged Rs %.2f → Balance Rs %.2f\n",
                          amt, getBalance());
        } else {
            Serial.println("ERR: Amount must be positive");
        }
    }
    // STATUS
    else if (strcmp(cmd, "STATUS") == 0) {
        Serial.printf("Balance: Rs %.2f | Relay: %s | State: %d\n",
                      getBalance(), isRelayOn() ? "ON" : "OFF",
                      getBalanceState());
        Serial.printf("Buffered: %d | Ledger: %d | Time: %s\n",
                      getBufferedReadingCount(), getLedgerCount(),
                      getTimeSourceStr());
    }
    // TARIFF (informational)
    else if (strcmp(cmd, "TARIFF") == 0) {
        Serial.printf("Grid: Rs %.2f/kWh | DG: Rs %.2f/kWh\n",
                      DEFAULT_TARIFF_GRID, DEFAULT_TARIFF_DG);
    }
    // HELP
    else {
        Serial.println("Commands: RECHARGE <amt> | STATUS | TARIFF | HELP");
    }
}

// ─────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.printf("\n===== Lampitex v%s =====\n", FW_VERSION);

    initTimeProvider();
    initStorage();
    initMeter();
    initBilling();
    initDisplay();
    initNetwork();
    initOTA();
    initDiagnostics();

    memset(&latestReading, 0, sizeof(latestReading));

    Serial.println("[MAIN] All subsystems initialised");
    Serial.println("Type HELP for serial commands\n");
}

// ─────────────────────────────────────────────────────────
void loop() {
    // ── 1. Network maintenance (non-blocking) ────────────
    maintainNetwork(latestReading);

    // ── 2. Meter read every 2 seconds ────────────────────
    if (millis() - lastRead >= READ_INTERVAL_MS) {
        lastRead = millis();

        bool ok = readMeter(latestReading);

        if (!ok) {
            incrementErrorCounter("pzem_read_fail");
        } else if (!firstSuccessfulRead) {
            firstSuccessfulRead = true;
        }

        // Billing always runs (uses valid flag internally)
        updateBilling(latestReading);

        // Display always updates
        updateDisplay(latestReading, getBalance(), getBalanceState(), isRelayOn());

        // Flash write policy
        if (shouldSaveBalance()) {
            saveBalanceNow("policy");
        }

        // Accumulate for 5-min aggregate
        if (ok) {
            aggPowerSum += latestReading.power;
            aggPFSum    += latestReading.powerFactor;
            aggSamples++;
            if (aggEnergyStart < 0) aggEnergyStart = latestReading.energy;
        }

        // ── 3. Aggregate every 5 minutes ─────────────────
        if (millis() - lastAggregate >= AGGREGATE_INTERVAL_MS) {
            lastAggregate = millis();
            if (aggSamples > 0) {
                AggregatedReading agg;
                memset(&agg, 0, sizeof(agg));
                agg.timestamp   = getEpoch();
                strncpy(agg.timeSource, getTimeSourceStr(), 11);
                agg.avgPower    = aggPowerSum / aggSamples;
                agg.avgPF       = aggPFSum / aggSamples;
                agg.energyDelta = (ok ? latestReading.energy : aggEnergyStart) - aggEnergyStart;
                agg.balanceAfter = getBalance();
                agg.relayOn     = isRelayOn();

                addReadingToBuffer(agg);

                // Reset accumulators
                aggPowerSum = aggPFSum = 0;
                aggSamples  = 0;
                aggEnergyStart = -1;
            }
        }
    }

    // ── 4. OTA check ─────────────────────────────────────
    checkOTA();

    // Mark OTA valid after first successful read + time sync
    if (!otaMarkedValid && firstSuccessfulRead && isWiFiConnected()) {
        markOTAValid();
        otaMarkedValid = true;
    }

    // ── 5. Diagnostics ───────────────────────────────────
    reportDiagnostics();

    // ── 6. Serial commands ───────────────────────────────
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (cmdPos > 0) {
                cmdBuf[cmdPos] = '\0';
                processSerialCommand(cmdBuf);
                cmdPos = 0;
            }
        } else if (cmdPos < (int)sizeof(cmdBuf) - 1) {
            cmdBuf[cmdPos++] = c;
        }
    }
}
