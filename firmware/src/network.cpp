#include "network.h"
#include "config.h"
#include "time_provider.h"
#include "storage.h"
#include "billing.h"
#include "display.h"
#include "diagnostics.h"
#include "ota.h"

#include <WiFi.h>
#include <time.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <Preferences.h>

static Preferences netPrefs;
static unsigned long lastRetry = 0;
static unsigned long lastSync = 0;
static unsigned long lastAnonReport = 0;
static bool ntpSynced = false;

// Anonymous R&D metrics accumulators
static float vMin = 999.0f;
static float vMax = 0.0f;
static float vSum = 0.0f;
static float pfSum = 0.0f;
static unsigned long sampleCount = 0;

static String cachedMac = "";
static String cachedSerial = "";
static String cachedClaimCode = "";
static String cachedServerUrl = "";

String getDeviceMac() {
    if (cachedMac.length() == 0) {
        cachedMac = WiFi.macAddress();
    }
    return cachedMac;
}

String getSerialNumber() {
    if (cachedSerial.length() == 0) {
        String mac = getDeviceMac();
        mac.replace(":", "");
        if (mac.length() >= 4) {
            cachedSerial = "LMPTX-" + mac.substring(mac.length() - 4);
        } else {
            cachedSerial = "LMPTX-DEVICE";
        }
    }
    return cachedSerial;
}

String getClaimCode() {
    if (cachedClaimCode.length() == 0) {
        netPrefs.begin("device", false);
        // Default seed code matching database factory registry for C104: 749215
        cachedClaimCode = netPrefs.getString("claim_code", "749215");
        netPrefs.end();
    }
    return cachedClaimCode;
}

String getServerUrl() {
    if (cachedServerUrl.length() == 0) {
        netPrefs.begin("device", false);
        cachedServerUrl = netPrefs.getString("server_url", FALLBACK_SERVER_URL);
        netPrefs.end();
    }
    return cachedServerUrl;
}

void resetWiFiCredentials() {
    WiFiManager wm;
    wm.resetSettings();
    Serial.println("[NET] WiFi settings reset to factory defaults");
}

static void configModeCallback(WiFiManager *myWiFiManager) {
    Serial.println("[NET] Entered WiFiManager AP Mode");
    Serial.printf("[NET] Connect to AP: %s\n", myWiFiManager->getConfigPortalSSID().c_str());
    Serial.printf("[NET] IP Address: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("[NET] Serial Number: %s | Claim Code: %s\n", 
                  getSerialNumber().c_str(), getClaimCode().c_str());

    showSetupScreen(myWiFiManager->getConfigPortalSSID().c_str(), getClaimCode().c_str());
}

void initNetwork() {
    WiFi.mode(WIFI_STA);

    WiFiManager wm;
    wm.setAPCallback(configModeCallback);
    wm.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT);

    String serial = getSerialNumber();
    String code = getClaimCode();
    String server = getServerUrl();

    // Custom HTML in the setup portal showing device credentials
    String credsHtml = "<div style='background:#f0f4ff;padding:15px;border-radius:10px;margin:15px 0;border:1px solid #c7d2fe;font-family:sans-serif;'>"
                       "<h3 style='margin:0 0 10px 0;color:#3730a3;'>Lampitex Security Credentials</h3>"
                       "<p style='margin:5px 0;'><b>Serial Number:</b> <span style='font-family:monospace;font-size:16px;'>" + serial + "</span></p>"
                       "<p style='margin:5px 0;'><b>Security Claim Code:</b> <span style='font-family:monospace;font-size:22px;color:#1e40af;letter-spacing:3px;font-weight:bold;'>" + code + "</span></p>"
                       "<p style='margin:8px 0 0 0;font-size:12px;color:#6b7280;'>Write down this 6-digit claim code. You will need it to link this meter to your dashboard.</p>"
                       "</div>";

    WiFiManagerParameter customHtml(credsHtml.c_str());
    WiFiManagerParameter customServerUrl("server", "Dashboard API URL", server.c_str(), 120);

    wm.addParameter(&customHtml);
    wm.addParameter(&customServerUrl);

    String apName = AP_SSID_PREFIX + serial.substring(serial.length() > 4 ? serial.length() - 4 : 0);
    Serial.printf("[NET] Attempting WiFi autoConnect on %s...\n", apName.c_str());

    bool connected = wm.autoConnect(apName.c_str());

    if (connected) {
        Serial.printf("[NET] WiFi Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        showWiFiConnected(WiFi.localIP().toString().c_str());

        // Save server URL if changed
        if (strlen(customServerUrl.getValue()) > 0 && String(customServerUrl.getValue()) != server) {
            netPrefs.begin("device", false);
            netPrefs.putString("server_url", customServerUrl.getValue());
            cachedServerUrl = customServerUrl.getValue();
            netPrefs.end();
            Serial.printf("[NET] Updated Server URL to: %s\n", cachedServerUrl.c_str());
        }
    } else {
        Serial.println("[NET] WiFi config timeout. Running in offline/autonomous metering mode.");
    }
}

