// Original: volition/rfg/code/pathfinding/nav_cell_detour.h
// Implementation: volition/rfg/code/pathfinding/nav_cell_detour.cpp
//
// Nav cell detours: movers (vehicles, debris, dynamic objects) standing on the nav mesh register detour requests whose
// 2D convex hulls are merged into compound hulls; paths and line-of-sight tests go around them. Requests are added
// and removed as movers are created, destroyed, stream in or out (obj_zone, object_mover).
#pragma once

#include <stdint.h>
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentityactivationlistener.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"

struct compound_hull;
class hkpEntity;
class human;
struct nav_cell;
class nav_cell_los_test_point;
class navpoint;
class obj_zone;
class object;
class object_action_node;
class object_guard_node;
class object_mover;

enum detour_path_types
{
    DETOUR_PATH_NONE = -1,
    DETOUR_PATH_CLOCKWISE = 0,
    DETOUR_PATH_COUNTER_CLOCKWISE = 1,
    NUM_DETOUR_PATH_TYPES = 2,
};

// size 0x18
struct compound_hull_vert
{
    compound_hull_vert(); // 0x00AED330

    unsigned short next; // 0x00
    unsigned short prev; // 0x02
    vector2 pos; // 0x04
    uint64_t nav_cell_handle; // 0x10
};

// size 0x5C
struct convex_hull_2d
{
    convex_hull_2d(); // 0x00AED320

    unsigned short next; // 0x00
    unsigned short prev; // 0x02
    struct
    {
        unsigned char high_priority : 1; // 0x00:0
        unsigned char from_destruction : 1; // 0x00:1
        unsigned char dislodged_mover : 1; // 0x00:2
    } flags; // 0x04
    unsigned char num_points; // 0x05
    signed char request_obj_type; // 0x06
    signed char request_obj_subtype; // 0x07
    vector2 points[6]; // 0x08
    vector bmin; // 0x38
    vector bmax; // 0x44
    compound_hull* cmp_hull; // 0x50
    vector2 jitter; // 0x54
};

// size 0x28
struct compound_hull
{
    compound_hull(); // 0x00AED340

    unsigned short next; // 0x00
    unsigned short prev; // 0x02
    unsigned int handle; // 0x04
    unsigned short points; // 0x08
    unsigned short convex_hulls; // 0x0A
    vector bmin; // 0x0C
    vector bmax; // 0x18
    unsigned short reference_count; // 0x24
    unsigned char convex_hull_count; // 0x26
    struct
    {
        unsigned char combinations_considered : 1; // 0x00:0
        unsigned char nav_cells_computed : 1; // 0x00:1
        unsigned char contains_vehicle : 1; // 0x00:2
        unsigned char contains_destruction : 1; // 0x00:3
        unsigned char contains_dislodged_mover : 1; // 0x00:4
    } flags; // 0x27
};

// size 0x10
struct compound_hull_ref
{
    unsigned short allocation_next; // 0x00
    unsigned short allocation_prev; // 0x02
    unsigned short references_next; // 0x04
    unsigned short references_prev; // 0x06
    nav_cell* ncp; // 0x08
    compound_hull* cmp_hull; // 0x0C
};

// size 0x20
struct nav_cell_detour_request
{
    nav_cell_detour_request(); // 0x00AED350

    unsigned short next; // 0x00
    unsigned short prev; // 0x02
    unsigned int request_handle; // 0x04
    unsigned int object_handle; // 0x08
    vector last_update_pos; // 0x0C
    unsigned short convex_hull_idx; // 0x18
    short component_index; // 0x1A
    unsigned char disabled_flags; // 0x1C
};

// size 0x24
struct convex_hull_intersection
{
    convex_hull_intersection(); // 0x00AED2C0 (:173)

    bool ix_valid; // 0x00
    vector2 ix_pos; // 0x04
    convex_hull_2d* hull0; // 0x0C
    convex_hull_2d* hull1; // 0x10
    int hull0_vert_indices[2]; // 0x14
    int hull1_vert_indices[2]; // 0x1C
};

// size 0x20
struct compound_hull_intersection
{
    compound_hull_intersection(); // 0x00AED2F0 (:194)

    bool ix_valid; // 0x00
    vector2 ix_pos; // 0x04
    compound_hull* hull0; // 0x0C
    compound_hull_vert* hull0_verts[2]; // 0x10
    compound_hull_vert* hull1_verts[2]; // 0x18
};

// size 0x4
class nav_cell_detour_activation_listener : public hkpEntityActivationListener
{
public:
    virtual void entityDeactivatedCallback(hkpEntity* entity); // vtbl+0x04 0x00AF9650 (:5546)
    virtual void entityActivatedCallback(hkpEntity* entity); // vtbl+0x08 0x00AF96A0 (:5551)
    nav_cell_detour_activation_listener(); // 0x00AF95F0
    virtual ~nav_cell_detour_activation_listener(); // vtbl+0x00 0x00AF13F0

};

