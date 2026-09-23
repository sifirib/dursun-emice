#pragma once

#include <cstdint>

class TalkButton
{
public:
    void begin();
    void update();

    bool is_pressed() const
    {
        return stable_pressed_;
    }

    bool just_pressed() const
    {
        return just_pressed_;
    }

    bool just_released() const
    {
        return just_released_;
    }

private:
    bool raw_pressed_ = false;
    bool stable_pressed_ = false;
    bool just_pressed_ = false;
    bool just_released_ = false;
    uint32_t raw_changed_ms_ = 0;
};
