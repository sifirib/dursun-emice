#include <Arduino.h>
#include <ESP_I2S.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <esp_heap_caps.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esp_afe_config.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_nsn_models.h"
#include "model_path.h"


// ============================================================
// Donanim
// ============================================================

constexpr int MIC_SCK_PIN = 15;
constexpr int MIC_WS_PIN = 16;
constexpr int MIC_SD_PIN = 17;


// ============================================================
// Test ayarlari
// ============================================================

constexpr uint32_t SAMPLE_RATE = 16000;

// Tek kayitta 8 saniye.
// START'tan sonra ilk 1 saniye bekleyip sonra konus.
constexpr uint32_t CAPTURE_DURATION_SECONDS = 8;

constexpr size_t TARGET_SAMPLES =
    SAMPLE_RATE * CAPTURE_DURATION_SECONDS;


// ============================================================
// Global
// ============================================================

I2SClass i2s;

srmodel_list_t* models = nullptr;

const esp_afe_sr_iface_t* afe_handle = nullptr;
esp_afe_sr_data_t* afe_data = nullptr;

size_t feed_chunk_size = 0;
size_t fetch_chunk_size = 0;

size_t raw_frame_target = 0;
size_t filtered_frame_target = 0;

size_t raw_capacity_samples = 0;
size_t filtered_capacity_samples = 0;

int16_t* raw_capture = nullptr;
int16_t* filtered_capture = nullptr;

std::atomic<size_t> raw_frames_written { 0 };
std::atomic<size_t> filtered_frames_written { 0 };

std::atomic<bool> test_running { false };
std::atomic<bool> test_complete { false };
std::atomic<bool> stop_tasks { false };
std::atomic<bool> system_ready { false };

TaskHandle_t feed_task_handle = nullptr;
TaskHandle_t fetch_task_handle = nullptr;


// ============================================================
// Yardimci
// ============================================================

size_t divide_round_up(
    const size_t value,
    const size_t divisor
)
{
    return (value + divisor - 1) / divisor;
}


// ============================================================
// I2S
// ============================================================

bool init_microphone()
{
    i2s.setPins(
        MIC_SCK_PIN,
        MIC_WS_PIN,
        -1,
        MIC_SD_PIN
    );

    i2s.setTimeout(1000);

    const bool ok =
        i2s.begin(
            I2S_MODE_STD,
            SAMPLE_RATE,
            I2S_DATA_BIT_WIDTH_32BIT,
            I2S_SLOT_MODE_MONO,
            I2S_STD_SLOT_LEFT
        );

    if (!ok)
    {
        Serial.println(
            "HATA: INMP441 I2S baslatilamadi."
        );

        return false;
    }

    Serial.println(
        "INMP441: OK"
    );

    return true;
}


// ============================================================
// ESP-SR AFE
// ============================================================

