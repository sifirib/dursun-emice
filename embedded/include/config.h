#pragma once

#include <cstddef>
#include <cstdint>

#include "secrets.h"
#include "interaction_mode.h"


// =========================
// Wi-Fi reliability
// =========================

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr uint32_t WIFI_IP_TIMEOUT_MS = 5000;
constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 5000;

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
// Interaction mode
// =========================

// V1: kullanici butona basili tutarken kayit yapilir.
// Ileride buton kaldirilmak istenirse sadece bu satir
// interaction_mode::automatic_vad olarak degistirilebilir;
// ESP-SR / VAD / pre-roll ayarlari aynen korunur.
constexpr interaction_mode INTERACTION_MODE =
    interaction_mode::push_to_talk;

// =========================
// Push-to-talk button
// =========================

// Normalde acik (NO) buton:
// GPIO4 --- BUTON --- GND
// Dahili INPUT_PULLUP kullanilir.
constexpr int TALK_BUTTON_PIN = 4;
constexpr uint32_t TALK_BUTTON_DEBOUNCE_MS = 30;
constexpr uint32_t PTT_RELEASE_NOTICE_DELAY_MS = 500;

// Kullanici butonu biraktiktan sonra son heceyi kesmemek icin.
constexpr uint32_t PTT_RELEASE_TAIL_MS = 250;

// Greeting veya Dursun cevabi bittikten sonra yeni bir bas-konuş
// gelmezse V1 oturumu kapanir.
constexpr uint32_t PTT_WAIT_TIMEOUT_MS = 15000;

// Yanlislikla yapilan cok kisa dokunuslari sunucuya gonderme.
constexpr uint32_t PTT_MIN_HOLD_MS = 250;

// Bas-konuş kaydinda VADNet hic gercek konusma gormediyse
// WAV sunucuya gonderilmez. VAD kaydin baslangic/bitisini yonetmez.
constexpr bool PTT_REQUIRE_VAD_SPEECH = true;

// =========================
// Audio format
// =========================

constexpr uint32_t AUDIO_SAMPLE_RATE = 16000;
constexpr uint16_t AUDIO_BITS_PER_SAMPLE = 16;
constexpr uint16_t AUDIO_CHANNELS = 1;

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
// Kayit / ESP-SR VAD
// =========================

constexpr uint32_t MAX_RECORD_DURATION_MS = 12000;

// Konusma bittikten sonra bu kadar
// sessizlik gorulurse kayit kapanir.
constexpr uint32_t END_OF_SPEECH_SILENCE_MS = 900;

// VADNet ancak bu kadar sure devam eden
// konusmayi gercek SPEECH kabul eder.
//
// 384 ms = 12 x 32 ms AFE frame.
constexpr uint32_t VAD_MIN_SPEECH_MS = 384;

// VAD karar verirken konusmanin basini
// kaybetmemek icin ESP-SR'nin dahili cache'i.
constexpr uint32_t VAD_DELAY_MS = 512;

// ESP-SR vad_cache herhangi bir nedenle
// kullanilamazsa kendi yedek pre-roll buffer'imiz.
//
// 384 ms speech confirmation
// + VAD gecikmesi
// + guvenlik payi.
constexpr uint32_t VAD_PRE_ROLL_MS = 700;