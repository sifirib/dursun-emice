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

    // AFE/NSNet2/VADNet surekli calisir ve fetch task'i sonucu
    // surekli bosaltir. Greeting/TTS sirasinda Recorder IDLE oldugu
    // icin bu sonuclar tamamen yok sayilir. Dinlemeye gecerken
    // sadece VAD'in onceki playback durumunu temizliyoruz.
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

    // AFE/NSNet2/VADNet arka planda sicak kalir. Ancak Recorder IDLE
    // oldugu icin callback'ten gelen tum VAD sonuclari yok sayilir;
    // greeting veya Dursun'un TTS sesi kayit baslatamaz.
    state_.store(recorder_state::idle);
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
        // ESP-SR cache kullanilamazsa eski davranisi koruyan yedek pre-roll.
        wav_buffer_.push_pre_roll(frame.samples, frame.sample_count);

        if (frame.speech)
        {
            begin_recording(frame);
        }

        xSemaphoreGive(session_mutex_);
        return;
    }

    if (locked_state == recorder_state::recording)
    {
        wav_buffer_.append(frame.samples, frame.sample_count);

        const uint32_t elapsed_ms = millis() - recording_started_ms_;
        const bool max_duration = elapsed_ms >= MAX_RECORD_DURATION_MS;
        const bool speech_finished = !frame.speech;

        if (wav_buffer_.full() || max_duration || speech_finished)
        {
            finish_recording();
        }
    }

    xSemaphoreGive(session_mutex_);
}

void Recorder::begin_recording(const SpeechFrame& frame)
{
    const bool has_afe_prefix =
        frame.speech_prefix != nullptr &&
        frame.speech_prefix_sample_count > 0;

    if (has_afe_prefix)
    {
        // Espressif'in tarif ettigi sirayla: VAD cache once, mevcut frame sonra.
        wav_buffer_.append(
            frame.speech_prefix,
            frame.speech_prefix_sample_count
        );
        wav_buffer_.append(frame.samples, frame.sample_count);
    }
    else
    {
        // Current frame zaten pre-roll'a push edildi; burada tekrar eklenmez.
        wav_buffer_.append_pre_roll();
    }

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
        state_.store(recorder_state::waiting_for_speech);
        return;
    }

    wav_buffer_.finalize();
    state_.store(recorder_state::ready);

    Serial.println("<<< KONUSMA BITTI");
    Serial.print("WAV: ");
    Serial.print(wav_buffer_.size());
    Serial.println(" byte");
}

void Recorder::reset_session()
{
    wav_buffer_.reset();
    recording_started_ms_ = 0;
}

void Recorder::free_resources()
{
    initialized_ = false;
    state_.store(recorder_state::idle);

    // Callback kullanabilecek task'lar tamamen durmadan mutex/buffer yok edilmez.
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
