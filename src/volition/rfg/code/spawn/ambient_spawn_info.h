// Original: volition/rfg/code/spawn/ambient_spawn_info.h
// Implementation: volition/rfg/code/spawn/ambient_spawn_info.cpp
//
// Ambient spawn sets: per zone (ZONE_PROP_SPAWN_INFO, looked up by name in obj_zone::serialize), the pedestrian and
// vehicle densities and the NPC and vehicle lists ambient spawning draws from. amb_spawn_info_current is the set at the
// player's position.
#pragma once

#include "volition/vlib/code/util/varray.h"

class human_info;
class human_spawn_group_info;
class vector;
class vehicle_spawn_group_info;
struct xml_element;

enum ambient_spawn_density
{
    AMB_SPAWN_DENSITY_INVALID = -1,
    AMB_SPAWN_DENSITY_NONE = 0,
    AMB_SPAWN_DENSITY_LOW = 1,
    AMB_SPAWN_DENSITY_MEDIUM = 2,
    AMB_SPAWN_DENSITY_HIGH = 3,
    NUM_AMB_SPAWN_DENSITY = 4,
};

// size 0x24
class ambient_spawn_info
{
public:
    ambient_spawn_info(); // 0x00B46F20 (.h:52)
    bool ped_spawn_enabled() const; // 0x00B40DC0 (.h:83)
    bool veh_spawn_enabled() const; // 0x00B40DD0 (.h:84)
    bool load(xml_element* xml_root); // 0x00B54AA0 (:110)
    void reset(); // 0x00B40DE0 (.h:91)

    char* group_name; // 0x00
    const human_spawn_group_info* h_spawn_group; // 0x04
    const vehicle_spawn_group_info* v_spawn_group; // 0x08
    ambient_spawn_density ped_density; // 0x0C
    ambient_spawn_density night_ped_density; // 0x10
    ambient_spawn_density veh_density; // 0x14
    ambient_spawn_density night_veh_density; // 0x18
    int ambient_behavior_id; // 0x1C
    struct
    {
        bool ped_spawn_enabled : 1; // 0x00:0
        bool veh_spawn_enabled : 1; // 0x00:1
    } ambient_flags; // 0x20
};

void amb_spawn_info_system_init(int dlc_id); // 0x00B54C50 (:193)
void amb_spawn_info_dlc_shutdown(); // 0x00B46FD0 (:252)
const ambient_spawn_info* amb_spawn_info_pos(const vector& pos); // 0x00B40F20 (:261)
const ambient_spawn_info* amb_spawn_info_current(); // 0x00B40F40 (:276)
const ambient_spawn_info* amb_spawn_info_from_name(const char* name); // 0x00B47000 (:291)
ambient_spawn_density amb_spawn_density_from_name(const char* density_name); // 0x00B40F70 (:313)
void amb_spawn_info_build_common_npc_list(farray<human_info const*,8>& returned_npc_list); // 0x00B54DF0 (:351)
