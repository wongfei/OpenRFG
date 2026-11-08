// Original: volition/rfg/code/traffic/traffic.cpp
//
// The traffic lane data (NPC.md NP5): a packed file of lane splines and a box tree over them, with the registry of
// vehicles on each spline. traffic_load_master has no caller in rfg.exe, so Traffic_data stays null, and only the null
// checks and the spline helpers run. Spline flag 0x80 marks a nav mesh path node, anything else is a city spline.

#include "volition/rfg/code/traffic/traffic.h"

#include "volition/render_lib/code/gr/gr.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/traffic/traffic_shared.h"
#include "volition/rfg/code/vehicle_ai/lane_heap.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/spline.h"
#include "volition/vlib/code/util/recursive_bbox.h"

#include <string.h>

static const short SPLINE_FLAG_NOT_CITY = 0x80;
static const unsigned short SPLINE_FLAG_OBSTRUCTED = 0x800;
static const short SPLINE_FLAG_INTERSECTION = 4;
static const int NUM_SPLINE_VEHICLE_REGISTRIES = 300;

// size 0x30
struct collect_query_data
{
    int lane_exclude_filter; // 0x00
    int lane_include_filter; // 0x04
    int max_collect_query_lanes; // 0x08
    vector collect_query_center; // 0x0C
    float sphere_rad_sqr; // 0x18
    int unused_1c[6];
    lane_spline** collect_query_lanes; // 0x34
    int num_collect_query_lanes; // 0x38
};

traffic_data* Traffic_data; // 0x0374BFC8
static int* Num_lane_drivers; // 0x0374BFD8
static spline_vehicles Spline_vehicle_registry[NUM_SPLINE_VEHICLE_REGISTRIES]; // 0x0374BFF0
static spline_vehicles* Free_spline_vehicle_registries; // 0x0374BFD0
static collect_query_data Collecter_callback; // 0x0374CE00

static bool Ignore_intersect; // 0x0374BFDC: never written in rfg.exe, so false
static lane_spline** Collect_query_lanes; // 0x0374BFCC
static int Lane_include_filter; // 0x0374BFD4
static int Num_collect_query_lanes; // 0x0374BFC0
static int Max_collect_query_lanes; // 0x0374BFC4

// 0x00B601F0
spline_vehicles::spline_vehicles()
{
}

// Adds a registry to the tail of the free list. Inlined at each use in rfg.exe.
static void registry_free_list_append(spline_vehicles* registry)
{
    if (Free_spline_vehicle_registries == nullptr)
    {
        registry->next.from_pointer(registry);
        registry->prev.from_pointer(registry);
        Free_spline_vehicle_registries = registry;
    }
    else
    {
        spline_vehicles* head = Free_spline_vehicle_registries;
        spline_vehicles* tail = head->prev.to_pointer();
        registry->prev.from_pointer(tail);
        registry->next.from_pointer(head);
        tail->next.from_pointer(registry);
        head->prev.from_pointer(registry);
    }
}

// 0x00B60910 (:153)
void free_spline_vehicle_registry(spline_vehicles** registry_list, spline_vehicles* registry)
{
    registry->v_handle = 0xFFFFFFFF;
    if (*registry_list == registry && registry->next.to_pointer() == registry)
    {
        *registry_list = nullptr;
    }
    else
    {
        if (*registry_list == registry)
        {
            *registry_list = registry->next.to_pointer();
        }
        spline_vehicles* prev = registry->prev.to_pointer();
        spline_vehicles* next = registry->next.to_pointer();
        if (prev != nullptr)
        {
            prev->next.from_pointer(next);
        }
        if (next != nullptr)
        {
            next->prev.from_pointer(prev);
        }
    }
    registry->next.from_pointer(nullptr);
    registry->prev.from_pointer(nullptr);
    registry_free_list_append(registry);
}

// 0x00B609B0 (:167)
void lane_spline_vehicle_registry_level_init()
{
    Free_spline_vehicle_registries = nullptr;
    for (int i = 0; i < NUM_SPLINE_VEHICLE_REGISTRIES; i++)
    {
        registry_free_list_append(&Spline_vehicle_registry[i]);
    }
}

