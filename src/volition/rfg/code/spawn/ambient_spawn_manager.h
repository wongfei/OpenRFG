// Original: volition/rfg/code/spawn/ambient_spawn_manager.h
// Implementation: volition/rfg/code/spawn/ambient_spawn_manager.cpp
//
// The ambient spawn manager: keeps the world around the player populated with pedestrians (on ped node networks built
// from the zone navpoints) and traffic (on road splines), within the slots the spawn resource manager grants, and
// despawns what is far away or offscreen. amb_spawn_zone_load_complete is called by obj_zone::stream; gameplay_do_frame
// calls amb_spawn_do_frame once per frame.
#pragma once

#include "volition/rfg/code/gameplay/gameflow/gameflow_enums.h"
#include "volition/rfg/code/spawn/ambient_spawn_info.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/varray.h"

class ai_pedestrian_info;
class automobile;
class human_info;
class navpoint;
class npc;
class object_path_road;
class player;
class vehicle;
class vehicle_info;

enum ped_node_select_modes
{
    INVALID_PED_NODE_SELECT_MODE = -1,
    PED_NODE_SELECT_MODE_NORMAL = 0,
    PED_NODE_SELECT_MODE_DESPAWN = 1,
    PED_NODE_SELECT_MODE_RANDOM = 2,
    NUM_PED_NODE_SELECT_MODE = 3,
};

enum quadrant_locations
{
    QUADRANT_LOC_INVALID = -1,
    QUADRANT_LOC_FRONT = 0,
    QUADRANT_LOC_RIGHT_SIDE = 1,
    QUADRANT_LOC_LEFT_SIDE = 2,
    QUADRANT_LOC_REAR = 3,
    NUM_QUADRANT_LOC = 4,
};

// size 0x7C
class player_bounding_region
{
public:
    player_bounding_region(); // 0x00B47070 (:306)
    bool inside_min_radius_npc(const vector& point) const; // 0x00B41BF0 (:4318)
    bool inside_min_radius_vehicle(const vector& point) const; // 0x00B41E70 (:4380)
    bool outside_max_radius_npc(const vector& point) const; // 0x00B420F0 (:4443)
    bool outside_max_radius_vehicle(const vector& point) const; // 0x00B42380 (:4505)
    bool in_spawn_region_npc(const vector& point) const; // 0x00B42610 (:4568)
    bool in_spawn_region_vehicle(const vector& point) const; // 0x00B428F0 (:4630)
    void update_region(); // 0x00B47B60 (:4705)
    const vector& get_move_direction() const; // 0x00B42BD0 (:4693)
    float get_move_speed() const; // 0x00B42BE0 (:4699)
    void reset(); // 0x00B42BF0 (:4775)

private:
    int last_framecount; // 0x00
    vector pp_pos; // 0x04
    vector pp_move_direction; // 0x10
    float pp_move_speed; // 0x1C
    float min_velocity_ped_spawn_distance; // 0x20
    float max_velocity_ped_spawn_distance; // 0x24
    float min_velocity_ped_spawn_distance_sqrd; // 0x28
    float max_velocity_ped_spawn_distance_sqrd; // 0x2C
    float min_velocity_veh_spawn_distance; // 0x30
    float max_velocity_veh_spawn_distance; // 0x34
    float min_velocity_veh_spawn_distance_sqrd; // 0x38
    float max_velocity_veh_spawn_distance_sqrd; // 0x3C
    float farthest_ped_max_sqrd; // 0x40
    float farthest_veh_max_sqrd; // 0x44
    vector camera_pos; // 0x48
    matrix camera_orient; // 0x54
    bool ignore_move_direction; // 0x78
};

// size 0x2C
class ped_node_network
{
public:
    ped_node_network(); // 0x00B47080 (:369)
    void reset(); // 0x00B42CB0 (:4803)
    void get_node_list(farray<navpoint*,512>& node_list, unsigned int session_id, bool skip_parented_nodes); // 0x00B4E950 (:4867)
    void get_node_list(farray<navpoint*,512>& node_list); // 0x00B55B70 (:4851)
private:
    void visit_node(unsigned int session_id, navpoint* navp, farray<navpoint*,512>& node_list, bool skip_parented_nodes); // 0x00B4E8A0 (:4814)

public:
    bbox region; // 0x00
    unsigned int head_node; // 0x18
    vector head_node_pos; // 0x1C
    int zone_xy; // 0x28
};

// size 0x34
class ambient_spawn_camera_info
{
public:
    ambient_spawn_camera_info(); // 0x00B41020 (:402)

    vector camera_pos; // 0x00
    matrix camera_orient_xz; // 0x0C
    struct
    {
        bool props_built : 1; // 0x00:0
        bool moving_forward : 1; // 0x00:1
        bool moving_backward : 1; // 0x00:2
        bool moving_right : 1; // 0x00:3
        bool moving_left : 1; // 0x00:4
        bool is_moving : 1; // 0x00:5
        bool turning_left : 1; // 0x00:6
        bool turning_right : 1; // 0x00:7
        bool is_turning : 1; // 0x01:0
    } camera_movement_props; // 0x30
};

// size 0x142C
class ambient_ped_spawn_info : public ambient_spawn_camera_info
{
public:
    ambient_ped_spawn_info(); // 0x00B4C490 (:435)
    ~ambient_ped_spawn_info(); // 0x00B4E570

