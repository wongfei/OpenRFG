// Original: volition/rfg/code/gamewide/gameseq.h
// Implementation: volition/rfg/code/gamewide/gameseq.cpp
//
// Game state machine (decompiled behaviour):
// - A stack of up to 16 game_states (file statics State_stack, Stack_top_index) and a per-state callback table
//   State_info[GS_NUM_STATES], filled by gameseq_register_state (gamestate_init in rfg.cpp registers the states).
// - gameseq_set/push/pop_state only queue a request in State_request_queue. push also stores `transparent` and
//   `pause_beneath` into State_info[new_state] immediately. A request is refused while the last queued one is
//   uninterruptible.
// - gameseq_process(GS_STEP_PROCESS) first applies the queued requests in order:
//     push: enter(new, old) unless the state is already active or the stack is full;
//     pop:  asks exit_notify(state beneath); false defers the rest of the queue to the next frame; else exit(...);
//     set:  pops everything (each may defer through exit_notify), then makes the new state the only one and enters it.
//   The cursor visibility follows the new top state's hide_cursor.
// - Then it runs the top state for the given step: process (sim), handshake (sync point) or render (render
//   thread, reading Render_snapshot.state_stack). A transparent state first runs the state beneath it
//   (gameseq_do_state, recursively); pause_beneath stops process/handshake below it but rendering continues.
// - Side effects: NVIDIA Highlights "gameplay" group open while GS_GAMEPLAY is on top; audiolib game state
//   (LOADING for GS_LOAD/GS_BOOT/GS_CREDITS, UNINITIALIZED for GS_MULTI_INIT/SHUTDOWN, SATELLITE for the satellite
//   HUD, otherwise IN_LEVEL).
#pragma once

template<typename T, int N> class farray;
class rl_renderer;

enum game_state
{
    GS_NONE = -1,
    GS_MAINMENU = 0,
    GS_GAMEPLAY = 1,
    GS_LOAD = 2,
    GS_BOOT = 3,
    GS_MULTI_INIT = 4,
    GS_MULTI_SHUTDOWN = 5,
    GS_CONNECTING_TO_INVITE = 6,
    GS_MAINMENU_OPTIONS = 7,
    GS_MAINMENU_OPTIONS_DISPLAY = 8,
    GS_MAINMENU_OPTIONS_AUDIO = 9,
    GS_MAINMENU_OPTIONS_LIVE = 10,
    GS_MAINMENU_OPTIONS_LIVE_QUICKMATCH = 11,
    GS_MAINMENU_OPTIONS_GAME_OPTIONS = 12,
    GS_MAINMENU_OPTIONS_EXTRAS = 13,
    GS_CHEATS = 14,
    GS_INGAME_OPTIONS = 15,
    GS_PLAYER_DEATH_OPTIONS = 16,
    GS_SONY_OPTIONS = 17,
    GS_CONTROLLER_OPTIONS = 18,
    GS_WRECKING_CREW_MAIN_MENU = 19,
    GS_WRECKING_CREW_CHARACTER_SELECT = 20,
    GS_WRECKING_CREW_SCOREBOARD = 21,
    GS_MULTIPLAYER_LIVE = 22,
    GS_MULTIPLAYER_CHANGE_MATCHMAKING = 23,
    GS_MULTIPLAYER_SEARCH_MATCHMAKING = 24,
    GS_MULTIPLAYER_GAME_LOBBY = 25,
    GS_MULTIPLAYER_PARTY_LOBBY = 26,
    GS_MULTIPLAYER_PARTY_OPTIONS = 27,
    GS_MULTIPLAYER_SYSLINK = 28,
    GS_MULTIPLAYER_SYSLINK_FIND_SERVERS = 29,
    GS_MULTIPLAYER_CHANGE_GAME_OPTIONS = 30,
    GS_MULTIPLAYER_PLAYER_SELECT = 31,
    GS_MULTIPLAYER_XP_TALLY = 32,
    GS_MULTIPLAYER_RESULTS = 33,
    GS_MULTIPLAYER_SCOREBOARD = 34,
    GS_MULTIPLAYER_STATS = 35,
    GS_MULTIPLAYER_TUTORIALS = 36,
    GS_INGAME_FULLSCREEN_MAP = 37,
    GS_INGAME_MISSION_BRIEFING = 38,
    GS_INGAME_ACTIVITY_BRIEFING = 39,
    GS_INGAME_MISSION_COMPLETE = 40,
    GS_INGAME_ACTIVITY_COMPLETE = 41,
    GS_INGAME_UPGRADES_HUD = 42,
    GS_INGAME_HANDBOOK_HUD = 43,
    GS_INGAME_RAIL_DRIVER_HUD = 44,
    GS_INGAME_DISTRICT_LIVERATION = 45,
    GS_INGAME_WEAPON_CABINET = 46,
    GS_LOAD_CUTSCENE = 47,
    GS_MAINMENU_IDLE = 48,
    GS_VIDEO_CUTSCENE_PLAY = 49,
    GS_SAVE_LOAD_SCREEN = 50,
    GS_PS3_DLC = 51,
    GS_VIEW_DLC_SCREEN = 52,
    GS_STATS_SCREEN = 53,
    GS_CREDITS = 54,
    GS_VERIFY_SAVEGAME = 55,
    GS_SHUTDOWN = 56,
    GS_DLC_INIT = 57,
    GS_DLC_SHUTDOWN = 58,
    GS_WC_INIT = 59,
    GS_WC_SHUTDOWN = 60,
    GS_RESOLUTION_CHANGE = 61,
    GS_QUICK_PAUSE = 62,
    GS_MULTIPLAYER_LIVE_FIND_SERVERS = 63,
    GS_NUM_STATES = 64,
};