bool init_afe()
{
    models =
        esp_srmodel_init(
            "model"
        );

    if (models == nullptr)
    {
        Serial.println(
            "HATA: model partition acilamadi."
        );

        return false;
    }


    char* ns_model_name =
        esp_srmodel_filter(
            models,
            ESP_NSNET_PREFIX,
            nullptr
        );

    if (ns_model_name == nullptr)
    {
        Serial.println(
            "HATA: NSNet modeli bulunamadi."
        );

        return false;
    }


    Serial.print(
        "NS model: "
    );

    Serial.println(
        ns_model_name
    );


    afe_config_t* afe_config =
        afe_config_init(
            "M",
            models,
            AFE_TYPE_SR,
            AFE_MODE_HIGH_PERF
        );

    if (afe_config == nullptr)
    {
        Serial.println(
            "HATA: afe_config_init basarisiz."
        );

        return false;
    }


    // ========================================================
    // Bu testte SADECE NSNet2 aktif.
    //
    // AEC yok
    // VAD yok
    // WakeNet yok
    // AGC yok
    // SE yok
    //
    // Boylece RAW vs NSNet2'yi izole ediyoruz.
    // ========================================================

    afe_config->aec_init = false;
    afe_config->se_init = false;
    afe_config->vad_init = false;
    afe_config->wakenet_init = false;
    afe_config->agc_init = false;

    afe_config->ns_init = true;

    afe_config->ns_model_name =
        ns_model_name;

    afe_config->afe_ns_mode =
        AFE_NS_MODE_NET;

    afe_config->memory_alloc_mode =
        AFE_MEMORY_ALLOC_MORE_PSRAM;


    afe_handle =
        esp_afe_handle_from_config(
            afe_config
        );

    if (afe_handle == nullptr)
    {
        Serial.println(
            "HATA: AFE handle alinamadi."
        );

        afe_config_free(
            afe_config
        );

        return false;
    }


    afe_data =
        afe_handle->create_from_config(
            afe_config
        );

    afe_config_free(
        afe_config
    );


    if (afe_data == nullptr)
    {
        Serial.println(
            "HATA: AFE instance olusturulamadi."
        );

        return false;
    }


    const int feed_chunk =
        afe_handle->get_feed_chunksize(
            afe_data
        );

    const int fetch_chunk =
        afe_handle->get_fetch_chunksize(
            afe_data
        );

    const int feed_channels =
        afe_handle->get_feed_channel_num(
            afe_data
        );

    const int afe_sample_rate =
        afe_handle->get_samp_rate(
            afe_data
        );


    if (
        feed_chunk <= 0 ||
        fetch_chunk <= 0
    )
    {
        Serial.println(
            "HATA: Gecersiz AFE chunk size."
        );

        return false;
    }


    if (feed_channels != 1)
    {
        Serial.print(
            "HATA: AFE input channel = "
        );

        Serial.println(
            feed_channels
        );

        return false;
    }


    if (
        afe_sample_rate !=
        static_cast<int>(SAMPLE_RATE)
    )
    {
        Serial.print(
            "HATA: AFE sample rate = "
        );

        Serial.println(
            afe_sample_rate
        );

        return false;
    }


    feed_chunk_size =
        static_cast<size_t>(
            feed_chunk
        );

    fetch_chunk_size =
        static_cast<size_t>(
            fetch_chunk
        );


    raw_frame_target =
        divide_round_up(
            TARGET_SAMPLES,
            feed_chunk_size
        );

    filtered_frame_target =
        divide_round_up(
            TARGET_SAMPLES,
            fetch_chunk_size
        );


    raw_capacity_samples =
        raw_frame_target *
        feed_chunk_size;

    filtered_capacity_samples =
        filtered_frame_target *
        fetch_chunk_size;


    Serial.println();
    Serial.println(
        "AFE: OK"
    );

    Serial.print(
        "Sample rate: "
    );

    Serial.println(
        afe_sample_rate
    );

    Serial.print(
        "Feed chunk: "
    );

    Serial.println(
        feed_chunk_size
    );

    Serial.print(
        "Fetch chunk: "
    );

    Serial.println(
        fetch_chunk_size
    );

    afe_handle->print_pipeline(
        afe_data
    );


    return true;
}


// ============================================================
// PSRAM
// ============================================================

bool allocate_buffers()
{
    raw_capture =
        static_cast<int16_t*>(
            heap_caps_malloc(
                raw_capacity_samples *
                sizeof(int16_t),

                MALLOC_CAP_SPIRAM |
                MALLOC_CAP_8BIT
            )
        );


    filtered_capture =
        static_cast<int16_t*>(
            heap_caps_malloc(
                filtered_capacity_samples *
                sizeof(int16_t),

                MALLOC_CAP_SPIRAM |
                MALLOC_CAP_8BIT
            )
        );


    if (
        raw_capture == nullptr ||
        filtered_capture == nullptr
    )
    {
        Serial.println(
            "HATA: PSRAM buffer ayrilamadi."
        );

        return false;
    }


    memset(
        raw_capture,
        0,
        raw_capacity_samples *
            sizeof(int16_t)
    );


    memset(
        filtered_capture,
        0,
        filtered_capacity_samples *
            sizeof(int16_t)
    );


    Serial.print(
        "RAW buffer: "
    );

    Serial.print(
        raw_capacity_samples *
            sizeof(int16_t)
    );

    Serial.println(
        " byte"
    );


    Serial.print(
        "FILTERED buffer: "
    );

    Serial.print(
        filtered_capacity_samples *
            sizeof(int16_t)
    );

    Serial.println(
        " byte"
    );


    return true;
}


