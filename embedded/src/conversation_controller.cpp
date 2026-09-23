#include "conversation_controller.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>

#include <esp_heap_caps.h>

#include "config.h"

namespace
{
    constexpr char GREETING_MP3_PATH[] = "/greeting.mp3";
    constexpr size_t DOWNLOAD_BUFFER_SIZE = 4096;
}

ConversationController::~ConversationController()
{
    player_.stop();
    recorder_.stop();
    chat_api_client_.end_chat();
    free_response_audio();
}

bool ConversationController::begin()
{
    ultrasonic_.begin();

    if (INTERACTION_MODE == interaction_mode::push_to_talk)
    {
        talk_button_.begin();
    }

    if (!recorder_.begin())
    {
        Serial.println("HATA: Recorder baslatilamadi.");
        return false;
    }

    player_.begin();

    if (!LittleFS.exists(GREETING_MP3_PATH))
    {
        Serial.println("HATA: /greeting.mp3 bulunamadi.");
        return false;
    }

    initialized_ = true;
    ultrasonic_armed_ = !ultrasonic_.person_present();

    change_state(conversation_state::idle);

    Serial.println("ConversationController: OK");

    if (INTERACTION_MODE == interaction_mode::push_to_talk)
    {
        Serial.println("Interaction mode: BASILI TUT VE KONUS");
    }
    else
    {
        Serial.println("Interaction mode: AUTOMATIC VAD");
    }

    return true;
}

void ConversationController::update()
{
    if (!initialized_)
    {
        return;
    }

    ultrasonic_.update();

    if (INTERACTION_MODE == interaction_mode::push_to_talk)
    {
        talk_button_.update();
    }

    if (
        current_state == conversation_state::idle &&
        !session_active_ &&
        !ultrasonic_.person_present() &&
        WiFi.status() == WL_CONNECTED
    )
    {
        keep_alive_.update();
    }

    switch (current_state)
    {
        case conversation_state::idle:
            update_idle();
            break;

        case conversation_state::greeting:
            update_greeting();
            break;

        case conversation_state::waiting_button:
            update_waiting_button();
            break;

        case conversation_state::listening:
            update_listening();
            break;

        case conversation_state::manual_recording:
            update_manual_recording();
            break;

        case conversation_state::release_tail:
            update_release_tail();
            break;

        case conversation_state::waiting_server:
            update_waiting_server();
            break;

        case conversation_state::playing_response:
            update_playing_response();
            break;
    }
}

void ConversationController::change_state(conversation_state state)
{
    current_state = state;
    state_started_ms_ = millis();

    switch (state)
    {
        case conversation_state::idle:
            enter_idle();
            break;

        case conversation_state::greeting:
            enter_greeting();
            break;

        case conversation_state::waiting_button:
            enter_waiting_button();
            break;

        case conversation_state::listening:
            enter_listening();
            break;

        case conversation_state::manual_recording:
            enter_manual_recording();
            break;

        case conversation_state::release_tail:
            enter_release_tail();
            break;

        case conversation_state::waiting_server:
            enter_waiting_server();
            break;

        case conversation_state::playing_response:
            enter_playing_response();
            break;
    }
}

void ConversationController::start_cooldown(uint32_t duration_ms)
{
    cooldown_started_ms_ = millis();
    cooldown_duration_ms_ = duration_ms;
}

bool ConversationController::cooldown_active()
{
    if (cooldown_duration_ms_ == 0)
    {
        return false;
    }

    if (millis() - cooldown_started_ms_ >= cooldown_duration_ms_)
    {
        cooldown_duration_ms_ = 0;
        return false;
    }

    return true;
}

void ConversationController::free_response_audio()
{
    if (response_mp3_data_ != nullptr)
    {
        heap_caps_free(response_mp3_data_);
        response_mp3_data_ = nullptr;
    }

    response_mp3_size_ = 0;
}

void ConversationController::end_session()
{
    session_active_ = false;
    button_ready_ = false;

    recorder_.stop();
    player_.stop();
    chat_api_client_.end_chat();
    free_response_audio();

    Serial.println();
    Serial.println("[SESSION] Oturum kapandi.");
}

// =========================
// IDLE
// =========================

void ConversationController::enter_idle()
{
    recorder_.pause_detection();
}

void ConversationController::update_idle()
{
    if (INTERACTION_MODE == interaction_mode::push_to_talk)
    {
        update_idle_push_to_talk();
        return;
    }

    update_idle_automatic_vad();
}

