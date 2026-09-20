#include <Arduino.h>
#include <ESP_I2S.h>

#include "esp_afe_config.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_nsn_models.h"
#include "esp_vadn_models.h"
#include "model_path.h"


constexpr int MIC_BCLK_PIN = 15;
constexpr int MIC_WS_PIN = 16;
constexpr int MIC_DATA_PIN = 17;

constexpr uint32_t SAMPLE_RATE = 16000;


I2SClass i2s;

static srmodel_list_t* models = nullptr;
static afe_config_t* afe_config = nullptr;

static const esp_afe_sr_iface_t* afe_handle = nullptr;
static esp_afe_sr_data_t* afe_data = nullptr;


// ------------------------------------------------------------
// INMP441
// ------------------------------------------------------------

bool begin_microphone()
{
    i2s.setPins(
        MIC_BCLK_PIN,
        MIC_WS_PIN,
        -1,
        MIC_DATA_PIN
    );

    i2s.setTimeout(1000);

    const bool success = i2s.begin(
        I2S_MODE_STD,
        SAMPLE_RATE,
        I2S_DATA_BIT_WIDTH_32BIT,
        I2S_SLOT_MODE_MONO,
        I2S_STD_SLOT_LEFT
    );

    if (!success)
    {
        Serial.println(
            "HATA: I2S mikrofon baslatilamadi."
        );

        return false;
    }

    Serial.println("INMP441: OK");

    return true;
}


// ------------------------------------------------------------
// AFE feed task
// ------------------------------------------------------------

void afe_feed_task(void* parameter)
{
    const int feed_chunk_size =
        afe_handle->get_feed_chunksize(
            afe_data
        );

    const int feed_channels =
        afe_handle->get_feed_channel_num(
            afe_data
        );

    Serial.print("Feed task chunk: ");
    Serial.println(feed_chunk_size);

    Serial.print("Feed task channels: ");
    Serial.println(feed_channels);

    if (feed_channels != 1)
    {
        Serial.println(
            "HATA: AFE tek kanal olmali."
        );

        vTaskDelete(nullptr);
        return;
    }

    int32_t* raw_buffer =
        static_cast<int32_t*>(
            heap_caps_malloc(
                feed_chunk_size *
                    sizeof(int32_t),
                MALLOC_CAP_8BIT
            )
        );

    int16_t* feed_buffer =
        static_cast<int16_t*>(
            heap_caps_malloc(
                feed_chunk_size *
                    sizeof(int16_t),
                MALLOC_CAP_8BIT
            )
        );

    if (
        raw_buffer == nullptr ||
        feed_buffer == nullptr
    )
    {
        Serial.println(
            "HATA: Feed buffer allocation."
        );

        vTaskDelete(nullptr);
        return;
    }

    const size_t raw_bytes_required =
        feed_chunk_size *
        sizeof(int32_t);

    while (true)
    {
        size_t total_bytes_read = 0;

        while (
            total_bytes_read <
            raw_bytes_required
        )
        {
            const size_t bytes_read =
                i2s.readBytes(
                    reinterpret_cast<char*>(
                        raw_buffer
                    ) +
                        total_bytes_read,
                    raw_bytes_required -
                        total_bytes_read
                );

            if (bytes_read == 0)
            {
                continue;
            }

            total_bytes_read += bytes_read;
        }

        for (
            int i = 0;
            i < feed_chunk_size;
            ++i
        )
        {
            int32_t sample =
                raw_buffer[i] >> 14;

            if (sample > 32767)
            {
                sample = 32767;
            }
            else if (sample < -32768)
            {
                sample = -32768;
            }

            feed_buffer[i] =
                static_cast<int16_t>(
                    sample
                );
        }

        afe_handle->feed(
            afe_data,
            feed_buffer
        );
    }
}


// ------------------------------------------------------------
// AFE VAD task
// ------------------------------------------------------------

