#include "time_provider.h"
#include "config.h"
#include <Preferences.h>

static Preferences timePrefs;
static uint32_t lastSyncedEpoch = 0;
static uint32_t millisAtSync    = 0;
static TimeSource currentSource = TIME_NONE;

void initTimeProvider() {
    timePrefs.begin("timeprov", false);
    lastSyncedEpoch = timePrefs.getUInt("epoch", 0);
    if (lastSyncedEpoch > 0) {
        currentSource = TIME_ESTIMATED;
        millisAtSync  = millis();
    }
}

uint32_t getEpoch() {
    if (currentSource == TIME_NONE) return 0;
    return lastSyncedEpoch + ((millis() - millisAtSync) / 1000);
}

bool isTimeSynced() {
    return (currentSource == TIME_NTP || currentSource == TIME_RTC);
}

TimeSource getTimeSource() { return currentSource; }

const char* getTimeSourceStr() {
    switch (currentSource) {
        case TIME_NTP:       return "NTP";
        case TIME_RTC:       return "RTC";
        case TIME_ESTIMATED: return "ESTIMATED";
        default:             return "NONE";
    }
}

void setTimeFromNTP(uint32_t epoch) {
    lastSyncedEpoch = epoch;
    millisAtSync    = millis();
    currentSource   = TIME_NTP;
    timePrefs.putUInt("epoch", epoch);
}

int getISTHour() {
    uint32_t epoch = getEpoch();
    if (epoch == 0) return -1;          // time unknown
    return ((epoch + IST_OFFSET_SECONDS) % 86400) / 3600;
}
