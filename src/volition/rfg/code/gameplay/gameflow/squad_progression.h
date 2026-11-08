// Original: volition/rfg/code/gameplay/gameflow/squad_progression.h
// Implementation: volition/rfg/code/gameplay/gameflow/squad_progression.cpp
//
// Squad progression: which humans and vehicles a squad is made of at each progression level, reserved from the spawn
// resource manager.
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/spawn/spawn_enums.h"

struct squad_definition;
class squad_progression_set;
class vehicle_info;
class vehicle_spawn_group_info;

void squad_progression_system_init(int dlc_id); // 0x0078CD10 (:911)
void squad_progression_level_init(); // 0x007742C0 (:973)
bool squad_progression_member_human_info(squad_progression_set& member_info, human_teams team, const squad_definition* squad_def, bool first_member, bool reinforcement); // 0x007A5660 (:1029)
void squad_progression_reserve_members(spawn_slot_types slot_type, human_teams team, int count); // 0x0079D4A0 (:1087)
const vehicle_info* squad_progression_member_vehicle_info(human_teams team, const vehicle_spawn_group_info* veh_spawn_group); // 0x0078D060 (:1210)