void ConversationController::update_idle_push_to_talk()
{
    if (cooldown_active())
    {
        return;
    }

    // HC-SR04 bu modda sadece yeni greeting tetikler. Session basladiktan
    // sonra kisinin hala sensor onunde olup olmadigina bakilmaz.
    if (!ultrasonic_armed_)
    {
        if (!ultrasonic_.person_present())
        {
            ultrasonic_armed_ = true;
            Serial.println("[ULTRASONIC] Yeni gecis icin hazir.");
        }

        return;
    }

    if (!ultrasonic_.person_present())
    {
        return;
    }

    ultrasonic_armed_ = false;
    session_active_ = true;

    Serial.println();
    Serial.println("================================");
    Serial.println("[SESSION] Yeni gecis algilandi.");
    Serial.println("================================");

    change_state(conversation_state::greeting);
}

void ConversationController::update_idle_automatic_vad()
{
    // Stage 2.1 otomatik davranisi korunur.
    if (
        session_active_ &&
        !ultrasonic_.person_present()
    )
    {
        end_session();
        return;
    }

    if (cooldown_active())
    {
        return;
    }

    if (session_active_)
    {
        if (ultrasonic_.person_present())
        {
            change_state(conversation_state::listening);
        }

        return;
    }

    if (ultrasonic_.person_present())
    {
        session_active_ = true;

        Serial.println();
        Serial.println("================================");
        Serial.println("[SESSION] Yeni kisi geldi.");
        Serial.println("================================");

        change_state(conversation_state::greeting);
    }
}

// =========================
// GREETING
// =========================

void ConversationController::enter_greeting()
{
    if (!recorder_.pause_detection())
    {
        Serial.println("HATA: Greeting oncesi algilama durdurulamadi.");
    }

    Serial.println("[GREETING] Karsilama sesi caliyor...");

    if (!player_.play_file(GREETING_MP3_PATH))
    {
        Serial.println("HATA: greeting.mp3 calinamadi.");
        end_session();
        start_cooldown(COOLDOWN_AFTER_ERROR_MS);
        change_state(conversation_state::idle);
    }
}

void ConversationController::update_greeting()
{
    player_.loop();

    if (player_.is_playing())
    {
        return;
    }

    player_.stop();
    Serial.println("[GREETING] Tamamlandi.");

    if (INTERACTION_MODE == interaction_mode::push_to_talk)
    {
        // PTT modunda ultrasonic sadece greeting tetikleyicisidir.
        change_state(conversation_state::waiting_button);
        return;
    }

    // Stage 2.1 otomatik yolu aynen korunur.
    if (!ultrasonic_.person_present())
    {
        end_session();
        change_state(conversation_state::idle);
        return;
    }

    change_state(conversation_state::listening);
}

// =========================
// PUSH-TO-TALK WAIT
// =========================

void ConversationController::enter_waiting_button()
{
    recorder_.pause_detection();

    // State degisimi ile ayni anda gerceklesen button release, debounce
    // tamamlanmadan once stable_pressed_ icinde kisa sure eski degeri
    // tasiyabilir. Bu nedenle burada aninda "butonu birak" demiyoruz.
    // Once stabil release gorulmesini bekliyoruz.
    button_ready_ = false;
    release_notice_shown_ = false;

    Serial.println();
    Serial.println("[BAS-KONUS] BASILI TUT VE KONUS.");
}

void ConversationController::update_waiting_button()
{
    if (cooldown_active())
    {
        return;
    }

    if (millis() - state_started_ms_ >= PTT_WAIT_TIMEOUT_MS)
    {
        Serial.println("[BAS-KONUS] Zaman asimi; oturum kapaniyor.");
        end_session();
        change_state(conversation_state::idle);
        return;
    }

    if (!button_ready_)
    {
        if (!talk_button_.is_pressed())
        {
            button_ready_ = true;
            Serial.println("[BAS-KONUS] Buton hazir.");
            return;
        }

        if (
            !release_notice_shown_ &&
            millis() - state_started_ms_ >= PTT_RELEASE_NOTICE_DELAY_MS
        )
        {
            release_notice_shown_ = true;
            Serial.println("[BAS-KONUS] Once butonu birak.");
        }

        return;
    }

    if (!talk_button_.just_pressed())
    {
        return;
    }

    manual_pressed_started_ms_ = millis();
    change_state(conversation_state::manual_recording);
}

// =========================
// AUTOMATIC VAD LISTENING
// =========================

