#pragma once

#include <cstdint>

#include "api_client.h"
#include "keep_alive.h"
#include "player.h"
#include "recorder.h"
#include "ultrasonic.h"

enum class conversation_state
{
    idle,
    greeting,
    listening,
    waiting_server,
    playing_response
};

// Tum modulleri (Ultrasonic, Recorder, Player, ApiClient) tek merkezden
// yoneten koordinatordur. Modullerin birbirini tanimasi gerekmez,
// tum akis burada kurulur.
class ConversationController
{
public:
    void begin();
    void update();

    conversation_state get_state() const { return current_state; }

private:
    conversation_state current_state = conversation_state::idle;

    Ultrasonic ultrasonic_;
    Recorder recorder_;
    Player player_;
    ApiClient api_client_;
    KeepAlive keep_alive_{api_client_};

    uint32_t cooldown_until_ms_ = 0;

    void change_state(conversation_state state);

    void enter_idle();
    void enter_greeting();
    void enter_listening();
    void enter_waiting_server();
    void enter_playing_response();

    void update_idle();
    void update_greeting();
    void update_listening();
    void update_waiting_server();
    void update_playing_response();
};
