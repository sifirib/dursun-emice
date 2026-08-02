#pragma once

#include <cstdint>
#include <cstddef>

// Ham PCM veriye standart 44 baytlik bir RIFF/WAV basligi eklemekten
// sorumlu tek bir yardimci. Recorder ile bilerek ayri tutuldu ki
// Recorder yalnizca mikrofon verisi toplamaya odaklansin.
namespace wav_header
{
    constexpr size_t SIZE = 44;

    // buffer en az SIZE bayt olmali. Ilk SIZE baytina basligi yazar.
    void write(
        uint8_t* buffer,
        uint32_t data_size,
        uint32_t sample_rate,
        uint16_t channels,
        uint16_t bits_per_sample
    );
}
