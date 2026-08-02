#include "keep_alive.h"

#include <Arduino.h>

#include "config.h"

void KeepAlive::update()
{
    uint32_t now = millis();

    if (last_ping_ms_ != 0 && now - last_ping_ms_ < KEEP_ALIVE_INTERVAL_MS)
    {
        return;
    }

    last_ping_ms_ = now;
    api_client_.health();
}
