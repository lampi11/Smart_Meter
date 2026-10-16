#include "ota.h"
#include "config.h"
#include "billing.h"
#include "network.h"
#include "display.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>
#include <esp_ota_ops.h>

#include <HTTPClient.h>
#include <ArduinoJson.h>

static unsigned long lastOTACheck = 0;
static bool otaValidated = false;
static bool otaInProgress = false;

// Returns: 1 if vA > vB, -1 if vA < vB, 0 if vA == vB
static int compareVersions(const char* vA, const char* vB) {
    if (!vA || !vB) return 0;
    int majA = 0, minA = 0, patA = 0;
    int majB = 0, minB = 0, patB = 0;
    sscanf(vA, "%d.%d.%d", &majA, &minA, &patA);
    sscanf(vB, "%d.%d.%d", &majB, &minB, &patB);
    if (majA != majB) return (majA > majB) ? 1 : -1;
    if (minA != minB) return (minA > minB) ? 1 : -1;
    if (patA != patB) return (patA > patB) ? 1 : -1;
    return 0;
}

void initOTA() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    Serial.printf("[OTA] Running firmware v%s from partition: %s\n", 
                  FW_VERSION, running ? running->label : "default");
}

bool isOTASafe() {
    // Only perform OTA if meter is stable and not currently updating
    return !otaInProgress;
}

void markOTAValid() {
    if (otaValidated) return;
    esp_ota_mark_app_valid_cancel_rollback();
    otaValidated = true;
    Serial.println("[OTA] Firmware image validated — rollback cancelled");
}

void triggerOTA(const char* customUrl) {
    if (!isWiFiConnected() || otaInProgress) {
        if (!isWiFiConnected()) Serial.println("[OTA] Cannot start OTA: WiFi disconnected");
        return;
    }

    String otaUrl = (customUrl && strlen(customUrl) > 0) ? String(customUrl) : (getServerUrl() + "/firmware.bin");
    Serial.printf("[OTA] Starting Over-The-Air update from: %s\n", otaUrl.c_str());

    otaInProgress = true;
    showOTAProgress(0);

    WiFiClientSecure secureClient;
    WiFiClient plainClient;
    httpUpdate.rebootOnUpdate(false);
    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    // Live progress callback: updates LCD screen on 64 kbps links as chunks download
    httpUpdate.onProgress([](int current, int total) {
        if (total > 0) {
            static int lastPct = -1;
            int pct = (current * 100) / total;
            if (pct != lastPct && (pct % 5 == 0 || pct == 100)) {
                lastPct = pct;
                showOTAProgress(pct);
                Serial.printf("[OTA] Download Progress: %d%% (%d / %d bytes)\n", pct, current, total);
            }
        }
    });

    // 240-second timeout (4 minutes) to support slow 64 kbps connections without aborting
    secureClient.setTimeout(240);
    plainClient.setTimeout(240);

    t_httpUpdate_return ret;
    if (otaUrl.startsWith("https://")) {
        secureClient.setInsecure();
        ret = httpUpdate.update(secureClient, otaUrl);
    } else {
        ret = httpUpdate.update(plainClient, otaUrl);
    }

    otaInProgress = false;

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("[OTA] Update failed (%d): %s\n", 
                          httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            showCustomMessage("OTA FAILED");
            break;

        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[OTA] No updates available");
            break;

        case HTTP_UPDATE_OK:
            Serial.println("[OTA] Firmware update SUCCESSFUL! Rebooting into new firmware...");
            showCustomMessage("UPDATE OK! BOOT");
            delay(1500);
            ESP.restart();
            break;
    }
}

void handleCloudVersionCheck(const char* latestVersion, const char* otaUrl) {
    if (!latestVersion || strlen(latestVersion) == 0 || otaInProgress) return;

    if (compareVersions(latestVersion, FW_VERSION) > 0) {
        Serial.printf("[OTA] Automatic update detected! Running: v%s → Latest: v%s. Initiating upgrade...\n",
                      FW_VERSION, latestVersion);
        triggerOTA(otaUrl);
    }
}

void checkOTA() {
    // Check every 30 minutes in background
    if (millis() - lastOTACheck < 1800000UL) return;
    lastOTACheck = millis();

    if (!isWiFiConnected() || !isOTASafe()) return;

    // Check version endpoint on cloud
    HTTPClient http;
    WiFiClientSecure secureClient;
    WiFiClient plainClient;
    String url = getServerUrl() + "/api/firmware/version";

    if (url.startsWith("https://")) {
        secureClient.setInsecure();
        http.begin(secureClient, url);
    } else {
        http.begin(plainClient, url);
    }
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(5000);

    int code = http.GET();
    if (code == 200) {
        String resp = http.getString();
        StaticJsonDocument<256> doc;
        DeserializationError err = deserializeJson(doc, resp);
        if (!err && doc.containsKey("version")) {
            const char* v = doc["version"];
            const char* u = doc.containsKey("url") ? doc["url"] : nullptr;
            handleCloudVersionCheck(v, u);
        }
    }
    http.end();
}
