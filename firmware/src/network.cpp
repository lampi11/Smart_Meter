#include "network.h"
#include "config.h"
#include "time_provider.h"
#include "storage.h"
#include "billing.h"
#include "display.h"
#include "diagnostics.h"
#include "ota.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include "secrets.h"

static Preferences netPrefs;
static unsigned long lastRetry = 0;
static unsigned long lastSync = 0;
static unsigned long lastAnonReport = 0;
static bool ntpSynced = false;
static unsigned long adaptiveSyncInterval = SYNC_INTERVAL_MS;

// Persistent HTTP Keep-Alive client for lightning-fast sub-second telemetry
static HTTPClient syncHttp;
static WiFiClientSecure syncSecureClient;
static WiFiClient syncPlainClient;
static bool syncHttpConnected = false;

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
        cachedServerUrl = netPrefs.getString("server_url", DEFAULT_SERVER_URL);
        netPrefs.end();
    }
    return cachedServerUrl;
}

void resetWiFiCredentials() {
    WiFiManager wm;
    wm.resetSettings();
    WiFi.disconnect(true, true);
    esp_wifi_restore();
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
    // Check if one-time OTA Wi-Fi reset was requested for v1.0.2
    netPrefs.begin("device", false);
    bool resetDone = netPrefs.getBool("v102_reset", false);
    if (!resetDone) {
        Serial.println("[NET] First boot on v1.0.2: Resetting WiFi credentials as requested via OTA!");
        resetWiFiCredentials();
        netPrefs.putBool("v102_reset", true);
    }
    netPrefs.end();

    WiFi.mode(WIFI_STA);

    // 1. Enable Channels 1-13 (India/Europe/Asia standard) - solves missing networks on channels 12 & 13
    wifi_country_t country = {
        .cc = "IN",
        .schan = 1,
        .nchan = 13,
        .max_tx_power = 20,
        .policy = WIFI_COUNTRY_POLICY_MANUAL
    };
    esp_wifi_set_country(&country);

    // 2. Disable modem sleep - eliminates packet latency & drops on weak networks
    WiFi.setSleep(false);

    // 3. Maximum RF transmit power for maximum wall penetration & signal range
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.setAutoReconnect(true);

    WiFiManager wm;
    wm.setAPCallback(configModeCallback);
    wm.setConnectTimeout(12);
    wm.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT);
    wm.setCaptivePortalEnable(true);       // Forces captive portal popup on all mobile OS
    wm.setAPClientCheck(true);             // Don't timeout while phone is actively connected
    wm.setShowStaticFields(true);          // Allows manual Static IP / Gateway entry if DHCP fails
    wm.setShowDnsFields(true);             // Allows custom DNS (e.g. 8.8.8.8)
    wm.setScanDispPerc(true);              // Shows signal % in scan list
    wm.setBreakAfterConfig(true);          // Resumes immediately once credentials saved

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
    Serial.printf("[NET] Attempting WiFi autoConnect on %s (15s timeout)...\n", apName.c_str());

    bool connected = wm.autoConnect(apName.c_str());

    // Fallback: If WiFiManager didn't connect, try configured credentials from secrets.h
    if (!connected && strlen(WIFI_SSID) > 0) {
        Serial.printf("[NET] AutoConnect portal closed/failed. Trying fallback SSID: %s...\n", WIFI_SSID);
        WiFi.disconnect();
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        unsigned long t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) {
            delay(250);
            Serial.print(".");
        }
        Serial.println();
        connected = (WiFi.status() == WL_CONNECTED);
    }

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
        Serial.println("[NET] WiFi not connected. Running in autonomous metering mode with periodic background reconnect.");
    }
}

bool isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

