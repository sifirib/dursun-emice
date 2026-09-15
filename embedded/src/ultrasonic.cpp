#include "ultrasonic.h"

#include <Arduino.h>

#include "config.h"

void Ultrasonic::begin()
{
    pinMode(
        ULTRASONIC_TRIG_PIN,
        OUTPUT
    );

    pinMode(
        ULTRASONIC_ECHO_PIN,
        INPUT
    );

    digitalWrite(
        ULTRASONIC_TRIG_PIN,
        LOW
    );
}

float Ultrasonic::measure_distance_cm()
{
    digitalWrite(
        ULTRASONIC_TRIG_PIN,
        LOW
    );

    delayMicroseconds(2);

    digitalWrite(
        ULTRASONIC_TRIG_PIN,
        HIGH
    );

    delayMicroseconds(10);

    digitalWrite(
        ULTRASONIC_TRIG_PIN,
        LOW
    );

    const uint32_t duration_us =
        pulseIn(
            ULTRASONIC_ECHO_PIN,
            HIGH,
            ULTRASONIC_ECHO_TIMEOUT_US
        );

    if (duration_us == 0)
    {
        return -1.0f;
    }

    return (
        static_cast<float>(duration_us) *
        0.0343f /
        2.0f
    );
}

void Ultrasonic::update()
{
    const uint32_t now = millis();

    if (
        last_sample_ms_ != 0 &&
        now - last_sample_ms_ <
            ULTRASONIC_SAMPLE_INTERVAL_MS
    )
    {
        return;
    }

    last_sample_ms_ = now;

    last_distance_cm_ =
        measure_distance_cm();

    if (!person_present_)
    {
        if (
            last_distance_cm_ > 0.0f &&
            last_distance_cm_ <=
                PERSON_ENTER_DISTANCE_CM
        )
        {
            ++near_sample_count_;

            if (
                near_sample_count_ >=
                ULTRASONIC_CONFIRM_SAMPLES
            )
            {
                person_present_ = true;

                near_sample_count_ = 0;
                far_sample_count_ = 0;

                Serial.print(
                    "[ULTRASONIC] Kisi algilandi: "
                );

                Serial.print(
                    last_distance_cm_
                );

                Serial.println(" cm");
            }
        }
        else
        {
            near_sample_count_ = 0;
        }

        return;
    }

    const bool person_is_far =
        (
            last_distance_cm_ < 0.0f ||
            last_distance_cm_ >=
                PERSON_EXIT_DISTANCE_CM
        );

    if (person_is_far)
    {
        ++far_sample_count_;

        if (
            far_sample_count_ >=
            ULTRASONIC_CONFIRM_SAMPLES
        )
        {
            person_present_ = false;

            near_sample_count_ = 0;
            far_sample_count_ = 0;

            Serial.println(
                "[ULTRASONIC] Kisi ayrildi."
            );
        }
    }
    else
    {
        far_sample_count_ = 0;
    }
}