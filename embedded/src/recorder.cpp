#include "recorder.h"

#include <Arduino.h>

#include "config.h"

Recorder::~Recorder()
{
    free_resources();
}

bool Recorder::begin()
{
    free_resources();

    state_.store(recorder_state::idle);
    manual_speech_seen_.store(false);

    session_mutex_ = xSemaphoreCreateMutex();
    if (session_mutex_ == nullptr)
    {
        Serial.println("HATA: Recorder mutex olusturulamadi.");
        return false;
    }

    if (!wav_buffer_.begin(MAX_RECORD_DURATION_MS, VAD_PRE_ROLL_MS))
    {
        Serial.println("HATA: Recorder PSRAM buffer ayrilamadi.");
        free_resources();
        return false;
    }

    if (!audio_input_.begin())
    {
        free_resources();
        return false;
    }

    if (!speech_frontend_.begin(
        audio_input_,
        frontend_result_entry,
        this
    ))
    {
        free_resources();
        return false;
    }

    reset_session();
    initialized_ = true;

    Serial.println();
    Serial.println("Recorder: OK");
    Serial.print("Pre-roll fallback: ");
    Serial.print(VAD_PRE_ROLL_MS);
    Serial.println(" ms");
    Serial.print("VAD min speech: ");
    Serial.print(VAD_MIN_SPEECH_MS);
    Serial.println(" ms");
    Serial.print("VAD delay/cache: ");
    Serial.print(VAD_DELAY_MS);
    Serial.println(" ms");
    Serial.print("VAD end silence: ");
    Serial.print(END_OF_SPEECH_SILENCE_MS);
    Serial.println(" ms");

    return true;
}

