// Original: volition/rfg/code/gameplay/missions.h
// Implementation: volition/rfg/code/gameplay/missions.cpp
//
// Story missions at run time: the active mission, its start and termination, and mission_data (handles and values
// scripts store per mission). mission_system_do_frame runs once per gameplay frame. Placement of mission_data
// inferred.
#pragma once

#include "volition/rfg/code/gameplay/mission_info.h"

struct cfile;
enum mission_handle;

// size 0x18
struct mission_data
{
    void reset(); // 0x00753CE0 (.h:24)

    unsigned int handle1; // 0x00
    unsigned int handle2; // 0x04
    unsigned int handle3; // 0x08
    float fl_value1; // 0x0C
    float fl_value2; // 0x10
    bool valid; // 0x14
};

void mission_system_level_init(); // 0x007A58B0 (:369)
void mission_system_level_init_post(); // 0x0075E9B0 (:411)
void mission_system_init(); // 0x0075E9C0 (:415)
void mission_system_level_shutdown(); // 0x0078E3D0 (:420)
bool mission_system_save(cfile* save_file); // 0x0078E420 (:438)
bool mission_system_load(cfile* save_file); // 0x0079D700 (:455)
bool mission_is_ready(); // 0x0078E460 (:468)
void mission_setup_do_frame(); // 0x0079D840 (:632)
void mission_end_warp_to_object(unsigned int warp_to_obj_handle); // 0x0075E9D0 (:758)
void mission_system_do_frame(); // 0x007B4CB0 (:766)
void mission_activate(mission_handle handle, bool autostart); // 0x0078E5E0 (:901)
void mission_activate(const char* mission_name, bool autostart); // 0x0079D9D0 (:907)
mission_handle mission_get_current_mission(); // 0x0075E9E0 (:933)
unsigned int mission_get_checkpoint_start_location_handle(); // 0x0075E9F0 (:938)
const char* mission_get_name(); // 0x0079DA30 (:951)
const char* mission_get_description(); // 0x0079DA60 (:956)
const char* mission_get_name_from_handle(mission_handle m_handle); // 0x0078E5F0 (:962)
const char* mission_get_description_from_handle(mission_handle m_handle); // 0x0078E620 (:975)
mission_mode mission_get_mode(); // 0x0079DA90 (:989)
mission_mode mission_get_mode_from_handle(mission_handle m_handle); // 0x0078E650 (:995)
const char* mission_crit_area_warning(); // 0x0078E680 (:1008)
const mission_data* mission_get_data(); // 0x00774970 (:1026)
bool mission_set_data(unsigned int handle1, unsigned int handle2, unsigned int handle3, float fl_value1, float fl_value2); // 0x00774990 (:1043)
void mission_clear_data(); // 0x0079DAC0 (:1064)
bool mission_is_active(); // 0x0075EA00 (:1084)
bool mission_is_terminating(); // 0x0075EA10 (:1090)
bool mission_layer_resources_serialized(); // 0x0078E6B0 (:1104)
void mission_cancel(); // 0x0078E6F0 (:1121)
void mission_failure(); // 0x0075EA40 (:1173)
void mission_player_died(); // 0x0075EA60 (:1181)
void mission_terminate(bool called_from_script); // 0x0075EA80 (:1194)
void mission_checkpoint_save(); // 0x0075EAA0 (:1201)
bool mission_set_player_never_die(bool never_die); // 0x0075EAB0 (:1257)
mission_handle mission_get_checkpoint_handle(); // 0x0075EAF0 (:1281)
void mission_terminate_cleanup(); // 0x007B0420 (:1287)
bool mission_autostarted(); // 0x0075EB00 (:1510)
bool mission_alert_allows_start(); // 0x0075EB10 (:1516)
bool mission_allow_at_yellow_alert(); // 0x0075EB40 (:1536)
