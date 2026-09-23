#include "talk_button.h"

#include <Arduino.h>

#include "config.h"

void TalkButton::begin()
{
    pinMode(TALK_BUTTON_PIN, INPUT_PULLUP);

    const bool pressed = digitalRead(TALK_BUTTON_PIN) == LOW;

    raw_pressed_ = pressed;
    stable_pressed_ = pressed;
    just_pressed_ = false;
    just_released_ = false;
    raw_changed_ms_ = millis();

    Serial.print("Talk button GPIO: ");
    Serial.println(TALK_BUTTON_PIN);
}

void TalkButton::update()
{
    just_pressed_ = false;
    just_released_ = false;

    const bool pressed = digitalRead(TALK_BUTTON_PIN) == LOW;

    if (pressed != raw_pressed_)
    {
        raw_pressed_ = pressed;
        raw_changed_ms_ = millis();
        return;
    }

    if (
        raw_pressed_ != stable_pressed_ &&
        millis() - raw_changed_ms_ >= TALK_BUTTON_DEBOUNCE_MS
    )
    {
        stable_pressed_ = raw_pressed_;

        if (stable_pressed_)
        {
            just_pressed_ = true;
        }
        else
        {
            just_released_ = true;
        }
    }
}
