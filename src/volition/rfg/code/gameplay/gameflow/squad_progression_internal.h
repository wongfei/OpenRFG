// Original: volition/rfg/code/gameplay/gameflow/squad_progression_internal.h
//
// Squad progression tables: progression sets and levels (weighted human infos) and the per-squad progress info.
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/spawn/spawn_enums.h"
#include "volition/vlib/code/util/varray.h"

class human_info;
struct xml_element;

// size 0xC
class squad_progression_set
{
public:
    squad_progression_set(); // 0x0076A150 (squad_progression.h:16)
    void reset(); // 0x0075E480 (squad_progression.cpp:345)
    bool load(xml_element* xml_root); // 0x007740E0 (squad_progression.cpp:352)

    const human_info* h_info; // 0x00
    int member_load; // 0x04
    int spawn_weight; // 0x08
};

// size 0x94
class squad_progression_level
{
public:
    squad_progression_level(); // 0x0078C490 (.h:27)
    void reset(); // 0x00774170 (squad_progression.cpp:381)
    int get_weight_for_human(unsigned int srid) const; // 0x007741A0 (squad_progression.cpp:393)
    const human_info* find_h_info(unsigned int srid); // 0x007741D0 (squad_progression.cpp:408)
    void copy_from(const squad_progression_level& other_progression_info); // 0x0078C850 (squad_progression.cpp:491)
    bool load(xml_element* xml_root); // 0x0078C610 (squad_progression.cpp:423)
    ~squad_progression_level(); // 0x0078C4D0

    farray<squad_progression_set,8> squad_progression_list; // 0x00
    int total_spawn_weight; // 0x70
    const human_info* priority_reservations[8]; // 0x74
};

// size 0x4AC
class squad_progress_info
{
public:
    squad_progress_info(); // 0x0079D150 (.h:73)
    void reset(); // 0x00774210 (squad_progression.cpp:505)
    bool load(xml_element* xml_root, const char* new_name); // 0x0078C900 (squad_progression.cpp:519)
    void make_squad_reservations(int count, spawn_slot_types slot_type, const base_array<human_info const*>& desired_npc_list) const; // 0x00774250 (squad_progression.cpp:850)
    bool make_squad_reservations(spawn_slot_types slot_type) const; // 0x0079D320 (squad_progression.cpp:755)
    void make_squad_reservations(int count, spawn_slot_types slot_type) const; // 0x0079D3A0 (squad_progression.cpp:789)
    bool get_squad_member_info(squad_progression_set& member_info, const base_array<human_info const*>& desired_npc_list) const; // 0x0079D230 (squad_progression.cpp:706)
    bool get_squad_member_info(squad_progression_set& member_info, int progression_level) const; // 0x0078CA00 (squad_progression.cpp:576)
    bool get_squad_member_info(squad_progression_set& member_info) const; // 0x0079D1D0 (squad_progression.cpp:639)
    const human_info* select_human_info(const base_array<human_info const*>& desired_npc_list) const; // 0x0078CB90 (squad_progression.cpp:665)
private:
    int get_progression_level() const; // 0x0078CC70 (squad_progression.cpp:881)
public:
    ~squad_progress_info(); // 0x0078C4E0

    const char* name; // 0x00
    human_teams team; // 0x04
    squad_progression_level progression_list[8]; // 0x08
    int last_member_used; // 0x4A8
};
