#pragma once

#include <Stream.h>

#include <cstddef>
#include <cstdint>

class AudioGeneratorMP3;
class AudioOutputI2S;
class AudioFileSource;

class Player
{
public:
    void begin();

    bool play_file(
        const char* path
    );

    bool play_stream(
        Stream* stream,
        size_t size
    );

    bool play_memory(
        const uint8_t* data,
        size_t size
    );

    void loop();

    bool is_playing();

    void stop();

private:
    AudioGeneratorMP3* generator_ = nullptr;
    AudioOutputI2S* output_ = nullptr;
    AudioFileSource* source_ = nullptr;

    void cleanup_source();
};