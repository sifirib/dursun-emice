#pragma once

#include <cstdint>

// =========================
// WiFi
// =========================

constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";

// =========================
// Server
// =========================

constexpr char SERVER_URL[] = "https://dursun-emice.onrender.com";

// =========================
// Timing
// =========================

constexpr uint32_t KEEP_ALIVE_INTERVAL = 300000;   // 5 dakika
constexpr uint32_t HEALTH_TIMEOUT = 10000;         // 10 saniye

// =========================
// Ultrasonic
// =========================

constexpr float PERSON_DISTANCE_CM = 50.0f;

// =========================
// Recording
// =========================

constexpr uint32_t RECORD_DURATION_MS = 4000;