// ============================================================
// Feed task
//
// KRITIK NOKTA:
//
// INMP441 yalnizca BIR KEZ okunuyor.
//
// feed_buffer:
//
//     1. RAW capture'a kopyalaniyor.
//     2. AYNI buffer AFE / NSNet2'ye veriliyor.
//
// Iki farkli mikrofon okumasi YOK.
// ============================================================

void feed_task(
    void* parameter
)
{
    (void)parameter;


    int32_t* i2s_buffer =
        static_cast<int32_t*>(
            heap_caps_malloc(
                feed_chunk_size *
                    sizeof(int32_t),

                MALLOC_CAP_INTERNAL |
                MALLOC_CAP_8BIT
            )
        );


    int16_t* feed_buffer =
        static_cast<int16_t*>(
            heap_caps_malloc(
                feed_chunk_size *
                    sizeof(int16_t),

                MALLOC_CAP_INTERNAL |
                MALLOC_CAP_8BIT
            )
        );


    if (
        i2s_buffer == nullptr ||
        feed_buffer == nullptr
    )
    {
        Serial.println(
            "HATA: feed task buffer."
        );

        stop_tasks.store(
            true
        );

        vTaskDelete(
            nullptr
        );

        return;
    }


    const size_t required_bytes =
        feed_chunk_size *
        sizeof(int32_t);


    while (!stop_tasks.load())
    {
        size_t received_bytes = 0;


        while (
            received_bytes <
                required_bytes &&
            !stop_tasks.load()
        )
        {
            const size_t count =
                i2s.readBytes(
                    reinterpret_cast<char*>(
                        i2s_buffer
                    ) +
                        received_bytes,

                    required_bytes -
                        received_bytes
                );


            if (count == 0)
            {
                continue;
            }


            received_bytes +=
                count;
        }


        if (stop_tasks.load())
        {
            break;
        }


        // ====================================================
        // INMP441 32-bit slot -> int16 PCM
        //
        // Su an production'da kullandigimiz AYNI donusum.
        // Dolayisiyla test tam olarak mevcut sistemimizi
        // karsilastiriyor.
        // ====================================================

        for (
            size_t i = 0;
            i < feed_chunk_size;
            ++i
        )
        {
            int32_t sample =
                i2s_buffer[i] >> 14;


            sample =
                std::clamp<int32_t>(
                    sample,
                    -32768,
                    32767
                );


            feed_buffer[i] =
                static_cast<int16_t>(
                    sample
                );
        }


        // ====================================================
        // RAW
        // ====================================================

        if (test_running.load())
        {
            const size_t frame_index =
                raw_frames_written.load();


            if (
                frame_index <
                    raw_frame_target
            )
            {
                int16_t* destination =
                    raw_capture +
                    (
                        frame_index *
                        feed_chunk_size
                    );


                memcpy(
                    destination,
                    feed_buffer,
                    feed_chunk_size *
                        sizeof(int16_t)
                );


                raw_frames_written.store(
                    frame_index + 1
                );
            }
        }


        // ====================================================
        // AYNI FRAME NSNet2'ye gidiyor.
        // ====================================================

        afe_handle->feed(
            afe_data,
            feed_buffer
        );
    }


    heap_caps_free(
        feed_buffer
    );

    heap_caps_free(
        i2s_buffer
    );


    feed_task_handle = nullptr;


    vTaskDelete(
        nullptr
    );
}


// ============================================================
// Fetch task
// ============================================================