void ConversationController::enter_listening()
{
    if (!ultrasonic_.person_present())
    {
        end_session();
        change_state(conversation_state::idle);
        return;
    }

    if (!recorder_.start_listening())
    {
        Serial.println("HATA: Mikrofon dinleme baslatilamadi.");
        start_cooldown(COOLDOWN_AFTER_ERROR_MS);
        change_state(conversation_state::idle);
        return;
    }

    Serial.println("[LISTENING] Dursun Emice dinliyor.");
}

void ConversationController::update_listening()
{
    if (!ultrasonic_.person_present())
    {
        recorder_.stop();
        end_session();
        change_state(conversation_state::idle);
        return;
    }

    recorder_.update();

    if (!recorder_.has_recording())
    {
        return;
    }

    Serial.print("[LISTENING] Anlamli konusma hazir: ");
    Serial.print(recorder_.wav_size());
    Serial.println(" byte");

    change_state(conversation_state::waiting_server);
}

// =========================
// MANUAL PUSH-TO-TALK RECORDING
// =========================

void ConversationController::enter_manual_recording()
{
    if (!recorder_.start_manual_recording())
    {
        Serial.println("HATA: Bas-konuş kaydi baslatilamadi.");
        end_session();
        start_cooldown(COOLDOWN_AFTER_ERROR_MS);
        change_state(conversation_state::idle);
        return;
    }

    Serial.println("[BAS-KONUS] KAYIT: butonu basili tut.");
}

void ConversationController::update_manual_recording()
{
    recorder_.update();

    // Recorder maksimum sure veya dolu buffer nedeniyle kendisi bitirebilir.
    if (recorder_.has_recording())
    {
        Serial.println("[BAS-KONUS] Maksimum kayit suresine ulasildi.");
        change_state(conversation_state::waiting_server);
        return;
    }

    // Manuel kayit maksimum sure/buffer nedeniyle sonlanmis fakat VADNet
    // hic konusma gormemisse Recorder kaydi READY yapmak yerine atar.
    if (!recorder_.is_recording())
    {
        Serial.println("[BAS-KONUS] Gecerli konusma yok; tekrar deneyebilirsin.");
        change_state(conversation_state::waiting_button);
        return;
    }

    if (!talk_button_.just_released())
    {
        return;
    }

    const uint32_t held_ms = millis() - manual_pressed_started_ms_;

    if (held_ms < PTT_MIN_HOLD_MS)
    {
        Serial.println("[BAS-KONUS] Cok kisa basildi; kayit iptal.");
        recorder_.stop();
        change_state(conversation_state::waiting_button);
        return;
    }

    Serial.print("[BAS-KONUS] Buton birakildi. Son ");
    Serial.print(PTT_RELEASE_TAIL_MS);
    Serial.println(" ms aliniyor...");

    change_state(conversation_state::release_tail);
}

void ConversationController::enter_release_tail()
{
    // Recorder manuel RECORDING durumunda kalir. Son hece icin kisa kuyruk.
}

void ConversationController::update_release_tail()
{
    recorder_.update();

    if (recorder_.has_recording())
    {
        change_state(conversation_state::waiting_server);
        return;
    }

    if (!recorder_.is_recording())
    {
        Serial.println("[BAS-KONUS] Gecerli konusma yok; sunucuya gonderilmedi.");
        change_state(conversation_state::waiting_button);
        return;
    }

    if (millis() - state_started_ms_ < PTT_RELEASE_TAIL_MS)
    {
        return;
    }

    if (!recorder_.finish_manual_recording())
    {
        if (!recorder_.manual_speech_detected())
        {
            Serial.println("[BAS-KONUS] Konusma algilanmadi; sunucuya gonderilmedi.");
        }
        else
        {
            Serial.println("HATA: Bas-konuş kaydi sonlandirilamadi.");
        }

        recorder_.stop();
        change_state(conversation_state::waiting_button);
        return;
    }

    Serial.print("[BAS-KONUS] WAV hazir: ");
    Serial.print(recorder_.wav_size());
    Serial.println(" byte");

    change_state(conversation_state::waiting_server);
}

// =========================
// WAITING SERVER
// =========================

