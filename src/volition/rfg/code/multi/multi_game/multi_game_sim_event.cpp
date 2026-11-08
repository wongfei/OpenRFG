// Original: volition/rfg/code/multi/multi_game/multi_game_sim_event.cpp
//
// Decompiled whole (multiplayer.md MP5): events received from the server that are applied when the network clock
// reaches their time (object creation and destruction, explosions, respawns, weapon updates), one queue per type, in
// arrival order.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_sim_event.h"

#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_pool.inl"
#include "volition/rfg/code/multi/multi_game/multi_game_player.h"

// ---- file-static globals
// jip_destruction_sync jip_destruction_sync_state (0x0167E708) is used here too; placed in multi.cpp.
// multi_pool<multi_game_sim_event,1024> Sim_event_allocator (0x01693A10) is used here too; placed in multi_game.cpp.
// multi_game_sim_event* Event_queue[6] (0x02FE5BFC) is used here too; placed in multi_game_player.cpp.
// unsigned int Manual_camera_marker_handle (0x02FE5C14) is used here too; placed in multi_game_camera.cpp.
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// migration_state Migration_state (0x02FEB6C4) is used here too; placed in multi.cpp.

// 0x009E26C0 (:116): 1024 events
void multi_game_sim_event_init()
{
    Sim_event_allocator.init(MP_allocators_mempool);
    multi_game_sim_event_reset();
}

// 0x009D2CA0 (:124): the queued events are dropped without their destroy callbacks; the event data pools are cleared
void multi_game_sim_event_reset()
{
    for (multi_game_sim_event* event = Sim_event_allocator.get_first(0); event != nullptr;)
    {
        event = Sim_event_allocator.free(event);
    }
    multi_core_sim_event_clear();
    multi_game_explosion_event_clear();
    multi_game_player_weapon_update_clear();
    for (int i = 0; i < MULTI_GAME_SIM_EVENT_TYPE_SIZE; i++)
    {
        Event_queue[i] = nullptr;
    }
}

// 0x009B3420 (:144)
// Each event whose network time has come is processed (when allowed); a processed event, or one more than 10 s late
// that may time out, leaves its queue. A client waits during the join in progress destruction sync, and during a
// migration until it waits for the creates.
void multi_game_sim_event_process()
{
    const unsigned int now = Clock_current_time;
    if (Multiplayer && !Server)
    {
        if (Migration_state == MIGRATION_STATE_NONE)
        {
            if (jip_destruction_sync_state != JIP_DESTRUCTION_STATE_NONE)
            {
                return;
            }
        }
        else if (Migration_state != MIGRATION_CLIENT_WAIT_FOR_CREATE_PROCESS)
        {
            return;
        }
    }
    for (int type = 0; type < MULTI_GAME_SIM_EVENT_TYPE_SIZE; type++)
    {
        multi_game_sim_event* event = Event_queue[type];
        while (event != nullptr)
        {
            multi_game_sim_event* const next = event->event_next != Event_queue[type] ? event->event_next : nullptr;
            if (event->m_network_time <= now)
            {
                bool done = (event->m_allow_process_callback == nullptr || event->m_allow_process_callback(event->m_data)) && event->m_process_callback(event->m_data);
                if (!done && event->m_network_time + 10000 < now && !event->cannot_timeout) // Magic number: 10 s timeout
                {
                    done = true;
                }
                if (done)
                {
                    mc_list_remove(Event_queue[type], event, &multi_game_sim_event::event_next, &multi_game_sim_event::event_prev, false);
                    if (event->m_destroy_callback != nullptr)
                    {
                        event->m_destroy_callback(event->m_data);
                    }
                    Sim_event_allocator.free(event);
                }
            }
            event = next;
        }
    }
}

// 0x009A2620 (:199)
// A network time of 0 processes the event now (without the allow callback); otherwise it is appended to its type's
// queue. False when the event pool is full.
bool multi_game_sim_event_add(unsigned int network_time, multi_game_sim_event_type event_type, bool cannot_timeout, void* data, bool (*allow_process_callback)(void*), bool (*process_callback)(void*), void (*destroy_callback)(void*))
{
    if (network_time == 0)
    {
        process_callback(data);
        if (destroy_callback != nullptr)
        {
            destroy_callback(data);
        }
        return true;
    }
    multi_game_sim_event* event = Sim_event_allocator.alloc(0);
    if (event == nullptr)
    {
        return false;
    }
    event->cannot_timeout = cannot_timeout;
    event->m_data = data;
    event->m_allow_process_callback = allow_process_callback;
    event->m_destroy_callback = destroy_callback;
    event->m_network_time = network_time;
    event->m_event_type = event_type;
    event->m_process_callback = process_callback;
    multi_game_sim_event*& queue = Event_queue[event_type];
    if (queue == nullptr)
    {
        event->event_next = event;
        event->event_prev = event;
        queue = event;
        return true;
    }
    // The tail: the queue keeps its head.
    event->event_prev = queue->event_prev;
    event->event_next = queue;
    queue->event_prev->event_next = event;
    queue->event_prev = event;
    return true;
}
