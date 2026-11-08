// Original: volition/rfg/code/gamewide/gameseq.cpp
//
// The game state machine (gameseq.h), whole (multiplayer.md MP1). rfg_gamewide_unity.obj is a unity build:
// game_handshake.cpp reads this file's state stack and Wait_for_prev_done_frame, and gamewide.cpp reads the stack too
// (rfg_gamewide_unity.cpp includes gameseq.cpp first). The render snapshot is game_handshake.h's. s_groupIdGameplay is
// a header static of nvidia_highlights.h.
//
// Notes (decompiled behaviour, kept as is):
// - gameseq_get_previous_state and the pop request read State_stack[Stack_top_index - 1] (Ghidra shows it as an
//   offset from Gamemusic_curr_intensity_factors, the global before State_stack).
// - gameseq_is_state_in_stack scans all 16 slots, including those above the top (stale states of popped entries).
// - gameseq_has_current_state_self_stream_flag with an empty stack reads bit 0 of the byte after State_info[GS_NONE]'s
//   flags, i.e. out of the table (TOD_global_info in Ghidra); here it returns false.

#include "volition/rfg/code/gamewide/gameseq.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/gamewide/game_handshake.h"
#include "volition/rfg/code/gamewide/game_trace.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/nvidia_highlights.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/util/varray.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include "keen/base/trace.hpp"

farray<gameseq_request, 64> State_request_queue; // 0x0165E518
state_info State_info[64]; // 0x0253BDA0

static game_state State_stack[16]; // 0x02532BE8
static int Stack_top_index = -1; // 0x0165CBCC, -1 when empty
// Framecount_increment_always at the last state change or handshake (game_handshake_in_handshake); also written by
// game_handshake.cpp and gamestate_gameplay.cpp (rfg_gamewide_unity), so it is placed in the first file that uses it.
static volatile unsigned int Wait_for_prev_done_frame; // 0x025338E0

static void gameseq_enter_state(game_state state, game_state old_state); // 0x007BFB00 (:173)
static void gameseq_exit_state(game_state state, game_state new_state); // 0x007BFB40 (:198)
static void gameseq_do_state(int stack_index, bool paused, gameseq_state_step step, bool no_handshake, rl_renderer* pRenderer); // 0x007BFB60 (:220)

// 0x007BFF10 (:781)
state_info::state_info()
{
    enter = nullptr;
    exit = nullptr;
    exit_notify = nullptr;
    process = nullptr;
    handshake = nullptr;
    render = nullptr;
    transparent = false;
    pause_beneath = false;
    user_flags = 0;
    hide_cursor = false;
    registered = false;
}

// 0x007B99F0 (.h:56)
bool state_info::is_registered() const
{
    return registered;
}

