#include "wav_buffer.h"

#include <algorithm>
#include <cstring>

#include <esp_heap_caps.h>

#include "config.h"
#include "wav_header.h"

WavBuffer::~WavBuffer()
{
    end();
}

bool WavBuffer::begin(uint32_t max_duration_ms, uint32_t pre_roll_ms)
{
    end();

    const size_t max_pcm_bytes =
        (static_cast<size_t>(AUDIO_SAMPLE_RATE) * max_duration_ms / 1000UL) *
        AUDIO_CHANNELS *
        (AUDIO_BITS_PER_SAMPLE / 8);

    pcm_capacity_ = max_pcm_bytes;
    buffer_capacity_ = wav_header::SIZE + pcm_capacity_;

    buffer_ = static_cast<uint8_t*>(
        heap_caps_malloc(
            buffer_capacity_,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
        )
    );

    if (buffer_ == nullptr)
    {
        end();
        return false;
    }

    pre_roll_capacity_samples_ =
        static_cast<size_t>(AUDIO_SAMPLE_RATE) * pre_roll_ms / 1000UL;

    if (pre_roll_capacity_samples_ == 0)
    {
        pre_roll_capacity_samples_ = 1;
    }

    pre_roll_buffer_ = static_cast<int16_t*>(
        heap_caps_malloc(
            pre_roll_capacity_samples_ * sizeof(int16_t),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
        )
    );

    if (pre_roll_buffer_ == nullptr)
    {
        end();
        return false;
    }

    reset();
    return true;
}

void WavBuffer::reset()
{
    pcm_written_ = 0;
    wav_size_ = 0;
    pre_roll_count_ = 0;
    pre_roll_write_index_ = 0;
}

void WavBuffer::append(const int16_t* samples, size_t sample_count)
{
    if (
        buffer_ == nullptr ||
        samples == nullptr ||
        sample_count == 0 ||
        pcm_written_ >= pcm_capacity_
    )
    {
        return;
    }

    size_t bytes_to_copy = sample_count * sizeof(int16_t);
    const size_t remaining = pcm_capacity_ - pcm_written_;
    bytes_to_copy = std::min(bytes_to_copy, remaining);

    memcpy(
        buffer_ + wav_header::SIZE + pcm_written_,
        samples,
        bytes_to_copy
    );

    pcm_written_ += bytes_to_copy;
}

void WavBuffer::push_pre_roll(const int16_t* samples, size_t sample_count)
{
    if (
        pre_roll_buffer_ == nullptr ||
        pre_roll_capacity_samples_ == 0 ||
        samples == nullptr
    )
    {
        return;
    }

    for (size_t i = 0; i < sample_count; ++i)
    {
        pre_roll_buffer_[pre_roll_write_index_] = samples[i];
        pre_roll_write_index_ =
            (pre_roll_write_index_ + 1) % pre_roll_capacity_samples_;

        if (pre_roll_count_ < pre_roll_capacity_samples_)
        {
            ++pre_roll_count_;
        }
    }
}

void WavBuffer::append_pre_roll()
{
    if (pre_roll_count_ == 0 || pre_roll_buffer_ == nullptr)
    {
        return;
    }

    if (pre_roll_count_ < pre_roll_capacity_samples_)
    {
        append(pre_roll_buffer_, pre_roll_count_);
        return;
    }

    const size_t first_part =
        pre_roll_capacity_samples_ - pre_roll_write_index_;

    append(pre_roll_buffer_ + pre_roll_write_index_, first_part);

    if (pre_roll_write_index_ > 0)
    {
        append(pre_roll_buffer_, pre_roll_write_index_);
    }
}

void WavBuffer::finalize()
{
    if (buffer_ == nullptr || pcm_written_ == 0)
    {
        wav_size_ = 0;
        return;
    }

    wav_header::write(
        buffer_,
        static_cast<uint32_t>(pcm_written_),
        AUDIO_SAMPLE_RATE,
        AUDIO_CHANNELS,
        AUDIO_BITS_PER_SAMPLE
    );

    wav_size_ = wav_header::SIZE + pcm_written_;
}

void WavBuffer::end()
{
    if (buffer_ != nullptr)
    {
        heap_caps_free(buffer_);
        buffer_ = nullptr;
    }

    if (pre_roll_buffer_ != nullptr)
    {
        heap_caps_free(pre_roll_buffer_);
        pre_roll_buffer_ = nullptr;
    }

    buffer_capacity_ = 0;
    pcm_capacity_ = 0;
    pre_roll_capacity_samples_ = 0;

    reset();
}
