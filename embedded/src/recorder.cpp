#include "recorder.h"

#include <Arduino.h>

#include <cmath>
#include <cstring>

#include <esp_heap_caps.h>

#include "config.h"
#include "wav_header.h"


namespace
{
    /*
     * 64 sample @ 16 kHz = 4 ms.
     */
    constexpr size_t RAW_SAMPLE_BUFFER_SIZE = 64;

    constexpr uint32_t I2S_READ_TIMEOUT_MS = 30;

    constexpr uint32_t FRAME_DURATION_MS =
        (
            RAW_SAMPLE_BUFFER_SIZE *
            1000UL
        ) /
        Recorder::SAMPLE_RATE;
}


Recorder::~Recorder()
{
    if (i2s_initialized_)
    {
        i2s_.end();

        i2s_initialized_ = false;
    }

    if (buffer_ != nullptr)
    {
        heap_caps_free(
            buffer_
        );

        buffer_ = nullptr;
    }

    if (pre_roll_buffer_ != nullptr)
    {
        heap_caps_free(
            pre_roll_buffer_
        );

        pre_roll_buffer_ = nullptr;
    }
}


bool Recorder::begin()
{
    // =========================
    // WAV buffer
    // =========================

    buffer_capacity_ =
        wav_header::SIZE +
        (
            static_cast<size_t>(
                SAMPLE_RATE
            ) *
            MAX_RECORD_DURATION_MS /
            1000UL *
            CHANNELS *
            (
                BITS_PER_SAMPLE /
                8
            )
        );

    buffer_ =
        static_cast<uint8_t*>(
            heap_caps_malloc(
                buffer_capacity_,
                MALLOC_CAP_SPIRAM |
                MALLOC_CAP_8BIT
            )
        );

    if (buffer_ == nullptr)
    {
        Serial.println(
            "HATA: Recorder PSRAM buffer ayrilamadi."
        );

        return false;
    }

    // =========================
    // Pre-roll buffer
    // =========================

    pre_roll_capacity_samples_ =
        (
            static_cast<size_t>(
                SAMPLE_RATE
            ) *
            VAD_PRE_ROLL_MS
        ) /
        1000UL;

    pre_roll_buffer_ =
        static_cast<int16_t*>(
            heap_caps_malloc(
                pre_roll_capacity_samples_ *
                    sizeof(int16_t),
                MALLOC_CAP_SPIRAM |
                MALLOC_CAP_8BIT
            )
        );

    if (pre_roll_buffer_ == nullptr)
    {
        Serial.println(
            "HATA: VAD pre-roll buffer ayrilamadi."
        );

        heap_caps_free(
            buffer_
        );

        buffer_ = nullptr;

        return false;
    }

    // =========================
    // I2S
    // =========================

    i2s_.setPins(
        MIC_SCK_PIN,
        MIC_WS_PIN,
        -1,
        MIC_SD_PIN
    );

    const bool started =
        i2s_.begin(
            I2S_MODE_STD,
            SAMPLE_RATE,
            I2S_DATA_BIT_WIDTH_32BIT,
            I2S_SLOT_MODE_MONO,
            I2S_STD_SLOT_LEFT
        );

    if (!started)
    {
        Serial.println(
            "HATA: INMP441 I2S baslatilamadi."
        );

        heap_caps_free(
            pre_roll_buffer_
        );

        pre_roll_buffer_ = nullptr;

        heap_caps_free(
            buffer_
        );

        buffer_ = nullptr;

        return false;
    }

    i2s_initialized_ = true;

    i2s_.setTimeout(
        I2S_READ_TIMEOUT_MS
    );

    Serial.println(
        "INMP441 I2S: OK"
    );

    Serial.print(
        "Recorder max buffer: "
    );

    Serial.print(
        buffer_capacity_
    );

    Serial.println(
        " byte"
    );

    Serial.print(
        "VAD pre-roll: "
    );

    Serial.print(
        VAD_PRE_ROLL_MS
    );

    Serial.println(
        " ms"
    );

    Serial.print(
        "PSRAM toplam: "
    );

    Serial.print(
        ESP.getPsramSize()
    );

    Serial.println(
        " byte"
    );

    Serial.print(
        "PSRAM bos: "
    );

    Serial.print(
        ESP.getFreePsram()
    );

    Serial.println(
        " byte"
    );

    return true;
}


