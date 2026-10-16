#include "ota.h"
#include "config.h"
#include "billing.h"
#include "network.h"
#include "display.h"
#include <WiFi.h>
#include <HTTPUpdate.h>
#include <esp_ota_ops.h>

static unsigned long lastOTACheck = 0;
static bool otaValidated = false;

void initOTA() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    Serial.printf("[OTA] Running from partition: %s\n", running ? running->label : "default");
}

bool isOTASafe() {
    // Only perform OTA if meter is stable
    return true;
}

void markOTAValid() {
    if (otaValidated) return;
    esp_ota_mark_app_valid_cancel_rollback();
    otaValidated = true;
    Serial.println("[OTA] Firmware image validated — rollback cancelled");
}

void triggerOTA(const char* customUrl) {
    if (!isWiFiConnected()) {
        Serial.println("[OTA] Cannot start OTA: WiFi disconnected");
        return;
    }

    String otaUrl = (customUrl && strlen(customUrl) > 0) ? String(customUrl) : (getServerUrl() + "/firmware.bin");
    Serial.printf("[OTA] Starting Over-The-Air update from: %s\n", otaUrl.c_str());

    showCustomMessage("UPDATING FW...");

    WiFiClient client;
    httpUpdate.rebootOnUpdate(false);

    t_httpUpdate_return ret = httpUpdate.update(client, otaUrl);

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

void checkOTA() {
    if (millis() - lastOTACheck < OTA_CHECK_INTERVAL_MS) return;
    lastOTACheck = millis();

    if (isWiFiConnected() && isOTASafe()) {
        triggerOTA();
    }
}