// The game trace's state names (game_trace.h; not in rfg.exe).
const char* gameseq_trace_state_name(game_state state)
{
    static const char* const names[GS_NUM_STATES] = {
        "MAINMENU",
        "GAMEPLAY",
        "LOAD",
        "BOOT",
        "MULTI_INIT",
        "MULTI_SHUTDOWN",
        "CONNECTING_TO_INVITE",
        "MAINMENU_OPTIONS",
        "MAINMENU_OPTIONS_DISPLAY",
        "MAINMENU_OPTIONS_AUDIO",
        "MAINMENU_OPTIONS_LIVE",
        "MAINMENU_OPTIONS_LIVE_QUICKMATCH",
        "MAINMENU_OPTIONS_GAME_OPTIONS",
        "MAINMENU_OPTIONS_EXTRAS",
        "CHEATS",
        "INGAME_OPTIONS",
        "PLAYER_DEATH_OPTIONS",
        "SONY_OPTIONS",
        "CONTROLLER_OPTIONS",
        "WRECKING_CREW_MAIN_MENU",
        "WRECKING_CREW_CHARACTER_SELECT",
        "WRECKING_CREW_SCOREBOARD",
        "MULTIPLAYER_LIVE",
        "MULTIPLAYER_CHANGE_MATCHMAKING",
        "MULTIPLAYER_SEARCH_MATCHMAKING",
        "MULTIPLAYER_GAME_LOBBY",
        "MULTIPLAYER_PARTY_LOBBY",
        "MULTIPLAYER_PARTY_OPTIONS",
        "MULTIPLAYER_SYSLINK",
        "MULTIPLAYER_SYSLINK_FIND_SERVERS",
        "MULTIPLAYER_CHANGE_GAME_OPTIONS",
        "MULTIPLAYER_PLAYER_SELECT",
        "MULTIPLAYER_XP_TALLY",
        "MULTIPLAYER_RESULTS",
        "MULTIPLAYER_SCOREBOARD",
        "MULTIPLAYER_STATS",
        "MULTIPLAYER_TUTORIALS",
        "INGAME_FULLSCREEN_MAP",
        "INGAME_MISSION_BRIEFING",
        "INGAME_ACTIVITY_BRIEFING",
        "INGAME_MISSION_COMPLETE",
        "INGAME_ACTIVITY_COMPLETE",
        "INGAME_UPGRADES_HUD",
        "INGAME_HANDBOOK_HUD",
        "INGAME_RAIL_DRIVER_HUD",
        "INGAME_DISTRICT_LIVERATION",
        "INGAME_WEAPON_CABINET",
        "LOAD_CUTSCENE",
        "MAINMENU_IDLE",
        "VIDEO_CUTSCENE_PLAY",
        "SAVE_LOAD_SCREEN",
        "PS3_DLC",
        "VIEW_DLC_SCREEN",
        "STATS_SCREEN",
        "CREDITS",
        "VERIFY_SAVEGAME",
        "SHUTDOWN",
        "DLC_INIT",
        "DLC_SHUTDOWN",
        "WC_INIT",
        "WC_SHUTDOWN",
        "RESOLUTION_CHANGE",
        "QUICK_PAUSE",
        "MULTIPLAYER_LIVE_FIND_SERVERS",
    };
    return (unsigned int)state < GS_NUM_STATES ? names[state] : "NONE";
}

// The game trace of a request deferred by an exit_notify: it is retried every frame, so only its first time is written.
static game_state Trace_deferred_state = GS_NONE;

static void gameseq_trace_deferred(game_state state, game_state blocked_by)
{
    if (state != Trace_deferred_state)
    {
        Trace_deferred_state = state;
        GAME_TRACE("state", "change to %s deferred by the exit_notify of %s", gameseq_trace_state_name(state), gameseq_trace_state_name(blocked_by));
    }
}

// 0x007CC8B0 (:160)
bool gameseq_might_process_exit()
{
    return State_request_queue.size() > 0;
}

// 0x007BFB00 (:173)
// LTCG: state and old_state in registers (mapped in the Ghidra DB, Tier 1).
static void gameseq_enter_state(game_state state, game_state old_state)
{
    if (state != old_state)
    {
        if (State_info[state].enter != nullptr)
        {
            State_info[state].enter(old_state);
        }
        ui_cursor_set_visible(!State_info[state].hide_cursor);
    }
}

// 0x007BFB40 (:198)
// LTCG: state and new_state in registers (mapped in the Ghidra DB, Tier 1).
static void gameseq_exit_state(game_state state, game_state new_state)
{
    if (state != GS_NONE && State_info[state].exit != nullptr)
    {
        State_info[state].exit(new_state);
    }
}

// 0x007BFB60 (:220)
static void gameseq_do_state(int stack_index, bool paused, gameseq_state_step step, bool no_handshake, rl_renderer* pRenderer)
{
    const game_state state = step != GS_STEP_RENDER || no_handshake ? State_stack[stack_index] : Render_snapshot.state_stack[stack_index];
    if (State_info[state].transparent)
    {
        if (stack_index < 1)
        {
            VLIB_CRITICAL_FAILURE();
        }
        else
        {
            gameseq_do_state(stack_index - 1, State_info[state].pause_beneath || paused, step, no_handshake, pRenderer);
        }
    }
    if (step == GS_STEP_PROCESS)
    {
        if (!paused && State_info[state].process != nullptr)
        {
            State_info[state].process();
        }
    }
    else if (step == GS_STEP_HANDSHAKE)
    {
        if (!paused && State_info[state].handshake != nullptr)
        {
            State_info[state].handshake(pRenderer);
        }
    }
    else if (step == GS_STEP_RENDER && State_info[state].render != nullptr)
    {
        State_info[state].render(pRenderer);
    }
}