// 0x00B60320 (:188): a leaf of the packed tree holds a lane index, made a pointer here.
static void lane_rbb_leaf_fixup_callback(void* objp)
{
    lane_spline** leaf = static_cast<lane_spline**>(objp);
    *leaf = get_traffic_lane_spline_from_index((int)(intptr_t)*leaf);
}

// 0x00B5FED0 (:194)
static bool lane_rbb_bbox_callback(void* objp, vector& _bmin, vector& _bmax)
{
    const lane_spline* lane = *static_cast<lane_spline**>(objp);
    _bmin = lane->bmin;
    _bmax = lane->bmax;
    return true;
}

// 0x00B5FF00 (:204)
static bool lane_rbb_collector_callback(void* objp)
{
    lane_spline* lane = *static_cast<lane_spline**>(objp);
    if ((Ignore_intersect && (lane->spline_flags & SPLINE_FLAG_INTERSECTION) != 0) || (Lane_include_filter & lane->spline_flags) == 0)
    {
        return true;
    }
    Collect_query_lanes[Num_collect_query_lanes] = lane;
    Num_collect_query_lanes++;
    return Num_collect_query_lanes < Max_collect_query_lanes;
}

// 0x00B60340 (:225)
static bool lane_rbb_sphere_collector_callback(void* objp)
{
    lane_spline* lane = *static_cast<lane_spline**>(objp);
    const int flags = lane->spline_flags;
    if ((flags & Collecter_callback.lane_include_filter) != Collecter_callback.lane_include_filter || (Collecter_callback.lane_exclude_filter & flags) != 0)
    {
        return true;
    }
    const vector* points = lane->approx_points.to_pointer();
    for (int i = 0; i < lane->seg_count; i++)
    {
        float t;
        const float dist = vec_find_closest_dist_squared_to_linesegment(t, Collecter_callback.collect_query_center, points[i], points[i + 1]);
        if (dist < Collecter_callback.sphere_rad_sqr)
        {
            Collecter_callback.collect_query_lanes[Collecter_callback.num_collect_query_lanes] = lane;
            Collecter_callback.num_collect_query_lanes++;
            break;
        }
    }
    return Collecter_callback.num_collect_query_lanes < Collecter_callback.max_collect_query_lanes;
}

// 0x00B603F0 (:350): LTCG, lane_query in ESI.
static void collect_lane_splines_query_sphere(traffic_lane_query& lane_query)
{
    if (Traffic_data == nullptr || Traffic_data->lane_rbb_tree.to_pointer() == nullptr)
    {
        lane_query.num_found_lane_splines = 0;
        return;
    }
    Collecter_callback.num_collect_query_lanes = 0;
    Collecter_callback.max_collect_query_lanes = lane_query.max_lane_splines;
    Collecter_callback.collect_query_lanes = lane_query.found_lanes;
    Collecter_callback.lane_exclude_filter = lane_query.exclude_flags;
    Collecter_callback.lane_include_filter = lane_query.include_flags;
    Collecter_callback.sphere_rad_sqr = lane_query.radius * lane_query.radius;
    Collecter_callback.collect_query_center = lane_query.search_center;
    Traffic_data->lane_rbb_tree.to_pointer()->query_sphere(lane_query.search_center, lane_query.radius, lane_rbb_bbox_callback, lane_rbb_sphere_collector_callback);
    lane_query.num_found_lane_splines = Collecter_callback.num_collect_query_lanes;
}

// 0x00B604A0 (:373): LTCG, max_splines in EDX, num_splines in ESI. Original: the exclude filter is dropped, unread.
static void collect_lane_splines_query_aabb(const vector& bmin, const vector& bmax, const int lane_include_filter, lane_spline** spline_list, int max_splines, int& num_splines)
{
    if (Traffic_data == nullptr || Traffic_data->lane_rbb_tree.to_pointer() == nullptr)
    {
        num_splines = 0;
        return;
    }
    Collect_query_lanes = spline_list;
    Max_collect_query_lanes = max_splines;
    Lane_include_filter = lane_include_filter;
    Num_collect_query_lanes = 0;
    Traffic_data->lane_rbb_tree.to_pointer()->query_aabb(bmin, bmax, lane_rbb_bbox_callback, lane_rbb_collector_callback);
    num_splines = Num_collect_query_lanes;
}

