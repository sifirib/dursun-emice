#pragma once

enum class conversation_state
{
    idle,
    greeting,
    listening,
    waiting_server,
    playing_response
};

class ConversationController
{
public:
    void begin();

    void update();

private:
    conversation_state current_state = conversation_state::idle;

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