// 0x007CC8C0 (:364)
void gameseq_init()
{
    Stack_top_index = -1;
    State_request_queue.clear();
}

// 0x007BFC50 (:370)
// Notes (decompiled behaviour, kept as is): the range check is state + 1 < 65, so GS_NONE (-1) is accepted and writes
// State_info[-1].
void gameseq_register_state(game_state state, const state_info& callbacks)
{
    if ((unsigned int)(state + 1) < 65)
    {
        // The copy takes the caller's registered flag too; the table entry is then marked.
        State_info[state] = callbacks;
        State_info[state].registered = true;
    }
}

// 0x007BFCB0 (:379)
bool gameseq_state_is_registered(game_state state)
{
    if ((unsigned int)(state + 1) < 65)
    {
        return State_info[state].is_registered();
    }
    return false;
}

// 0x007D8710 (:393)
void gameseq_set_state(game_state state, bool uninterruptible)
{
    const int num = State_request_queue.size();
    if (num > 0 && State_request_queue[num - 1].uninterruptible)
    {
        GAME_TRACE("state", "%s dropped: an uninterruptible change is pending", gameseq_trace_state_name(state));
        keen::debug::nativePrintArgs("Someone is trying to set a new state, when another uninteruptible state change is pending.  If you hit this, please grab a programmer and do not skip this!!");
        return;
    }
    gameseq_request request;
    request.type = GS_TRANSITION_SET;
    request.state = state;
    request.uninterruptible = uninterruptible;
    State_request_queue.add(request);
    State_info[state].transparent = false;
    State_info[state].pause_beneath = false;
}

// 0x007CC8E0 (:425)
void gameseq_clear_all_pending_NEVER_CALL_ME_EXCEPT_DURING_EMERGENCY_SHUTDOWN()
{
    State_request_queue.clear();
}

// 0x007D87C0 (:437)
void gameseq_push_state(game_state new_state, bool transparent, bool pause_beneath)
{
    const int num = State_request_queue.size();
    if (num > 0 && State_request_queue[num - 1].uninterruptible)
    {
        GAME_TRACE("state", "%s dropped: an uninterruptible change is pending", gameseq_trace_state_name(new_state));
        keen::debug::nativePrintArgs("Someone is trying to set a new state, when another uninteruptible state change is pending.  If you hit this, please grab a programmer and do not skip this!!");
        return;
    }
    gameseq_request request;
    request.type = GS_TRANSITION_PUSH;
    request.state = new_state;
    request.uninterruptible = false;
    State_request_queue.add(request);
    State_info[new_state].transparent = transparent;
    State_info[new_state].pause_beneath = pause_beneath;
}

// 0x007D8870 (:470)
void gameseq_pop_state()
{
    const int num = State_request_queue.size();
    if (num > 0 && State_request_queue[num - 1].uninterruptible)
    {
        GAME_TRACE("state", "%s dropped: an uninterruptible change is pending", "pop");
        keen::debug::nativePrintArgs("Someone is trying to set a new state, when another uninteruptible state change is pending.  If you hit this, please grab a programmer and do not skip this!!");
        return;
    }
    gameseq_request request;
    request.type = GS_TRANSITION_POP;
    request.state = GS_NONE;
    request.uninterruptible = false;
    State_request_queue.add(request);
}

// 0x007BFCD0 (:498)
game_state gameseq_get_previous_state()
{
    if (Stack_top_index > 0)
    {
        return State_stack[Stack_top_index - 1];
    }
    return GS_NONE;
}

// 0x007BFCF0 (:509)
game_state gameseq_get_state()
{
    if (Stack_top_index > -1)
    {
        return State_stack[Stack_top_index];
    }
    return GS_NONE;
}

// 0x007CC8F0 (:520)
// The state of the last queued request (GS_NONE for a pop), or GS_NONE without requests.
game_state gameseq_get_pending_state()
{
    const int num = State_request_queue.size();
    if (num > 0)
    {
        return State_request_queue[num - 1].state;
    }
    return GS_NONE;
}

