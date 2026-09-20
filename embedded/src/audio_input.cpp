#include "audio_input.h"

#include <Arduino.h>

#include <algorithm>

#include <esp_heap_caps.h>

#include "config.h"

AudioInput::~AudioInput()
{
    end();
}

bool AudioInput::begin()
{
    end();

    i2s_.setPins(
        MIC_SCK_PIN,
        MIC_WS_PIN,
        -1,
        MIC_SD_PIN
    );

    // readBytes() kapanis sirasinda sonsuza kadar bloklanmasin.
    i2s_.setTimeout(100);

    const bool started = i2s_.begin(
        I2S_MODE_STD,
        AUDIO_SAMPLE_RATE,
        I2S_DATA_BIT_WIDTH_32BIT,
        I2S_SLOT_MODE_MONO,
        I2S_STD_SLOT_LEFT
    );

    if (!started)
    {
        Serial.println("HATA: INMP441 I2S baslatilamadi.");
        return false;
    }

    initialized_ = true;
    Serial.println("INMP441 I2S: OK");
    return true;
}

bool AudioInput::ensure_raw_buffer(size_t sample_count)
{
    if (sample_count <= raw_capacity_samples_ && raw_buffer_ != nullptr)
    {
        return true;
    }

    if (raw_buffer_ != nullptr)
    {
        heap_caps_free(raw_buffer_);
        raw_buffer_ = nullptr;
        raw_capacity_samples_ = 0;
    }

    raw_buffer_ = static_cast<int32_t*>(
        heap_caps_malloc(
            sample_count * sizeof(int32_t),
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT
        )
    );

    if (raw_buffer_ == nullptr)
    {
        Serial.println("HATA: INMP441 raw buffer ayrilamadi.");
        return false;
    }

    raw_capacity_samples_ = sample_count;
    return true;
}

bool AudioInput::read(int16_t* samples, size_t sample_count)
{
    if (!initialized_ || samples == nullptr || sample_count == 0)
    {
        return false;
    }

    if (!ensure_raw_buffer(sample_count))
    {
        return false;
    }

    const size_t required_bytes = sample_count * sizeof(int32_t);
    size_t total_bytes_read = 0;

    while (total_bytes_read < required_bytes)
    {
        const size_t bytes_read = i2s_.readBytes(
            reinterpret_cast<char*>(raw_buffer_) + total_bytes_read,
            required_bytes - total_bytes_read
        );

        if (bytes_read == 0)
        {
            return false;
        }

        total_bytes_read += bytes_read;
    }

    // INMP441 verisi mevcut calisan projedeki gibi 32-bit I2S slotundan
    // PCM16'ya indirgeniyor. Bu kaydirma daha once gerçek donanimda test edildi.
    for (size_t i = 0; i < sample_count; ++i)
    {
        int32_t sample = raw_buffer_[i] >> 14;
        sample = std::clamp<int32_t>(sample, -32768, 32767);
        samples[i] = static_cast<int16_t>(sample);
    }

    return true;
}

void AudioInput::end()
{
    if (initialized_)
    {
        i2s_.end();
        initialized_ = false;
    }

    if (raw_buffer_ != nullptr)
    {
        heap_caps_free(raw_buffer_);
        raw_buffer_ = nullptr;
    }

    raw_capacity_samples_ = 0;
}
