#pragma once

#include <HTTPClient.h>
#include <Stream.h>
#include <WiFiClientSecure.h>

// ApiClient'in tek gorevi: sunucuyla HTTP uzerinden konusmak.
// Recorder'i, Player'i ya da konusma akisini bilmez.
class ApiClient
{
public:
    bool health();

    // WAV sesini /chat_raw'a POST eder. Basariliysa true doner ve
    // cevap govdesi (mp3) get_stream() / content_length() ile okunabilir.
    bool begin_chat(const uint8_t* audio, size_t size);

    Stream* get_stream();
    int content_length() const { return content_length_; }

    void end_chat();

private:
    HTTPClient http;
    WiFiClientSecure client;
    int content_length_ = 0;
};