enum gameseq_state_step
{
    GS_STEP_PROCESS = 0, // sim thread, rfg_do_frame
    GS_STEP_HANDSHAKE = 1, // sim thread inside the handshake, render thread blocked
    GS_STEP_RENDER = 2, // render thread
};

// Placement inferred.
enum gameseq_request_type
{
    GS_TRANSITION_PUSH = 0,
    GS_TRANSITION_POP = 1,
    GS_TRANSITION_SET = 2,
};

// Placement inferred.
// size 0xC
struct gameseq_request
{
    gameseq_request_type type; // 0x00
    game_state state; // 0x04
    bool uninterruptible; // 0x08
};

// Callbacks of one game state. Any callback may be null.
// size 0x24
class state_info
{
public:
    state_info(); // 0x007BFF10 (:781)
    bool is_registered() const; // 0x007B99F0 (.h:56)

    void (*enter)(game_state); // 0x00, argument: the previous top state (GS_NONE after a set)
    void (*exit)(game_state); // 0x04, argument: the state that replaces it
    bool (*exit_notify)(game_state); // 0x08, return false to postpone leaving this state
    void (*process)(); // 0x0C
    void (*handshake)(rl_renderer*); // 0x10
    void (*render)(rl_renderer*); // 0x14
    bool transparent; // 0x18, the state beneath also runs
    bool pause_beneath; // 0x19, ...but only renders
    int user_flags; // 0x1C, bit 0: no stream2_do_frame in the handshake (game_handshake_do_middle_of_frame)
    bool hide_cursor; // 0x20
private:
    friend void gameseq_register_state(game_state state, const state_info& callbacks);

    bool registered; // 0x21
};

extern farray<gameseq_request, 64> State_request_queue; // 0x0165E518
extern state_info State_info[64]; // 0x0253BDA0

bool gameseq_might_process_exit(); // 0x007CC8B0 (:160), requests pending; rfg_do_frame then holds the render lock
void gameseq_init(); // 0x007CC8C0 (:364)
void gameseq_register_state(game_state state, const state_info& callbacks); // 0x007BFC50 (:370)
bool gameseq_state_is_registered(game_state state); // 0x007BFCB0 (:379)
void gameseq_set_state(game_state state, bool uninterruptible); // 0x007D8710 (:393)
void gameseq_clear_all_pending_NEVER_CALL_ME_EXCEPT_DURING_EMERGENCY_SHUTDOWN(); // 0x007CC8E0 (:425)
void gameseq_push_state(game_state new_state, bool transparent, bool pause_beneath); // 0x007D87C0 (:437)
void gameseq_pop_state(); // 0x007D8870 (:470)
game_state gameseq_get_previous_state(); // 0x007BFCD0 (:498)
game_state gameseq_get_state(); // 0x007BFCF0 (:509), top of the stack or GS_NONE
game_state gameseq_get_pending_state(); // 0x007CC8F0 (:520)
game_state gameseq_process(gameseq_state_step step, rl_renderer* pRenderer, bool no_handshake); // 0x007CC910 (:533), returns the top state
bool gameseq_state_is_active(game_state state); // 0x007BFD10 (:680)
bool gameseq_is_state_in_stack(game_state state); // 0x007BFD40 (:693)
bool gameseq_in_transition(); // 0x007CCD30 (:705), requests pending
int gameseq_get_user_flags(game_state state); // 0x007BFD70 (:716)
bool gameseq_has_current_state_self_stream_flag(); // 0x007BFD80 (:721)
void gameseq_save_stack(game_state* backup_stack, state_info* backup_state_info, int* backup_stack_top_index); // 0x007BFDB0 (:728)
void gameseq_load_stack(game_state* backup_stack, state_info* backup_state_info, int backup_stack_top_index); // 0x007BFE30 (:744)
void gameseq_abort_state_transition(); // 0x007CCD40 (:757)
// Copies the stack for the render thread (Sim_snapshot, see game_handshake.h).
void game_handshake_copy_gameseq(game_state (&dest)[16], int& dest_index); // 0x007BFEB0 (:764)
const char* gameseq_trace_state_name(game_state state); // not in rfg.exe: the game trace (game_trace.h)
bool gameseq_current_state_has_cursor(); // 0x007BFEE0 (:771)
