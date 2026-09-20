#include "speech_frontend.h"

#include <Arduino.h>

#include <esp_err.h>
#include <esp_heap_caps.h>

#include "audio_input.h"
#include "config.h"
#include "esp_afe_config.h"
#include "esp_afe_sr_models.h"
#include "esp_nsn_models.h"
#include "esp_vadn_models.h"

namespace
{
    constexpr EventBits_t STOP_REQUESTED = BIT0;
    constexpr EventBits_t FEED_STOPPED = BIT1;
    constexpr EventBits_t FETCH_STOPPED = BIT2;
}

SpeechFrontend::~SpeechFrontend()
{
    end();
}

bool SpeechFrontend::begin(
    AudioInput& audio_input,
    result_callback_t callback,
    void* callback_context
)
{
    end();

    audio_input_ = &audio_input;
    callback_ = callback;
    callback_context_ = callback_context;

    if (!audio_input_->is_ready())
    {
        Serial.println("HATA: SpeechFrontend icin mikrofon hazir degil.");
        end();
        return false;
    }

    if (!begin_afe())
    {
        end();
        return false;
    }

    task_events_ = xEventGroupCreate();
    if (task_events_ == nullptr)
    {
        Serial.println("HATA: SpeechFrontend event group olusturulamadi.");
        end();
        return false;
    }

    if (!create_tasks())
    {
        end();
        return false;
    }

    initialized_ = true;

    Serial.println("ESP-SR AFE / NSNet2 / VADNet: OK");
    Serial.print("AFE feed chunk: ");
    Serial.println(feed_chunk_size_);
    Serial.print("AFE fetch chunk: ");
    Serial.println(fetch_chunk_size_);

    return true;
}

bool SpeechFrontend::begin_afe()
{
    models_ = esp_srmodel_init("model");
    if (models_ == nullptr)
    {
        Serial.println("HATA: ESP-SR model partition okunamadi.");
        return false;
    }

    char* ns_model_name = esp_srmodel_filter(
        models_,
        ESP_NSNET_PREFIX,
        nullptr
    );

    char* vad_model_name = esp_srmodel_filter(
        models_,
        ESP_VADN_PREFIX,
        nullptr
    );

    if (ns_model_name == nullptr || vad_model_name == nullptr)
    {
        Serial.println("HATA: NSNet2 veya VADNet bulunamadi.");
        return false;
    }

    Serial.print("NS model: ");
    Serial.println(ns_model_name);
    Serial.print("VAD model: ");
    Serial.println(vad_model_name);

    afe_config_t* afe_config = afe_config_init(
        "M",
        models_,
        AFE_TYPE_SR,
        AFE_MODE_HIGH_PERF
    );

    if (afe_config == nullptr)
    {
        Serial.println("HATA: AFE config olusturulamadi.");
        return false;
    }

    // Dursun Emice icin yalnızca gürültü azaltma + neural VAD gerekiyor.
    afe_config->aec_init = false;
    afe_config->se_init = false;
    afe_config->agc_init = false;
    afe_config->wakenet_init = false;

    afe_config->ns_init = true;
    afe_config->ns_model_name = ns_model_name;
    afe_config->afe_ns_mode = AFE_NS_MODE_NET;

    afe_config->vad_init = true;
    afe_config->vad_model_name = vad_model_name;
    afe_config->vad_min_speech_ms = VAD_MIN_SPEECH_MS;
    afe_config->vad_min_noise_ms = END_OF_SPEECH_SILENCE_MS;
    afe_config->vad_delay_ms = VAD_DELAY_MS;

    afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;

    afe_handle_ = esp_afe_handle_from_config(afe_config);
    if (afe_handle_ == nullptr)
    {
        Serial.println("HATA: AFE handle alinamadi.");
        afe_config_free(afe_config);
        return false;
    }

    afe_data_ = afe_handle_->create_from_config(afe_config);
    afe_config_free(afe_config);

    if (afe_data_ == nullptr)
    {
        Serial.println("HATA: AFE instance olusturulamadi.");
        return false;
    }

    const int feed_chunk = afe_handle_->get_feed_chunksize(afe_data_);
    const int fetch_chunk = afe_handle_->get_fetch_chunksize(afe_data_);
    const int feed_channels = afe_handle_->get_feed_channel_num(afe_data_);
    const int sample_rate = afe_handle_->get_samp_rate(afe_data_);

    if (feed_chunk <= 0 || fetch_chunk <= 0)
    {
        Serial.println("HATA: AFE chunk boyutu gecersiz.");
        return false;
    }

    if (feed_channels != 1)
    {
        Serial.println("HATA: AFE tek mikrofon kanali bekleniyor.");
        return false;
    }

    if (sample_rate != static_cast<int>(AUDIO_SAMPLE_RATE))
    {
        Serial.println("HATA: AFE sample rate 16 kHz degil.");
        return false;
    }

    feed_chunk_size_ = static_cast<size_t>(feed_chunk);
    fetch_chunk_size_ = static_cast<size_t>(fetch_chunk);
    return true;
}

