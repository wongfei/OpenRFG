// Original: volition/rfg/code/gameplay/activities/activity_raid_attack.h
// Implementation: volition/rfg/code/gameplay/activities/activity_raid_attack.cpp
//
// Offensive raids (the player attacks with guerrillas): start info, start, and the intro npc.
#pragma once


class npc;
class vector;

bool activity_raid_attack_start_info(vector& start_pos, unsigned int& start_node_handle); // 0x00759CE0 (:918)
void activity_raid_attack_start(); // 0x00799C00 (:934)
void activity_raid_attack_level_init_post(); // 0x007B2DC0 (:952)
npc* activity_raid_attack_get_intro_npc(); // 0x007704E0 (:970)
