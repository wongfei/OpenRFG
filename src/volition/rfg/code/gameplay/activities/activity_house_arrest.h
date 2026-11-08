// Original: volition/rfg/code/gameplay/activities/activity_house_arrest.h
// Implementation: volition/rfg/code/gameplay/activities/activity_house_arrest.cpp
//
// House arrest activity: rescuing hostages from EDF houses; the informant, escorts and hostages, streaming reports, the
// voice lines, and the start / replay / exit flow.
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_enums.h"
#include "volition/vlib/code/util/varray.h"

class hostage_info;
class human;
class human_spawn_group_info;
class npc;
class object;
class object_action_node;
class object_activity_spawn;
class object_house_arrest_node;
class player;
class rfg_mover;
class vector;

bool is_npc_an_escort(const player* pp, const npc* np); // 0x0076D350 (:326)
void pick_spawn_point(const object* spawn_objp, const human_spawn_group_info* spawn_group, vector& spawn_point); // 0x007823B0 (:348)
void house_arrest_level_init_post(); // 0x007B60D0 (:446)
unsigned int house_arrest_get_informant_npc(); // 0x00782790 (:791)
float house_arrest_get_nearest_activity(unsigned int& as_node_handle, unsigned int& ha_node_handle); // 0x007983A0 (:797)
object_action_node* house_arrest_get_action_node(object_activity_spawn* node); // 0x0076D800 (:842)
bool activity_house_arrest_start(const object_house_arrest_node* ha_node); // 0x007AD8C0 (:2922)
void activity_house_arrest_report_streaming(const object_house_arrest_node* ha_node); // 0x00758BD0 (:2950)
void activity_house_arrest_play_voice_line(const vector& pos, human_teams team, voice_lines line); // 0x007833F0 (:2956)
void activity_house_arrest_npc_at_safehouse(npc* np); // 0x0076E750 (:2961)
bool activity_house_arrest_is_house(const rfg_mover* house_target); // 0x0076E770 (:2968)
bool activity_house_arrest_nodes_are_relatives(unsigned int node_handle_a, unsigned int node_handle_b); // 0x0076E7F0 (:2993)
bool activity_house_arrest_all_hostages_freed(); // 0x00758BE0 (:3022)
bool activity_house_arrest_is_human_hostage(const human* hp); // 0x0076E860 (:3028)
void activity_house_arrest_exit(); // 0x007AD940 (:3041)
void activity_house_arrest_replay(); // 0x00758BF0 (:3062)
void activity_house_arrest_process_player_death(); // 0x00758C10 (:3074)
bool activity_house_arrest_has_interface(); // 0x00758C30 (:3081)
void activity_house_arrest_hostage_status(farray<hostage_info,3>& status_list); // 0x00783410 (:3087)
bool house_arrest_can_be_started(); // 0x007A1D10 (:3138)
unsigned int house_arrest_get_activity_node_startup_squad(unsigned int base_handle); // 0x00758C40 (:3170)
