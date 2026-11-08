// Original: volition/rfg/code/spawn/spawn_resource_data.h
// Implementation: volition/rfg/code/spawn/spawn_resource_data.cpp
//
// Spawn resource sets: per zone (looked up in obj_zone::serialize, or the default set), how many human and vehicle
// resource slots of each info may be reserved, read from the spawn resource tables (DLC adds its own).
#pragma once

#include "volition/vlib/code/util/varray.h"

class human_spawn_group_info;
class vehicle_spawn_group_info;
struct xml_element;

// size 0x8
class human_reservation_info
{
public:
    human_reservation_info(); // 0x00B46F90 (.h:29)
    void reset(); // 0x00B43D60 (:187)
    bool load(xml_element* xml_root); // 0x00B51720 (:193)

    const human_spawn_group_info* spawn_group; // 0x00
    int count; // 0x04
};

// size 0x8
class vehicle_reservation_info
{
public:
    vehicle_reservation_info(); // 0x00B46FA0 (.h:47)
    void reset(); // 0x00B43D70 (:219)
    bool load(xml_element* xml_root); // 0x00B51790 (:226)

    const vehicle_spawn_group_info* spawn_group; // 0x00
    int count; // 0x04
};

// size 0xF0
class spawn_resource_data
{
public:
    spawn_resource_data(); // 0x00B4C3C0 (.h:74)
    void reset(); // 0x00B48D90 (:251)
    bool load(xml_element* xml_root, bool dlc_only); // 0x00B51800 (:267)
    bool check_for_allocation_errors() const; // 0x00B48DC0 (:341)

    const char* name; // 0x00
    int team_squad_counts[4]; // 0x04
    farray<human_reservation_info,16> required_human_slots; // 0x14
    farray<vehicle_reservation_info,7> required_vehicle_slots; // 0xA4
    bool dlc_resource; // 0xEC
};

// stub_impl.md S5: defined in downloadable_content.cpp, next to its file statics.
extern farray<spawn_resource_data*,22> Dlc_spawn_resource_data_list; // 0x0165E1B8

void spawn_res_dlc_unload(); // 0x00B56F60 (:82)
void spawn_load_resource_data(int dlc_id); // 0x00B57050 (:101)
void spawn_res_data_system_init(); // 0x00B5CC10 (:379)
void spawn_res_data_level_init_pre(); // 0x00B43D80 (:399)
const spawn_resource_data* spawn_res_data_find(const char* name); // 0x00B51A10 (:405)
const spawn_resource_data* spawn_res_data_default(); // 0x00B43D90 (:418)
const spawn_resource_data* spawn_res_data_current(); // 0x00B43DA0 (:424)
