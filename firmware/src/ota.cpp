#include "ota.h"
#include "config.h"
#include "billing.h"
#include <WiFi.h>
#include <HTTPUpdate.h>
#include <esp_ota_ops.h>

static unsigned long lastOTACheck  = 0;
static bool otaValidated = false;

// OTA URL pointing to the Next.js static file
#define OTA_URL "http://lampitex-dashboard.loca.lt/firmware.bin"

void initOTA() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    Serial.printf("[OTA] Running from partition: %s\n", running->label);
}

bool isOTASafe() {
    // Never OTA during a cutoff/restore event
    return isRelayOn();  // only safe if relay is stably on
}

void markOTAValid() {
    if (otaValidated) return;
    esp_ota_mark_app_valid_cancel_rollback();
    otaValidated = true;
    Serial.println("[OTA] Image marked valid — rollback cancelled");
}

void checkOTA() {
    if (millis() - lastOTACheck < OTA_CHECK_INTERVAL_MS) return;
    lastOTACheck = millis();

    if (!isOTASafe()) {
        Serial.println("[OTA] Skipped — relay not in safe state");
        return;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[OTA] Checking for updates at: " OTA_URL);
        
        WiFiClient client;
        // Don't auto-reboot to ensure graceful shutdown of LittleFS/Preferences
        httpUpdate.rebootOnUpdate(false);
        
        t_httpUpdate_return ret = httpUpdate.update(client, OTA_URL);
        
        switch (ret) {
            case HTTP_UPDATE_FAILED:
                Serial.printf("[OTA] Update failed (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
                break;
            case HTTP_UPDATE_NO_UPDATES:
                Serial.println("[OTA] No updates available");
                break;
            case HTTP_UPDATE_OK:
                Serial.println("[OTA] Update successful. Rebooting now...");
                delay(1000);
                ESP.restart();
                break;
        }
    }
}
