#include "wifi_manager.h"

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"

void WifiManager::begin()
{
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);

    Serial.println("WiFi baglaniliyor...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    const uint32_t start_time = millis();

    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - start_time >= WIFI_CONNECT_TIMEOUT_MS)
        {
            Serial.println("WiFi baglantisi zaman asimina ugradi.");
            return;
        }

        delay(250);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi baglandi.");

    const uint32_t ip_wait_start = millis();

    while (WiFi.localIP() == IPAddress(0, 0, 0, 0))
    {
        if (millis() - ip_wait_start >= WIFI_IP_TIMEOUT_MS)
        {
            Serial.println("DHCP IP alinamadi.");
            return;
        }

        delay(100);
    }

    Serial.print("ESP IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
}

void WifiManager::update()
{
    if (
        WiFi.status() == WL_CONNECTED &&
        WiFi.localIP() != IPAddress(0, 0, 0, 0)
    )
    {
        return;
    }

    reconnect();
}

void WifiManager::reconnect()
{
    static uint32_t last_attempt_ms = 0;

    if (millis() - last_attempt_ms < WIFI_RECONNECT_INTERVAL_MS)
    {
        return;
    }

    last_attempt_ms = millis();

    Serial.println("WiFi yeniden baglaniliyor...");

    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}