bool Recorder::start_listening()
{
    if (
        !i2s_initialized_ ||
        buffer_ == nullptr ||
        pre_roll_buffer_ == nullptr
    )
    {
        return false;
    }

    /*
     * Player calisirken mikrofon RX tarafini
     * okumadigimiz icin DMA'da eski birkac
     * frame kalabilir.
     *
     * Sabit miktarda eski veriyi atiyoruz.
     *
     * while(available()) KULLANMIYORUZ.
     */
    int32_t discard_buffer[
        RAW_SAMPLE_BUFFER_SIZE
    ];

    for (
        uint8_t i = 0;
        i < 8;
        ++i
    )
    {
        i2s_.readBytes(
            reinterpret_cast<char*>(
                discard_buffer
            ),
            sizeof(discard_buffer)
        );
    }

    reset_waiting_state();

    Serial.println();

    Serial.println(
        "[VAD] Dinleme aktif."
    );

    Serial.println(
        "[VAD] Konusma bekleniyor..."
    );

    return true;
}


void Recorder::reset_waiting_state()
{
    state_ =
        recorder_state::waiting_for_speech;

    pcm_written_ = 0;

    last_wav_size_ = 0;

    pre_roll_count_ = 0;

    pre_roll_write_index_ = 0;

    voice_confirm_frames_ = 0;

    continuation_voice_frames_ = 0;

    recording_started_ms_ = 0;

    last_voice_ms_ = 0;

    voiced_duration_ms_ = 0;

    noise_rms_ =
        VAD_INITIAL_NOISE_RMS;

    smoothed_rms_ = 0.0f;

    last_debug_ms_ =
        millis();
}


void Recorder::stop()
{
    state_ =
        recorder_state::idle;

    pcm_written_ = 0;

    last_wav_size_ = 0;

    pre_roll_count_ = 0;

    pre_roll_write_index_ = 0;

    voice_confirm_frames_ = 0;

    continuation_voice_frames_ = 0;

    recording_started_ms_ = 0;

    last_voice_ms_ = 0;

    voiced_duration_ms_ = 0;

    smoothed_rms_ = 0.0f;
}


void Recorder::update()
{
    if (
        state_ ==
            recorder_state::idle ||
        state_ ==
            recorder_state::ready
    )
    {
        return;
    }

    /*
     * available() kullanmiyoruz.
     *
     * Her update'te yalnizca tek bir
     * 4 ms frame okuyoruz.
     */
    int32_t raw_samples[
        RAW_SAMPLE_BUFFER_SIZE
    ];

    const size_t bytes_read =
        i2s_.readBytes(
            reinterpret_cast<char*>(
                raw_samples
            ),
            sizeof(raw_samples)
        );

    if (bytes_read == 0)
    {
        return;
    }

    const size_t sample_count =
        bytes_read /
        sizeof(int32_t);

    if (sample_count == 0)
    {
        return;
    }

    int16_t pcm_samples[
        RAW_SAMPLE_BUFFER_SIZE
    ];

    for (
        size_t i = 0;
        i < sample_count;
        ++i
    )
    {
        pcm_samples[i] =
            static_cast<int16_t>(
                raw_samples[i] >> 14
            );
    }

    process_samples(
        pcm_samples,
        sample_count
    );
}


float Recorder::calculate_rms(
    const int16_t* samples,
    size_t sample_count
)
{
    if (
        samples == nullptr ||
        sample_count == 0
    )
    {
        return 0.0f;
    }

    /*
     * DC offset'i cikart.
     */
    int64_t sum = 0;

    for (
        size_t i = 0;
        i < sample_count;
        ++i
    )
    {
        sum += samples[i];
    }

    const float mean =
        static_cast<float>(
            sum
        ) /
        static_cast<float>(
            sample_count
        );

    double squared_sum = 0.0;

    for (
        size_t i = 0;
        i < sample_count;
        ++i
    )
    {
        const float centered =
            static_cast<float>(
                samples[i]
            ) -
            mean;

        squared_sum +=
            static_cast<double>(
                centered
            ) *
            static_cast<double>(
                centered
            );
    }

    return sqrtf(
        static_cast<float>(
            squared_sum /
            sample_count
        )
    );
}


float Recorder::current_voice_threshold() const
{
    float threshold =
        noise_rms_ *
        VAD_NOISE_MULTIPLIER;

    if (
        threshold <
        VAD_MIN_RMS_THRESHOLD
    )
    {
        threshold =
            VAD_MIN_RMS_THRESHOLD;
    }

    return threshold;
}


