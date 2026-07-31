#include <Arduino.h>

#include "api_client.h"
#include "wifi_manager.h"

ApiClient api_client;
WifiManager wifi_manager;

void setup()
{
    Serial.begin(115200);

    wifi_manager.begin();

    if (api_client.health())
    {
        Serial.println("Server ulasilabilir.");
    }
    else
    {
        Serial.println("Server ulasilamiyor.");
    }
}

void loop()
{
    delay(30000);

    if (api_client.health())
    {
        Serial.println("Health OK");
    }
    else
    {
        Serial.println("Health FAILED");
    }
}