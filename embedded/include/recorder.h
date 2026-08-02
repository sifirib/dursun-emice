#pragma once

#include <cstdint>
#include <cstddef>

// Recorder'in tek gorevi: INMP441 mikrofonundan ses toplamak ve bunu
// WAV formatinda bir arabellege yazmak. Sunucuya gonderme, oynatma vb.
// hicbir sey bilmez.
class Recorder
{
public:
    static constexpr uint32_t SAMPLE_RATE = 16000;
    static constexpr uint16_t BITS_PER_SAMPLE = 16;
    static constexpr uint16_t CHANNELS = 1;

    // Mikrofonu I2S olarak kurar ve arabellegi bir kez ayirir.
    // false donerse bellek yetersizdir.
    bool begin();

    // RECORD_DURATION_MS kadar kayit yapar (bloklayici).
    // Toplam WAV boyutunu (baslik + veri) bayt cinsinden dondurur.
    size_t record();

    const uint8_t* wav_data() const { return buffer_; }
    size_t wav_size() const { return last_wav_size_; }

private:
    uint8_t* buffer_ = nullptr;
    size_t buffer_capacity_ = 0;
    size_t last_wav_size_ = 0;
};
