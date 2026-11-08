// Original: volition/rfg/code/multi/multi_game/multi_game_sim_event.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_sim_event.cpp
//
// Simulation events: game events (kills, captures, ...) queued and processed on the server.
#pragma once

enum multi_game_sim_event_type
{
    MULTI_GAME_SIM_EVENT_TYPE_CREATE_OBJECT = 0,
    MULTI_GAME_SIM_EVENT_EXPLOSION = 1,
    MULTI_GAME_SIM_EVENT_PLAYER_RESPAWN = 2,
    MULTI_GAME_SIM_EVENT_PLAYER_WEAPON_UPDATE = 3,
    MULTI_GAME_SIM_EVENT_TYPE_DESTROY_OBJECT = 4,
    MULTI_GAME_SIM_EVENT_VEHICLE_RESPAWN = 5,
    MULTI_GAME_SIM_EVENT_TYPE_SIZE = 6,
};

// size 0x24
class multi_game_sim_event
{
public:
    unsigned int m_network_time; // 0x00
    bool cannot_timeout; // 0x04
    multi_game_sim_event_type m_event_type; // 0x08
    void* m_data; // 0x0C
    bool (*m_allow_process_callback)(void*); // 0x10
    bool (*m_process_callback)(void*); // 0x14
    void (*m_destroy_callback)(void*); // 0x18
    multi_game_sim_event* event_prev; // 0x1C
    multi_game_sim_event* event_next; // 0x20
};

void multi_game_sim_event_init(); // 0x009E26C0 (:116)
void multi_game_sim_event_reset(); // 0x009D2CA0 (:124)
void multi_game_sim_event_process(); // 0x009B3420 (:144)
bool multi_game_sim_event_add(unsigned int network_time, multi_game_sim_event_type event_type, bool cannot_timeout, void* data, bool (*allow_process_callback)(void*), bool (*process_callback)(void*), void (*destroy_callback)(void*)); // 0x009A2620 (:199)
multi_game_sim_event_type operator++(multi_game_sim_event_type& rs, int); // 0x00947BF0 (.h:39)
