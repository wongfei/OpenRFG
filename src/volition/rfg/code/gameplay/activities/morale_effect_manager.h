// Original: volition/rfg/code/gameplay/activities/morale_effect_manager.h
// Implementation: volition/rfg/code/gameplay/activities/morale_effect_manager.cpp
//
// Civilian morale effects: converting civilians to guerrillas around events (draft mode, spontaneous conversion after
// nearby events, sound propagation).
#pragma once


class human;
class npc;
class vector;

void morale_effect_level_init_pre(); // 0x00771920 (:409)
void morale_effect_do_frame(); // 0x007B3DC0 (:420)
void morale_effect_disable_encounter(); // 0x0075B660 (:467)
void morale_start_draft_mode(); // 0x00771950 (:473)
void morale_end_draft_mode(); // 0x0075B670 (:481)
bool morale_npc_can_be_converted(const npc* np); // 0x007AF310 (:487)
int morale_maybe_convert_civilians_spontaneously_around_event(const vector& pos, float radius, const human* enemy, float pct_chance); // 0x007B3E40 (:566)
bool morale_maybe_convert_civilian_spontaneously(npc* np, const human* enemy, float pct_chance); // 0x007AF3E0 (:660)
void morale_convert_single_npc(npc* np, const human* enemy_to_target); // 0x00788E60 (:727)
bool morale_use_sound_propagation_spontaneous_conversion(); // 0x0075B680 (:770)
