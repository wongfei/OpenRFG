// Original: volition/rfg/code/multi/multi_core/global/multi_session.cpp
//
// The session pool (party, transition and game sessions, constructed in place from the multiplayer allocators): one
// process step per session a frame, lookups by type and by id. Shutting down deletes every session and processes them
// until all are gone.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"

multi_session* Game_session; // 0x02FEB654

// ---- globals
multi_pool<multi_session, 4> Session_pool; // 0x01691D10 (:73)

// 0x009F7CB0 (:530)
void multi_session_system_init()
{
    Session_pool.init(MP_allocators_mempool);
}

// 0x009FC6E0 (:539): delete every session, then process them until all reached MSS_NONE (each freed then)
void multi_session_system_shutdown()
{
    bool pending = false;
    Session_pool.thread_access.enter();
    multi_session* sp = Session_pool.get_first(0);
    while (sp != nullptr)
    {
        multi_session* const next = Session_pool.get_next(sp);
        sp->session_delete();
        if (sp->get_state() == MSS_NONE)
        {
            Session_pool.free(sp);
        }
        else
        {
            pending = true;
        }
        sp = next;
    }
    Session_pool.thread_access.leave();
    while (pending)
    {
        pending = false;
        Session_pool.thread_access.enter();
        sp = Session_pool.get_first(0);
        while (sp != nullptr)
        {
            multi_session* const next = Session_pool.get_next(sp);
            sp->process();
            if (sp->get_state() == MSS_NONE)
            {
                Session_pool.free(sp);
            }
            else
            {
                pending = true;
            }
            sp = next;
        }
        Session_pool.thread_access.leave();
    }
}

// 0x009FC8B0 (:575)
void multi_session_system_process()
{
    Session_pool.thread_access.enter();
    multi_session* sp = Session_pool.get_first(0);
    while (sp != nullptr)
    {
        multi_session* const next = Session_pool.get_next(sp);
        sp->process();
        sp = next;
    }
    Session_pool.thread_access.leave();
}

// 0x00990D80 (:592)
multi_session* multi_session_allocate(const char* debug_name)
{
    multi_session* sp = Session_pool.alloc(0);
    if (sp == nullptr)
    {
        return nullptr;
    }
    sp->init(true);
    return sp;
}

// 0x009AABD0 (:607)
void multi_session_free(multi_session* sp)
{
    Session_pool.free(sp);
}

// 0x0094B910 (:614). LTCG: sp in EAX.
static bool session_is_in_valid_find_state(multi_session* sp)
{
    const multi_session_state state = sp->get_state();
    return state == MSS_READY || state == MSS_IN_GAME || state == MSS_STARTING;
}

// 0x00990DB0 (:623): a game session in any state, the others only when ready, starting or in game
multi_session* multi_session_get_by_type(session_types type)
{
    Session_pool.thread_access.enter();
    for (multi_session* sp = Session_pool.get_first(0); sp != nullptr; sp = Session_pool.get_next(sp))
    {
        if (sp->get_info()->type == type && (type == ST_GAME || session_is_in_valid_find_state(sp)))
        {
            Session_pool.thread_access.leave();
            return sp;
        }
    }
    Session_pool.thread_access.leave();
    return nullptr;
}

// 0x0094B930 (:644)
bool multi_session_is_session_game_session(const multi_session* sp)
{
    return sp != nullptr && sp->get_info()->type == ST_GAME;
}

// 0x0094B950 (:651): original behaviour kept: tests ST_GAME as well (there is no lobby session type)
bool multi_session_is_session_lobby_session(const multi_session* sp)
{
    return sp != nullptr && sp->get_info()->type == ST_GAME;
}

// 0x0094B970 (:658)
bool multi_session_is_session_party_session(const multi_session* sp)
{
    return sp != nullptr && sp->get_info()->type == ST_PARTY;
}

// 0x0094B990 (:665)
bool multi_session_is_active_game_session(const multi_session* sp)
{
    if (sp == nullptr || sp->get_info()->type != ST_GAME)
    {
        return false;
    }
    return sp->get_state() == MSS_IN_GAME || sp->get_state() == MSS_MIGRATING;
}

// 0x0094B9C0 (:674)
session_types multi_session_get_session_type(const multi_session* sp)
{
    if (sp == nullptr)
    {
        return ST_INVALID;
    }
    return sp->get_info()->type;
}

// 0x00990E30 (:681)
multi_session* multi_session_find_by_id(const multi_session_info& info)
{
    Session_pool.thread_access.enter();
    for (multi_session* sp = Session_pool.get_first(0); sp != nullptr; sp = Session_pool.get_next(sp))
    {
        if (sp->compare_by_id(&info) == 0)
        {
            Session_pool.thread_access.leave();
            return sp;
        }
    }
    Session_pool.thread_access.leave();
    return nullptr;
}
