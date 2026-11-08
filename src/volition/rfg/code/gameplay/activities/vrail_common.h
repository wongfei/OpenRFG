// Original: volition/rfg/code/gameplay/activities/vrail_common.h
// Implementation: volition/rfg/code/gameplay/activities/vrail_common.cpp
//
// Shared vehicle rail data: the patrol (a vehicle following a rail of nodes, drop offs and waits) and the spawn data
// (chase vehicles, spawn regions along the rail).
#pragma once

#include "volition/rfg/code/gameplay/activities/reinforcement_squad.h"
#include "volition/rfg/code/gameplay/activities/vrail_system.h"
#include "volition/rfg/code/objects/object_patrol.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

class npc;
class object_spawn_region;
class object_squad;
class vehicle;
class vehicle_rail_definition;
class vrail_patrol_data;

// size 0x2C
class shared_vrail_patrol_data
{
public:
    shared_vrail_patrol_data(); // 0x00768EB0 (.h:87)
    bool vrail_init(object_squad* squad, const vector& drop_point); // 0x0078A500 (:267)
    unsigned int get_patrol_vehicle_handle() const; // 0x00772210 (:179)
    bool is_in_use() const; // 0x0075BB60 (:237)
    vrail_patrol_state do_frame(); // 0x0079B610 (:459)
    vrail_patrol_state get_patrol_status(const vrail_patrol_data& vrail_objects) const; // 0x007723A0 (:357)
    void release_and_reset(); // 0x007A9E10 (:336)
    bool player_in_vrail_vehicle() const; // 0x00772240 (:192)
    npc* get_vrail_driver() const; // 0x00772330 (:243)
protected:
    void reset(); // 0x0075BB20 (:167)
private:
    void update_vrail_progress(const vrail_patrol_data& vrail_objects); // 0x0075BC20 (:435)
    void update_vrail_progress_patrol(const vrail_patrol_data& vrail_objects); // 0x0075BB70 (:391)
    void update_vrail_progress_drop_off(const vrail_patrol_data& vrail_objects); // 0x0075BBC0 (:410)
    void update_vrail_progress_complete_wait(const vrail_patrol_data& vrail_objects); // 0x0075BBF0 (:421)
    bool get_frame_data(vrail_patrol_data& vrail_objects); // 0x00772290 (:204)

public:
    unsigned int vrail_patrol_handle; // 0x00
    unsigned int vrail_patrol_orders_handle; // 0x04
    unsigned int vrail_patrol_squad; // 0x08
    patrol_node_handle last_node_handle; // 0x0C
    vrail_patrol_state last_patrol_state; // 0x18
    vector drop_off_pos; // 0x1C
    struct
    {
        bool patrol_ready : 1; // 0x00:0
        bool found_last_node : 1; // 0x00:1
        bool visited_last_node : 1; // 0x00:2
        bool player_exited_vehicle : 1; // 0x00:3
        bool vrail_complete : 1; // 0x00:4
        bool vrail_failed : 1; // 0x00:5
    } vrail_patrol_flags; // 0x28
};

// size 0x20
class vrail_spawn_region_info
{
public:
    vrail_spawn_region_info(); // 0x00768E60 (.h:42)
    void reset(); // 0x00754170 (.h:51)

    unsigned int region_handle; // 0x00
    vector nearest_pos; // 0x04
    float dist_to_route; // 0x10
    int index_a; // 0x14
    int index_b; // 0x18
    bool past_region; // 0x1C
};

// size 0x8BC
class shared_vrail_spawn_data
{
public:
    shared_vrail_spawn_data(); // 0x007A0080 (.h:180)
    bool vrail_init(const object_squad* squad, const vehicle_rail_definition* new_vrail_def, const object_patrol* vrail_patrol, int new_chase_vehicle_count); // 0x0079B920 (:577)
    bool is_in_use() const; // 0x0075BCC0 (:566)
    void release_and_reset(); // 0x0079B7F0 (:540)
    void set_chase_vehicle_count(int new_chase_vehicle_count); // 0x00772410 (:600)
    void do_frame_spawn(); // 0x007B7080 (:607)
    void do_frame(); // 0x007AF620 (:617)
    int get_squads(base_array<object_squad*>& squad_list); // 0x0078A990 (:898)
protected:
    void reset(); // 0x0079B730 (:520)
private:
    void do_spawn(int squad_index, reinforcement_squad& chase_vehicle_info); // 0x007B4300 (:813)
    void do_frame_spawn_new(); // 0x007B44D0 (:877)
    const object_spawn_region* find_vehicle_spawn_region(); // 0x0078A950 (:794)
    const object_spawn_region* find_vehicle_spawn_region_both(); // 0x00772490 (:723)
    const object_spawn_region* find_vehicle_spawn_region_dir(bool behind_vrail); // 0x00772590 (:753)
    bool build_spawn_region_info(); // 0x0078A690 (:643)
    bool get_next_patrol_node_handle(patrol_node_handle& node_handle); // 0x00772440 (:627)
public:
    ~shared_vrail_spawn_data(); // 0x0077E3A0

    unsigned int vrail_patrol_handle; // 0x00
    unsigned int vrail_patrol_squad; // 0x04
    const vehicle_rail_definition* current_vrail_def; // 0x08
    farray<reinforcement_squad,12> chase_vehicle_array; // 0x0C
    farray<vrail_spawn_region_info,32> spawn_region_list; // 0x49C
    timestamp spawn_evaluation_period; // 0x8AC
    timestamp region_evaluation_period; // 0x8B0
    timestamp orders_evaluation_period; // 0x8B4
private:
    int chase_vehicle_count; // 0x8B8
};

// size 0xC
class vrail_patrol_data
{
public:
    vrail_patrol_data(); // 0x00768EA0 (.h:68)
    void reset(); // 0x007541B0 (.h:74)

    const object_patrol* patrol; // 0x00
    const object_squad* squad; // 0x04
    vehicle* vp; // 0x08
};
