// Original: volition/rfg/code/gameplay/gameflow/gameflow_manager.h
// Implementation: volition/rfg/code/gameplay/gameflow/gameflow_manager.cpp
//
// The game flow manager (gfm): initializes and advances its modules (alert level, orders, reinforcements, morale and
// control, tech level, structure defense) once per frame, and saves and loads them.
#pragma once

#include "volition/rfg/code/gameplay/gameflow/gameflow_enums.h"

struct cfile;
class color;
class object;
class vector;

void gameflow_manager_init(); // 0x007A4910 (:94)
void gfm_level_init_pre(); // 0x007AAB60 (:107)
void gfm_do_frame(); // 0x007B97A0 (:129)
bool gfm_pick_exterior_point(const object* objp, vector& returned_pos); // 0x0078BCA0 (:157)
bool gfm_pick_exterior_point(const vector& point, float size, vector& returned_pos, const object* exclude_objp); // 0x007735F0 (:172)
void gfm_load(const cfile* input_file); // 0x0075DA20 (:237)
void gfm_save(cfile* output_file); // 0x0075DA30 (:246)
color gfm_get_alert_color(gfm_alert_levels alert_level); // 0x00773770 (:255)
