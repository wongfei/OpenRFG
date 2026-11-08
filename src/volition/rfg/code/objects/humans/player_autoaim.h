// Original: volition/rfg/code/objects/humans/player_autoaim.h
// Implementation: volition/rfg/code/objects/humans/player_autoaim.cpp
//
// The player's aim assist: picking a combat target and slowing or steering the aim towards it (tweak table driven),
// and the RPG lock-on cones.
#pragma once


class vector;

void player_autoaim_apply_assisted_aiming(); // 0x00A868D0 (:146)
void player_autoaim_calculate_assisted_aiming(); // 0x00A5D5E0 (:302)
void player_autoaim_render_debug(); // 0x00A114D0 (:540)
vector player_autoaim_get_aim_vector(); // 0x00A114E0 (:589)
float player_get_aim_assist_dist(); // 0x00A11540 (:597)
void player_set_autoaim_enabled(bool enabled); // 0x00A11550 (:602)
bool player_get_autoaim_enabled(); // 0x00A11560 (:607)

// Aim assist on (the game options read and reset it; player_autoaim.cpp).
extern bool Auto_aim_enabled; // 0x01694496
