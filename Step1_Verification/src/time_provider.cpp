#include "time_provider.h"
#include <Preferences.h>

static Preferences timePrefs;
static uint32_t lastSyncedEpoch = 0;
static uint32_t millisAtSync = 0;
static TimeSource currentSource = TIME_NONE;

void initTimeProvider() {
    timePrefs.begin("time", false);
    lastSyncedEpoch = timePrefs.getUInt("epoch", 0);
    
    // If we have a saved epoch, we can estimate time from boot
    if (lastSyncedEpoch > 0) {
        currentSource = TIME_ESTIMATED;
        millisAtSync = millis(); 
    }
}

uint32_t getEpoch() {
    if (currentSource == TIME_NONE) return 0;
    // Calculate current epoch based on millis elapsed since sync
    return lastSyncedEpoch + ((millis() - millisAtSync) / 1000);
}

bool isTimeSynced() {
    return (currentSource == TIME_NTP || currentSource == TIME_RTC);
}

TimeSource getTimeSource() {
    return currentSource;
}

void setTimeFromNTP(uint32_t epoch) {
    lastSyncedEpoch = epoch;
    millisAtSync = millis();
    currentSource = TIME_NTP;
    timePrefs.putUInt("epoch", epoch);
}