void afe_detect_task(void* parameter)
{
    vad_state_t previous_state =
        VAD_SILENCE;

    Serial.println();
    Serial.println("==============================");
    Serial.println("VAD TEST BASLADI");
    Serial.println("==============================");
    Serial.println("SILENCE");

    while (true)
    {
        afe_fetch_result_t* result =
            afe_handle->fetch(
                afe_data
            );

        if (result == nullptr)
        {
            Serial.println(
                "HATA: AFE fetch nullptr."
            );

            delay(100);
            continue;
        }

        if (result->ret_value == ESP_FAIL)
        {
            Serial.println(
                "HATA: AFE fetch basarisiz."
            );

            delay(100);
            continue;
        }

        const vad_state_t current_state =
            result->vad_state;

        if (current_state == previous_state)
        {
            continue;
        }

        previous_state = current_state;

        if (current_state == VAD_SPEECH)
        {
            Serial.println(
                ">>> SPEECH"
            );
        }
        else
        {
            Serial.println(
                "<<< SILENCE"
            );
        }
    }
}


// ------------------------------------------------------------
// Setup
// ------------------------------------------------------------

void setup()
{
    Serial.begin(115200);
    delay(1500);

    Serial.println();
    Serial.println("==============================");
    Serial.println("DURSUN EMICE");
    Serial.println("NSNET2 + VADNET TEST");
    Serial.println("==============================");

    if (!begin_microphone())
    {
        return;
    }

    models = esp_srmodel_init(
        "model"
    );

    if (models == nullptr)
    {
        Serial.println(
            "HATA: ESP-SR modelleri yuklenemedi."
        );

        return;
    }

    char* ns_model_name =
        esp_srmodel_filter(
            models,
            ESP_NSNET_PREFIX,
            nullptr
        );

    char* vad_model_name =
        esp_srmodel_filter(
            models,
            ESP_VADN_PREFIX,
            nullptr
        );

    Serial.print("NS: ");
    Serial.println(
        ns_model_name
            ? ns_model_name
            : "YOK"
    );

    Serial.print("VAD: ");
    Serial.println(
        vad_model_name
            ? vad_model_name
            : "YOK"
    );

    if (
        ns_model_name == nullptr ||
        vad_model_name == nullptr
    )
    {
        Serial.println(
            "HATA: Gerekli modeller yok."
        );

        return;
    }

    afe_config = afe_config_init(
        "M",
        models,
        AFE_TYPE_SR,
        AFE_MODE_HIGH_PERF
    );

    if (afe_config == nullptr)
    {
        Serial.println(
            "HATA: AFE config."
        );

        return;
    }

    // Sadece NSNet2 + VADNet.
    afe_config->aec_init = false;
    afe_config->se_init = false;
    afe_config->agc_init = false;
    afe_config->wakenet_init = false;

    afe_config->ns_init = true;
    afe_config->ns_model_name =
        ns_model_name;

    afe_config->afe_ns_mode =
        AFE_NS_MODE_NET;

    afe_config->vad_init = true;
    afe_config->vad_model_name =
        vad_model_name;

    afe_config->memory_alloc_mode =
        AFE_MEMORY_ALLOC_MORE_PSRAM;

    afe_handle =
        esp_afe_handle_from_config(
            afe_config
        );

    if (afe_handle == nullptr)
    {
        Serial.println(
            "HATA: AFE handle."
        );

        return;
    }

    afe_data =
        afe_handle->create_from_config(
            afe_config
        );

    if (afe_data == nullptr)
    {
        Serial.println(
            "HATA: AFE instance."
        );

        return;
    }

    Serial.println();
    Serial.println("AFE: OK");

    Serial.print("Sample rate: ");
    Serial.println(
        afe_handle->get_samp_rate(
            afe_data
        )
    );

    Serial.print("Feed chunk: ");
    Serial.println(
        afe_handle->get_feed_chunksize(
            afe_data
        )
    );

    Serial.print("Fetch chunk: ");
    Serial.println(
        afe_handle->get_fetch_chunksize(
            afe_data
        )
    );

    xTaskCreatePinnedToCore(
        afe_feed_task,
        "afe_feed",
        6144,
        nullptr,
        5,
        nullptr,
        0
    );

    xTaskCreatePinnedToCore(
        afe_detect_task,
        "afe_detect",
        6144,
        nullptr,
        5,
        nullptr,
        1
    );
}


void loop()
{
    delay(1000);
}