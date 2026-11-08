// Original: volition/rfg/code/gameplay/activities/base_activity_data.h
// Implementation: volition/rfg/code/gameplay/activities/base_activity_data.cpp
//
// The common activity table entry (base_activity_data): name, level, voice persona reservation, music threshold and the
// standard rewards; base_activity_load_table reads the table (and DLC entries), activity_data_find_* look entries up.
#pragma once

#include "volition/rfg/code/gameplay/gameflow/gameflow_enums.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/vlib/code/util/varray.h"

class human_spawn_group_info;
class human_voice_persona;
class managed_layer_set;
class vehicle_spawn_group_info;
struct xml_element;

// size 0x50
class base_activity_data
{
public:
    base_activity_data(); // 0x0077E5D0 (.h:34)
    void reset(); // 0x00770FE0 (:184)
    bool reserve_voice(unsigned int activity_spawn_node) const; // 0x0075AE60 (:202)
    bool voice_loaded(unsigned int activity_spawn_node) const; // 0x0075AE80 (:213)
    bool release_voice(unsigned int activity_spawn_node) const; // 0x0075AEC0 (:225)
    bool can_reserve_voice(unsigned int activity_spawn_node) const; // 0x0075AEE0 (:235)
    bool load(xml_element* xml_root); // 0x007A2870 (:249)
    ~base_activity_data(); // 0x00764290
    base_activity_data& operator=(const base_activity_data& __that); // 0x007AC8F0

    const char* name; // 0x00
    float control_limit; // 0x04
    farray<int,4> managed_script_list; // 0x08
    managed_layer_set* managed_layers; // 0x28
    gfm_alert_levels min_alert_cap; // 0x2C
    gfm_alert_levels max_alert_cap; // 0x30
    const vehicle_spawn_group_info* v_spawn_group; // 0x34
    const vehicle_spawn_group_info* v_opt_spawn_group; // 0x38
    const human_spawn_group_info* h_spawn_group; // 0x3C
    const human_voice_persona* desired_voice; // 0x40
    GAMEMUSIC_THRESHOLDS desired_music_type; // 0x44
    const char* special_score_name; // 0x48
    struct
    {
        unsigned int scripts_loaded : 1; // 0x00:0
        unsigned int disabled : 1; // 0x00:1
        unsigned int load_layer_at_start : 1; // 0x00:2
        unsigned int reserve_vehicle_at_start : 1; // 0x00:3
        unsigned int reserve_human_at_start : 1; // 0x00:4
        unsigned int restore_at_start : 1; // 0x00:5
        unsigned int persona_at_start : 1; // 0x00:6
        unsigned int ratchet_alert_level : 1; // 0x00:7
        unsigned int allow_guerrilla_reinforcement : 1; // 0x00:8
    } activity_flags; // 0x4C
};

void base_activity_load_table(int dlc_id); // 0x007B3520 (:109)
void base_activity_unload_dlc(); // 0x007B36E0 (:167)
void activity_data_system_init(); // 0x007B37F0 (:424)
const base_activity_data* activity_data_find_handle(const char* data_name); // 0x00786AB0 (:451)
const base_activity_data* activity_data_find_by_index(int index); // 0x00771020 (:457)
