#include "storage.h"
#include "config.h"
#include "time_provider.h"
#include <LittleFS.h>

// Ring-buffer metadata
struct BufferMeta {
    int head;        // index of oldest record
    int tail;        // index of next write slot
    int count;       // number of records stored
    uint32_t overflows;
};

static BufferMeta meta = {0, 0, 0, 0};
static const char* META_PATH   = "/readings/meta.bin";
static const char* LEDGER_PATH = "/ledger/ledger.bin";

// ── helpers ──────────────────────────────────────────────
static void saveMeta() {
    File f = LittleFS.open(META_PATH, "w");
    if (f) { f.write((uint8_t*)&meta, sizeof(meta)); f.close(); }
}

static void loadMeta() {
    File f = LittleFS.open(META_PATH, "r");
    if (f && f.size() == sizeof(meta)) {
        f.read((uint8_t*)&meta, sizeof(meta));
        f.close();
    }
}

static String readingPath(int idx) {
    char buf[32];
    snprintf(buf, sizeof(buf), "/readings/%04d.bin", idx);
    return String(buf);
}

// ── public API ───────────────────────────────────────────
void initStorage() {
    if (!LittleFS.begin(true)) {  // true = format on fail
        Serial.println("[STORAGE] LittleFS mount failed!");
        return;
    }
    LittleFS.mkdir("/readings");
    LittleFS.mkdir("/ledger");
    loadMeta();
    Serial.printf("[STORAGE] Ready — %d buffered, %lu overflows\n",
                  meta.count, (unsigned long)meta.overflows);
}

void addReadingToBuffer(const AggregatedReading &r) {
    // If buffer full, drop oldest
    if (meta.count >= MAX_BUFFER_RECORDS) {
        LittleFS.remove(readingPath(meta.head));
        meta.head = (meta.head + 1) % MAX_BUFFER_RECORDS;
        meta.count--;
        meta.overflows++;
    }
    File f = LittleFS.open(readingPath(meta.tail), "w");
    if (f) {
        f.write((uint8_t*)&r, sizeof(r));
        f.close();
    }
    meta.tail = (meta.tail + 1) % MAX_BUFFER_RECORDS;
    meta.count++;
    saveMeta();
}

int getBufferedReadingCount() { return meta.count; }

bool popOldestReading(AggregatedReading &r) {
    if (meta.count <= 0) return false;
    File f = LittleFS.open(readingPath(meta.head), "r");
    if (!f) return false;
    f.read((uint8_t*)&r, sizeof(r));
    f.close();
    LittleFS.remove(readingPath(meta.head));
    meta.head = (meta.head + 1) % MAX_BUFFER_RECORDS;
    meta.count--;
    saveMeta();
    return true;
}

void addLedgerEntry(const char* type, float amount,
                    float balanceAfter, const char* note) {
    LedgerEntry e;
    memset(&e, 0, sizeof(e));
    e.timestamp    = getEpoch();
    e.amount       = amount;
    e.balanceAfter = balanceAfter;
    strncpy(e.entryType, type, sizeof(e.entryType) - 1);
    strncpy(e.note,      note, sizeof(e.note) - 1);

    File f = LittleFS.open(LEDGER_PATH, "a");
    if (f) {
        f.write((uint8_t*)&e, sizeof(e));
        f.close();
    }
    Serial.printf("[LEDGER] %s %+.2f → Rs %.2f (%s)\n",
                  type, amount, balanceAfter, note);
}

int getLedgerCount() {
    File f = LittleFS.open(LEDGER_PATH, "r");
    if (!f) return 0;
    int n = f.size() / sizeof(LedgerEntry);
    f.close();
    return n;
}

uint32_t getBufferOverflowCount() { return meta.overflows; }