bool ConversationController::download_response_to_psram()
{
    free_response_audio();

    const int content_length = chat_api_client_.content_length();

    if (content_length <= 0)
    {
        Serial.println("HATA: Gecersiz MP3 Content-Length.");
        chat_api_client_.end_chat();
        return false;
    }

    if (static_cast<size_t>(content_length) > MAX_RESPONSE_MP3_BYTES)
    {
        Serial.print("HATA: MP3 fazla buyuk: ");
        Serial.println(content_length);
        chat_api_client_.end_chat();
        return false;
    }

    Stream* stream = chat_api_client_.get_stream();

    if (stream == nullptr)
    {
        Serial.println("HATA: MP3 stream alinamadi.");
        chat_api_client_.end_chat();
        return false;
    }

    response_mp3_data_ = static_cast<uint8_t*>(
        heap_caps_malloc(
            static_cast<size_t>(content_length),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
        )
    );

    if (response_mp3_data_ == nullptr)
    {
        Serial.println("HATA: Response icin PSRAM ayrilamadi.");
        chat_api_client_.end_chat();
        return false;
    }

    const size_t expected_size = static_cast<size_t>(content_length);
    size_t received_size = 0;
    uint32_t last_progress_ms = millis();

    while (received_size < expected_size)
    {
        const int available_bytes = stream->available();

        if (available_bytes <= 0)
        {
            if (millis() - last_progress_ms > RESPONSE_STREAM_STALL_TIMEOUT_MS)
            {
                Serial.println("HATA: MP3 indirme timeout.");
                chat_api_client_.end_chat();
                free_response_audio();
                return false;
            }

            delay(2);
            continue;
        }

        size_t read_size = static_cast<size_t>(available_bytes);

        if (read_size > DOWNLOAD_BUFFER_SIZE)
        {
            read_size = DOWNLOAD_BUFFER_SIZE;
        }

        const size_t remaining = expected_size - received_size;

        if (read_size > remaining)
        {
            read_size = remaining;
        }

        const size_t bytes_read = stream->readBytes(
            response_mp3_data_ + received_size,
            read_size
        );

        if (bytes_read == 0)
        {
            continue;
        }

        received_size += bytes_read;
        last_progress_ms = millis();
    }

    chat_api_client_.end_chat();
    response_mp3_size_ = received_size;

    Serial.print("[SERVER] MP3 PSRAM'e alindi: ");
    Serial.print(response_mp3_size_);
    Serial.println(" byte");

    return true;
}

void ConversationController::enter_waiting_server()
{
    if (
        WiFi.status() != WL_CONNECTED ||
        WiFi.localIP() == IPAddress(0, 0, 0, 0)
    )
    {
        Serial.println("HATA: WiFi/IP hazir degil.");
        end_session();
        start_cooldown(COOLDOWN_AFTER_ERROR_MS);
        change_state(conversation_state::idle);
        return;
    }

    Serial.println("[SERVER] Ses gonderiliyor...");

    const bool started = chat_api_client_.begin_chat(
        recorder_.wav_data(),
        recorder_.wav_size()
    );

    if (!started)
    {
        Serial.println("HATA: Sunucu cevabi alinamadi.");
        end_session();
        start_cooldown(COOLDOWN_AFTER_ERROR_MS);
        change_state(conversation_state::idle);
        return;
    }

    if (!download_response_to_psram())
    {
        end_session();
        start_cooldown(COOLDOWN_AFTER_ERROR_MS);
        change_state(conversation_state::idle);
        return;
    }

    change_state(conversation_state::playing_response);
}

void ConversationController::update_waiting_server()
{
    // HTTP istegi su an senkron/bloklayici.
}

// =========================
// PLAYING RESPONSE
// =========================

void ConversationController::enter_playing_response()
{
    if (!recorder_.pause_detection())
    {
        Serial.println("HATA: Response oncesi algilama durdurulamadi.");
    }

    Serial.println("[RESPONSE] Dursun Emice konusuyor...");

    if (!player_.play_memory(response_mp3_data_, response_mp3_size_))
    {
        Serial.println("HATA: Response MP3 baslatilamadi.");
        free_response_audio();
        end_session();
        start_cooldown(COOLDOWN_AFTER_ERROR_MS);
        change_state(conversation_state::idle);
    }
}

void ConversationController::update_playing_response()
{
    player_.loop();

    if (player_.is_playing())
    {
        return;
    }

    player_.stop();
    free_response_audio();

    Serial.println("[RESPONSE] Konusma tamamlandi.");

    start_cooldown(COOLDOWN_AFTER_CHAT_MS);
    continue_after_playback();
}

void ConversationController::continue_after_playback()
{
    if (INTERACTION_MODE == interaction_mode::push_to_talk)
    {
        // PTT modunda ultrasonic artik session'i yonetmez. Kullanici yeni
        // soru icin tekrar butona basar; timeout olursa session kapanir.
        change_state(conversation_state::waiting_button);
        return;
    }

    // Stage 2.1 otomatik davranisi.
    change_state(conversation_state::idle);
}
