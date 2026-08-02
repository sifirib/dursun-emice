#pragma once

#include <Stream.h>
#include <cstddef>

class AudioGeneratorMP3;
class AudioOutputI2S;
class AudioFileSource;

// Player'in tek gorevi: sesi calmak. Sunucuyu, Gemini'yi, Recorder'i
// veya Ultrasonic'i bilmez - sadece kendisine verilen bir dosyayi ya da
// akisi MAX98357A uzerinden seslendirir.
class Player
{
public:
    void begin();

    // LittleFS'teki sabit bir MP3'u calar (orn. karsilama cumlesi).
    bool play_file(const char* path);

    // Boyutu onceden bilinen bir MP3 akisini (sunucu cevabi) calar.
    bool play_stream(Stream* stream, size_t size);

    // Ana loop() icinde surekli cagrilmali.
    void loop();

    bool is_playing();
    void stop();

private:
    AudioGeneratorMP3* generator_ = nullptr;
    AudioOutputI2S* output_ = nullptr;
    AudioFileSource* source_ = nullptr;

    void cleanup_source();
};
