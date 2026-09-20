#pragma once

#include <ESP_I2S.h>

#include <cstddef>
#include <cstdint>

class AudioInput
{
public:
    bool begin();
    bool read(int16_t* samples, size_t sample_count);
    void end();

    bool is_ready() const
    {
        return initialized_;
    }

    ~AudioInput();

private:
    bool ensure_raw_buffer(size_t sample_count);

    I2SClass i2s_;

    int32_t* raw_buffer_ = nullptr;
    size_t raw_capacity_samples_ = 0;

    bool initialized_ = false;
};