bool Recorder::start_listening()
{
    if (!initialized_ || session_mutex_ == nullptr)
    {
        return false;
    }

    if (xSemaphoreTake(session_mutex_, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return false;
    }

    const recorder_state current = state_.load();
    if (
        current == recorder_state::waiting_for_speech ||
        current == recorder_state::recording
    )
    {
        xSemaphoreGive(session_mutex_);
        return false;
    }

    reset_session();
    capture_mode_ = capture_mode::automatic_vad;

    // Stage 2.1'de donanimda calistigi dogrulanan otomatik yol aynen korunur.
    if (!speech_frontend_.reset_vad())
    {
        xSemaphoreGive(session_mutex_);
        return false;
    }

    state_.store(recorder_state::waiting_for_speech);
    xSemaphoreGive(session_mutex_);

    Serial.println();
    Serial.println("DINLEME BASLADI...");
    Serial.println("[VAD] Algilama ARM edildi.");
    return true;
}

bool Recorder::start_manual_recording()
{
    if (!initialized_ || session_mutex_ == nullptr)
    {
        return false;
    }

    if (xSemaphoreTake(session_mutex_, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return false;
    }

    const recorder_state current = state_.load();
    if (
        current == recorder_state::waiting_for_speech ||
        current == recorder_state::recording
    )
    {
        xSemaphoreGive(session_mutex_);
        return false;
    }

    reset_session();
    capture_mode_ = capture_mode::manual;
    manual_speech_seen_.store(false);

    // Manuel mod VAD kararina bagli degil. Reset sadece greeting/TTS'den
    // kalan VAD state'ini temiz tutar; basarisiz olsa bile manuel kayit devam eder.
    if (!speech_frontend_.reset_vad())
    {
        Serial.println("UYARI: Manuel kayit basinda VAD reset basarisiz.");
    }

    recording_started_ms_ = millis();
    state_.store(recorder_state::recording);

    xSemaphoreGive(session_mutex_);

    Serial.println();
    Serial.println(">>> MANUEL KAYIT BASLADI");
    return true;
}

bool Recorder::finish_manual_recording()
{
    if (!initialized_ || session_mutex_ == nullptr)
    {
        return false;
    }

    if (xSemaphoreTake(session_mutex_, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return false;
    }

    const recorder_state current = state_.load();

    // Maksimum sure veya buffer dolmasi kaydi daha once bitirmis olabilir.
    if (current == recorder_state::ready)
    {
        xSemaphoreGive(session_mutex_);
        return true;
    }

    if (
        current != recorder_state::recording ||
        capture_mode_ != capture_mode::manual
    )
    {
        xSemaphoreGive(session_mutex_);
        return false;
    }

    finish_recording();
    const bool ready = state_.load() == recorder_state::ready;

    xSemaphoreGive(session_mutex_);
    return ready;
}

bool Recorder::pause_detection()
{
    if (!initialized_ || session_mutex_ == nullptr)
    {
        return false;
    }

    if (xSemaphoreTake(session_mutex_, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return false;
    }

    state_.store(recorder_state::idle);
    capture_mode_ = capture_mode::automatic_vad;
    reset_session();

    xSemaphoreGive(session_mutex_);

    Serial.println("[VAD] Algilama DISARM edildi.");
    return true;
}

void Recorder::update()
{
    // Mikrofon ve AFE FreeRTOS task'larinda calisiyor.
}

void Recorder::stop()
{
    pause_detection();

    if (session_mutex_ == nullptr)
    {
        state_.store(recorder_state::idle);
        return;
    }

    if (xSemaphoreTake(session_mutex_, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        state_.store(recorder_state::idle);
        capture_mode_ = capture_mode::automatic_vad;
        reset_session();
        xSemaphoreGive(session_mutex_);
    }
}

bool Recorder::is_active() const
{
    const recorder_state current = state_.load();
    return
        current == recorder_state::waiting_for_speech ||
        current == recorder_state::recording;
}

bool Recorder::is_recording() const
{
    return state_.load() == recorder_state::recording;
}

bool Recorder::has_recording() const
{
    return state_.load() == recorder_state::ready;
}

recorder_state Recorder::state() const
{
    return state_.load();
}

void Recorder::frontend_result_entry(
    const SpeechFrame& frame,
    void* context
)
{
    if (context == nullptr)
    {
        return;
    }

    static_cast<Recorder*>(context)->handle_frontend_result(frame);
}

void Recorder::handle_frontend_result(const SpeechFrame& frame)
{
    if (frame.samples == nullptr || frame.sample_count == 0)
    {
        return;
    }

    const recorder_state current = state_.load();
    if (current == recorder_state::idle || current == recorder_state::ready)
    {
        return;
    }

    if (xSemaphoreTake(session_mutex_, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return;
    }

    const recorder_state locked_state = state_.load();
    if (locked_state == recorder_state::idle || locked_state == recorder_state::ready)
    {
        xSemaphoreGive(session_mutex_);
        return;
    }

    if (locked_state == recorder_state::waiting_for_speech)
    {
        // Otomatik VAD yolu Stage 2.1 ile ayni.
        wav_buffer_.push_pre_roll(frame.samples, frame.sample_count);

        if (frame.speech)
        {
            begin_automatic_recording(frame);
        }

        xSemaphoreGive(session_mutex_);
        return;
    }

    if (locked_state == recorder_state::recording)
    {
        if (
            capture_mode_ == capture_mode::manual &&
            frame.speech
        )
        {
            manual_speech_seen_.store(true);
        }

        wav_buffer_.append(frame.samples, frame.sample_count);

        const uint32_t elapsed_ms = millis() - recording_started_ms_;
        const bool max_duration = elapsed_ms >= MAX_RECORD_DURATION_MS;
        const bool automatic_speech_finished =
            capture_mode_ == capture_mode::automatic_vad &&
            !frame.speech;

        if (
            wav_buffer_.full() ||
            max_duration ||
            automatic_speech_finished
        )
        {
            finish_recording();
        }
    }

    xSemaphoreGive(session_mutex_);
}

void Recorder::begin_automatic_recording(const SpeechFrame& frame)
{
    const bool has_afe_prefix =
        frame.speech_prefix != nullptr &&
        frame.speech_prefix_sample_count > 0;

    if (has_afe_prefix)
    {
        wav_buffer_.append(
            frame.speech_prefix,
            frame.speech_prefix_sample_count
        );
        wav_buffer_.append(frame.samples, frame.sample_count);
    }
    else
    {
        wav_buffer_.append_pre_roll();
    }

    capture_mode_ = capture_mode::automatic_vad;
    recording_started_ms_ = millis();
    state_.store(recorder_state::recording);

    Serial.print(">>> KONUSMA BASLADI [");
    Serial.print(has_afe_prefix ? "VAD CACHE" : "PRE-ROLL FALLBACK");
    Serial.println("]");
}

void Recorder::finish_recording()
{
    if (wav_buffer_.pcm_bytes() == 0)
    {
        if (capture_mode_ == capture_mode::automatic_vad)
        {
            state_.store(recorder_state::waiting_for_speech);
        }
        else
        {
            state_.store(recorder_state::idle);
        }

        return;
    }

    if (
        capture_mode_ == capture_mode::manual &&
        PTT_REQUIRE_VAD_SPEECH &&
        !manual_speech_seen_.load()
    )
    {
        wav_buffer_.reset();
        state_.store(recorder_state::idle);

        Serial.println("[BAS-KONUS] Konusma algilanmadi; kayit atildi.");
        return;
    }

    wav_buffer_.finalize();
    state_.store(recorder_state::ready);

    Serial.println("<<< KAYIT BITTI");
    Serial.print("WAV: ");
    Serial.print(wav_buffer_.size());
    Serial.println(" byte");
}

void Recorder::reset_session()
{
    wav_buffer_.reset();
    recording_started_ms_ = 0;
    manual_speech_seen_.store(false);
}

void Recorder::free_resources()
{
    initialized_ = false;
    state_.store(recorder_state::idle);
    capture_mode_ = capture_mode::automatic_vad;

    speech_frontend_.end();
    audio_input_.end();

    if (session_mutex_ != nullptr)
    {
        vSemaphoreDelete(session_mutex_);
        session_mutex_ = nullptr;
    }

    wav_buffer_.end();
    recording_started_ms_ = 0;
}