bool ncd_ix_edge_edge(const vector2& edge0_vert0, const vector2& edge0_vert1, const vector2& edge1_vert0, const vector2& edge1_vert1, vector2* ix_pos); // 0x00AED360 (:418)
compound_hull* create_compound_hull_from_convex_hulls(bool& consumed_cpu, convex_hull_2d* start_hull); // 0x00B07850 (:4150)
void nav_cell_detour_level_init_pre(); // 0x00AF1410 (:5565)
void nav_cell_detour_do_frame(); // 0x00B1E940 (:5684)
void nav_cell_detour_load_zone(const obj_zone* zone); // 0x00AF2920 (:5733)
void nav_cell_detour_unload_zone(const obj_zone* zone); // 0x00B081C0 (:5751)
unsigned int nav_cell_detour_add_request(unsigned int obj_handle, short component_index); // 0x00AF2A20 (:5792)
void nav_cell_detour_set_dont_update(unsigned int request_handle); // 0x00AF2BB0 (:5873)
void nav_cell_detour_clear_dont_update(unsigned int request_handle); // 0x00AF2BC0 (:5884)
void nav_cell_detour_remove_request(unsigned int request_handle); // 0x00B190F0 (:5899)
bool nav_cell_detour_has_valid_convex_hull(unsigned int request_handle); // 0x00AF2BD0 (:5927)
void nav_cell_detour_remove_convex_hull_for_request(unsigned int request_handle); // 0x00B191E0 (:5942)
const compound_hull* nav_cell_detour_get_compound_hull(unsigned int handle); // 0x00AF2C00 (:5962)
const compound_hull_vert* nav_cell_detour_get_compound_hull_vert_from_index(const compound_hull* cmp_hull, int index); // 0x00AF2C30 (:5988)
int nav_cell_detour_get_compound_hull_vert_index(const compound_hull* cmp_hull, const compound_hull_vert* vert); // 0x00AF2C80 (:6013)
int nav_cell_detour_get_num_compound_hull_verts(const compound_hull* cmp_hull); // 0x00AF2CD0 (:6035)
bool nav_cell_detour_is_point_in_compound_hull(const vector2& pt, const compound_hull* cmp_hull); // 0x00AF2D10 (:6055)
bool nav_cell_detour_is_line_in_compound_hull(const vector& start_pt, const vector& end_pt, const compound_hull* cmp_hull); // 0x00AF96B0 (:6091)
bool nav_cell_detour_is_point_in_compound_hull(const vector& pt, const compound_hull* cmp_hull); // 0x00AF2E30 (:6112)
compound_hull* nav_cell_detour_is_point_in_compound_hull(const vector& pt, const nav_cell* ncp); // 0x00AF2E60 (:6125)
void nav_cell_detour_get_closest_point_on_compound_hull(vector2* closest_pt, int* closest_edge_index, const vector2& pos, const compound_hull* cmp_hull); // 0x00AF2EE0 (:6146)
void nav_cell_detour_get_closest_point_on_compound_hull(vector2* closest_pt, int* closest_edge_index, const vector& pos, const compound_hull* cmp_hull); // 0x00AF2FC0 (:6177)
bool nav_cell_detour_get_closest_compound_hull_intersection(vector* ix_pos, compound_hull** closest_cmp_hull, int* entry_edge_index, nav_cell* ncp, const vector& start_pos, const vector& end_pos); // 0x00B0BF80 (:6196)
bool nav_cell_detour_add_detour_points_to_path(human* hp, unsigned int flags, float check_dist); // 0x00B1E960 (:6253)
bool nav_cell_detour_test_line_of_sight(const nav_cell_los_test_point& start_point, const nav_cell_los_test_point& end_point); // 0x00B0C230 (:6637)
void nav_cell_detour_mover_created(object_mover* om); // 0x00AF9760 (:6725)
void nav_cell_detour_mover_destroyed(object_mover* om); // 0x00B19210 (:6735)
void nav_cell_detour_check_hull_against_current_path(human* hp, object* repulsor, unsigned int request_handle); // 0x00B1F090 (:6747)
bool nav_cell_detour_object_is_flat(object* obj); // 0x00AF3030 (:6855)
unsigned int nav_cell_detour_get_handle_from_object(const object* objp); // 0x00AF3080 (:6860)
void nav_cell_detour_add_impassable_nav_cells(human* hp, uint64_t ignore_handle_one, uint64_t ignore_handle_two); // 0x00B082C0 (:6882)
void nav_cell_detour_navpoint_removed(navpoint* navp); // 0x00AF30E0 (:6899)
void nav_cell_detour_action_node_removed(object_action_node* node); // 0x00AF30F0 (:6907)
void nav_cell_detour_guard_node_removed(object_guard_node* node); // 0x00AF3100 (:6915)
