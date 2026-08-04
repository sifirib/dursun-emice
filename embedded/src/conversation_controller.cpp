#include "conversation_controller.h"

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"

void ConversationController::begin()
{
    ultrasonic_.begin();

    if (!recorder_.begin())
    {
        Serial.println("HATA: Recorder baslatilamadi.");
    }

    player_.begin();

    change_state(conversation_state::idle);
}

void ConversationController::update()
{
    // Sunucuyu uyanik tutmak icin, hangi durumda olursak olalim calisir.
    keep_alive_.update();

    switch (current_state)
    {
        case conversation_state::idle:            update_idle(); break;
        case conversation_state::greeting:         update_greeting(); break;
        case conversation_state::listening:        update_listening(); break;
        case conversation_state::waiting_server:   update_waiting_server(); break;
        case conversation_state::playing_response: update_playing_response(); break;
    }
}

void ConversationController::change_state(conversation_state state)
{
    current_state = state;

    switch (state)
    {
        case conversation_state::idle:            enter_idle(); break;
        case conversation_state::greeting:         enter_greeting(); break;
        case conversation_state::listening:        enter_listening(); break;
        case conversation_state::waiting_server:   enter_waiting_server(); break;
        case conversation_state::playing_response: enter_playing_response(); break;
    }
}

// ---------- idle: kisi bekleniyor ----------

void ConversationController::enter_idle()
{
}

void ConversationController::update_idle()
{
    // NOT: "millis() < cooldown_until_ms_" yerine cikarma tabanli
    // karsilastirma kullaniyoruz; millis() ~49.7 gunde bir tasar
    // (overflow), dogrudan karsilastirma o anda cooldown'u yanlis
    // degerlendirebilirdi. Cikarma + isaretli tur, tasmada da dogru sonuc verir.
    if (static_cast<int32_t>(millis() - cooldown_until_ms_) < 0)
    {
        return;
    }

    // Cooldown_until_ms_'i "guncel" tutmak icin burada da simdiki zamana
    // esitliyoruz. Aksi halde haftalarca kimse gelmezse (cooldown_until_ms_
    // hic tazelenmez) millis() 24.8 gunu gectiginde yukaridaki karsilastirma
    // (isaretli tur nedeniyle) yanlis sonuc vermeye baslar.
    cooldown_until_ms_ = millis();

    if (WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    if (ultrasonic_.person_detected())
    {
        change_state(conversation_state::greeting);
    }
}

// ---------- greeting: sabit ilk cumle calinir ----------

void ConversationController::enter_greeting()
{
    if (!player_.play_file("/greeting.mp3"))
    {
        // Cooldown olmadan idle'a donersek, kisi hala <50cm oldugu icin
        // bir sonraki update() tik'inde ayni hata sonsuza kadar tekrar
        // dener (siki dongu). Diger hata yollariyla tutarli olsun diye
        // burada da cooldown koyuyoruz.
        Serial.println("HATA: greeting.mp3 calinamadi (LittleFS'e yuklendi mi?).");
        cooldown_until_ms_ = millis() + COOLDOWN_AFTER_ERROR_MS;
        change_state(conversation_state::idle);
    }
}

void ConversationController::update_greeting()
{
    player_.loop();

    if (!player_.is_playing())
    {
        change_state(conversation_state::listening);
    }
}

// ---------- listening: mikrofon kaydi ----------

// NOT: record() bilerek bloklayicidir (RECORD_DURATION_MS boyunca sistemi
// mesgul eder). Basitligi ve guvenilirligi icin boyle tasarlandi; bu
// yuzden update_listening() hicbir zaman anlamli bir sey yapmaz.
void ConversationController::enter_listening()
{
    recorder_.record();
    change_state(conversation_state::waiting_server);
}

void ConversationController::update_listening()
{
}

// ---------- waiting_server: kayit sunucuya gonderilir ----------

// NOT: begin_chat() de HTTP baglantisi kurulana kadar bloklar, bu yuzden
// gecis de burada, senkron olarak yapiliyor.
void ConversationController::enter_waiting_server()
{
    bool started = api_client_.begin_chat(
        recorder_.wav_data(),
        recorder_.wav_size()
    );

    if (!started)
    {
        Serial.println("HATA: Sunucuya baglanilamadi ya da beklenmeyen cevap geldi.");
        cooldown_until_ms_ = millis() + COOLDOWN_AFTER_ERROR_MS;
        change_state(conversation_state::idle);
        return;
    }

    bool playing = player_.play_stream(
        api_client_.get_stream(),
        api_client_.content_length()
    );

    if (!playing)
    {
        Serial.println("HATA: Sunucu cevabi calinamadi.");
        api_client_.end_chat();
        cooldown_until_ms_ = millis() + COOLDOWN_AFTER_ERROR_MS;
        change_state(conversation_state::idle);
        return;
    }

    change_state(conversation_state::playing_response);
}

void ConversationController::update_waiting_server()
{
}

// ---------- playing_response: sunucu cevabi calinir ----------

void ConversationController::enter_playing_response()
{
}

void ConversationController::update_playing_response()
{
    player_.loop();

    if (!player_.is_playing())
    {
        api_client_.end_chat();
        cooldown_until_ms_ = millis() + COOLDOWN_AFTER_CHAT_MS;
        change_state(conversation_state::idle);
    }
}
