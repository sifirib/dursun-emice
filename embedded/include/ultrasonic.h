#pragma once

// Ultrasonic'in tek gorevi: bir insan yakinda mi diye olcmek.
class Ultrasonic
{
public:
    void begin();

    // cm cinsinden mesafe. Yanki alinamazsa -1 doner.
    float distance_cm();

    // config.h'daki PERSON_DISTANCE_CM esigine gore kisa mesafe kontrolu.
    bool person_detected();
};
    