#pragma once
#include <Arduino.h>

struct AggregatedReading {
    uint32_t timestamp;
    char     timeSource[12];
    float    avgPower;
    float    avgPF;
    float    energyDelta;
    float    balanceAfter;
    bool     relayOn;
};

struct LedgerEntry {
    uint32_t timestamp;
    char     entryType[20];
    float    amount;
    float    balanceAfter;
    char     note[32];
};

void     initStorage();
void     addReadingToBuffer(const AggregatedReading &r);
int      getBufferedReadingCount();
bool     popOldestReading(AggregatedReading &r);
void     addLedgerEntry(const char* type, float amount,
                        float balanceAfter, const char* note);
int      getLedgerCount();
uint32_t getBufferOverflowCount();
