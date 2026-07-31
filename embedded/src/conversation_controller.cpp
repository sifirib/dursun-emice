#include "conversation_controller.h"

void ConversationController::begin()
{
    change_state(conversation_state::idle);
}

void ConversationController::update()
{
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

void ConversationController::change_state(conversation_state state)
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

void ConversationController::enter_idle()
{
}

void ConversationController::enter_greeting()
{
}

void ConversationController::enter_listening()
{
}

void ConversationController::enter_waiting_server()
{
}

void ConversationController::enter_playing_response()
{
}

void ConversationController::update_idle()
{
}

void ConversationController::update_greeting()
{
}

void ConversationController::update_listening()
{
}

void ConversationController::update_waiting_server()
{
}

void ConversationController::update_playing_response()
{
}