// 0x00B5FF50 (:416)
traffic_lane_query::traffic_lane_query(const vector& _search_center, float _radius, int _max_lane_splines, lane_spline** _found_lanes)
    : search_center(_search_center), radius(_radius), include_flags(8), exclude_flags(0x100), max_lane_splines(_max_lane_splines), num_found_lane_splines(0), found_lanes(_found_lanes)
{
}

// 0x00B60B60 (:430): counts along the single-successor chain, at most 10 splines, and stops at a loop.
int traffic_count_vehicles_on_spline(const lane_spline* lsp, bool forward)
{
    const lane_spline* visited[10];
    int num_visited = 0;
    int count = 0;
    const lane_spline* lane = lsp;
    do
    {
        for (int i = 0; i < num_visited; i++)
        {
            if (lane == visited[i])
            {
                return count;
            }
        }
        visited[num_visited++] = lane;
        const spline_vehicles* head = lane->vehicle_list.to_pointer();
        for (const spline_vehicles* entry = head; entry != nullptr;)
        {
            if (entry->v_handle != 0xFFFFFFFF)
            {
                const object* objp = World.object_from_handle(entry->v_handle);
                if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
                {
                    count++;
                }
            }
            const spline_vehicles* next = entry->next.to_pointer();
            entry = next == head ? nullptr : next;
        }
        if (forward)
        {
            if (lane->num_next_splines != 1)
            {
                return count;
            }
            lane = lane->next_splines.to_pointer()[0].to_pointer();
        }
        else
        {
            if (lane->num_previous_splines != 1)
            {
                return count;
            }
            lane = lane->previous_splines.to_pointer()[0].to_pointer();
        }
    } while (num_visited < 10);
    return count;
}

// 0x00B5FF90 (:484)
bool is_spline_city_spline(const lane_spline_base* lsp)
{
    return (lsp->spline_flags & SPLINE_FLAG_NOT_CITY) == 0;
}

// 0x00B5FFB0 (:495)
const lane_spline* get_city_spline_from_base_spline(const lane_spline_base* lsp)
{
    return is_spline_city_spline(lsp) ? static_cast<const lane_spline*>(lsp) : nullptr;
}

// 0x00B60510 (:505)
int get_traffic_lane_index(const lane_spline_base* lsp)
{
    if (lsp == nullptr || !is_spline_city_spline(lsp))
    {
        return -1;
    }
    return (int)(static_cast<const lane_spline*>(lsp) - Traffic_data->lane_splines.to_pointer());
}

// 0x00B60550 (:519)
lane_spline* get_traffic_lane_spline_from_index(int lane_spline_index)
{
    return Traffic_data->lane_splines.to_pointer() + lane_spline_index;
}

// A header offset of the file is from the file's start; makes it self-relative. -1 stays: no array.
static void fix_header_offset(int& offset, int field_offset)
{
    if (offset != -1)
    {
        offset -= field_offset;
    }
}

// An offset inside an array entry is from the start of another array; makes it self-relative. -1 stays.
template<typename T>
static void fix_array_offset(et_ptr_offset<T, 0>& field, char* base)
{
    const int offset = field.get_offset();
    if (offset != -1)
    {
        field.from_pointer(reinterpret_cast<T*>(base + offset));
    }
}

