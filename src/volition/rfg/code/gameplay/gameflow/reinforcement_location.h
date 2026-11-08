// Original: volition/rfg/code/gameplay/gameflow/reinforcement_location.h
// Implementation: volition/rfg/code/gameplay/gameflow/reinforcement_location.cpp
//
// Reinforcement locations: where reinforcements should head, from the reported player and damage positions.
#pragma once


class npc;
class vector;

void reinforcement_loc_level_init_pre(); // 0x0075DFD0 (:80)
void reinforcement_loc_do_frame(); // 0x0078C210 (:92)
void reinforcement_report_player_pos(const npc* np, const vector& player_pos); // 0x0075E010 (:156)
void reinforcement_report_damage_pos(unsigned int damaged_object); // 0x0075E070 (:176)
const vector& reinforcement_get_best_position(); // 0x0075E260 (:218)
