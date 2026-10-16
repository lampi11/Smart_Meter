#pragma once
#include <Arduino.h>

// Firmware
#define FW_VERSION "1.0.0"

// --- Hardware Pins (FROZEN) ---
#define PZEM_RX_PIN 16
#define PZEM_TX_PIN 17
#define RELAY_PIN   4
#define RELAY_ON_STATE  HIGH
#define RELAY_OFF_STATE LOW
#define I2C_SDA     21
#define I2C_SCL     22

// --- Billing ---
#define DEFAULT_TARIFF_GRID    8.0f    // Rs/kWh
#define DEFAULT_TARIFF_DG      12.0f   // Rs/kWh on generator
#define EMERGENCY_CREDIT_LIMIT -30.0f  // Rs (negative = credit)
#define LOW_BALANCE_WARN       50.0f
#define URGENT_BALANCE_WARN    20.0f
#define DEFAULT_BALANCE        100.0f

// --- No-Cut Window (IST hours) ---
#define NO_CUT_START_HOUR 22  // 10 PM
#define NO_CUT_END_HOUR    7  //  7 AM

// --- Flash Write Policy ---
#define SAVE_THRESHOLD_RS  0.50f
#define SAVE_HEARTBEAT_MS  300000UL  // 5 minutes

// --- Measurement ---
#define READ_INTERVAL_MS       2000UL    // 2 seconds
#define AGGREGATE_INTERVAL_MS  300000UL  // 5 minutes

// --- Validation Bounds ---
#define MAX_VOLTAGE 300.0f
#define MAX_CURRENT 100.0f

// --- Ring Buffer ---
#define MAX_BUFFER_RECORDS 900  // ~3 days of 5-min records

// --- Network & Provisioning ---
#define NTP_SERVER             "pool.ntp.org"
#define IST_OFFSET_SECONDS     19800     // UTC+5:30
#define WIFI_RETRY_INTERVAL_MS 30000UL
#define SYNC_INTERVAL_MS       2000UL    // 2-second live dashboard updates
#define DEFAULT_SERVER_URL     "https://smart-meter-tau.vercel.app"
#define FALLBACK_SERVER_URL    "http://lampitex-dashboard.loca.lt"
#define AP_SSID_PREFIX         "Lampitex-Setup-"
#define CONFIG_PORTAL_TIMEOUT  180       // 3 minutes captive portal timeout
#define RND_TELEMETRY_INTERVAL 3600000UL // 1 hour R&D anonymous telemetry

// --- OTA ---
#define OTA_CHECK_INTERVAL_MS 21600000UL  // 6 hours

// --- Diagnostics ---
#define DIAG_INTERVAL_MS 3600000UL  // 1 hour

// --- Relay Verification ---
#define RELAY_VERIFY_DELAY_MS            1000
#define RELAY_VERIFY_CURRENT_THRESHOLD   0.5f
