// Original: volition/rfg/code/pathfinding/bb_pathfinding.cpp
//
// The pathfinding front end (stub_impl.md S9): nav mesh A* with raw and simplified paths, nodal paths through buildings,
// repulsors, dynamic holes in destroyed walls, road and vehicle paths. rfg.exe's "Int3 critical function" prints (the
// release build of its asserts) are left out: they only log.

#include "volition/rfg/code/pathfinding/bb_pathfinding.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "volition/rfg/code/ai/formation.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/self_preservation_actions.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/havok/havok_game_filter.h"
#include "volition/rfg/code/objects/movers/object_mover_lists.h"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/objects/object_guard_node.h"
#include "volition/rfg/code/scripting/action_script/global_event.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/spawn/ambient_spawn_manager.h"
#include "volition/rfg/code/traffic/traffic_shared.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle_ai/vehicle_navmesh_common.h"
#include "volition/rfg/code/vehicle_ai/vehicle_navmesh_path_create.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_move.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding_data.h"
#include "volition/rfg/code/pathfinding/nav_cell_detour.h"
#include "volition/rfg/code/pathfinding/nav_heap.h"
#include "volition/rfg/code/pathfinding/road_heap.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/plane.h"
#include "volition/vlib/code/math/spline.h"
#include "volition/vlib/code/memory/object_pool.h"

// Globals of other files, with no extern in their headers.
extern nav_heap Nav_heap; // nav_heap.cpp
extern navpoint_heap Navpoint_heap; // nav_heap.cpp
extern road_heap Road_heap; // road_heap.cpp
extern bool Navpoint_treat_all_as_enabled; // object_navpoint.cpp
extern float Proxy_mine_radius; // projectile.cpp
extern object_pool<rfg_destruction_event, 1000, 1> RFG_destruction_events; // rfg_dynamics.cpp

// Magic numbers: the navpoint types the paths tell apart (rfg.pdb has no enum for them).
static const signed char NAVPOINT_TYPE_PATH = 10;
static const signed char NAVPOINT_TYPE_TRANSITION = 11;
static const signed char NAVPOINT_TYPE_LADDER_TRANSITION = 12;
static const signed char NAVPOINT_TYPE_LADDER = 13;
static const signed char NAVPOINT_TYPE_STAIRS = 14;
static const signed char NAVPOINT_TYPE_TRANSITION_JUMP = 15;
static const signed char NAVPOINT_TYPE_LEAP_OVER = 16;
static const signed char NAVPOINT_TYPE_ROAD_BRIDGE = 22;
static const unsigned char NAVPOINT_DISABLED_BY_DETOUR = 4; // 1 << NPDR_BLOCKED

// ---- file-static globals
static const float Y_HEIGHT_LIMIT = 2.4f; // 0x0131B240
static bool Disable_vehicle_smoothing = true; // 0x016B000A
static float Rfg_pf_edf_squad_spread_factor = 500.0f; // 0x016B0178
static float PED_DEFAULT_PATH_CALC_DIST_MAX = 350.0f; // 0x016B017C
static float VEHICLE_DEFAULT_PATH_CALC_DIST_MAX = 500.0f; // 0x016B0180
static float PATH_CALC_DIST_BASE = 150.0f; // 0x016B0184
static float PATH_CALC_DIST_MAX_FACTOR = 1.5f; // 0x016B0188
static float EST_COST_TO_GOAL_MULTIPLIER = 0.75f; // 0x016B018C
int Player_num_nearby_repulsor_edges; // 0x035B607C
static bb_pfg_layer* Outer_edge_bsearch_level_data_key; // 0x035B6084
static nav_cell* Impassable_nav_cells[384]; // 0x035B9088
static nav_cell* Navmesh_closest_cell; // 0x035B968C
static bb_pfg_layer* Outer_edge_bsearch_level_data_candidate; // 0x035B9690
static float Navmesh_closest_dist_sq; // 0x035BB614
uint64_t Player_nav_cell_handle; // 0x035BB9C8
static path_node* Free_path_nodes; // 0x035BB9D0
static obj_zone* Neighbor_zone; // 0x035BBD5C
static obj_zone* Current_zone; // 0x035BBD60
static int Num_impassable_nav_cells; // 0x035BBD64
static bool Pf_use_turn_anims; // 0x035BBD68
vector Player_adjusted_pos; // 0x035BBD74
static path_node Path_nodes[1024]; // 0x035C1D80
static vector Navmesh_closest_point; // 0x035FD580
static vector Navmesh_test_point; // 0x035FD58C
static object_pool<dynamic_hole_request,100,1> Dynamic_hole_requests; // 0x0360DC20
repulsor_edge Player_nearby_repulsor_edges[16]; // 0x03610B20
static unsigned int $S5; // 0x03610D74

// ---- internal functions
static void navmesh_collect_callback(nav_cell* ncp); // 0x00B02920 (:501)
static void determine_turning_direction(const vector* turning_point, const vector* start_pos, const vector* end_pos, vehicle_path_info& path_info); // 0x00AEA050 (:530)
static float get_dist_squared_to_nav_cell(const nav_cell* ncp, const vector& pos); // 0x00B0C3D0 (:675)
static void maybe_update_closest_nav_cell(const nav_cell** closest_ncp, float* closest_dist_squared, const nav_cell* ncp, const vector& pos); // 0x00B0C4E0 (:705)
static const nav_cell* get_closest_adjacent_nav_cell(const nav_cell* ncp, const vector& pos, nav_cell_traversal_rule traversal_rule); // 0x00B0C580 (:773)
static bool ix_line_line_find_closest_distance(vector* pa, vector* pb, const vector& p1, const vector& p2, const vector& p3, const vector& p4, float* mua, float* mub); // 0x00AEA190 (:956)
static bool do_oriented_bounding_boxes_intersect(const vector& bbox_size0, const vector& bbox_pos0, const matrix& bbox_orient0, const vector& bbox_size1, const vector& bbox_pos1, const matrix& bbox_orient1); // 0x00AEA5D0 (:1005)
static int bsearch_outer_edges_callback(const void* elem0, const void* elem1); // 0x00B02A30 (:1025)
static void stitch_zone_single(obj_zone* zone, vector (*zone_bounding_boxes)[2], bool vehicle_pfg); // 0x00B08410 (:1111)
static void stitch_zone(obj_zone* zone); // 0x00B086D0 (:1238)
static void unstitch_zone_single(obj_zone* zone, bb_pfg_layer* neighboring_pfg); // 0x00B08940 (:1302)
static void unstitch_zone(obj_zone* zone); // 0x00B08990 (:1329)
bool search_for_nav_cell(nav_cell** closest_nav_cell, nav_cell* start_nav_cell, const vector& pos, nav_cell_traversal_rule traversal_rule); // 0x00B08AF0 (:1403)
static path_node* bb_pf_get_free_path_node(); // 0x00AEA720 (:1452)
static void build_raw_path(path_node** path, nav_cell* start_nav_cell, const vector& start_pos); // 0x00B0CDC0 (:1485)
static vector get_bridge_offset(const vector& start_pt, const vector& end_pt); // 0x00AEA7B0 (:1602)
static void build_raw_path(path_node** path, const nav_cell* start_nav_cell, const vector& start_pos, nav_heap* nhp); // 0x00B0D1F0 (:1670)
static bool optimize_node_position(vector* new_pos, path_node* node0, path_node* node1, path_node* node2, const vector& start_pos, const vector& end_pos, bool bias_towards_start, const edge_constraint_info& econstraint_info, unsigned int nav_cell_los_flags, bool found); // 0x00B13F20 (:1860)
static void add_out_of_chunk_path_segment(path_node** out_of_chunk_path, raw_path& r_path); // 0x00AF3EB0 (:2000)
static bool test_path_node_los(const path_node* start_node, const path_node* end_node, const pf_calc_flags& flags); // 0x00B14320 (:2022)
static path_node* get_path_node_from_offset(path_node* node, path_node* path, int n); // 0x00AEAAF0 (:2080)
static int optimize_node_positions(path_node* path, int num_nodes_to_optimize, unsigned int nav_cell_los_flags, bool constrain_edge_sliding, float max_optimize_dist); // 0x00B147B0 (:2351)
static void simplify_nav_path(path_node** path, const pf_calc_flags& flags, bool constrain_edge_sliding, float max_simplify_dist, int num_vehicle_nodes_to_optimize); // 0x00B14DD0 (:2530)
static void finalize_path(path_node** path); // 0x00AF3F00 (:2567)
static float get_max_search_distance(const nav_cell* start_nav_cell, const vector& start_pos, const nav_cell* end_nav_cell, const vector& end_pos, bool vehicle_pfg, bool scripted_move); // 0x00B02D00 (:2662)
static void build_out_of_chunk_path(raw_path& output, const vector& start_pos, const vector& end_pos); // 0x00AEAB30 (:2735)
static bool is_in_allowed_set(const navpoint* navp, const signed char* allowed_types, const int num_allowed_types); // 0x00AEAC00 (:2756)
static const navpoint* find_nearest_exit_navpoint(const vector& pos, const farray<navpoint const*,32>& point_list); // 0x00AF4200 (:2770)
static const navpoint* find_best_exit_trans_point(navpoint* starting_navp, const farray<navpoint const*,32>& point_list, const signed char* allowed_types, const int num_allowed_types, bool use_doors); // 0x00B14E60 (:2793)
static navpoint* rfg_pf_find_best_transition_node(const vector& dest_pos, navpoint* starting_point, float& cost, bool allow_jumps, bool use_doors, unsigned int node_to_avoid); // 0x00B15180 (:2878)
static void query_mover_for_path(navpoint* navp, nav_cell* arrival_cell, const vector& arrival_pos, float arrival_cost, const vector& goal_pos, nav_heap* nhp, bool use_doors); // 0x00B15570 (:2979)
static float rfg_pf_edf_spread_traversal_boost(const nav_cell* ncp); // 0x00B08BF0 (:3044)
static bool get_raw_path_internal(raw_path& output, nav_heap* nhp, float max_search_dist, unsigned short ignore_set, bool encourage_edf_spread, bool use_doors, unsigned int node_to_avoid); // 0x00B156B0 (:3067)
static void get_raw_path(raw_path& output, const vector& start_pos, const vector& end_pos, uint64_t start_nav_cell_hint_handle, uint64_t end_nav_cell_hint_handle, bool vehicle_pfg, bool out_of_chunk, unsigned short ignore_set, bool use_doors, bool encourage_edf_spread, bool scripted_move, unsigned int node_to_avoid); // 0x00B19260 (:3182)
static void get_raw_path(raw_path& output, const pf_calc_params& params); // 0x00B19630 (:3311)
static path_node* get_path(const vector* points, int num_points, const uint64_t* nav_cell_hints, bool vehicle_pfg, bool out_of_chunk, unsigned short ignore_set, bool stuck, bool use_doors, bool encourage_edf_spread, bool scripted_move, unsigned int node_to_avoid); // 0x00B1A680 (:3786)
static path_node* get_path(const vector& start_pos, const vector& end_pos, uint64_t start_nav_cell_hint_handle, uint64_t end_nav_cell_hint_handle, bool vehicle_pfg, bool out_of_chunk, unsigned short ignore_set, bool stuck, bool use_doors, bool encourage_edf_spread, bool scripted_move, unsigned int node_to_avoid); // 0x00B1AA40 (:3946)
static int get_nearby_repulsor_edges(repulsor_edge* repulsor_edges, nav_cell* start_nav_cell, const vector& pos, float max_dist); // 0x00B02FA0 (:3964)
static bool maybe_ignore_repulsor(human* hp, const vector& pos, const vector& dest, object* repulsor, const repulsor_oriented_bbox& oriented_bbox, bool recollected_repulsors); // 0x00AF4290 (:4052)
static bool maybe_merge_repulsors(const repulsor_oriented_bbox& oriented_bbox0, const repulsor_oriented_bbox& oriented_bbox1); // 0x00AEAC30 (:4180)
static void merge_bounding_spheres(vector& sphere_pos0, float& sphere_radius0, const vector& sphere_pos1, float sphere_radius1); // 0x00AEACF0 (:4221)
static void calculate_compound_repulsor_bounding_sphere(compound_repulsor* repulsor, const human* hp); // 0x00AFA180 (:4251)
static int create_compound_repulsors(compound_repulsor* compound_repulsors, human* hp, const vector& pos, const vector& dest, int num_repulsor_handles, unsigned int* repulsor_handles, bool recollected_repulsors); // 0x00AFA570 (:4290)
static bool maybe_expand_compound_repulsor_to_include_repulsor_edge(compound_repulsor* repulsor, const repulsor_edge* r_edge); // 0x00AEAFB0 (:4599)
static void maybe_expand_player_repulsor(int num_compound_repulsors, compound_repulsor* compound_repulsors); // 0x00AEB0E0 (:4628)
static bool calculate_repulsion_vector(vector* repulsion_vector, human* hp, const vector& pos, compound_repulsor* repulsor, const vector& dir_to_dest_xz); // 0x00AEB290 (:4669)
static raycast_output::hit_info* get_best_navmesh_spherecast_hit(const vector& pos, raycast_output& out, bool vehicle_pfg, bool force_to_cell, float& best_dist_sqr, vector* best_navcell_pos, nav_cell** best_ncp); // 0x00B0D8F0 (:5186)
static raycast_output::hit_info* get_best_navmesh_spherecast_hit(const vector& pos, raycast_output& out_down, raycast_output& out_up, bool vehicle_pfg, bool force_to_cell, vector* adjusted_pos, nav_cell** best_ncp); // 0x00B0DA30 (:5253)
static path_node* rfg_pf_find_path_from_navmesh_to_nodal(human* hp, const vector& start_pos, const vector& end_pos, const nav_cell* start_nav_cell, navpoint* end_navp, unsigned short ignore_set); // 0x00B1AAE0 (:6307)
static path_node* rfg_pf_find_path_between_nodes_of_same_set(human* hp, const vector& start_pos, const vector& end_pos, navpoint* start_navp, navpoint* end_navp); // 0x00AFAAF0 (:6452)
static path_node* rfg_pf_find_path_from_nodal_to_navmesh(human* hp, const vector& start_pos, const vector& end_pos, navpoint* start_navp, const nav_cell* end_nav_cell); // 0x00B1AEC0 (:6998)
static path_node* rfg_pf_find_path_between_nodes(human* hp, const vector& start_pos, const vector& end_pos, navpoint* start_navp, navpoint* end_navp); // 0x00B15AB0 (:7104)
static path_node* rfg_pf_find_path_from_null_area(human* hp, const vector& start_pos, const vector& end_pos, const nav_cell* end_nav_cell, bool out_of_chunk); // 0x00B08E30 (:7233)
static bool collect_only_hole_navpoints(object* hit_objp, unsigned int shape_key, const void* user_data); // 0x00AF4720 (:8138)
static bool rfg_pf_repulsor_collector(object* repulsor, unsigned int shape_key, const void* user_data); // 0x00AF4750 (:8150)
static float rfg_pf_get_projectile_repulsor_radius(const projectile* proj); // 0x00AEBA30 (:8772)
static void remove_dynamic_hole_request(dynamic_hole_request* request); // 0x00B03730 (:9458)
static dynamic_hole_request* get_free_dynamic_hole_request(); // 0x00B03760 (:9469)
static bool find_closest_point_on_link_from_navpoint_or_in_navpoint(const vector hit_pos, const navpoint* current_navpoint, navpoint*& closest_navpoint, vector& closest_point, float& closest_dist); // 0x00AEBD70 (:9531)
static navpoint* create_navpoints_for_hole(const vector hit_pos, const vector closest_point, navpoint* closest_navpoint, navpoint* second_navpoint, const float closest_dist, bool is_wall, float new_radius); // 0x00AF5450 (:9592)
static void handle_hole_in_floor(navpoint* new_node, nav_cell* closest_nav_cell, vector navmesh_pos); // 0x00B0F4B0 (:9786)
static void handle_hole_in_interior_wall(navpoint* new_node, matrix orient_of_wall); // 0x00B03970 (:9942)
static void dynamic_hole_request_process_bridge_event(dynamic_hole_request* request); // 0x00AFC7B0 (:10138)
static void dynamic_hole_request_process_destruction_event(dynamic_hole_request* request); // 0x00B0FAB0 (:10328)
static void check_nav_links_against_destruction_event(rfg_destruction_event* devp); // 0x00B04190 (:11358)
static void simplify_vehicle_nav_path(path_node** path, const pf_calc_flags& flags); // 0x00B16E80 (:11613)
static void bb_pf_simplify_vehicle_raw_path(path_node** std_path, const pf_calc_flags& flags); // 0x00B16FF0 (:11669)
static void bb_pf_get_vehicle_path_core(raw_path& r_path, pf_calc_params& seg_params); // 0x00B1BF30 (:11674)
static float find_u_value_for_dist_along_spline_from_start_value(cubic_bezier_spline* cbs, float total_length, float starting_u_value, float ending_u_value, float advance_dist); // 0x00AEC0C0 (:11962)
static navmesh_lane_spline_node* generate_intersection_curve(navmesh_lane_spline_node* last_node, navmesh_lane_spline_node* temp_node, cubic_bezier_spline* last_spline, cubic_bezier_spline* cbs, float dot_product); // 0x00AFD060 (:12010)


// ---- implementation

// Magic numbers: rfg.exe's tolerances for vertices of neighbouring zones, and how far a zone's box reaches for stitching.
static const float ZONE_VERT_MATCH_XZ = 0.001f;
static const float ZONE_VERT_MATCH_Y = 0.05f;
static const float OUTER_EDGE_MATCH = 0.05f;
static const float ZONE_STITCH_BOX_GROW = 2.5f;
// Magic number: 3 x 3 zones around the player, the stack array of stitch_zone in rfg.exe.
static const int MAX_STITCH_ZONES = 9;

static bool bb_pf_cell_traversable(const nav_cell* ncp, nav_cell_traversal_rule rule)
{
    return rule == NAV_CELL_TRAVERSAL_ALL
        || (rule == NAV_CELL_TRAVERSAL_ENABLED_PLUS_LIGHT && (ncp->flags & NAV_CELL_FLAG_LIGHT) != 0)
        || (ncp->flags & NAV_CELL_FLAG_DISABLED) == 0;
}

// 0x00B02920 (:501)
static void navmesh_collect_callback(nav_cell* ncp)
{
    vector verts[3];
    nav_cell_get_all_verts(ncp, verts[0], verts[1], verts[2]);
    if ((ncp->flags & (NAV_CELL_FLAG_DISABLED | NAV_CELL_FLAG_LIGHT)) != 0) {
        return;
    }
    for (int i = 0; i < 3; i++) {
        vector closest;
        ix_closest_point_on_line_segment(closest, Navmesh_test_point, verts[i], verts[(i + 1) % 3]);
        float dist_sq = vec_dist_squared(Navmesh_test_point, closest);
        if (dist_sq < Navmesh_closest_dist_sq) {
            Navmesh_closest_cell = ncp;
            Navmesh_closest_point = closest;
            Navmesh_closest_dist_sq = dist_sq;
        }
    }
}

// 0x00AEA050 (:530) LTCG: turning_point EAX, start_pos ESI, end_pos EDI, path_info on the stack
static void determine_turning_direction(const vector* turning_point, const vector* start_pos, const vector* end_pos, vehicle_path_info& path_info)
{
    vector to_turn = *start_pos - *turning_point;
    if (!to_turn.is_null()) {
        vector from_turn = *end_pos - *start_pos;
        if (!from_turn.is_null()) {
            // Original bug kept: the turning direction bit is only ever set here, never cleared.
            if (0.0f < from_turn.x * to_turn.z + 0.0f + -to_turn.x * from_turn.z) {
                path_info.flags.ixc_turning_right = 1;
            }
            return;
        }
    }
    path_info.flags.ixc_turning = 0;
}

// 0x00B0C3D0 (:675) LTCG: ncp EDI, pos on the stack
static float get_dist_squared_to_nav_cell(const nav_cell* ncp, const vector& pos)
{
    if (nav_cell_is_point_in_cell_column(ncp, pos)) {
        return 0.0f;
    }
    float best = -1.0f;
    for (int edge = 0; edge < 3; edge++) {
        const vector& vert0 = *nav_cell_get_vertex(ncp, edge, nullptr);
        const vector& vert1 = *nav_cell_get_vertex(ncp, (edge + 1) % 3, nullptr);
        float dist_sq = vec_find_closest_dist_squared_to_linesegment(pos, vert0, vert1, nullptr);
        if (dist_sq < best || best < 0.0f) {
            best = dist_sq;
        }
    }
    return best;
}

// 0x00B0C4E0 (:705) LTCG: ncp EAX, pos ESI
static void maybe_update_closest_nav_cell(const nav_cell** closest_ncp, float* closest_dist_squared, const nav_cell* ncp, const vector& pos)
{
    float dist_sq = get_dist_squared_to_nav_cell(ncp, pos);
    if (dist_sq > *closest_dist_squared) {
        return;
    }
    if (*closest_dist_squared <= dist_sq) {
        // Equally close: the nearer centroid wins.
        vector closest_centroid, centroid;
        nav_cell_get_centroid(&closest_centroid, *closest_ncp);
        nav_cell_get_centroid(&centroid, ncp);
        if (vec_dist_squared(pos, closest_centroid) <= vec_dist_squared(pos, centroid)) {
            return;
        }
    }
    *closest_dist_squared = dist_sq;
    *closest_ncp = ncp;
}

// The vertex of ncp that is vert, exactly or within the tolerance across zones; 3 when there is none.
static int bb_pf_find_shared_vertex(const nav_cell* ncp, const vector* vert)
{
    for (int i = 0; i < 3; i++) {
        const vector* other = nav_cell_get_vertex(ncp, i, nullptr);
        if (other->x == vert->x && other->y == vert->y && other->z == vert->z) {
            return i;
        }
    }
    for (int i = 0; i < 3; i++) {
        const vector* other = nav_cell_get_vertex(ncp, i, nullptr);
        if (fabsf(other->x - vert->x) < ZONE_VERT_MATCH_XZ && fabsf(other->y - vert->y) < ZONE_VERT_MATCH_Y && fabsf(other->z - vert->z) < ZONE_VERT_MATCH_XZ) {
            return i;
        }
    }
    return 3;
}

// 0x00B0C580 (:773) LTCG: pos on the stack first, ncp EDI
static const nav_cell* get_closest_adjacent_nav_cell(const nav_cell* ncp, const vector& pos, nav_cell_traversal_rule traversal_rule)
{
    const nav_cell* best = ncp;
    float best_dist_sq = get_dist_squared_to_nav_cell(ncp, pos);
    // Around each vertex of the cell: one way until a border, then the other way.
    for (int vertex = 0; vertex < 3; vertex++) {
        const vector* vert = nav_cell_get_vertex(ncp, vertex, nullptr);
        const nav_cell* cell = ncp->adjacent_cells[vertex].to_pointer();
        while (cell != nullptr && cell != ncp) {
            if (!bb_pf_cell_traversable(cell, traversal_rule)) {
                break;
            }
            if (cell != best) {
                maybe_update_closest_nav_cell(&best, &best_dist_sq, cell, pos);
            }
            int shared = bb_pf_find_shared_vertex(cell, vert);
            if (shared > 2) {
                return best;
            }
            cell = cell->adjacent_cells[shared].to_pointer();
        }
        if (cell == ncp) {
            continue;
        }
        cell = ncp->adjacent_cells[(vertex + 2) % 3].to_pointer();
        while (cell != nullptr && cell != ncp && bb_pf_cell_traversable(cell, traversal_rule)) {
            if (cell != best) {
                maybe_update_closest_nav_cell(&best, &best_dist_sq, cell, pos);
            }
            int shared = bb_pf_find_shared_vertex(cell, vert);
            if (shared > 2) {
                return best;
            }
            cell = cell->adjacent_cells[(shared + 2) % 3].to_pointer();
        }
    }
    return best;
}

// 0x00AFA0E0 (:886)
void rfg_pf_offsets_to_pointer(bb_pfg_layer* pfg)
{
    // The file stores offsets from the start of the layer; et_ptr_offset is relative to the field itself.
    if (pfg->vertices.get_offset() != -1) {
        pfg->vertices.set_offset(pfg->vertices.get_offset() - offsetof(bb_pfg_layer, vertices));
    }
    if (pfg->nav_cells.get_offset() != -1) {
        pfg->nav_cells.set_offset(pfg->nav_cells.get_offset() - offsetof(bb_pfg_layer, nav_cells));
    }
    if (pfg->nav_cell_heap.get_offset() != -1) {
        pfg->nav_cell_heap.set_offset(pfg->nav_cell_heap.get_offset() - offsetof(bb_pfg_layer, nav_cell_heap));
    }
    if (pfg->nav_cell_outer_edges.get_offset() != -1) {
        pfg->nav_cell_outer_edges.set_offset(pfg->nav_cell_outer_edges.get_offset() - offsetof(bb_pfg_layer, nav_cell_outer_edges));
    }
    nav_cell* cells = pfg->nav_cells.to_pointer();
    for (int i = 0; i < pfg->num_nav_cells; i++) {
        nav_cell& cell = cells[i];
        cell.navp = 0xFFFFFFFF;
        for (int edge = 0; edge < 3; edge++) {
            int offset = cell.adjacent_cells[edge].get_offset();
            if (offset != -1) {
                cell.adjacent_cells[edge].set_offset(offset - (int)((char*)&cell.adjacent_cells[edge] - (char*)pfg));
            }
        }
    }
}

// 0x00AEA190 (:956) LTCG: p3 EDX, p4 EAX
static bool ix_line_line_find_closest_distance(vector* pa, vector* pb, const vector& p1, const vector& p2, const vector& p3, const vector& p4, float* mua, float* mub)
{
    float local_mua, local_mub;
    float* out_mua = mua != nullptr ? mua : &local_mua;
    float* out_mub = mub != nullptr ? mub : &local_mub;
    float d43x = p4.x - p3.x;
    float d43y = p4.y - p3.y;
    float d43z = p4.z - p3.z;
    float d21x = p2.x - p1.x;
    float d21y = p2.y - p1.y;
    float d21z = p2.z - p1.z;
    float d13x = p1.x - p3.x;
    float d13y = p1.y - p3.y;
    float d13z = p1.z - p3.z;
    float d1343 = d13x * d43x + d13y * d43y + d13z * d43z;
    float d4321 = d21x * d43x + d21y * d43y + d21z * d43z;
    float d4343 = d43y * d43y + d43x * d43x + d43z * d43z;
    float denominator = (d21y * d21y + d21x * d21x + d21z * d21z) * d4343 - d4321 * d4321;
    if (denominator == 0.0f) {
        return false;
    }
    *out_mua = (d4321 * d1343 - (d13x * d21x + d13y * d21y + d13z * d21z) * d4343) / denominator;
    *out_mub = (*out_mua * d4321 + d1343) / d4343;
    pa->x = p1.x + d21x * *out_mua;
    pa->y = p1.y + d21y * *out_mua;
    pa->z = d21z * *out_mua + p1.z;
    pb->x = p3.x + d43x * *out_mub;
    pb->y = p3.y + *out_mub * d43y;
    pb->z = d43z * *out_mub + p3.z;
    return true;
}

// 0x00AEA5D0 (:1005) LTCG: bbox_orient1 ECX, bbox_pos1 ESI, bbox_size1 EDI
static bool do_oriented_bounding_boxes_intersect(const vector& bbox_size0, const vector& bbox_pos0, const matrix& bbox_orient0, const vector& bbox_size1, const vector& bbox_pos1, const matrix& bbox_orient1)
{
    matrix transpose = bbox_orient1.copy_transpose();
    matrix relative = bbox_orient0 * transpose;
    vector delta = bbox_pos1 - bbox_pos0;
    vector local_delta = delta.copy_rotate(bbox_orient0);
    vector half0(bbox_size0.x * 0.5f, bbox_size0.y * 0.5f, bbox_size0.z * 0.5f);
    vector half1(bbox_size1.x * 0.5f, bbox_size1.y * 0.5f, bbox_size1.z * 0.5f);
    return ix_box_box(relative, local_delta, half0, half1) == 0;
}

// 0x00B02A30 (:1025)
static int bsearch_outer_edges_callback(const void* elem0, const void* elem1)
{
    const nav_cell_outer_edge* edge0 = (const nav_cell_outer_edge*)elem0;
    const nav_cell_outer_edge* edge1 = (const nav_cell_outer_edge*)elem1;
    const nav_cell* cell0 = Outer_edge_bsearch_level_data_key->nav_cells.to_pointer() + edge0->nav_cell_index;
    const nav_cell* cell1 = Outer_edge_bsearch_level_data_candidate->nav_cells.to_pointer() + edge1->nav_cell_index;
    const vector* a0 = nav_cell_get_vertex(cell0, edge0->edge_index, Current_zone);
    const vector* a1 = nav_cell_get_vertex(cell0, (edge0->edge_index + 1) % 3, Current_zone);
    const vector* b0 = nav_cell_get_vertex(cell1, edge1->edge_index, Neighbor_zone);
    const vector* b1 = nav_cell_get_vertex(cell1, (edge1->edge_index + 1) % 3, Neighbor_zone);
    // The shared edge runs the other way in the neighbour.
    if (a1->x - b0->x < -OUTER_EDGE_MATCH) {
        return -1;
    }
    if (a1->x - b0->x > OUTER_EDGE_MATCH) {
        return 1;
    }
    if (a1->z - b0->z < -OUTER_EDGE_MATCH) {
        return -1;
    }
    if (a1->z - b0->z > OUTER_EDGE_MATCH) {
        return 1;
    }
    if (a1->y - b0->y < -OUTER_EDGE_MATCH) {
        return -1;
    }
    if (a1->y - b0->y > OUTER_EDGE_MATCH) {
        return 1;
    }
    if (a0->x - b1->x < -OUTER_EDGE_MATCH) {
        return -1;
    }
    if (a0->x - b1->x > OUTER_EDGE_MATCH) {
        return 1;
    }
    if (a0->z - b1->z < -OUTER_EDGE_MATCH) {
        return -1;
    }
    if (a0->z - b1->z > OUTER_EDGE_MATCH) {
        return 1;
    }
    if (a0->y - b1->y < -OUTER_EDGE_MATCH) {
        return -1;
    }
    if (a0->y - b1->y > OUTER_EDGE_MATCH) {
        return 1;
    }
    return 0;
}

// 0x00B08410 (:1111)
static void stitch_zone_single(obj_zone* zone, vector (*zone_bounding_boxes)[2], bool vehicle_pfg)
{
    bb_pfg_layer* layer = vehicle_pfg ? zone->zone_pfg.vehicle_pfg : zone->zone_pfg.pfg;
    nav_cell_outer_edge* edges = layer->nav_cell_outer_edges.to_pointer();
    Outer_edge_bsearch_level_data_key = layer;
    int num_zones = World.get_num_objects<obj_zone>();
    for (int e = 0; e < layer->num_nav_cell_outer_edges; e++) {
        nav_cell_outer_edge& edge = edges[e];
        nav_cell* cell = layer->nav_cells.to_pointer() + edge.nav_cell_index;
        vector* vertices = ((cell->flags & NAV_CELL_FLAG_VEHICLE) != 0 ? zone->zone_pfg.vehicle_pfg : zone->zone_pfg.pfg)->vertices.to_pointer();
        vector* vert0 = vertices + cell->vertices[edge.edge_index];
        vector* vert1 = vertices + cell->vertices[(edge.edge_index + 1) % 3];
        Current_zone = zone;
        for (int z = 0; z < num_zones; z++) {
            obj_zone* other = World.get_object<obj_zone>(z);
            const vector* box = zone_bounding_boxes[z];
            if (other == zone || !ix_point_in_box(*vert0, box[0], box[1]) || !ix_point_in_box(*vert1, box[0], box[1])) {
                continue;
            }
            bb_pfg_layer* other_layer = vehicle_pfg ? other->zone_pfg.vehicle_pfg : other->zone_pfg.pfg;
            Outer_edge_bsearch_level_data_candidate = other_layer;
            Neighbor_zone = other;
            nav_cell_outer_edge* found = (nav_cell_outer_edge*)bsearch(&edge, other_layer->nav_cell_outer_edges.to_pointer(), other_layer->num_nav_cell_outer_edges, sizeof(nav_cell_outer_edge), bsearch_outer_edges_callback);
            if (found != nullptr) {
                nav_cell* other_cell = other_layer->nav_cells.to_pointer() + found->nav_cell_index;
                cell->adjacent_cells[edge.edge_index].from_pointer(other_cell);
                edge.adjacent_zone = other->get_handle();
                other_cell->adjacent_cells[found->edge_index].from_pointer(cell);
                found->adjacent_zone = zone->get_handle();
                // The two zones' copies of the edge's vertices become exactly equal.
                nav_cell_get_edge_verts(vert1, vert0, other_cell, found->edge_index, other);
                break;
            }
        }
    }
}

// 0x00B086D0 (:1238)
static void stitch_zone(obj_zone* zone)
{
    vector boxes[MAX_STITCH_ZONES][2];
    int num_zones = World.get_num_objects<obj_zone>();
    for (int pass = 0; pass < 2; pass++) {
        for (int z = 0; z < num_zones; z++) {
            obj_zone* other = World.get_object<obj_zone>(z);
            const bb_pfg_layer* layer = pass == 0 ? other->zone_pfg.pfg : other->zone_pfg.vehicle_pfg;
            if (layer == nullptr) {
                boxes[z][0].zero();
                boxes[z][1].zero();
            } else {
                boxes[z][0] = vector(layer->bbox_min.x - ZONE_STITCH_BOX_GROW, layer->bbox_min.y - ZONE_STITCH_BOX_GROW, layer->bbox_min.z - ZONE_STITCH_BOX_GROW);
                boxes[z][1] = vector(layer->bbox_max.x + ZONE_STITCH_BOX_GROW, layer->bbox_max.y + ZONE_STITCH_BOX_GROW, layer->bbox_max.z + ZONE_STITCH_BOX_GROW);
            }
        }
        stitch_zone_single(zone, boxes, pass != 0);
    }
}

// 0x00B08940 (:1302) LTCG: zone EAX, neighboring_pfg ECX
static void unstitch_zone_single(obj_zone* zone, bb_pfg_layer* neighboring_pfg)
{
    unsigned int handle = zone->get_handle();
    nav_cell_outer_edge* edges = neighboring_pfg->nav_cell_outer_edges.to_pointer();
    for (int e = 0; e < neighboring_pfg->num_nav_cell_outer_edges; e++) {
        if (edges[e].adjacent_zone == handle) {
            neighboring_pfg->nav_cells.to_pointer()[edges[e].nav_cell_index].adjacent_cells[edges[e].edge_index].set_offset(-1);
            edges[e].adjacent_zone = 0xFFFFFFFF;
        }
    }
}

static void unstitch_layer_edges(bb_pfg_layer* layer)
{
    if (layer == nullptr) {
        return;
    }
    nav_cell_outer_edge* edges = layer->nav_cell_outer_edges.to_pointer();
    for (int e = 0; e < layer->num_nav_cell_outer_edges; e++) {
        nav_cell& cell = layer->nav_cells.to_pointer()[edges[e].nav_cell_index];
        if (cell.adjacent_cells[edges[e].edge_index].to_pointer() != nullptr) {
            cell.adjacent_cells[edges[e].edge_index].set_offset(-1);
            edges[e].adjacent_zone = 0xFFFFFFFF;
        }
    }
}

// 0x00B08990 (:1329)
static void unstitch_zone(obj_zone* zone)
{
    int num_zones = World.get_num_objects<obj_zone>();
    for (int z = 0; z < num_zones; z++) {
        obj_zone* other = World.get_object<obj_zone>(z);
        if (other != zone) {
            if (other->zone_pfg.pfg != nullptr) {
                unstitch_zone_single(zone, other->zone_pfg.pfg);
            }
            if (other->zone_pfg.vehicle_pfg != nullptr) {
                unstitch_zone_single(zone, other->zone_pfg.vehicle_pfg);
            }
        }
    }
    unstitch_layer_edges(zone->zone_pfg.pfg);
    unstitch_layer_edges(zone->zone_pfg.vehicle_pfg);
}

// 0x00B08AF0 (:1403) LTCG: start_nav_cell EAX. nav_cell.cpp calls this too, so it is not static here.
bool search_for_nav_cell(nav_cell** closest_nav_cell, nav_cell* start_nav_cell, const vector& pos, nav_cell_traversal_rule traversal_rule)
{
    *closest_nav_cell = nullptr;
    nav_cell* previous = start_nav_cell;
    nav_cell* cell = start_nav_cell;
    vector2 pos_2d(pos.x, pos.z);
    for (;;) {
        int outside_edge = -1;
        nav_cell* next = nullptr;
        for (int edge = 0; edge < 3 && next == nullptr; edge++) {
            if (nav_cell_classify_point_vs_edge(cell, edge, pos_2d) == POINT_CLASS_LEFT) {
                outside_edge = edge;
                nav_cell* adjacent = cell->adjacent_cells[edge].to_pointer();
                if (adjacent != nullptr && adjacent != previous && bb_pf_cell_traversable(adjacent, traversal_rule)) {
                    next = adjacent;
                }
            }
        }
        if (next == nullptr) {
            *closest_nav_cell = cell;
            return outside_edge < 0;
        }
        previous = cell;
        cell = next;
    }
}

// 0x00AEA720 (:1452)
static path_node* bb_pf_get_free_path_node()
{
    path_node* node = Free_path_nodes;
    if (node == nullptr) {
        return nullptr;
    }
    node->pos = ZERO_VECTOR;
    node->nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
    node->navpoint_handle = 0xFFFFFFFF;
    node->normal.zero();
    node->compound_hull_handle = 0xFFFFFFFF;
    node->compound_hull_vert_index = -1;
    memset(&node->flags, 0, sizeof(node->flags));
    if (Free_path_nodes == node && (Free_path_nodes = node->next, Free_path_nodes == node)) {
        node->next = nullptr;
        node->prev = nullptr;
        Free_path_nodes = nullptr;
        return node;
    }
    if (node->prev != nullptr) {
        node->prev->next = node->next;
    }
    if (node->next != nullptr) {
        node->next->prev = node->prev;
    }
    node->next = nullptr;
    node->prev = nullptr;
    return node;
}

// Paths are circular lists: head->prev is the tail. rfg.exe inlines these list operations everywhere.
static void bb_pf_path_append(path_node** path, path_node* node)
{
    if (*path == nullptr) {
        node->next = node;
        node->prev = node;
        *path = node;
    } else {
        node->prev = (*path)->prev;
        node->next = *path;
        (*path)->prev->next = node;
        (*path)->prev = node;
    }
}

static void bb_pf_path_unlink(path_node** path, path_node* node)
{
    if (*path == node && node->next == node) {
        node->next = nullptr;
        node->prev = nullptr;
        *path = nullptr;
        return;
    }
    if (*path == node) {
        *path = node->next;
    }
    if (node->prev != nullptr) {
        node->prev->next = node->next;
    }
    if (node->next != nullptr) {
        node->next->prev = node->prev;
    }
    node->next = nullptr;
    node->prev = nullptr;
}

// Appends the circular list other to the circular list at path.
static void bb_pf_path_splice(path_node** path, path_node* other)
{
    if (other == nullptr) {
        return;
    }
    if (*path == nullptr) {
        *path = other;
        return;
    }
    path_node* tail = (*path)->prev;
    path_node* other_tail = other->prev;
    tail->next = other;
    other_tail->next = *path;
    other->prev = tail;
    (*path)->prev = other_tail;
}

static path_node* bb_pf_path_next(const path_node* node, const path_node* head)
{
    return node->next != head ? node->next : nullptr;
}

// Pulls the off-nav-mesh nodes out of the list, in order, into their own circular list.
static path_node* bb_pf_path_take_off_navmesh(path_node** path)
{
    path_node* off_navmesh = nullptr;
    path_node* node = *path;
    while (node != nullptr) {
        path_node* next = bb_pf_path_next(node, *path);
        if (node->flags.off_navmesh) {
            bb_pf_path_unlink(path, node);
            bb_pf_path_append(&off_navmesh, node);
        }
        node = next;
    }
    return off_navmesh;
}

// The navpoint of a cell's mover arrival edge: edge 3 is the cell's first navpoint, each further edge the next one.
static navpoint* bb_pf_arrival_navpoint(const nav_cell* ncp, char arrival_edge)
{
    navpoint* navp = World.object_from_handle<navpoint>(ncp->navp);
    for (int edge = 3; navp != nullptr && edge < arrival_edge; edge++) {
        navp = World.object_from_handle<navpoint>(navp->get_next_navpoint());
    }
    return navp;
}

// 0x00B0CDC0 (:1485) LTCG: start_nav_cell EAX
static void build_raw_path(path_node** path, nav_cell* start_nav_cell, const vector& start_pos)
{
    nav_cell* cell = start_nav_cell;
    *path = nullptr;
    path_node* node = bb_pf_get_free_path_node();
    if (node == nullptr) {
        return;
    }
    node->nav_cell_handle = nav_cell_get_handle(cell);
    node->pos = start_pos;
    bb_pf_path_append(path, node);
    for (;;) {
        nav_heap_cell_data* data = Nav_heap.get_cell_data(cell);
        node = bb_pf_get_free_path_node();
        if (node == nullptr) {
            return;
        }
        node->nav_cell_handle = nav_cell_get_handle(cell);
        node->pos = nav_cell_force_point_to_cell(cell, data->arrival_pos);
        bb_pf_path_append(path, node);
        char arrival_edge = data->arrival_edge;
        if (arrival_edge < 0) {
            return;
        }
        if (arrival_edge > 2) {
            // Arrived through a mover's navpoint. The path leaves the nav mesh here, through the door or hole.
            vector arrival = node->pos;
            navpoint* navp = bb_pf_arrival_navpoint(cell, arrival_edge);
            if (navp == nullptr) {
                return;
            }
            float gap = vec_xz_dist(navp->pos, arrival) - navp->radius;
            for (int i = 0; i < navp->num_links; i++) {
                const navpoint* link = navp->links[i];
                if (link != nullptr) {
                    vector closest;
                    ix_closest_point_on_line_segment(closest, arrival, navp->pos, link->pos);
                    float dist = vec_xz_dist(closest, arrival);
                    float radius = link->radius <= navp->radius ? link->radius : navp->radius;
                    if (dist - radius < gap) {
                        gap = dist - radius;
                    }
                }
            }
            if (gap <= 0.0f) {
                node->nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
                node->navpoint_handle = navp->get_handle();
            } else {
                node = bb_pf_get_free_path_node();
                if (node == nullptr) {
                    return;
                }
                node->pos = navp->pos;
                bb_pf_path_append(path, node);
                node->nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
                node->navpoint_handle = navp->get_handle();
            }
            // Magic numbers: the Navpoint_type_table values of the nodes that go on inside.
            if (navp->num_links == 1 && navp->links[0]->type == 11) {
                node = bb_pf_get_free_path_node();
                if (node != nullptr) {
                    node->pos = navp->links[0]->pos;
                    bb_pf_path_append(path, node);
                    node->nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
                    node->navpoint_handle = navp->links[0]->get_handle();
                }
                return;
            }
            for (int i = 0; i < navp->num_links; i++) {
                navpoint* link = navp->links[i];
                signed char type = link->type;
                if (type == 17 || type == 10 || type == 18 || type == 19 || type == 14) {
                    node = bb_pf_get_free_path_node();
                    if (node != nullptr) {
                        node->pos = link->pos;
                        bb_pf_path_append(path, node);
                        node->nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
                        node->navpoint_handle = link->get_handle();
                    }
                    return;
                }
            }
            return;
        }
        cell = cell->adjacent_cells[arrival_edge].to_pointer();
    }
}

// 0x00AEA7B0 (:1602)
static vector get_bridge_offset(const vector& start_pt, const vector& end_pt)
{
    vector closest;
    float start_index;
    // Magic number: 60 m search radius.
    object_path_road* road = object_road_get_closest_road(start_pt, false, closest, start_index, 60.0f, false, nullptr, nullptr, false);
    if (road == nullptr) {
        return ZERO_VECTOR;
    }
    float end_index = road->find_closest_point_on_road(end_pt, closest, false);
    bool forward = start_index <= end_index;
    float low = forward ? start_index : end_index;
    float high = forward ? end_index : start_index;
    int high_index = (int)ceil((double)high);
    road_broken_state state = ROAD_BROKEN_NEITHER;
    for (int index = (int)floor((double)low); index < high_index; index++) {
        road_broken_state broken = road->is_broken_at_index(index);
        if (broken == ROAD_BROKEN_COMPLETELY) {
            return ZERO_VECTOR;
        }
        if (broken != ROAD_BROKEN_NEITHER && state != broken) {
            if (state != ROAD_BROKEN_NEITHER) {
                return ZERO_VECTOR;
            }
            state = broken;
        }
    }
    if (state == ROAD_BROKEN_NEITHER) {
        return ZERO_VECTOR;
    }
    vector dir = end_pt - start_pt;
    dir.normalize();
    vector side(UP_VECTOR.z * dir.y - UP_VECTOR.y * dir.z, UP_VECTOR.x * dir.z - UP_VECTOR.z * dir.x, UP_VECTOR.y * dir.x - UP_VECTOR.x * dir.y);
    if (forward ? state == ROAD_BROKEN_RIGHT : state == ROAD_BROKEN_LEFT) {
        side.x = -side.x;
        side.y = -side.y;
        side.z = -side.z;
    }
    return side;
}

static path_node* bb_pf_add_vehicle_special_node(path_node** path, const navpoint* navp, bool jumping)
{
    path_node* node = bb_pf_get_free_path_node();
    if (node == nullptr) {
        return nullptr;
    }
    node->pos = navp->pos;
    node->flags.do_not_remove = true;
    if (jumping) {
        node->flags.jumping = true;
    } else {
        node->flags.bridge = true;
    }
    return node;
}

// 0x00B0D1F0 (:1670) LTCG: start_pos EAX, start_nav_cell ECX
static void build_raw_path(path_node** path, const nav_cell* start_nav_cell, const vector& start_pos, nav_heap* nhp)
{
    const signed char TYPE_ROAD_JUMP = 6;
    const signed char TYPE_ROAD_JUMP_DEST = 7;
    const signed char TYPE_ROAD_JUMP_START = 8;
    const signed char TYPE_ROAD_BRIDGE = 22;
    const nav_cell* cell = start_nav_cell;
    vector pos = start_pos;
    nav_cell_force_point_to_cell_plane(pos, cell);
    *path = nullptr;
    path_node* node = bb_pf_get_free_path_node(pos, nav_cell_get_handle(cell), 0xFFFFFFFF);
    if (node == nullptr) {
        return;
    }
    bb_pf_path_append(path, node);
    for (;;) {
        nav_heap_cell_data* data = nhp->get_cell_data(cell);
        uint64_t handle = nav_cell_get_handle(cell);
        vector arrival = nav_cell_force_point_to_cell_no_height(cell, data->arrival_pos);
        node = bb_pf_get_free_path_node(arrival, handle, 0xFFFFFFFF);
        if (node == nullptr) {
            return;
        }
        bb_pf_path_append(path, node);
        char arrival_edge = data->arrival_edge;
        if (arrival_edge < 0) {
            return;
        }
        if (arrival_edge > 2) {
            navpoint* navp = bb_pf_arrival_navpoint(cell, arrival_edge);
            if (navp == nullptr) {
                return;
            }
            if (navp->type == TYPE_ROAD_JUMP_START) {
                path_node* jump = bb_pf_add_vehicle_special_node(path, navp, true);
                if (jump == nullptr) {
                    return;
                }
                jump->nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
                bb_pf_path_append(path, jump);
                for (int i = 0; i < navp->num_links; i++) {
                    const navpoint* link = navp->links[i];
                    if (link->type != TYPE_ROAD_JUMP) {
                        continue;
                    }
                    jump = bb_pf_add_vehicle_special_node(path, link, true);
                    if (jump == nullptr) {
                        return;
                    }
                    jump->nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
                    bb_pf_path_append(path, jump);
                    for (int j = 0; j < link->num_links; j++) {
                        if (link->links[j]->type == TYPE_ROAD_JUMP_DEST) {
                            jump = bb_pf_add_vehicle_special_node(path, link->links[j], true);
                            if (jump == nullptr) {
                                return;
                            }
                            jump->nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
                            bb_pf_path_append(path, jump);
                            return;
                        }
                    }
                }
                return;
            }
            if (navp->type != TYPE_ROAD_BRIDGE) {
                return;
            }
            node->flags.do_not_remove = true;
            node->flags.bridge = true;
            bool crossed = false;
            for (int i = 0; i < navp->num_links && !crossed; i++) {
                navpoint* link = navp->links[i];
                if (link->type != TYPE_ROAD_BRIDGE) {
                    continue;
                }
                path_node* far_node = bb_pf_add_vehicle_special_node(path, link, false);
                if (far_node == nullptr) {
                    return;
                }
                far_node->nav_cell_handle = nav_cell_get_handle(link->nav_cell_ptr);
                far_node->navpoint_handle = link->get_handle();
                cell = link->nav_cell_ptr;
                bb_pf_path_append(path, far_node);
                vector offset = get_bridge_offset(node->pos, far_node->pos);
                if (offset != ZERO_VECTOR) {
                    // A broken road: both ends move to its intact side.
                    float radius = link->radius < navp->radius ? navp->radius : link->radius;
                    vector target(offset.x * radius * 0.5f + node->pos.x, node->pos.y + offset.y * radius * 0.5f, node->pos.z + offset.z * radius * 0.5f);
                    const nav_cell* end_cell;
                    nav_cell_find_longest_line_of_sight_no_detour(node->pos, end_cell, node->pos, navp->nav_cell_ptr, target, false, nullptr);
                    node->nav_cell_handle = nav_cell_get_handle(end_cell);
                    radius = link->radius < navp->radius ? navp->radius : link->radius;
                    target = vector(far_node->pos.x + offset.x * radius * 0.5f, far_node->pos.y + offset.y * radius * 0.5f, far_node->pos.z + offset.z * radius * 0.5f);
                    nav_cell_find_longest_line_of_sight_no_detour(far_node->pos, end_cell, far_node->pos, link->nav_cell_ptr, target, false, nullptr);
                    far_node->nav_cell_handle = nav_cell_get_handle(end_cell);
                }
                crossed = true;
            }
            if (crossed) {
                continue;
            }
            // Original bug kept: a bridge node with no linked bridge node goes on to read adjacent_cells[arrival_edge],
            // past the array when the edge is 3 or more. Nothing here guards it in rfg.exe; such data would break it.
        }
        cell = (&cell->adjacent_cells[0] + arrival_edge)->to_pointer();
    }
}

// 0x00B13F20 (:1860)
static bool optimize_node_position(vector* new_pos, path_node* node0, path_node* node1, path_node* node2, const vector& start_pos, const vector& end_pos, bool bias_towards_start, const edge_constraint_info& econstraint_info, unsigned int nav_cell_los_flags, bool found)
{
    // Magic number: the bisection ends under 2 m.
    double dx = (double)(start_pos.x - end_pos.x);
    double dz = (double)(start_pos.z - end_pos.z);
    if ((float)(dx * dx + 0.0 + dz * dz) < 4.0f) {
        return found;
    }
    vector mid((end_pos.x + start_pos.x) * 0.5f, (start_pos.y + end_pos.y) * 0.5f, (end_pos.z + start_pos.z) * 0.5f);
    const nav_cell* cell = nav_cell_from_handle(node1->nav_cell_handle);
    mid = cell != nullptr ? nav_cell_force_point_to_cell_no_height(cell, mid) : ZERO_VECTOR;
    bool clamped = false;
    if (econstraint_info.active) {
        const vector& p0 = econstraint_info.edge_p0;
        const vector& dir = econstraint_info.edge_dir;
        float t = (dir.y * (mid.y - p0.y) + dir.x * (mid.x - p0.x) + dir.z * (mid.z - p0.z)) / econstraint_info.edge_length;
        if (t < econstraint_info.ix_min || econstraint_info.ix_max < t) {
            t = t < econstraint_info.ix_min ? econstraint_info.ix_min : econstraint_info.ix_max;
            float along = econstraint_info.edge_length * t;
            vector point(p0.x + dir.x * along, p0.y + dir.y * along, p0.z + dir.z * along);
            clamped = true;
            mid = nav_cell_force_point_to_cell_no_height(node1->nav_cell_handle, point);
        }
    }
    bool ok = nav_cell_test_line_of_sight_no_height(node0->nav_cell_handle, node0->pos, node1->nav_cell_handle, mid, nav_cell_los_flags);
    ok = nav_cell_test_line_of_sight_no_height(node1->nav_cell_handle, mid, node2->nav_cell_handle, node2->pos, nav_cell_los_flags) && ok;
    if (ok) {
        *new_pos = mid;
        found = true;
    }
    if (clamped) {
        return found;
    }
    if (ok == bias_towards_start) {
        return optimize_node_position(new_pos, node0, node1, node2, start_pos, mid, bias_towards_start, econstraint_info, nav_cell_los_flags, found);
    }
    return optimize_node_position(new_pos, node0, node1, node2, mid, end_pos, bias_towards_start, econstraint_info, nav_cell_los_flags, found);
}

// 0x00AF3EB0 (:2000) LTCG: r_path EAX, out_of_chunk_path ESI
static void add_out_of_chunk_path_segment(path_node** out_of_chunk_path, raw_path& r_path)
{
    if (*out_of_chunk_path != nullptr) {
        // The segment's first node repeats the end of the last one.
        bb_pf_free_path_node(&r_path.path, r_path.path);
        if (*out_of_chunk_path != nullptr) {
            bb_pf_path_splice(out_of_chunk_path, r_path.path);
            return;
        }
    }
    *out_of_chunk_path = r_path.path;
}

// 0x00B14320 (:2022) LTCG: start_node ESI
static bool test_path_node_los(const path_node* start_node, const path_node* end_node, const pf_calc_flags& flags)
{
    unsigned int los_flags = NAV_CELL_LOS_NO_CLIMBING;
    if (flags.allow_land_to_water != 0) {
        los_flags |= NAV_CELL_LOS_LAND_TO_WATER;
    }
    if (flags.ignore_detour_hulls != 0) {
        los_flags |= NAV_CELL_LOS_IGNORE_DETOUR;
    }
    if (flags.use_disabled_cells != 0) {
        los_flags |= NAV_CELL_LOS_ALLOW_DISABLED;
    }
    if (flags.use_light_structure_disabled_cells != 0) {
        los_flags |= NAV_CELL_LOS_ALLOW_LIGHT;
    }
    nav_cell_los_test_point start_point(start_node->pos, start_node->nav_cell_handle, start_node->compound_hull_handle, start_node->compound_hull_vert_index);
    nav_cell_los_test_point end_point(end_node->pos, end_node->nav_cell_handle, end_node->compound_hull_handle, end_node->compound_hull_vert_index);
    if (flags.pre_detour_simplification == 0) {
        return nav_cell_test_line_of_sight_no_height(start_point, end_point, los_flags);
    }
    nav_cell_longest_los_input input;
    nav_cell_longest_los_output output;
    input.flags.ignore_detour_hulls = flags.ignore_detour_hulls;
    input.start_pos = start_node->pos;
    input.start_nav_cell = nav_cell_from_handle(start_node->nav_cell_handle);
    input.delta_2d = vector2(end_node->pos.x - start_node->pos.x, end_node->pos.z - start_node->pos.z);
    input.flags.use_light_nav_cells = flags.use_light_structure_disabled_cells;
    input.flags.use_disabled_nav_cells = flags.use_disabled_cells;
    nav_cell_find_longest_line_of_sight_point(output, input);
    return output.result == NAV_CELL_LONGEST_LOS_RESULT_REACHED_DESTINATION && output.end_nav_cell == nav_cell_from_handle(end_node->nav_cell_handle);
}

// 0x00AEAAF0 (:2080) LTCG: path EDX, n ECX
static path_node* get_path_node_from_offset(path_node* node, path_node* path, int n)
{
    for (; n > 0; n--) {
        node = bb_pf_path_next(node, path);
    }
    return node;
}

// 0x00B14520 (:2101)
void simplify_nav_path_by_los(path_node** path, const pf_calc_flags& flags)
{
    if (*path == nullptr) {
        return;
    }
    path_node* off_navmesh = bb_pf_path_take_off_navmesh(path);
    if (*path == nullptr) {
        *path = off_navmesh;
        return;
    }
    path_node* last = (*path)->prev;
    if (*path == last) {
        // Original bug kept: with one node left the off-mesh nodes are neither put back nor freed.
        return;
    }
    path_node* cur = *path;
    for (;;) {
        path_node* first = cur->next;
        if (first == *path || first == nullptr || first == last) {
            break;
        }
        // Up to the next node that must stay; then a binary search for the furthest one in line of sight.
        int high = 1;
        path_node* high_node = first;
        while (!high_node->flags.do_not_remove) {
            high_node = bb_pf_path_next(high_node, *path);
            high++;
            if (high_node == last) {
                break;
            }
        }
        if (high_node == first) {
            cur = first;
            continue;
        }
        int low = 0;
        path_node* low_node = cur;
        path_node* candidate = high_node;
        int candidate_index = high;
        path_node* taken = nullptr;
        while (candidate != cur) {
            if (test_path_node_los(cur, candidate, flags)) {
                if (candidate == high_node) {
                    taken = candidate;
                    break;
                }
                low = candidate_index;
                low_node = candidate;
            } else {
                high_node = candidate == *path ? nullptr : candidate->prev;
                high = candidate_index - 1;
            }
            if (low == high) {
                taken = low_node;
                break;
            }
            candidate_index = (low + 1 + high) / 2;
            candidate = get_path_node_from_offset(cur, *path, candidate_index);
        }
        if (taken == nullptr || taken == cur) {
            cur = first;
            continue;
        }
        path_node* remove = bb_pf_path_next(cur, *path);
        while (remove != taken) {
            path_node* next = remove->next;
            path_node* head = *path;
            bb_pf_free_path_node(path, remove);
            remove = next != head ? next : nullptr;
        }
        cur = taken;
    }
    path_node* node = *path;
    while (node != nullptr) {
        path_node* next = node->next;
        if (next == *path || next == nullptr || next == last) {
            break;
        }
        if (node->pos == next->pos) {
            path_node* head = *path;
            node = next->next != head ? next->next : nullptr;
            bb_pf_free_path_node(path, next);
        } else {
            node = next;
        }
    }
    bb_pf_path_splice(path, off_navmesh);
}

// 0x00B147B0 (:2351)
static int optimize_node_positions(path_node* path, int num_nodes_to_optimize, unsigned int nav_cell_los_flags, bool constrain_edge_sliding, float max_optimize_dist)
{
    path_node* cur = path;
    for (;;) {
        if (cur == nullptr) {
            return 0;
        }
        if (!cur->flags.optimized) {
            break;
        }
        cur = cur->next;
        if (cur == path) {
            return 0;
        }
    }
    path_node* prev = cur == path ? nullptr : cur->prev;
    path_node* next = bb_pf_path_next(cur, path);
    if (prev == nullptr || next == nullptr) {
        cur->flags.optimized = true;
        return 0;
    }
    if (next->flags.off_navmesh) {
        return 0;
    }
    path_node* off_navmesh = bb_pf_path_take_off_navmesh(&path);
    int num_moved = 0;
    while (num_nodes_to_optimize != 0) {
        const nav_cell* cell = nullptr;
        if (!cur->flags.optimized && cur->compound_hull_handle == 0xFFFFFFFF && !cur->flags.do_not_remove && prev->pos != next->pos) {
            cell = nav_cell_from_handle(cur->nav_cell_handle);
        }
        if (cell != nullptr) {
            // The node slides along its cell's nearest open edge towards the line from prev to next.
            int best_edge = -1;
            float best_dist_sq = 0.0f;
            for (int edge = 0; edge < 3; edge++) {
                const nav_cell* adjacent = cell->adjacent_cells[edge].to_pointer();
                if (adjacent == nullptr) {
                    continue;
                }
                if ((nav_cell_los_flags & NAV_CELL_LOS_ALLOW_DISABLED) == 0
                    && !((nav_cell_los_flags & NAV_CELL_LOS_ALLOW_LIGHT) != 0 && (adjacent->flags & NAV_CELL_FLAG_LIGHT) != 0)
                    && (adjacent->flags & NAV_CELL_FLAG_DISABLED) != 0) {
                    continue;
                }
                vector midpoint;
                nav_cell_get_edge_midpoint(&midpoint, cell, edge);
                float dist_sq = vec_dist_squared(midpoint, cur->pos);
                if (best_edge < 0 || dist_sq < best_dist_sq) {
                    best_edge = edge;
                    best_dist_sq = dist_sq;
                }
            }
            vector edge0, edge1;
            nav_cell_get_edge_verts(&edge0, &edge1, cell, best_edge, nullptr);
            edge_constraint_info constraint;
            constraint.active = constrain_edge_sliding;
            if (constrain_edge_sliding) {
                constraint.edge_p0 = edge0;
                constraint.edge_p1 = edge1;
                constraint.edge_dir = edge1 - edge0;
                constraint.edge_length = constraint.edge_dir.mag_and_normalize_safe();
                // Magic numbers: stay 5 m from an edge's ends; an edge under 10 m pins the node to its middle.
                if (10.0f <= constraint.edge_length) {
                    constraint.ix_min = 5.0f / constraint.edge_length;
                    constraint.ix_max = 1.0f - constraint.ix_min;
                } else {
                    constraint.ix_max = 0.5f;
                    constraint.ix_min = 0.5f;
                }
            }
            vector on_path, on_edge;
            float path_t, edge_t;
            if (ix_line_line_find_closest_distance(&on_path, &on_edge, prev->pos, next->pos, edge0, edge1, &path_t, &edge_t)) {
                vector new_pos;
                bool bias_towards_start = 0.5f > edge_t;
                const vector& start_pos = 0.5f <= edge_t ? cur->pos : edge0;
                const vector& end_pos = 0.5f <= edge_t ? edge1 : cur->pos;
                if (optimize_node_position(&new_pos, prev, cur, next, start_pos, end_pos, bias_towards_start, constraint, nav_cell_los_flags | NAV_CELL_LOS_NO_CLIMBING, false)) {
                    cur->pos = new_pos;
                    havok_terrain_height(cur->pos.y, new_pos.x, new_pos.z, false, nullptr);
                    num_moved++;
                }
            }
        }
        cur->flags.optimized = true;
        max_optimize_dist -= vec_dist(prev->pos, cur->pos);
        if (max_optimize_dist <= 0.0f) {
            break;
        }
        path_node* after = next->next;
        if (after == path || after == nullptr) {
            break;
        }
        if (num_nodes_to_optimize > 0) {
            num_nodes_to_optimize--;
        }
        prev = cur;
        cur = next;
        next = after;
        if (after->flags.off_navmesh) {
            break;
        }
    }
    path_node* tail = path == nullptr ? nullptr : path->prev;
    path_node* before_tail = tail == nullptr || tail == path ? nullptr : tail->prev;
    if (tail != nullptr && before_tail != nullptr && before_tail->flags.optimized) {
        tail->flags.optimized = true;
    }
    if (off_navmesh != nullptr && path != nullptr) {
        bb_pf_path_splice(&path, off_navmesh);
    }
    return num_moved;
}

// 0x00B14DD0 (:2530) LTCG: flags ESI, path EDI
static void simplify_nav_path(path_node** path, const pf_calc_flags& flags, bool constrain_edge_sliding, float max_simplify_dist, int num_vehicle_nodes_to_optimize)
{
    pf_calc_flags los_calc_flags = flags;
    los_calc_flags.ignore_detour_hulls = 1;
    simplify_nav_path_by_los(path, los_calc_flags);
    (*path)->flags.optimized = true;
    unsigned int los_flags = nav_cell_get_los_obstruction_flags_from_path_calc_flags(flags);
    // Magic numbers: -1 optimizes every node, a human path optimizes one at a time.
    int num_nodes = num_vehicle_nodes_to_optimize;
    if (flags.force_complete_node_optimization != 0) {
        num_nodes = -1;
    } else if (flags.vehicle_pfg == 0) {
        num_nodes = 1;
    }
    optimize_node_positions(*path, num_nodes, los_flags, constrain_edge_sliding, max_simplify_dist);
}

// 0x00AF3F00 (:2567)
static void finalize_path(path_node** path)
{
    const signed char TYPE_LADDER_TRANSITION = 12;
    const signed char TYPE_LADDER = 13;
    // A path ending on a ladder goes on to the ladder's other end.
    if (*path != nullptr && (*path)->prev != nullptr && (*path)->prev->navpoint_handle != 0xFFFFFFFF) {
        object* objp = World.object_from_handle((*path)->prev->navpoint_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_NAVPOINT) {
            navpoint* navp = (navpoint*)objp;
            if ((navp->type == TYPE_LADDER || navp->type == TYPE_LADDER_TRANSITION) && navp->num_links > 1 && navp->links[0] != nullptr) {
                navpoint* other = navp->links[0];
                if (other->type == TYPE_LADDER || other->type == TYPE_LADDER_TRANSITION) {
                    other = navp->links[1];
                }
                if (other != nullptr) {
                    path_node* node = bb_pf_get_free_path_node();
                    if (node != nullptr) {
                        node->pos = other->pos;
                        node->navpoint_handle = other->get_handle();
                        bb_pf_path_append(path, node);
                    }
                }
            }
        }
    }
    // Magic number: nodes within 10 cm merge.
    path_node* node = *path;
    if (node != nullptr) {
        path_node* previous = nullptr;
        for (;;) {
            path_node* keep = node;
            if (previous != nullptr && vec_dist_squared(node->pos, previous->pos) < 0.01f) {
                bb_pf_free_path_node(path, node);
                keep = previous;
            }
            node = keep->next;
            if (node == *path || node == nullptr) {
                break;
            }
            previous = keep;
        }
    }
    if (*path == nullptr) {
        // rfg.exe would write through null here; every caller has a path.
        return;
    }
    float total = 0.0f;
    vector dir = FORWARD_VECTOR;
    node = *path;
    for (;;) {
        node->distance_left = total;
        if (node == (*path)->prev) {
            break;
        }
        path_node* next = node->next;
        node->normal = next->pos - node->pos;
        total = sqrtf(node->normal.x * node->normal.x + node->normal.y * node->normal.y + node->normal.z * node->normal.z) + total;
        node->normal.normalize();
        dir = node->normal;
        node = next;
    }
    node->normal = dir;
    node = *path;
    do {
        node->distance_left = total - node->distance_left;
        node = node->next;
    } while (node != *path);
}

// The search radius from the two cells' sizes and the distance between them; get_raw_path(params) inlines the same.
static float bb_pf_cells_max_search_distance(const nav_cell* start_nav_cell, const vector& start_pos, const nav_cell* end_nav_cell, const vector& end_pos, bool vehicle_pfg)
{
    vector start_mid[3];
    vector end_mid[3];
    for (int edge = 0; edge < 3; edge++) {
        nav_cell_get_edge_midpoint(&start_mid[edge], start_nav_cell, edge);
        nav_cell_get_edge_midpoint(&end_mid[edge], end_nav_cell, edge);
    }
    float start_size = 0.0f;
    for (int edge = 0; edge < 3; edge++) {
        if (start_nav_cell->adjacent_cells[edge].to_pointer() != nullptr) {
            float dist_sq = vec_dist_squared(start_pos, start_mid[edge]);
            if (start_size - dist_sq < 0.0f) {
                start_size = dist_sq;
            }
        }
    }
    float end_size = 0.0f;
    for (int edge = 0; edge < 3; edge++) {
        if (end_nav_cell->adjacent_cells[edge].to_pointer() != nullptr) {
            float dist_sq = vec_dist_squared(end_pos, end_mid[edge]);
            if (end_size - dist_sq < 0.0f) {
                end_size = dist_sq;
            }
        }
    }
    float between = 0.0f;
    for (int i = 0; i < 3; i++) {
        if (start_nav_cell->adjacent_cells[i].to_pointer() != nullptr) {
            for (int j = 0; j < 3; j++) {
                if (end_nav_cell->adjacent_cells[j].to_pointer() != nullptr) {
                    float dist_sq = vec_dist_squared(start_mid[i], end_mid[j]);
                    if (between - dist_sq < 0.0f) {
                        between = dist_sq;
                    }
                }
            }
        }
    }
    float result = (sqrtf(end_size) + sqrtf(start_size) + sqrtf(between)) * PATH_CALC_DIST_MAX_FACTOR + PATH_CALC_DIST_BASE;
    float limit = vehicle_pfg ? VEHICLE_DEFAULT_PATH_CALC_DIST_MAX : PED_DEFAULT_PATH_CALC_DIST_MAX;
    return 0.0f <= result - limit ? limit : result;
}

// 0x00B02D00 (:2662) LTCG: end_pos ECX
static float get_max_search_distance(const nav_cell* start_nav_cell, const vector& start_pos, const nav_cell* end_nav_cell, const vector& end_pos, bool vehicle_pfg, bool scripted_move)
{
    if (vehicle_pfg && !scripted_move) {
        // Original quirk kept: an unscripted vehicle path takes the pedestrian limit.
        return PED_DEFAULT_PATH_CALC_DIST_MAX;
    }
    return bb_pf_cells_max_search_distance(start_nav_cell, start_pos, end_nav_cell, end_pos, vehicle_pfg);
}

// 0x00AEAB30 (:2735) LTCG: output ESI, start_pos EAX
static void build_out_of_chunk_path(raw_path& output, const vector& start_pos, const vector& end_pos)
{
    output.out_of_chunk = true;
    output.adjusted_start_pos = start_pos;
    output.adjusted_end_pos = end_pos;
    output.path = nullptr;
    path_node* node = bb_pf_get_free_path_node();
    if (node == nullptr) {
        return;
    }
    node->pos = start_pos;
    node->nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
    bb_pf_path_append(&output.path, node);
    node = bb_pf_get_free_path_node();
    if (node == nullptr) {
        return;
    }
    node->pos = end_pos;
    node->nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
    bb_pf_path_append(&output.path, node);
}

// 0x00AEAC00 (:2756) LTCG: navp EDX, allowed_types ESI, num_allowed_types ECX
static bool is_in_allowed_set(const navpoint* navp, const signed char* allowed_types, const int num_allowed_types)
{
    for (int i = 0; i < num_allowed_types; i++) {
        if (allowed_types[i] == navp->type) {
            return true;
        }
    }
    return false;
}

// 0x00AF4200 (:2770) LTCG: point_list ESI
static const navpoint* find_nearest_exit_navpoint(const vector& pos, const farray<navpoint const*,32>& point_list)
{
    if (point_list.size() <= 0) {
        return nullptr;
    }
    int best = 0;
    float best_dist_sq = vec_dist_squared(pos, point_list[0]->pos);
    for (int i = 1; i < point_list.size(); i++) {
        float dist_sq = vec_dist_squared(pos, point_list[i]->pos);
        if (dist_sq < best_dist_sq) {
            best = i;
            best_dist_sq = dist_sq;
        }
    }
    return point_list[best];
}

// 0x00B14E60 (:2793) LTCG: starting_navp EAX, point_list EDI
static const navpoint* find_best_exit_trans_point(navpoint* starting_navp, const farray<navpoint const*,32>& point_list, const signed char* allowed_types, const int num_allowed_types, bool use_doors)
{
    Navpoint_heap.start_session();
    navpoint_query_for_path(starting_navp, nullptr, 0.0f, find_nearest_exit_navpoint(starting_navp->pos, point_list)->pos, &Navpoint_heap);
    const navpoint* best = nullptr;
    float best_cost = 3.4028235e+38f;
    while (Navpoint_heap.num_navpoints > 0) {
        navpoint* navp = Navpoint_heap.remove_best_cell();
        navp->heap_data.flags.is_open = 0;
        if ((!Navpoint_treat_all_as_enabled && navp->heap_data.disabled_by != 0) || best_cost < navp->heap_data.arrival_cost) {
            continue;
        }
        if (is_in_allowed_set(navp, allowed_types, num_allowed_types)) {
            vector adjusted;
            const nav_cell* cell = bb_pf_get_nav_cell_from_hint(&adjusted, navp->nav_cell_ptr, navp->pos, false, nullptr, false, false, NAV_CELL_TRAVERSAL_ENABLED);
            if (cell != nullptr && nav_cell_detour_is_point_in_compound_hull(navp->pos, cell) == nullptr && navp->heap_data.arrival_cost < best_cost) {
                best = navp;
                best_cost = navp->heap_data.arrival_cost;
            }
            continue;
        }
        for (int i = 0; i < navp->num_links; i++) {
            navpoint* link = navp->links[i];
            if (use_doors || navp->nav_flags.door == 0 || link->nav_flags.door == 0) {
                float cost = vec_dist(navp->pos, link->pos) + navp->heap_data.arrival_cost;
                if (cost <= best_cost) {
                    navpoint_query_for_path(link, navp, cost, find_nearest_exit_navpoint(link->pos, point_list)->pos, &Navpoint_heap);
                }
            }
        }
    }
    Navpoint_heap.end_session();
    return best;
}

// 0x00B15180 (:2878) LTCG: starting_point ECX
static navpoint* rfg_pf_find_best_transition_node(const vector& dest_pos, navpoint* starting_point, float& cost, bool allow_jumps, bool use_doors, unsigned int node_to_avoid)
{
    const signed char TYPE_TRANSITION = 11;
    const signed char TYPE_LADDER_TRANSITION = 12;
    const signed char TYPE_TRANSITION_JUMP = 15;
    const signed char TYPE_TRANSITION_LEAP_OVER = 16;
    // Magic number: rfg.exe weighs the distance left to the goal by half again.
    const float TO_GOAL_WEIGHT = 1.5f;
    unsigned int session = Navpoint_heap.start_session();
    navpoint_query_for_path(starting_point, nullptr, 0.0f, dest_pos, &Navpoint_heap);
    cost = 0.0f;
    navpoint* best = nullptr;
    float best_to_goal = 3.4028235e+38f;
    // A node inside a detour hull only counts when there is nothing else.
    navpoint* blocked_best = nullptr;
    float blocked_to_goal = 3.4028235e+38f;
    float blocked_cost = 0.0f;
    while (Navpoint_heap.num_navpoints > 0) {
        navpoint* navp = Navpoint_heap.remove_best_cell();
        navp->heap_data.flags.is_open = 0;
        if ((!Navpoint_treat_all_as_enabled && navp->heap_data.disabled_by != 0) || navp->obj_flags.streamed == 0) {
            continue;
        }
        signed char type = navp->type;
        if ((type == TYPE_TRANSITION || type == TYPE_LADDER_TRANSITION || (allow_jumps && (type == TYPE_TRANSITION_JUMP || type == TYPE_TRANSITION_LEAP_OVER))) && navp->get_handle() != node_to_avoid) {
            vector adjusted;
            const nav_cell* cell = bb_pf_get_nav_cell_from_hint(&adjusted, navp->nav_cell_ptr, navp->pos, false, nullptr, false, false, NAV_CELL_TRAVERSAL_ENABLED);
            if (cell != nullptr && nav_cell_detour_is_point_in_compound_hull(navp->pos, cell) != nullptr) {
                float to_goal = vec_dist(navp->pos, dest_pos) * TO_GOAL_WEIGHT;
                if (blocked_best == nullptr || to_goal < blocked_to_goal) {
                    blocked_cost = navp->heap_data.arrival_cost;
                    blocked_best = navp;
                    blocked_to_goal = to_goal;
                }
                continue;
            }
            float to_goal = vec_dist(navp->pos, dest_pos) * TO_GOAL_WEIGHT;
            if (navp->heap_data.arrival_cost + to_goal < cost + best_to_goal) {
                cost = navp->heap_data.arrival_cost;
                best_to_goal = to_goal;
                best = navp;
            }
        }
        for (int i = 0; i < navp->num_links; i++) {
            if (!Navpoint_treat_all_as_enabled && (navp->link_disabled_by[i] & NAVPOINT_DISABLED_BY_DETOUR) != 0) {
                continue;
            }
            navpoint* link = navp->links[i];
            if ((!Navpoint_treat_all_as_enabled && link->heap_data.disabled_by != 0) || link->obj_flags.streamed == 0) {
                continue;
            }
            // Magic numbers: type 20 nodes are entered only from type 19 nodes.
            if (link->type == 20 && navp->type != 19) {
                continue;
            }
            if (link->heap_data.session_id == session && link->heap_data.flags.is_open == 0) {
                continue;
            }
            if (!use_doors && navp->nav_flags.door != 0 && link->nav_flags.door != 0) {
                continue;
            }
            navpoint_query_for_path(link, navp, navp->heap_data.arrival_cost + vec_dist(link->pos, navp->pos), dest_pos, &Navpoint_heap);
        }
    }
    Navpoint_heap.end_session();
    if (best == nullptr && blocked_best != nullptr) {
        cost = blocked_cost;
        return blocked_best;
    }
    return best;
}

// 0x00B15570 (:2979)
static void query_mover_for_path(navpoint* navp, nav_cell* arrival_cell, const vector& arrival_pos, float arrival_cost, const vector& goal_pos, nav_heap* nhp, bool use_doors)
{
    float through_cost = 0.0f;
    navpoint* exit_navp = rfg_pf_find_best_transition_node(goal_pos, navp, through_cost, false, use_doors, navp->get_handle());
    if (exit_navp == nullptr || exit_navp == navp || (!Navpoint_treat_all_as_enabled && exit_navp->heap_data.disabled_by != 0) || exit_navp->obj_flags.streamed == 0) {
        return;
    }
    vector adjusted;
    nav_cell* cell = bb_pf_get_nav_cell_from_hint(&adjusted, exit_navp->nav_cell_ptr, exit_navp->pos, false, nullptr, false, false, NAV_CELL_TRAVERSAL_ENABLED);
    if (cell == nullptr || (cell->flags & NAV_CELL_FLAG_DISABLED) != 0 || cell == arrival_cell) {
        return;
    }
    // The exit's place in its cell's navpoint chain is what build_raw_path finds it by again.
    navpoint* chain = World.object_from_handle<navpoint>(cell->navp);
    unsigned short which_navpoint = 0;
    if (chain == nullptr) {
        return;
    }
    while (chain != exit_navp) {
        chain = World.object_from_handle<navpoint>(chain->get_next_navpoint());
        which_navpoint++;
        if (chain == nullptr) {
            return;
        }
    }
    // Magic number: a way through a building costs a quarter more than its length.
    nav_cell_query_for_path(cell, arrival_cell, navp->pos, through_cost * 1.25f + arrival_cost, goal_pos, nhp, true, which_navpoint);
}

// 0x00B08BF0 (:3044)
static float rfg_pf_edf_spread_traversal_boost(const nav_cell* ncp)
{
    uint64_t handle = nav_cell_get_handle(ncp);
    int num_on_cell = 0;
    for (int i = 0; i < World.get_num_objects<npc>(); i++) {
        npc* np = World.get_object<npc>(i);
        // Magic number: the EDF team's value.
        if ((int)np->current_team == 1 && np->pf.last_visited_nav_cell_handle == handle) {
            num_on_cell++;
        }
    }
    if (num_on_cell <= 0) {
        return 0.0f;
    }
    return (float)num_on_cell * Rfg_pf_edf_squad_spread_factor / nav_cell_get_area_2d(ncp);
}

// 0x00B156B0 (:3067)
static bool get_raw_path_internal(raw_path& output, nav_heap* nhp, float max_search_dist, unsigned short ignore_set, bool encourage_edf_spread, bool use_doors, unsigned int node_to_avoid)
{
    // Magic number: rfg.exe's limit on a cell's navpoint chain.
    const int MAX_CELL_NAVPOINTS = 26;
    // The search runs from the end to the start, so build_raw_path can walk the arrival edges forward.
    nav_cell_query_for_path(output.end_nav_cell, nullptr, output.adjusted_end_pos, 0.0f, output.adjusted_start_pos, nhp, false, 0);
    if (nhp->num_nav_cells <= 0) {
        return false;
    }
    for (;;) {
        nav_cell* cell = nhp->remove_best_cell();
        if (cell == output.start_nav_cell) {
            return true;
        }
        if ((cell->flags & (NAV_CELL_FLAG_BLOCKED | NAV_CELL_FLAG_DISABLED)) == 0) {
            nav_heap_cell_data* data = nhp->get_cell_data(cell);
            char arrival_edge = data->arrival_edge;
            vector arrival = arrival_edge < 0 ? output.adjusted_end_pos : data->arrival_pos;
            for (int edge = 0; edge < 3; edge++) {
                if (edge == data->arrival_edge) {
                    continue;
                }
                nav_cell* adjacent = cell->adjacent_cells[edge].to_pointer();
                if (adjacent == nullptr) {
                    continue;
                }
                const nav_heap_cell_data* adjacent_data = nhp->get_cell_data(adjacent);
                if ((adjacent_data != nullptr && !adjacent_data->is_open) || (adjacent->flags & (NAV_CELL_FLAG_BLOCKED | NAV_CELL_FLAG_DISABLED)) != 0) {
                    continue;
                }
                vector midpoint;
                nav_cell_get_edge_midpoint(&midpoint, cell, edge);
                float cost = vec_dist(arrival, midpoint);
                if (adjacent == output.start_nav_cell) {
                    cost += vec_dist(midpoint, output.adjusted_start_pos);
                }
                if (encourage_edf_spread && adjacent != output.start_nav_cell) {
                    cost += rfg_pf_edf_spread_traversal_boost(adjacent);
                }
                cost += data->arrival_cost;
                if (data->est_cost_to_goal * EST_COST_TO_GOAL_MULTIPLIER + cost <= max_search_dist) {
                    nav_cell_query_for_path(adjacent, cell, midpoint, cost, output.adjusted_start_pos, nhp, false, 0);
                }
            }
            navpoint* navp = World.object_from_handle<navpoint>(cell->navp);
            for (int index = 0; navp != nullptr;) {
                if (navp->set != ignore_set && (int)arrival_edge != index + 3 && navp->get_handle() != node_to_avoid) {
                    query_mover_for_path(navp, cell, arrival, vec_dist(arrival, navp->pos) + data->arrival_cost, output.adjusted_start_pos, nhp, use_doors);
                }
                unsigned int next_handle = navp->get_next_navpoint();
                if (navp->get_handle() == next_handle) {
                    navp->clear_next_navpoint();
                    break;
                }
                navp = World.object_from_handle<navpoint>(next_handle);
                index++;
                if (index >= MAX_CELL_NAVPOINTS) {
                    break;
                }
            }
        }
        if (nhp->num_nav_cells < 1) {
            return false;
        }
    }
}

// What get_raw_path does with a cell hint: the hint when the point is in it or close by, else a full search.
static nav_cell* bb_pf_cell_from_raw_hint(uint64_t hint_handle, const vector& pos, bool vehicle_pfg, vector& adjusted)
{
    // Magic number: a hint within 10 m is searched from.
    nav_cell* cell = nav_cell_from_handle(hint_handle);
    adjusted = pos;
    if (cell == nullptr) {
        cell = bb_pf_get_nav_cell(pos, vehicle_pfg, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, &adjusted);
    } else if (nav_cell_is_point_in_cell_column(cell, pos)) {
        if ((cell->flags & NAV_CELL_FLAG_DISABLED) != 0) {
            return nullptr;
        }
    } else {
        nav_cell* closest = nullptr;
        if (nav_cell_get_closest_xz_dist_squared_to_point(cell, pos) <= 100.0f && search_for_nav_cell(&closest, cell, pos, NAV_CELL_TRAVERSAL_ENABLED)) {
            cell = closest;
        } else {
            cell = bb_pf_get_nav_cell(pos, vehicle_pfg, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, &adjusted);
        }
    }
    if (cell != nullptr) {
        adjusted = nav_cell_force_point_to_cell(cell, pos);
    }
    return cell;
}

// 0x00B19260 (:3182) LTCG: output EAX
static void get_raw_path(raw_path& output, const vector& start_pos, const vector& end_pos, uint64_t start_nav_cell_hint_handle, uint64_t end_nav_cell_hint_handle, bool vehicle_pfg, bool out_of_chunk, unsigned short ignore_set, bool use_doors, bool encourage_edf_spread, bool scripted_move, unsigned int node_to_avoid)
{
    output.path = nullptr;
    output.adjusted_start_pos.zero();
    output.start_nav_cell = nullptr;
    output.adjusted_end_pos.zero();
    output.out_of_chunk = false;
    output.end_nav_cell = nullptr;
    output.result = PR_UNKNOWN;
    // Magic number: about 3 cm.
    if (vec_dist_squared(start_pos, end_pos) < 0.001f) {
        return;
    }
    output.start_nav_cell = bb_pf_cell_from_raw_hint(start_nav_cell_hint_handle, start_pos, vehicle_pfg, output.adjusted_start_pos);
    // Magic number: the nearest mesh within 2 m will do for a start.
    if (output.start_nav_cell == nullptr
        && (!bb_pf_find_closest_point_on_nav_mesh(start_pos, output.adjusted_start_pos, output.start_nav_cell, false) || 4.0f <= vec_xz_dist_squared(output.adjusted_start_pos, start_pos))) {
        if (!out_of_chunk) {
            output.result = PR_ERROR_START_NAVCELL;
            return;
        }
        build_out_of_chunk_path(output, start_pos, end_pos);
        output.result = PR_OUT_OF_CHUNK;
        return;
    }
    output.end_nav_cell = bb_pf_cell_from_raw_hint(end_nav_cell_hint_handle, end_pos, vehicle_pfg, output.adjusted_end_pos);
    if (output.end_nav_cell == nullptr) {
        if (!out_of_chunk) {
            output.result = PR_ERROR_END_NAVCELL;
            return;
        }
        build_out_of_chunk_path(output, start_pos, end_pos);
        output.result = PR_OUT_OF_CHUNK;
        return;
    }
    if (output.adjusted_start_pos == output.adjusted_end_pos) {
        output.result = PR_ERROR_DEGENERATE;
        return;
    }
    float max_dist = get_max_search_distance(output.start_nav_cell, output.adjusted_start_pos, output.end_nav_cell, output.adjusted_end_pos, vehicle_pfg, scripted_move);
    Nav_heap.start_session();
    if (get_raw_path_internal(output, &Nav_heap, max_dist, ignore_set, encourage_edf_spread, use_doors, node_to_avoid)) {
        build_raw_path(&output.path, output.start_nav_cell, output.adjusted_start_pos);
        output.result = PR_OK;
    }
    Nav_heap.end_session();
}

// A cell hint resolved the way the path calculations inline it, for a vehicle graph.
static nav_cell* bb_pf_vehicle_cell_for_navpoint(const navpoint* navp)
{
    vector adjusted;
    return bb_pf_get_nav_cell_from_hint(&adjusted, navp->nav_cell_ptr, navp->pos, true, nullptr, false, false, NAV_CELL_TRAVERSAL_ENABLED);
}

// 0x00B19630 (:3311) LTCG: output ECX, params EDX
static void get_raw_path(raw_path& output, const pf_calc_params& params)
{
    const signed char TYPE_ROAD_JUMP = 6;
    const signed char TYPE_ROAD_JUMP_DEST = 7;
    const signed char TYPE_ROAD_JUMP_START = 8;
    const signed char TYPE_ROAD_BRIDGE = 22;
    // Magic number: the ray that tells a hole in the mesh from a place the world has ground.
    const float NO_GROUND_RAY_REACH = 150.0f;
    output.path = nullptr;
    output.adjusted_start_pos.zero();
    output.start_nav_cell = nullptr;
    output.adjusted_end_pos.zero();
    output.end_nav_cell = nullptr;
    output.result = PR_UNKNOWN;
    output.out_of_chunk = false;
    const pf_calc_point& start_point = params.points[0];
    const pf_calc_point& end_point = params.points[1];
    if (start_point.pos == end_point.pos) {
        output.result = PR_ERROR_DEGENERATE;
        return;
    }
    bool vehicle_pfg = params.flags.vehicle_pfg != 0;
    bool out_of_chunk = params.flags.allow_out_of_chunk != 0;
    bool force_to_cell = params.flags.force_to_nav_cell != 0;
    bool encourage_spread = params.flags.encourage_edf_spread != 0;
    nav_cell_traversal_rule rule = NAV_CELL_TRAVERSAL_ENABLED;
    if (params.flags.use_disabled_cells != 0) {
        rule = NAV_CELL_TRAVERSAL_ALL;
    } else if (params.flags.use_light_structure_disabled_cells != 0) {
        rule = NAV_CELL_TRAVERSAL_ENABLED_PLUS_LIGHT;
    }
    uint64_t found_handle;
    nav_cell* bridge_cell = nullptr;
    vector bridge_pos = ZERO_VECTOR;
    if (!params.on_bridge) {
        output.start_nav_cell = bb_pf_get_nav_cell_from_hint(&output.adjusted_start_pos, nav_cell_from_handle(start_point.nav_cell_hint), start_point.pos, vehicle_pfg, &found_handle, false, force_to_cell, rule);
        if (output.start_nav_cell == nullptr) {
            if (out_of_chunk) {
                // Original quirk kept: this path leaves result at PR_UNKNOWN.
                build_out_of_chunk_path(output, start_point.pos, end_point.pos);
                return;
            }
            output.result = PR_ERROR_START_NAVCELL;
            return;
        }
    } else {
        const navpoint* bridge_navp = World.object_from_handle<navpoint>(params.bridge_navpoint_handle);
        if (bridge_navp == nullptr) {
            output.result = PR_ERROR_START_NAVCELL;
            return;
        }
        output.start_nav_cell = bridge_navp->nav_cell_ptr;
        output.adjusted_start_pos = bridge_navp->pos;
        if (bridge_navp->links[0] != nullptr) {
            bridge_cell = bridge_navp->links[0]->nav_cell_ptr;
            bridge_pos = bridge_navp->links[0]->pos;
        }
        if (output.start_nav_cell == nullptr) {
            output.result = PR_ERROR_START_NAVCELL;
            return;
        }
    }
    output.end_nav_cell = bb_pf_get_nav_cell_from_hint(&output.adjusted_end_pos, nav_cell_from_handle(end_point.nav_cell_hint), end_point.pos, vehicle_pfg, &found_handle, false, force_to_cell, rule);
    if (output.end_nav_cell == nullptr) {
        if (out_of_chunk) {
            bool has_ground = false;
            if (params.flags.allow_out_of_chunk_no_raycast == 0) {
                raycast_input in;
                in.radius = 0.0f;
                in.m_filter_info = COLLIDE_RAY_DEFAULT;
                in.start = vector(end_point.pos.x, end_point.pos.y + NO_GROUND_RAY_REACH, end_point.pos.z);
                in.end = vector(end_point.pos.x, end_point.pos.y - NO_GROUND_RAY_REACH, end_point.pos.z);
                in.filter_params = nullptr;
                in.filter_callback = nullptr;
                in.m_hit_collector_type = (raycast_input::hit_collector_types)0;
                in.m_flags = 0;
                raycast_output out;
                has_ground = havok_raycast_against_static_level_only(in, out);
            }
            if (!has_ground) {
                build_out_of_chunk_path(output, start_point.pos, end_point.pos);
                return;
            }
        }
        output.result = PR_ERROR_END_NAVCELL;
        return;
    }
    if (params.flags.allow_dest_in_detour_hull == 0 && params.flags.ignore_detour_hulls == 0) {
        const compound_hull* end_hull = nav_cell_detour_is_point_in_compound_hull(output.adjusted_end_pos, output.end_nav_cell);
        if (end_hull != nullptr && !nav_cell_detour_is_point_in_compound_hull(vector2(output.adjusted_start_pos.x, output.adjusted_start_pos.z), end_hull)) {
            output.result = PR_ERROR_DETOUR_HULL;
            return;
        }
    }
    if (output.adjusted_start_pos == output.adjusted_end_pos) {
        output.result = PR_ERROR_DEGENERATE;
        return;
    }
    float max_search_dist = bb_pf_cells_max_search_distance(output.start_nav_cell, output.adjusted_start_pos, output.end_nav_cell, output.adjusted_end_pos, vehicle_pfg);
    Nav_heap.start_session();
    bool found = false;
    bool failed = !nav_cell_query_for_path(output.end_nav_cell, nullptr, output.adjusted_end_pos, 0.0f, output.adjusted_start_pos, &Nav_heap, false, 0);
    while (!failed && Nav_heap.num_nav_cells > 0) {
        nav_cell* cell = Nav_heap.remove_best_cell();
        if (params.on_bridge) {
            if (cell == output.start_nav_cell) {
                found = true;
                break;
            }
            if (cell == bridge_cell) {
                output.start_nav_cell = bridge_cell;
                output.adjusted_start_pos = bridge_pos;
                found = true;
                break;
            }
        } else if (cell == output.start_nav_cell) {
            found = true;
            break;
        }
        nav_heap_cell_data* data = Nav_heap.get_cell_data(cell);
        vector arrival = data->arrival_edge < 0 ? output.adjusted_end_pos : data->arrival_pos;
        for (int edge = 0; edge < 3 && !failed; edge++) {
            if (edge == data->arrival_edge) {
                continue;
            }
            nav_cell* adjacent = cell->adjacent_cells[edge].to_pointer();
            if (adjacent == nullptr) {
                continue;
            }
            if ((adjacent->flags & NAV_CELL_FLAG_DISABLED) != 0
                && !((adjacent->flags & NAV_CELL_FLAG_LIGHT) != 0 && params.flags.use_light_structure_disabled_cells != 0)
                && params.flags.use_disabled_cells == 0) {
                continue;
            }
            const nav_heap_cell_data* adjacent_data = Nav_heap.get_cell_data(adjacent);
            if ((adjacent_data != nullptr && !adjacent_data->is_open) || (adjacent->flags & NAV_CELL_FLAG_BLOCKED) != 0) {
                continue;
            }
            vector midpoint;
            nav_cell_get_edge_midpoint(&midpoint, cell, edge);
            float step = vec_dist(midpoint, arrival);
            if (0.0f <= step) {
                if (encourage_spread) {
                    step += rfg_pf_edf_spread_traversal_boost(adjacent);
                }
                if (adjacent == output.start_nav_cell) {
                    step += vec_dist(midpoint, output.adjusted_start_pos);
                }
                float cost = data->arrival_cost + step;
                if (cost <= max_search_dist && !nav_cell_query_for_path(adjacent, cell, midpoint, cost, output.adjusted_start_pos, &Nav_heap, false, 0)) {
                    failed = true;
                }
            }
        }
        // Road jumps and bridges link cells that have no shared edge.
        navpoint* navp = World.object_from_handle<navpoint>(cell->navp);
        for (unsigned short which = 0; navp != nullptr && !failed; which++) {
            if (navp->type == TYPE_ROAD_JUMP_DEST) {
                float cost = vec_dist(arrival, navp->pos) + data->arrival_cost;
                for (int i = 0; i < navp->num_links && !failed; i++) {
                    const navpoint* jump = navp->links[i];
                    if (jump == nullptr || jump->type != TYPE_ROAD_JUMP) {
                        continue;
                    }
                    for (int j = 0; j < jump->num_links && !failed; j++) {
                        const navpoint* jump_start = jump->links[j];
                        if (jump_start == nullptr || jump_start->type != TYPE_ROAD_JUMP_START) {
                            continue;
                        }
                        nav_cell* jump_cell = bb_pf_vehicle_cell_for_navpoint(jump_start);
                        if (jump_cell != nullptr) {
                            float total = vec_dist(jump->pos, jump_start->pos) + vec_dist(navp->pos, jump->pos) + cost;
                            failed = !nav_cell_query_for_path(jump_cell, cell, jump_start->pos, total, output.adjusted_start_pos, &Nav_heap, true, which);
                        }
                    }
                }
            } else if (navp->type == TYPE_ROAD_BRIDGE) {
                float cost = vec_dist(arrival, navp->pos) + data->arrival_cost;
                for (int i = 0; i < navp->num_links && !failed; i++) {
                    const navpoint* link = navp->links[i];
                    if (link == nullptr || link->type != TYPE_ROAD_BRIDGE) {
                        continue;
                    }
                    nav_cell* link_cell = bb_pf_vehicle_cell_for_navpoint(link);
                    if (link_cell != nullptr) {
                        failed = !nav_cell_query_for_path(link_cell, cell, link->pos, vec_dist(navp->pos, link->pos) + cost, output.adjusted_start_pos, &Nav_heap, true, which);
                    }
                }
            }
            navp = World.object_from_handle<navpoint>(navp->get_next_navpoint());
        }
    }
    if (!found) {
        output.result = PR_ERROR_UNKNOWN;
        Nav_heap.end_session();
        return;
    }
    build_raw_path(&output.path, output.start_nav_cell, output.adjusted_start_pos, &Nav_heap);
    if (params.on_bridge) {
        path_node* node = bb_pf_get_free_path_node();
        if (node != nullptr) {
            node->pos = start_point.pos;
            node->flags.do_not_remove = true;
            node->flags.bridge = true;
            node->nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
            vector offset = get_bridge_offset(output.path->pos, node->pos);
            const navpoint* bridge_navp = World.object_from_handle<navpoint>(params.bridge_navpoint_handle);
            if (bridge_navp != nullptr) {
                path_node* first = output.path;
                if (offset != ZERO_VECTOR && first->nav_cell_handle != 0xFFFFFFFFFFFFFFFFull) {
                    float reach = bridge_navp->radius * 0.5f;
                    vector target(first->pos.x + reach * offset.x, first->pos.y + offset.y * reach, first->pos.z + offset.z * reach);
                    const nav_cell* end_cell;
                    nav_cell_find_longest_line_of_sight_no_detour(first->pos, end_cell, first->pos, nav_cell_from_handle(first->nav_cell_handle), target, false, nullptr);
                    first->nav_cell_handle = nav_cell_get_handle(end_cell);
                }
            }
            // The node goes in front of the path.
            bb_pf_path_append(&output.path, node);
            output.path = node;
        }
    }
    output.result = PR_OK;
    Nav_heap.end_session();
}

// 0x00B1A680 (:3786) LTCG: nav_cell_hints ECX
static path_node* get_path(const vector* points, int num_points, const uint64_t* nav_cell_hints, bool vehicle_pfg, bool out_of_chunk, unsigned short ignore_set, bool stuck, bool use_doors, bool encourage_edf_spread, bool scripted_move, unsigned int node_to_avoid)
{
    // Magic number: rfg.exe's stack arrays hold 10 points. stuck is not read.
    vector local_points[10];
    uint64_t hints[10];
    memcpy(local_points, points, num_points * sizeof(vector));
    if (nav_cell_hints == nullptr) {
        for (int i = 0; i < num_points; i++) {
            hints[i] = 0xFFFFFFFFFFFFFFFFull;
        }
    } else {
        memcpy(hints, nav_cell_hints, num_points * sizeof(uint64_t));
    }
    path_node* total = nullptr;
    for (int i = 0; i < num_points - 1; i++) {
        raw_path raw;
        get_raw_path(raw, local_points[i], local_points[i + 1], hints[i], hints[i + 1], vehicle_pfg, out_of_chunk, ignore_set, use_doors, encourage_edf_spread, scripted_move, node_to_avoid);
        if (raw.path == nullptr) {
            bb_pf_free_path(&total);
            return nullptr;
        }
        if (!raw.out_of_chunk) {
            pf_calc_flags flags(false);
            flags.pre_detour_simplification = 1;
            flags.ignore_detour_hulls = 1;
            simplify_nav_path_by_los(&raw.path, flags);
            raw.path->flags.optimized = true;
            // Magic number: FLT_MAX: the whole path in reach.
            optimize_node_positions(raw.path, 1, 0, false, 3.4028235e+38f);
        }
        if (total == nullptr) {
            total = raw.path;
        } else {
            path_node* tail = total->prev;
            if (raw.out_of_chunk) {
                if (raw.path->nav_cell_handle == 0xFFFFFFFFFFFFFFFFull) {
                    raw.path->nav_cell_handle = tail->nav_cell_handle;
                }
                if (tail->nav_cell_handle == 0xFFFFFFFFFFFFFFFFull) {
                    tail->nav_cell_handle = raw.path->nav_cell_handle;
                }
            }
            // The segment's first node repeats the end of the last one.
            bb_pf_free_path_node(&raw.path, raw.path);
            bb_pf_path_splice(&total, raw.path);
        }
        local_points[i + 1] = raw.adjusted_end_pos;
        hints[i + 1] = nav_cell_get_handle(raw.end_nav_cell);
    }
    return total;
}

// 0x00B1AA40 (:3946) LTCG: start_pos ECX, end_pos EAX
static path_node* get_path(const vector& start_pos, const vector& end_pos, uint64_t start_nav_cell_hint_handle, uint64_t end_nav_cell_hint_handle, bool vehicle_pfg, bool out_of_chunk, unsigned short ignore_set, bool stuck, bool use_doors, bool encourage_edf_spread, bool scripted_move, unsigned int node_to_avoid)
{
    vector points[2] = { start_pos, end_pos };
    uint64_t hints[2] = { start_nav_cell_hint_handle, end_nav_cell_hint_handle };
    return get_path(points, 2, hints, vehicle_pfg, out_of_chunk, ignore_set, stuck, use_doors, encourage_edf_spread, scripted_move, node_to_avoid);
}

// 0x00B02FA0 (:3964) LTCG: pos EAX
static int get_nearby_repulsor_edges(repulsor_edge* repulsor_edges, nav_cell* start_nav_cell, const vector& pos, float max_dist)
{
    // Magic numbers: at most 16 edges, a search over at most 64 cells. Edges with no neighbour push away.
    const int MAX_EDGES = 16;
    const int MAX_CELLS = 64;
    const nav_cell* queue[MAX_CELLS] = {};
    vector2 pos_2d(pos.x, pos.z);
    float max_dist_sq = max_dist * max_dist;
    int num_edges = 0;
    int num_queued = 1;
    queue[0] = start_nav_cell;
    for (int index = 0; index < num_queued && num_edges < MAX_EDGES; index++) {
        const nav_cell* cell = queue[index];
        for (int edge = 0; edge < 3 && num_edges < MAX_EDGES; edge++) {
            const vector* vert0 = nav_cell_get_vertex(cell, edge, nullptr);
            const vector* vert1 = nav_cell_get_vertex(cell, (edge + 1) % 3, nullptr);
            vector2 edge0(vert0->x, vert0->z);
            vector2 edge1(vert1->x, vert1->z);
            vector2 rvec(edge1.y - edge0.y, -(edge1.x - edge0.x));
            const nav_cell* adjacent = cell->adjacent_cells[edge].to_pointer();
            vector2 closest;
            if (adjacent == nullptr) {
                if (0.0f <= (pos_2d.y - edge0.y) * rvec.y + (pos_2d.x - edge0.x) * rvec.x) {
                    vec2_find_closest_point_on_linesegment(closest, pos_2d, edge0, edge1);
                    float dist_sq = vec2_dist_squared(pos_2d, closest);
                    if (dist_sq <= max_dist_sq) {
                        repulsor_edge& out = repulsor_edges[num_edges++];
                        out.edge_vert0 = edge0;
                        out.edge_vert1 = edge1;
                        out.edge_rvec = rvec;
                        out.edge_rvec.normalize();
                        out.closest_point = closest;
                        out.dist_squared_to_closest_point = dist_sq;
                    }
                }
            } else if (num_queued < MAX_CELLS) {
                vec2_find_closest_point_on_linesegment(closest, pos_2d, edge0, edge1);
                if (vec2_dist_squared(pos_2d, closest) <= max_dist_sq) {
                    int i = 0;
                    while (i < num_queued && queue[i] != adjacent) {
                        i++;
                    }
                    if (i >= num_queued) {
                        queue[num_queued++] = adjacent;
                    }
                }
            }
        }
    }
    return num_edges;
}

static ai_plan_conditions DISABLE_REPULSOR_GOAL_CONDITIONS; // 0x03610D60

// 0x00AF4290 (:4052) LTCG: oriented_bbox ECX, repulsor ESI, dest EDI
static bool maybe_ignore_repulsor(human* hp, const vector& pos, const vector& dest, object* repulsor, const repulsor_oriented_bbox& oriented_bbox, bool recollected_repulsors)
{
    switch (repulsor->get_obj_type()) {
    case OT_HUMAN:
        if (hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC) {
            ai_system* ai = ((npc*)hp)->get_ai();
            if (ai->get_target_handle() == repulsor->get_handle()) {
                if (($S5 & 1) == 0) {
                    $S5 |= 1;
                    // Magic numbers: the goal condition bits while attacking hand to hand.
                    DISABLE_REPULSOR_GOAL_CONDITIONS = ai_plan_conditions(0x6A) | ai_plan_conditions(0x65) | ai_plan_conditions(0x4F);
                }
                if (ai->planner.is_pursuing_condition(DISABLE_REPULSOR_GOAL_CONDITIONS)) {
                    return true;
                }
            }
            if (hp->hflags.on_mover == 0 && nav_cell_detour_has_valid_convex_hull(((human*)repulsor)->nav_cell_detour_request_handle)) {
                return true;
            }
        }
        break;
    case OT_ITEM:
    case 5:
        break;
    case OT_MOVER:
        if (hp->hflags.on_mover == 0 && nav_cell_detour_has_valid_convex_hull(((object_mover*)repulsor)->nav_cell_detour_request_handle)) {
            return true;
        }
        break;
    case OT_VEHICLE:
        if (hp->hflags.on_mover == 0 && nav_cell_detour_has_valid_convex_hull(((vehicle*)repulsor)->m_nav_cell_detour_request_handle)) {
            return true;
        }
        break;
    case OT_NAVPOINT: {
        vector to_repulsor = repulsor->pos - pos;
        vector from_repulsor = dest - repulsor->pos;
        float dist = to_repulsor.mag_and_normalize_safe();
        from_repulsor.normalize_safe();
        if (from_repulsor.y * to_repulsor.y + from_repulsor.x * to_repulsor.x + from_repulsor.z * to_repulsor.z < 0.0f && ((navpoint*)repulsor)->radius < dist) {
            return true;
        }
        break;
    }
    default:
        return true;
    }
    // Magic numbers: half a metre around the destination; a destination this close to the repulsor keeps it.
    if (repulsor->get_obj_type() != OT_HUMAN && ix_sphere_in_box_oriented(dest, 0.5f, oriented_bbox.pos, oriented_bbox.orient, oriented_bbox.size)) {
        return true;
    }
    vector to_dest = dest - repulsor->pos;
    if (0.0f <= (dest.y - pos.y) * to_dest.y + (dest.x - pos.x) * to_dest.x + (dest.z - pos.z) * to_dest.z || to_dest.x * to_dest.x + to_dest.y * to_dest.y + to_dest.z * to_dest.z <= 0.3f) {
        return false;
    }
    return true;
}

// 0x00AEAC30 (:4180) LTCG: oriented_bbox0 EDX, oriented_bbox1 EAX
static bool maybe_merge_repulsors(const repulsor_oriented_bbox& oriented_bbox0, const repulsor_oriented_bbox& oriented_bbox1)
{
    // Magic number: both boxes grow by 1 m: a human cannot pass between boxes closer than that.
    vector size0(oriented_bbox0.size.x + 1.0f, oriented_bbox0.size.y + 1.0f, oriented_bbox0.size.z + 1.0f);
    vector size1(oriented_bbox1.size.x + 1.0f, oriented_bbox1.size.y + 1.0f, oriented_bbox1.size.z + 1.0f);
    return do_oriented_bounding_boxes_intersect(size0, oriented_bbox0.pos, oriented_bbox0.orient, size1, oriented_bbox1.pos, oriented_bbox1.orient);
}

// 0x00AEACF0 (:4221) LTCG: sphere_pos0 ESI, sphere_pos1 EDI
static void merge_bounding_spheres(vector& sphere_pos0, float& sphere_radius0, const vector& sphere_pos1, float sphere_radius1)
{
    vector dir = sphere_pos1 - sphere_pos0;
    float dist = dir.mag_and_normalize_safe();
    if (sphere_radius0 >= dist + sphere_radius1) {
        return;
    }
    if (sphere_radius0 + dist <= sphere_radius1) {
        sphere_pos0 = sphere_pos1;
        sphere_radius0 = sphere_radius1;
        return;
    }
    vector far0(sphere_pos0.x - dir.x * sphere_radius0, sphere_pos0.y - sphere_radius0 * dir.y, sphere_pos0.z - dir.z * sphere_radius0);
    vector far1(sphere_pos1.x + dir.x * sphere_radius1, dir.y * sphere_radius1 + sphere_pos1.y, sphere_pos1.z + dir.z * sphere_radius1);
    vector span = far1 - far0;
    sphere_pos0 = vector((far1.x + far0.x) * 0.5f, (far1.y + far0.y) * 0.5f, (far1.z + far0.z) * 0.5f);
    sphere_radius0 = sqrtf(span.x * span.x + span.y * span.y + span.z * span.z) * 0.5f;
}

// 0x00AFA180 (:4251) LTCG: repulsor ESI
static void calculate_compound_repulsor_bounding_sphere(compound_repulsor* repulsor, const human* hp)
{
    // The spheres are flattened to the human's height: only xz distances matter to the repulsion.
    vector positions[8];
    float radii[8];
    for (int i = 0; i < repulsor->num_objects; i++) {
        positions[i] = repulsor->objects[i].oriented_bbox.pos;
        radii[i] = bb_pf_get_repulsor_xz_radius(repulsor->objects[i].objp, repulsor->objects[i].oriented_bbox);
    }
    repulsor->pos = positions[0];
    repulsor->pos.y = hp->pos.y;
    repulsor->radius = radii[0];
    for (int i = 1; i < repulsor->num_objects; i++) {
        vector flat = positions[i];
        flat.y = hp->pos.y;
        merge_bounding_spheres(repulsor->pos, repulsor->radius, flat, radii[i]);
    }
}

// 0x00AFA570 (:4290) LTCG: num_repulsor_handles EAX, repulsor_handles ECX
static int create_compound_repulsors(compound_repulsor* compound_repulsors, human* hp, const vector& pos, const vector& dest, int num_repulsor_handles, unsigned int* repulsor_handles, bool recollected_repulsors)
{
    // Magic number: an rfg mover whose bbox diagonal is over 6 m (36 squared) is a building part, not a repulsor.
    const float MAX_REPULSOR_MOVER_DIAGONAL_SQ = 36.0f;
    int num = 0;
    for (int i = 0; i < num_repulsor_handles; i++) {
        object* objp = World.object_from_handle(repulsor_handles[i]);
        if (objp == nullptr) {
            continue;
        }
        if (objp->get_obj_type() == OT_MOVER) {
            unsigned int handle = objp->havok_handle;
            if (handle == 0xFFFFFFFF) {
                continue;
            }
            if (!Havok_filter->isCollisionEnabled(COLLIDE_NPC, havok_body_get_collision_type(handle))) {
                continue;
            }
            if (objp->get_sub_type() == OT_SUB_MOVER_RFG) {
                if ((int)((rfg_mover*)objp)->get_move_type() == 0) {
                    continue;
                }
                vector bmin, bmax;
                objp->get_bbox(bmin, bmax);
                vector diagonal = bmax - bmin;
                if (diagonal.x * diagonal.x + diagonal.y * diagonal.y + diagonal.z * diagonal.z > MAX_REPULSOR_MOVER_DIAGONAL_SQ) {
                    continue;
                }
            } else if (havok_body_get_collision_type(handle) == COLLIDE_RFG_WORLD_ONLY) {
                continue;
            }
        }
        repulsor_oriented_bbox oriented_bbox;
        if (bb_pf_get_repulsor_oriented_bounding_box(&oriented_bbox, objp) && !maybe_ignore_repulsor(hp, pos, dest, objp, oriented_bbox, recollected_repulsors)) {
            compound_repulsors[num].num_objects = 1;
            compound_repulsors[num].objects[0].objp = objp;
            compound_repulsors[num].objects[0].oriented_bbox = oriented_bbox;
            num++;
        }
    }
    // Merge those near enough that a human cannot pass between them. Original bug kept: nothing checks the 8 object limit.
    for (int i = 0; i < num - 1; i++) {
        compound_repulsor& first = compound_repulsors[i];
        for (int j = i + 1; j < num; j++) {
            compound_repulsor& second = compound_repulsors[j];
            bool merge = false;
            for (int a = 0; a < first.num_objects && !merge; a++) {
                const object* objp = first.objects[a].objp;
                if (objp != nullptr && (objp->get_sub_type() == 1 || objp->get_obj_type() == OT_NAVPOINT)) {
                    continue;
                }
                for (int b = 0; b < second.num_objects; b++) {
                    if (maybe_merge_repulsors(first.objects[a].oriented_bbox, second.objects[b].oriented_bbox)) {
                        merge = true;
                        break;
                    }
                }
            }
            if (merge) {
                for (int b = 0; b < second.num_objects; b++) {
                    first.objects[first.num_objects++] = second.objects[b];
                }
                int following = num - j - 1;
                if (following > 0) {
                    memmove(&compound_repulsors[j], &compound_repulsors[j + 1], following * sizeof(compound_repulsor));
                }
                num--;
                j--;
            }
        }
    }
    for (int i = 0; i < num; i++) {
        calculate_compound_repulsor_bounding_sphere(&compound_repulsors[i], hp);
    }
    return num;
}

// 0x00AEAFB0 (:4599) LTCG: repulsor ESI, r_edge EDI
static bool maybe_expand_compound_repulsor_to_include_repulsor_edge(compound_repulsor* repulsor, const repulsor_edge* r_edge)
{
    vector2 center(repulsor->pos.x, repulsor->pos.z);
    vector2 closest;
    vec2_find_closest_point_on_linesegment(closest, center, r_edge->edge_vert0, r_edge->edge_vert1);
    vector2 away(center.x - closest.x, center.y - closest.y);
    float dist = sqrtf(away.x * away.x + away.y * away.y);
    // Magic number: a gap under 1 m between the sphere and the wall cannot be walked.
    if (dist - repulsor->radius <= 1.0f && 0.0f < r_edge->edge_rvec.y * away.y + r_edge->edge_rvec.x * away.x) {
        repulsor->pos.x = closest.x;
        repulsor->pos.z = closest.y;
        repulsor->radius += dist;
        return true;
    }
    return false;
}

// 0x00AEB0E0 (:4628)
static void maybe_expand_player_repulsor(int num_compound_repulsors, compound_repulsor* compound_repulsors)
{
    if (Player_num_nearby_repulsor_edges <= 0) {
        return;
    }
    player* local_player = player_get_local_player();
    for (int i = 0; i < num_compound_repulsors; i++) {
        compound_repulsor& repulsor = compound_repulsors[i];
        for (int j = 0; j < repulsor.num_objects; j++) {
            if ((object*)repulsor.objects[j].objp == (object*)local_player) {
                for (int edge = 0; edge < Player_num_nearby_repulsor_edges; edge++) {
                    maybe_expand_compound_repulsor_to_include_repulsor_edge(&repulsor, &Player_nearby_repulsor_edges[edge]);
                }
                return;
            }
        }
    }
}

// 0x00AEB290 (:4669) LTCG: pos EAX
static bool calculate_repulsion_vector(vector* repulsion_vector, human* hp, const vector& pos, compound_repulsor* repulsor, const vector& dir_to_dest_xz)
{
    // Magic numbers: half a metre of body width, no repulsion from over 10 m, the strength capped at 1000.
    const float BODY_RADIUS = 0.5f;
    const float MAX_REPULSOR_GAP = 10.0f;
    const float MAX_REPULSION = 1000.0f;
    vector to_repulsor(repulsor->pos.x - pos.x, 0.0f, repulsor->pos.z - pos.z);
    if (to_repulsor.x != ZERO_VECTOR.x || ZERO_VECTOR.y != 0.0f || to_repulsor.z != ZERO_VECTOR.z) {
        float gap = to_repulsor.mag_and_normalize() - BODY_RADIUS - repulsor->radius;
        bool inside = gap < 0.0f;
        if (inside) {
            gap = 0.0f;
        }
        if (gap <= MAX_REPULSOR_GAP && 0.0f <= dir_to_dest_xz.x * to_repulsor.x + dir_to_dest_xz.y * to_repulsor.y + dir_to_dest_xz.z * to_repulsor.z) {
            vector ahead(dir_to_dest_xz.x + pos.x, pos.y + dir_to_dest_xz.y, dir_to_dest_xz.z + pos.z);
            vector closest;
            float factor;
            if (repulsor->radius * repulsor->radius <= vec_find_closest_dist_squared_to_line(repulsor->pos, pos, ahead, &closest)) {
                vector along(closest.x - pos.x, 0.0f, closest.z - pos.z);
                if (along.is_null()) {
                    factor = 0.0f;
                } else {
                    along.normalize();
                    factor = dir_to_dest_xz.y * along.y + along.x * dir_to_dest_xz.x + dir_to_dest_xz.z * along.z;
                    if (factor < 0.0f) {
                        factor = 0.0f;
                    }
                }
            } else {
                factor = 1.0f;
            }
            float speed = hp->max_speed;
            if (hp->max_speed - hp->actual_velocity_mag < 0.0f) {
                speed = hp->actual_velocity_mag;
            }
            float strength = (repulsor->radius * speed * factor) / gap;
            if (fl_is_nan(strength) || MAX_REPULSION < strength) {
                strength = MAX_REPULSION;
            }
            repulsion_vector->x = (to_repulsor.z * UP_VECTOR.y - UP_VECTOR.z * to_repulsor.y) * strength;
            repulsion_vector->y = (UP_VECTOR.z * to_repulsor.x - to_repulsor.z * UP_VECTOR.x) * strength;
            repulsion_vector->z = (UP_VECTOR.x * to_repulsor.y - UP_VECTOR.y * to_repulsor.x) * strength;
            if (vec_delta_ang(to_repulsor, dir_to_dest_xz, UP_VECTOR) < 0.0f) {
                repulsion_vector->x *= -1.0f;
                repulsion_vector->y *= -1.0f;
                repulsion_vector->z *= -1.0f;
            }
            return inside;
        }
    }
    repulsion_vector->zero();
    return false;
}

// 0x00AEB750 (:4883)
pf_calc_flags::pf_calc_flags(bool _vehicle_pfg)
{
    memset(this, 0, sizeof(*this));
    vehicle_pfg = _vehicle_pfg ? 1 : 0;
}

// 0x00AEB770 (:4895)
pf_calc_point::pf_calc_point(const vector& _pos, uint64_t _nav_cell_hint, unsigned int _zone_handle)
    : pos(_pos), nav_cell_hint(_nav_cell_hint), zone_handle(_zone_handle)
{
}

// 0x00AEC090
pf_calc_point::pf_calc_point(const pf_calc_point& __that)
    : pos(__that.pos), nav_cell_hint(__that.nav_cell_hint), zone_handle(__that.zone_handle)
{
}

// 0x00AEB7B0 (:4910)
pf_calc_params::pf_calc_params(int _num_points, const pf_calc_point* _points, const pf_calc_flags& _flags, bool _use_road, bool _use_special_roads, bool _flying, bool _on_bridge, unsigned int _bridge_navpoint_handle)
    : num_points(_num_points), points(_points), flags(_flags), use_road(_use_road), flying(_flying), on_bridge(_on_bridge),
      use_special_roads(_use_special_roads), cap_max_points(true), ignore_road_directionality(false), bridge_navpoint_handle(_bridge_navpoint_handle)
{
}

// 0x00AEA030
path_node::path_node()
{
}

// 0x00AEAC20
raw_path::raw_path()
{
}

// 0x00AEA040
repulsor_edge::repulsor_edge()
{
}

// 0x00A0B8F0
repulsor_oriented_bbox::repulsor_oriented_bbox()
{
}

// 0x00A31410
repulsor_object::repulsor_object()
{
}

// 0x00A56A10
compound_repulsor::compound_repulsor()
{
}

// 0x00AEAB20
edge_constraint_info::edge_constraint_info()
{
}

// 0x00AEB900
subpiece_data::subpiece_data()
{
}

// 0x00B08D50 (:5076)
void rfg_pf_instantiate(obj_zone* zonep)
{
    if (zonep->zone_pfg.pfg != nullptr && zonep->zone_pfg.vehicle_pfg != nullptr) {
        zonep->zone_pfg.pfg_havok_handle = havok_create_navmesh_rb(zonep->zone_pfg.pfg_shape, zonep, ZERO_VECTOR, IDENTITY_MATRIX, false);
        zonep->zone_pfg.vehicle_pfg_havok_handle = havok_create_navmesh_rb(zonep->zone_pfg.vehicle_pfg_shape, zonep, ZERO_VECTOR, IDENTITY_MATRIX, true);
        stitch_zone(zonep);
    }
}

// 0x00B08DC0 (:5107)
void rfg_pf_delete(obj_zone* zonep)
{
    unstitch_zone(zonep);
    if (zonep->zone_pfg.pfg_havok_handle != 0xFFFFFFFF) {
        havok_body_free(zonep->zone_pfg.pfg_havok_handle, false, true, true);
        zonep->zone_pfg.pfg_havok_handle = 0xFFFFFFFF;
    }
    if (zonep->zone_pfg.vehicle_pfg_havok_handle != 0xFFFFFFFF) {
        havok_body_free(zonep->zone_pfg.vehicle_pfg_havok_handle, false, true, true);
        zonep->zone_pfg.vehicle_pfg_havok_handle = 0xFFFFFFFF;
    }
    if (zonep->zone_pfg.pfg != nullptr) {
        nav_cell_detour_unload_zone(zonep);
    }
}

// 0x00B1F2C0 (:5144)
void rfg_pf_do_frame()
{
    nav_cell_detour_do_frame();
    player* local_player = player_get_local_player();
    vector adjusted;
    nav_cell* ncp = bb_pf_get_nav_cell_from_hint(&adjusted, nav_cell_from_handle(Player_nav_cell_handle), local_player->pos, false, nullptr, false, false, NAV_CELL_TRAVERSAL_ENABLED);
    adjusted.y = local_player->pos.y;
    if (ncp != nullptr) {
        Player_nav_cell_handle = nav_cell_get_handle(ncp);
        Player_adjusted_pos = adjusted;
        // Magic number: 1 m around the player.
        Player_num_nearby_repulsor_edges = get_nearby_repulsor_edges(Player_nearby_repulsor_edges, ncp, Player_adjusted_pos, 1.0f);
    }
}

// The nav mesh a spherecast hit belongs to, or null.
static const bb_pfg_layer* bb_pf_hit_layer(const raycast_output::hit_info& hit, bool vehicle_pfg)
{
    if (hit.hit_object_handle == 0xFFFFFFFF) {
        return nullptr;
    }
    object* objp = World.object_from_handle(hit.hit_object_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_ZONE || ((obj_zone*)objp)->zone_pfg.pfg == nullptr) {
        return nullptr;
    }
    const obj_zone* zone = (obj_zone*)objp;
    return vehicle_pfg ? zone->zone_pfg.vehicle_pfg : zone->zone_pfg.pfg;
}

// 0x00B0D8F0 (:5186)
static raycast_output::hit_info* get_best_navmesh_spherecast_hit(const vector& pos, raycast_output& out, bool vehicle_pfg, bool force_to_cell, float& best_dist_sqr, vector* best_navcell_pos, nav_cell** best_ncp)
{
    best_dist_sqr = 3.4028235e+38f; // Magic number: FLT_MAX.
    raycast_output::hit_info* best = nullptr;
    for (int i = 0; i < out.m_hits.getSize(); i++) {
        raycast_output::hit_info& hit = out.m_hits[i];
        const bb_pfg_layer* layer = bb_pf_hit_layer(hit, vehicle_pfg);
        if (layer == nullptr) {
            continue;
        }
        nav_cell* ncp = const_cast<nav_cell*>(layer->nav_cells.to_pointer()) + hit.hit_havok_shape_key;
        vector candidate = force_to_cell ? nav_cell_force_point_to_cell(ncp, pos) : hit.hit_pos;
        float dist_sq = vec_dist_squared(pos, candidate);
        if (dist_sq < best_dist_sqr) {
            best_dist_sqr = dist_sq;
            if (best_navcell_pos != nullptr) {
                *best_navcell_pos = candidate;
            }
            *best_ncp = ncp;
            best = &hit;
        }
    }
    return best;
}

// 0x00B0DA30 (:5253)
static raycast_output::hit_info* get_best_navmesh_spherecast_hit(const vector& pos, raycast_output& out_down, raycast_output& out_up, bool vehicle_pfg, bool force_to_cell, vector* adjusted_pos, nav_cell** best_ncp)
{
    float dist_down, dist_up;
    vector pos_down, pos_up;
    nav_cell* cell_down;
    nav_cell* cell_up;
    raycast_output::hit_info* down = get_best_navmesh_spherecast_hit(pos, out_down, vehicle_pfg, force_to_cell, dist_down, &pos_down, &cell_down);
    raycast_output::hit_info* up = get_best_navmesh_spherecast_hit(pos, out_up, vehicle_pfg, force_to_cell, dist_up, &pos_up, &cell_up);
    if (down == nullptr && up == nullptr) {
        return nullptr;
    }
    if (down != nullptr && (up == nullptr || dist_down < dist_up)) {
        if (adjusted_pos != nullptr) {
            *adjusted_pos = pos_down;
        }
        *best_ncp = cell_down;
        return down;
    }
    if (adjusted_pos != nullptr) {
        *adjusted_pos = pos_up;
    }
    *best_ncp = cell_up;
    return up;
}

// 0x00B0DB40 (:5325)
nav_cell* bb_pf_get_nav_cell_new(const vector& pos, bool vehicle_pfg, uint64_t* nav_cell_handle, bool force_to_cell, vector* adjust_pos)
{
    nav_cell* cells[64]; // Magic number: rfg.exe's array.
    if (nav_cell_handle != nullptr) {
        *nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
    }
    if (adjust_pos != nullptr) {
        *adjust_pos = pos;
    }
    // Magic number: 4 m around the position.
    int count = havok_collect_nav_cells(pos, 4.0f, cells, 64, vehicle_pfg);
    for (int i = 0; i < count; i++) {
        nav_cell* ncp = cells[i];
        if ((ncp->flags & NAV_CELL_FLAG_DISABLED) == 0 && nav_cell_is_point_in_cell_column(ncp, pos)) {
            if (nav_cell_handle != nullptr) {
                *nav_cell_handle = nav_cell_get_handle(ncp);
            }
            return ncp;
        }
    }
    return nullptr;
}

// Whether a disabled cell may be used under a traversal rule.
static bool bb_pf_disabled_cell_allowed(const nav_cell* ncp, nav_cell_traversal_rule traversal_rule)
{
    if ((ncp->flags & NAV_CELL_FLAG_DISABLED) == 0) {
        return true;
    }
    if ((ncp->flags & NAV_CELL_FLAG_LIGHT) == 0) {
        return traversal_rule == NAV_CELL_TRAVERSAL_ALL;
    }
    return traversal_rule != NAV_CELL_TRAVERSAL_ENABLED;
}

// 0x00B0DC10 (:5368)
nav_cell* bb_pf_get_nav_cell_original(const vector& pos, bool vehicle_pfg, uint64_t* nav_cell_handle, bool force_to_cell, nav_cell_traversal_rule traversal_rule, vector* adjust_pos)
{
    // Magic numbers: the casts run from 5 m past the position to 150 m beyond, with the agent's width as radius.
    const float CAST_START = 5.0f;
    const float CAST_REACH = 150.0f;
    const float VEHICLE_RADIUS = 3.0f;
    const float HUMAN_RADIUS = 0.5f;
    if (nav_cell_handle != nullptr) {
        *nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
    }
    if (adjust_pos != nullptr) {
        *adjust_pos = pos;
    }
    raycast_input down;
    down.start = vector(pos.x, pos.y + CAST_START, pos.z);
    down.end = vector(pos.x, pos.y - CAST_REACH, pos.z);
    down.filter_params = nullptr;
    down.filter_callback = nullptr;
    down.m_filter_info = COLLIDE_RAY_DEFAULT;
    down.m_flags = 0;
    down.m_hit_collector_type = raycast_input::COLLECT_ALL_HITS;
    raycast_output out_down;
    if (vehicle_pfg) {
        down.radius = VEHICLE_RADIUS;
        havok_spherecast_vehicle_navmesh(down, out_down);
    } else {
        down.radius = HUMAN_RADIUS;
        havok_spherecast_navmesh(down, out_down);
    }
    raycast_input up;
    up.start = vector(pos.x, pos.y - CAST_START, pos.z);
    up.end = vector(pos.x, pos.y + CAST_REACH, pos.z);
    up.filter_params = nullptr;
    up.filter_callback = nullptr;
    up.m_filter_info = COLLIDE_RAY_DEFAULT;
    up.m_flags = 0;
    up.m_hit_collector_type = raycast_input::COLLECT_ALL_HITS;
    raycast_output out_up;
    if (vehicle_pfg) {
        up.radius = VEHICLE_RADIUS;
        havok_spherecast_vehicle_navmesh(up, out_up);
    } else {
        up.radius = HUMAN_RADIUS;
        havok_spherecast_navmesh(up, out_up);
    }
    nav_cell* best = nullptr;
    raycast_output::hit_info* hit = get_best_navmesh_spherecast_hit(pos, out_down, out_up, vehicle_pfg, force_to_cell, adjust_pos, &best);
    if (hit == nullptr || !bb_pf_disabled_cell_allowed(best, traversal_rule)) {
        return nullptr;
    }
    if (force_to_cell) {
        if (nav_cell_handle != nullptr) {
            *nav_cell_handle = nav_cell_get_handle(best);
        }
        return best;
    }
    if (bb_pf_hit_layer(*hit, false) != nullptr && search_for_nav_cell(&best, best, pos, traversal_rule)) {
        if (nav_cell_handle != nullptr) {
            *nav_cell_handle = nav_cell_get_handle(best);
        }
        return best;
    }
    return nullptr;
}

// 0x00B0DFD0 (:5475)
nav_cell* bb_pf_get_nav_cell(const vector& pos, bool vehicle_pfg, uint64_t* nav_cell_handle, bool force_to_cell, nav_cell_traversal_rule traversal_rule, vector* adjust_pos)
{
    if (nav_cell_handle != nullptr) {
        *nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
    }
    if (adjust_pos != nullptr) {
        *adjust_pos = pos;
    }
    // The box stands on the terrain, whatever the height asked for.
    float terrain_y = 0.0f;
    havok_terrain_height(terrain_y, pos.x, pos.z, false, nullptr);
    // Magic numbers: a 3 m box half-width for vehicles and 0.5 m for humans, 5 m up and down, 256 cells.
    float half_width = vehicle_pfg ? 3.0f : 0.5f;
    vector bmax(half_width, 5.0f, half_width);
    vector bmin(-half_width, -5.0f, -half_width);
    vector center(pos.x, terrain_y, pos.z);
    nav_cell* cells[256];
    int count = havok_collect_nav_cells(bmin, bmax, center, IDENTITY_MATRIX, cells, 256, vehicle_pfg, nullptr);
    float best_dist_sq = 3.4028235e+38f; // Magic number: FLT_MAX.
    vector best_pos = ZERO_VECTOR;
    nav_cell* best = nullptr;
    for (int i = 0; i < count; i++) {
        vector forced = nav_cell_force_point_to_cell(cells[i], pos);
        float dist_sq = vec_xz_dist_squared(pos, forced);
        if (dist_sq < best_dist_sq) {
            best_pos = forced;
            best_dist_sq = dist_sq;
            best = cells[i];
        }
    }
    if (adjust_pos != nullptr) {
        *adjust_pos = best_pos;
    }
    if (best == nullptr || !bb_pf_disabled_cell_allowed(best, traversal_rule)) {
        return nullptr;
    }
    if (!force_to_cell && !nav_cell_is_point_in_cell_column(best, pos)) {
        return nullptr;
    }
    if (nav_cell_handle != nullptr) {
        *nav_cell_handle = nav_cell_get_handle(best);
    }
    return best;
}

// 0x00AEB810 (:5627)
void bb_pf_enable_nav_cells(vector& bmin, vector& bmax, vector& pos, matrix& orient)
{
    nav_cell* cells[256]; // Magic number: rfg.exe's array.
    for (int vehicle = 1; vehicle >= 0; vehicle--) {
        int count = havok_collect_nav_cells(bmin, bmax, pos, orient, cells, 256, vehicle != 0, nullptr);
        for (int i = 0; i < count; i++) {
            cells[i]->flags &= ~(NAV_CELL_FLAG_BLOCKED | NAV_CELL_FLAG_DISABLED);
        }
    }
}

// 0x00AEB8D0 (:5658)
bool rfg_snap_to_ground(vector& pos)
{
    return havok_terrain_height(pos.y, pos.x, pos.z, false, nullptr);
}

// 0x00B0E200 (:5667)
bool rfg_snap_down_to_nav_mesh(vector& pos, const nav_cell* nav_cell)
{
    if (nav_cell == nullptr) {
        nav_cell = bb_pf_get_nav_cell(pos, false, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
        if (nav_cell == nullptr) {
            return false;
        }
    }
    if ((nav_cell->flags & NAV_CELL_FLAG_DISABLED) != 0) {
        return false;
    }
    pos = nav_cell_force_point_to_cell(nav_cell, pos);
    return true;
}

// 0x00B159C0 (:5711)
nav_cell* bb_pf_get_nav_cell_from_hint(vector* adjusted_pos, uint64_t nav_cell_hint_hande, const vector& pos, bool vehicle_pfg, uint64_t* nav_cell_handle, bool simple_search, bool force_to_cell, nav_cell_traversal_rule traversal_rule)
{
    return bb_pf_get_nav_cell_from_hint(adjusted_pos, nav_cell_from_handle(nav_cell_hint_hande), pos, vehicle_pfg, nav_cell_handle, simple_search, force_to_cell, traversal_rule);
}

// 0x00B0E260 (:5717)
nav_cell* bb_pf_get_nav_cell_from_hint(vector* adjusted_pos, nav_cell* nav_cell_hint, const vector& pos, bool vehicle_pfg, uint64_t* nav_cell_handle, bool simple_search, bool force_to_cell, nav_cell_traversal_rule traversal_rule)
{
    // Magic number: a cell over 10 m (100 squared) from the hint is searched for from scratch.
    const float MAX_HINT_DIST_SQ = 100.0f;
    *adjusted_pos = pos;
    if (nav_cell_handle != nullptr) {
        *nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
    }
    nav_cell* cell = nav_cell_hint;
    if (cell == nullptr) {
        if (simple_search) {
            return nullptr;
        }
        cell = bb_pf_get_nav_cell(pos, vehicle_pfg, nav_cell_handle, force_to_cell, traversal_rule, adjusted_pos);
    } else if (nav_cell_is_point_in_cell_column(cell, pos)) {
        if (!bb_pf_disabled_cell_allowed(cell, traversal_rule)) {
            return nullptr;
        }
    } else {
        nav_cell* closest = nullptr;
        if (nav_cell_get_closest_xz_dist_squared_to_point(cell, pos) <= MAX_HINT_DIST_SQ && search_for_nav_cell(&closest, cell, pos, traversal_rule)) {
            cell = closest;
        } else {
            if (simple_search) {
                return nullptr;
            }
            cell = bb_pf_get_nav_cell(pos, vehicle_pfg, nav_cell_handle, force_to_cell, traversal_rule, adjusted_pos);
        }
    }
    if (cell == nullptr) {
        return nullptr;
    }
    *adjusted_pos = nav_cell_force_point_to_cell(cell, pos);
    if (nav_cell_handle != nullptr && *nav_cell_handle == 0xFFFFFFFFFFFFFFFFull) {
        *nav_cell_handle = nav_cell_get_handle(cell);
    }
    return cell;
}

// 0x00B15A00 (:5803)
void bb_pf_simplify_raw_path(path_node** path, const pf_calc_flags& flags)
{
    // Magic numbers: FLT_MAX, the whole path; 4 vehicle nodes to optimize.
    simplify_nav_path(path, flags, false, 3.4028235e+38f, 4);
}

// 0x00B0E3C0 (:5825)
bool bb_pf_is_nav_cell_within_vertical_tolerance(const nav_cell* ncp, const vector& pos, float* delta_y)
{
    // Magic number: 3.8 m, a floor of a building.
    float delta = pos.y - nav_cell_force_point_to_cell(ncp, pos).y;
    if (delta_y != nullptr) {
        *delta_y = delta;
    }
    return fabsf(delta) < 3.8f;
}

// An edge a human cannot cross: nothing beyond it, or a disabled cell.
static bool bb_pf_is_wall_edge(const nav_cell* ncp, int edge)
{
    const nav_cell* adjacent = ncp->adjacent_cells[edge].to_pointer();
    return adjacent == nullptr || (adjacent->flags & NAV_CELL_FLAG_DISABLED) != 0;
}

// The closest point on one edge of a cell, from the cell's own layer.
static float bb_pf_closest_on_edge(vector& closest, const nav_cell* ncp, int edge, const vector& pos)
{
    const vector* vert0 = nav_cell_get_vertex(ncp, edge, nullptr);
    const vector* vert1 = nav_cell_get_vertex(ncp, (edge + 1) % 3, nullptr);
    ix_closest_point_on_line_segment(closest, pos, *vert0, *vert1);
    return vec_dist_squared(pos, closest);
}

// 0x00B0E440 (:5930)
bool bb_pf_find_closest_point_on_nav_mesh_fast(vector& adjusted_pos, nav_cell*& found_nav_cell, const vector& pos, float* dist_sq_out, bool* in_found_cell, bool vehicle_pfg, float collect_radius)
{
    nav_cell* cells[80]; // Magic number: rfg.exe's array.
    float best_dist_sq = -1.0f;
    nav_cell* best = nullptr;
    bool in_cell = false;
    vector best_pos;
    int count = havok_collect_nav_cells(pos, collect_radius, cells, 80, vehicle_pfg);
    for (int i = 0; i < count && !in_cell; i++) {
        nav_cell* ncp = cells[i];
        if ((ncp->flags & NAV_CELL_FLAG_DISABLED) != 0) {
            continue;
        }
        if (nav_cell_is_point_in_cell_column(ncp, pos)) {
            best_pos = pos;
            best_dist_sq = 0.0f;
            in_cell = true;
            best = ncp;
            break;
        }
        for (int edge = 0; edge < 3; edge++) {
            if (bb_pf_is_wall_edge(ncp, edge)) {
                vector closest;
                float dist_sq = bb_pf_closest_on_edge(closest, ncp, edge, pos);
                if (best == nullptr || dist_sq < best_dist_sq) {
                    best_pos = closest;
                    best = ncp;
                    best_dist_sq = dist_sq;
                }
            }
        }
    }
    if (best != nullptr) {
        adjusted_pos = best_pos;
        found_nav_cell = best;
        if (in_found_cell != nullptr) {
            *in_found_cell = in_cell;
        }
        if (dist_sq_out != nullptr) {
            *dist_sq_out = best_dist_sq;
        }
        return true;
    }
    found_nav_cell = nullptr;
    return false;
}

// One step of the two closest-point searches below: a disabled cell the position lies in hands over to its enabled neighbours.
static void bb_pf_closest_from_disabled_cell(const nav_cell* ncp, const vector& pos, float& best_dist_sq, nav_cell*& best, vector& best_pos)
{
    if (!nav_cell_is_point_in_cell_column(ncp, pos)) {
        return;
    }
    for (int edge = 0; edge < 3; edge++) {
        nav_cell* adjacent = ncp->adjacent_cells[edge].to_pointer();
        if (adjacent != nullptr && (adjacent->flags & NAV_CELL_FLAG_DISABLED) == 0) {
            vector closest;
            float dist_sq = bb_pf_closest_on_edge(closest, ncp, edge, pos);
            if (dist_sq < best_dist_sq) {
                best = adjacent;
                best_pos = closest;
                best_dist_sq = dist_sq;
            }
        }
    }
}

// 0x00B0E680 (:5993)
bool bb_pf_find_closest_point_on_nav_mesh(const vector& pos, vector& adjusted_pos, nav_cell*& found_nav_cell, bool vehicle_pfg)
{
    nav_cell* cells[128]; // Magic number: rfg.exe's array.
    float best_dist_sq = 1e+06f; // Magic number: 1000 m squared.
    vector best_pos = ZERO_VECTOR;
    nav_cell* best = nullptr;
    // Magic number: 5 m around the position.
    int count = havok_collect_nav_cells(pos, 5.0f, cells, 128, vehicle_pfg);
    for (int i = 0; i < count; i++) {
        nav_cell* ncp = cells[i];
        if ((ncp->flags & NAV_CELL_FLAG_DISABLED) == 0) {
            for (int edge = 0; edge < 3; edge++) {
                if (bb_pf_is_wall_edge(ncp, edge)) {
                    vector closest;
                    float dist_sq = bb_pf_closest_on_edge(closest, ncp, edge, pos);
                    if (dist_sq < best_dist_sq) {
                        best_pos = closest;
                        best_dist_sq = dist_sq;
                        best = ncp;
                    }
                }
            }
        } else {
            bb_pf_closest_from_disabled_cell(ncp, pos, best_dist_sq, best, best_pos);
        }
    }
    if (best != nullptr) {
        adjusted_pos = nav_cell_force_point_to_cell(best, best_pos);
        found_nav_cell = best;
        return true;
    }
    return false;
}

// 0x00B0EA00 (:6122)
bool bb_pf_find_closest_point_in_or_on_nav_mesh(const vector& pos, vector& adjusted_pos, nav_cell*& found_nav_cell, bool& on_navmesh, bool vehicle_pfg)
{
    nav_cell* cells[128]; // Magic number: rfg.exe's array.
    float best_dist_sq = 1e+06f; // Magic number: 1000 m squared.
    vector best_pos = ZERO_VECTOR;
    nav_cell* best = nullptr;
    on_navmesh = false;
    // Magic number: 5 m around the position.
    int count = havok_collect_nav_cells(pos, 5.0f, cells, 128, vehicle_pfg);
    for (int i = 0; i < count; i++) {
        nav_cell* ncp = cells[i];
        if ((ncp->flags & NAV_CELL_FLAG_DISABLED) == 0) {
            if (nav_cell_is_point_in_cell_column(ncp, pos)) {
                best_pos = pos;
                on_navmesh = true;
                best = ncp;
                break;
            }
            for (int edge = 0; edge < 3; edge++) {
                if (bb_pf_is_wall_edge(ncp, edge)) {
                    vector closest;
                    float dist_sq = bb_pf_closest_on_edge(closest, ncp, edge, pos);
                    if (dist_sq < best_dist_sq) {
                        best_pos = closest;
                        best_dist_sq = dist_sq;
                        best = ncp;
                    }
                }
            }
        } else {
            bb_pf_closest_from_disabled_cell(ncp, pos, best_dist_sq, best, best_pos);
        }
    }
    if (best != nullptr) {
        adjusted_pos = nav_cell_force_point_to_cell(best, best_pos);
        found_nav_cell = best;
        return true;
    }
    return false;
}

// 0x00B0EDB0 (:6203)
bool bb_pf_find_closest_point_on_nav_mesh_slow(const vector& pos, vector& adjusted_pos, nav_cell*& found_nav_cell)
{
    float best_dist_sq = 1e+06f; // Magic number: 1000 m squared.
    vector best_pos = ZERO_VECTOR;
    nav_cell* best = nullptr;
    int num_zones = World.get_num_objects<obj_zone>();
    for (int z = 0; z < num_zones; z++) {
        const obj_zone* zone = World.get_object<obj_zone>(z);
        const bb_pfg_layer* layer = zone->zone_pfg.pfg;
        if (layer == nullptr) {
            continue;
        }
        nav_cell* cells = const_cast<nav_cell*>(layer->nav_cells.to_pointer());
        for (int i = 0; i < layer->num_nav_cells; i++) {
            nav_cell* ncp = &cells[i];
            if ((ncp->flags & NAV_CELL_FLAG_DISABLED) != 0) {
                continue;
            }
            for (int edge = 0; edge < 3; edge++) {
                if (bb_pf_is_wall_edge(ncp, edge)) {
                    vector closest;
                    float dist_sq = bb_pf_closest_on_edge(closest, ncp, edge, pos);
                    if (dist_sq < best_dist_sq) {
                        best_pos = closest;
                        best_dist_sq = dist_sq;
                        best = ncp;
                    }
                }
            }
        }
    }
    if (best == nullptr) {
        return false;
    }
    adjusted_pos = nav_cell_force_point_to_cell(best, best_pos);
    found_nav_cell = best;
    return true;
}

// 0x00B0F040 (:6261)
bool rfg_pf_find_closest_point_on_nav_mesh_using_collect(const vector& pos, bool vehicle_pfg, vector& adjusted_pos, nav_cell*& found_nav_cell, float box_size)
{
    Navmesh_closest_point = ZERO_VECTOR;
    Navmesh_closest_dist_sq = 3.4028235e+38f; // Magic number: FLT_MAX.
    Navmesh_test_point = pos;
    Navmesh_closest_cell = nullptr;
    vector bmax(box_size, box_size, box_size);
    vector bmin(-box_size, -box_size, -box_size);
    nav_cell* unused;
    havok_collect_nav_cells(bmin, bmax, pos, IDENTITY_MATRIX, &unused, 1, vehicle_pfg, navmesh_collect_callback);
    if (Navmesh_closest_cell != nullptr) {
        adjusted_pos = nav_cell_force_point_to_cell(Navmesh_closest_cell, Navmesh_closest_point);
        found_nav_cell = Navmesh_closest_cell;
        return true;
    }
    return false;
}

// 0x00B15A30 (:6284)
bool rfg_pf_find_best_transition_node_from_pos(const vector& dest_pos, const vector& pos, navpoint*& found_node)
{
    // Magic numbers: the navpoint types PATH (10) to 21 are the ones inside buildings.
    navpoint* other = nullptr;
    navpoint* closest = rfg_pf_find_closest_navpoint_to_pos(pos, other, 10, 0x15);
    float cost;
    if (closest != nullptr) {
        found_node = rfg_pf_find_best_transition_node(dest_pos, closest, cost, true, false, 0xFFFFFFFF);
        if (found_node != nullptr) {
            return true;
        }
    }
    if (other != nullptr) {
        found_node = rfg_pf_find_best_transition_node(dest_pos, other, cost, true, false, 0xFFFFFFFF);
        if (found_node != nullptr) {
            return true;
        }
    }
    return false;
}

// Whether a human's navpoint path node may be walked past: a real navpoint node, not on the mesh.
static bool bb_pf_is_navpoint_node(const path_node* node)
{
    return node->nav_cell_handle == 0xFFFFFFFFFFFFFFFFull && node->navpoint_handle != 0xFFFFFFFF;
}

// The transition the path just used, to steer the next calculation away from it.
static void bb_pf_avoid_second_last_navpoint(human* hp, path_node* path)
{
    path_node* tail = path->prev;
    if (tail == nullptr || !bb_pf_is_navpoint_node(tail) || tail == path) {
        return;
    }
    path_node* before = tail->prev;
    if (before != nullptr && before != tail && bb_pf_is_navpoint_node(before)) {
        hp->pf.transition_node_to_avoid = before->navpoint_handle;
    }
}

// 0x00B1AAE0 (:6307) LTCG: end_navp ECX; end_pos is not passed.
static path_node* rfg_pf_find_path_from_navmesh_to_nodal(human* hp, const vector& start_pos, const vector& end_pos, const nav_cell* start_nav_cell, navpoint* end_navp, unsigned short ignore_set)
{
    if (end_navp == nullptr) {
        return nullptr;
    }
    bool is_edf = hp != nullptr && hp->current_team == HUMAN_TEAM_EDF;
    float cost = 0.0f;
    navpoint* transition = rfg_pf_find_best_transition_node(start_pos, end_navp, cost, false, is_edf, 0xFFFFFFFF);
    bool encourage_spread = false;
    if (hp != nullptr && hp->current_team == HUMAN_TEAM_EDF && human_combat_get_target(hp) != nullptr) {
        encourage_spread = true;
    }
    if (transition == nullptr) {
        return nullptr;
    }
    vector transition_pos;
    nav_cell* transition_cell = bb_pf_get_nav_cell_from_hint(&transition_pos, transition->nav_cell_ptr, transition->pos, false, nullptr, false, false, NAV_CELL_TRAVERSAL_ENABLED);
    if (transition_cell == nullptr && !bb_pf_find_closest_point_on_nav_mesh(transition->pos, transition_pos, transition_cell, false)) {
        return nullptr;
    }
    uint64_t transition_handle = nav_cell_get_handle(transition_cell);
    if (transition_pos == ZERO_VECTOR) {
        transition_pos = transition->pos;
    }
    bool scripted = false;
    if (hp->get_sub_type() == OT_SUB_HUMAN_NPC) {
        scripted = ((npc*)hp)->npc_flags.scripted_move != 0;
    }
    path_node* path = get_path(start_pos, transition_pos, nav_cell_get_handle(start_nav_cell), transition_handle, false, false, ignore_set, false, is_edf, encourage_spread, scripted, hp->pf.transition_node_to_avoid);
    if (path != nullptr) {
        path_node* tail = path->prev;
        // Magic number: 0.1 m (squared): the path reached the transition itself, so it goes on into the building.
        if (tail == nullptr || 0.1f <= vec_xz_dist_squared(tail->pos, transition->pos)) {
            bb_pf_avoid_second_last_navpoint(hp, path);
        } else {
            for (int i = 0; i < transition->num_links; i++) {
                signed char type = transition->links[i]->type;
                if (type == 0x11 || type == NAVPOINT_TYPE_PATH || type == 0x12 || type == 0x13 || type == NAVPOINT_TYPE_LADDER) {
                    path_node* node = bb_pf_get_free_path_node();
                    if (node != nullptr) {
                        node->pos = transition->links[i]->pos;
                        node->nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
                        node->navpoint_handle = transition->links[i]->get_handle();
                        bb_pf_path_append(&path, node);
                        if (transition->type == NAVPOINT_TYPE_LADDER_TRANSITION && transition->links[i]->type == NAVPOINT_TYPE_LADDER) {
                            tail->flags.ladder = true;
                        }
                        break;
                    }
                }
            }
        }
    }
    hp->pf.start_pos = start_pos;
    hp->hflags.on_mover = 0;
    return path;
}

// Whether a navpoint is one end of a ladder.
static bool bb_pf_is_ladder_navpoint(const navpoint* navp)
{
    return navp->type == NAVPOINT_TYPE_LADDER || navp->type == NAVPOINT_TYPE_LADDER_TRANSITION;
}

// A horizontal vector at a right angle to v.
static vector bb_pf_up_cross(const vector& v)
{
    return vector(v.z * UP_VECTOR.y - v.y * UP_VECTOR.z, v.x * UP_VECTOR.z - v.z * UP_VECTOR.x, v.y * UP_VECTOR.x - v.x * UP_VECTOR.y);
}

// A point along the side of a navpoint's path corridor. Magic number: 0.9, inside the radius.
static vector bb_pf_corridor_point(const navpoint* navp, const vector& dir, float radius)
{
    return vector(dir.x * radius * 0.9f + navp->pos.x, navp->pos.y + radius * dir.y * 0.9f, navp->pos.z + radius * dir.z * 0.9f);
}

static float bb_pf_min_radius(const navpoint* a, const navpoint* b)
{
    return a->radius < b->radius ? a->radius : b->radius;
}

// 0x00AFAAF0 (:6452) LTCG: end_pos EAX
static path_node* rfg_pf_find_path_between_nodes_of_same_set(human* hp, const vector& start_pos, const vector& end_pos, navpoint* start_navp, navpoint* end_navp)
{
    // Magic numbers: 10000 m squared bounds the search for a corridor; a node 0.85 of the way along the last link and 0.7
    // of the radius into the first one; a disabled link costs 2.5 times its length.
    const float MAX_CORRIDOR_DIST_SQ = 10000.0f;
    const float END_LINK_FRACTION = 0.85f;
    const float START_RADIUS_FRACTION = 0.7f;
    const float DISABLED_LINK_COST = 2.5f;
    const signed char TYPE_ONE_WAY_UP = 0x13;
    const signed char TYPE_ONE_WAY_DOWN = 0x14;
    if (end_navp == nullptr || start_navp == nullptr || end_navp->set != start_navp->set || end_navp == start_navp) {
        return nullptr;
    }
    // The destination may lie off the node graph, beside a link.
    bool off_graph_end = false;
    vector end_corridor_pos = ZERO_VECTOR;
    vector end_link_pos = ZERO_VECTOR;
    double end_radius = end_navp->radius;
    if (end_radius * end_radius < (double)vec_dist_squared(end_pos, end_navp->pos)) {
        float best_dist_sq = MAX_CORRIDOR_DIST_SQ;
        for (int i = 0; i < end_navp->num_links; i++) {
            const navpoint* link = end_navp->links[i];
            vector closest;
            ix_closest_point_on_line_segment(closest, end_pos, end_navp->pos, link->pos);
            vector out(end_pos.x - closest.x, end_pos.y - closest.y, end_pos.z - closest.z);
            out.normalize_safe();
            float radius = bb_pf_min_radius(end_navp, link);
            vector candidate(out.x * radius + closest.x, out.y * radius + closest.y, out.z * radius + closest.z);
            float dist_sq = vec_dist_squared(candidate, end_pos);
            if (dist_sq < best_dist_sq) {
                end_corridor_pos = candidate;
                off_graph_end = true;
                best_dist_sq = dist_sq;
                vector along = link->pos - end_navp->pos;
                along.normalize_safe();
                end_link_pos = vector(along.x * END_LINK_FRACTION + end_navp->pos.x, along.y * END_LINK_FRACTION + end_navp->pos.y, along.z * END_LINK_FRACTION + end_navp->pos.z);
            }
        }
    }
    bool is_edf = hp != nullptr && hp->current_team == HUMAN_TEAM_EDF;
    unsigned int session = Navpoint_heap.start_session();
    navpoint_query_for_path(start_navp, nullptr, 0.0f, end_navp->pos, &Navpoint_heap);
    bool found = false;
    while (Navpoint_heap.num_navpoints > 0) {
        navpoint* best = Navpoint_heap.remove_best_cell();
        if (best == end_navp) {
            found = true;
            break;
        }
        if (!Navpoint_treat_all_as_enabled && best->heap_data.disabled_by != 0) {
            continue;
        }
        if (best->type == NAVPOINT_TYPE_TRANSITION_JUMP || best->type == NAVPOINT_TYPE_LEAP_OVER) {
            continue;
        }
        bool up = best->type == TYPE_ONE_WAY_UP;
        bool down = best->type == TYPE_ONE_WAY_DOWN;
        bool stairs = best->type == NAVPOINT_TYPE_STAIRS;
        for (int i = 0; i < best->num_links; i++) {
            navpoint* link = best->links[i];
            if (link->heap_data.session_id == session && link->heap_data.flags.is_open == 0) {
                continue;
            }
            if (!is_edf && best->nav_flags.door != 0 && link->nav_flags.door != 0) {
                continue;
            }
            if (up && link->type != TYPE_ONE_WAY_DOWN && link->type != NAVPOINT_TYPE_TRANSITION_JUMP) {
                continue;
            }
            if (down && link->type == TYPE_ONE_WAY_UP) {
                continue;
            }
            float cost = vec_dist(link->pos, best->pos);
            if (!Navpoint_treat_all_as_enabled && best->link_disabled_by[i] != 0) {
                // Bit 4: a detour disabled the link. Disabled stairs can still be walked down.
                if ((stairs && link->type == NAVPOINT_TYPE_STAIRS && best->pos.y < link->pos.y) || (best->link_disabled_by[i] & 4) != 0) {
                    continue;
                }
                cost *= DISABLED_LINK_COST;
            }
            navpoint_query_for_path(link, best, best->heap_data.arrival_cost + cost, end_navp->pos, &Navpoint_heap);
        }
    }
    Navpoint_heap.in_session = false;
    if (!found) {
        return nullptr;
    }
    navpoint* walker = end_navp;
    do {
        walker->prev_path_navpoint->next_path_navpoint = walker;
        walker = walker->prev_path_navpoint;
    } while (walker != start_navp);
    end_navp->next_path_navpoint = nullptr;

    path_node* path = nullptr;
    navpoint* current = start_navp;
    // The start may lie off the node graph too: it joins the graph on the link nearest to it.
    double start_radius = start_navp->radius;
    if (start_radius * start_radius < (double)vec_dist_squared(start_pos, start_navp->pos)) {
        float best_dist_sq = MAX_CORRIDOR_DIST_SQ;
        vector corridor_pos = start_pos;
        navpoint* best_link = nullptr;
        for (int i = 0; i < start_navp->num_links; i++) {
            navpoint* link = start_navp->links[i];
            vector closest;
            ix_closest_point_on_line_segment(closest, start_pos, start_navp->pos, link->pos);
            vector out(start_pos.x - closest.x, start_pos.y - closest.y, start_pos.z - closest.z);
            out.normalize_safe();
            double radius = bb_pf_min_radius(start_navp, link);
            vector candidate;
            if ((double)vec_dist_squared(closest, start_pos) <= radius * radius) {
                candidate = start_pos;
            } else {
                float r = (float)radius;
                candidate = vector(out.x * r + closest.x, closest.y + r * out.y, closest.z + r * out.z);
            }
            float dist_sq = vec_dist_squared(candidate, start_pos);
            if (dist_sq < best_dist_sq) {
                corridor_pos = candidate;
                best_link = link;
                best_dist_sq = dist_sq;
            }
        }
        path = bb_pf_get_free_path_node();
        if (path == nullptr) {
            return nullptr;
        }
        path->pos = corridor_pos;
        path->navpoint_handle = start_navp->get_handle();
        path->next = path;
        path->prev = path;
        if (bb_pf_is_ladder_navpoint(start_navp) && best_link != nullptr && bb_pf_is_ladder_navpoint(best_link)) {
            path->flags.ladder = true;
        }
        if (start_navp->type == NAVPOINT_TYPE_STAIRS && best_link != nullptr && best_link->type == NAVPOINT_TYPE_STAIRS) {
            path->flags.stairs = true;
        }
        navpoint* next = start_navp->next_path_navpoint;
        path_node* join = nullptr;
        if (best_link == next) {
            if (next != nullptr) {
                vector back = start_navp->pos - next->pos;
                back.normalize_safe();
                float reach = next->radius * START_RADIUS_FRACTION;
                join = bb_pf_get_free_path_node();
                if (join == nullptr) {
                    bb_pf_free_path(&path);
                    return nullptr;
                }
                join->pos = vector(back.x * reach + next->pos.x, next->pos.y + reach * back.y, next->pos.z + reach * back.z);
                join->navpoint_handle = next->get_handle();
                bb_pf_path_append(&path, join);
                current = next;
            }
        } else {
            // Original quirk kept: with no links at all best_link is null here; a path with a next node always has one.
            vector along = best_link->pos - start_navp->pos;
            along.normalize_safe();
            float reach = start_navp->radius * START_RADIUS_FRACTION;
            join = bb_pf_get_free_path_node();
            if (join == nullptr) {
                bb_pf_free_path(&path);
                return nullptr;
            }
            join->pos = vector(along.x * reach + start_navp->pos.x, start_navp->pos.y + reach * along.y, start_navp->pos.z + reach * along.z);
            join->navpoint_handle = start_navp->get_handle();
            bb_pf_path_append(&path, join);
        }
    }

    if (current != end_navp) {
        navpoint* navp = current->next_path_navpoint;
        if (bb_pf_is_ladder_navpoint(current) && bb_pf_is_ladder_navpoint(navp)) {
            path_node* node = bb_pf_get_free_path_node();
            if (node == nullptr) {
                bb_pf_free_path(&path);
                return nullptr;
            }
            node->pos = current->pos;
            node->flags.ladder = true;
            node->navpoint_handle = current->get_handle();
            bb_pf_path_append(&path, node);
        }
        bool done = false;
        navpoint* prev = nullptr;
        do {
            navpoint* after = navp->next_path_navpoint;
            if (navp == end_navp) {
                done = true;
                after = nullptr;
            }
            signed char type = navp->type;
            bool climbing = type == NAVPOINT_TYPE_LADDER || type == NAVPOINT_TYPE_LADDER_TRANSITION || type == TYPE_ONE_WAY_UP || type == TYPE_ONE_WAY_DOWN;
            bool jump_next = after != nullptr && (after->type == NAVPOINT_TYPE_TRANSITION_JUMP || after->type == NAVPOINT_TYPE_LEAP_OVER);
            path_node* node = nullptr;
            if ((after == nullptr && prev == nullptr) || jump_next || climbing) {
                // Straight through the middle of the navpoint, with no handle (the node is not a corner).
                node = bb_pf_get_free_path_node();
                if (node == nullptr) {
                    bb_pf_free_path(&path);
                    return nullptr;
                }
                node->pos = navp->pos;
                bb_pf_path_append(&path, node);
            } else {
                vector in_dir = prev != nullptr ? navp->pos - prev->pos : ZERO_VECTOR;
                vector out_dir = after != nullptr ? after->pos - navp->pos : ZERO_VECTOR;
                if (prev == nullptr) {
                    node = bb_pf_get_free_path_node();
                    if (node == nullptr) {
                        bb_pf_free_path(&path);
                        return nullptr;
                    }
                    out_dir.flatten_normalize_safe();
                    node->pos = bb_pf_corridor_point(navp, bb_pf_up_cross(out_dir), bb_pf_min_radius(navp, after));
                    node->navpoint_handle = navp->get_handle();
                    bb_pf_path_append(&path, node);
                } else if (after == nullptr) {
                    node = bb_pf_get_free_path_node();
                    if (node == nullptr) {
                        bb_pf_free_path(&path);
                        return nullptr;
                    }
                    in_dir.flatten_normalize_safe();
                    node->pos = bb_pf_corridor_point(navp, bb_pf_up_cross(in_dir), bb_pf_min_radius(navp, prev));
                    node->navpoint_handle = navp->get_handle();
                    bb_pf_path_append(&path, node);
                } else {
                    in_dir.flatten_normalize_safe();
                    out_dir.flatten_normalize_safe();
                    vector right = bb_pf_up_cross(in_dir);
                    if (right.x * out_dir.x + right.y * out_dir.y + right.z * out_dir.z <= 0.0f) {
                        // Around the outside of the corner: one node for each link.
                        path_node* first = bb_pf_get_free_path_node();
                        node = bb_pf_get_free_path_node();
                        if (first == nullptr || node == nullptr) {
                            // Original bug kept: a second node without a first one is never handed back.
                            if (first != nullptr) {
                                bb_pf_path_append(&path, first);
                            }
                            bb_pf_free_path(&path);
                            return nullptr;
                        }
                        first->pos = bb_pf_corridor_point(navp, right, bb_pf_min_radius(navp, prev));
                        first->navpoint_handle = navp->get_handle();
                        bb_pf_path_append(&path, first);
                        node->pos = bb_pf_corridor_point(navp, bb_pf_up_cross(out_dir), bb_pf_min_radius(navp, after));
                        node->navpoint_handle = navp->get_handle();
                        bb_pf_path_append(&path, node);
                    } else {
                        node = bb_pf_get_free_path_node();
                        if (node == nullptr) {
                            bb_pf_free_path(&path);
                            return nullptr;
                        }
                        vector middle(out_dir.x - in_dir.x, out_dir.y - in_dir.y, out_dir.z - in_dir.z);
                        vector bisector = middle.copy_normalize_safe();
                        float radius = bb_pf_min_radius(navp, prev);
                        if (after->radius < radius) {
                            radius = after->radius;
                        }
                        node->pos = bb_pf_corridor_point(navp, bisector, radius);
                        node->navpoint_handle = navp->get_handle();
                        bb_pf_path_append(&path, node);
                    }
                }
            }
            if (after != nullptr) {
                if (after->type == NAVPOINT_TYPE_TRANSITION_JUMP) {
                    node->flags.jumping = true;
                }
                if (after->type == NAVPOINT_TYPE_LEAP_OVER) {
                    node->flags.leap_over = true;
                }
                if (after->type == NAVPOINT_TYPE_STAIRS) {
                    node->flags.stairs = true;
                }
                if (bb_pf_is_ladder_navpoint(navp) && bb_pf_is_ladder_navpoint(after)) {
                    node->flags.ladder = true;
                }
            }
            prev = navp;
            navp = navp->next_path_navpoint;
        } while (!done);
    }
    if (!off_graph_end) {
        return path;
    }
    path_node* link_node = bb_pf_get_free_path_node();
    if (link_node == nullptr) {
        bb_pf_free_path(&path);
        return nullptr;
    }
    link_node->pos = end_link_pos;
    bb_pf_path_append(&path, link_node);
    path_node* corridor_node = bb_pf_get_free_path_node();
    if (corridor_node == nullptr) {
        return path;
    }
    corridor_node->pos = end_corridor_pos;
    bb_pf_path_append(&path, corridor_node);
    return path;
}

// 0x00B1AEC0 (:6998)
static path_node* rfg_pf_find_path_from_nodal_to_navmesh(human* hp, const vector& start_pos, const vector& end_pos, navpoint* start_navp, const nav_cell* end_nav_cell)
{
    if (start_navp == nullptr) {
        return nullptr;
    }
    bool allow_jumps = World.object_from_handle(hp->combat.target_handle) != nullptr;
    npc* np = (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC) ? (npc*)hp : nullptr;
    bool scripted = false;
    if (np != nullptr) {
        if (np->get_ai()->is_in_player_squad(true) != nullptr) {
            allow_jumps = true;
        }
        scripted = np->npc_flags.scripted_move != 0;
    }
    bool edf_in_combat = hp->current_team == HUMAN_TEAM_EDF && human_combat_get_target(hp) != nullptr;
    float cost = 0.0f;
    navpoint* transition = rfg_pf_find_best_transition_node(end_pos, start_navp, cost, allow_jumps, hp->current_team == HUMAN_TEAM_EDF, hp->pf.transition_node_to_avoid);
    if (transition == nullptr) {
        return nullptr;
    }
    path_node* path = nullptr;
    if (start_navp != transition) {
        path = rfg_pf_find_path_between_nodes_of_same_set(hp, start_pos, transition->pos, start_navp, transition);
        if (path == nullptr) {
            return nullptr;
        }
    }
    const vector& anchor = (path != nullptr && path->prev != nullptr) ? path->prev->pos : transition->pos;
    vector adjusted;
    nav_cell* cell = bb_pf_get_nav_cell_from_hint(&adjusted, transition->nav_cell_ptr, anchor, false, nullptr, false, false, NAV_CELL_TRAVERSAL_ENABLED);
    if (cell != nullptr && start_navp == transition) {
        // Original quirks kept from the asm's arguments: stuck travels in ignore_set's slot, the EDF combat flag in
        // use_doors' and the scripted flag in encourage_edf_spread's; the path to avoid is none.
        return get_path(start_pos, end_pos, nav_cell_get_handle(cell), nav_cell_get_handle(end_nav_cell), false, false, (unsigned short)hp->hflags.stuck, false, edf_in_combat, scripted, false, 0xFFFFFFFF);
    }
    bool have_cell = cell != nullptr;
    if (!have_cell) {
        have_cell = bb_pf_find_closest_point_on_nav_mesh(anchor, adjusted, cell, false);
    }
    if (have_cell) {
        path_node* node = bb_pf_get_free_path_node();
        if (node == nullptr) {
            bb_pf_free_path(&path);
            return nullptr;
        }
        node->pos = adjusted == ZERO_VECTOR ? transition->pos : adjusted;
        node->nav_cell_handle = nav_cell_get_handle(cell);
        bb_pf_path_append(&path, node);
    }
    hp->hflags.recalculate_at_node = 1;
    // Magic number: no path recheck for 500 m.
    hp->pf.dist_until_recheck = 500.0f;
    hp->pf.start_pos = start_pos;
    hp->hflags.on_mover = 1;
    return path;
}

// 0x00B15AB0 (:7104) LTCG: hp EDI, start_navp EAX
static path_node* rfg_pf_find_path_between_nodes(human* hp, const vector& start_pos, const vector& end_pos, navpoint* start_navp, navpoint* end_navp)
{
    if (start_navp == nullptr || end_navp == nullptr) {
        return nullptr;
    }
    bool other_set = false;
    path_node* path;
    if (start_navp->set == end_navp->set) {
        if (start_navp == end_navp) {
            path = bb_pf_get_free_path_node();
            if (path == nullptr) {
                return nullptr;
            }
            path->pos = end_pos;
            path->next = path;
            path->prev = path;
        } else {
            path = rfg_pf_find_path_between_nodes_of_same_set(hp, start_pos, end_pos, start_navp, end_navp);
            if (path == nullptr) {
                return nullptr;
            }
            path_node* node = bb_pf_get_free_path_node();
            if (node != nullptr) {
                node->pos = end_pos;
                bb_pf_path_append(&path, node);
            }
        }
    } else {
        other_set = true;
        float cost = 0.0f;
        bool allow_jumps = World.object_from_handle(hp->combat.target_handle) != nullptr;
        navpoint* transition = rfg_pf_find_best_transition_node(end_navp->pos, start_navp, cost, allow_jumps, hp->current_team == HUMAN_TEAM_EDF, hp->pf.transition_node_to_avoid);
        if (transition == nullptr) {
            return nullptr;
        }
        if (start_navp == transition) {
            path = bb_pf_get_free_path_node();
            if (path == nullptr) {
                return nullptr;
            }
            path->pos = start_navp->pos;
            path->next = path;
            path->prev = path;
        } else {
            path = rfg_pf_find_path_between_nodes_of_same_set(hp, start_pos, transition->pos, start_navp, transition);
        }
    }
    if (path == nullptr) {
        return nullptr;
    }
    if (hp != nullptr) {
        hp->hflags.recalculate_at_node = other_set ? 1 : 0;
        // Magic number: no path recheck for 500 m.
        hp->pf.dist_until_recheck = 500.0f;
        hp->pf.start_pos = start_pos;
        hp->hflags.on_mover = 1;
    }
    return path;
}

// 0x00B08E30 (:7233) LTCG: only start_pos is passed.
static path_node* rfg_pf_find_path_from_null_area(human* hp, const vector& start_pos, const vector& end_pos, const nav_cell* end_nav_cell, bool out_of_chunk)
{
    nav_cell* cells[64]; // Magic number: rfg.exe's array.
    // Magic number: 25 m around the position.
    int count = havok_collect_nav_cells(start_pos, 25.0f, cells, 64, false);
    if (count == 0) {
        return nullptr;
    }
    float best_dist_sq = 3.4028235e+38f; // Magic number: FLT_MAX.
    vector best_pos = ZERO_VECTOR;
    nav_cell* best = nullptr;
    for (int i = 0; i < count; i++) {
        if (cells[i] == nullptr) {
            continue;
        }
        for (int edge = 0; edge < 3; edge++) {
            vector closest;
            float dist_sq = bb_pf_closest_on_edge(closest, cells[i], edge, start_pos);
            if (dist_sq < best_dist_sq) {
                best_pos = closest;
                best = cells[i];
                best_dist_sq = dist_sq;
            }
        }
    }
    if (best_pos == ZERO_VECTOR) {
        return nullptr;
    }
    path_node* path = bb_pf_get_free_path_node();
    if (path == nullptr) {
        return nullptr;
    }
    path->pos = best_pos;
    path->nav_cell_handle = nav_cell_get_handle(best);
    path->next = path;
    path->prev = path;
    finalize_path(&path);
    return path;
}

// 0x00B032F0 (:7299)
navpoint* rfg_pf_find_closest_navpoint_to_pos(const vector& start_pos, navpoint*& other_navpoint, signed char low_type, signed char high_type)
{
    // Magic numbers: a box of 21.21 m around the position (15 m links at 45 degrees), 2 m up and down; 2 m of height
    // difference separates floors of a building.
    const float BOX_HALF_WIDTH = 10.605f;
    const float BOX_HALF_HEIGHT = 2.0f;
    const float FLOOR_HEIGHT = 2.0f;
    vector bmin(-BOX_HALF_WIDTH, -BOX_HALF_HEIGHT, -BOX_HALF_WIDTH);
    vector bmax(BOX_HALF_WIDTH, BOX_HALF_HEIGHT, BOX_HALF_WIDTH);
    matrix orient;
    orient.make_quick(vector(1.0f, 0.0f, 0.0f));
    unsigned int handles[64];
    int count = World.collect_objects<navpoint>(bmin, bmax, start_pos, orient, handles, 64, nullptr, nullptr);
    if (count < 1) {
        return nullptr;
    }
    float best_dist = 3.4028235e+38f; // Magic number: FLT_MAX.
    navpoint* best = nullptr;
    for (int i = 0; i < count; i++) {
        navpoint* navp = World.object_from_handle<navpoint>(handles[i]);
        if (handles[i] == 0xFFFFFFFF || navp == nullptr || navp->type < low_type || high_type < navp->type) {
            continue;
        }
        float dist = vec_xz_dist(navp->pos, start_pos);
        if (dist < navp->radius) {
            float gap = dist - navp->radius;
            if (gap < best_dist || other_navpoint != nullptr) {
                best = navp;
                other_navpoint = nullptr;
                best_dist = gap;
            }
            continue;
        }
        if (!(0.0f < best_dist) && other_navpoint == nullptr) {
            continue;
        }
        if (navp->num_links < 1) {
            float gap = vec_xz_dist(start_pos, navp->pos) - navp->radius;
            if (gap < best_dist) {
                best = navp;
                best_dist = gap;
            }
        }
        bool on_a_link = false;
        for (int l = 0; l < navp->num_links; l++) {
            navpoint* link = navp->links[l];
            if (link == nullptr || link->type < low_type || high_type < link->type) {
                continue;
            }
            float radius = bb_pf_min_radius(navp, link);
            vector closest;
            ix_closest_point_on_line_segment(closest, start_pos, navp->pos, link->pos);
            if ((bb_pf_is_ladder_navpoint(navp) && bb_pf_is_ladder_navpoint(link)) || fabsf(start_pos.y - closest.y) <= FLOOR_HEIGHT) {
                on_a_link = true;
                float gap = vec_xz_dist(closest, start_pos) - radius;
                if (gap < best_dist) {
                    float to_navp = vec_xz_dist(closest, navp->pos) - navp->radius;
                    if (vec_xz_dist(closest, link->pos) - link->radius < to_navp) {
                        other_navpoint = navp;
                        best = link;
                    } else {
                        other_navpoint = link;
                        best = navp;
                    }
                    best_dist = gap;
                }
            }
        }
        if (!on_a_link) {
            float gap = dist - navp->radius;
            if (gap < best_dist) {
                best = navp;
                other_navpoint = nullptr;
                best_dist = gap;
            }
        }
    }
    return best;
}

// 0x00B1B330 (:7406)
path_node* rfg_pf_pathfind(human* hp, const vector& start_pos, const vector& end_pos, uint64_t start_nav_cell_hint_handle, uint64_t end_nav_cell_hint_handle, bool out_of_chunk)
{
    // Magic numbers: what separates "on the mesh" from "in a building" (metres). 0.65 m of height leaves the mesh;
    // 3 m from a navpoint, 1.5 m (2.25 squared) or 0.5 m (0.25 squared) from the mesh decide between the two; navpoint
    // types 10 to 21 are the building ones.
    const float ON_MESH_HEIGHT = 0.65f;
    const signed char LOW_TYPE = 10;
    const signed char HIGH_TYPE = 0x15;
    if (hp == nullptr) {
        return nullptr;
    }
    hp->hflags.on_mover = 0;
    hp->hflags.recalculate_at_node = 0;
    hp->pf.dist_until_recheck = 0.0f;
    bool end_adjusted = false;
    bool start_adjusted = false;
    uint64_t start_handle = 0xFFFFFFFFFFFFFFFFull;
    uint64_t end_handle = 0xFFFFFFFFFFFFFFFFull;
    vector adjusted_start, adjusted_end;
    nav_cell* start_cell = bb_pf_get_nav_cell_from_hint(&adjusted_start, nav_cell_from_handle(start_nav_cell_hint_handle), start_pos, false, &start_handle, false, false, NAV_CELL_TRAVERSAL_ENABLED);
    nav_cell* end_cell = bb_pf_get_nav_cell_from_hint(&adjusted_end, nav_cell_from_handle(end_nav_cell_hint_handle), end_pos, false, &end_handle, false, false, NAV_CELL_TRAVERSAL_ENABLED);
    bool had_start_cell = start_cell != nullptr;
    bool encourage_spread = hp->current_team == HUMAN_TEAM_EDF && human_combat_get_target(hp) != nullptr;
    if (hp->movement_mode == HMM_PATHFINDING_TO_VEHICLE && !had_start_cell && end_cell == nullptr) {
        return nullptr;
    }
    bool is_npc = hp->get_sub_type() == OT_SUB_HUMAN_NPC;
    navpoint* other_start = nullptr;
    navpoint* other_end = nullptr;
    navpoint* closest_start = rfg_pf_find_closest_navpoint_to_pos(start_pos, other_start, LOW_TYPE, HIGH_TYPE);
    navpoint* start_navp = closest_start;
    float start_dist = 1000.0f;
    vector start_on_link;
    if (closest_start != nullptr) {
        if (other_start == nullptr) {
            start_dist = vec_xz_dist(closest_start->pos, start_pos) - closest_start->radius;
        } else {
            ix_closest_point_on_line_segment(start_on_link, start_pos, closest_start->pos, other_start->pos);
            float radius = other_start->radius <= closest_start->radius ? other_start->radius : closest_start->radius;
            start_dist = vec_xz_dist(start_on_link, start_pos) - radius;
        }
    }
    if (start_cell != nullptr && closest_start != nullptr) {
        // The mesh cell may lie on another floor than a nearby navpoint.
        vector forced = nav_cell_force_point_to_cell(start_cell, start_pos);
        float y = start_pos.y;
        float height = fabsf(y - forced.y);
        bool drop_cell = false;
        if (other_start == nullptr || closest_start->type != NAVPOINT_TYPE_STAIRS || other_start->type != NAVPOINT_TYPE_STAIRS) {
            if (ON_MESH_HEIGHT < height && fabsf(y - closest_start->pos.y) < 2.0f && start_dist < 4.0f) {
                float terrain_y;
                drop_cell = !havok_terrain_height(terrain_y, start_pos.x, start_pos.z, false, nullptr) || ON_MESH_HEIGHT < start_pos.y - terrain_y;
            }
        } else if (fabsf(start_on_link.y - y) < 2.0f) {
            drop_cell = start_dist < 1.0f || 2.5f < height;
        }
        if (drop_cell) {
            start_cell = nullptr;
        }
    } else if (start_cell == nullptr) {
        if (bb_pf_find_closest_point_on_nav_mesh(start_pos, adjusted_start, start_cell, false)) {
            float dist_sq = vec_xz_dist_squared(adjusted_start, start_pos);
            if (((start_dist <= 3.0f || 2.25f <= dist_sq) && (ON_MESH_HEIGHT <= fabsf(adjusted_start.y - start_pos.y) || 0.25f <= dist_sq))
                || (closest_start != nullptr && vec_xz_dist_squared(start_pos, closest_start->pos) <= dist_sq)) {
                start_cell = nullptr;
            } else {
                start_adjusted = true;
                if (start_cell != nullptr) {
                    start_handle = nav_cell_get_handle(start_cell);
                }
            }
        }
        if (closest_start != nullptr && start_adjusted && 0.0f < start_dist) {
            start_navp = nullptr;
        }
        if (closest_start == nullptr && !start_adjusted) {
            path_node* path = rfg_pf_find_path_from_null_area(hp, start_pos, end_pos, end_cell, out_of_chunk);
            if (path != nullptr) {
                return path;
            }
            if (is_npc) {
                ((npc*)hp)->get_ai()->pf_failure_reason = AI_PF_FAILURE_START_INVALID;
            }
            return nullptr;
        }
    }
    navpoint* end_navp = rfg_pf_find_closest_navpoint_to_pos(end_pos, other_end, LOW_TYPE, HIGH_TYPE);
    if (end_cell != nullptr && end_navp != nullptr) {
        vector forced = nav_cell_force_point_to_cell(end_cell, end_pos);
        float height = fabsf(end_pos.y - forced.y);
        if (other_end == nullptr || end_navp->type != NAVPOINT_TYPE_STAIRS) {
            if (ON_MESH_HEIGHT < height && fabsf(end_pos.y - end_navp->pos.y) < ON_MESH_HEIGHT) {
                end_cell = nullptr;
            }
        } else {
            double radius = (end_navp->radius < other_end->radius ? other_end->radius : end_navp->radius) + 0.5f;
            vector on_link;
            ix_closest_point_on_line_segment(on_link, end_pos, end_navp->pos, other_end->pos);
            if (fabsf(on_link.y - end_pos.y) < 2.0f && !(radius * radius <= (double)vec_xz_dist_squared(on_link, end_pos) && height <= 2.5f)) {
                end_cell = nullptr;
            }
        }
    } else if (end_cell == nullptr) {
        bool dropped = false;
        if (end_navp == nullptr) {
            if (bb_pf_find_closest_point_on_nav_mesh(end_pos, adjusted_end, end_cell, false)) {
                // Magic number: 2 m (4 squared).
                if (4.0f <= vec_xz_dist_squared(adjusted_end, end_pos)) {
                    if (is_npc) {
                        ((npc*)hp)->get_ai()->pf_failure_reason = AI_PF_FAILURE_DEST_INVALID;
                    }
                    return nullptr;
                }
                end_adjusted = true;
            }
        } else {
            float end_dist;
            if (other_end == nullptr) {
                end_dist = vec_xz_dist(end_navp->pos, end_pos) - end_navp->radius;
            } else {
                vector on_link;
                ix_closest_point_on_line_segment(on_link, end_pos, end_navp->pos, other_end->pos);
                float radius = other_end->radius <= end_navp->radius ? other_end->radius : end_navp->radius;
                end_dist = vec_xz_dist(on_link, end_pos) - radius;
            }
            if (bb_pf_find_closest_point_on_nav_mesh(end_pos, adjusted_end, end_cell, false)) {
                float dist_sq = vec_xz_dist_squared(adjusted_end, end_pos);
                if ((end_dist <= 3.0f || 2.25f <= dist_sq) && (ON_MESH_HEIGHT <= fabsf(adjusted_end.y - end_pos.y) || 0.25f <= dist_sq)) {
                    dropped = true;
                } else {
                    end_adjusted = true;
                }
            }
        }
        if (dropped) {
            end_cell = nullptr;
        } else if (end_cell != nullptr) {
            end_handle = nav_cell_get_handle(end_cell);
        }
    }
    vector goal = end_adjusted ? adjusted_end : end_pos;
    vector start = start_adjusted ? adjusted_start : start_pos;
    unsigned short ignore_set = 0;
    if (start_cell == nullptr && start_navp != nullptr && (end_navp == nullptr || end_navp->set != start_navp->set)) {
        ignore_set = start_navp->set;
    }
    bool is_edf = hp->current_team == HUMAN_TEAM_EDF;
    if (end_navp != nullptr && end_cell == nullptr && start_cell != nullptr && start_navp != nullptr && start_navp->set == end_navp->set) {
        // Beside a building's door, staying on the nodes beats walking around on the mesh.
        short links = start_navp->num_links;
        bool at_door = false;
        if (links <= 2 && !is_edf) {
            if (start_navp->nav_flags.door == 0) {
                at_door = start_navp->type == NAVPOINT_TYPE_TRANSITION && links == 1 && start_navp->links[0] != nullptr && start_navp->links[0]->nav_flags.door != 0;
            } else if (links == 2) {
                bool plain0 = start_navp->links[0] != nullptr && start_navp->links[0]->type == NAVPOINT_TYPE_TRANSITION && start_navp->links[0]->nav_flags.door == 0;
                bool plain1 = start_navp->links[1] != nullptr && start_navp->links[1]->type == NAVPOINT_TYPE_TRANSITION && start_navp->links[1]->nav_flags.door == 0;
                at_door = plain0 || plain1;
            } else {
                at_door = true;
            }
        }
        if (!at_door) {
            signed char type = start_navp->type;
            bool keep = (had_start_cell || 3.0f <= start_dist) && (type != NAVPOINT_TYPE_TRANSITION || 1.0f <= start_dist);
            if (keep) {
                float needed = (other_start != nullptr && other_start->type == NAVPOINT_TYPE_TRANSITION) ? 1.0f : 0.0f;
                if (other_start == nullptr || other_start->type == NAVPOINT_TYPE_TRANSITION) {
                    keep = needed <= start_dist;
                }
                if (keep && (type == NAVPOINT_TYPE_LADDER_TRANSITION || type == NAVPOINT_TYPE_LADDER)) {
                    keep = 1.0f <= start_dist;
                }
            }
            if (!keep) {
                start_cell = nullptr;
            }
        }
    }
    // Magic number: 5 m (25 squared): a new destination forgets the transition to avoid.
    if (25.0f < vec_dist_squared(end_pos, hp->pf.eventual_goal_pos)) {
        hp->pf.transition_node_to_avoid = 0xFFFFFFFF;
    }
    bool scripted = false;
    if (is_npc) {
        scripted = ((npc*)hp)->npc_flags.scripted_move != 0;
        encourage_spread = false;
    }
    path_node* path = nullptr;
    if (start_cell != nullptr && end_cell != nullptr) {
        hp->hflags.on_mover = 0;
        path = get_path(start, goal, start_handle, end_handle, false, out_of_chunk, ignore_set, false, is_edf, encourage_spread, scripted, hp->pf.transition_node_to_avoid);
        if (path != nullptr) {
            bb_pf_avoid_second_last_navpoint(hp, path);
        }
    } else if (start_cell != nullptr && end_navp != nullptr) {
        path = rfg_pf_find_path_from_navmesh_to_nodal(hp, start, goal, start_cell, end_navp, ignore_set);
    } else if (start_navp != nullptr) {
        if (end_cell != nullptr) {
            path = rfg_pf_find_path_from_nodal_to_navmesh(hp, start, goal, start_navp, end_cell);
        } else if (end_navp != nullptr) {
            path = rfg_pf_find_path_between_nodes(hp, start, goal, start_navp, end_navp);
        }
    }
    if (path == nullptr) {
        if (is_npc) {
            ai_pathfind_results reason = AI_PF_FAILURE_NO_PATH;
            if (end_navp == nullptr && end_cell == nullptr) {
                reason = AI_PF_FAILURE_DEST_INVALID;
            } else if (start_navp == nullptr && start_cell == nullptr) {
                reason = AI_PF_FAILURE_START_INVALID;
            }
            ((npc*)hp)->get_ai()->pf_failure_reason = reason;
        }
        return nullptr;
    }
    finalize_path(&path);
    return path;
}

// 0x00B0F140 (:7821)
nav_cell* bb_pf_get_closest_nav_cell(const vector& pos, bool vehicle_pfg, uint64_t* nav_cell_handle)
{
    if (nav_cell_handle != nullptr) {
        *nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
    }
    uint64_t found_handle;
    nav_cell* column_cell = bb_pf_get_nav_cell(pos, vehicle_pfg, &found_handle, true, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
    float column_delta = 3.4028235e+38f; // Magic number: FLT_MAX.
    if (column_cell != nullptr) {
        column_delta = pos.y - nav_cell_force_point_to_cell(column_cell, pos).y;
        // Magic number: 3.8 m, a floor of a building.
        if (fabsf(column_delta) < 3.8f) {
            if (nav_cell_handle != nullptr) {
                *nav_cell_handle = found_handle;
            }
            return column_cell;
        }
    }
    nav_cell* cells[256]; // Magic number: rfg.exe's array.
    float best_dist_sq = 3.4028235e+38f;
    nav_cell* best = nullptr;
    // Magic number: 10 m around the position.
    int count = havok_collect_nav_cells(pos, 10.0f, cells, 256, vehicle_pfg);
    for (int i = 0; i < count; i++) {
        nav_cell* ncp = cells[i];
        for (int edge = 0; edge < 3; edge++) {
            if (ncp->adjacent_cells[edge].to_pointer() == nullptr && nav_cell_classify_point_vs_edge(ncp, edge, vector2(pos.x, pos.z)) == POINT_CLASS_LEFT) {
                float dist_sq = vec_find_closest_dist_squared_to_linesegment(pos, *nav_cell_get_vertex(ncp, edge, nullptr), *nav_cell_get_vertex(ncp, (edge + 1) % 3, nullptr), nullptr);
                if (dist_sq < best_dist_sq) {
                    best = ncp;
                    best_dist_sq = dist_sq;
                }
            }
        }
    }
    // Original quirk kept: a squared distance compared with a signed height difference.
    if (best != nullptr && (column_cell == nullptr || best_dist_sq <= column_delta)) {
        if (nav_cell_handle != nullptr) {
            *nav_cell_handle = nav_cell_get_handle(best);
        }
        return best;
    }
    if (column_cell != nullptr && nav_cell_handle != nullptr) {
        *nav_cell_handle = found_handle;
    }
    return column_cell;
}

// 0x00B15C80 (:7936)
void bb_pf_update_last_visited_nav_cell(vehicle* vp)
{
    bb_pf_update_last_visited_nav_cell(vp->m_navcell_info.last_visited_nav_cell_handle, vp->m_navcell_info.last_visited_nav_cell_update_timer, vp->pos, true);
}

// 0x00B15CA0 (:7945)
void bb_pf_update_last_visited_nav_cell(human* hp)
{
    if (hp->hflags.on_mover == 0 && hp->hflags.is_on_ladder == 0) {
        bb_pf_update_last_visited_nav_cell(hp->pf.last_visited_nav_cell_handle, hp->pf.last_visited_nav_cell_update_timer, hp->pos, false);
    }
}

// 0x00B0F3F0 (:7967)
void bb_pf_update_last_visited_nav_cell(uint64_t& last_visited_nav_cell_handle, timestamp& last_visited_nav_cell_update_timer, const vector& pos, bool vehicle_pfg)
{
    nav_cell* ncp = nav_cell_from_handle(last_visited_nav_cell_handle);
    if (ncp == nullptr) {
        if (!last_visited_nav_cell_update_timer.valid() || last_visited_nav_cell_update_timer.elapsed()) {
            uint64_t handle;
            bb_pf_get_nav_cell(pos, vehicle_pfg, &handle, true, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
            if (handle != 0xFFFFFFFFFFFFFFFFull) {
                last_visited_nav_cell_handle = handle;
            }
            // Magic numbers: the next try in half a second to a second.
            last_visited_nav_cell_update_timer.set_rand(500, 1000);
        }
    } else if (!nav_cell_is_point_in_cell_column(ncp, pos)) {
        const nav_cell* closest = get_closest_adjacent_nav_cell(ncp, pos, NAV_CELL_TRAVERSAL_ENABLED);
        if (closest != ncp) {
            last_visited_nav_cell_handle = nav_cell_get_handle(closest);
        }
    }
}

// 0x00B15CD0 (:8013)
bool bb_pf_is_point_visible(const vector& start_pos, const vector& end_pos, uint64_t start_nav_cell_hint_handle, uint64_t end_nav_cell_hint_handle, bool vehicle_pfg, bool* no_start_nav_cell)
{
    uint64_t start_handle = start_nav_cell_hint_handle;
    const nav_cell* hint = start_handle == 0xFFFFFFFFFFFFFFFFull ? nullptr : nav_cell_from_handle(start_handle);
    if (hint == nullptr || !nav_cell_is_point_in_cell_column(hint, start_pos)) {
        bb_pf_get_nav_cell(start_pos, vehicle_pfg, &start_handle, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
        if (start_handle == 0xFFFFFFFFFFFFFFFFull) {
            if (no_start_nav_cell != nullptr) {
                *no_start_nav_cell = true;
            }
            return false;
        }
    }
    uint64_t end_handle = end_nav_cell_hint_handle;
    hint = end_handle == 0xFFFFFFFFFFFFFFFFull ? nullptr : nav_cell_from_handle(end_handle);
    if (hint == nullptr || !nav_cell_is_point_in_cell_column(hint, end_pos)) {
        bb_pf_get_nav_cell(end_pos, vehicle_pfg, &end_handle, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
        if (end_handle == 0xFFFFFFFFFFFFFFFFull) {
            return false;
        }
    }
    return nav_cell_test_line_of_sight(start_handle, start_pos, end_handle, end_pos, 0);
}

// 0x00AF4660 (:8058)
path_node* rfg_pf_advance_to_next_path_node(path_node** path, path_node* node)
{
    if (node == nullptr) {
        return nullptr;
    }
    path_node* next = node->next != *path ? node->next : nullptr;
    bb_pf_free_path_node(path, node);
    return next;
}

// 0x00AEB910 (:8075)
path_node* bb_pf_get_free_path_node(const vector& pos, uint64_t nav_cell_handle, unsigned int navpoint_handle)
{
    path_node* node = Free_path_nodes;
    if (node == nullptr) {
        return nullptr;
    }
    node->pos = pos;
    node->nav_cell_handle = nav_cell_handle;
    node->navpoint_handle = navpoint_handle;
    node->normal.zero();
    node->compound_hull_handle = 0xFFFFFFFF;
    node->compound_hull_vert_index = -1;
    memset(&node->flags, 0, sizeof(node->flags));
    bb_pf_path_unlink(&Free_path_nodes, node);
    return node;
}

// 0x00AF4690 (:8113)
void bb_pf_free_path(path_node** path)
{
    path_node* node = *path;
    while (node != nullptr) {
        path_node* next = node->next != *path ? node->next : nullptr;
        bb_pf_path_unlink(path, node);
        bb_pf_path_append(&Free_path_nodes, node);
        node = next;
    }
}

// 0x00AEB9B0 (:8127)
void bb_pf_free_path_node(path_node** path, path_node* node)
{
    bb_pf_path_unlink(path, node);
    bb_pf_path_append(&Free_path_nodes, node);
}

// 0x00AF4720 (:8138)
static bool collect_only_hole_navpoints(object* hit_objp, unsigned int shape_key, const void* user_data)
{
    // Magic number: type 19, the navpoints of holes. Original quirk kept: only navpoints are ever collected.
    return hit_objp == nullptr || ((navpoint*)hit_objp)->type == 0x13;
}

// 0x00AF4750 (:8150)
static bool rfg_pf_repulsor_collector(object* repulsor, unsigned int shape_key, const void* user_data)
{
    human* hp = (human*)user_data;
    if (repulsor == hp || ai_formation_is_follower_of(repulsor, hp)) {
        return false;
    }
    switch (repulsor->get_obj_type()) {
    case OT_HUMAN: {
        human* other = (human*)repulsor;
        if (other->hflags.hidden != 0) {
            return false;
        }
        // Magic number: civilians walk around bodies only at a walk.
        if (human_is_dead(other) && (!human_team_is_non_combatant(hp) || 1 < (int)hp->cur_movement_state)) {
            return false;
        }
        if (human_in_vehicle(other) != nullptr) {
            return false;
        }
        return !human_should_ragdoll_other_human(hp, other);
    }
    case OT_MOVER:
        if (!rfg_pf_should_avoid_mover((object_mover*)repulsor)) {
            return false;
        }
        return !nav_cell_detour_object_is_flat(repulsor);
    case OT_VEHICLE:
    case 5:
        return true;
    default:
        return false;
    }
}

// 0x00AF4800 (:8223)
void rfg_pf_maybe_add_projectile_repulsors(human* hp)
{
    // Magic numbers: 8 repulsors at most, 6 beliefs read.
    if (8 <= hp->pf.num_path_repulsors || hp->get_obj_type() != OT_HUMAN || hp->get_sub_type() != OT_SUB_HUMAN_NPC) {
        return;
    }
    ai_system* ai = ((npc*)hp)->get_ai();
    if (ai == nullptr) {
        return;
    }
    // Something being fled from is not walked around: the flight has already taken it into account.
    unsigned int ignored = 0xFFFFFFFF;
    if (ai->planner.get_current_action() == flee_action::get_instance()) {
        ignored = ai->get_target_handle();
    }
    // Magic numbers: any handle, any age, no requirements (BELIEF_REQUIREMENTS_NONE, 0x01D680B4, is all zero).
    ai_belief_specification spec(AI_BELIEF_PROJECTILE, 0xFFFFFFFF, -1, ai_belief_requirements(0, 0, 0));
    ai_belief* beliefs[6];
    int count = ai->perception.collect_all_matching_beliefs(beliefs, 6, &spec, 1, 0xFFFFFFFF);
    for (int i = 0; i < count; i++) {
        if (7 < hp->pf.num_path_repulsors) {
            return;
        }
        if (beliefs[i]->handle == ignored) {
            continue;
        }
        projectile* proj = World.object_from_handle<projectile>(beliefs[i]->handle);
        if (proj == nullptr || proj->w_info == nullptr) {
            continue;
        }
        bool add = false;
        weapon_class_type weapon_class = proj->w_info->weapon_class;
        if (weapon_class == WEAPON_CLASS_THROWN_CHARGE) {
            // A charge stuck to a person or a vehicle moves on with them.
            if (proj->is_attached()) {
                object* parent = World.object_from_handle(proj->get_parent());
                add = parent == nullptr || (parent->get_obj_type() != OT_HUMAN && parent->get_obj_type() != OT_VEHICLE);
            }
        } else if (weapon_class == WEAPON_CLASS_THROWN_MINE || weapon_class == WEAPON_CLASS_SINGULARITY_BOMB) {
            add = true;
        }
        if (add) {
            hp->pf.path_repulsor_handles[hp->pf.num_path_repulsors] = proj->get_handle();
            hp->pf.num_path_repulsors++;
        }
    }
}

// The sum of the repulsion vectors of a set of compound repulsors, and the field timer's bookkeeping.
static int bb_pf_sum_repulsion(vector& sum, timestamp& field_timer, human* hp, const vector& pos, compound_repulsor* repulsors, int count, const vector& dir)
{
    bool inside = false;
    int active = 0;
    for (int i = 0; i < count; i++) {
        vector repulsion;
        if (calculate_repulsion_vector(&repulsion, hp, pos, &repulsors[i], dir)) {
            inside = true;
        }
        if (repulsion != ZERO_VECTOR) {
            active++;
            sum.x += repulsion.x;
            sum.y += repulsion.y;
            sum.z += repulsion.z;
        }
    }
    if (inside) {
        // Magic numbers: 6.25 to 7.75 seconds inside a field before giving up and backing out.
        if (!field_timer.valid()) {
            field_timer.set_rand(0x186A, 0x1E46);
        }
    } else if (field_timer.valid()) {
        field_timer.invalidate();
    }
    return active;
}

// 0x00B15DF0 (:8302)
vector bb_pf_calculate_turn_to_target(human* hp, const vector& dest, int& num_active_repulsors, compound_repulsor* active_compound_repulsors, bool check_start_nav_cell)
{
    // Magic numbers: the collection box is 5 m wide, 1.9 m tall (2 m up and down for holes) and reaches the destination;
    // it is collected again in 1 to 1.5 seconds divided by speed, between 0.2 and 1.5 seconds.
    const float BOX_HALF_WIDTH = 2.5f;
    const float BOX_HEIGHT = 1.9f;
    const float HOLE_BOX_HALF_HEIGHT = 2.0f;
    const int MIN_COLLECT_MS = 200;
    const int MAX_COLLECT_MS = 1500;
    vector result = dest;
    vector sum(0.0f, 0.0f, 0.0f);
    vector hole_sum(0.0f, 0.0f, 0.0f);
    vector pos = hp->pos;
    nav_cell* start_cell = nullptr;
    if (hp->hflags.on_mover == 0 && check_start_nav_cell) {
        start_cell = bb_pf_get_nav_cell_from_hint(&pos, nav_cell_from_handle(hp->pf.last_visited_nav_cell_handle), hp->pos, false, nullptr, false, false, NAV_CELL_TRAVERSAL_ENABLED);
        pos.y = hp->pos.y;
    }
    vector flat_dest(dest.x, hp->pos.y, dest.z);
    vector dir = flat_dest - pos;
    if (dir.is_null()) {
        dir.z = 1.0f;
    }
    float dist = dir.mag_and_normalize();
    float speed = hp->actual_velocity_mag;
    if (0.0f <= hp->max_speed - speed) {
        speed = hp->max_speed;
    }
    if (dir.is_valid() && speed != 0.0f) {
        bool recollected = false;
        if (!hp->pf.path_repulsors_collection_time.valid() || hp->pf.path_repulsors_collection_time.elapsed()) {
            recollected = true;
            float half = dist * 0.5f;
            vector bmin(-BOX_HALF_WIDTH, 0.0f, -half);
            vector bmax(BOX_HALF_WIDTH, BOX_HEIGHT, half);
            vector center(dir.x * half + pos.x, dir.y * half + pos.y, dir.z * half + pos.z);
            matrix orient;
            orient.make_quick(dir);
            hp->pf.num_path_repulsors = (unsigned char)havok_collect_objects_fast(bmin, bmax, center, orient, hp->pf.path_repulsor_handles, 8, COLLIDE_OBJECT_COLLECTOR_NO_ITEMS, rfg_pf_repulsor_collector, hp);
            if (hp->get_sub_type() == OT_SUB_HUMAN_NPC) {
                rfg_pf_maybe_add_projectile_repulsors(hp);
            }
            int delay = (int)((float)int_rand_range(1000, MAX_COLLECT_MS) / speed);
            if (delay <= MIN_COLLECT_MS) {
                delay = MIN_COLLECT_MS;
            } else if (MAX_COLLECT_MS <= delay) {
                delay = MAX_COLLECT_MS;
            }
            hp->pf.path_repulsors_collection_time.set(delay);
            if (hp->hflags.on_mover == 0) {
                hp->pf.num_hole_repulsors = 0;
            } else {
                vector hole_min(-BOX_HALF_WIDTH, -HOLE_BOX_HALF_HEIGHT, -half);
                vector hole_max(BOX_HALF_WIDTH, HOLE_BOX_HALF_HEIGHT, half);
                hp->pf.num_hole_repulsors = (unsigned char)World.collect_objects<navpoint>(hole_min, hole_max, center, orient, hp->pf.hole_repulsors, 8, collect_only_hole_navpoints, nullptr);
            }
        }
        // Two sets of 8 compound repulsors: 0x214 bytes each, as on rfg.exe's stack.
        compound_repulsor path_repulsors[8];
        compound_repulsor hole_repulsors[8];
        if (hp->pf.num_path_repulsors != 0) {
            int count = create_compound_repulsors(path_repulsors, hp, pos, flat_dest, hp->pf.num_path_repulsors, hp->pf.path_repulsor_handles, recollected);
            maybe_expand_player_repulsor(count, path_repulsors);
            int active = bb_pf_sum_repulsion(sum, hp->pf.repulsion_field_timer, hp, pos, path_repulsors, count, dir);
            if (hp->pf.repulsion_field_timer.valid() && hp->pf.repulsion_field_timer.elapsed()) {
                result = vector(pos.x - dir.x, pos.y - dir.y, pos.z - dir.z);
            } else if (sum != ZERO_VECTOR) {
                vector steer(dir.x * speed + sum.x, dir.y * speed + sum.y, speed * dir.z + sum.z);
                if (active == 1 && path_repulsors[0].num_objects == 1 && steer.y * dir.y + dir.x * steer.x + steer.z * dir.z < 0.0f) {
                    vector parallel;
                    vec_projection_parallel(parallel, steer, dir);
                    steer -= parallel;
                }
                if (start_cell != nullptr && 0 < count) {
                    vector2 flat(steer.x, steer.z);
                    repulsor_edge edges[16];
                    // Magic number: 10 cm around the human.
                    int num_edges = get_nearby_repulsor_edges(edges, start_cell, pos, 0.1f);
                    for (int i = 0; i < num_edges; i++) {
                        // Reflect off walls. Original quirk kept: the test keeps using the steering before reflections.
                        if (flat.y * edges[i].edge_rvec.y + flat.x * edges[i].edge_rvec.x < 0.0f) {
                            vector normal(edges[i].edge_rvec.x, 0.0f, edges[i].edge_rvec.y);
                            vector parallel;
                            vec_projection_parallel(parallel, steer, normal);
                            steer.x -= parallel.x * 2.0f;
                            steer.y -= parallel.y * 2.0f;
                            steer.z -= parallel.z * 2.0f;
                        }
                    }
                }
                result = vector(pos.x + steer.x, steer.y + pos.y, pos.z + steer.z);
            }
            num_active_repulsors = count;
            if (0 < count) {
                memcpy(active_compound_repulsors, path_repulsors, count * sizeof(compound_repulsor));
            }
        }
        if (hp->pf.num_hole_repulsors != 0) {
            int count = create_compound_repulsors(hole_repulsors, hp, pos, flat_dest, hp->pf.num_hole_repulsors, hp->pf.hole_repulsors, recollected);
            int active = bb_pf_sum_repulsion(hole_sum, hp->pf.hole_repulsion_field_timer, hp, pos, hole_repulsors, count, dir);
            if (hp->pf.hole_repulsion_field_timer.valid() && hp->pf.hole_repulsion_field_timer.elapsed()) {
                result = vector(pos.x - dir.x, pos.y - dir.y, pos.z - dir.z);
            } else if (hole_sum != ZERO_VECTOR) {
                vector steer(dir.x * speed + hole_sum.x, dir.y * speed + hole_sum.y, speed * dir.z + hole_sum.z);
                if (active == 1 && hole_repulsors[0].num_objects == 1 && steer.y * dir.y + steer.x * dir.x + steer.z * dir.z < 0.0f) {
                    vector parallel;
                    vec_projection_parallel(parallel, steer, dir);
                    steer -= parallel;
                }
                result.x += steer.x;
                result.y += steer.y;
                result.z += steer.z;
            }
        }
    }
    if (result == dest) {
        hp->pf.affected_by_repulsors = 0;
        hp->pf.repulsion_field_timer.invalidate();
        hp->pf.hole_repulsion_field_timer.invalidate();
    } else {
        hp->pf.affected_by_repulsors = 1;
    }
    return result;
}

// 0x00AEBA30 (:8772) LTCG: proj EAX
static float rfg_pf_get_projectile_repulsor_radius(const projectile* proj)
{
    // Magic numbers: stay outside what the explosion hurts, less 1 m for a charge and 2 m for a singularity bomb.
    switch (proj->w_info->weapon_class) {
    case WEAPON_CLASS_THROWN_CHARGE:
        return (float)((double)proj->w_info->m_explosion_info->m_radius - 1.0);
    case WEAPON_CLASS_THROWN_MINE:
        return Proxy_mine_radius;
    case WEAPON_CLASS_SINGULARITY_BOMB:
        return (float)((double)proj->w_info->m_explosion_info->m_knockdown_radius - 2.0);
    default:
        return 0.0f;
    }
}

// The box offset and size shared by the object cases of bb_pf_get_repulsor_oriented_bounding_box.
static void bb_pf_box_from_local_bbox(repulsor_oriented_bbox* oriented_bbox, const vector& bmin, const vector& bmax, const vector& origin, const matrix& orient)
{
    oriented_bbox->size = vector(bmax.x - bmin.x, bmax.y - bmin.y, bmax.z - bmin.z);
    vector center((bmin.x + bmax.x) * 0.5f, (bmax.y + bmin.y) * 0.5f, (bmax.z + bmin.z) * 0.5f);
    vector rotated = center.copy_unrotate(orient);
    oriented_bbox->pos = vector(rotated.x + origin.x, origin.y + rotated.y, origin.z + rotated.z);
}

// Whether an item is a thrown weapon with a repulsor radius.
static bool bb_pf_is_repulsor_projectile(const object* repulsor)
{
    return repulsor->get_obj_type() == OT_ITEM && repulsor->get_sub_type() == 8 && ((const projectile*)repulsor)->w_info != nullptr && ((const projectile*)repulsor)->w_info->m_explosion_info != nullptr;
}

// 0x00AF4950 (:8798)
bool bb_pf_get_repulsor_oriented_bounding_box(repulsor_oriented_bbox* oriented_bbox, const object* repulsor)
{
    // Magic numbers (metres): a standing human 0.5 wide and 1.9 tall, a body 3.8 wide and 0.5 tall.
    vector bmin, bmax;
    switch (repulsor->get_obj_type()) {
    case OT_HUMAN:
        oriented_bbox->pos = repulsor->pos;
        if (repulsor->is_killed()) {
            oriented_bbox->size = vector(3.8f, 0.5f, 3.8f);
            oriented_bbox->pos.y += 0.25f;
        } else {
            oriented_bbox->size = vector(0.5f, 1.9f, 0.5f);
            oriented_bbox->pos.y += 0.95f;
        }
        oriented_bbox->orient = repulsor->orient;
        return true;
    case OT_ITEM:
        if (bb_pf_is_repulsor_projectile(repulsor)) {
            float radius = rfg_pf_get_projectile_repulsor_radius((const projectile*)repulsor);
            if (0.0f < radius) {
                oriented_bbox->pos = repulsor->pos;
                oriented_bbox->orient = IDENTITY_MATRIX;
                // Magic number: 2 / sqrt(3): the cube that holds the sphere of the radius within its corners.
                radius *= 1.1547005f;
                oriented_bbox->size = vector(radius, radius, radius);
                return true;
            }
        }
        return false;
    case OT_MOVER:
        oriented_bbox->orient = repulsor->orient;
        repulsor->get_local_bbox(bmin, bmax);
        bb_pf_box_from_local_bbox(oriented_bbox, bmin, bmax, repulsor->pos, repulsor->orient);
        return true;
    case OT_VEHICLE: {
        vector origin;
        vehicle_get_aligned_box(bmin, bmax, origin, oriented_bbox->orient, (const vehicle*)repulsor);
        bb_pf_box_from_local_bbox(oriented_bbox, bmin, bmax, origin, oriented_bbox->orient);
        return true;
    }
    case 5:
        repulsor->get_local_bbox(bmin, bmax);
        bb_pf_box_from_local_bbox(oriented_bbox, bmin, bmax, repulsor->pos, repulsor->orient);
        oriented_bbox->orient = repulsor->orient;
        return true;
    case OT_NAVPOINT:
        oriented_bbox->pos = repulsor->pos;
        oriented_bbox->orient = repulsor->orient;
        repulsor->get_local_bbox(bmin, bmax);
        oriented_bbox->size = vector(bmax.x - bmin.x, bmax.y - bmin.y, bmax.z - bmin.z);
        return true;
    default:
        return false;
    }
}

// 0x00AF50A0 (:8899)
float bb_pf_get_repulsor_xz_radius(const object* repulsor, const repulsor_oriented_bbox& oriented_bbox)
{
    vector half(oriented_bbox.size.x * 0.5f, oriented_bbox.size.y * 0.5f, oriented_bbox.size.z * 0.5f);
    vector corner0(-half.x, -half.y, -half.z);
    vector corner1 = half;
    switch (repulsor->get_obj_type()) {
    case OT_HUMAN:
        return repulsor->is_killed() ? 1.9f : 0.5f; // Magic numbers: half a body's length, a person's width.
    case OT_ITEM:
        if (bb_pf_is_repulsor_projectile(repulsor)) {
            return rfg_pf_get_projectile_repulsor_radius((const projectile*)repulsor);
        }
        return 1.0f;
    case OT_MOVER: {
        corner0.unrotate(oriented_bbox.orient);
        corner1.unrotate(oriented_bbox.orient);
        corner0.y = 0.0f;
        corner1.y = 0.0f;
        // Magic numbers: half the diagonal for a piece of a building, three quarters for anything else.
        if (repulsor->get_sub_type() == OT_SUB_MOVER_RFG && (int)((const rfg_mover*)repulsor)->get_move_type() == 0) {
            return vec_dist(corner1, corner0) * 0.5f;
        }
        return vec_dist(corner1, corner0) * 0.75f;
    }
    case OT_VEHICLE:
    case 5:
        corner0.unrotate(oriented_bbox.orient);
        corner1.unrotate(oriented_bbox.orient);
        corner0.x += oriented_bbox.pos.x;
        corner0.z += oriented_bbox.pos.z;
        corner1.x += oriented_bbox.pos.x;
        corner1.z += oriented_bbox.pos.z;
        corner0.y = 0.0f;
        corner1.y = 0.0f;
        return vec_dist(corner1, corner0) * 0.5f;
    case OT_NAVPOINT:
        return ((const navpoint*)repulsor)->radius;
    default:
        return 0.0f;
    }
}

// 0x00AFC470 (:8971)
void bb_pf_check_for_obstructed_destinations()
{
    // Magic numbers: checked each second, with 0.5 m (0.25 squared) of movement and 0.25 m/s of speed the limits;
    // movement modes 2 and 3 are the path following ones; action alias 0x407 is the melee stance.
    int count = World.get_num_objects<npc>();
    for (int i = 0; i < count; i++) {
        npc* np = World.get_object<npc>(i);
        if (np == nullptr || np->is_killed() || np->hflags.hidden != 0) {
            continue;
        }
        ai_system* ai = np->get_ai();
        bool following = ((int)np->movement_mode == 2 || (int)np->movement_mode == 3) && np->pf.path != nullptr;
        if ((ai->is_in_player_squad(true) == nullptr && !following) || np->pf.dest_check_timer.time_since() <= 999) {
            continue;
        }
        np->hflags.use_bigsteps = 0;
        np->hflags.stuck = 0;
        float moved_sq = vec_dist_squared(np->pos, np->pf.last_dest_check_pos);
        np->pf.dest_check_timer.set(0);
        np->pf.last_dest_check_pos = np->pos;
        if (!np->char_inst->is_playing_action(0x407) && moved_sq <= 0.25f && np->actual_velocity_mag <= 0.25f) {
            np->hflags.stuck = 1;
            ai->perception.pf_dest_obstructed();
            np->hflags.use_bigsteps = 1;
            np->pf.flags.smoothing = 0;
        }
    }
}

// 0x00AFC5F0 (:9035)
void bb_pf_check_extended_stuck()
{
    // Magic numbers: 10 seconds within 3 m (9 squared); 5 seconds within 1.5 m (2.25 squared) on script or with the player.
    int count = World.get_num_objects<npc>();
    for (int i = 0; i < count; i++) {
        npc* np = World.get_object<npc>(i);
        if (np == nullptr || np->is_killed() || np->hflags.hidden != 0) {
            continue;
        }
        timestamp& timer = np->pf.extended_stuck_timer;
        if ((int)np->movement_mode == 2 || np->hflags.last_pf_failed != 0) {
            if (!timer.valid()) {
                timer.set(0);
                np->pf.extended_stuck_pos = np->pos;
            }
        } else if (timer.valid()) {
            np->pf.extended_stuck_cumulative += timer.time_since();
            timer.invalidate();
        }
        int stuck_ms = np->pf.extended_stuck_cumulative;
        if (timer.valid()) {
            stuck_ms += timer.time_since();
        }
        int limit = 10000;
        if (np->npc_flags.scripted_move != 0 || np->get_ai()->is_in_player_squad(true) != nullptr) {
            limit = 5000;
        }
        float moved_sq = vec_dist_squared(np->pos, np->pf.extended_stuck_pos);
        if (moved_sq <= (limit == 10000 ? 9.0f : 2.25f)) {
            if (limit <= stuck_ms) {
                np->hflags.extended_stuck = 1;
            }
        } else {
            timer.set(0);
            np->hflags.extended_stuck = 0;
            np->pf.extended_stuck_pos = np->pos;
            np->pf.extended_stuck_cumulative = 0;
        }
    }
}

// 0x00AEBAA0 (:9126)
int bb_pf_collect_obstructions(unsigned int* handles, int max_handles, npc* np, const vector& dest)
{
    vector bmin, bmax;
    np->get_local_bbox(bmin, bmax);
    unsigned int collected[8]; // Magic number: rfg.exe's array.
    int count = havok_collect_objects(bmin, bmax, dest, np->orient, collected, 8, COLLIDE_OBJECT_COLLECTOR_NO_ITEMS, nullptr, nullptr);
    int num = 0;
    for (int i = 0; i < count; i++) {
        if (max_handles <= num) {
            return num;
        }
        // Original quirk kept: the handle is not checked for a live object.
        object* objp = World.object_from_handle(collected[i]);
        bool obstructs = false;
        if (objp->get_obj_type() == OT_HUMAN) {
            obstructs = objp != np && !objp->is_killed() && ((human*)objp)->hflags.hidden == 0;
        } else if (objp->get_obj_type() == OT_MOVER) {
            if (objp->get_sub_type() != OT_SUB_MOVER_RFG) {
                vector velocity;
                havok_body_get_linear_velocity(objp->havok_handle, velocity);
                // Magic number: 0.1 m/s, squared.
                obstructs = velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z <= 0.010000001f;
            }
        } else if (objp->get_obj_type() == OT_VEHICLE) {
            float speed;
            vehicle_get_speed((vehicle*)objp, speed);
            obstructs = fabsf(speed) <= 0.1f; // Magic number: 0.1 m/s.
        }
        if (obstructs) {
            handles[num++] = objp->get_handle();
        }
    }
    return num;
}

// 0x00AEBC30 (:9209)
bool rfg_pf_should_avoid_mover(object_mover* mover)
{
    // Magic numbers: under 100 kg and 1.5 m wide a mover is kicked aside; flag 4 movers (furniture) under 500 kg and 3 m.
    if (mover->havok_handle == 0xFFFFFFFF) {
        return false;
    }
    bool large_limits = false;
    if ((mover->mover_flags & 4) != 0) {
        if ((mover->mover_flags & 2) != 0) {
            return true;
        }
        large_limits = true;
    }
    if (mover->get_mass() < (large_limits ? 500.0f : 100.0f)) {
        float limit = large_limits ? 3.0f : 1.5f;
        vector bmin, bmax;
        mover->get_local_bbox(bmin, bmax);
        if (bmax.x - bmin.x < limit && bmax.y - bmin.y < limit && bmax.z - bmin.z < limit) {
            return false;
        }
    }
    return true;
}

// 0x00AF5440 (:9444)
dynamic_hole_request::dynamic_hole_request()
{
    last_modified_timestamp.invalidate();
    devp = nullptr;
}

// 0x00AEBD60 (:9452)
void dynamic_hole_request::reset()
{
    last_modified_timestamp.invalidate();
    devp = nullptr;
}

// 0x00B03730 (:9458) LTCG: request ECX
static void remove_dynamic_hole_request(dynamic_hole_request* request)
{
    if (request != nullptr) {
        if (request->devp != nullptr) {
            request->devp->request = nullptr;
        }
        request->devp = nullptr;
        Dynamic_hole_requests.free(request);
    }
}

// 0x00B03760 (:9469)
static dynamic_hole_request* get_free_dynamic_hole_request()
{
    // A full pool drops its oldest request.
    if (Dynamic_hole_requests.get_free() < 1) {
        remove_dynamic_hole_request(Dynamic_hole_requests.get_first(0));
    }
    return Dynamic_hole_requests.alloc(0);
}

// 0x00B037C0 (:9484)
bool bb_pf_find_closest_navpoint(const vector& hit_pos, navpoint*& closest_navpoint, float& closest_dist_sq)
{
    // Magic numbers: navpoints within 5 m, 3 m under to 2.5 m over; types 10 to 17, without the dead ends of a transition.
    float best = 100.0f;
    closest_navpoint = nullptr;
    unsigned int handles[32];
    int count = World.collect_objects<navpoint>(hit_pos, 5.0f, handles, 32, nullptr, nullptr);
    for (int i = 0; i < count; i++) {
        navpoint* navp = handles[i] == 0xFFFFFFFF ? nullptr : World.object_from_handle<navpoint>(handles[i]);
        if (navp == nullptr || navp->type <= 9 || 0x12 <= navp->type) {
            continue;
        }
        if ((navp->type == NAVPOINT_TYPE_TRANSITION || navp->type == NAVPOINT_TYPE_LADDER_TRANSITION) && navp->num_links == 1) {
            continue;
        }
        float height = hit_pos.y - navp->pos.y;
        if (-2.5f <= height && height <= 3.0f) {
            float gap = vec_xz_dist(hit_pos, navp->pos) - navp->radius;
            if (gap < best) {
                closest_navpoint = navp;
                best = gap;
            }
        }
    }
    if (best < 0.0f) {
        best = 0.0f;
    }
    closest_dist_sq = best * best;
    return closest_navpoint != nullptr;
}

// 0x00AEBD70 (:9531) LTCG: closest_point EDI
static bool find_closest_point_on_link_from_navpoint_or_in_navpoint(const vector hit_pos, const navpoint* current_navpoint, navpoint*& closest_navpoint, vector& closest_point, float& closest_dist)
{
    closest_dist = 3.4028235e+38f; // Magic number: FLT_MAX.
    closest_navpoint = nullptr;
    closest_point = ZERO_VECTOR;
    vector dir = hit_pos - current_navpoint->pos;
    float gap = dir.mag_and_normalize_safe() - current_navpoint->radius;
    if (0 < current_navpoint->num_links && 0.0f <= gap) {
        bool any = false;
        for (int i = 0; i < current_navpoint->num_links; i++) {
            navpoint* link = current_navpoint->links[i];
            if (link == nullptr) {
                continue;
            }
            // Hole navpoints count only when they join more of the graph than the transition that made them.
            if (link->type == 0x12 || link->type == 0x13) {
                if (link->num_links == 1) {
                    continue;
                }
                if (link->num_links == 2) {
                    const navpoint* a = link->links[0];
                    const navpoint* b = link->links[1];
                    if ((a != nullptr && (a->type == NAVPOINT_TYPE_TRANSITION || a->type == NAVPOINT_TYPE_LADDER_TRANSITION))
                        || (b != nullptr && (b->type == NAVPOINT_TYPE_TRANSITION || b->type == NAVPOINT_TYPE_LADDER_TRANSITION))) {
                        continue;
                    }
                }
            }
            any = true;
            vector closest = ZERO_VECTOR;
            ix_closest_point_on_line_segment(closest, hit_pos, current_navpoint->pos, link->pos);
            float dist = vec_dist(closest, hit_pos);
            if (dist < closest_dist) {
                closest_navpoint = link;
                closest_dist = dist;
                closest_point = closest;
            }
        }
        if (any) {
            return closest_navpoint != nullptr;
        }
    }
    closest_dist = gap;
    float radius = current_navpoint->radius;
    closest_point = vector(current_navpoint->pos.x + dir.x * radius, current_navpoint->pos.y + dir.y * radius, current_navpoint->pos.z + dir.z * radius);
    return true;
}

// A navpoint made at run time under a parent. obj_nav_props is inlined at most of rfg.exe's call sites.
static navpoint* bb_pf_create_navpoint(const vector& pos, const matrix& orient, signed char type, unsigned int parent)
{
    obj_nav_props props(pos, orient, type);
    return World.create_object<navpoint>(props, parent);
}

// 0x00AF5450 (:9592) LTCG: closest_navpoint EAX
static navpoint* create_navpoints_for_hole(const vector hit_pos, const vector closest_point, navpoint* closest_navpoint, navpoint* second_navpoint, const float closest_dist, bool is_wall, float new_radius)
{
    // Magic numbers: types 0x12 and 0x13 are the hole navpoints of walls and floors, 0x15 a junction on a link.
    const signed char TYPE_JUNCTION = 0x15;
    const signed char type = is_wall ? 0x12 : 0x13;
    matrix orient;
    orient.make_quick(vector(1.0f, 0.0f, 0.0f));
    if (closest_navpoint->set == 0) {
        if (second_navpoint == nullptr || second_navpoint->set == 0) {
            closest_navpoint->set = get_next_navpoint_set_id();
            closest_navpoint->propogate_set_info();
        } else {
            closest_navpoint->set = second_navpoint->set;
        }
    }
    float dist_first = vec_xz_dist(closest_navpoint->pos, hit_pos);
    float dist_second = second_navpoint != nullptr ? vec_xz_dist(second_navpoint->pos, hit_pos) : 3.4028235e+38f; // Magic number: FLT_MAX.
    float first_radius = closest_navpoint->radius;
    float closest_to_first_sq = vec_xz_dist_squared(closest_navpoint->pos, closest_point);
    bool in_second = second_navpoint != nullptr && vec_xz_dist_squared(second_navpoint->pos, closest_point) < second_navpoint->radius * second_navpoint->radius;
    float radius = closest_navpoint->radius;
    if (second_navpoint != nullptr && second_navpoint->radius <= radius) {
        radius = second_navpoint->radius;
    }
    float link_gap = closest_dist - radius;
    navpoint* node;
    navpoint* base = closest_navpoint;
    bool take_links = false;
    if (!(0.0f < dist_first - radius)) {
        node = bb_pf_create_navpoint(hit_pos, orient, type, closest_navpoint->get_parent());
        if (node == nullptr) {
            return nullptr;
        }
        link_navpoints(closest_navpoint, node, true);
        take_links = true;
    } else if (!(0.0f < dist_second - radius)) {
        node = bb_pf_create_navpoint(hit_pos, orient, type, second_navpoint->get_parent());
        if (node == nullptr) {
            return nullptr;
        }
        link_navpoints(second_navpoint, node, true);
        take_links = true;
        base = second_navpoint;
    } else if (closest_to_first_sq < first_radius * first_radius || dist_first - radius <= link_gap) {
        node = bb_pf_create_navpoint(hit_pos, orient, type, closest_navpoint->get_parent());
        if (node == nullptr) {
            return nullptr;
        }
        link_navpoints(closest_navpoint, node, true);
        take_links = true;
    } else if (in_second || dist_second - radius <= link_gap) {
        node = bb_pf_create_navpoint(hit_pos, orient, type, second_navpoint->get_parent());
        if (node == nullptr) {
            return nullptr;
        }
        link_navpoints(second_navpoint, node, true);
        take_links = true;
        base = second_navpoint;
    } else if (0.0f < link_gap && second_navpoint != nullptr) {
        // The hole lies off the link: a junction on the link, and the hole's navpoint beyond it.
        vector dir = hit_pos - closest_point;
        dir.normalize_safe();
        matrix out_orient, in_orient;
        out_orient.make_quick(dir);
        in_orient.make_quick(vector(-dir.x, -dir.y, -dir.z));
        navpoint* junction = bb_pf_create_navpoint(closest_point, out_orient, TYPE_JUNCTION, closest_navpoint->get_parent());
        if (junction == nullptr) {
            return nullptr;
        }
        node = bb_pf_create_navpoint(hit_pos, in_orient, type, closest_navpoint->get_parent());
        if (node == nullptr) {
            return nullptr;
        }
        junction->set = closest_navpoint->set;
        remove_navpoint_link(closest_navpoint, second_navpoint);
        link_navpoints(closest_navpoint, junction, true);
        link_navpoints(second_navpoint, junction, true);
        link_navpoints(node, junction, true);
    } else if (!(0.0f < link_gap) && second_navpoint != nullptr) {
        node = bb_pf_create_navpoint(hit_pos, orient, type, closest_navpoint->get_parent());
        if (node == nullptr) {
            return nullptr;
        }
        // A narrower navpoint leaves the wide link standing; a wider one replaces it.
        if (!(new_radius < radius)) {
            remove_navpoint_link(closest_navpoint, second_navpoint);
        }
        link_navpoints(closest_navpoint, node, true);
        link_navpoints(second_navpoint, node, true);
    } else {
        vector dir = hit_pos - closest_point;
        dir.normalize_safe();
        matrix in_orient;
        in_orient.make_quick(vector(-dir.x, -dir.y, -dir.z));
        node = bb_pf_create_navpoint(hit_pos, in_orient, type, closest_navpoint->get_parent());
        if (node == nullptr) {
            return nullptr;
        }
        link_navpoints(node, closest_navpoint, true);
    }
    node->set = closest_navpoint->set;
    node->radius = new_radius;
    if (!is_wall && take_links) {
        // A floor hole takes over the links whose walkway it falls in.
        navpoint* taken[10]; // Magic number: rfg.exe's array; navpoints have fewer links than that.
        int num_taken = 0;
        for (int i = 0; i < base->num_links; i++) {
            navpoint* link = base->links[i];
            if (link == nullptr || link == node || link->type == 0x13 || link->type == 0x14 || link->type == 0x15 || link->type == NAVPOINT_TYPE_TRANSITION_JUMP || link->type == NAVPOINT_TYPE_LEAP_OVER) {
                continue;
            }
            if (vec_find_closest_dist_squared_to_line(node->pos, base->pos, link->pos, nullptr) < new_radius * new_radius) {
                taken[num_taken++] = link;
            }
        }
        for (int i = 0; i < num_taken; i++) {
            remove_navpoint_link(base, taken[i]);
            link_navpoints(node, taken[i], true);
        }
    }
    return node;
}

// 0x00B0F4B0 (:9786) LTCG: new_node ESI
static void handle_hole_in_floor(navpoint* new_node, nav_cell* closest_nav_cell, vector navmesh_pos)
{
    // Magic numbers: the ray goes from 1 m above to 6 m below; a floor between 1.41 m and 5 m under the hole is another
    // storey (2 and 25 squared); a nav mesh within 5.5 m can be jumped down to; 0x14 and 0xF are drop and jump navpoints.
    const signed char TYPE_DROP = 0x14;
    raycast_input in;
    in.start = vector(new_node->pos.x, new_node->pos.y + 1.0f, new_node->pos.z);
    in.end = vector(new_node->pos.x, new_node->pos.y - 6.0f, new_node->pos.z);
    in.radius = 0.0f;
    in.filter_params = nullptr;
    in.filter_callback = nullptr;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 0;
    in.m_filter_info = COLLIDE_RAY_WORLD_ONLY;
    raycast_output out;
    matrix orient;
    orient.make_quick(vector(1.0f, 0.0f, 0.0f));
    if (havok_raycast_against_static_level_only(in, out)) {
        for (int i = 0; i < out.m_hits.getSize(); i++) {
            object* objp = World.object_from_handle(out.m_hits[i].hit_object_handle);
            if (objp->get_obj_type() != OT_MOVER || objp->get_sub_type() != OT_SUB_MOVER_RFG || (int)((rfg_mover*)objp)->get_move_type() != 0) {
                continue;
            }
            // Original quirk kept: the first hit's position, whichever hit matched.
            vector hit_pos = out.m_hits[0].hit_pos;
            float dist_sq = vec_dist_squared(hit_pos, new_node->pos);
            if (dist_sq <= 2.0f || 25.0f < dist_sq) {
                return;
            }
            navpoint* other = nullptr;
            navpoint* below = rfg_pf_find_closest_navpoint_to_pos(hit_pos, other, 10, NAVPOINT_TYPE_STAIRS);
            if (below == nullptr) {
                return;
            }
            if (other == nullptr) {
                navpoint* drop = bb_pf_create_navpoint(hit_pos, orient, TYPE_DROP, new_node->get_parent());
                if (drop == nullptr) {
                    return;
                }
                drop->set = new_node->set;
                link_navpoints(new_node, drop, true);
                link_navpoints(below, drop, true);
                if (below->set != new_node->set) {
                    drop->propogate_set_info();
                }
                return;
            }
            float radius = other->radius;
            if (below->radius < radius) {
                radius = below->radius;
            }
            vector on_link;
            ix_closest_point_on_line_segment(on_link, hit_pos, below->pos, other->pos);
            if (vec_xz_dist(on_link, hit_pos) - radius <= 0.0f) {
                navpoint* drop = bb_pf_create_navpoint(on_link, orient, TYPE_DROP, new_node->get_parent());
                if (drop == nullptr) {
                    return;
                }
                remove_navpoint_link(below, other);
                drop->radius = radius;
                drop->set = new_node->set;
                link_navpoints(drop, new_node, true);
                link_navpoints(below, drop, true);
                link_navpoints(other, drop, true);
                if (below->set != new_node->set) {
                    drop->propogate_set_info();
                }
                return;
            }
            navpoint* drop = bb_pf_create_navpoint(hit_pos, orient, TYPE_DROP, new_node->get_parent());
            if (drop == nullptr) {
                return;
            }
            navpoint* junction = bb_pf_create_navpoint(on_link, orient, NAVPOINT_TYPE_PATH, new_node->get_parent());
            if (junction == nullptr) {
                return;
            }
            remove_navpoint_link(below, other);
            junction->radius = radius;
            drop->set = new_node->set;
            junction->set = new_node->set;
            link_navpoints(drop, new_node, true);
            link_navpoints(drop, junction, true);
            link_navpoints(junction, below, true);
            link_navpoints(junction, other, true);
            if (below->set != junction->set) {
                junction->propogate_set_info();
            }
            return;
        }
    }
    if (closest_nav_cell == nullptr && bb_pf_get_nav_cell(new_node->pos, false, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, &navmesh_pos) == nullptr) {
        return;
    }
    if (new_node->pos.y - navmesh_pos.y <= 5.5f) {
        matrix down_orient;
        down_orient.make_quick(FORWARD_VECTOR);
        navpoint* jump = bb_pf_create_navpoint(navmesh_pos, down_orient, NAVPOINT_TYPE_TRANSITION_JUMP, new_node->get_parent());
        if (jump == nullptr) {
            return;
        }
        link_navpoints(new_node, jump, true);
        jump->set = new_node->set;
    }
}

// Gives up on a wall hole that leads nowhere.
static void bb_pf_destroy_hole_navpoint(navpoint* new_node)
{
    // Original bug kept: each removal moves the last link into the freed slot and lowers the count, so this skips half of them.
    for (int i = 0; i < new_node->num_links; i++) {
        remove_navpoint_link(new_node, new_node->links[i]);
    }
    World.destroy_object(new_node, false);
}

// 0x00B03970 (:9942) LTCG: new_node ESI
static void handle_hole_in_interior_wall(navpoint* new_node, matrix orient_of_wall)
{
    // Magic numbers: the other side of the wall is 1.5 m away; cosines of 75 and 30 degrees; 4 m and 2.5 m of walkable gap.
    const float PROBE_DIST = 1.5f;
    const float COS_75 = 0.25881904f;
    const float COS_30 = 0.8660254f;
    if (new_node->num_links < 1) {
        return;
    }
    vector to_link = new_node->links[0]->pos - new_node->pos;
    to_link.normalize_safe();
    vector normal = orient_of_wall.fvec;
    if (normal.x * to_link.x * -1.0f + normal.y * to_link.y * -1.0f + to_link.z * -1.0f * normal.z <= 0.0f) {
        normal = vector(-normal.x, -normal.y, -normal.z);
    }
    vector probe(normal.x * PROBE_DIST + new_node->pos.x, new_node->pos.y + normal.y * PROBE_DIST, new_node->pos.z + normal.z * PROBE_DIST);
    navpoint* other = nullptr;
    navpoint* closest = rfg_pf_find_closest_navpoint_to_pos(probe, other, 10, NAVPOINT_TYPE_STAIRS);
    if (closest == nullptr) {
        return;
    }
    for (int i = 0; i < new_node->num_links; i++) {
        if (new_node->links[i] == closest || (other != nullptr && new_node->links[i] == other)) {
            bb_pf_destroy_hole_navpoint(new_node);
            return;
        }
    }
    if (closest->type == NAVPOINT_TYPE_STAIRS && other != nullptr && other->type == NAVPOINT_TYPE_STAIRS) {
        bb_pf_destroy_hole_navpoint(new_node);
        return;
    }
    matrix orient;
    orient.make_quick(FORWARD_VECTOR);
    if (other == nullptr) {
        vector back = new_node->links[0]->pos - new_node->pos;
        back = vector(back.x * -1.0f, back.y * -1.0f, back.z * -1.0f);
        back.normalize_safe();
        vector to_closest = closest->pos - probe;
        float facing = to_closest.x * back.x + to_closest.y * back.y + to_closest.z * back.z;
        if (facing < COS_75) {
            bb_pf_destroy_hole_navpoint(new_node);
            return;
        }
        float gap = vec_dist(probe, closest->pos) - closest->radius;
        if (COS_30 < facing) {
            if (4.0f < gap) {
                bb_pf_destroy_hole_navpoint(new_node);
                return;
            }
            link_navpoints(closest, new_node, true);
            if (new_node->set != closest->set) {
                new_node->propogate_set_info();
            }
            return;
        }
        if (0.0f <= gap && 2.5f < gap) {
            bb_pf_destroy_hole_navpoint(new_node);
            return;
        }
        navpoint* node = bb_pf_create_navpoint(probe, orient, NAVPOINT_TYPE_PATH, new_node->get_parent());
        if (node == nullptr) {
            return;
        }
        link_navpoints(node, closest, true);
        link_navpoints(node, new_node, true);
        node->set = closest->set;
        if (new_node->set != closest->set) {
            new_node->propogate_set_info();
        }
        return;
    }
    vector on_link = ZERO_VECTOR;
    if (ix_closest_point_on_line_segment(on_link, new_node->pos, closest->pos, other->pos) == 0.0f) {
        link_navpoints(new_node, closest, true);
        if (new_node->set != closest->set) {
            new_node->propogate_set_info();
        }
        return;
    }
    navpoint* node = bb_pf_create_navpoint(on_link, orient, NAVPOINT_TYPE_PATH, new_node->get_parent());
    if (node == nullptr) {
        return;
    }
    remove_navpoint_link(closest, other);
    link_navpoints(closest, node, true);
    link_navpoints(other, node, true);
    link_navpoints(node, new_node, true);
    node->set = closest->set;
    if (new_node->set != closest->set) {
        new_node->propogate_set_info();
    }
}

// 0x00AEC070 (:10127)
bool walls_floors_collect_cb(object* obj, unsigned int shape_key, const void* cb_data)
{
    // Magic numbers: the hole navpoint types.
    signed char type = ((navpoint*)obj)->type;
    return type == 0x12 || type == 0x13;
}

// The event rfg.exe posts for a bridge, its object's handle after the type. It has no name in rfg.pdb.
struct bridge_unusable_event : public event_data
{
    bridge_unusable_event(unsigned int _handle) : event_data(EVENT_BRIDGE_BECOMES_UNUSABLE), handle(_handle) {}
    unsigned int handle;
};

// Whether a point at the side of a road falls in or within half the road's width of the destroyed box.
static bool bb_pf_road_side_in_event(const vector& point, const rfg_destruction_event* devp, float width)
{
    if (ix_point_in_box_oriented(point, devp->pos, devp->orient, devp->bmin, devp->bmax)) {
        return true;
    }
    vector local = point;
    vec_change_to_local_coords(local, devp->pos, devp->orient);
    double reach = width * 0.5f;
    return (double)bbox_find_xz_distance_squared(devp->bmin, devp->bmax, local) < reach * reach;
}

// 0x00AFC7B0 (:10138) LTCG: request EAX
static void dynamic_hole_request_process_bridge_event(dynamic_hole_request* request)
{
    // Magic numbers: a bridge's event reaches a road up to 2.5 m under it; 322 roads, the capacity of the farray; road
    // navpoint type 22 is ROAD_BRIDGE.
    const rfg_destruction_event* devp = request->devp;
    vector center((devp->bmax.x + devp->bmin.x) * 0.5f, (devp->bmax.y + devp->bmin.y) * 0.5f, (devp->bmax.z + devp->bmin.z) * 0.5f);
    vec_change_to_global_coords(center, devp->pos, devp->orient);
    float extent_z = fabsf(devp->bmin.z) < fabsf(devp->bmax.z) ? fabsf(devp->bmax.z) : fabsf(devp->bmin.z);
    float extent_y = fabsf(devp->bmin.y) < fabsf(devp->bmax.y) ? fabsf(devp->bmax.y) : fabsf(devp->bmin.y);
    float extent_x = fabsf(devp->bmin.x) < fabsf(devp->bmax.x) ? fabsf(devp->bmax.x) : fabsf(devp->bmin.x);
    float extent = sqrtf(extent_y * extent_y + extent_x * extent_x + extent_z * extent_z);
    object_path_road* roads[322];
    int num_roads = 0;
    int num_all = World.get_num_objects<object_path_road>();
    for (int i = 0; i < num_all; i++) {
        object_path_road* road = World.get_object<object_path_road>(i);
        vector bmin, bmax;
        road->get_bbox(bmin, bmax);
        if (ix_sphere_in_box(devp->pos, extent, bmin, bmax) && road->road_flags.special == 0 && road->road_flags.gps_only == 0 && num_roads < 322) {
            roads[num_roads++] = road;
        }
    }
    for (int i = 0; i < num_roads; i++) {
        object_path_road* road = roads[i];
        vector closest = ZERO_VECTOR;
        float u = road->find_closest_point_on_road(center, closest, true);
        if (closest.y < devp->pos.y + devp->bmin.y - 2.5f) {
            continue;
        }
        float width = road->get_width(u);
        int index = (int)floor((double)u);
        if (index == road->spline_data.num_points - 1) {
            continue;
        }
        road_broken_state state = road->is_broken_at_index(index);
        if (state == ROAD_BROKEN_COMPLETELY) {
            continue;
        }
        double width_d = width;
        if (!((double)vec_xz_dist_squared(center, closest) <= width_d * width_d) && !ix_point_in_box_oriented(closest, devp->pos, devp->orient, devp->bmin, devp->bmax)) {
            continue;
        }
        vector tangent;
        road->get_tangent(tangent, u);
        tangent.normalize_safe();
        vector side(tangent.y * UP_VECTOR.z - tangent.z * UP_VECTOR.y, tangent.z * UP_VECTOR.x - tangent.x * UP_VECTOR.z, tangent.x * UP_VECTOR.y - tangent.y * UP_VECTOR.x);
        vector one(side.x * width + closest.x, side.y * width + closest.y, side.z * width + closest.z);
        vector two(-side.x * width + closest.x, -side.y * width + closest.y, -side.z * width + closest.z);
        bool hit_one = bb_pf_road_side_in_event(one, devp, width);
        bool hit_two = bb_pf_road_side_in_event(two, devp, width);
        // A side already broken adds to what this event breaks.
        if (state == ROAD_BROKEN_LEFT && hit_one) {
            hit_two = true;
        } else if (state == ROAD_BROKEN_RIGHT && hit_two) {
            hit_one = true;
        }
        if (!hit_one && !hit_two) {
            continue;
        }
        if (!(hit_one && hit_two)) {
            road->break_road(index, hit_one ? ROAD_BROKEN_RIGHT : ROAD_BROKEN_LEFT);
            continue;
        }
        road->break_road(index, ROAD_BROKEN_COMPLETELY);
        object_mover* bridge = oml_find_nearest_mover(center, MOVER_TYPE_BRIDGE);
        if (bridge != nullptr) {
            object* first = bridge->child_ptr;
            object* child = first;
            while (child != nullptr) {
                if (child->get_obj_type() == OT_NAVPOINT && ((navpoint*)child)->type == NAVPOINT_TYPE_ROAD_BRIDGE && ((navpoint*)child)->num_links == 1) {
                    remove_navpoint_link((navpoint*)child, ((navpoint*)child)->links[0]);
                    break;
                }
                child = child->child_next;
                if (child == first) {
                    break;
                }
            }
            bridge_unusable_event event(bridge->get_handle());
            gem_post_event(event);
        }
    }
}

// 0x00B040A0 (:11305)
void add_dynamic_hole_request(rfg_destruction_event* devp)
{
    if (Multiplayer) {
        return;
    }
    dynamic_hole_request* request = devp->request;
    if (request != nullptr) {
        request->last_modified_timestamp.set(0);
        if (devp->flags.is_bridge != 0) {
            request->flags.processed = 0;
        }
        request->flags.pass_1 = 0;
        request->flags.pass_2 = 0;
        return;
    }
    request = get_free_dynamic_hole_request();
    if (request != nullptr) {
        request->last_modified_timestamp.set(0);
        request->devp = devp;
        devp->request = request;
        request->flags.processed = 0;
        request->flags.pass_2 = 0;
        request->flags.pass_1 = 0;
        request->flags.removed = 0;
    }
}

// 0x00B04100 (:11337)
void remove_dynamic_hole_request(rfg_destruction_event* devp)
{
    remove_dynamic_hole_request(devp->request);
}

// 0x00B04130 (:11346)
void dynamic_hole_requests_level_init_post()
{
    dynamic_hole_request* request = Dynamic_hole_requests.get_first(0);
    while (request != nullptr) {
        remove_dynamic_hole_request(request);
        request = Dynamic_hole_requests.get_first(0);
    }
}

// 0x00B04190 (:11358) LTCG: devp ECX
static void check_nav_links_against_destruction_event(rfg_destruction_event* devp)
{
    // Magic numbers: boxes grow by 10 cm; a bridge's by 0.9 m upward; boxes under 7.5 m search 3.25 m further.
    const float MIN_SEARCH_SPAN = 7.5f;
    const float SEARCH_GROWTH = 3.25f;
    vector bmin = devp->bmin;
    vector bmax = devp->bmax;
    vector search_min = devp->bmin;
    vector search_max = devp->bmax;
    vector growth(0.1f, 0.1f, 0.1f);
    bbox_grow(bmin, bmax, growth);
    vector negative(-growth.x, -growth.y, -growth.z);
    bbox_grow(bmin, bmax, negative);
    if (devp->flags.is_bridge != 0) {
        vector up(0.0f, 0.9f, 0.0f);
        bbox_grow(bmin, bmax, up);
    }
    for (int axis = 0; axis < 3; axis++) {
        if ((&search_max.x)[axis] - (&search_min.x)[axis] < MIN_SEARCH_SPAN) {
            vector plus(0.0f, 0.0f, 0.0f);
            (&plus.x)[axis] = SEARCH_GROWTH;
            bbox_grow(search_min, search_max, plus);
            vector minus(0.0f, 0.0f, 0.0f);
            (&minus.x)[axis] = -SEARCH_GROWTH;
            bbox_grow(search_min, search_max, minus);
        }
    }
    unsigned int handles[32];
    int count = World.collect_objects<navpoint>(search_min, search_max, devp->pos, devp->orient, handles, 32, nullptr, nullptr);
    for (int i = 0; i < count; i++) {
        navpoint* navp = handles[i] == 0xFFFFFFFF ? nullptr : World.object_from_handle<navpoint>(handles[i]);
        if (navp == nullptr || navp->type <= 9 || navp->type == NAVPOINT_TYPE_TRANSITION) {
            continue;
        }
        for (int l = 0; l < navp->num_links; l++) {
            navpoint* link = navp->links[l];
            if (link == nullptr || link->type == NAVPOINT_TYPE_TRANSITION) {
                continue;
            }
            vector start = navp->pos;
            vec_change_to_local_coords(start, devp->pos, devp->orient);
            vector end = link->pos;
            vec_change_to_local_coords(end, devp->pos, devp->orient);
            // Original bug kept: rfg.exe grows a copy of the box by the walkway's radius, then tests the ungrown one.
            vector intercept;
            if (ix_linesegment_boundingbox(intercept, bmin, bmax, start, end)) {
                navp->link_disabled_by[l] |= 1 << NPDR_DESTRUCTION;
                for (int back = 0; back < link->num_links; back++) {
                    if (link->links[back] == navp) {
                        link->link_disabled_by[back] |= 1 << NPDR_DESTRUCTION;
                        break;
                    }
                }
            }
        }
    }
}

// 0x00B119E0 (:11441)
void dynamic_hole_requests_do_frame()
{
    // Magic numbers: an event is looked at 3, 9 and 27 seconds after its last change, with up to 2 per frame and 150 dynamic
    // navpoints at most. A box must be over 1.5 m wide to let anybody through.
    const int MAX_DYNAMIC_NAVPOINTS = 150;
    const float MIN_HOLE_SIZE = 1.5f;
    if (Multiplayer) {
        return;
    }
    int processed = 0;
    bool made_hole = false;
    Dynamic_hole_requests.thread_access.enter();
    dynamic_hole_request* next = Dynamic_hole_requests.get_first(0);
    while (next != nullptr) {
        dynamic_hole_request* request = next;
        next = Dynamic_hole_requests.get_next(request);
        int since = request->last_modified_timestamp.time_since();
        if ((request->flags.pass_2 != 0 && since < 27000) || (request->flags.pass_1 != 0 && since < 9000) || since < 3000) {
            continue;
        }
        rfg_destruction_event* devp = request->devp;
        check_nav_links_against_destruction_event(devp);
        if (devp->flags.is_bridge != 0) {
            dynamic_hole_request_process_bridge_event(request);
            request->flags.processed = 1;
        } else if (navpoint_get_dynamic_navpoint_count() < MAX_DYNAMIC_NAVPOINTS) {
            if (MIN_HOLE_SIZE < devp->bmax.x - devp->bmin.x || MIN_HOLE_SIZE < devp->bmax.z - devp->bmin.z) {
                dynamic_hole_request_process_destruction_event(request);
                made_hole = true;
            } else {
                request->flags.processed = 1;
            }
            cover_dyn_process_destruction_event(devp);
            action_node_process_destruction_event(devp);
            guard_node_process_destruction_event(devp);
        }
        if (request->flags.removed != 0) {
            remove_destruction_event(devp);
        } else if (request->flags.pass_1 == 0) {
            request->flags.pass_1 = 1;
        } else if (request->flags.pass_2 != 0) {
            remove_dynamic_hole_request(request);
        } else {
            request->flags.pass_2 = 1;
        }
        processed++;
        if (1 < processed || made_hole) {
            break;
        }
    }
    Dynamic_hole_requests.thread_access.leave();
}

// 0x00B04760 (:11554)
void dynamic_hole_requests_remove_for_building(rfg_mover* rfgmp)
{
    vector bmin, bmax;
    rfgmp->get_bbox(bmin, bmax);
    Dynamic_hole_requests.thread_access.enter();
    dynamic_hole_request* next = Dynamic_hole_requests.get_first(0);
    while (next != nullptr) {
        dynamic_hole_request* request = next;
        next = Dynamic_hole_requests.get_next(request);
        rfg_destruction_event* devp = request->devp;
        if (devp == nullptr) {
            request->devp = nullptr;
            Dynamic_hole_requests.free(request);
        } else {
            vector event_min, event_max;
            bbox_instance(event_min, event_max, devp->orient, devp->pos, devp->bmin, devp->bmax);
            if (bmin.x < event_max.x && bmin.y < event_max.y && bmin.z < event_max.z && event_min.x < bmax.x && event_min.y < bmax.y && event_min.z < bmax.z) {
                if (request->flags.processed == 0) {
                    request->flags.removed = 1;
                } else {
                    remove_destruction_event(devp);
                }
            }
        }
    }
    Dynamic_hole_requests.thread_access.leave();
    RFG_destruction_events.thread_access.enter();
    rfg_destruction_event* next_event = RFG_destruction_events.get_first(0);
    while (next_event != nullptr) {
        rfg_destruction_event* event = next_event;
        next_event = RFG_destruction_events.get_next(event);
        vector event_min, event_max;
        bbox_instance(event_min, event_max, event->orient, event->pos, event->bmin, event->bmax);
        if (bmin.x < event_max.x && bmin.y < event_max.y && bmin.z < event_max.z && event_min.x < bmax.x && event_min.y < bmax.y && event_min.z < bmax.z && event->request == nullptr) {
            remove_destruction_event(event);
        }
    }
    RFG_destruction_events.thread_access.leave();
}

// 0x00B16E80 (:11613) LTCG: path EAX
static void simplify_vehicle_nav_path(path_node** path, const pf_calc_flags& flags)
{
    nav_cell_traversal_rule rule = NAV_CELL_TRAVERSAL_ENABLED;
    if (flags.use_disabled_cells != 0) {
        rule = NAV_CELL_TRAVERSAL_ALL;
    } else if (flags.use_light_structure_disabled_cells != 0) {
        rule = NAV_CELL_TRAVERSAL_ENABLED_PLUS_LIGHT;
    }
    if (Disable_vehicle_smoothing) {
        pf_calc_flags los_flags = flags;
        los_flags.ignore_detour_hulls = 1;
        simplify_nav_path_by_los(path, los_flags);
        vehicle_path_flatten(path, rule);
        return;
    }
    // Magic numbers: only the first 150 m and one vehicle node.
    simplify_nav_path(path, flags, true, 150.0f, 1);
    // The nodes beyond the optimized part wait outside while the rest is flattened.
    path_node* rest = nullptr;
    path_node* node = *path;
    while (node != nullptr) {
        path_node* following = node->next != *path ? node->next : nullptr;
        if (!node->flags.optimized || node->flags.off_navmesh) {
            bb_pf_path_unlink(path, node);
            bb_pf_path_append(&rest, node);
        }
        node = following;
    }
    vehicle_path_flatten(path, rule);
    if (rest != nullptr) {
        bb_pf_path_splice(path, rest);
    }
}

// Whether a solid object taller than half a metre stands in a capsule. Magic numbers: 16 objects, half a metre.
static bool bb_pf_capsule_blocked(const vector& pos, float height, float min_mass)
{
    float radius = 0.5f;
    unsigned int handles[16];
    int count = havok_collect_objects_in_capsule(pos, radius, height, handles, 16, COLLIDE_RAY_RFG_DESTROYABLE, nullptr, nullptr);
    for (int i = 0; i < count; i++) {
        object* objp = World.object_from_handle(handles[i]);
        if (objp == nullptr || objp->get_obj_type() != OT_MOVER) {
            continue;
        }
        if (!Havok_filter->isCollisionEnabled(COLLIDE_NPC, havok_body_get_collision_type(objp->havok_handle))) {
            continue;
        }
        if (min_mass <= 0.0f || (float)min_mass <= objp->get_mass()) {
            vector bmin, bmax;
            objp->get_bbox(bmin, bmax);
            if (0.5 <= (double)bmax.y - (double)bmin.y) {
                return true;
            }
        }
    }
    return false;
}

// The direction a wall faces: along the box's thin side, in the world.
static vector bb_pf_wall_normal(float size_x, float size_z, const matrix& orient)
{
    vector normal = ZERO_VECTOR;
    if (size_x <= size_z) {
        normal.x = size_x;
    } else {
        normal.z = size_z;
    }
    normal.unrotate(orient);
    normal.normalize_safe();
    return normal;
}

// Whether a ray at a human's chest height passes between two points without hitting anything destroyable.
static bool bb_pf_chest_ray_blocked(const vector& start, const vector& end)
{
    // Magic number: 0.95 m, half a human's height.
    raycast_input in;
    in.radius = 0.0f;
    in.start = vector(start.x, (float)((double)start.y + 0.95), start.z);
    in.end = vector(end.x, (float)((double)end.y + 0.95), end.z);
    in.m_filter_info = COLLIDE_RAY_DEFAULT;
    in.filter_params = nullptr;
    in.filter_callback = nullptr;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 0;
    raycast_output out;
    return havok_raycast_against_destroyable_geometry(in, out);
}

// A floor hole cuts the ped network nodes around it. The rest of dynamic_hole_request_process_destruction_event, split out.
static void bb_pf_cut_network_around_hole(navpoint* node)
{
    // Magic numbers: 16 links and 16 nodes (32 once links empty) at most; links within 25 m (625 squared); nodes under the hole.
    float radius = node->radius;
    float x = node->pos.x;
    float z = node->pos.z;
    float y = (float)((double)node->pos.y - 0.5);
    bbox box;
    box.min = vector(x - radius, y, z - radius);
    box.max = vector(x + radius, y, z + radius);
    farray<navpoint*,512> network;
    ped_node_get_network_nodes(box, network);
    double reach = (double)radius + 1.0;
    float reach_sq = (float)(reach * reach);
    unsigned int destroyed[32];
    unsigned int cut_from[16];
    unsigned int cut_to[16];
    int num_destroyed = 0;
    int num_cut = 0;
    for (int i = 0; i < network.size() && num_destroyed <= 15; i++) {
        navpoint* navp = network[i];
        if (navp->get_parent() == 0xFFFFFFFF) {
            continue;
        }
        float dist_sq = vec_dist_squared(navp->pos, node->pos);
        if (dist_sq < reach_sq) {
            destroyed[num_destroyed++] = navp->get_handle();
        } else if (dist_sq < 625.0f && num_cut < 16) {
            for (int l = 0; l < navp->num_links; l++) {
                navpoint* link = navp->links[l];
                if (link != nullptr) {
                    vector closest;
                    ix_closest_point_on_line_segment(closest, node->pos, navp->pos, link->pos);
                    if (vec_dist_squared(closest, node->pos) < reach_sq) {
                        cut_from[num_cut] = navp->get_handle();
                        cut_to[num_cut] = link->get_handle();
                        num_cut++;
                    }
                }
            }
        }
    }
    for (int i = 0; i < num_cut; i++) {
        navpoint* from = World.object_from_handle<navpoint>(cut_from[i]);
        navpoint* to = World.object_from_handle<navpoint>(cut_to[i]);
        if (from != nullptr && to != nullptr && 0 < from->num_links && 0 < to->num_links) {
            remove_navpoint_link(from, to);
            if (from->num_links == 0 && num_destroyed < 32) {
                destroyed[num_destroyed++] = cut_from[i];
            }
            if (to->num_links == 0 && num_destroyed < 32) {
                destroyed[num_destroyed++] = cut_to[i];
            }
        }
    }
    for (int i = 0; i < num_destroyed; i++) {
        World.destroy_object(destroyed[i], false);
    }
}

// 0x00B0FAB0 (:10328). Ghidra cannot decompile it with its local types; read with the locals retyped to undefined.
static void dynamic_hole_request_process_destruction_event(dynamic_hole_request* request)
{
    // Magic numbers (metres): a box over 2 m tall or taller than it is wide is a wall; a hole needs 1.5 m; 2 m (4 squared)
    // separates holes; a floor must be 2.5 m up to be left by a floor hole; walls and floors that small have a radius of 0.5.
    const float TALL_BOX = 2.0f;
    const float MIN_HOLE = 1.5f;
    const float HOLE_SPACING_SQ = 4.0f;
    const signed char TYPE_WALL_HOLE = 0x12;
    const signed char TYPE_FLOOR_HOLE = 0x13;
    const signed char TYPE_DROP = 0x14;
    const signed char TYPE_JUNCTION = 0x15;
    request->flags.processed = 1;
    rfg_destruction_event* devp = request->devp;
    float size_x = devp->bmax.x - devp->bmin.x;
    float size_y = devp->bmax.y - devp->bmin.y;
    float size_z = devp->bmax.z - devp->bmin.z;
    bool is_wall = size_z < size_y || size_x < size_y || TALL_BOX < size_y;
    if (!is_wall && (size_x < MIN_HOLE || size_z < MIN_HOLE)) {
        return;
    }
    vector center((float)((double)(devp->bmin.x + devp->bmax.x) * 0.5), (float)((double)(devp->bmin.y + devp->bmax.y) * 0.5), (float)((double)(devp->bmin.z + devp->bmax.z) * 0.5));
    center.unrotate(devp->orient);
    center = vector(center.x + devp->pos.x, devp->pos.y + center.y, devp->pos.z + center.z);
    if (is_wall) {
        center.y = devp->pos.y + devp->bmin.y;
    }
    // A hole already here is updated instead of made again.
    bool existing = false;
    bool nearby = false;
    unsigned int handles[32];
    int count = World.collect_objects<navpoint>(devp->bmin, devp->bmax, devp->pos, devp->orient, handles, 32, nullptr, nullptr);
    for (int i = 0; i < count; i++) {
        navpoint* navp = handles[i] == 0xFFFFFFFF ? nullptr : World.object_from_handle<navpoint>(handles[i]);
        if (navp == nullptr || (navp->type != TYPE_WALL_HOLE && navp->type != TYPE_FLOOR_HOLE)) {
            continue;
        }
        if (!ix_point_in_box_oriented(navp->pos, devp->pos, devp->orient, devp->bmin, devp->bmax)) {
            if (vec_dist_squared(navp->pos, center) < HOLE_SPACING_SQ) {
                nearby = true;
            }
            continue;
        }
        if (existing) {
            // A second hole in the same box goes, with what it brought along.
            navpoint* links[16]; // rfg.exe copies them over a matrix and what follows it on the stack.
            int num_links = navp->num_links;
            for (int l = 0; l < num_links; l++) {
                links[l] = navp->links[l];
            }
            World.destroy_object(navp, false);
            for (int l = 0; l < num_links; l++) {
                if (links[l] != nullptr && (links[l]->type == TYPE_JUNCTION || links[l]->type == NAVPOINT_TYPE_TRANSITION || links[l]->type == TYPE_DROP)) {
                    World.destroy_object(links[l], false);
                }
            }
            continue;
        }
        if (HOLE_SPACING_SQ < vec_xz_dist_squared(navp->pos, center)) {
            continue;
        }
        existing = true;
        if (navp->type == TYPE_FLOOR_HOLE) {
            bool has_drop = false;
            for (int l = 0; l < navp->num_links; l++) {
                if (navp->links[l]->type == TYPE_DROP) {
                    has_drop = true;
                    break;
                }
            }
            if (!has_drop) {
                handle_hole_in_floor(navp, nullptr, ZERO_VECTOR);
            }
        }
        if (navp->type == TYPE_WALL_HOLE) {
            // Magic number: a wall hole drops to the floor of its first walkable link, up to 2.5 m under it.
            for (int l = 0; l < navp->num_links; l++) {
                signed char link_type = navp->links[l]->type;
                if (link_type != NAVPOINT_TYPE_TRANSITION && link_type != NAVPOINT_TYPE_TRANSITION_JUMP && link_type != NAVPOINT_TYPE_LEAP_OVER && link_type != TYPE_DROP) {
                    float height = navp->pos.y - navp->links[l]->pos.y;
                    if (height < 0.0f && -2.5f < height) {
                        navp->pos.y = navp->links[l]->pos.y;
                    }
                    break;
                }
            }
        }
    }
    if (existing || nearby) {
        return;
    }
    vector ground = center;
    if (!havok_terrain_height(ground.y, center.x, center.z, false, nullptr)) {
        return;
    }
    navpoint* other = nullptr;
    navpoint* closest = rfg_pf_find_closest_navpoint_to_pos(center, other, 10, NAVPOINT_TYPE_STAIRS);
    bool outdoors = closest == nullptr;
    if (closest != nullptr) {
        unsigned int parent = closest->get_parent();
        object* building = parent != 0xFFFFFFFF ? World.object_from_handle(parent) : nullptr;
        if (building != nullptr && building->get_obj_type() == OT_MOVER && building->get_sub_type() == OT_SUB_MOVER_RFG) {
            vector bmin, bmax;
            building->get_local_bbox(bmin, bmax);
            outdoors = !ix_point_in_box_oriented(center, building->pos, building->orient, bmin, bmax);
        }
    }
    if (!outdoors) {
        if (other != nullptr && closest->type == NAVPOINT_TYPE_STAIRS && other->type == NAVPOINT_TYPE_STAIRS) {
            return;
        }
        if (closest->type == NAVPOINT_TYPE_TRANSITION && closest->num_links == 1 && closest->links[0]->type == NAVPOINT_TYPE_TRANSITION) {
            if (vec_xz_dist_squared(closest->pos, center) < HOLE_SPACING_SQ) {
                return;
            }
            outdoors = true;
        }
    }
    if (outdoors) {
        if (!is_wall || 0.5 < (double)devp->pos.y + (double)devp->bmin.y - (double)ground.y) {
            return;
        }
        // A wall with no navigation nodes around it: a two node door across it on the mesh.
        center.y = (float)((double)ground.y + 0.25);
        uint64_t cell_handle;
        nav_cell* cell = bb_pf_get_nav_cell(ground, false, &cell_handle, false, NAV_CELL_TRAVERSAL_ALL, nullptr);
        if (cell == nullptr) {
            return;
        }
        vector capsule_pos(center.x, (float)((double)ground.y + 0.25), center.z);
        // Magic number: a 0.9 m tall capsule.
        if (bb_pf_capsule_blocked(capsule_pos, 0.9f, 0.0f)) {
            return;
        }
        vector normal = bb_pf_wall_normal(size_x, size_z, devp->orient);
        vector out_one((float)((double)normal.x * 2.0) + center.x, (float)((double)normal.y * 2.0) + center.y, (float)((double)normal.z * 2.0) + center.z);
        vector out_two((float)((double)-normal.x * 2.0) + center.x, (float)((double)-normal.y * 2.0) + center.y, (float)((double)-normal.z * 2.0) + center.z);
        vector valid = ZERO_VECTOR;
        const nav_cell* cell_one = find_first_valid_nav_cell_along_path(center, out_one, cell, valid);
        if (cell_one == nullptr) {
            return;
        }
        valid = vector((float)((double)normal.x * 0.15) + valid.x, (float)((double)normal.y * 0.15) + valid.y, (float)((double)normal.z * 0.15) + valid.z);
        vector point_one = nav_cell_force_point_to_cell(cell_one, valid);
        valid = ZERO_VECTOR;
        const nav_cell* cell_two = find_first_valid_nav_cell_along_path(center, out_two, cell, valid);
        if (cell_two == nullptr) {
            return;
        }
        valid = vector((float)((double)-normal.x * 0.15) + valid.x, (float)((double)-normal.y * 0.15) + valid.y, (float)((double)-normal.z * 0.15) + valid.z);
        vector point_two = nav_cell_force_point_to_cell(cell_two, valid);
        if (bb_pf_chest_ray_blocked(point_one, point_two)) {
            return;
        }
        navpoint* one = bb_pf_create_navpoint(point_one, devp->orient, NAVPOINT_TYPE_TRANSITION, 0xFFFFFFFF);
        if (one != nullptr) {
            navpoint* two = bb_pf_create_navpoint(point_two, devp->orient, NAVPOINT_TYPE_TRANSITION, 0xFFFFFFFF);
            if (two != nullptr) {
                link_navpoints(one, two, true);
                one->set = get_next_navpoint_set_id();
                one->propogate_set_info();
                request->flags.pass_1 = 1;
                request->flags.pass_2 = 1;
            }
        }
        return;
    }
    if (request->flags.removed != 0) {
        return;
    }
    // Magic numbers: a human sized box with no hole navpoint in it.
    vector probe_min(-0.5f, -0.95f, -0.5f);
    vector probe_max(0.5f, 0.95f, 0.5f);
    unsigned int holes[16];
    if (0 < World.collect_objects<navpoint>(probe_min, probe_max, center, devp->orient, holes, 16, walls_floors_collect_cb, nullptr)) {
        return;
    }
    vector wall_normal = ZERO_VECTOR;
    if (is_wall) {
        float height = center.y - closest->pos.y;
        if (height < 0.0f && -2.5f < height) {
            center.y = closest->pos.y;
        }
        // Magic number: 20 cm above the navpoint's floor or no way through.
        if ((double)devp->pos.y + (double)devp->bmax.y - (double)closest->pos.y < 0.2) {
            return;
        }
        if (height < 0.0f && size_y < fabsf(height)) {
            return;
        }
        if (0.5f < height) {
            return;
        }
        wall_normal = bb_pf_wall_normal(size_x, size_z, devp->orient);
    }
    // Magic numbers: a capsule from 15 cm over the hole, 0.75 m tall; for a floor 3.25 m tall reaching 2.5 m under it; only
    // heavy objects (500 kg) block.
    vector capsule_pos(center.x, (float)((double)center.y + 0.15), center.z);
    float capsule_height = 0.75f;
    if (!is_wall) {
        capsule_pos.y = (float)((double)capsule_pos.y - 2.5);
        capsule_height = 3.25f;
    }
    if (bb_pf_capsule_blocked(capsule_pos, capsule_height, 500.0f)) {
        return;
    }
    navpoint* second = nullptr;
    vector closest_point = ZERO_VECTOR;
    float closest_dist = 10000.0f;
    if (!find_closest_point_on_link_from_navpoint_or_in_navpoint(center, closest, second, closest_point, closest_dist) || 5.0f < closest_dist) {
        return;
    }
    float new_radius = 0.5f;
    if (!is_wall) {
        float span = devp->bmax.z - devp->bmin.z;
        if (devp->bmax.x - devp->bmin.x < span) {
            span = devp->bmax.x - devp->bmin.x;
        }
        new_radius = (float)((double)span * 0.5);
        vector local = ground;
        vec_change_to_local_coords(local, devp->pos, devp->orient);
        if ((double)devp->bmin.y - (double)local.y < 2.5) {
            return;
        }
    } else {
        vector to_link = closest_point - center;
        to_link.flatten_normalize_safe();
        float facing = to_link.x * wall_normal.x + to_link.y * wall_normal.y + wall_normal.z * to_link.z;
        // Magic number: the cosine of 75 degrees: a wall has to face the graph.
        if (facing < 0.25881904f && -0.25881904f < facing) {
            return;
        }
    }
    if (bb_pf_chest_ray_blocked(closest_point, center)) {
        return;
    }
    navpoint* node = create_navpoints_for_hole(center, closest_point, closest, second, closest_dist, is_wall, new_radius);
    if (node == nullptr) {
        return;
    }
    request->flags.pass_1 = 1;
    request->flags.pass_2 = 1;
    vector navmesh_pos = ZERO_VECTOR;
    nav_cell* mesh_cell = nullptr;
    float drop = node->pos.y - ground.y;
    bool too_high = false;
    bool over_ground = false;
    bool on_navmesh = false;
    if (2.5f < drop) {
        if (5.5f <= drop) {
            too_high = true;
        } else {
            over_ground = true;
        }
    }
    bool found = false;
    if (!is_wall) {
        found = bb_pf_find_closest_point_in_or_on_nav_mesh(node->pos, navmesh_pos, mesh_cell, on_navmesh, false);
    } else {
        if (0 < node->num_links) {
            vector to_link = node->links[0]->pos - node->pos;
            to_link.normalize_safe();
            if (0.0f < to_link.x * wall_normal.x + to_link.y * wall_normal.y + wall_normal.z * to_link.z) {
                wall_normal = vector((float)((double)wall_normal.x * -1.0), (float)((double)wall_normal.y * -1.0), (float)((double)wall_normal.z * -1.0));
            }
        }
        navmesh_pos = vector((float)((double)wall_normal.x * 2.0) + node->pos.x, node->pos.y + (float)((double)wall_normal.y * 2.0), node->pos.z + (float)((double)wall_normal.z * 2.0));
        mesh_cell = bb_pf_get_nav_cell(navmesh_pos, false, nullptr, true, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
        found = mesh_cell != nullptr;
    }
    bool to_mesh = !too_high && found;
    if (to_mesh) {
        float dist_sq = vec_xz_dist_squared(navmesh_pos, node->pos);
        // Magic number: 2.5 m (6.25 squared) to the mesh.
        if (6.25 < (double)dist_sq) {
            to_mesh = false;
        } else if (over_ground) {
            if (is_wall) {
                if (HOLE_SPACING_SQ < dist_sq) {
                    to_mesh = false;
                } else {
                    // Above the ground: nothing of a building may stand between the hole and the landing.
                    vector landing = navmesh_pos;
                    landing.y = (float)((double)navmesh_pos.y + 1.0);
                    float height = (float)((double)node->pos.y + 1.0 - (double)landing.y);
                    unsigned int objects[16];
                    int num_objects;
                    float radius = 0.5f;
                    // Magic number: 1e-5 m, a capsule with no height is a sphere.
                    if (1e-5f <= height) {
                        num_objects = havok_collect_objects_in_capsule(landing, radius, height, objects, 16, COLLIDE_RAY_WORLD_ONLY, nullptr, nullptr);
                    } else {
                        num_objects = havok_collect_objects_fast(landing, radius, objects, 16, COLLIDE_RAY_WORLD_ONLY, nullptr, nullptr);
                    }
                    for (int i = 0; i < num_objects; i++) {
                        object* objp = World.object_from_handle(objects[i]);
                        if (objp != nullptr && objp->get_sub_type() == OT_SUB_MOVER_RFG) {
                            to_mesh = false;
                            break;
                        }
                    }
                    if (to_mesh) {
                        navmesh_pos = landing;
                    }
                }
            } else {
                to_mesh = false;
            }
        }
    }
    if (to_mesh) {
        if (is_wall) {
            vector out(navmesh_pos.x - node->pos.x, 0.0f, navmesh_pos.z - node->pos.z);
            out.normalize_safe();
            navmesh_pos = vector(out.x + navmesh_pos.x, out.y + navmesh_pos.y, out.z + navmesh_pos.z);
            vector land = nav_cell_force_point_to_cell(mesh_cell, navmesh_pos);
            matrix orient;
            orient.make_quick(vector(1.0f, 0.0f, 0.0f));
            signed char type = NAVPOINT_TYPE_TRANSITION;
            if (over_ground) {
                type = NAVPOINT_TYPE_TRANSITION_JUMP;
            } else if (bb_pf_chest_ray_blocked(node->pos, land)) {
                return;
            }
            navpoint* outside = bb_pf_create_navpoint(land, orient, type, node->get_parent());
            if (outside != nullptr) {
                link_navpoints(node, outside, true);
                outside->set = node->set;
            }
        }
        // A floor hole close to the mesh needs nothing more.
        return;
    }
    if (is_wall) {
        matrix orient;
        orient.make_quick(bb_pf_wall_normal(size_x, size_z, devp->orient));
        handle_hole_in_interior_wall(node, orient);
        return;
    }
    if (!on_navmesh) {
        navmesh_pos = ZERO_VECTOR;
        mesh_cell = nullptr;
    }
    handle_hole_in_floor(node, mesh_cell, navmesh_pos);
    bb_pf_cut_network_around_hole(node);
}

// 0x00B16FF0 (:11669) LTCG: std_path EAX
static void bb_pf_simplify_vehicle_raw_path(path_node** std_path, const pf_calc_flags& flags)
{
    simplify_vehicle_nav_path(std_path, flags);
}

// 0x00B1BF30 (:11674) LTCG: r_path ESI, seg_params EDI
static void bb_pf_get_vehicle_path_core(raw_path& r_path, pf_calc_params& seg_params)
{
    get_raw_path(r_path, seg_params);
    if (seg_params.flags.vehicle_simplify_path_after_segment != 0 && r_path.path != nullptr) {
        simplify_vehicle_nav_path(&r_path.path, seg_params.flags);
    }
}

// Adds a calculated segment to a vehicle path. The first node of a later segment repeats the end of the last one.
static void bb_pf_vehicle_add_segment(path_node** total, raw_path& raw)
{
    if (*total == nullptr) {
        *total = raw.path;
        return;
    }
    bb_pf_free_path_node(&raw.path, raw.path);
    if (raw.path != nullptr) {
        bb_pf_path_splice(total, raw.path);
    }
}

// 0x00B1BF60 (:11693)
path_node* rfg_pf_vehicle_get_path(const pf_calc_params& params, path_outcome& error_code)
{
    // Magic number: rfg.exe's array holds 10 points (0x20 bytes each, copied whole).
    vector zero = ZERO_VECTOR;
    pf_calc_point points[10] = {
        pf_calc_point(zero, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF), pf_calc_point(zero, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF),
        pf_calc_point(zero, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF), pf_calc_point(zero, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF),
        pf_calc_point(zero, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF), pf_calc_point(zero, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF),
        pf_calc_point(zero, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF), pf_calc_point(zero, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF),
        pf_calc_point(zero, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF), pf_calc_point(zero, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF),
    };
    int num_points = params.num_points;
    if (9 < num_points) {
        num_points = 10;
    }
    if (0 < num_points) {
        memcpy((void*)points, params.points, num_points * sizeof(pf_calc_point));
    }
    path_node* total = nullptr;
    bool out_of_chunk = false;
    int num_segments = num_points - 1;
    for (int i = 0; i < num_segments; i++) {
        raw_path raw;
        raw.path = nullptr;
        raw.end_nav_cell = nullptr;
        raw.out_of_chunk = false;
        raw.result = PR_UNKNOWN;
        if (!out_of_chunk) {
            pf_calc_flags flags = params.flags;
            flags.pre_detour_simplification = 0;
            // A segment into or out of an unloaded zone may leave the mesh.
            if (points[i].zone_handle != 0xFFFFFFFF && points[i + 1].zone_handle != 0xFFFFFFFF) {
                object* zone0 = World.object_from_handle(points[i].zone_handle);
                object* zone1 = zone0 != nullptr && zone0->get_obj_type() == OT_ZONE ? World.object_from_handle(points[i + 1].zone_handle) : nullptr;
                if (zone1 == nullptr || zone1->get_obj_type() != OT_ZONE) {
                    flags.allow_out_of_chunk = 1;
                    flags.allow_out_of_chunk_no_raycast = 1;
                }
            }
            pf_calc_point seg_points[2] = { points[i], points[i + 1] };
            pf_calc_params seg_params(2, seg_points, flags, params.use_road, params.use_special_roads, params.flying, params.on_bridge, params.bridge_navpoint_handle);
            // Original quirk kept: a request without vehicle_pfg calculates nothing; rfg.exe then reads an unset result.
            if (params.flags.vehicle_pfg != 0) {
                bb_pf_get_vehicle_path_core(raw, seg_params);
            }
            if (raw.path == nullptr) {
                if (raw.result == PR_ERROR_DEGENERATE) {
                    continue;
                }
                if (raw.result != PR_ERROR_UNKNOWN || params.flags.partial_path_ok == 0 || total == nullptr) {
                    error_code = raw.result;
                    bb_pf_free_path(&total);
                    return nullptr;
                }
                break;
            }
        } else {
            build_out_of_chunk_path(raw, points[i].pos, points[i + 1].pos);
            // Original bug kept: after an out of chunk segment this one goes in here and again below.
            bb_pf_vehicle_add_segment(&total, raw);
        }
        raw.path->prev->flags.do_not_remove = true;
        if (!raw.out_of_chunk) {
            bb_pf_vehicle_add_segment(&total, raw);
            points[i + 1].pos = raw.adjusted_end_pos;
            points[i + 1].nav_cell_hint = raw.end_nav_cell != nullptr ? nav_cell_get_handle(raw.end_nav_cell) : 0xFFFFFFFFFFFFFFFFull;
        } else {
            bb_pf_vehicle_add_segment(&total, raw);
            out_of_chunk = true;
        }
    }
    if (total != nullptr) {
        if (params.flags.vehicle_pfg != 0 && params.flags.vehicle_simplify_path_after_segment == 0) {
            simplify_vehicle_nav_path(&total, params.flags);
        }
        error_code = PR_OK;
        return total;
    }
    error_code = PR_ERROR_UNKNOWN;
    return nullptr;
}

// 0x00AEC0C0 (:11962) LTCG: cbs ESI, total_length XMM1
static float find_u_value_for_dist_along_spline_from_start_value(cubic_bezier_spline* cbs, float total_length, float starting_u_value, float ending_u_value, float advance_dist)
{
    if ((double)total_length < (double)advance_dist) {
        return ending_u_value;
    }
    // Magic number: steps of 2 m along the tangents.
    const float STEP = 2.0f;
    float u = starting_u_value;
    float remaining = advance_dist;
    bool last;
    do {
        last = remaining <= STEP;
        float step = last ? remaining : STEP;
        vector tangent;
        cbs->get_tangent(tangent, u);
        float speed = tangent.mag_and_normalize_safe();
        if (ending_u_value <= starting_u_value) {
            u -= step / speed;
            if (u < ending_u_value) {
                u = ending_u_value;
            }
        } else {
            u += step / speed;
            if (ending_u_value <= u) {
                u = ending_u_value;
            }
        }
        remaining -= step;
    } while (!last);
    return u;
}

// Copies a split spline's two knots back to a lane node.
static void bb_pf_lane_store_spline(lane_spline_base& lane, const cubic_bezier_spline& spline, int count)
{
    vector* knots = lane.knots.to_pointer();
    vector* in_handles = lane.in_handles.to_pointer();
    vector* out_handles = lane.out_handles.to_pointer();
    for (int i = 0; i < count; i++) {
        knots[i] = spline.knots[i];
        in_handles[i] = spline.incoming_handles[i];
        out_handles[i] = spline.outgoing_handles[i];
    }
}

// A cubic bezier over a lane's two knots.
static float bb_pf_lane_store_length(lane_spline_base& lane)
{
    cubic_bezier_spline spline;
    spline.set_points(2, lane.knots.to_pointer(), lane.in_handles.to_pointer(), lane.out_handles.to_pointer());
    float half = spline.get_length_approx(5, 0.0f, 0.5f);
    float rest = spline.get_length_approx(5, 0.5f, 1.0f);
    lane.arc_length[0] = half + rest;
    lane.arc_length[1] = half;
    return half + rest;
}

// 0x00AFD060 (:12010) LTCG: last_spline EAX, temp_node ECX
static navmesh_lane_spline_node* generate_intersection_curve(navmesh_lane_spline_node* last_node, navmesh_lane_spline_node* temp_node, cubic_bezier_spline* last_spline, cubic_bezier_spline* cbs, float dot_product)
{
    // Magic numbers: the curve takes 10 m off each road, 22.5 m where the roads turn back by over 120 degrees; a curve
    // shorter than 1 cm has no tangent; lane flag 0x80 marks a lane that is not on a road.
    const float MIN_LENGTH = 0.01f;
    navmesh_lane_spline_node* node = get_free_ls_node();
    if (node == nullptr) {
        return nullptr;
    }
    float cut = dot_product < -0.5f ? 22.5f : 10.0f;
    vector knots0[2], in0[2], out0[2], knots1[2], in1[2], out1[2];
    cubic_bezier_spline first, second;
    first.set_points(2, knots0, in0, out0);
    second.set_points(2, knots1, in1, out1);
    float u = find_u_value_for_dist_along_spline_from_start_value(last_spline, last_node->lane.arc_length[0], 1.0f, 0.0f, cut);
    last_spline->split_at_parametric_pos(u, &first, &second);
    float out_length = last_spline->get_length_approx(5, u, 1.0f);
    bb_pf_lane_store_spline(last_node->lane, first, first.num_knots);
    float half = first.get_length_approx(5, 0.0f, 0.5f);
    float rest = first.get_length_approx(5, 0.5f, 1.0f);
    last_node->lane.arc_length[1] = half;
    last_node->lane.arc_length[0] = rest + half;
    vector tangent_out;
    if (MIN_LENGTH <= rest + half) {
        first.get_tangent(tangent_out, 1.0f);
    } else {
        second.get_tangent(tangent_out, 0.0f);
    }
    tangent_out.flatten_normalize_safe();
    cubic_bezier_spline before, after;
    before.set_points(2, knots0, in0, out0);
    after.set_points(2, knots1, in1, out1);
    u = find_u_value_for_dist_along_spline_from_start_value(cbs, temp_node->lane.arc_length[0], 0.0f, 1.0f, cut);
    cbs->split_at_parametric_pos(u, &before, &after);
    float in_length = cbs->get_length_approx(5, 0.0f, u);
    bb_pf_lane_store_spline(temp_node->lane, after, before.num_knots);
    half = after.get_length_approx(5, 0.0f, 0.5f);
    rest = after.get_length_approx(5, 0.5f, 1.0f);
    temp_node->lane.arc_length[1] = half;
    temp_node->lane.arc_length[0] = rest + half;
    vector* knots = node->lane.knots.to_pointer();
    knots[0] = last_node->lane.knots.to_pointer()[1];
    knots[1] = temp_node->lane.knots.to_pointer()[0];
    node->lane.num_knots = 2;
    vector tangent_in;
    if (MIN_LENGTH <= rest + half) {
        after.get_tangent(tangent_in, 0.0f);
    } else {
        before.get_tangent(tangent_in, 1.0f);
    }
    tangent_in.flatten_normalize_safe();
    in_length *= 0.5f;
    node->lane.in_handles.to_pointer()[1] = vector(knots[1].x - tangent_in.x * in_length, knots[1].y - tangent_in.y * in_length, knots[1].z - tangent_in.z * in_length);
    out_length *= 0.5f;
    node->lane.out_handles.to_pointer()[0] = vector(knots[0].x + tangent_out.x * out_length, knots[0].y + tangent_out.y * out_length, knots[0].z + tangent_out.z * out_length);
    bb_pf_lane_store_length(node->lane);
    node->lane.spline_flags = 0x80;
    return node;
}

// 0x00B04A70 (:12122)
bool rfg_pf_create_route_segment(const vector& start_pos, const vector& end_pos, farray<vector,512>& route, bool include_start)
{
    // Magic numbers: roads within 250 m; the flags are 0xD901 in rfg.exe, a partial GPS route over all cells.
    vehicle_path_info info;
    info.start_pos.zero();
    info.start_nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
    info.end_pos.zero();
    memset(&info.flags, 0, sizeof(info.flags));
    info.flags.gps_path = 1;
    route.clear();
    info.closest_road_index = 0.0f;
    info.error_code = PR_UNKNOWN;
    info.end_nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
    info.closest_road_spline = 0xFFFFFFFF;
    pf_calc_flags flags(true);
    flags.allow_dest_in_detour_hull = 1;
    flags.ignore_detour_hulls = 1;
    flags.partial_path_ok = 1;
    flags.use_disabled_cells = 1;
    flags.use_light_structure_disabled_cells = 1;
    vector start_on_road = ZERO_VECTOR;
    vector end_on_road;
    float u;
    object_path_road* start_road = object_road_get_closest_road(start_pos, true, start_on_road, u, 250.0f, false, nullptr, nullptr, true);
    object_path_road* end_road = object_road_get_closest_road(end_pos, true, end_on_road, u, 250.0f, false, nullptr, nullptr, true);
    bool on_roads = start_road != nullptr && end_road != nullptr;
    pf_calc_point points[2] = {
        pf_calc_point(on_roads ? start_on_road : start_pos, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF),
        pf_calc_point(on_roads ? end_on_road : end_pos, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF),
    };
    pf_calc_params params(2, points, flags, true, true, false, false, 0xFFFFFFFF);
    params.cap_max_points = false;
    params.ignore_road_directionality = true;
    bool try_normal;
    navmesh_lane_spline_node* path = rfg_pf_vehicle_use_road_splines(params, info, try_normal, nullptr);
    if (path != nullptr) {
        if (path != path->prev) {
            if (include_start) {
                route.add(start_on_road);
            }
            navmesh_lane_spline_node* node = path;
            do {
                if (1 < node->lane.num_knots) {
                    route.add(node->lane.knots.to_pointer()[1]);
                    if (route.capacity() <= route.size()) {
                        vehicle_free_ls_path(&path);
                        return true;
                    }
                }
                node = node->next;
            } while (node != nullptr && node != path);
        }
        vehicle_free_ls_path(&path);
    }
    return 0 < route.size();
}

// A lane node's spline.
static void bb_pf_ls_set_points(cubic_bezier_spline& spline, navmesh_lane_spline_node* node)
{
    spline.set_points(2, node->lane.knots.to_pointer(), node->lane.in_handles.to_pointer(), node->lane.out_handles.to_pointer());
}

// A lane node over two road points, walked in either direction. Magic number: lane flag 0x80, a lane made at run time.
static void bb_pf_ls_from_road(navmesh_lane_spline_node* node, const road_spline_point* from, const road_spline_point* to, bool reverse)
{
    vector* knots = node->lane.knots.to_pointer();
    knots[0] = from->pos;
    knots[1] = to->pos;
    node->lane.num_knots = 2;
    if (reverse) {
        node->lane.in_handles.to_pointer()[1] = vector(knots[1].x + to->pt_handle.x, knots[1].y + to->pt_handle.y, knots[1].z + to->pt_handle.z);
        node->lane.out_handles.to_pointer()[0] = vector(knots[0].x - from->pt_handle.x, knots[0].y - from->pt_handle.y, knots[0].z - from->pt_handle.z);
    } else {
        node->lane.in_handles.to_pointer()[1] = vector(knots[1].x - to->pt_handle.x, knots[1].y - to->pt_handle.y, knots[1].z - to->pt_handle.z);
        node->lane.out_handles.to_pointer()[0] = vector(knots[0].x + from->pt_handle.x, knots[0].y + from->pt_handle.y, knots[0].z + from->pt_handle.z);
    }
    bb_pf_lane_store_length(node->lane);
    node->lane.spline_flags = 0x80;
}

static void bb_pf_ls_append(navmesh_lane_spline_node** path, navmesh_lane_spline_node* node)
{
    if (*path == nullptr) {
        node->next = node;
        node->prev = node;
        *path = node;
    } else {
        node->prev = (*path)->prev;
        node->next = *path;
        (*path)->prev->next = node;
        (*path)->prev = node;
    }
}

// The flattened tangent where two splines meet, as a cosine.
static float bb_pf_spline_join_dot(const cubic_bezier_spline& first, const cubic_bezier_spline& second)
{
    vector out_tangent, in_tangent;
    second.get_tangent(in_tangent, 0.0f);
    in_tangent.flatten_normalize_safe();
    first.get_tangent(out_tangent, 1.0f);
    out_tangent.flatten_normalize_safe();
    return in_tangent.x * out_tangent.x + out_tangent.y * in_tangent.y + out_tangent.z * in_tangent.z;
}

// Which side a road turn goes: the flattened cross product of the two legs.
static void bb_pf_road_turn_side(vehicle_path_info& path_info, const vector& corner, const vector& before, const vector& after)
{
    vector in_leg(corner.x - before.x, corner.y - before.y, corner.z - before.z);
    if (in_leg.is_null()) {
        path_info.flags.ixc_turning = 0;
        return;
    }
    vector out_leg(after.x - corner.x, after.y - corner.y, after.z - corner.z);
    if (out_leg.is_null()) {
        path_info.flags.ixc_turning = 0;
        return;
    }
    if (0.0f < out_leg.x * in_leg.z + 0.0f + -in_leg.x * out_leg.z) {
        path_info.flags.ixc_turning_right = 1;
    }
}

// Whether a point past either end of a road lies beyond the road's end.
static bool bb_pf_off_road_end(object_path_road* road, float u, const vector& to_point, bool& off_road)
{
    // Magic number: within 0.05 of the parameter's ends.
    const float NEAR_END = 0.05f;
    bool near_start = !(NEAR_END <= u || NEAR_END <= -u);
    float from_end = u - (float)(road->spline_data.num_points - 1);
    bool near_end = from_end < NEAR_END && -from_end < NEAR_END;
    if (!near_start && !near_end) {
        return false;
    }
    vector tangent;
    road->get_tangent(tangent, u);
    float dot = tangent.y * to_point.y + tangent.x * to_point.x + tangent.z * to_point.z;
    bool inside = near_start ? 0.0f < dot : dot < 0.0f;
    off_road = !inside && dot != 0.0f;
    return true;
}

// 0x00AFD7A0 (:12189)
navmesh_lane_spline_node* rfg_pf_vehicle_use_road_splines(pf_calc_params real_params, vehicle_path_info& path_info, bool& try_normal_pathfinding, vehicle* vp)
{
    // Magic numbers: roads are searched within 50 m (250 m for a GPS route); a turn over 45 degrees (cosine 0.707) gets an
    // intersection curve; a lane under 1 cm is dropped; a path has at most 20 lanes when capped.
    // rfg.exe's offsets of the first lane from the road stay zero at every use, so that part of the original is left out.
    const float TURN_COSINE = 0.707f;
    const float MIN_LANE_LENGTH = 0.01f;
    const int MAX_LANES = 0x13;
    const pf_calc_point* points = real_params.points;
    vector start_on_road = ZERO_VECTOR;
    vector end_on_road = ZERO_VECTOR;
    float start_u = 0.0f;
    float end_u = 0.0f;
    try_normal_pathfinding = true;
    path_info.flags.use_road_offset = 0;
    path_info.flags.ixc_can_control = 0;
    bool gps = path_info.flags.gps_path != 0;
    const vector* fvec = vp != nullptr ? &vp->orient.fvec : nullptr;
    bool cap_lanes = real_params.cap_max_points;
    object_path_road* start_road = object_road_get_closest_road(points[0].pos, real_params.use_special_roads, start_on_road, start_u, gps ? 250.0f : 50.0f, false, fvec, nullptr, gps);
    object_path_road* end_road = object_road_get_closest_road(points[1].pos, real_params.use_special_roads, end_on_road, end_u, gps ? 250.0f : 50.0f, false, nullptr, start_road, gps);
    if (start_road == nullptr || end_road == nullptr) {
        return nullptr;
    }
    float start_dist_sq = vec_xz_dist_squared(start_on_road, points[0].pos);
    float end_dist_sq = vec_xz_dist_squared(end_on_road, points[1].pos);
    path_info.closest_road_spline = start_road->get_handle();
    path_info.closest_road_index = start_u;
    bool off_start = false;
    bool off_end = false;
    float width = start_road->get_width(start_u);
    // Magic numbers: 2.5 m beside a road, or twice its width in the air, is still on it.
    float limit = real_params.flying ? width * 2.0f : width + 2.5f;
    if (limit * limit < start_dist_sq) {
        off_start = true;
    } else {
        bb_pf_off_road_end(start_road, start_u, points[0].pos - start_on_road, off_start);
    }
    width = end_road->get_width(end_u);
    limit = real_params.flying ? width * 2.0f : width * 1.5f;
    vector end_offset = ZERO_VECTOR;
    if (end_dist_sq <= limit * limit) {
        end_offset = points[1].pos - end_on_road;
        bb_pf_off_road_end(end_road, end_u, end_offset, off_end);
    } else {
        off_end = true;
    }
    bool same_piece = end_road == start_road && ceil((double)end_u) == ceil((double)start_u) && floor((double)end_u) == floor((double)start_u) && fabsf(start_u - end_u) < 0.5f;
    pf_calc_point segment[2] = { points[0], points[1] };
    if (off_start || (off_end && same_piece)) {
        try_normal_pathfinding = false;
        if (!off_end || !same_piece) {
            // The mesh takes the vehicle to the road; the next calculation goes on from there.
            segment[1] = pf_calc_point(start_on_road, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF);
            path_info.flags.road_spline_path = 1;
            path_info.flags.off_road = 1;
        }
        pf_calc_params params(2, segment, real_params.flags, false, false, real_params.flying, false, 0xFFFFFFFF);
        return vehicle_get_navmesh_path_internal(params, path_info, vp);
    }
    navmesh_lane_spline_node* path = nullptr;
    const road_spline_point* start_points = start_road->spline_data.points.to_pointer();
    const road_spline_point* end_points = end_road->spline_data.points.to_pointer();
    if (end_road == start_road && (unsigned int)end_u == (unsigned int)(int)start_u && (real_params.ignore_road_directionality || start_road->road_flags.one_way == 0 || start_u < end_u)) {
        if (start_road->has_breaks() || start_road->has_road_blocks()) {
            // Original quirk kept: only the low byte of the index is looked at.
            int index = (int)((unsigned int)end_u & 0xFF);
            road_broken_state state = start_road->is_broken_at_index(index);
            bool blocked = !gps && start_road->is_road_blocked_at_index(index);
            if (state == ROAD_BROKEN_COMPLETELY || blocked) {
                try_normal_pathfinding = false;
                pf_calc_params params(2, segment, real_params.flags, false, false, real_params.flying, false, 0xFFFFFFFF);
                return vehicle_get_navmesh_path_internal(params, path_info, vp);
            }
        }
        // Original quirk kept: no check for a free node.
        navmesh_lane_spline_node* node = get_free_ls_node();
        if (start_u <= end_u) {
            bb_pf_ls_from_road(node, &start_points[(int)floor((double)start_u)], &start_points[(int)ceil((double)end_u)], false);
            path_info.flags.next_point_inc = 1;
        } else {
            bb_pf_ls_from_road(node, &start_points[(int)ceil((double)start_u)], &start_points[(int)floor((double)end_u)], true);
            path_info.flags.next_point_inc = 0;
        }
        bb_pf_ls_append(&path, node);
        path_info.flags.ixc_can_control = 1;
        path_info.flags.jumping = 0;
        path_info.flags.use_road_offset = 1;
        path_info.flags.off_road = 0;
        path_info.flags.road_spline_path = 1;
        path_info.flags.partial_path = 0;
        path_info.start_pos = points[0].pos;
        path_info.start_nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
        path_info.end_pos = points[1].pos;
        path_info.end_nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
        return path;
    }
    // A* from the destination back to the start, over the road points.
    Road_heap.start_session();
    unsigned short end_high = (unsigned short)(int)ceil((double)end_u);
    unsigned short end_low = (unsigned short)(int)floor((double)end_u);
    bool end_both_ways = true;
    if (!real_params.ignore_road_directionality && end_road->road_flags.one_way != 0) {
        end_both_ways = false;
    } else if (!object_path_road_query_for_path(end_road, end_high, nullptr, end_high, vec_dist(points[1].pos, end_points[end_high].pos), start_on_road, &Road_heap, false, gps)) {
        return nullptr;
    }
    if (!object_path_road_query_for_path(end_road, end_low, nullptr, end_low, vec_dist(points[1].pos, end_points[end_low].pos), start_on_road, &Road_heap, false, gps)) {
        return nullptr;
    }
    unsigned short start_high = (unsigned short)(int)ceil((double)start_u);
    unsigned short start_low = (unsigned short)(int)floor((double)start_u);
    bool start_both_ways = real_params.ignore_road_directionality || start_road->road_flags.one_way == 0;
    bool backwards = false;
    bool found = false;
    road_spline_identifier id = {};
    unsigned short chosen_point = 0;
    unsigned short other_point = 0;
    while (0 < Road_heap.num_road_splines) {
        id = Road_heap.remove_best_road_spline();
        object_path_road* road = id.road_spline;
        if (road == nullptr || (road->road_flags.gps_only != 0 && !gps)) {
            continue;
        }
        unsigned short point = id.point;
        if (road == start_road) {
            if (point == start_high) {
                chosen_point = start_high;
                other_point = start_low;
                found = true;
            } else if (start_both_ways && point == start_low) {
                backwards = true;
                chosen_point = start_low;
                other_point = start_high;
                found = true;
            }
            if (found) {
                id.point = chosen_point;
                id.road_spline = start_road;
                break;
            }
        }
        const road_heap_data* data = Road_heap.get_road_spline_data(&id);
        const road_spline_point* road_points = road->spline_data.points.to_pointer();
        bool block_next = false;
        bool block_prev = false;
        if (road->has_breaks()) {
            road_broken_state here = road->is_broken_at_index(point);
            road_broken_state before = road->is_broken_at_index(point - 1);
            block_next = here == ROAD_BROKEN_COMPLETELY || here == ROAD_BROKEN_RIGHT;
            block_prev = before == ROAD_BROKEN_COMPLETELY || before == ROAD_BROKEN_LEFT;
        }
        if (!gps && road->has_road_blocks()) {
            if (road->is_road_blocked_at_index(point)) {
                block_next = true;
            }
            if (road->is_road_blocked_at_index(point - 1)) {
                block_prev = true;
            }
        }
        unsigned short next_point = (unsigned short)(point + 1);
        if (!block_next && next_point < road->spline_data.num_points && (road->road_flags.one_way == 0 || real_params.ignore_road_directionality)) {
            float cost = vec_dist(road_points[point].pos, road_points[next_point].pos) + data->arrival_cost;
            object_path_road_query_for_path(road, next_point, road, point, cost, start_on_road, &Road_heap, false, gps);
        }
        if (!block_prev && point != 0) {
            unsigned short prev_point = (unsigned short)(point - 1);
            float cost = vec_dist(road_points[point].pos, road_points[prev_point].pos) + data->arrival_cost;
            object_path_road_query_for_path(road, prev_point, road, point, cost, start_on_road, &Road_heap, false, gps);
        }
        const road_spline_connection* connections = road->spline_data.connections.to_pointer();
        for (int i = 0; i < road->spline_data.num_connections; i++) {
            const road_spline_connection& connection = connections[i];
            if (connection.from_pt != point || connection.handle == 0xFFFFFFFF) {
                continue;
            }
            object_path_road* other = (object_path_road*)World.object_from_handle(connection.handle);
            if (other != nullptr && other->get_obj_type() == OT_PATH_ROAD && (other->road_flags.special == 0 || real_params.use_special_roads)) {
                object_path_road_query_for_path(other, connection.to_pt, road, point, data->arrival_cost, start_on_road, &Road_heap, false, gps);
            }
        }
    }
    if (found) {
        // Walk the arrivals forward from the start, a lane for each piece of road.
        int num_lanes = 0;
        bool going_down = false;
        bool going_up = false;
        bool tangent_check = false;
        bool need_curve = false;
        bool joined = false;
        cubic_bezier_spline previous;
        previous.num_knots = 0;
        const road_spline_point* corner_from = &start_points[chosen_point];
        const vector* corner = nullptr;
        for (;;) {
            object_path_road* road = id.road_spline;
            if (((end_both_ways && id.point == end_high) || id.point == end_low) && road == end_road) {
                break;
            }
            const road_heap_data* data = Road_heap.get_road_spline_data(&id);
            unsigned short arrival = data->arrival_point;
            unsigned int arrival_spline = data->arrival_spline;
            int step = (int)arrival - (int)id.point;
            if (arrival_spline != road->get_handle() || (step != 1 && step != -1)) {
                // A connection to another road: no lane, only the turn it makes.
                bool at_start = road == start_road && chosen_point == id.point;
                id.point = arrival;
                object* next_object = arrival_spline != 0xFFFFFFFF ? World.object_from_handle(arrival_spline) : nullptr;
                object_path_road* next_road = next_object != nullptr && next_object->get_obj_type() == OT_PATH_ROAD ? (object_path_road*)next_object : nullptr;
                id.road_spline = next_road;
                const road_spline_point* next_points = next_road->spline_data.points.to_pointer();
                if (corner != nullptr) {
                    bb_pf_road_turn_side(path_info, *corner, corner_from->pos, next_points[id.point].pos);
                    corner = nullptr;
                }
                going_up = false;
                going_down = false;
                if (!joined) {
                    path_info.flags.ixc_turning = 1;
                    corner = &next_points[id.point].pos;
                    joined = true;
                    if (at_start) {
                        corner_from = &start_points[other_point];
                    }
                }
                tangent_check = true;
                if (at_start) {
                    vector on_next;
                    float next_u = next_road->find_closest_point_on_road(points[0].pos, on_next, false);
                    float next_width = next_road->get_width(next_u);
                    if (vec_xz_dist(points[0].pos, on_next) < next_width) {
                        path_info.closest_road_spline = next_road->get_handle();
                        path_info.closest_road_index = next_road->find_closest_point_on_road(points[0].pos, on_next, false);
                        tangent_check = false;
                        need_curve = true;
                    }
                }
                continue;
            }
            const road_spline_point* road_points = road->spline_data.points.to_pointer();
            if (!joined) {
                const road_spline_connection* connections = road->spline_data.connections.to_pointer();
                for (int i = 0; i < road->spline_data.num_connections; i++) {
                    if (connections[i].from_pt == arrival) {
                        joined = true;
                        break;
                    }
                }
            }
            if (corner != nullptr) {
                bb_pf_road_turn_side(path_info, *corner, corner_from->pos, road_points[arrival].pos);
                corner = nullptr;
            }
            navmesh_lane_spline_node* node = get_free_ls_node();
            if (node == nullptr) {
                return path;
            }
            going_down = arrival <= id.point;
            going_up = !going_down;
            bb_pf_ls_from_road(node, &road_points[id.point], &road_points[arrival], going_down);
            cubic_bezier_spline current;
            bb_pf_ls_set_points(current, node);
            bool skip = false;
            if (tangent_check) {
                tangent_check = false;
                if (previous.num_knots == 0) {
                    need_curve = true;
                } else {
                    float dot = bb_pf_spline_join_dot(previous, current);
                    if (dot < TURN_COSINE) {
                        navmesh_lane_spline_node* last = path != nullptr ? path->prev : nullptr;
                        navmesh_lane_spline_node* curve = generate_intersection_curve(last, node, &previous, &current, dot);
                        if (curve != nullptr) {
                            bb_pf_ls_set_points(current, node);
                            if (last->lane.arc_length[0] < MIN_LANE_LENGTH) {
                                vehicle_free_ls_node(&path, last);
                                num_lanes--;
                            }
                            if (node->lane.arc_length[0] < MIN_LANE_LENGTH) {
                                skip = true;
                                bb_pf_ls_set_points(current, curve);
                            }
                            bb_pf_ls_append(&path, curve);
                            num_lanes++;
                        }
                    }
                }
            }
            previous = current;
            if (!skip) {
                bb_pf_ls_append(&path, node);
                num_lanes++;
                if (cap_lanes && MAX_LANES < num_lanes) {
                    break;
                }
            } else {
                vehicle_free_individual_ls_node(node);
            }
            id.point = data->arrival_point;
        }

        // The start of the road piece the vehicle is on, ahead of the first full lane.
        unsigned short end_point = id.point;
        unsigned short start_point = backwards ? start_low : start_high;
        road_spline_identifier start_id;
        start_id.road_spline = start_road;
        start_id.point = start_point;
        const road_heap_data* start_data = Road_heap.get_road_spline_data(&start_id);
        bool same_road = start_data->arrival_spline == start_road->get_handle();
        navmesh_lane_spline_node* first_lane = nullptr;
        if ((start_data->arrival_point <= start_point && same_road) || (float)start_point <= start_u || start_point == 0) {
            if ((start_data->arrival_point < start_point || !same_road) && (float)start_point < start_u && start_point != start_road->spline_data.num_points - 1) {
                first_lane = get_free_ls_node();
                if (first_lane == nullptr) {
                    return path;
                }
                bb_pf_ls_from_road(first_lane, &start_points[start_point + 1], &start_points[start_point], true);
            }
        } else {
            first_lane = get_free_ls_node();
            if (first_lane == nullptr) {
                return path;
            }
            bb_pf_ls_from_road(first_lane, &start_points[start_point - 1], &start_points[start_point], false);
        }
        if (first_lane != nullptr) {
            bool skip = false;
            cubic_bezier_spline spline;
            bb_pf_ls_set_points(spline, first_lane);
            if (!need_curve || path == nullptr) {
                if (previous.num_knots == 0) {
                    previous = spline;
                }
            } else {
                navmesh_lane_spline_node* head = path;
                cubic_bezier_spline head_spline;
                bb_pf_ls_set_points(head_spline, head);
                float dot = bb_pf_spline_join_dot(spline, head_spline);
                if (dot < TURN_COSINE) {
                    navmesh_lane_spline_node* curve = generate_intersection_curve(first_lane, head, &spline, &head_spline, dot);
                    if (curve != nullptr) {
                        if (head->lane.arc_length[0] < MIN_LANE_LENGTH) {
                            vehicle_free_ls_node(&path, head);
                        }
                        if (first_lane->lane.arc_length[0] < MIN_LANE_LENGTH) {
                            skip = true;
                            bb_pf_ls_set_points(spline, curve);
                        }
                        // The curve goes in front of the path.
                        num_lanes++;
                        bb_pf_ls_append(&path, curve);
                        path = curve;
                    }
                }
            }
            if (!skip) {
                bb_pf_ls_append(&path, first_lane);
                path = first_lane;
            } else {
                vehicle_free_individual_ls_node(first_lane);
            }
        }
        // And the road piece the destination is on, after the last.
        if (id.point == end_point && id.road_spline == end_road) {
            bool last_up = going_up;
            bool last_down = going_down;
            bool from_connection = false;
            bool extend = true;
            if (!last_down && !last_up) {
                if ((float)(int)end_point <= end_u) {
                    if (end_u <= (float)(int)end_point) {
                        extend = false;
                    } else {
                        last_up = true;
                        tangent_check = true;
                        from_connection = true;
                    }
                } else {
                    last_down = true;
                    tangent_check = true;
                    from_connection = true;
                }
            }
            if (extend) {
                if (last_up) {
                    extend = (float)(int)end_point != (float)(end_road->spline_data.num_points - 1) && (float)(int)end_point < end_u;
                } else {
                    extend = last_down && end_point != 0 && end_u < (float)(int)end_point;
                }
            }
            if (extend) {
                navmesh_lane_spline_node* node = get_free_ls_node();
                if (node == nullptr) {
                    return path;
                }
                bb_pf_ls_from_road(node, &end_points[end_point], last_up ? &end_points[end_point + 1] : &end_points[end_point - 1], !last_up);
                cubic_bezier_spline spline;
                bb_pf_ls_set_points(spline, node);
                bool skip = false;
                if (tangent_check && path != nullptr) {
                    navmesh_lane_spline_node* last = path->prev;
                    cubic_bezier_spline last_spline;
                    last_spline.set_points(last->lane.num_knots, last->lane.knots.to_pointer(), last->lane.in_handles.to_pointer(), last->lane.out_handles.to_pointer());
                    float dot = bb_pf_spline_join_dot(last_spline, spline);
                    if (dot < TURN_COSINE) {
                        navmesh_lane_spline_node* curve = generate_intersection_curve(last, node, &last_spline, &spline, dot);
                        if (curve != nullptr) {
                            if (last->lane.arc_length[0] < MIN_LANE_LENGTH) {
                                vehicle_free_ls_node(&path, last);
                            }
                            if (node->lane.arc_length[0] < MIN_LANE_LENGTH) {
                                skip = true;
                            }
                            bb_pf_ls_append(&path, curve);
                        }
                    }
                }
                if (!from_connection) {
                    // The last lane bends to the destination's side of the road.
                    vector* knots = node->lane.knots.to_pointer();
                    knots[1].x += end_offset.x;
                    knots[1].y += end_offset.y;
                    knots[1].z += end_offset.z;
                    vector* in_handles = node->lane.in_handles.to_pointer();
                    in_handles[1].x += end_offset.x;
                    in_handles[1].y += end_offset.y;
                    in_handles[1].z += end_offset.z;
                }
                if (!skip) {
                    bb_pf_ls_append(&path, node);
                } else {
                    vehicle_free_individual_ls_node(node);
                }
            }
        }
        path_info.flags.ixc_can_control = 1;
        path_info.flags.jumping = 0;
        path_info.flags.next_point_inc = backwards ? 0 : 1;
        path_info.flags.use_road_offset = 1;
        path_info.flags.off_road = 0;
        path_info.flags.road_spline_path = 1;
        path_info.flags.partial_path = 0;
        path_info.start_pos = points[0].pos;
        path_info.start_nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
        path_info.end_pos = points[1].pos;
        path_info.end_nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
    }
    if (Road_heap.in_session) {
        Road_heap.end_session();
    }
    return path;
}

// 0x00B01800 (:13183)
bool rfg_find_closest_pt_on_road_to_bbox(vector& closest_pt, const vector& bmin, const vector& bmax, const vector& other_point)
{
    // Magic number: navpoint type 5 is a road point. rfg.exe takes a random point on its farthest link.
    closest_pt = ZERO_VECTOR;
    float best_dist_sq = 3.4028235e+38f; // Magic number: FLT_MAX.
    vector best_on_box = ZERO_VECTOR;
    navpoint* best = nullptr;
    int count = World.get_num_objects<navpoint>();
    for (int i = 0; i < count; i++) {
        navpoint* navp = World.get_object<navpoint>(i);
        if (navp->type == 5) {
            vector on_box;
            float box_dist = bbox_find_closest_point(on_box, bmin, bmax, navp->pos);
            float dist_sq = vec_dist_squared(navp->pos, other_point);
            if (0.0f < box_dist && dist_sq < best_dist_sq) {
                best_on_box = on_box;
                best = navp;
                best_dist_sq = dist_sq;
            }
        }
    }
    if (best == nullptr) {
        return false;
    }
    float farthest = 0.0f;
    vector far_point = ZERO_VECTOR;
    for (int i = 0; i < best->num_links; i++) {
        float dist_sq = vec_dist_squared(best_on_box, best->links[i]->pos);
        if (farthest < dist_sq) {
            far_point = best->links[i]->pos;
            farthest = dist_sq;
        }
    }
    vector along = far_point - best->pos;
    float t = fl_rand();
    closest_pt = vector(best->pos.x + along.x * t, best->pos.y + along.y * t, best->pos.z + along.z * t);
    return true;
}

// 0x00B17000 (:13234)
const navpoint* rfg_find_best_exit_point(const object* building, navpoint* starting_navp, bool one_way, bool use_doors)
{
    // Magic numbers: 11 and 12 are the two transition types, 15 and 16 jumps and leaps that only go one way; 32 points.
    static const signed char ONE_WAY_TYPES[4] = { NAVPOINT_TYPE_TRANSITION, NAVPOINT_TYPE_LADDER_TRANSITION, NAVPOINT_TYPE_TRANSITION_JUMP, NAVPOINT_TYPE_LEAP_OVER };
    static const signed char TWO_WAY_TYPES[2] = { NAVPOINT_TYPE_TRANSITION, NAVPOINT_TYPE_LADDER_TRANSITION };
    const signed char* types = one_way ? ONE_WAY_TYPES : TWO_WAY_TYPES;
    int num_types = one_way ? 4 : 2;
    farray<const navpoint*,32> points;
    object* child = building->child_ptr;
    while (child != nullptr) {
        if (child->get_obj_type() == OT_NAVPOINT) {
            for (int i = 0; i < num_types; i++) {
                if (types[i] == ((navpoint*)child)->type) {
                    points.add((const navpoint*)child);
                    break;
                }
            }
            if (points.capacity() <= points.size()) {
                break;
            }
        }
        child = child->child_next;
        if (child == building->child_ptr) {
            break;
        }
    }
    if (0 < points.size()) {
        return find_best_exit_trans_point(starting_navp, points, types, num_types, use_doors);
    }
    return nullptr;
}

// 0x00AEC200 (:13295)
void rfg_pf_system_init()
{
}

// 0x00AF5DF0 (:13318)
void rfg_pf_level_init_pre()
{
    Free_path_nodes = nullptr;
    for (int i = 0; i < 1024; i++) {
        bb_pf_path_append(&Free_path_nodes, &Path_nodes[i]);
    }
    Nav_heap.in_session = false;
    Nav_heap.session_id = 0;
    Nav_heap.num_allocated_heap_data = 0;
    Nav_heap.num_nav_cells = 0;
    Navpoint_heap.num_navpoints = 0;
    Navpoint_heap.in_session = false;
    Navpoint_heap.session_id = 0;
    nav_cell_detour_level_init_pre();
    Player_nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
}

// 0x00AEC210 (:13349)
void rfg_pf_level_shutdown()
{
}

// 0x00AEC220 (:13353)
void rfg_pf_get_smoothing_position(human* hp, vector& dest_pos)
{
    // Magic number: PI / 16 along the circle.
    const double STEP = 0.19634954631328583;
    vector radial(hp->pos.x - hp->pf.center_of_circle.x, 0.0f, hp->pos.z - hp->pf.center_of_circle.z);
    radial.normalize();
    float turn = hp->pf.flags.turn_right != 0 ? 1.0f : -1.0f;
    float radius = hp->pf.movement_radius;
    vector tangent(turn * radial.z - radial.y * 0.0f, radial.x * 0.0f - radial.z * 0.0f, radial.y * 0.0f - turn * radial.x);
    float along = (float)sin(STEP) * radius;
    float out = (float)cos(STEP) * radius;
    dest_pos.x = out * radial.x + hp->pf.center_of_circle.x + tangent.x * along;
    dest_pos.y = hp->pf.center_of_circle.y + out * radial.y + tangent.y * along;
    dest_pos.z = hp->pf.center_of_circle.z + radial.z * out + tangent.z * along;
    dest_pos.y = hp->pos.y;
}

// The angle wrapped to 0..2 pi, as rfg.exe does it: with a double for the subtraction.
static float bb_pf_wrap_angle(float angle)
{
    // Magic numbers: 2 pi as a float, 6.2831855, and the same as a double.
    const double TWO_PI_D = 6.2831854820251465;
    while ((double)TWO_PI_D < (double)angle) {
        angle = (float)((double)angle - TWO_PI_D);
    }
    while (angle < 0.0f) {
        angle = (float)((double)angle + TWO_PI_D);
    }
    return angle;
}

// The tangent from a point onto a circle around center, on one side, and the length of the arc to it.
struct bb_pf_tangent_arc
{
    vector tangent_point;
    float angle_final;
    float arc_length;
    float straight_length;
};

// Magic number: 2 pi.
static void bb_pf_circle_tangent(bb_pf_tangent_arc& arc, float dest_x, float dest_z, float center_x, float center_z, float radius, bool right_turn, const vector2& fvec, const vector& pos)
{
    float to_x = dest_x - center_x;
    float to_z = dest_z - center_z;
    float dist = sqrtf(to_x * to_x + to_z * to_z);
    arc.straight_length = sqrtf(dist * dist - radius * radius);
    float bearing = (float)atan2((double)to_z, (double)to_x);
    float spread = fl_acos(radius / dist);
    arc.angle_final = bb_pf_wrap_angle(right_turn ? bearing + spread : bearing - spread);
    float cosine;
    float sine = fl_sincos(arc.angle_final, &cosine);
    arc.tangent_point = vector(cosine * radius + center_x, pos.y, sine * radius + center_z);
    float from_x = arc.tangent_point.x - pos.x;
    float from_z = arc.tangent_point.z - pos.z;
    float cos_arc = ((arc.tangent_point.z - center_z) * (pos.z - center_z) + (arc.tangent_point.x - center_x) * (pos.x - center_x)) / (radius * radius);
    if (cos_arc < -1.0f) {
        cos_arc = -1.0f;
    }
    if (1.0f <= cos_arc) {
        cos_arc = 1.0f;
    }
    float swept = fl_acos(cos_arc);
    if (from_z * fvec.y + from_x * fvec.x < 0.0f) {
        swept = 6.2831855f - swept;
    }
    arc.arc_length = bb_pf_wrap_angle(swept) * radius;
}

// 0x00B1C610 (:13375)
bool rfg_pf_calculate_smoothing_info(human* hp)
{
    // Magic numbers: a turn under 30 degrees (cosine 0.866) needs no curve; over 3.4 m/s a human runs; a turn of 1 m radius
    // up to three quarters of a circle; cos 45 for the turning animations; 0.1 m (squared) at the node counts as there.
    const float RADIUS = 1.0f;
    const float MAX_ARC = 4.712389f;
    if (hp->hflags.on_mover != 0) {
        return false;
    }
    path_node* node = hp->pf.path;
    if (node == nullptr) {
        return false;
    }
    if (vec_dist_squared(node->pos, hp->pos) < 0.1f) {
        if (node == hp->pf.path->prev) {
            return false;
        }
        node = rfg_pf_advance_to_next_path_node(&hp->pf.path, node);
    }
    if (node == nullptr) {
        return false;
    }
    const vector& dest = node->pos;
    vector dir = dest - hp->pos;
    dir.normalize_safe();
    float facing = hp->orient.fvec.y * dir.y + hp->orient.fvec.x * dir.x + hp->orient.fvec.z * dir.z;
    if (0.8660254f < facing || hp->cur_state != HS_STANDARD || hp->cur_stance != HST_STAND) {
        return false;
    }
    bool running = 3.4f < hp->actual_velocity_mag;
    if (running && facing <= 0.0f) {
        if (!Pf_use_turn_anims) {
            return false;
        }
        animation_action action;
        float time = 0.3f;
        if (facing < -0.70710677f) {
            action = ANIMATION_ACTION_TURN_BACK_RUN;
            time = -1.0f;
        } else {
            action = 0.0f < dir.x * hp->orient.rvec.x + dir.y * hp->orient.rvec.y + dir.z * hp->orient.rvec.z ? ANIMATION_ACTION_TURN_RIGHT_RUN : ANIMATION_ACTION_TURN_LEFT_RUN;
        }
        hp->char_inst->play_action(action, ANIM_ALIAS_TURNING, 1.0f, 0);
        matrix orient;
        orient.make_quick(dir);
        hp->char_inst->set_action_dest_orient(hp->orient, orient, ANIM_ALIAS_TURNING, time);
        return false;
    }
    vector2 rvec(hp->orient.rvec.x, hp->orient.rvec.z);
    rvec.normalize_safe();
    vector2 fvec(hp->orient.fvec.x, hp->orient.fvec.z);
    fvec.normalize_safe();
    hp->pf.movement_radius = RADIUS;
    vector2 right_center(rvec.x + hp->pos.x, rvec.y + hp->pos.z);
    vector2 left_center(-rvec.x + hp->pos.x, -rvec.y + hp->pos.z);
    float cos_right = hp->pos.x - right_center.x;
    if (cos_right < -1.0f) {
        cos_right = -1.0f;
    } else if (1.0f < cos_right) {
        cos_right = 1.0f;
    }
    float right_start = bb_pf_wrap_angle((float)acos((double)cos_right));
    float left_start = bb_pf_wrap_angle((float)asin((double)(hp->pos.z - left_center.y)));
    float to_right_x = dest.x - right_center.x;
    float to_right_z = dest.z - right_center.y;
    float to_left_x = dest.x - left_center.x;
    float to_left_z = dest.z - left_center.y;
    bool use_left = 1.0f <= sqrtf(to_left_x * to_left_x + to_left_z * to_left_z);
    bool use_right = 1.0f <= sqrtf(to_right_x * to_right_x + to_right_z * to_right_z);
    if (!use_left && !use_right) {
        return false;
    }
    float left_cost = 1000.0f;
    float right_cost = 1000.0f;
    path_node* left_curve = nullptr;
    path_node* right_curve = nullptr;
    bb_pf_tangent_arc left_arc = {};
    bb_pf_tangent_arc right_arc = {};
    if (use_left) {
        bb_pf_circle_tangent(left_arc, dest.x, dest.z, left_center.x, left_center.y, RADIUS, false, fvec, hp->pos);
        if (left_arc.arc_length <= MAX_ARC && nav_cell_check_curve(&left_curve, hp->pf.last_visited_nav_cell_handle, hp->pos, left_arc.tangent_point, RADIUS, left_center, left_start, left_arc.angle_final, dest, left_arc.arc_length, false, false)) {
            left_cost = left_arc.straight_length + left_arc.arc_length;
        } else {
            use_left = false;
        }
    }
    if (use_right) {
        bb_pf_circle_tangent(right_arc, dest.x, dest.z, right_center.x, right_center.y, RADIUS, true, fvec, hp->pos);
        if (right_arc.arc_length <= MAX_ARC && nav_cell_check_curve(&right_curve, hp->pf.last_visited_nav_cell_handle, hp->pos, right_arc.tangent_point, RADIUS, right_center, right_start, right_arc.angle_final, dest, right_arc.arc_length, true, false)) {
            right_cost = right_arc.straight_length + right_arc.arc_length;
        } else {
            use_right = false;
        }
    }
    hp->pf.center_of_circle.y = hp->pos.y;
    if (use_left && use_right) {
        if (left_cost <= right_cost) {
            use_right = false;
            bb_pf_free_path(&right_curve);
        } else {
            use_left = false;
            bb_pf_free_path(&left_curve);
        }
    }
    path_node* curve = use_right ? right_curve : (use_left ? left_curve : nullptr);
    if (curve != nullptr) {
        if (hp->pf.path != nullptr) {
            bb_pf_path_splice(&curve, hp->pf.path);
        }
        hp->pf.path = curve;
    }
    finalize_path(&hp->pf.path);
    float length;
    if (use_right) {
        hp->pf.flags.turn_right = 1;
        hp->pf.center_of_circle.x = right_center.x;
        hp->pf.center_of_circle.z = right_center.y;
        hp->pf.smoothPoint = right_arc.tangent_point;
        length = right_arc.arc_length;
    } else if (use_left) {
        hp->pf.flags.turn_right = 0;
        hp->pf.center_of_circle.x = left_center.x;
        hp->pf.center_of_circle.z = left_center.y;
        hp->pf.smoothPoint = left_arc.tangent_point;
        length = left_arc.arc_length;
    } else {
        if (0.70710677f < facing || !Pf_use_turn_anims) {
            return false;
        }
        // Magic number: a run's actions follow the walk's by 3.
        int run = running ? 3 : 0;
        animation_action action;
        float time = 0.3f;
        if (facing < -0.70710677f) {
            action = (animation_action)(ANIMATION_ACTION_TURN_BACK_WALK + run);
            time = -1.0f;
        } else if (dir.x * hp->orient.rvec.x + dir.y * hp->orient.rvec.y + dir.z * hp->orient.rvec.z <= 0.0f) {
            action = (animation_action)(ANIMATION_ACTION_TURN_LEFT_WALK + run);
        } else {
            action = (animation_action)(ANIMATION_ACTION_TURN_RIGHT_WALK + run);
        }
        hp->char_inst->play_action(action, ANIM_ALIAS_TURNING, 1.0f, 0);
        matrix orient;
        orient.make_quick(dir);
        hp->char_inst->set_action_dest_orient(hp->orient, orient, ANIM_ALIAS_TURNING, time);
        return false;
    }
    hp->pf.curve_distance = length;
    hp->pf.smooth_start_pos = hp->pos;
    hp->pf.smooth_start_fvec = hp->orient.fvec;
    hp->pf.smooth_start_fvec.flatten_normalize_safe();
    havok_terrain_height(hp->pf.center_of_circle.y, hp->pf.center_of_circle.x, hp->pf.center_of_circle.z, false, nullptr);
    return true;
}

// 0x00AF5EF0 (:13815)
int rfg_pf_calculate_turning_info_for_flyers(vehicle* vp, const vector& dest_point, int max_points, vector* points)
{
    // Magic numbers: circles of 20 m, 30 m for a bomber, three quarters of the way round at most; a point each 30 degrees
    // (0.52358997 radians); cos 45 of a turn needs no circle; a flyer climbing near straight up (0.85) uses its nose.
    vector ground = vp->pos;
    havok_terrain_height(ground.y, ground.x, ground.z, false, nullptr);
    if (vec_xz_dist_squared(dest_point, ground) < 0.1f) {
        return 0;
    }
    vector to_dest = dest_point - ground;
    to_dest.normalize_safe();
    vector velocity;
    vehicle_get_velocity(vp, velocity);
    float speed = velocity.mag_and_normalize_safe();
    if (vehicle_is_bomber(vp) || speed < 1.0f || 0.85f < velocity.y) {
        velocity = vp->orient.fvec;
    }
    if (0.70710677f < to_dest.y * velocity.y + to_dest.x * velocity.x + to_dest.z * velocity.z) {
        return 0;
    }
    vector2 side(velocity.z * UP_VECTOR.y - UP_VECTOR.z * velocity.y, UP_VECTOR.x * velocity.y - velocity.x * UP_VECTOR.y);
    side.normalize_safe();
    vector2 flat(velocity.x, velocity.z);
    flat.normalize_safe();
    float radius = vehicle_is_bomber(vp) ? 30.0f : 20.0f;
    vector2 right_center(radius * side.x + ground.x, radius * side.y + ground.z);
    vector2 left_center(-side.x * radius + ground.x, -side.y * radius + ground.z);
    float right_dist = sqrtf((dest_point.x - right_center.x) * (dest_point.x - right_center.x) + (dest_point.z - right_center.y) * (dest_point.z - right_center.y));
    float left_dist = sqrtf((dest_point.x - left_center.x) * (dest_point.x - left_center.x) + (dest_point.z - left_center.y) * (dest_point.z - left_center.y));
    bool use_left = radius <= left_dist;
    bool use_right = radius <= right_dist;
    if (!use_left && !use_right) {
        return 0;
    }
    float max_arc = radius * 6.2831855f * 0.75f;
    vector flat_pos(vp->pos.x, ground.y, vp->pos.z);
    bb_pf_tangent_arc left_arc = {};
    bb_pf_tangent_arc right_arc = {};
    float left_cost = 1000.0f;
    if (use_left) {
        // rfg.exe measures the sweep from the ground position's circle but its forward check from the vehicle: the same xz.
        bb_pf_circle_tangent(left_arc, dest_point.x, dest_point.z, left_center.x, left_center.y, radius, false, flat, flat_pos);
        use_left = left_arc.arc_length <= max_arc;
        if (use_left) {
            left_cost = left_arc.straight_length + left_arc.arc_length;
        }
    }
    if (use_right) {
        bb_pf_circle_tangent(right_arc, dest_point.x, dest_point.z, right_center.x, right_center.y, radius, true, flat, flat_pos);
        if (max_arc < right_arc.arc_length) {
            use_right = false;
        }
    }
    float turn;
    vector2 center;
    float arc_length;
    if (use_right && (!use_left || right_arc.straight_length + right_arc.arc_length < left_cost)) {
        turn = 1.0f;
        center = right_center;
        arc_length = right_arc.arc_length;
    } else if (use_left) {
        turn = -1.0f;
        center = left_center;
        arc_length = left_arc.arc_length;
    } else {
        return 0;
    }
    vector radial(ground.x - center.x, 0.0f, ground.z - center.y);
    radial.normalize();
    vector tangent(turn * radial.z - radial.y * 0.0f, radial.x * 0.0f - radial.z * 0.0f, radial.y * 0.0f - turn * radial.x);
    int num_points = 0;
    int step = 0;
    while (num_points < max_points) {
        step++;
        float angle = (float)step * 0.52358997f;
        if (arc_length < angle * radius) {
            return num_points;
        }
        float along = (float)sin((double)angle) * radius;
        float out = (float)cos((double)angle) * radius;
        points[num_points].x = out * radial.x + center.x + along * tangent.x;
        points[num_points].y = ground.y;
        points[num_points].z = radial.z * out + center.y + along * tangent.z;
        num_points++;
    }
    return num_points;
}

// 0x00B1D6D0 (:14117)
bool rfg_pf_calculate_turning_info_for_vehicle(vehicle* vp, path_node** current_path)
{
    // Magic numbers: circles of 7.5 m, 12.5 m for vehicle subtype 5; three quarters of the way round at most; cos 45 of a turn
    // needs no circle.
    path_node* first = *current_path;
    if (first == nullptr) {
        return false;
    }
    path_node* node = first;
    bool at_first = vec_xz_dist_squared(first->pos, vp->pos) < 0.1f;
    if (at_first) {
        node = first->next != *current_path ? first->next : nullptr;
    }
    if (node == nullptr) {
        return false;
    }
    const vector& dest = node->pos;
    vector dir = dest - vp->pos;
    dir.normalize_safe();
    if (0.70710677f < vp->orient.fvec.y * dir.y + dir.x * vp->orient.fvec.x + vp->orient.fvec.z * dir.z) {
        return false;
    }
    vector2 rvec(vp->orient.rvec.x, vp->orient.rvec.z);
    rvec.normalize_safe();
    vector2 fvec(vp->orient.fvec.x, vp->orient.fvec.z);
    fvec.normalize_safe();
    float radius = vp->get_sub_type() == 5 ? 12.5f : 7.5f;
    vector2 right_center(radius * rvec.x + vp->pos.x, radius * rvec.y + vp->pos.z);
    vector2 left_center(-rvec.x * radius + vp->pos.x, -rvec.y * radius + vp->pos.z);
    float cos_right = (vp->pos.x - right_center.x) / radius;
    if (cos_right < -1.0f) {
        cos_right = -1.0f;
    } else if (1.0f < cos_right) {
        cos_right = 1.0f;
    }
    float right_start = bb_pf_wrap_angle((float)acos((double)cos_right));
    float left_start = bb_pf_wrap_angle((float)asin((double)((vp->pos.z - left_center.y) / radius)));
    float right_dist = sqrtf((dest.x - right_center.x) * (dest.x - right_center.x) + (dest.z - right_center.y) * (dest.z - right_center.y));
    float left_dist = sqrtf((dest.x - left_center.x) * (dest.x - left_center.x) + (dest.z - left_center.y) * (dest.z - left_center.y));
    bool use_left = radius <= left_dist;
    bool use_right = radius <= right_dist;
    if (!use_left && !use_right) {
        return false;
    }
    float max_arc = radius * 6.2831855f * 0.75f;
    float left_cost = 1000.0f;
    float right_cost = 1000.0f;
    path_node* left_curve = nullptr;
    path_node* right_curve = nullptr;
    bb_pf_tangent_arc left_arc = {};
    bb_pf_tangent_arc right_arc = {};
    uint64_t cell_handle = vp->m_navcell_info.last_visited_nav_cell_handle;
    if (use_left) {
        bb_pf_circle_tangent(left_arc, dest.x, dest.z, left_center.x, left_center.y, radius, false, fvec, vp->pos);
        if (left_arc.arc_length <= max_arc && nav_cell_check_curve(&left_curve, cell_handle, vp->pos, left_arc.tangent_point, radius, left_center, left_start, left_arc.angle_final, dest, left_arc.arc_length, false, true)) {
            left_cost = left_arc.straight_length + left_arc.arc_length;
        } else {
            use_left = false;
        }
    }
    if (use_right) {
        bb_pf_circle_tangent(right_arc, dest.x, dest.z, right_center.x, right_center.y, radius, true, fvec, vp->pos);
        if (right_arc.arc_length <= max_arc && nav_cell_check_curve(&right_curve, cell_handle, vp->pos, right_arc.tangent_point, radius, right_center, right_start, right_arc.angle_final, dest, right_arc.arc_length, true, true)) {
            right_cost = right_arc.straight_length + right_arc.arc_length;
        } else {
            use_right = false;
        }
    }
    if (!use_left && !use_right) {
        return false;
    }
    if (use_left && use_right) {
        if (left_cost <= right_cost) {
            use_right = false;
            bb_pf_free_path(&right_curve);
        } else {
            use_left = false;
            bb_pf_free_path(&left_curve);
        }
    }
    // The node the vehicle is at waits outside while the curve goes in front of the path.
    if (at_first) {
        bb_pf_path_unlink(current_path, first);
    }
    path_node* curve = use_right ? right_curve : left_curve;
    if (curve != nullptr) {
        if (*current_path != nullptr) {
            bb_pf_path_splice(&curve, *current_path);
        }
        *current_path = curve;
    }
    if (at_first) {
        (*current_path)->prev->next = first;
        first->prev = (*current_path)->prev;
        (*current_path)->prev = first;
        first->next = *current_path;
        *current_path = first;
    }
    human* driver = vehicle_get_first_passenger(vp, VSI_DRIVER);
    if (driver == nullptr) {
        return false;
    }
    driver->pf.center_of_circle.y = vp->pos.y;
    float length;
    if (use_right) {
        driver->pf.flags.turn_right = 1;
        driver->pf.center_of_circle.x = right_center.x;
        driver->pf.center_of_circle.z = right_center.y;
        driver->pf.smoothPoint = right_arc.tangent_point;
        length = right_arc.arc_length;
    } else {
        driver->pf.flags.turn_right = 0;
        driver->pf.center_of_circle.x = left_center.x;
        driver->pf.center_of_circle.z = left_center.y;
        driver->pf.smoothPoint = left_arc.tangent_point;
        length = left_arc.arc_length;
    }
    driver->pf.curve_distance = length;
    driver->pf.smooth_start_pos = vp->pos;
    driver->pf.smooth_start_fvec = vp->orient.fvec;
    driver->pf.smooth_start_fvec.flatten_normalize_safe();
    havok_terrain_height(driver->pf.center_of_circle.y, driver->pf.center_of_circle.x, driver->pf.center_of_circle.z, false, nullptr);
    driver->pf.flags.smoothing = 1;
    return true;
}

// 0x00AEC4F0 (:14454)
bool rfg_pf_add_impassable_nav_cell(nav_cell* ncp)
{
    if (Num_impassable_nav_cells < 384 && (ncp->flags & NAV_CELL_FLAG_BLOCKED) == 0) {
        ncp->flags |= NAV_CELL_FLAG_BLOCKED;
        Impassable_nav_cells[Num_impassable_nav_cells] = ncp;
        Num_impassable_nav_cells++;
        return true;
    }
    return false;
}

// 0x00AEC530 (:14467)
void rfg_pf_clear_impassable_nav_cells()
{
    for (int i = 0; i < Num_impassable_nav_cells; i++) {
        Impassable_nav_cells[i]->flags &= ~NAV_CELL_FLAG_BLOCKED;
    }
    Num_impassable_nav_cells = 0;
}
