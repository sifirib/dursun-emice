#include "wifi_manager.h"

#include <WiFi.h>

#include "config.h"

bool WifiManager::begin()
{
    WiFi.mode(WIFI_STA);

    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );

    Serial.print("WiFi baglaniyor");

    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    }

    Serial.println();
    Serial.println("WiFi baglandi.");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    return true;
}

bool WifiManager::is_connected()
{
    return WiFi.status() == WL_CONNECTED;
}