// Original: volition/rfg/code/gameplay/gameflow/gfm_structure_defense_coordinator.h
// Implementation: volition/rfg/code/gameplay/gameflow/gfm_structure_defense_coordinator.cpp
//
// The structure defense coordinator (sdc): damage tracking reports for important EDF structures, so the EDF reacts when
// they are attacked. Movers create and destroy the reports.
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/obj_enums.h"

class gfm_damage_report;
class rfg_mover;

void sdc_level_init_pre(); // 0x007A5490 (:91)
void sdc_do_frame(); // 0x0078BF70 (:99)
int sdc_create_damage_tracking_report(rfg_mover* rfgmp, object_importance_level importance_level, human_teams team); // 0x0078C080 (:134)
void sdc_report_new_damage(rfg_mover* rfgmp, float percent_destroyed); // 0x00773A20 (:173)
void sdc_destroy_report(const rfg_mover* rfgmp); // 0x0079CE50 (:193)
gfm_damage_report* sdc_get_structure_damage_report(const rfg_mover* rfgmp); // 0x00773AB0 (:205)
