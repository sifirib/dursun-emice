#pragma once

#include <cstddef>
#include <cstdint>

class WavBuffer
{
public:
    bool begin(uint32_t max_duration_ms, uint32_t pre_roll_ms);
    void end();
    void reset();

    void append(const int16_t* samples, size_t sample_count);

    void push_pre_roll(const int16_t* samples, size_t sample_count);
    void append_pre_roll();

    void finalize();

    bool full() const
    {
        return pcm_written_ >= pcm_capacity_;
    }

    size_t pcm_bytes() const
    {
        return pcm_written_;
    }

    const uint8_t* data() const
    {
        return buffer_;
    }

    size_t size() const
    {
        return wav_size_;
    }

    ~WavBuffer();

private:
    uint8_t* buffer_ = nullptr;
    size_t buffer_capacity_ = 0;
    size_t pcm_capacity_ = 0;
    size_t pcm_written_ = 0;
    size_t wav_size_ = 0;

    int16_t* pre_roll_buffer_ = nullptr;
    size_t pre_roll_capacity_samples_ = 0;
    size_t pre_roll_count_ = 0;
    size_t pre_roll_write_index_ = 0;
};
