#include "diagnostics.h"
#include "config.h"
#include "storage.h"
#include <WiFi.h>
#include <esp_system.h>

// Anonymous — NO PII, no balances, no tenant data
static char deviceId[18] = {0};
static unsigned long lastDiag = 0;

// Simple error counter map
#define MAX_ERROR_TYPES 10
static char   errKeys[MAX_ERROR_TYPES][20];
static int    errVals[MAX_ERROR_TYPES];
static int    errCount = 0;

void initDiagnostics() {
    uint64_t mac = ESP.getEfuseMac();
    snprintf(deviceId, sizeof(deviceId), "%02X:%02X:%02X:%02X:%02X:%02X",
             (uint8_t)(mac), (uint8_t)(mac >> 8), (uint8_t)(mac >> 16),
             (uint8_t)(mac >> 24), (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));
    memset(errKeys, 0, sizeof(errKeys));
    memset(errVals, 0, sizeof(errVals));
    Serial.printf("[DIAG] Device ID: %s\n", deviceId);
}

void incrementErrorCounter(const char* errorType) {
    for (int i = 0; i < errCount; i++) {
        if (strcmp(errKeys[i], errorType) == 0) {
            errVals[i]++;
            return;
        }
    }
    if (errCount < MAX_ERROR_TYPES) {
        strncpy(errKeys[errCount], errorType, 19);
        errVals[errCount] = 1;
        errCount++;
    }
}

void reportDiagnostics() {
    if (millis() - lastDiag < DIAG_INTERVAL_MS) return;
    lastDiag = millis();

    int rssi = WiFi.isConnected() ? WiFi.RSSI() : 0;

    Serial.printf("[DIAG] {\"device_id\":\"%s\",\"fw\":\"%s\","
                  "\"reset_reason\":%d,\"free_heap\":%u,"
                  "\"uptime_s\":%lu,\"wifi_rssi\":%d,"
                  "\"buf_overflow\":%lu,\"errors\":{",
                  deviceId, FW_VERSION,
                  (int)esp_reset_reason(), ESP.getFreeHeap(),
                  (unsigned long)(millis() / 1000), rssi,
                  (unsigned long)getBufferOverflowCount());
    for (int i = 0; i < errCount; i++) {
        Serial.printf("\"%s\":%d%s", errKeys[i], errVals[i],
                      i < errCount - 1 ? "," : "");
    }
    Serial.println("}}");
}
