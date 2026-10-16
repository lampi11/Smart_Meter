#pragma once
#include <Arduino.h>

enum TimeSource { TIME_NTP, TIME_RTC, TIME_ESTIMATED, TIME_NONE };

void        initTimeProvider();
uint32_t    getEpoch();
bool        isTimeSynced();
TimeSource  getTimeSource();
const char* getTimeSourceStr();
void        setTimeFromNTP(uint32_t epoch);
int         getISTHour();
