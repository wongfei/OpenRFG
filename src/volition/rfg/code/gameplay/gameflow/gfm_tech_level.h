// Original: volition/rfg/code/gameplay/gameflow/gfm_tech_level.h
// Implementation: volition/rfg/code/gameplay/gameflow/gfm_tech_level.cpp
//
// EDF tech level and progression: the tech level and its maximum, the EDF and marauder progression levels and the
// weapons each tech level unlocks (Tech_level_weapons); world::init_territory resets the tech level.
#pragma once


struct cfile;
struct inv_item_info;

// size 0x8
struct tech_level_weapon
{
    float tech_level_value; // 0x00
    inv_item_info* i_info; // 0x04
};

void unused_dcf_global_progression_level(); // 0x0079CF50 (:85)
void unused_dcf_edf_progression_level(); // 0x0079D000 (:107)
void unused_dcf_marauder_progression_level(); // 0x0079D070 (:129)
void unused_dcf_tech_level_set(); // 0x0079D0E0 (:152)
void unused_dcf_tech_level_change(); // 0x00773AF0 (:173)
void unused_dcf_tech_level_max_set(); // 0x00773BB0 (:193)
void unused_dcf_tech_level_max_change(); // 0x00773C50 (:213)
void unused_dcf_tech_level_display(); // 0x0079D140 (:233)
void gfm_tech_level_init(); // 0x00773CF0 (:244)
void gfm_reset_tech_level(); // 0x0078C120 (:280)
float gfm_get_tech_level(); // 0x0075DE90 (:289)
void gfm_set_tech_level(float tech_level_value); // 0x0075DEB0 (:298)
void gfm_change_tech_level(float tech_level_delta); // 0x0075DED0 (:303)
float gfm_get_tech_level_max(); // 0x0075DEF0 (:308)
void gfm_set_tech_level_max(float tech_level_max_value); // 0x0075DF00 (:313)
void gfm_change_tech_level_max(float tech_level_max_delta); // 0x0075DF20 (:318)
void edf_progression_set_level(int new_progression_level); // 0x00773EE0 (:324)
int edf_progression_level_internal(); // 0x0078C140 (:334)
void edf_progression_lock_mission(); // 0x0078C1D0 (:364)
int edf_progression_level(); // 0x0078C1E0 (:372)
void marauder_progression_set_level(int new_progression_level); // 0x00773F10 (:388)
int marauder_progression_level(); // 0x0075DF40 (:398)
void progression_level_system_save(cfile* save_file); // 0x0075DF60 (:408)
void progression_level_system_load(cfile* load_file); // 0x00773F40 (:417)