static void sendAnonymousReport() {
    if (!isWiFiConnected()) return;

    HTTPClient http;
    WiFiClientSecure secureClient;
    WiFiClient plainClient;
    String url = getServerUrl() + "/api/anonymous";

    if (url.startsWith("https://")) {
        secureClient.setInsecure();
        http.begin(secureClient, url);
    } else {
        http.begin(plainClient, url);
    }
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(4000);
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

    String url = getServerUrl() + "/api/telemetry";

    if (!syncHttpConnected || !syncHttp.connected()) {
        if (syncHttpConnected) syncHttp.end();
        if (url.startsWith("https://")) {
            syncSecureClient.setInsecure();
            syncHttp.begin(syncSecureClient, url);
        } else {
            syncHttp.begin(syncPlainClient, url);
        }
        syncHttp.setReuse(true);
        syncHttp.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        syncHttp.setTimeout(2500);
        syncHttpConnected = true;
    }
    syncHttp.addHeader("Content-Type", "application/json");

    // Ultra-compact micro-JSON payload (optimized for 64 kbps links: only ~95 bytes!)
    String payload = "{";
    payload += "\"d\":\"" + getDeviceMac() + "\",";
    payload += "\"b\":" + String(getBalance(), 2) + ",";
    payload += "\"r\":" + String(isRelayOn() ? "1" : "0") + ",";
    payload += "\"rdgs\":[{";
    payload += "\"t\":" + String(getEpoch()) + ",";
    payload += "\"v\":" + String(live.valid ? live.voltage : 0, 1) + ",";
    payload += "\"i\":" + String(live.valid ? live.current : 0, 2) + ",";
    payload += "\"p\":" + String(live.valid ? live.power : 0, 1) + ",";
    payload += "\"e\":" + String(live.valid ? live.energy : 0, 2);
    payload += "}]}";

    unsigned long t0 = millis();
    int httpResponseCode = syncHttp.POST(payload);
    unsigned long rtt = millis() - t0;

    if (httpResponseCode == 200) {
        // Dynamic Bandwidth Adaptation for 64 kbps / throttled networks:
        if (rtt < 350) {
            adaptiveSyncInterval = 1000;  // 1-second real-time sync on broadband/4G
        } else if (rtt < 900) {
            adaptiveSyncInterval = 2500;  // 2.5-second moderate sync
        } else {
            adaptiveSyncInterval = 5000;  // 5.0-second low-bandwidth mode for 64 kbps
        }

        String response = syncHttp.getString();
        StaticJsonDocument<384> doc;
        DeserializationError err = deserializeJson(doc, response);
        
        if (!err && doc.containsKey("recharge")) {
            float amt = doc["recharge"];
            if (amt != 0) {
                Serial.printf("[NET] Instant Remote Fund Adjustment: %+.2f\n", amt);
                applyRecharge(amt);

                // Immediate LCD flash feedback
                char balMsg[17];
                if (amt > 0) snprintf(balMsg, sizeof(balMsg), "+Rs%.0f Added!", amt);
                else snprintf(balMsg, sizeof(balMsg), "Rs%.0f Deducted", -amt);
                showCustomMessage(balMsg);
            }
        }
        if (!err && doc["message"].is<const char*>()) {
            const char* msg = doc["message"];
            if (strlen(msg) > 0) {
                Serial.printf("[NET] Received LCD Message: %s\n", msg);
                showCustomMessage(msg);
            }
        }
        if (!err && doc.containsKey("latest_version")) {
            const char* latestVer = doc["latest_version"];
            const char* otaUrl = doc.containsKey("ota_url") ? doc["ota_url"] : nullptr;
            handleCloudVersionCheck(latestVer, otaUrl);
        }
        if (!err && doc.containsKey("reset_wifi") && doc["reset_wifi"] == true) {
            Serial.println("[NET] Received Remote Wi-Fi Reset Command from Cloud!");
            showCustomMessage("RESETTING WIFI");
            delay(1000);
            resetWiFiCredentials();
            syncHttp.end();
            syncHttpConnected = false;
            delay(500);
            ESP.restart();
        }
        if (!err && doc.containsKey("ota") && doc["ota"] == true) {
            const char* customOtaUrl = doc["ota_url"].as<const char*>();
            Serial.println("[NET] Cloud triggered Over-The-Air firmware update!");
            syncHttp.end();
            syncHttpConnected = false;
            triggerOTA(customOtaUrl);
        }
    } else {
        adaptiveSyncInterval = 10000; // Back off on packet drop / offline
        if (httpResponseCode < 0) {
            syncHttp.end();
            syncHttpConnected = false;
        }
    }
}

void maintainNetwork(const MeterReading &live) {
    if (isWiFiConnected()) {
        if (!ntpSynced) syncNTP();
        
        // Adaptive sync interval (1s on fast network, 5s on 64 kbps, 10s on backoff)
        if (millis() - lastSync > adaptiveSyncInterval) {
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

    if (syncHttpConnected) {
        syncHttp.end();
        syncHttpConnected = false;
    }

    if (millis() - lastRetry > WIFI_RETRY_INTERVAL_MS) {
        lastRetry = millis();
        Serial.println("[NET] WiFi not connected, attempting reconnect...");
        WiFi.disconnect();
        WiFi.mode(WIFI_STA);
        if (strlen(WIFI_SSID) > 0) {
            WiFi.begin(WIFI_SSID, WIFI_PASS);
        } else {
            WiFi.begin();
        }
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