bool SpeechFrontend::create_tasks()
{
    BaseType_t result = xTaskCreatePinnedToCore(
        feed_task_entry,
        "afe_feed",
        6144,
        this,
        5,
        &feed_task_handle_,
        0
    );

    if (result != pdPASS)
    {
        feed_task_handle_ = nullptr;
        Serial.println("HATA: AFE feed task olusturulamadi.");
        return false;
    }

    result = xTaskCreatePinnedToCore(
        fetch_task_entry,
        "afe_fetch",
        6144,
        this,
        5,
        &fetch_task_handle_,
        1
    );

    if (result != pdPASS)
    {
        fetch_task_handle_ = nullptr;
        Serial.println("HATA: AFE fetch task olusturulamadi.");
        return false;
    }

    return true;
}

void SpeechFrontend::feed_task_entry(void* parameter)
{
    static_cast<SpeechFrontend*>(parameter)->feed_task();
}

void SpeechFrontend::fetch_task_entry(void* parameter)
{
    static_cast<SpeechFrontend*>(parameter)->fetch_task();
}

void SpeechFrontend::feed_task()
{
    int16_t* feed_buffer = static_cast<int16_t*>(
        heap_caps_malloc(
            feed_chunk_size_ * sizeof(int16_t),
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT
        )
    );

    if (feed_buffer == nullptr)
    {
        Serial.println("HATA: AFE feed buffer ayrilamadi.");
        xEventGroupSetBits(task_events_, FEED_STOPPED);
        vTaskDelete(nullptr);
        return;
    }

    while ((xEventGroupGetBits(task_events_) & STOP_REQUESTED) == 0)
    {
        if (!audio_input_->read(feed_buffer, feed_chunk_size_))
        {
            continue;
        }

        if ((xEventGroupGetBits(task_events_) & STOP_REQUESTED) != 0)
        {
            break;
        }

        afe_handle_->feed(afe_data_, feed_buffer);
    }

    heap_caps_free(feed_buffer);

    xEventGroupSetBits(task_events_, FEED_STOPPED);
    vTaskDelete(nullptr);
}

bool SpeechFrontend::cache_has_signal(
    const int16_t* samples,
    size_t sample_count
) const
{
    if (samples == nullptr || sample_count == 0)
    {
        return false;
    }

    int32_t max_abs_sample = 0;

    for (size_t i = 0; i < sample_count; ++i)
    {
        const int32_t sample = static_cast<int32_t>(samples[i]);
        const int32_t absolute = sample < 0 ? -sample : sample;

        if (absolute > max_abs_sample)
        {
            max_abs_sample = absolute;
        }
    }

    // VAD threshold'u degil. Eski ESP-SR surumlerinde raporlanan
    // tamamen/neredeyse bos vad_cache durumunda fallback secmek icin.
    return max_abs_sample >= 16;
}

