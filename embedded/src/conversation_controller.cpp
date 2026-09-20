#include "conversation_controller.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>

#include <esp_heap_caps.h>

#include "config.h"


namespace
{
    constexpr char GREETING_MP3_PATH[] =
        "/greeting.mp3";

    constexpr size_t DOWNLOAD_BUFFER_SIZE =
        4096;
}


ConversationController::~ConversationController()
{
    player_.stop();

    chat_api_client_.end_chat();

    free_response_audio();
}


bool ConversationController::begin()
{
    ultrasonic_.begin();

    if (!recorder_.begin())
    {
        Serial.println(
            "HATA: Recorder baslatilamadi."
        );

        return false;
    }

    player_.begin();

    if (
        !LittleFS.exists(
            GREETING_MP3_PATH
        )
    )
    {
        Serial.println(
            "HATA: /greeting.mp3 bulunamadi."
        );

        return false;
    }

    initialized_ = true;

    change_state(
        conversation_state::idle
    );

    Serial.println(
        "ConversationController: OK"
    );

    return true;
}


void ConversationController::update()
{
    if (!initialized_)
    {
        return;
    }

    /*
     * HC-SR04'u surekli fakat kontrollu
     * araliklarla guncelle.
     */
    ultrasonic_.update();

    /*
     * Keep-alive sadece aktif bir sohbet
     * yokken calissin.
     */
    if (
        current_state ==
            conversation_state::idle &&
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

        case conversation_state::listening:
            update_listening();
            break;

        case conversation_state::waiting_server:
            update_waiting_server();
            break;

        case conversation_state::playing_response:
            update_playing_response();
            break;
    }
}


void ConversationController::change_state(
    conversation_state state
)
{
    current_state = state;

    switch (state)
    {
        case conversation_state::idle:
            enter_idle();
            break;

        case conversation_state::greeting:
            enter_greeting();
            break;

        case conversation_state::listening:
            enter_listening();
            break;

        case conversation_state::waiting_server:
            enter_waiting_server();
            break;

        case conversation_state::playing_response:
            enter_playing_response();
            break;
    }
}


void ConversationController::start_cooldown(
    uint32_t duration_ms
)
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

    if (
        millis() - cooldown_started_ms_ >=
        cooldown_duration_ms_
    )
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
        heap_caps_free(
            response_mp3_data_
        );

        response_mp3_data_ = nullptr;
    }

    response_mp3_size_ = 0;
}


void ConversationController::end_session()
{
    session_active_ = false;

    player_.stop();

    chat_api_client_.end_chat();

    free_response_audio();

    Serial.println();
    Serial.println(
        "[SESSION] Kisi ayrildi. Oturum kapandi."
    );
}


// =========================
// IDLE
// =========================

void ConversationController::enter_idle()
{
}


void ConversationController::update_idle()
{
    /*
     * Aktif oturum varken kisi uzaklastiysa
     * oturumu kapat.
     */
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

    /*
     * Zaten greeting yapilmis aktif bir kisi
     * hala onumuzdeyse yeni tur baslat.
     */
    if (session_active_)
    {
        if (ultrasonic_.person_present())
        {
            change_state(
                conversation_state::listening
            );
        }

        return;
    }

    /*
     * Yeni kisi.
     */
    if (ultrasonic_.person_present())
    {
        session_active_ = true;

        Serial.println();
        Serial.println(
            "================================"
        );

        Serial.println(
            "[SESSION] Yeni kisi geldi."
        );

        Serial.println(
            "================================"
        );

        change_state(
            conversation_state::greeting
        );
    }
}


// =========================
// GREETING
// =========================

