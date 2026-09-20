#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include "esp_afe_sr_iface.h"
#include "model_path.h"

class AudioInput;

struct SpeechFrame
{
    const int16_t* samples = nullptr;
    size_t sample_count = 0;

    // ESP-SR VAD cache. Frontend bunu yalnizca kullanilabilir gorurse sunar.
    const int16_t* speech_prefix = nullptr;
    size_t speech_prefix_sample_count = 0;

    bool speech = false;
};

class SpeechFrontend
{
public:
    using result_callback_t = void (*)(
        const SpeechFrame& frame,
        void* context
    );

    bool begin(
        AudioInput& audio_input,
        result_callback_t callback,
        void* callback_context
    );

    bool reset_vad();

    void end();

    ~SpeechFrontend();

private:
    static void feed_task_entry(void* parameter);
    static void fetch_task_entry(void* parameter);

    void feed_task();
    void fetch_task();

    bool begin_afe();
    bool create_tasks();
    void stop_tasks();
    void release_afe();

    bool cache_has_signal(
        const int16_t* samples,
        size_t sample_count
    ) const;

    AudioInput* audio_input_ = nullptr;

    result_callback_t callback_ = nullptr;
    void* callback_context_ = nullptr;

    srmodel_list_t* models_ = nullptr;
    const esp_afe_sr_iface_t* afe_handle_ = nullptr;
    esp_afe_sr_data_t* afe_data_ = nullptr;

    size_t feed_chunk_size_ = 0;
    size_t fetch_chunk_size_ = 0;

    EventGroupHandle_t task_events_ = nullptr;
    TaskHandle_t feed_task_handle_ = nullptr;
    TaskHandle_t fetch_task_handle_ = nullptr;

    bool initialized_ = false;
};
