// Original: volition/rfg/code/gameplay/activities/vrail_data.h
// Implementation: volition/rfg/code/gameplay/activities/vrail_data.cpp
//
// Vehicle rail definitions (rails, squads and vehicles) read from the vehicle rail table (and DLC entries).
#pragma once

#include "volition/vlib/code/util/varray.h"

struct squad_definition;
class vehicle_spawn_group_info;
struct xml_element;

// size 0xC
class squad_vehicle_pair
{
public:
    squad_vehicle_pair(); // 0x00768E40 (.h:42)
    void reset(); // 0x00754030 (.h:48)

    const squad_definition* squad_def; // 0x00
    const vehicle_spawn_group_info* veh_group; // 0x04
    int respawn_delay; // 0x08
};

// size 0xB4
class vehicle_rail_definition
{
public:
    vehicle_rail_definition(); // 0x0077E210 (.h:61)
    void reset(); // 0x007726C0 (:72)
    bool load(xml_element* xml_root); // 0x0078AA20 (:83)
    const squad_vehicle_pair& get_rand_veh_info() const; // 0x007726E0 (:183)

    const char* name; // 0x00
    int chase_vehicle_count; // 0x04
    const vehicle_spawn_group_info* player_veh_spawn_group; // 0x08
    const squad_definition* driver_squad_def; // 0x0C
    farray<squad_vehicle_pair,12> chase_vehicle_list; // 0x10
    struct
    {
        bool spawn_both : 1; // 0x00:0
        bool spawn_back : 1; // 0x00:1
    } vrail_flags; // 0xB0
};

const vehicle_rail_definition* vrail_data_get_def(int index); // 0x00772710 (:212)
const vehicle_rail_definition* vrail_data_get_def(const char* name); // 0x0078ACB0 (:222)
int vrail_data_get_type(const char* name); // 0x0078AD20 (:239)
void vrail_data_system_init(int dlc_id); // 0x007A31D0 (:263)
void vrail_data_unload_dlc(); // 0x0079B9A0 (:307)