void ConversationController::enter_greeting()
{
    // Mikrofon + AFE + VADNet sicak kalir; Recorder algilamayi
    // DISARM eder. Boylece greeting sirasindaki VAD sonuclari
    // kayit baslatamaz.
    if (!recorder_.pause_detection())
    {
        Serial.println(
            "HATA: Greeting oncesi algilama durdurulamadi."
        );
    }

    Serial.println(
        "[GREETING] Karsilama sesi caliyor..."
    );

    if (
        !player_.play_file(
            GREETING_MP3_PATH
        )
    )
    {
        Serial.println(
            "HATA: greeting.mp3 calinamadi."
        );

        session_active_ = false;

        start_cooldown(
            COOLDOWN_AFTER_ERROR_MS
        );

        change_state(
            conversation_state::idle
        );
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

    Serial.println(
        "[GREETING] Tamamlandi."
    );

    if (!ultrasonic_.person_present())
    {
        end_session();

        change_state(
            conversation_state::idle
        );

        return;
    }

    change_state(
        conversation_state::listening
    );
}


// =========================
// LISTENING
// =========================

void ConversationController::enter_listening()
{
    if (!ultrasonic_.person_present())
    {
        end_session();

        change_state(
            conversation_state::idle
        );

        return;
    }

    if (!recorder_.start_listening())
    {
        Serial.println(
            "HATA: Mikrofon dinleme baslatilamadi."
        );

        start_cooldown(
            COOLDOWN_AFTER_ERROR_MS
        );

        change_state(
            conversation_state::idle
        );

        return;
    }

    Serial.println(
        "[LISTENING] Dursun Emice dinliyor."
    );
}


void ConversationController::update_listening()
{
    /*
     * Kisi konusmadan giderse hiçbir sey
     * sunucuya gondermiyoruz.
     */
    if (!ultrasonic_.person_present())
    {
        recorder_.stop();

        end_session();

        change_state(
            conversation_state::idle
        );

        return;
    }

    /*
     * Her loop'ta kucuk bir ses blogu
     * isle.
     */
    recorder_.update();

    /*
     * Henuz anlamli bir konusma yok.
     *
     * Burada sonsuza kadar sessizce
     * bekleyebilir.
     */
    if (!recorder_.has_recording())
    {
        return;
    }

    Serial.print(
        "[LISTENING] Anlamli konusma hazir: "
    );

    Serial.print(
        recorder_.wav_size()
    );

    Serial.println(
        " byte"
    );

    change_state(
        conversation_state::waiting_server
    );
}


// =========================
// WAITING SERVER
// =========================

bool ConversationController::download_response_to_psram()
{
    free_response_audio();

    const int content_length =
        chat_api_client_.content_length();

    if (content_length <= 0)
    {
        Serial.println(
            "HATA: Gecersiz MP3 Content-Length."
        );

        chat_api_client_.end_chat();

        return false;
    }

    if (
        static_cast<size_t>(content_length) >
        MAX_RESPONSE_MP3_BYTES
    )
    {
        Serial.print(
            "HATA: MP3 fazla buyuk: "
        );

        Serial.println(content_length);

        chat_api_client_.end_chat();

        return false;
    }

    Stream* stream =
        chat_api_client_.get_stream();

    if (stream == nullptr)
    {
        Serial.println(
            "HATA: MP3 stream alinamadi."
        );

        chat_api_client_.end_chat();

        return false;
    }

    response_mp3_data_ =
        static_cast<uint8_t*>(
            heap_caps_malloc(
                static_cast<size_t>(
                    content_length
                ),
                MALLOC_CAP_SPIRAM |
                MALLOC_CAP_8BIT
            )
        );

    if (response_mp3_data_ == nullptr)
    {
        Serial.println(
            "HATA: Response icin PSRAM ayrilamadi."
        );

        chat_api_client_.end_chat();

        return false;
    }

    const size_t expected_size =
        static_cast<size_t>(
            content_length
        );

    size_t received_size = 0;

    uint32_t last_progress_ms =
        millis();

    while (received_size < expected_size)
    {
        const int available_bytes =
            stream->available();

        if (available_bytes <= 0)
        {
            if (
                millis() - last_progress_ms >
                RESPONSE_STREAM_STALL_TIMEOUT_MS
            )
            {
                Serial.println(
                    "HATA: MP3 indirme timeout."
                );

                chat_api_client_.end_chat();

                free_response_audio();

                return false;
            }

            delay(2);

            continue;
        }

        size_t read_size =
            static_cast<size_t>(
                available_bytes
            );

        if (read_size > DOWNLOAD_BUFFER_SIZE)
        {
            read_size =
                DOWNLOAD_BUFFER_SIZE;
        }

        const size_t remaining =
            expected_size -
            received_size;

        if (read_size > remaining)
        {
            read_size = remaining;
        }

        const size_t bytes_read =
            stream->readBytes(
                response_mp3_data_ +
                    received_size,
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

    response_mp3_size_ =
        received_size;

    Serial.print(
        "[SERVER] MP3 PSRAM'e alindi: "
    );

    Serial.print(
        response_mp3_size_
    );

    Serial.println(" byte");

    return true;
}


void ConversationController::enter_waiting_server()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println(
            "HATA: WiFi bagli degil."
        );

        start_cooldown(
            COOLDOWN_AFTER_ERROR_MS
        );

        change_state(
            conversation_state::idle
        );

        return;
    }

    Serial.println(
        "[SERVER] Ses gonderiliyor..."
    );

    const bool started =
        chat_api_client_.begin_chat(
            recorder_.wav_data(),
            recorder_.wav_size()
        );

    if (!started)
    {
        Serial.println(
            "HATA: Sunucu cevabi alinamadi."
        );

        start_cooldown(
            COOLDOWN_AFTER_ERROR_MS
        );

        change_state(
            conversation_state::idle
        );

        return;
    }

    if (!download_response_to_psram())
    {
        start_cooldown(
            COOLDOWN_AFTER_ERROR_MS
        );

        change_state(
            conversation_state::idle
        );

        return;
    }

    change_state(
        conversation_state::playing_response
    );
}


void ConversationController::update_waiting_server()
{
    /*
     * HTTP istegi su an senkron/bloklayici.
     */
}


// =========================
// PLAYING RESPONSE
// =========================

void ConversationController::enter_playing_response()
{
    // Dursun kendi TTS cevabini calarken Recorder DISARM edilir.
    // AFE pipeline sicak kalir; playback VAD sonucu kayda donusemez.
    if (!recorder_.pause_detection())
    {
        Serial.println(
            "HATA: Response oncesi algilama durdurulamadi."
        );
    }

    Serial.println(
        "[RESPONSE] Dursun Emice konusuyor..."
    );

    if (
        !player_.play_memory(
            response_mp3_data_,
            response_mp3_size_
        )
    )
    {
        Serial.println(
            "HATA: Response MP3 baslatilamadi."
        );

        free_response_audio();

        start_cooldown(
            COOLDOWN_AFTER_ERROR_MS
        );

        change_state(
            conversation_state::idle
        );
    }
}


void ConversationController::update_playing_response()
{
    player_.loop();

    if (player_.is_playing())
    {
        return;
    }

    /*
     * Player source'u artik kullanmiyor.
     * Buffer guvenle serbest birakilabilir.
     */
    player_.stop();

    free_response_audio();

    Serial.println(
        "[RESPONSE] Konusma tamamlandi."
    );

    start_cooldown(
        COOLDOWN_AFTER_CHAT_MS
    );

    change_state(
        conversation_state::idle
    );
}