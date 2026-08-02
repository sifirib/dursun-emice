#include "wifi_manager.h"

#include <WiFi.h>

#include "config.h"

void WifiManager::begin()
{
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void WifiManager::update()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }

    reconnect();
}

void WifiManager::reconnect()
{
    static uint32_t last_attempt = 0;

    if (millis() - last_attempt < 5000)
    {
        return;
    }

    last_attempt = millis();

    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}