void Recorder::push_pre_roll(
    const int16_t* samples,
    size_t sample_count
)
{
    if (
        samples == nullptr ||
        sample_count == 0 ||
        pre_roll_capacity_samples_ == 0
    )
    {
        return;
    }

    for (
        size_t i = 0;
        i < sample_count;
        ++i
    )
    {
        pre_roll_buffer_[
            pre_roll_write_index_
        ] = samples[i];

        ++pre_roll_write_index_;

        if (
            pre_roll_write_index_ >=
            pre_roll_capacity_samples_
        )
        {
            pre_roll_write_index_ = 0;
        }

        if (
            pre_roll_count_ <
            pre_roll_capacity_samples_
        )
        {
            ++pre_roll_count_;
        }
    }
}


void Recorder::copy_pre_roll_to_recording()
{
    if (
        pre_roll_count_ == 0
    )
    {
        return;
    }

    size_t start_index = 0;

    if (
        pre_roll_count_ ==
        pre_roll_capacity_samples_
    )
    {
        start_index =
            pre_roll_write_index_;
    }

    for (
        size_t i = 0;
        i < pre_roll_count_;
        ++i
    )
    {
        const size_t index =
            (
                start_index + i
            ) %
            pre_roll_capacity_samples_;

        append_pcm(
            &pre_roll_buffer_[index],
            1
        );
    }
}


void Recorder::append_pcm(
    const int16_t* samples,
    size_t sample_count
)
{
    if (
        samples == nullptr ||
        sample_count == 0
    )
    {
        return;
    }

    uint8_t* pcm_start =
        buffer_ +
        wav_header::SIZE;

    const size_t pcm_capacity =
        buffer_capacity_ -
        wav_header::SIZE;

    if (
        pcm_written_ >=
        pcm_capacity
    )
    {
        return;
    }

    const size_t requested_bytes =
        sample_count *
        sizeof(int16_t);

    const size_t remaining =
        pcm_capacity -
        pcm_written_;

    size_t bytes_to_write =
        requested_bytes;

    if (
        bytes_to_write >
        remaining
    )
    {
        bytes_to_write =
            remaining;
    }

    memcpy(
        pcm_start +
            pcm_written_,
        samples,
        bytes_to_write
    );

    pcm_written_ +=
        bytes_to_write;
}


void Recorder::begin_recording()
{
    pcm_written_ = 0;

    /*
     * Konusma algilanmadan onceki
     * ~300 ms'i de kayda ekle.
     */
    copy_pre_roll_to_recording();

    state_ =
        recorder_state::recording;

    recording_started_ms_ =
        millis();

    /*
     * Konusma zaten 5 frame boyunca
     * dogrulandi.
     */
    last_voice_ms_ =
        recording_started_ms_;

    voiced_duration_ms_ =
        VAD_START_CONFIRM_FRAMES *
        FRAME_DURATION_MS;

    continuation_voice_frames_ =
        VAD_CONTINUE_CONFIRM_FRAMES;

    Serial.println();

    Serial.println(
        "[VAD] KONUSMA BASLADI."
    );
}


void Recorder::finish_recording()
{
    if (
        voiced_duration_ms_ <
        VAD_MIN_SPEECH_MS
    )
    {
        reject_false_trigger();

        return;
    }

    wav_header::write(
        buffer_,
        static_cast<uint32_t>(
            pcm_written_
        ),
        SAMPLE_RATE,
        CHANNELS,
        BITS_PER_SAMPLE
    );

    last_wav_size_ =
        wav_header::SIZE +
        pcm_written_;

    state_ =
        recorder_state::ready;

    Serial.println();

    Serial.println(
        "[VAD] KONUSMA BITTI."
    );

    Serial.print(
        "[VAD] Kayit: "
    );

    Serial.print(
        last_wav_size_
    );

    Serial.print(
        " byte, sure: "
    );

    Serial.print(
        millis() -
        recording_started_ms_
    );

    Serial.println(
        " ms"
    );
}


void Recorder::reject_false_trigger()
{
    Serial.println(
        "[VAD] Kisa gurultu reddedildi."
    );

    reset_waiting_state();
}