void fetch_task(
    void* parameter
)
{
    (void)parameter;


    while (!stop_tasks.load())
    {
        afe_fetch_result_t* result =
            afe_handle->fetch(
                afe_data
            );


        if (stop_tasks.load())
        {
            break;
        }


        if (result == nullptr)
        {
            continue;
        }


        if (
            result->ret_value ==
                ESP_FAIL ||
            result->data == nullptr ||
            result->data_size <= 0
        )
        {
            continue;
        }


        if (!test_running.load())
        {
            continue;
        }


        const size_t frame_index =
            filtered_frames_written.load();


        if (
            frame_index >=
                filtered_frame_target
        )
        {
            continue;
        }


        const size_t returned_samples =
            static_cast<size_t>(
                result->data_size
            ) /
            sizeof(int16_t);


        const size_t samples_to_copy =
            std::min(
                returned_samples,
                fetch_chunk_size
            );


        int16_t* destination =
            filtered_capture +
            (
                frame_index *
                fetch_chunk_size
            );


        memcpy(
            destination,
            result->data,
            samples_to_copy *
                sizeof(int16_t)
        );


        if (
            samples_to_copy <
                fetch_chunk_size
        )
        {
            memset(
                destination +
                    samples_to_copy,

                0,

                (
                    fetch_chunk_size -
                    samples_to_copy
                ) *
                    sizeof(int16_t)
            );
        }


        filtered_frames_written.store(
            frame_index + 1
        );
    }


    fetch_task_handle = nullptr;


    vTaskDelete(
        nullptr
    );
}


// ============================================================
// Test baslat
// ============================================================

bool start_test()
{
    if (test_running.load())
    {
        return false;
    }


    if (test_complete.load())
    {
        Serial.println(
            "Test zaten tamamlandi."
        );

        Serial.println(
            "Tekrar test icin ESP'yi resetle."
        );

        return false;
    }


    memset(
        raw_capture,
        0,
        raw_capacity_samples *
            sizeof(int16_t)
    );


    memset(
        filtered_capture,
        0,
        filtered_capacity_samples *
            sizeof(int16_t)
    );


    raw_frames_written.store(
        0
    );

    filtered_frames_written.store(
        0
    );

    stop_tasks.store(
        false
    );


    // Eski AFE ring-buffer verisini at.
    afe_handle->reset_buffer(
        afe_data
    );


    test_running.store(
        true
    );


    Serial.println();
    Serial.println(
        "======================================"
    );

    Serial.println(
        "TEST BASLADI"
    );

    Serial.println(
        "Ayni ses RAW + NSNet2 kaydediliyor."
    );

    Serial.println();
    Serial.println(
        "Ilk 1 saniye sessiz kal."
    );

    Serial.println(
        "Sonra normal sesle cumleni soyle."
    );

    Serial.println(
        "Toplam sure: 8 saniye"
    );

    Serial.println(
        "======================================"
    );


    BaseType_t result =
        xTaskCreatePinnedToCore(
            feed_task,
            "compare_feed",
            6144,
            nullptr,
            5,
            &feed_task_handle,
            0
        );


    if (result != pdPASS)
    {
        Serial.println(
            "HATA: feed task olusturulamadi."
        );

        test_running.store(
            false
        );

        return false;
    }


    result =
        xTaskCreatePinnedToCore(
            fetch_task,
            "compare_fetch",
            8192,
            nullptr,
            5,
            &fetch_task_handle,
            1
        );


    if (result != pdPASS)
    {
        Serial.println(
            "HATA: fetch task olusturulamadi."
        );

        stop_tasks.store(
            true
        );

        test_running.store(
            false
        );

        return false;
    }


    return true;
}


// ============================================================
// Binary dump
//
// Format:
//
// 8 byte magic:
// "AFECMP01"
//
// uint32 little endian:
// sample_rate
// raw_sample_count
// filtered_sample_count
//
// ardindan:
// raw int16 PCM
// filtered int16 PCM
//
// Boylece Python tarafinda Serial metin/binary karismiyor.
// ============================================================

