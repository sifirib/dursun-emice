#include <Arduino.h>
#include <LittleFS.h>

#include "conversation_controller.h"
#include "wifi_manager.h"

ConversationController controller;
WifiManager wifi_manager;

void setup()
{
    Serial.begin(115200);

    if (!LittleFS.begin(true))
    {
        Serial.println("HATA: LittleFS baslatilamadi.");
    }

    wifi_manager.begin();
    controller.begin();

    Serial.println("Dursun Emice hazir.");
}

void loop()
{
    wifi_manager.update();
    controller.update();
}
