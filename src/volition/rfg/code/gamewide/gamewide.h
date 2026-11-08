// Original: volition/rfg/code/gamewide/gamewide.h
// Implementation: volition/rfg/code/gamewide/gamewide.cpp
//
// Recovery scope (Tier 1): the per-frame entry points used by rfg_do_frame, plus the return-to-main-menu flow.
// Screenshots, invites and the loading screen are listed for completeness.
//
// game_input_poll (decompiled): controls_poll(); debug key toggles the console; chat keys (CBA_OPEN_CHAT /
// CBA_OPEN_TEAM_CHAT) open console chat in multiplayer; when the OS asks the window to close it requests
// gameseq_set_state(GS_SHUTDOWN, uninterruptible), or during GS_LOAD sets Gameplay_shutdown_in_progress and
// Game_boot_exit_flag instead.
// game_check_for_gameplay_shutdown (decompiled, start of rfg_do_frame): during GS_LOAD with a shutdown in progress it
// aborts the world load; otherwise a pending Gameplay_shutdown_in_progress becomes a state change back to
// GS_MAINMENU, through the multiplayer / DLC / Wrecking Crew shutdown states when one of those modes is active.
#pragma once

enum boot_to_main_menu_cause
{
    INVALID_BOOT_CAUSE = 0,
    BC_DLC_REMOVED = 1,
    BC_LOGIN_CHANGE = 2,
    BC_CONNECTION_LOST_XBL = 3,
    BC_CONNECTION_LOST_SYSLINK = 4,
    BC_GAME_MIGRATION_FAIL = 5,
    BC_HOST_LEFT = 6,
    BC_KICKED_FROM_LOBBY = 7,
    BC_LOBBY_MIGRATION_FAIL = 8,
    BC_GAME_ALREADY_STARTED = 9,
    BC_DLC = 10,
    BC_SAVE_ERROR = 11,
    BC_PROFILE_LOAD_FAILED = 12,
    BC_ACCEPTED_INVITE = 13,
    BC_REMOTE_GAME_SESSION = 14,
};

// Placement inferred (only used by gamewide.cpp, file static Booted_to_main_menu).
// size 0xC
struct boot_to_main_menu_info
{
    bool booted; // 0x00
    boot_to_main_menu_cause boot_cause; // 0x04
    int msgbox_handle; // 0x08, message box explaining the cause, -1 when none
};

extern bool Handled_debug_key; // 0x0165CBD1
extern bool Gameplay_shutdown_in_progress; // 0x02533876
extern bool Screenshot_pending; // 0x0253386B
extern bool Dump_tga; // 0x0253387B

void gamewide_process_invite(); // 0x007C0740 (:304)
bool gamewide_check_and_clear_invite_flag(); // 0x007C07D0 (:324)
void game_clear_boot_to_main_menu(); // 0x007C07E0 (:331)
void game_boot_to_main_menu(boot_to_main_menu_cause cause); // 0x007D91B0 (:337)
void game_show_boot_to_main_menu_message_box(); // 0x007C07F0 (:429)
void gamewide_shutdown(); // 0x007D92B0 (:500)
void gamewide_system_init(); // 0x007E9010 (:571)
void gamewide_login_changed(bool only_changed_live_connectivity, bool remote_game_session_related); // 0x007D9390 (:596)
bool game_should_exit_due_to_profile_change(); // 0x007C09C0 (:647)
bool game_booted_to_main_menu(); // 0x007C0A60 (:684)
boot_to_main_menu_cause game_get_booted_to_main_menu_cause(); // 0x007C0A70 (:690)
void game_check_for_gameplay_shutdown(); // 0x007D96E0 (:700)
void game_input_poll(); // 0x007D9830 (:776)
void game_print_screen(char* filename); // 0x007C0A90 (:901)
void game_do_pending_screenshot(); // 0x007C0AA0 (:925)
void game_display_loading_screen(); // 0x007C0AE0 (:948)
