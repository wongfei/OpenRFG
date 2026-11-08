// Original: volition/rfg/code/control/control.cpp
//
// The control mode (player_minimal.md P6): control_set_mode requests a mode, control_process switches to it (the
// modes' exit and enter callbacks) and runs the active mode's process callback every frame (gamestate_gp_simulate_frame
// after the interface). Compiled in rfg_control_unity.cpp, before control_gameplay.cpp, which uses this file's
// statics. Not ported: the debug console commands (unused_dcf_*).

#include "volition/rfg/code/control/control.h"

#include "volition/rfg/code/control/control_gameplay.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/control/control_spectator.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/vlib/code/input/input.h"

static float Player_normal_movement_last_v; // 0x01E2A9A0
static float Player_normal_movement_last_h; // 0x01E2A9A8
static control_mode Active_control_mode; // 0x01E2A9B0
static control_mode Pending_control_mode; // 0x01E2A9B4
static bool Control_player_pratfalls; // 0x01E2A9B8
static bool Player_input_disabled; // 0x01E2A9B9
static bool s_backpackButtonDownLastFrame; // 0x01E2A9C3
static unsigned int s_timeBackpackButtonPressedDown; // 0x01E2A9C4, the time the backpack button went down (ms)

// 0x0164AD4C (.data, dumped in player_minimal.md P0)
control_mode_funcs Control_mode_funcs[NUM_CONTROL_MODES] = {
    {nullptr, nullptr, nullptr, nullptr}, // CM_INVALID
    {control_gameplay_enter, control_gameplay_exit, control_gameplay_process, control_gameplay_render}, // CM_GAMEPLAY
    {control_spectator_enter, control_spectator_exit, control_spectator_process, control_spectator_render_debug}, // CM_SPECTATOR
};

static void control_exit_mode(control_mode cm); // 0x006F6910 (:107)
static void control_enter_mode(control_mode cm); // 0x006F6920 (:114)

// 0x006F6910 (:107)
// LTCG: cm in EAX.
static void control_exit_mode(control_mode cm)
{
    if (Control_mode_funcs[cm].exit != nullptr)
    {
        Control_mode_funcs[cm].exit();
    }
}

// 0x006F6920 (:114)
// LTCG: cm in EAX.
static void control_enter_mode(control_mode cm)
{
    if (Control_mode_funcs[cm].enter != nullptr)
    {
        Control_mode_funcs[cm].enter();
    }
}

// 0x006FB780 (:141)
void control_system_init()
{
}

// 0x00700990 (:153)
void control_level_init_post()
{
    Active_control_mode = CM_INVALID;
    control_gameplay_level_init_post(); // inlined in the binary
    Pending_control_mode = CM_INVALID;
}

// 0x006FB790 (:169)
void control_process()
{
    if (Pending_control_mode != Active_control_mode && Pending_control_mode != CM_INVALID)
    {
        control_exit_mode(Active_control_mode); // inlined in the binary
        Active_control_mode = Pending_control_mode;
        control_enter_mode(Pending_control_mode); // inlined in the binary
    }
    Pending_control_mode = CM_INVALID;
    // No rumble while paused or without a game pad (Player_controller_index 0xFFFFFFFF stops controller 0).
    if (game_is_paused() || !input_get_playing_with_controller())
    {
        input_set_feedback(Player_controller_index != 0xFFFFFFFF ? Player_controller_index : 0, 0.0f, 0.0f);
    }
    if (Control_mode_funcs[Active_control_mode].process != nullptr)
    {
        Control_mode_funcs[Active_control_mode].process();
    }
}

// 0x006F6930 (:230)
void control_set_mode(control_mode cm)
{
    Pending_control_mode = cm;
}

// 0x006F6940 (:245)
control_mode control_get_mode()
{
    return Active_control_mode;
}
