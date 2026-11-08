// Original: volition/rfg/code/ai/civilian/civilian_common.h
// Implementation: volition/rfg/code/ai/civilian/civilian_common.cpp
//
// Shared civilian helpers: walking the ped node network, driving along roads, honking, picking destinations.
#pragma once

#include "volition/rfg/code/objects/humans/human_enums.h"
#include "volition/rfg/code/spawn/ambient_spawn_manager.h"
#include "volition/vlib/code/math/vector3.h"

class navpoint;
class npc;
class object_path_road;
class vehicle;

// size 0x8
struct initial_ped_node_candidate
{
    initial_ped_node_candidate(const navpoint* npp_, const vector& reference_pos, const vector& reference_dir); // 0x005E0D20 (:42)
    initial_ped_node_candidate(); // 0x005E0D00 (:39)

    const navpoint* npp; // 0x00
    float direction_dot; // 0x04
};

// size 0x1C
class road_destination_info
{
public:
    road_destination_info(); // 0x005E0E10 (:55)

    const object_path_road* dest_prp; // 0x00
    vector dest_point; // 0x04
    int dest_index; // 0x10
    const object_path_road* current_prp; // 0x14
    float current_road_dist; // 0x18
};

const navpoint* find_last_destination_nav(const npc* np, const navpoint* dest_npp); // 0x005E1040 (:367)
bool civilian_is_near_ped_node(const npc* np); // 0x005E11D0 (:527)
const navpoint* civilian_pick_next_ped_node(npc* np); // 0x006542D0 (:537)
const navpoint* civilian_pick_new_ped_node(npc* np, ped_node_select_modes selection_mode); // 0x006545B0 (:592)
bool civilian_is_near_road(const npc* np); // 0x005E1290 (:617)
bool civilian_pick_destination_on_current_road(npc* np); // 0x00654610 (:646)
bool civilian_ped_enter_common(npc* np, ped_node_select_modes selection_mode); // 0x00654AF0 (:763)
bool civilian_ped_do_action_common(npc* np, human_movement_states m_state, ped_node_select_modes selection_mode); // 0x00690960 (:783)
void civilian_horn_do_enter(npc* np, bool same_action); // 0x005E12D0 (:829)
void civilian_horn_do_honk(npc* np, bool should_honk); // 0x00654E90 (:954)
void civilian_horn_do_clean_up(npc* np, bool same_action); // 0x005E1300 (:1001)
bool civilian_drive_do_action_common(npc* np, vehicle* vp, bool obey_checkpoints_and_roadblocks); // 0x00690A50 (:1012)