    const player* pp; // 0x34
    const ambient_spawn_info* amb_spawn_info; // 0x38
    farray<ped_node_network*,128> network_list; // 0x3C
    vector spawn_direction; // 0x24C
    vector despawn_direction; // 0x258
    int npcs_in_spawn_direction; // 0x264
    farray<navpoint*,512> node_list; // 0x268
    farray<navpoint*,512> spawn_node_list; // 0xA78
    navpoint* nearest_navp; // 0x1288
    farray<npc const*,100> npc_list; // 0x128C
};

// size 0x24
class road_spawn_info
{
public:
    road_spawn_info(); // 0x00B4C5A0 (:479)
    void reset(); // 0x00B470B0 (:484)
    ~road_spawn_info(); // 0x00B451E0

    object_path_road* road; // 0x00
    farray<unsigned short,8> index_list; // 0x04
};

// size 0x328
class ambient_veh_spawn_info
{
public:
    ambient_veh_spawn_info(); // 0x00B55020 (:509)
    void reset(); // 0x00B470C0 (:513)
    ~ambient_veh_spawn_info(); // 0x00B4E530

    vector camera_pos; // 0x00
    matrix camera_orient; // 0x0C
    const player* pp; // 0x30
    const ambient_spawn_info* amb_spawn_info; // 0x34
    vector move_direction; // 0x38
    float move_speed; // 0x44
    float minimap_radius; // 0x48
    const object_path_road* nearest_road; // 0x4C
    vector nearest_pos_on_road; // 0x50
    float dist_on_road; // 0x5C
    farray<vehicle const*,20> ambient_vehicle_list; // 0x60
    int vehicles_in_region; // 0xC0
    int vehicle_density; // 0xC4
    int quadrant_count[4]; // 0xC8
    farray<road_spawn_info,16> road_spawn_list; // 0xD8
};

// size 0x510
class node_list_cache
{
public:
    node_list_cache(); // 0x00B4C5C0 (:594)
    void reset(); // 0x00B47B10 (:4161)
    void update_node_cost(); // 0x00B59E00 (:4169)
    float get_travel_distance(unsigned int node_handle); // 0x00B4E850 (:4300)
private:
    void rebuild_network_cost(const navpoint* nearest_navp); // 0x00B4E5A0 (:4207)
public:
    ~node_list_cache(); // 0x00B47150

private:
    unsigned int head_node_handle; // 0x00
    data_hash_table<unsigned int,float,&uint_hash,128,unsigned char> node_to_cost_hash; // 0x04
    timestamp update_period; // 0x50C
};

automobile* find_farthest_offscreen_auto(); // 0x00B4D950 (:2836)
void do_frame_despawn_auto_aggressive(); // 0x00B4DDE0 (:3047)
void amb_spawn_system_init(); // 0x00B42CE0 (:4901)
void amb_spawn_level_init_pre(); // 0x00B59F80 (:4912)
void amb_spawn_do_frame(); // 0x00B5F800 (:4938)
void amb_spawn_zone_load_complete(unsigned short zone_gid); // 0x00B55BA0 (:4994)
bool ped_node_near_box(const bbox& area); // 0x00B5A060 (:5043)
const navpoint* ped_node_find_nearest(const vector& pos, float max_search_size); // 0x00B5F8D0 (:5053)
void ped_node_get_network_nodes(const bbox& area, farray<navpoint*,512>& node_list); // 0x00B5A0B0 (:5096)
void amb_spawn_level_init_post(); // 0x00B5FA70 (:5117)
void amb_spawn_level_init_spawn(); // 0x00B5FAD0 (:5138)
void amb_set_ambient_vehicle_spawn_disabled(bool disabled); // 0x00B42CF0 (:5223)
void amb_spawn_lock_ped_levels(gfm_alert_levels desired_alert_value, ambient_spawn_density desired_spawn_density); // 0x00B42D00 (:5229)
void amb_spawn_lock_veh_levels(gfm_alert_levels desired_alert_value, ambient_spawn_density desired_spawn_density); // 0x00B42D30 (:5243)
void amb_spawn_unlock_ambient_levels(); // 0x00B42D60 (:5257)
const navpoint* find_root_ped_node(const npc* np, const ai_pedestrian_info& ped_info); // 0x00B5A150 (:5274)
const navpoint* find_linked_navpoint(const navpoint* navp, const vector& desired_direction, const farray<unsigned int,8>& waypoint_list); // 0x00B4E9F0 (:5348)
bool build_new_waypoints_random(const npc* np, farray<unsigned int,8>& new_waypoint_list, const navpoint* root_navp, const ai_pedestrian_info& ped_info); // 0x00B4F520 (:5694)
bool build_new_waypoints_despawn(const npc* np, farray<unsigned int,8>& new_waypoint_list, const navpoint* root_navp, const ai_pedestrian_info& ped_info); // 0x00B4F5A0 (:5726)
bool amb_spawn_build_ped_waypoints(const npc* np, ai_pedestrian_info& ped_info, ped_node_select_modes desired_mode); // 0x00B5A4B0 (:5791)
const farray<human_info const*,16>& amb_spawn_desired_guerrilla_list(); // 0x00B42D80 (:5838)
const farray<vehicle_info const*,128>& amb_spawn_desired_vehicle_list(); // 0x00B42D90 (:5844)
float amb_spawn_get_navpoint_cost(unsigned int navpoint_handle); // 0x00B4F760 (:5851)
bool amb_spawn_in_no_spawn(unsigned int road_handle, float road_dist); // 0x00B47D80 (:5857)
