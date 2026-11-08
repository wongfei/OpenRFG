// Original: volition/rfg/code/spawn/spawn_request.h
// Implementation: volition/rfg/code/spawn/spawn_request.cpp
//
// Spawn requests: the parameters of an NPC spawn (info, team, position or spawn node, orders, flags) and of a vehicle
// spawn, collected in an npc_spawn_param_list that object_spawn_do_spawn processes.
#pragma once

#include <stdint.h>
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_shared_defines.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/varray.h"

class human_info;
class navpoint;
class object;
class object_spawn_region;
class rfg_mover;
class vehicle_info;
class vehicle_spawn_group_info;

// size 0x50
class npc_spawn_params
{
public:
    npc_spawn_params(const human_info* new_hum_info); // 0x007053D0 (.h:36)
    npc_spawn_params(); // 0x007053C0 (.h:35)
    bool spawn_failed() const; // 0x00B40FC0 (.h:80)
    bool spawn_attempted() const; // 0x00B40FE0 (.h:83)
    void set_nav_cell_handle(uint64_t _handle); // 0x00B40FF0 (.h:85)
    bool is_nav_cell_handle_set(); // 0x00B41010 (.h:87)
    uint64_t get_nav_cell_handle();
    void reset(); // 0x00B43BD0 (:45)
    bool rm_check_ready() const; // 0x00B43C00 (:57)
    bool placement_ready() const; // 0x00B43C20 (:65)

    const human_info* hum_info; // 0x00
    int member_load; // 0x04
    npc* np; // 0x08
    vector spawn_pos; // 0x0C
    matrix spawn_orient; // 0x18
    uint64_t nav_cell_handle; // 0x40
    struct
    {
        bool attempted_spawned : 1; // 0x00:0
        bool rm_checked : 1; // 0x00:1
        bool nav_cell_set : 1; // 0x00:2
    } spawn_flags; // 0x48
};

// size 0x5C
class vehicle_spawn_params
{
public:
    vehicle_spawn_params(); // 0x00705400 (.h:208)
    bool pick_vehicle_info(const vehicle_spawn_group_info* v_spawn_group); // 0x00B5CB60 (:104)
    bool pick_special_vehicle_info(const vehicle_spawn_group_info* v_spawn_group); // 0x00B5CB80 (:111)
    bool rm_check_ready() const; // 0x00B43CD0 (:141)
    bool placement_ready() const; // 0x00B43D00 (:157)
    void reset(); // 0x00B43C50 (:87)

    const vehicle_info* veh_info; // 0x00
    vector spawn_pos; // 0x04
    const object_spawn_region* osrp; // 0x10
    unsigned short zone_id_xy; // 0x14
    matrix spawn_orient; // 0x18
    float forward_speed; // 0x3C
    unsigned int parent_handle; // 0x40
    unsigned int building_handle; // 0x44
    object_spawn_priority spawn_priority; // 0x48
    vehicle_flags vflags; // 0x4C
    struct
    {
        unsigned int no_save : 1; // 0x00:0
        unsigned int one_attempt : 1; // 0x00:1
        unsigned int interior_spawn : 1; // 0x00:2
        unsigned int road_spawn : 1; // 0x00:3
        unsigned int high_priority_clean_up : 1; // 0x00:4
        unsigned int light_clean_up : 1; // 0x00:5
        unsigned int force_to_ground : 1; // 0x00:6
        unsigned int air_vehicle : 1; // 0x00:7
        unsigned int static_spawn_placement : 1; // 0x00:8
        unsigned int convoy_vehicle_spawn : 1; // 0x00:9
        unsigned int mobile_vehicle_spawn : 1; // 0x00:10
        unsigned int infinite_mass : 1; // 0x00:11
        unsigned int has_car_bomb : 1; // 0x00:12
    } veh_spawn_flags; // 0x54
    vehicle* vp; // 0x58
};

// size 0x1
class npc_spawn_param_flags
{
public:
    npc_spawn_param_flags(); // 0x00708680 (.h:105)
    void reset(); // 0x007053F0 (.h:121)

    bool spawn_in_cover : 1; // 0x00:0
    bool spawn_in_guard : 1; // 0x00:1
    bool spawn_on_node : 1; // 0x00:2
    bool spawn_in_action_node : 1; // 0x00:3
    bool should_save : 1; // 0x00:4
    bool static_spawn_placement : 1; // 0x00:5
};

// size 0x3A8
class npc_spawn_param_list
{
public:
    npc_spawn_param_list(npc_manager_systems creation_system); // 0x0070BE40 (.h:130)
    void reset(); // 0x00B48B70 (:172)
    bool all_spawned() const; // 0x00B48C70 (:221)
    npc_spawn_params* find_next_npc_spawn(); // 0x00B48C40 (:207)
    bool rm_check_ready() const; // 0x00B48CB0 (:238)
    bool placement_ready() const; // 0x00B48D10 (:265)
    void assign_spawn_parent(const object* new_spawn_parent_obj); // 0x00B43D30 (:196)
    ~npc_spawn_param_list(); // 0x0070CFC0

    vector spawn_pos; // 0x00
    const object_spawn_region* osrp; // 0x0C
    farray<npc_spawn_params,10> npc_spawn_info; // 0x10
    const npc_manager_systems management_system; // 0x340
    unsigned short zone_id_xy; // 0x344
    vehicle* vp; // 0x348
    vehicle_seat_index first_seat; // 0x34C
    const navpoint* navp; // 0x350
    const object* spawn_parent_obj; // 0x354
    unsigned int spawn_building_handle; // 0x358
    int ambient_behavior_id; // 0x35C
    npc_flags_struct npc_flags; // 0x360
    human_flags hflags; // 0x36C
    npc_spawn_param_flags spawn_flags; // 0x37C
    matrix spawn_orient; // 0x380
    const char* node_type; // 0x3A4
};

const rfg_mover* find_spawn_building_handle(const object* objp); // 0x00B48B40 (:14)
