#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "config.h"
#include "audio_input.h"
#include "speech_frontend.h"
#include "wav_buffer.h"

enum class recorder_state : uint8_t
{
    idle,
    waiting_for_speech,
    recording,
    ready
};

class Recorder
{
public:
    static constexpr uint32_t SAMPLE_RATE = AUDIO_SAMPLE_RATE;
    static constexpr uint16_t BITS_PER_SAMPLE = AUDIO_BITS_PER_SAMPLE;
    static constexpr uint16_t CHANNELS = AUDIO_CHANNELS;

    bool begin();
    bool start_listening();
    bool pause_detection();
    void update();
    void stop();

    bool is_active() const;
    bool is_recording() const;
    bool has_recording() const;
    recorder_state state() const;

    const uint8_t* wav_data() const
    {
        return wav_buffer_.data();
    }

    size_t wav_size() const
    {
        return wav_buffer_.size();
    }

    ~Recorder();

private:
    static void frontend_result_entry(
        const SpeechFrame& frame,
        void* context
    );

    void handle_frontend_result(const SpeechFrame& frame);
    void begin_recording(const SpeechFrame& frame);
    void finish_recording();
    void reset_session();
    void free_resources();

    AudioInput audio_input_;
    SpeechFrontend speech_frontend_;
    WavBuffer wav_buffer_;

    SemaphoreHandle_t session_mutex_ = nullptr;

    std::atomic<recorder_state> state_ {
        recorder_state::idle
    };

    uint32_t recording_started_ms_ = 0;
    bool initialized_ = false;
};
