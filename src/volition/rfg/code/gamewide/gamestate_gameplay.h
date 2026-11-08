// Original: volition/rfg/code/gamewide/gamestate_gameplay.h
// Implementation: volition/rfg/code/gamewide/gamestate_gameplay.cpp
//
// The gameplay game state (GS_GAMEPLAY): gameseq calls gamestate_gp_process each sim frame, which scales Frametime by
// the debug speed scale and runs gamestate_gp_simulate_frame unless paused, then sets Sim_snapshot.in_gameplay.
// gamestate_gp_simulate_frame (file static, decompiled) first handles the death of each player (mission or activity
// failure, the death options state, or multiplayer respawn), then runs the frame in this order: HUD and camera,
// dialogue and music, the game clock, explosions, pathfinding, AI and convoys (single player), rfg_do_frame_pre
// (destruction), cutscenes, gameplay_do_frame, character matrices, turrets, vehicle interaction, players and humans
// (+ animations), items, salvage, debris, voices, vehicles and movers before the step; havok_simulate; then human
// velocities, vehicles, character controllers, humans, air bombs, projectiles, destruction events, turrets, camera,
// weapons, IK, items, cloth, triggers, missions and scripts, rfg_do_frame_post, weather, effects, traffic, movers and
// the remaining HUD updates; finally havok_bpo_do_frame and the collision event end. While the game is paused only
// the activity system and custom objects run.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

void gamestate_gp_init(); // 0x007F0F00 (:979)
void gamestate_gp_enter(game_state prev_state); // 0x007C0000 (:995)
void gamestate_gp_exit(game_state new_state); // 0x007F0D30 (:1066)
void gamestate_gp_process(); // 0x007EE450 (:1251)
void process_save_load(); // 0x007C0160 (:1281)
void gamestate_gp_handshake(rl_renderer* pRenderer); // 0x007D9120 (:1330)
void gamestate_gp_render(rl_renderer* pRenderer); // 0x007CD330 (:1360)
int gamestate_gp_last_havok_sim_time(); // 0x007C0200 (:1376)
void gamestate_gp_level_init_pre(); // 0x007EF790 (:1382)
void gamestate_gp_level_init_post(); // 0x007EFCB0 (:1598)

extern bool Reload_level_on_death; // 0x02533874, a single player death reloads the level (a debug setting)
