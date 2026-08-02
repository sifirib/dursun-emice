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

constexpr char SERVER_BASE_URL[] = "https://dursun-emice.onrender.com";

constexpr char HEALTH_ENDPOINT[] = "/health";
constexpr char CHAT_ENDPOINT[] = "/chat_raw";

constexpr uint32_t HTTP_TIMEOUT_MS = 30000;

// =========================
// Timing
// =========================

constexpr uint32_t KEEP_ALIVE_INTERVAL_MS = 300000; // 5 dakika
constexpr uint32_t COOLDOWN_AFTER_CHAT_MS = 3000;   // konusma sonrasi bekleme
constexpr uint32_t COOLDOWN_AFTER_ERROR_MS = 10000; // sunucu hatasi sonrasi bekleme

// =========================
// Ultrasonic (HC-SR04)
// =========================

constexpr float PERSON_DISTANCE_CM = 50.0f;

constexpr int ULTRASONIC_TRIG_PIN = 5;
constexpr int ULTRASONIC_ECHO_PIN = 18; // DIKKAT: 1k+2k voltaj bolucuyle baglayin (5V -> 3.3V)

// =========================
// Mikrofon (INMP441, I2S girisi - I2S_NUM_0)
// =========================

constexpr int MIC_SCK_PIN = 32;
constexpr int MIC_WS_PIN  = 25;
constexpr int MIC_SD_PIN  = 33;

// =========================
// Hoparlor (MAX98357A, I2S cikisi - I2S_NUM_1)
// =========================

constexpr int SPEAKER_BCLK_PIN = 26;
constexpr int SPEAKER_LRC_PIN  = 27;
constexpr int SPEAKER_DIN_PIN  = 22;

// =========================
// Kayit (Recorder)
// =========================

constexpr uint32_t RECORD_DURATION_MS = 4000;
    