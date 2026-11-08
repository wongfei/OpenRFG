// Original: volition/rfg/code/ai/ambient_edf_action_manager.h
// Implementation: volition/rfg/code/ai/ambient_edf_action_manager.cpp
//
// The ambient EDF action manager: occasional EDF presence around the player (fly-bys, drive-bys, harassing the player,
// dumping civilians out of vehicles to check their IDs) with participants, destinations and cooldowns.
// gameplay_do_frame calls amb_edf_action_manager_do_frame.
#pragma once

#include "volition/rfg/code/ai/ambient_edf_action_manager_enum.h"
#include "volition/vlib/code/math/vector3.h"

struct item_info;
class npc;
class object_path_road;
class player;

// size 0x1C
struct ambient_road_spawn_info
{
    ambient_road_spawn_info(); // 0x005DFB60

    vector destination_pos; // 0x00
    object_path_road* prp; // 0x0C
    unsigned short index; // 0x10
    float travel_dist; // 0x14
    float dist_sqrd; // 0x18
};

bool pick_flyer_destination_pos(vector& destination_pos, const player* pp); // 0x005DFF10 (:608)
void amb_edf_action_manager_level_init_post(); // 0x0060D530 (:1157)
void amb_edf_action_manager_do_frame(); // 0x00651F30 (:1174)
bool amb_edf_action_participant(const npc* np); // 0x005DFFC0 (:1194)
bool amb_edf_action_support_participant(const npc* np); // 0x005E0000 (:1209)
ambient_edf_action_type amb_edf_action_type(const npc* np); // 0x005E0030 (:1230)
bool amb_edf_action_complete(const npc* np); // 0x005E0060 (:1240)
const vector& amb_edf_action_destination(const npc* np); // 0x005E00A0 (:1250)
void amb_edf_set_dump_civ_cooldown(); // 0x005E00E0 (:1260)
void amb_edf_set_flyby_cooldown(); // 0x005E0100 (:1265)
void amb_edf_set_harass_player_cooldown(); // 0x005E0120 (:1270)
item_info* amb_edf_get_civ_id_item(); // 0x005E0140 (:1276)
int amb_edf_get_civ_id_anim_trigger_id(); // 0x005E0150 (:1282)
