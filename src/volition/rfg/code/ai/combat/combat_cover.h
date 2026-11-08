// Original: volition/rfg/code/ai/combat/combat_cover.h
// Implementation: volition/rfg/code/ai/combat/combat_cover.cpp
//
// Cover helpers: paths to cover nodes, firing positions and stances at a node, stepping in and out.
#pragma once

#include "volition/rfg/code/ai/ai_types.h"
#include "volition/rfg/code/objects/humans/human_enums.h"
#include "volition/rfg/code/objects/object_cover_node_enums.h"

class cover_node;
class matrix;
class npc;
class vector;

bool cover_get_stepped_out_fvec(vector& fvec, cover_node_stances cover_stance, cover_node_firing_positions cover_position, const cover_node* node); // 0x005E3440 (:127)
bool cover_get_stepped_in_orient(matrix& final_orient, const cover_node_stances cover_stance, const cover_node* node); // 0x005E3500 (:152)
bool cover_is_at_node(const npc* np, const cover_node* node); // 0x005E35C0 (:235)
cover_node_stances cover_get_new_stance(const npc* np, const cover_node* node, const vector& target_pos); // 0x005E3600 (:252)
cover_node_firing_positions cover_pick_firing_position(cover_node_stances cover_stance, const cover_node* node, const vector& target_pos); // 0x005E3770 (:321)
cover_node_stances cover_pick_stance(const cover_node* node, const vector& target_pos); // 0x005E3870 (:367)
cover_node_stances cover_get_current_stance(const npc* np); // 0x005E39A0 (:406)
cover_node_firing_positions cover_get_current_firing_position(const npc* np); // 0x005E3A30 (:454)
ai_results cover_generate_path_to_node(npc* np, const cover_node* node, human_movement_states m_state, human_stances h_stance); // 0x006946B0 (:486)
