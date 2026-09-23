#pragma once

#include <cstddef>
#include <cstdint>

#include "api_client.h"
#include "keep_alive.h"
#include "player.h"
#include "recorder.h"
#include "talk_button.h"
#include "ultrasonic.h"

enum class conversation_state
{
    idle,
    greeting,
    waiting_button,
    listening,
    manual_recording,
    release_tail,
    waiting_server,
    playing_response
};

class ConversationController
{
public:
    bool begin();
    void update();

    ~ConversationController();

    conversation_state get_state() const
    {
        return current_state;
    }

private:
    conversation_state current_state = conversation_state::idle;

    Ultrasonic ultrasonic_;
    TalkButton talk_button_;
    Recorder recorder_;
    Player player_;

    ApiClient chat_api_client_;
    ApiClient keep_alive_api_client_;

    KeepAlive keep_alive_ {
        keep_alive_api_client_
    };

    bool initialized_ = false;
    bool session_active_ = false;

    // Push-to-talk modunda ayni gecisin tekrar greeting tetiklemesini onler.
    // Yeni greeting icin HC-SR04 once bir kez CLEAR gormelidir.
    bool ultrasonic_armed_ = true;

    // Greeting/response sirasinda basili tutulmus buton yeni kaydi
    // otomatik baslatmasin; once birakilip tekrar basilmasi gerekir.
    bool button_ready_ = false;
    bool release_notice_shown_ = false;

    uint32_t state_started_ms_ = 0;
    uint32_t manual_pressed_started_ms_ = 0;

    uint32_t cooldown_started_ms_ = 0;
    uint32_t cooldown_duration_ms_ = 0;

    uint8_t* response_mp3_data_ = nullptr;
    size_t response_mp3_size_ = 0;

    void change_state(conversation_state state);

    void enter_idle();
    void enter_greeting();
    void enter_waiting_button();
    void enter_listening();
    void enter_manual_recording();
    void enter_release_tail();
    void enter_waiting_server();
    void enter_playing_response();

    void update_idle();
    void update_idle_push_to_talk();
    void update_idle_automatic_vad();
    void update_greeting();
    void update_waiting_button();
    void update_listening();
    void update_manual_recording();
    void update_release_tail();
    void update_waiting_server();
    void update_playing_response();

    void continue_after_playback();

    void start_cooldown(uint32_t duration_ms);
    bool cooldown_active();

    bool download_response_to_psram();
    void free_response_audio();

    void end_session();
};
