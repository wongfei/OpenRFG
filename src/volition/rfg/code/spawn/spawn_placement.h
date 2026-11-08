// Original: volition/rfg/code/spawn/spawn_placement.h
// Implementation: volition/rfg/code/spawn/spawn_placement.cpp
//
// Spawn placement: finding where to put a requested NPC or vehicle (spawn nodes, nav cells or navpoints out of the
// player's view, road points, building interiors) and placing it (sp_do_spawn, sp_spawn_vehicle, putting vehicles on
// the ground, moving streamed vehicles and clearing the stream area).
#pragma once

#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/varray.h"

class cover_node;
class human_info;
struct nav_cell;
class navpoint;
class npc_spawn_param_list;
class object;
class object_action_node;
class object_guard_node;
class object_npc_spawn_node;
class object_path_road;
class vehicle;
class vehicle_spawn_params;

// size 0x8
struct spawn_point
{
    int object_index; // 0x00
    float weight; // 0x04
};

// size 0x20
struct nav_cell_search_node
{
    const nav_cell* m_nav_cell; // 0x00
    vector center_point; // 0x04
    float distance_sqrd; // 0x10
    int depth; // 0x14
    nav_cell_search_node* next; // 0x18
    nav_cell_search_node* prev; // 0x1C
};

// size 0x8
struct navpoint_evaluation_set
{
    const navpoint* navp; // 0x00
    float dist_sqrd; // 0x04
};

// size 0x78
struct road_search_info_placement
{
    road_search_info_placement(); // 0x00B43B70

    const object_path_road* prp; // 0x00
    vector original_pos; // 0x04
    matrix original_orient; // 0x10
    vector final_pos; // 0x34
    matrix final_orient; // 0x40
    float current_road_dist; // 0x64
    float min_road_dist; // 0x68
    float max_road_dist; // 0x6C
    float min_desired_dist_sqrd; // 0x70
    float max_desired_dist_sqrd; // 0x74
};

// size 0x2958
class building_child_list
{
public:
    building_child_list(); // 0x00B4FD40 (:281)
    void generate_lists(const object* new_objp); // 0x00B5A890 (:398)
    void generate_list_action_node(const object* new_objp); // 0x00B4FE00 (:377)
    void reset(); // 0x00B481E0 (:505)
    ~building_child_list(); // 0x00B50600

    farray<object_action_node const*,512> action_node_list; // 0x00
    farray<object_guard_node const*,512> guard_node_list; // 0x810
    farray<cover_node const*,512> cover_node_list; // 0x1020
    farray<object_npc_spawn_node const*,64> spawn_node_list; // 0x1830
    farray<navpoint_evaluation_set,512> nav_node_list; // 0x1940
private:
    const object* spawn_objp; // 0x2950
    bool list_built; // 0x2954
};

// size 0x14
class nav_cell_search_class
{
public:
    nav_cell_search_class(nav_cell_search_node* _node_pool, int _node_pool_size); // 0x00B481C0 (:324)
    void add_new_node(const nav_cell* new_nav_cell, const vector& spawn_point, int new_depth); // 0x00B48210 (:575)
    const nav_cell_search_node* get_next_searh_cell(); // 0x00B42F70 (:524)
    bool is_empty() const; // 0x00B42F30 (:333)
    int search_set_size() const; // 0x00B43020 (:553)
    void reset(); // 0x00B43040 (:566)
private:
    nav_cell_search_node* get_new_node(); // 0x00B43050 (:618)

    nav_cell_search_node* visited_list_head; // 0x00
    nav_cell_search_node* search_list_head; // 0x04
    int node_pool_size; // 0x08
    nav_cell_search_node* node_pool; // 0x0C
    int next_node; // 0x10
};

// size 0x14
class navcell_evaluation_set
{
public:
    navcell_evaluation_set(const vector& pos, const nav_cell* new_cell); // 0x00B42F40 (:369)
    navcell_evaluation_set();

    const nav_cell* cell; // 0x00
    vector center_point; // 0x04
    float distance_sqrd; // 0x10
};

bool find_nearest_spawn_node(const vector& pos, farray<object_npc_spawn_node const*,64>& spawn_node_list, const human_info* h_info, int& best_node_index); // 0x00B488B0 (:2222)
bool sp_do_spawn(npc_spawn_param_list& spawn_param_list); // 0x00B5CA50 (:3380)
bool sp_spawn_vehicle(vehicle_spawn_params& spawn_param); // 0x00B56420 (:3454)
bool sp_should_move_streamed_vehicle(const vehicle* vp); // 0x00B56E10 (:4096)
void sp_move_streamed_vehicle(vehicle* vp); // 0x00B56EA0 (:4138)
void sp_place_vehicle_on_ground(vehicle* vp); // 0x00B43B80 (:4163)
void sp_clean_stream_area(const vehicle* vp); // 0x00B56EE0 (:4171)
