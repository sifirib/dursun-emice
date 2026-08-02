#include "recorder.h"

#include <Arduino.h>
#include <cstring>
#include <new>
#include <driver/i2s.h>

#include "config.h"
#include "wav_header.h"

bool Recorder::begin()
{
    size_t pcm_capacity = static_cast<size_t>(
        SAMPLE_RATE * (RECORD_DURATION_MS / 1000.0f)
    ) * (BITS_PER_SAMPLE / 8) * CHANNELS;

    buffer_capacity_ = wav_header::SIZE + pcm_capacity;
    buffer_ = new (std::nothrow) uint8_t[buffer_capacity_];

    if (buffer_ == nullptr)
    {
        Serial.println("HATA: Recorder arabellegi ayrilamadi (bellek yetersiz).");
        return false;
    }

    i2s_config_t cfg = {
        .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // INMP441, 32 bit slotta 24 bit veri
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 256,
        .use_apll = false
    };

    i2s_pin_config_t pins = {
        .bck_io_num = MIC_SCK_PIN,
        .ws_io_num = MIC_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = MIC_SD_PIN
    };

    i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
    i2s_set_pin(I2S_NUM_0, &pins);

    return true;
}

size_t Recorder::record()
{
    if (buffer_ == nullptr)
    {
        last_wav_size_ = 0;
        return 0;
    }

    uint8_t* pcm_start = buffer_ + wav_header::SIZE;
    size_t pcm_capacity = buffer_capacity_ - wav_header::SIZE;
    size_t pcm_written = 0;

    int32_t raw[256];
    unsigned long start = millis();

    while (millis() - start < RECORD_DURATION_MS &&
           pcm_written + sizeof(int16_t) <= pcm_capacity)
    {
        size_t bytes_read = 0;
        i2s_read(I2S_NUM_0, raw, sizeof(raw), &bytes_read, portMAX_DELAY);

        size_t samples = bytes_read / sizeof(int32_t);

        for (size_t i = 0; i < samples; i++)
        {
            if (pcm_written + sizeof(int16_t) > pcm_capacity)
            {
                break;
            }

            // INMP441, 32 bitlik slotun ust taraflarina 24 bit veri koyar.
            // Anlamli 16 bite indirgemek icin sagdan kaydiriyoruz.
            int16_t sample16 = static_cast<int16_t>(raw[i] >> 14);
            memcpy(pcm_start + pcm_written, &sample16, sizeof(int16_t));
            pcm_written += sizeof(int16_t);
        }
    }

    wav_header::write(
        buffer_,
        static_cast<uint32_t>(pcm_written),
        SAMPLE_RATE,
        CHANNELS,
        BITS_PER_SAMPLE
    );

    last_wav_size_ = wav_header::SIZE + pcm_written;
    return last_wav_size_;
}
