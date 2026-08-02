#pragma once

#include <cstdint>

#include "api_client.h"

// KeepAlive'in tek gorevi: Render'daki ucretsiz sunucunun uykuya
// gecmemesi icin belirli araliklarla /health'e istek atmak.
class KeepAlive
{
public:
    explicit KeepAlive(ApiClient& api_client) : api_client_(api_client) {}

    void update();

private:
    ApiClient& api_client_;
    uint32_t last_ping_ms_ = 0;
};
