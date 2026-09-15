#pragma once

#include <cstddef>
#include <cstdint>

#include "secrets.h"

// =========================
// Server
// =========================

constexpr char HEALTH_ENDPOINT[] = "/health";
constexpr char CHAT_ENDPOINT[] = "/chat_raw";

constexpr uint32_t HTTP_TIMEOUT_MS = 60000;
constexpr uint32_t HEALTH_TIMEOUT_MS = 5000;

// =========================
// Timing
// =========================

constexpr uint32_t KEEP_ALIVE_INTERVAL_MS = 300000;

constexpr uint32_t COOLDOWN_AFTER_CHAT_MS = 300;
constexpr uint32_t COOLDOWN_AFTER_ERROR_MS = 10000;

constexpr uint32_t RESPONSE_STREAM_STALL_TIMEOUT_MS = 10000;

// =========================
// Gemini / TTS response
// =========================

constexpr size_t MAX_RESPONSE_MP3_BYTES =
    512 * 1024;

// =========================
// Ultrasonic - HC-SR04
// =========================

constexpr int ULTRASONIC_TRIG_PIN = 5;
constexpr int ULTRASONIC_ECHO_PIN = 6;

constexpr float PERSON_ENTER_DISTANCE_CM = 50.0f;
constexpr float PERSON_EXIT_DISTANCE_CM = 65.0f;

constexpr uint32_t ULTRASONIC_SAMPLE_INTERVAL_MS = 200;
constexpr uint32_t ULTRASONIC_ECHO_TIMEOUT_US = 6000;

constexpr uint8_t ULTRASONIC_CONFIRM_SAMPLES = 2;

// =========================
// Mikrofon - INMP441
// =========================

constexpr int MIC_SCK_PIN = 15;
constexpr int MIC_WS_PIN = 16;
constexpr int MIC_SD_PIN = 17;

// =========================
// Hoparlor - MAX98357A
// =========================

constexpr int SPEAKER_BCLK_PIN = 41;
constexpr int SPEAKER_LRC_PIN = 40;
constexpr int SPEAKER_DIN_PIN = 42;

// =========================
// Recording / VAD
// =========================

/*
 * Kullanici en fazla 12 saniye
 * kesintisiz konusabilir.
 */
constexpr uint32_t MAX_RECORD_DURATION_MS = 12000;

/*
 * Konusma basladiktan sonra 900 ms
 * gercek sessizlik olursa cumle bitti.
 */
constexpr uint32_t END_OF_SPEECH_SILENCE_MS = 900;

/*
 * Ilk heceyi kaybetmemek icin konusma
 * algilanmadan onceki 300 ms de WAV'e
 * eklenir.
 */
constexpr uint32_t VAD_PRE_ROLL_MS = 300;

/*
 * Cok kisa tik / vuruntu gibi sesleri
 * konusma kabul etme.
 */
constexpr uint32_t VAD_MIN_SPEECH_MS = 250;

// =========================
// VAD thresholds
// =========================

/*
 * Senin gercek testinde:
 *
 * sessizlik: ~20 - 120 RMS
 * konusma:   ~1400 - 3000+ RMS
 *
 * 350 su anda iyi bir baslangic esigi.
 */
constexpr float VAD_MIN_RMS_THRESHOLD = 350.0f;

constexpr float VAD_NOISE_MULTIPLIER = 3.0f;

constexpr float VAD_INITIAL_NOISE_RMS = 100.0f;

constexpr float VAD_MAX_NOISE_RMS = 600.0f;

/*
 * Konusmanin baslamasi icin
 * arka arkaya 5 frame.
 *
 * 64 sample/frame @ 16 kHz:
 * frame ~= 4 ms
 *
 * 5 frame ~= 20 ms
 */
constexpr uint8_t VAD_START_CONFIRM_FRAMES = 5;

/*
 * Kayit devam ederken tek bir gurultu
 * last_voice zamanini yenilemesin.
 *
 * En az 3 ard arda frame gercek ses
 * olarak gorulmeli.
 *
 * ~= 12 ms
 */
constexpr uint8_t VAD_CONTINUE_CONFIRM_FRAMES = 3;

/*
 * Ham RMS'i yumusat.
 *
 * Tek frame'lik 400-1000 RMS spike'lar
 * VAD'yi gereksiz yere tetiklemesin.
 */
constexpr float VAD_RMS_SMOOTHING_ALPHA = 0.20f;

// =========================
// VAD debug
// =========================

constexpr bool VAD_DEBUG = true;

constexpr uint32_t VAD_DEBUG_INTERVAL_MS = 500;