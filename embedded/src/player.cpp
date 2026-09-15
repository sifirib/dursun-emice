#include "player.h"

#include <AudioFileSource.h>
#include <AudioFileSourceLittleFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>

#include <LittleFS.h>

#include <cstring>

#include "config.h"

namespace
{
    class StreamSource : public AudioFileSource
    {
    public:
        StreamSource(
            Stream* stream,
            size_t size
        )
            : stream_(stream),
              size_(size)
        {
        }

        uint32_t read(
            void* data,
            uint32_t len
        ) override
        {
            if (
                stream_ == nullptr ||
                pos_ >= size_
            )
            {
                return 0;
            }

            const uint32_t remaining =
                static_cast<uint32_t>(
                    size_ - pos_
                );

            const uint32_t want =
                len < remaining
                    ? len
                    : remaining;

            const int read_size =
                stream_->readBytes(
                    reinterpret_cast<uint8_t*>(data),
                    want
                );

            if (read_size <= 0)
            {
                return 0;
            }

            pos_ +=
                static_cast<uint32_t>(
                    read_size
                );

            return static_cast<uint32_t>(
                read_size
            );
        }

        bool close() override
        {
            stream_ = nullptr;
            return true;
        }

        bool isOpen() override
        {
            return (
                stream_ != nullptr &&
                pos_ < size_
            );
        }

        uint32_t getSize() override
        {
            return static_cast<uint32_t>(
                size_
            );
        }

        uint32_t getPos() override
        {
            return pos_;
        }

    private:
        Stream* stream_ = nullptr;

        size_t size_ = 0;
        uint32_t pos_ = 0;
    };


    class MemorySource : public AudioFileSource
    {
    public:
        MemorySource(
            const uint8_t* data,
            size_t size
        )
            : data_(data),
              size_(size)
        {
        }

        uint32_t read(
            void* destination,
            uint32_t len
        ) override
        {
            if (
                data_ == nullptr ||
                pos_ >= size_
            )
            {
                return 0;
            }

            size_t remaining =
                size_ - pos_;

            size_t read_size = len;

            if (read_size > remaining)
            {
                read_size = remaining;
            }

            memcpy(
                destination,
                data_ + pos_,
                read_size
            );

            pos_ += read_size;

            return static_cast<uint32_t>(
                read_size
            );
        }

        bool seek(
            int32_t offset,
            int direction
        ) override
        {
            int64_t new_position = 0;

            switch (direction)
            {
                case SEEK_SET:
                    new_position = offset;
                    break;

                case SEEK_CUR:
                    new_position =
                        static_cast<int64_t>(pos_) +
                        offset;
                    break;

                case SEEK_END:
                    new_position =
                        static_cast<int64_t>(size_) +
                        offset;
                    break;

                default:
                    return false;
            }

            if (
                new_position < 0 ||
                new_position >
                    static_cast<int64_t>(size_)
            )
            {
                return false;
            }

            pos_ =
                static_cast<size_t>(
                    new_position
                );

            return true;
        }

        bool close() override
        {
            data_ = nullptr;
            size_ = 0;
            pos_ = 0;

            return true;
        }

        bool isOpen() override
        {
            return data_ != nullptr;
        }

        uint32_t getSize() override
        {
            return static_cast<uint32_t>(
                size_
            );
        }

        uint32_t getPos() override
        {
            return static_cast<uint32_t>(
                pos_
            );
        }

    private:
        const uint8_t* data_ = nullptr;

        size_t size_ = 0;
        size_t pos_ = 0;
    };
}


void Player::begin()
{
    output_ =
        new AudioOutputI2S();

    output_->SetPinout(
        SPEAKER_BCLK_PIN,
        SPEAKER_LRC_PIN,
        SPEAKER_DIN_PIN
    );

    output_->SetGain(2.0f);
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


bool Player::play_file(
    const char* path
)
{
    cleanup_source();

    source_ =
        new AudioFileSourceLittleFS(
            path
        );

    if (
        source_ == nullptr ||
        !source_->isOpen()
    )
    {
        cleanup_source();

        return false;
    }

    generator_ =
        new AudioGeneratorMP3();

    if (generator_ == nullptr)
    {
        cleanup_source();

        return false;
    }

    if (
        !generator_->begin(
            source_,
            output_
        )
    )
    {
        cleanup_source();

        return false;
    }

    return true;
}


bool Player::play_stream(
    Stream* stream,
    size_t size
)
{
    cleanup_source();

    if (
        stream == nullptr ||
        size == 0
    )
    {
        return false;
    }

    source_ =
        new StreamSource(
            stream,
            size
        );

    generator_ =
        new AudioGeneratorMP3();

    if (
        source_ == nullptr ||
        generator_ == nullptr
    )
    {
        cleanup_source();

        return false;
    }

    if (
        !generator_->begin(
            source_,
            output_
        )
    )
    {
        cleanup_source();

        return false;
    }

    return true;
}


bool Player::play_memory(
    const uint8_t* data,
    size_t size
)
{
    cleanup_source();

    if (
        data == nullptr ||
        size == 0
    )
    {
        return false;
    }

    source_ =
        new MemorySource(
            data,
            size
        );

    generator_ =
        new AudioGeneratorMP3();

    if (
        source_ == nullptr ||
        generator_ == nullptr
    )
    {
        cleanup_source();

        return false;
    }

    if (
        !generator_->begin(
            source_,
            output_
        )
    )
    {
        cleanup_source();

        return false;
    }

    return true;
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
    return (
        generator_ != nullptr &&
        generator_->isRunning()
    );
}


void Player::stop()
{
    cleanup_source();
}