void SpeechFrontend::fetch_task()
{
    while ((xEventGroupGetBits(task_events_) & STOP_REQUESTED) == 0)
    {
        // Normal fetch() 2 saniyeye kadar bloklayabilir. Kisa timeout,
        // task'in kontrollu sekilde kapanmasini kolaylastirir.
        afe_fetch_result_t* result = afe_handle_->fetch_with_delay(
            afe_data_,
            pdMS_TO_TICKS(100)
        );

        if ((xEventGroupGetBits(task_events_) & STOP_REQUESTED) != 0)
        {
            break;
        }

        if (
            result == nullptr ||
            result->ret_value != ESP_OK ||
            result->data == nullptr ||
            result->data_size <= 0
        )
        {
            continue;
        }

        SpeechFrame frame;
        frame.samples = result->data;
        frame.sample_count =
            static_cast<size_t>(result->data_size) / sizeof(int16_t);
        frame.speech = result->vad_state == VAD_SPEECH;

        if (
            result->vad_cache != nullptr &&
            result->vad_cache_size > 0
        )
        {
            const size_t cache_sample_count =
                static_cast<size_t>(result->vad_cache_size) / sizeof(int16_t);

            if (cache_has_signal(result->vad_cache, cache_sample_count))
            {
                frame.speech_prefix = result->vad_cache;
                frame.speech_prefix_sample_count = cache_sample_count;
            }
        }

        if (callback_ != nullptr)
        {
            callback_(frame, callback_context_);
        }
    }

    xEventGroupSetBits(task_events_, FETCH_STOPPED);
    vTaskDelete(nullptr);
}

bool SpeechFrontend::reset_vad()
{
    if (afe_handle_ == nullptr || afe_data_ == nullptr)
    {
        return false;
    }

    // Espressif'in resmi API'si: onceki VAD durumunu/cache state'ini
    // temizle. AFE feed/fetch pipeline'i calismaya devam eder.
    const int result = afe_handle_->reset_vad(afe_data_);

    if (result < 0)
    {
        Serial.println("HATA: VADNet reset basarisiz.");
        return false;
    }

    return true;
}


void SpeechFrontend::stop_tasks()
{
    if (task_events_ == nullptr)
    {
        return;
    }

    EventBits_t wait_for = 0;

    if (feed_task_handle_ != nullptr)
    {
        wait_for |= FEED_STOPPED;
    }

    if (fetch_task_handle_ != nullptr)
    {
        wait_for |= FETCH_STOPPED;
    }

    if (wait_for == 0)
    {
        return;
    }

    // Espressif'in Arduino ESP_SR implementasyonundaki lifecycle ile ayni fikir:
    // task'lari disaridan zorla silmek yerine kapanma sinyali ver ve cikmalarini bekle.
    xEventGroupSetBits(task_events_, STOP_REQUESTED);

    xEventGroupWaitBits(
        task_events_,
        wait_for,
        pdFALSE,
        pdTRUE,
        portMAX_DELAY
    );

    feed_task_handle_ = nullptr;
    fetch_task_handle_ = nullptr;
}

void SpeechFrontend::release_afe()
{
    if (afe_data_ != nullptr && afe_handle_ != nullptr)
    {
        afe_handle_->destroy(afe_data_);
        afe_data_ = nullptr;
    }

    afe_handle_ = nullptr;

    if (models_ != nullptr)
    {
        esp_srmodel_deinit(models_);
        models_ = nullptr;
    }

    feed_chunk_size_ = 0;
    fetch_chunk_size_ = 0;
}

void SpeechFrontend::end()
{
    initialized_ = false;

    stop_tasks();
    release_afe();

    if (task_events_ != nullptr)
    {
        vEventGroupDelete(task_events_);
        task_events_ = nullptr;
    }

    feed_task_handle_ = nullptr;
    fetch_task_handle_ = nullptr;

    audio_input_ = nullptr;
    callback_ = nullptr;
    callback_context_ = nullptr;
}
