#include <Arduino.h>

#include "conversation_controller.h"
#include "wifi_manager.h"

ConversationController controller;
WifiManager wifi_manager;

void setup()
{
    Serial.begin(115200);

    wifi_manager.begin();

    controller.begin();
}

void loop()
{
    wifi_manager.update();

    controller.update();
}