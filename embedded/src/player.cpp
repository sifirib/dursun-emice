#include "player.h"

#include <AudioFileSource.h>
#include <AudioFileSourceLittleFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <LittleFS.h>

#include "config.h"

namespace
{
    // Sunucudan POST cevabi olarak gelen, Content-Length'i onceden
    // bilinen bir MP3 akisini AudioGeneratorMP3'un okuyabilecegi bir
    // "dosya" gibi sunan kucuk bir sarmalayici. Sadece player.cpp
    // icinde kullanildigi icin ayri bir modul yapmadik (anonymous namespace)
    class StreamSource : public AudioFileSource
    {
    public:
        StreamSource(Stream* stream, size_t size) : stream_(stream), size_(size) {}

        uint32_t read(void* data, uint32_t len) override
        {
            if (stream_ == nullptr || pos_ >= size_)
            {
                return 0;
            }

            uint32_t remaining = static_cast<uint32_t>(size_) - pos_;
            uint32_t want = len < remaining ? len : remaining;

            int n = stream_->readBytes(reinterpret_cast<uint8_t*>(data), want);

            if (n <= 0)
            {
                return 0;
            }

            pos_ += static_cast<uint32_t>(n);
            return static_cast<uint32_t>(n);
        }

        bool close() override
        {
            stream_ = nullptr;
            return true;
        }

        bool isOpen() override
        {
            return stream_ != nullptr && pos_ < size_;
        }

        uint32_t getSize() override { return static_cast<uint32_t>(size_); }
        uint32_t getPos() override { return pos_; }

    private:
        Stream* stream_;
        size_t size_;
        uint32_t pos_ = 0;
    };
}

void Player::begin()
{
    // Port 1 -> mikrofonun kullandigi I2S_NUM_0 ile cakismaz.
    output_ = new AudioOutputI2S(1);
    output_->SetPinout(SPEAKER_BCLK_PIN, SPEAKER_LRC_PIN, SPEAKER_DIN_PIN);
    output_->SetGain(0.6f);
}

void Player::cleanup_source()
{
    if (generator_ != nullptr)
    {
        generator_->stop();
        delete generator_;
        generator_ = nullptr;
    }

    if (source_ != nullptr)
    {
        source_->close();
        delete source_;
        source_ = nullptr;
    }
}

bool Player::play_file(const char* path)
{
    cleanup_source();

    source_ = new AudioFileSourceLittleFS(path);

    if (!source_->isOpen())
    {
        cleanup_source();
        return false;
    }

    generator_ = new AudioGeneratorMP3();
    return generator_->begin(source_, output_);
}

bool Player::play_stream(Stream* stream, size_t size)
{
    cleanup_source();

    if (stream == nullptr || size == 0)
    {
        return false;
    }

    source_ = new StreamSource(stream, size);
    generator_ = new AudioGeneratorMP3();
    return generator_->begin(source_, output_);
}

void Player::loop()
{
    if (generator_ == nullptr)
    {
        return;
    }

    if (generator_->isRunning())
    {
        if (!generator_->loop())
        {
            cleanup_source();
        }
    }
}

bool Player::is_playing()
{
    return generator_ != nullptr && generator_->isRunning();
}

void Player::stop()
{
    cleanup_source();
}
