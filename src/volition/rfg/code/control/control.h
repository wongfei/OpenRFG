// Original: volition/rfg/code/control/control.h
// Implementation: volition/rfg/code/control/control.cpp
//
// The control system: the active control mode (gameplay or spectator), entered and left through control_set_mode, and
// the per-frame dispatch to the mode's controls (control_process). Placement of control_mode inferred.
#pragma once

enum control_mode
{
    CM_INVALID = 0,
    CM_GAMEPLAY = 1,
    CM_SPECTATOR = 2,
    NUM_CONTROL_MODES = 3,
};

// The callbacks of one control mode. Placement inferred (the table is a public global of control.cpp).
// size 0x10
struct control_mode_funcs
{
    void (*enter)(); // 0x00
    void (*exit)(); // 0x04
    void (*process)(); // 0x08
    void (*render_debug)(); // 0x0C
};

// Indexed by control_mode. CM_INVALID is all null; CM_GAMEPLAY is control_gameplay_enter/_exit/_process/_render and
// CM_SPECTATOR control_spectator_enter/_exit/_process/_render_debug (rfg.exe .data, dumped in player_minimal.md P0).
extern control_mode_funcs Control_mode_funcs[NUM_CONTROL_MODES]; // 0x0164AD4C

void unused_dcf_player_pratfalls(); // 0x006F68E0 (:71)
void unused_dcf_raise_alert_level(); // 0x006FB730 (:82)
void unused_dcf_lower_alert_level(); // 0x006FB760 (:92)
void control_system_init(); // 0x006FB780 (:141)
void control_level_init_post(); // 0x00700990 (:153)
void control_process(); // 0x006FB790 (:169)
void control_set_mode(control_mode cm); // 0x006F6930 (:230)
control_mode control_get_mode(); // 0x006F6940 (:245)