void send_dump()
{
    if (!test_complete.load())
    {
        Serial.println(
            "HATA: Test henuz tamamlanmadi."
        );

        return;
    }


    static const uint8_t MAGIC[8] =
    {
        'A',
        'F',
        'E',
        'C',
        'M',
        'P',
        '0',
        '1'
    };


    const uint32_t sample_rate =
        SAMPLE_RATE;


    const uint32_t raw_samples =
        static_cast<uint32_t>(
            raw_capacity_samples
        );


    const uint32_t filtered_samples =
        static_cast<uint32_t>(
            filtered_capacity_samples
        );


    Serial.flush();


    Serial.write(
        MAGIC,
        sizeof(MAGIC)
    );


    Serial.write(
        reinterpret_cast<const uint8_t*>(
            &sample_rate
        ),
        sizeof(sample_rate)
    );


    Serial.write(
        reinterpret_cast<const uint8_t*>(
            &raw_samples
        ),
        sizeof(raw_samples)
    );


    Serial.write(
        reinterpret_cast<const uint8_t*>(
            &filtered_samples
        ),
        sizeof(filtered_samples)
    );


    Serial.write(
        reinterpret_cast<const uint8_t*>(
            raw_capture
        ),
        raw_capacity_samples *
            sizeof(int16_t)
    );


    Serial.write(
        reinterpret_cast<const uint8_t*>(
            filtered_capture
        ),
        filtered_capacity_samples *
            sizeof(int16_t)
    );


    Serial.flush();
}


// ============================================================
// setup
// ============================================================

void setup()
{
    Serial.begin(
        921600
    );

    delay(
        1500
    );


    Serial.println();
    Serial.println(
        "======================================"
    );

    Serial.println(
        "DURSUN EMICE - RAW / NSNET2 TEST"
    );

    Serial.println(
        "======================================"
    );


    if (!psramFound())
    {
        Serial.println(
            "HATA: PSRAM bulunamadi."
        );

        return;
    }


    Serial.print(
        "PSRAM free: "
    );

    Serial.println(
        ESP.getFreePsram()
    );


    if (!init_microphone())
    {
        return;
    }


    if (!init_afe())
    {
        return;
    }


    if (!allocate_buffers())
    {
        return;
    }

    system_ready.store(
        true
    );

    Serial.println();
    Serial.println(
        "AFE_COMPARE_READY"
    );

    Serial.println();
    Serial.println(
        "Komut:"
    );

    Serial.println(
        "START"
    );

    Serial.println(
        "Test bitince:"
    );

    Serial.println(
        "DUMP"
    );
}


// ============================================================
// loop
// ============================================================

void loop()
{
    if (test_running.load())
    {
        const bool raw_done =
            raw_frames_written.load() >=
            raw_frame_target;

        const bool filtered_done =
            filtered_frames_written.load() >=
            filtered_frame_target;

        if (
            raw_done &&
            filtered_done
        )
        {
            test_running.store(
                false
            );

            test_complete.store(
                true
            );

            stop_tasks.store(
                true
            );

            Serial.println();
            Serial.println(
                "CAPTURE_DONE"
            );
        }
    }


    if (Serial.available())
    {
        String command =
            Serial.readStringUntil(
                '\n'
            );

        command.trim();


        // PC -> ESP baglanti kontrolu.
        if (
            command.equalsIgnoreCase(
                "PING"
            )
        )
        {
            if (system_ready.load())
            {
                Serial.println(
                    "AFE_COMPARE_READY"
                );
            }
            else
            {
                Serial.println(
                    "AFE_COMPARE_NOT_READY"
                );
            }
        }


        else if (
            command.equalsIgnoreCase(
                "START"
            )
        )
        {
            if (!system_ready.load())
            {
                Serial.println(
                    "AFE_COMPARE_NOT_READY"
                );

                return;
            }

            start_test();
        }


        else if (
            command.equalsIgnoreCase(
                "DUMP"
            )
        )
        {
            send_dump();
        }
    }


    delay(
        1
    );
}