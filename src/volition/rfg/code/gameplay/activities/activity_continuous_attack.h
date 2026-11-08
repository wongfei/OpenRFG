// Original: volition/rfg/code/gameplay/activities/activity_continuous_attack.h
// Implementation: volition/rfg/code/gameplay/activities/activity_continuous_attack.cpp
//
// Continuous attack: EDF squads that keep attacking a target (or patrol) while an encounter runs, with a configurable
// attacker count and spawn group; scripts and DLC drive it through these functions.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_continuous_attack_internal.h"

class object;
class object_patrol;
class object_spawn_region;

base_continuous_attack_data* find_cont_attack_data(int cont_attack_index); // 0x0077FE60 (:156)
const object_spawn_region* find_local_spawn_region(bool ped_only); // 0x0077FED0 (:170)
object_patrol* check_for_patrol(const object_spawn_region* osrp); // 0x00796B70 (:239)
int activity_cont_attack_get_index(const char* cont_attack_name); // 0x00757570 (:856)
cont_attack_type activity_cont_attack_get_type_from_name(const char* type_name); // 0x00757590 (:866)
bool activity_cont_attack_get_attacker_count(int& count); // 0x007575D0 (:879)
void activity_cont_attack_set_attacker_count(int new_attacker_count); // 0x00757600 (:891)
bool activity_cont_attack_start(const object* target_obj, int cont_attack_index); // 0x007ACFA0 (:902)
void activity_cont_attack_end(); // 0x007A0DE0 (:934)
bool activity_cont_attack_check_squad(unsigned int squad_handle); // 0x0076A4A0 (:940)
void activity_cont_attack_set_spawn_group(unsigned int group_index); // 0x00757620 (:952)
int activity_cont_attack_get_squads(int max_num, unsigned int* squads); // 0x0076A4E0 (:959)
unsigned int activity_cont_attack_get_target(); // 0x00757640 (:964)
void activity_cont_attack_dlc_init(int dlc_id); // 0x00796BF0 (:969)