// 0x007CC910 (:533)
game_state gameseq_process(gameseq_state_step step, rl_renderer* pRenderer, bool no_handshake)
{
    if (Stack_top_index < 0 || State_stack[Stack_top_index] != GS_GAMEPLAY)
    {
        nvidia_highlights_close_group(s_groupIdGameplay, false);
    }
    else
    {
        nvidia_highlights_open_group(s_groupIdGameplay);
    }
    int index;
    game_state state;
    if (step == GS_STEP_PROCESS)
    {
        if (State_request_queue.size() > 0)
        {
            Wait_for_prev_done_frame = Framecount_increment_always;
        }
        bool deferred = false;
        for (int i = 0; i < State_request_queue.size(); i++)
        {
            const game_state top = Stack_top_index < 0 ? GS_NONE : State_stack[Stack_top_index];
            if (deferred)
            {
                break;
            }
            const gameseq_request& request = State_request_queue[i];
            if (request.type == GS_TRANSITION_PUSH)
            {
                if (Stack_top_index < 15 && !gameseq_state_is_active(request.state))
                {
                    Stack_top_index++;
                    State_stack[Stack_top_index] = request.state;
                    GAME_TRACE("state", "push %s on %s", gameseq_trace_state_name(request.state), gameseq_trace_state_name(top));
                    gameseq_enter_state(request.state, top);
                    continue;
                }
                GAME_TRACE("state", "push %s refused: the stack is full or the state is active", gameseq_trace_state_name(request.state));
                VLIB_CRITICAL_FAILURE();
            }
            else if (request.type == GS_TRANSITION_POP)
            {
                if (Stack_top_index < 1)
                {
                    VLIB_CRITICAL_FAILURE();
                    continue;
                }
                const game_state beneath = State_stack[Stack_top_index - 1];
                if (State_info[State_stack[Stack_top_index]].exit_notify == nullptr || State_info[State_stack[Stack_top_index]].exit_notify(beneath))
                {
                    GAME_TRACE("state", "pop %s to %s", gameseq_trace_state_name(top), gameseq_trace_state_name(beneath));
                    Stack_top_index--;
                    gameseq_exit_state(top, beneath);
                    ui_cursor_set_visible(!State_info[beneath].hide_cursor);
                }
                else
                {
                    gameseq_trace_deferred(beneath, top);
                    deferred = true;
                }
            }
            else if (request.type == GS_TRANSITION_SET)
            {
                while (Stack_top_index > -1)
                {
                    if (State_info[State_stack[Stack_top_index]].exit_notify != nullptr && !State_info[State_stack[Stack_top_index]].exit_notify(request.state))
                    {
                        gameseq_trace_deferred(request.state, State_stack[Stack_top_index]);
                        deferred = true;
                        break;
                    }
                    const game_state leaving = State_stack[Stack_top_index];
                    GAME_TRACE("state", "set %s: leaving %s", gameseq_trace_state_name(request.state), gameseq_trace_state_name(leaving));
                    Stack_top_index--;
                    gameseq_exit_state(leaving, request.state);
                }
                if (deferred)
                {
                    continue;
                }
                GAME_TRACE("state", "set %s", gameseq_trace_state_name(request.state));
                State_stack[0] = request.state;
                Stack_top_index = 0;
                if (request.state != GS_NONE)
                {
                    if (State_info[request.state].enter != nullptr)
                    {
                        State_info[request.state].enter(GS_NONE);
                    }
                    ui_cursor_set_visible(!State_info[request.state].hide_cursor);
                }
            }
            else
            {
                VLIB_CRITICAL_FAILURE();
            }
        }
        if (!deferred)
        {
            // Notes (decompiled behaviour, kept as is): a deferred request keeps the whole queue, including the
            // requests already applied before it, which run again next frame.
            State_request_queue.clear();
            Trace_deferred_state = GS_NONE;
            if (Stack_top_index == 0)
            {
                State_info[State_stack[0]].transparent = false;
            }
        }
        index = Stack_top_index;
        state = State_stack[Stack_top_index];
    }
    else if (step == GS_STEP_RENDER && !no_handshake)
    {
        index = Render_snapshot.stack_top_index;
        state = Render_snapshot.state_stack[Render_snapshot.stack_top_index];
    }
    else
    {
        index = Stack_top_index;
        state = State_stack[Stack_top_index];
    }
    if (State_info[state].transparent)
    {
        if (index < 1)
        {
            VLIB_CRITICAL_FAILURE();
        }
        else
        {
            gameseq_do_state(index - 1, State_info[state].pause_beneath, step, no_handshake, pRenderer);
        }
    }
    if (step == GS_STEP_PROCESS)
    {
        if (State_info[state].process != nullptr)
        {
            State_info[state].process();
        }
    }
    else if (step == GS_STEP_HANDSHAKE)
    {
        if (State_info[state].handshake != nullptr)
        {
            State_info[state].handshake(pRenderer);
        }
    }
    else if (step == GS_STEP_RENDER && State_info[state].render != nullptr)
    {
        State_info[state].render(pRenderer);
    }
    if (step == GS_STEP_PROCESS && Game_trace_file != nullptr && Game_trace_tick_hook != nullptr)
    {
        Game_trace_tick_hook();
    }
    const game_state top = Stack_top_index < 0 ? GS_NONE : State_stack[Stack_top_index];
    AUDIOLIB_GAME_STATE audio_game_state = AUDIOLIB_GAME_STATE_IN_LEVEL;
    switch (top)
    {
    case GS_LOAD:
    case GS_BOOT:
    case GS_CREDITS:
        audio_game_state = AUDIOLIB_GAME_STATE_LOADING;
        break;
    case GS_MULTI_INIT:
    case GS_MULTI_SHUTDOWN:
        audio_game_state = AUDIOLIB_GAME_STATE_UNITIALIZED;
        break;
    default:
        break;
    }
    if (hud_ui_get_loaded_module() == HUD_MODULE_SATELLITE && top != GS_INGAME_HANDBOOK_HUD)
    {
        audio_game_state = AUDIOLIB_GAME_STATE_SATELLITE;
    }
    audiolib_system_set_game_state(audio_game_state);
    return top;
}

