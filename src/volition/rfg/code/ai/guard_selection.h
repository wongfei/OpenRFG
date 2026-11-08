// Original: volition/rfg/code/ai/guard_selection.h
// Implementation: volition/rfg/code/ai/guard_selection.cpp
//
// Guard node selection: choosing a guard node (or sniper node) for an npc from the node groups of its orders, by
// selection mode.
#pragma once

#include "volition/vlib/code/util/varray.h"

class npc;
class object_guard_node;

enum guard_node_selection_modes
{
    GN_SELECT_PREFER_CURRENT_NODE = 0,
    GN_SELECT_CLOSEST_AVAILABLE = 0,
    GN_SELECT_NEW_NODE_SAME_GROUP = 1,
    GN_SELECT_DIFFERENT_GROUP = 2,
    NUM_GUARD_NODE_SELECTION_MODES = 3,
};

// size 0x10
struct guard_node_group_list
{
    guard_node_group_list(); // 0x005EAF30 (:31)

    int num_occupied_nodes; // 0x00
    float closest_node_dist_sq; // 0x04
    object_guard_node* closest_node; // 0x08
    unsigned char group_id; // 0x0C
};

object_guard_node* next_node_in_group(unsigned int np_handle, const base_array<object_guard_node*>& nodes, const object_guard_node* cur_node); // 0x00638D80 (:52)
object_guard_node* guard_selection_choose_node(const npc* np, guard_node_selection_modes mode); // 0x006614E0 (:233)
object_guard_node* guard_selection_choose_sniper_node(const npc* np, const object_guard_node* ignore_node); // 0x00661550 (:252)
