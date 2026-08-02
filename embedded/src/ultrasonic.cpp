#include "ultrasonic.h"

#include <Arduino.h>

#include "config.h"

void Ultrasonic::begin()
{
    pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
    pinMode(ULTRASONIC_ECHO_PIN, INPUT);
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
}

float Ultrasonic::distance_cm()
{
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

    uint32_t duration_us = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, 30000);

    if (duration_us == 0)
    {
        return -1.0f; // yanki alinamadi, cok uzak ya da bos olcum
    }

    return duration_us * 0.0343f / 2.0f;
}

bool Ultrasonic::person_detected()
{
    float d = distance_cm();
    return d > 0.0f && d < PERSON_DISTANCE_CM;
}