bool isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

static void sendAnonymousReport() {
    if (!isWiFiConnected()) return;

    HTTPClient http;
    String url = getServerUrl() + "/api/anonymous";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    float vAvg = (sampleCount > 0) ? (vSum / sampleCount) : 0;
    float pfAvg = (sampleCount > 0) ? (pfSum / sampleCount) : 0;

    StaticJsonDocument<384> doc;
    doc["device_id"] = getDeviceMac();
    doc["fw_version"] = FW_VERSION;
    doc["uptime_seconds"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["voltage_avg"] = serialized(String(vAvg, 1));
    doc["voltage_min"] = serialized(String(vMin < 900 ? vMin : 0, 1));
    doc["voltage_max"] = serialized(String(vMax, 1));
    doc["power_factor_avg"] = serialized(String(pfAvg, 2));
    doc["relay_cycle_count"] = 0;
    doc["error_count"] = 0;

    String payload;
    serializeJson(doc, payload);

    int httpResponseCode = http.POST(payload);
    if (httpResponseCode == 200) {
        Serial.println("[NET] Anonymous R&D telemetry report sent successfully");
        // Reset accumulators for next interval
        vMin = 999.0f;
        vMax = 0.0f;
        vSum = 0.0f;
        pfSum = 0.0f;
        sampleCount = 0;
    } else {
        Serial.printf("[NET] Anonymous report error (%d)\n", httpResponseCode);
    }
    http.end();
}

static void syncTelemetry(const MeterReading &live) {
    if (!isWiFiConnected()) return;

    // Track R&D metrics
    if (live.valid && live.voltage > 0) {
        if (live.voltage < vMin) vMin = live.voltage;
        if (live.voltage > vMax) vMax = live.voltage;
        vSum += live.voltage;
        pfSum += live.powerFactor;
        sampleCount++;
    }

    HTTPClient http;
    String url = getServerUrl() + "/api/telemetry";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String payload = "{";
    payload += "\"device_id\":\"" + getDeviceMac() + "\",";
    payload += "\"balance_after\":" + String(getBalance()) + ",";
    payload += "\"relay_on\":" + String(isRelayOn() ? "true" : "false") + ",";
    payload += "\"readings\":[";

    // Send the LIVE reading
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
        if (!err && doc.containsKey("ota") && doc["ota"] == true) {
            const char* customOtaUrl = doc["ota_url"].as<const char*>();
            Serial.println("[NET] Cloud triggered Over-The-Air firmware update!");
            triggerOTA(customOtaUrl);
        }
    }
    http.end();
}

void maintainNetwork(const MeterReading &live) {
    if (isWiFiConnected()) {
        if (!ntpSynced) syncNTP();
        
        // Fast sync telemetry every 2 seconds
        if (millis() - lastSync > SYNC_INTERVAL_MS) {
            lastSync = millis();
            syncTelemetry(live);
        }

        // Anonymous R&D telemetry report every 1 hour
        if (millis() - lastAnonReport > RND_TELEMETRY_INTERVAL) {
            lastAnonReport = millis();
            sendAnonymousReport();
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