// 0x007BFD10 (:680)
bool gameseq_state_is_active(game_state state)
{
    for (int i = 0; i <= Stack_top_index; i++)
    {
        if (State_stack[i] == state)
        {
            return true;
        }
    }
    return false;
}

// 0x007BFD40 (:693)
bool gameseq_is_state_in_stack(game_state state)
{
    for (int i = 0; i < 16; i++)
    {
        if (State_stack[i] == state)
        {
            return true;
        }
    }
    return false;
}

// 0x007CCD30 (:705)
bool gameseq_in_transition()
{
    return State_request_queue.size() > 0;
}

// 0x007BFD70 (:716)
int gameseq_get_user_flags(game_state state)
{
    return State_info[state].user_flags;
}

// 0x007BFD80 (:721)
bool gameseq_has_current_state_self_stream_flag()
{
    if (Stack_top_index > -1)
    {
        return (State_info[State_stack[Stack_top_index]].user_flags & 1) != 0;
    }
    return false; // see the file comment
}

// 0x007BFDB0 (:728)
void gameseq_save_stack(game_state* backup_stack, state_info* backup_state_info, int* backup_stack_top_index)
{
    for (int i = 0; i < 16; i++)
    {
        backup_stack[i] = State_stack[i];
    }
    for (int i = 0; i < 64; i++)
    {
        backup_state_info[i] = State_info[i];
    }
    *backup_stack_top_index = Stack_top_index;
}

// 0x007BFE30 (:744)
void gameseq_load_stack(game_state* backup_stack, state_info* backup_state_info, int backup_stack_top_index)
{
    for (int i = 0; i < 16; i++)
    {
        State_stack[i] = backup_stack[i];
    }
    for (int i = 0; i < 64; i++)
    {
        State_info[i] = backup_state_info[i];
    }
    Stack_top_index = backup_stack_top_index;
}

// 0x007CCD40 (:757)
void gameseq_abort_state_transition()
{
    State_request_queue.clear();
}

// 0x007BFEB0 (:764)
void game_handshake_copy_gameseq(game_state (&dest)[16], int& dest_index)
{
    for (int i = 0; i < 16; i++)
    {
        dest[i] = State_stack[i];
    }
    dest_index = Stack_top_index;
}

// 0x007BFEE0 (:771)
bool gameseq_current_state_has_cursor()
{
    if (Stack_top_index > -1 && State_stack[Stack_top_index] != GS_NONE)
    {
        return !State_info[State_stack[Stack_top_index]].hide_cursor;
    }
    return false;
}
