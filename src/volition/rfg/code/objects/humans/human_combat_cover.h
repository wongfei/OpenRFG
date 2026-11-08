// Original: volition/rfg/code/objects/humans/human_combat_cover.h
// Implementation: volition/rfg/code/objects/humans/human_combat_cover.cpp
//
// A human's use of cover nodes: reserving, using and abandoning a node, and its stepped in/out state.
#pragma once

#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/objects/object_cover_node_enums.h"

class cover_node;
class human;

bool human_combat_cover_has_node(const human* hp); // 0x005DDB50 (.h:28)
cover_node* human_combat_cover_get_node(const human* hp); // 0x00A2FAD0 (:29)
bool human_combat_cover_is_using_node(const human* hp); // 0x00A09D70 (:36)
bool human_combat_cover_is_stepped_in(const human* hp); // 0x00A09D90 (:43)
bool human_combat_cover_is_stepped_out(const human* hp); // 0x00A09DC0 (:62)
void human_combat_cover_abandon_node(human* hp); // 0x00A9D060 (:85)
void human_combat_cover_reserve_node(human* hp, cover_node* node); // 0x00A9D190 (:124)
animation_action human_combat_cover_get_transition_action(const human* hp, const cover_node* node, cover_node_stances cover_stance); // 0x00A09DE0 (:157)
void human_combat_cover_set_stepped_in_state(human* hp, cover_node_stances cover_stance); // 0x00A0A020 (:184)
void human_combat_cover_set_stepped_out_state(human* hp, cover_node_stances cover_stance, cover_node_firing_positions position); // 0x00A0A290 (:213)
