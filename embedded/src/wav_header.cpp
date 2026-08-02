#include "wav_header.h"

#include <cstring>

void wav_header::write(
    uint8_t* buffer,
    uint32_t data_size,
    uint32_t sample_rate,
    uint16_t channels,
    uint16_t bits_per_sample
)
{
    uint32_t chunk_size = 36 + data_size;
    uint32_t sub_chunk1_size = 16;
    uint16_t audio_format = 1; // PCM
    uint32_t byte_rate = sample_rate * channels * (bits_per_sample / 8);
    uint16_t block_align = channels * (bits_per_sample / 8);

    memcpy(buffer + 0, "RIFF", 4);
    memcpy(buffer + 4, &chunk_size, 4);
    memcpy(buffer + 8, "WAVE", 4);
    memcpy(buffer + 12, "fmt ", 4);
    memcpy(buffer + 16, &sub_chunk1_size, 4);
    memcpy(buffer + 20, &audio_format, 2);
    memcpy(buffer + 22, &channels, 2);
    memcpy(buffer + 24, &sample_rate, 4);
    memcpy(buffer + 28, &byte_rate, 4);
    memcpy(buffer + 32, &block_align, 2);
    memcpy(buffer + 34, &bits_per_sample, 2);
    memcpy(buffer + 36, "data", 4);
    memcpy(buffer + 40, &data_size, 4);
}
