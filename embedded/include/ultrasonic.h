#pragma once

#include <cstdint>

class Ultrasonic
{
public:
    void begin();

    void update();

    bool person_present() const
    {
        return person_present_;
    }

    float distance_cm() const
    {
        return last_distance_cm_;
    }

private:
    float measure_distance_cm();

    float last_distance_cm_ = -1.0f;

    bool person_present_ = false;

    uint8_t near_sample_count_ = 0;
    uint8_t far_sample_count_ = 0;

    uint32_t last_sample_ms_ = 0;
};