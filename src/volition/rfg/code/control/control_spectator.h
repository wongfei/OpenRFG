// Original: volition/rfg/code/control/control_spectator.h
// Implementation: volition/rfg/code/control/control_spectator.cpp
//
// The multiplayer spectator controls (the CM_SPECTATOR control mode): cycling the viewed player and the spectator
// camera. The signatures are the PDB's.
#pragma once

class object;

void control_spectator_enter(); // 0x00702BD0 (:52)
void control_spectator_exit(); // 0x006FB390 (:81)
void control_spectator_process(); // 0x00702C70 (:91)
void control_spectator_render_debug(); // 0x006FB3E0 (:193)
void control_spectator_set_viewed_player(unsigned int player_handle); // 0x00700360 (:199)
void control_spectator_next_viewed_player(bool force); // 0x007003E0 (:234)
void control_spectator_prev_viewed_player(bool force); // 0x00700470 (:276)
bool control_spectator_is_spectating_object(const object* target); // 0x006FB3F0 (:306)
bool control_spectator_camera_is_locked(); // 0x006FB420 (:311)
