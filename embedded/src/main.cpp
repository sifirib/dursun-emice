#include <Arduino.h>
#include <LittleFS.h>

#include "config.h"
#include "conversation_controller.h"
#include "wifi_manager.h"


WifiManager wifi_manager;
ConversationController conversation_controller;


void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println(
        "================================"
    );

    Serial.println(
        "DURSUN EMICE"
    );

    Serial.println(
        "================================"
    );

    if (!LittleFS.begin(false))
    {
        Serial.println(
            "HATA: LittleFS baslatilamadi."
        );

        return;
    }

    Serial.println(
        "LittleFS: OK"
    );

    wifi_manager.begin();

    Serial.print(
        "Server: "
    );

    Serial.println(
        SERVER_BASE_URL
    );

    if (!conversation_controller.begin())
    {
        Serial.println(
            "HATA: ConversationController baslatilamadi."
        );

        return;
    }

    Serial.println();
    Serial.println(
        "Dursun Emice hazir."
    );

    Serial.println(
        "Kisi bekleniyor..."
    );
}


void loop()
{
    wifi_manager.update();

    conversation_controller.update();

    delay(1);
}