// 0x00B60C30 (:526)
void traffic_load_master(const unsigned char* data)
{
    Traffic_data = reinterpret_cast<traffic_data*>(const_cast<unsigned char*>(data));
    if (data == nullptr)
    {
        return;
    }
    traffic_data* td = Traffic_data;
    int* header = reinterpret_cast<int*>(td);
    // The array offsets at the odd dwords of the header, 0x04 to 0x5C.
    for (int field = 4; field <= 0x5C; field += 8)
    {
        fix_header_offset(header[field / 4], field);
    }
    char* lanes = reinterpret_cast<char*>(td->lane_splines.to_pointer());
    char* knots = reinterpret_cast<char*>(td->lane_spline_knots.to_pointer());
    char* in_handles = reinterpret_cast<char*>(td->lane_spline_in_handles.to_pointer());
    char* out_handles = reinterpret_cast<char*>(td->lane_spline_out_handles.to_pointer());
    char* next_ptrs = reinterpret_cast<char*>(td->lane_spline_next_ptrs.to_pointer());
    char* previous_ptrs = reinterpret_cast<char*>(td->lane_spline_previous_ptrs.to_pointer());
    char* search_tree = reinterpret_cast<char*>(td->lane_search_tree.to_pointer());
    char* tree_links = reinterpret_cast<char*>(td->lane_search_tree_links.to_pointer());
    char* stages = reinterpret_cast<char*>(td->intersection_controller_stages.to_pointer());
    char* lane_ptrs = reinterpret_cast<char*>(td->lane_spline_ptrs.to_pointer());

    for (int i = 0; i < td->num_lane_splines; i++)
    {
        lane_spline& lane = td->lane_splines.to_pointer()[i];
        fix_array_offset(lane.knots, knots);
        fix_array_offset(lane.in_handles, in_handles);
        fix_array_offset(lane.out_handles, out_handles);
        fix_array_offset(lane.next_splines, next_ptrs);
        fix_array_offset(lane.previous_splines, previous_ptrs);
        fix_array_offset(lane.right, lanes);
        fix_array_offset(lane.left, lanes);
        // Original: vehicle_list counts from the lane array too, and yield_to_lanes and approx_points are not fixed up.
        fix_array_offset(lane.vehicle_list, lanes);
    }
    for (int i = 0; i < td->num_lane_spline_next_ptrs; i++)
    {
        fix_array_offset(td->lane_spline_next_ptrs.to_pointer()[i], lanes);
    }
    for (int i = 0; i < td->num_lane_spline_previous_ptrs; i++)
    {
        fix_array_offset(td->lane_spline_previous_ptrs.to_pointer()[i], lanes);
    }
    for (int i = 0; i < td->num_intersection_controllers; i++)
    {
        intersection_controller_old& controller = td->intersection_controllers.to_pointer()[i];
        fix_array_offset(controller.stages, stages);
        for (int j = 0; j < controller.num_stages; j++)
        {
            fix_array_offset(controller.stages.to_pointer()[j].active_lanes, lane_ptrs);
        }
    }
    for (int i = 0; i < td->num_lane_spline_ptrs; i++)
    {
        fix_array_offset(td->lane_spline_ptrs.to_pointer()[i], lanes);
    }
    for (int i = 0; i < td->num_lane_search_nodes; i++)
    {
        lane_search_node& node = td->lane_search_tree.to_pointer()[i];
        fix_array_offset(node.lane, lanes);
        fix_array_offset(node.links, tree_links);
    }
    for (int i = 0; i < td->num_lane_search_tree_links; i++)
    {
        fix_array_offset(td->lane_search_tree_links.to_pointer()[i], search_tree);
    }
    Num_lane_drivers = static_cast<int*>(Level_mempool.alloc(td->num_lane_splines * 4, 0));
    // rfg.exe logs "Int3 critical function" on a failed allocation and then writes through it.
    memset(Num_lane_drivers, 0, td->num_lane_splines * 4);
    if (td->num_lane_splines < 1)
    {
        td->lane_rbb_tree.from_pointer(nullptr);
    }
    else
    {
        td->lane_rbb_tree.from_pointer(rbb_read_tree(reinterpret_cast<unsigned char*>(td), td->lane_rbb_tree_offset, lane_rbb_leaf_fixup_callback));
    }
    lane_heap_add_buffer(reinterpret_cast<lane_search_node**>(td->lane_search_work_nodes.to_pointer()), td->num_lane_search_work_nodes);
    lane_heap_city_init();
    lane_spline_vehicle_registry_level_init();
}

// 0x00B5FFD0 (:626)
void traffic_unload()
{
    Traffic_data = nullptr;
}

// 0x00B5FFE0 (:633)
bool traffic_is_loaded()
{
    return Traffic_data != nullptr;
}

// 0x00B5FFF0 (:638)
void traffic_system_init()
{
    Traffic_data = nullptr;
}

