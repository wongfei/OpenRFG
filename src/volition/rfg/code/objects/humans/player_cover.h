// Original: volition/rfg/code/objects/humans/player_cover.h
// Implementation: volition/rfg/code/objects/humans/player_cover.cpp
//
// The player's cover: moving along cover, searching for its edges, leaning out, blind fire, crouching and exiting.
#pragma once


class object;
class player;
class vector;

bool cover_cast_ray(player* pp, float up, float right, vector* normal, float sphere_size, vector* hit_pos, float dist); // 0x00A86C70 (:57)
void player_cover_do_frame(player* pp); // 0x00A9EC30 (:118)
extern bool Player_cover_force_fine_aim; // 0x0302F0D0
// The game options' "disable cover".
extern bool Player_disable_cover; // 0x0302F0D4
void player_exit_cover(player* pp, bool dive); // 0x00A87030 (:673)
bool player_leanup_callback(object* hit_objp, void* vp); // 0x00A878E0 (:868)
void player_process_cover_crouch(player* pp, float h, float v); // 0x00A87900 (:881)
void player_process_cover_blindfire(player* pp); // 0x00A880D0 (:1048)
void player_process_cover_movement(player* pp, float h, float v); // 0x00A886F0 (:1189)
bool player_cover_search_for_edge(player* pp); // 0x00A11580 (:2012)
