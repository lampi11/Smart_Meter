#include "network.h"
#include "config.h"
#include "secrets.h"
#include "time_provider.h"
#include "storage.h"
#include "billing.h"
#include "display.h"
#include <WiFi.h>
#include <time.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static unsigned long lastRetry = 0;
static unsigned long lastSync = 0;
static bool ntpSynced = false;

void initNetwork() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.printf("[NET] Connecting to %s...\n", WIFI_SSID);
}

bool isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

static void syncTelemetry(const MeterReading &live) {
    if (!isWiFiConnected()) return;

    HTTPClient http;
    http.begin(LOCAL_API_URL);
    http.addHeader("Content-Type", "application/json");

    String payload = "{";
    payload += "\"device_id\":\"" + WiFi.macAddress() + "\",";
    payload += "\"balance_after\":" + String(getBalance()) + ",";
    payload += "\"relay_on\":" + String(isRelayOn() ? "true" : "false") + ",";
    payload += "\"readings\":[";

    // Send the LIVE reading!
    payload += "{";
    payload += "\"timestamp\":" + String(getEpoch()) + ",";
    payload += "\"voltage\":" + String(live.valid ? live.voltage : 0) + ",";
    payload += "\"current\":" + String(live.valid ? live.current : 0) + ",";
    payload += "\"power\":" + String(live.valid ? live.power : 0) + ",";
    payload += "\"energy\":" + String(live.valid ? live.energy : 0) + ",";
    payload += "\"balance_after\":" + String(getBalance()) + ",";
    payload += "\"relay_on\":" + String(isRelayOn() ? "true" : "false");
    payload += "}";

    payload += "]}";

    int httpResponseCode = http.POST(payload);
    if (httpResponseCode == 200) {
        // Parse response for custom LCD message & recharge
        String response = http.getString();
        StaticJsonDocument<256> doc;
        DeserializationError err = deserializeJson(doc, response);
        
        if (!err && doc.containsKey("recharge")) {
            float amt = doc["recharge"];
            if (amt != 0) {
                Serial.printf("[NET] Received Recharge: %.2f\n", amt);
                applyRecharge(amt);
            }
        }
        if (!err && doc["message"].is<const char*>()) {
            const char* msg = doc["message"];
            if (strlen(msg) > 0) {
                Serial.printf("[NET] Received LCD Message: %s\n", msg);
                showCustomMessage(msg);
            }
        }
    }
    http.end();
}

void maintainNetwork(const MeterReading &live) {
    if (isWiFiConnected()) {
        if (!ntpSynced) syncNTP();
        
        // Sync telemetry/heartbeat every 2 seconds if connected (ULTRA FAST DASHBOARD)
        if (millis() - lastSync > 2000) {
            lastSync = millis();
            syncTelemetry(live);
        }
        return;
    }
    if (millis() - lastRetry > WIFI_RETRY_INTERVAL_MS) {
        lastRetry = millis();
        WiFi.reconnect();
    }
}

void syncNTP() {
    configTime(IST_OFFSET_SECONDS, 0, NTP_SERVER);
    struct tm t;
    if (getLocalTime(&t, 5000)) {
        time_t epoch = mktime(&t) - IST_OFFSET_SECONDS; // store as UTC
        setTimeFromNTP((uint32_t)epoch);
        ntpSynced = true;
    }
}