// 0x00B60000 (:645): empty in rfg.exe.
void traffic_level_init_post()
{
}

// 0x00B60570 (:652): retail strips the debug drawing, leaving the colours and the spline set up for nothing.
void traffic_render_path(lane_path_node* path)
{
    for (const lane_path_node* node = path; node != nullptr;)
    {
        const bool last = node->next == path || node->next == nullptr;
        gr_set_color(node != path && last ? 0xFF : 0x40, node == path ? 0xFF : 0x40, 0x40, 0xFF);
        node = node->next;
        if (node == path)
        {
            return;
        }
    }
}

// 0x00B60010 (:898): empty in rfg.exe.
void traffic_do_frame()
{
}

// 0x00B60020 (:908): empty in rfg.exe.
void traffic_render()
{
}

static inline cubic_bezier_spline traffic_lane_bezier(const lane_spline* lane)
{
    return cubic_bezier_spline(lane->num_knots, lane->knots.to_pointer(), lane->in_handles.to_pointer(), lane->out_handles.to_pointer());
}

// 0x00B60600 (:921)
void lane_spline_calc_pos_and_orient(const lane_spline* ls, float parametric_pos, vector* pos, matrix* orient)
{
    const cubic_bezier_spline spline = traffic_lane_bezier(ls);
    spline.get_point(*pos, parametric_pos);
    vector tangent;
    spline.get_tangent(tangent, parametric_pos);
    tangent.normalize_safe();
    orient->make_uvec(tangent, UP_VECTOR);
}

// 0x00B60690 (:940)
void traffic_lane_enter(lane_spline_base* lsp)
{
    if (is_spline_city_spline(lsp))
    {
        Num_lane_drivers[get_traffic_lane_index(lsp)]++;
    }
}

// 0x00B606D0 (:950)
void traffic_lane_exit(lane_spline* lsp)
{
    if (is_spline_city_spline(lsp))
    {
        Num_lane_drivers[get_traffic_lane_index(lsp)]--;
    }
}

// 0x00B60710 (:960)
bool traffic_lane_is_occupied(lane_spline_base* lsp)
{
    return is_spline_city_spline(lsp) && Num_lane_drivers[get_traffic_lane_index(lsp)] > 0;
}

// 0x00B60760 (:978)
void traffic_lane_get_position(vector& out, const lane_spline* lane, float spline_pos)
{
    traffic_lane_bezier(lane).get_point(out, spline_pos);
}

// 0x00B607B0 (:993)
bool traffic_lanes_exist_near_point(const vector& pos, float radius, int lane_include_filter, int lane_exclude_filter)
{
    lane_spline* found;
    traffic_lane_query query(pos, radius, 1, &found);
    query.include_flags = lane_include_filter;
    query.exclude_flags = lane_exclude_filter;
    collect_lane_splines_query_sphere(query);
    return query.num_found_lane_splines > 0;
}

// 0x00B60810 (:1012)
void traffic_get_lanes_near_point(traffic_lane_query& lane_query)
{
    collect_lane_splines_query_sphere(lane_query);
}

// 0x00B60820 (:1029)
void traffic_get_lanes_in_aabb_crude(const vector& bmin, const vector& bmax, const int lane_include_filter, const int lane_exclude_filter, lane_spline** lanes, int& num_lanes, int max_lanes)
{
    collect_lane_splines_query_aabb(bmin, bmax, lane_include_filter, lanes, max_lanes, num_lanes);
}

// 0x00B60850 (:1141)
void traffic_clear_lane_obstructed_flags()
{
    if (Traffic_data == nullptr)
    {
        return;
    }
    for (int i = 0; i < Traffic_data->num_lane_splines; i++)
    {
        lane_spline& lane = Traffic_data->lane_splines.to_pointer()[i];
        lane.spline_flags = (short)((unsigned short)lane.spline_flags & (unsigned short)~SPLINE_FLAG_OBSTRUCTED);
    }
}

// 0x00B60030 (:1162)
float get_traffic_speed_from_lane_spline(const lane_spline_base* lsbp)
{
    return is_spline_city_spline(lsbp) ? 10.0f : 1000.0f;
}