void Recorder::process_samples(
    const int16_t* samples,
    size_t sample_count
)
{
    if (
        samples == nullptr ||
        sample_count == 0
    )
    {
        return;
    }

    const uint32_t now =
        millis();

    // =========================
    // RMS
    // =========================

    const float rms =
        calculate_rms(
            samples,
            sample_count
        );

    /*
     * RMS smoothing.
     *
     * Ilk frame'de direkt ham RMS'i al.
     * Sonrakilerde EMA uygula.
     */
    if (smoothed_rms_ <= 0.0f)
    {
        smoothed_rms_ = rms;
    }
    else
    {
        smoothed_rms_ =
            (
                smoothed_rms_ *
                (
                    1.0f -
                    VAD_RMS_SMOOTHING_ALPHA
                )
            ) +
            (
                rms *
                VAD_RMS_SMOOTHING_ALPHA
            );
    }

    const float threshold =
        current_voice_threshold();

    /*
     * VAD karari artik ham RMS yerine
     * smooth RMS ile veriliyor.
     */
    const bool voice_detected =
        smoothed_rms_ >=
        threshold;

    // =========================
    // Debug
    // =========================

    if (
        VAD_DEBUG &&
        now - last_debug_ms_ >=
            VAD_DEBUG_INTERVAL_MS
    )
    {
        last_debug_ms_ = now;

        Serial.print(
            "[VAD] RMS="
        );

        Serial.print(
            rms,
            1
        );

        Serial.print(
            " smooth="
        );

        Serial.print(
            smoothed_rms_,
            1
        );

        Serial.print(
            " noise="
        );

        Serial.print(
            noise_rms_,
            1
        );

        Serial.print(
            " threshold="
        );

        Serial.print(
            threshold,
            1
        );

        Serial.print(
            " state="
        );

        if (
            state_ ==
            recorder_state::waiting_for_speech
        )
        {
            Serial.println(
                "WAIT"
            );
        }
        else
        {
            Serial.println(
                "REC"
            );
        }
    }

    // =========================
    // WAITING FOR SPEECH
    // =========================

    if (
        state_ ==
        recorder_state::waiting_for_speech
    )
    {
        push_pre_roll(
            samples,
            sample_count
        );

        if (voice_detected)
        {
            if (
                voice_confirm_frames_ <
                255
            )
            {
                ++voice_confirm_frames_;
            }

            if (
                voice_confirm_frames_ >=
                VAD_START_CONFIRM_FRAMES
            )
            {
                begin_recording();
            }

            return;
        }

        voice_confirm_frames_ = 0;

        /*
         * Yalnizca gercek sessizlikte
         * noise floor'u takip et.
         */
        noise_rms_ =
            (
                noise_rms_ *
                0.97f
            ) +
            (
                rms *
                0.03f
            );

        if (
            noise_rms_ >
            VAD_MAX_NOISE_RMS
        )
        {
            noise_rms_ =
                VAD_MAX_NOISE_RMS;
        }

        return;
    }

    // =========================
    // RECORDING
    // =========================

    if (
        state_ !=
        recorder_state::recording
    )
    {
        return;
    }

    /*
     * Kayit sirasinda sessizlik dahil tum
     * PCM'i WAV'e yaz.
     */
    append_pcm(
        samples,
        sample_count
    );

    /*
     * KRITIK DUZELTME:
     *
     * Tek bir yuksek frame artik
     * last_voice_ms_ degerini YENILEMEZ.
     *
     * Sesin birkac frame boyunca devam
     * etmesi gerekir.
     */
    if (voice_detected)
    {
        if (
            continuation_voice_frames_ <
            255
        )
        {
            ++continuation_voice_frames_;
        }

        if (
            continuation_voice_frames_ >=
            VAD_CONTINUE_CONFIRM_FRAMES
        )
        {
            last_voice_ms_ = now;

            voiced_duration_ms_ +=
                FRAME_DURATION_MS;
        }
    }
    else
    {
        continuation_voice_frames_ = 0;
    }

    const uint32_t recording_duration =
        now -
        recording_started_ms_;

    // =========================
    // Maksimum sure
    // =========================

    if (
        recording_duration >=
        MAX_RECORD_DURATION_MS
    )
    {
        Serial.println(
            "[VAD] Maksimum kayit suresi."
        );

        finish_recording();

        return;
    }

    // =========================
    // Cumle sonu
    // =========================

    /*
     * Son DOGRULANMIS insan sesinden
     * itibaren 900 ms gercek sessizlik.
     *
     * Tek tik / elektriksel spike /
     * vuruntu artik bu zamani sifirlamaz.
     */
    if (
        now - last_voice_ms_ >=
        END_OF_SPEECH_SILENCE_MS
    )
    {
        finish_recording();
    }
}