#pragma once

#include <ESP_I2S.h>

#include <cstddef>
#include <cstdint>


enum class recorder_state
{
    idle,
    waiting_for_speech,
    recording,
    ready
};


class Recorder
{
public:
    static constexpr uint32_t SAMPLE_RATE = 16000;
    static constexpr uint16_t BITS_PER_SAMPLE = 16;
    static constexpr uint16_t CHANNELS = 1;

    bool begin();

    bool start_listening();

    void update();

    void stop();

    bool is_active() const
    {
        return (
            state_ ==
                recorder_state::waiting_for_speech ||
            state_ ==
                recorder_state::recording
        );
    }

    bool is_recording() const
    {
        return (
            state_ ==
            recorder_state::recording
        );
    }

    bool has_recording() const
    {
        return (
            state_ ==
            recorder_state::ready
        );
    }

    recorder_state state() const
    {
        return state_;
    }

    const uint8_t* wav_data() const
    {
        return buffer_;
    }

    size_t wav_size() const
    {
        return last_wav_size_;
    }

    ~Recorder();

private:
    // =========================
    // WAV buffer
    // =========================

    uint8_t* buffer_ = nullptr;

    size_t buffer_capacity_ = 0;
    size_t pcm_written_ = 0;
    size_t last_wav_size_ = 0;

    // =========================
    // Pre-roll
    // =========================

    int16_t* pre_roll_buffer_ = nullptr;

    size_t pre_roll_capacity_samples_ = 0;
    size_t pre_roll_count_ = 0;
    size_t pre_roll_write_index_ = 0;

    // =========================
    // I2S
    // =========================

    I2SClass i2s_;

    bool i2s_initialized_ = false;

    // =========================
    // State
    // =========================

    recorder_state state_ =
        recorder_state::idle;

    // =========================
    // VAD
    // =========================

    float noise_rms_ = 0.0f;

    /*
     * Ham RMS'in yumusatilmis hali.
     *
     * Tek frame'lik spike'lari bastirir.
     */
    float smoothed_rms_ = 0.0f;

    uint8_t voice_confirm_frames_ = 0;

    /*
     * Kayit devam ederken sesin gercekten
     * devam ettigini dogrulamak icin.
     */
    uint8_t continuation_voice_frames_ = 0;

    // =========================
    // Timing
    // =========================

    uint32_t recording_started_ms_ = 0;

    uint32_t last_voice_ms_ = 0;

    uint32_t voiced_duration_ms_ = 0;

    uint32_t last_debug_ms_ = 0;

    // =========================
    // Internal methods
    // =========================

    void process_samples(
        const int16_t* samples,
        size_t sample_count
    );

    float calculate_rms(
        const int16_t* samples,
        size_t sample_count
    );

    float current_voice_threshold() const;

    void push_pre_roll(
        const int16_t* samples,
        size_t sample_count
    );

    void copy_pre_roll_to_recording();

    void append_pcm(
        const int16_t* samples,
        size_t sample_count
    );

    void begin_recording();

    void finish_recording();

    void reject_false_trigger();

    void reset_waiting_state();
};