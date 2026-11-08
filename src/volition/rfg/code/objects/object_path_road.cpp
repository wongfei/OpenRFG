// Original: volition/rfg/code/objects/object_path_road.cpp
//
// Roads: the zone objects and their spline data. A spline point u is index + fraction into road_spline_data::points.

#include "volition/rfg/code/objects/object_path_road.h"

#include <limits.h>
#include <math.h>
#include <string.h>

#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/rfg_zonex_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/pathfinding/road_heap.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/vmemory.h"

static const int MAX_ROAD_BREAKS = 4;
static const int MAX_ROADS = 322;
static const unsigned char BREAK_INDEX_MASK = 0x3F; // a break is index | state << 6
static const float CLOSEST_ROAD_RADIUS = 30.0f; // the search radius of the break and rejoin helpers
static const float ROAD_END_U_TOLERANCE = 0.05f;
static const float ROAD_COST_REVERSE = 1000.0f;

static object_pool<object_path_road, MAX_ROADS, 1> object_path_roadpool; // 0x03550840
static fixed_mempool<141680> Road_spline_mempool("road_splines", 1, 0); // 0x0309DA30
static prop_name PATH_ROAD_PROP_STRUCT("path_road_struct"); // 0x016AD5B4
static prop_name PATH_ROAD_PROP_DATA("path_road_data"); // 0x016AD5BC
static prop_name PATH_ROAD_PROP_FLAGS("path_road_flags"); // 0x016AD5C4

// 0x00AAA510 (:26) is a debug console command, not ported.

// 0x00A1A760 (:53)
void types_object_path_road(signed char& type, signed char& subtype)
{
    type = OT_PATH_ROAD;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A1A770 (:53)
int bpo_index_object_path_road()
{
    return 1;
}

// 0x00A1A780 (:53)
const char* classname_object_path_road()
{
    return "object_path_road";
}

// 0x00A673A0 (:53)
object* alloc_object_path_road()
{
    return object_path_roadpool.alloc(0);
}

// Magic numbers: rfg.exe's arguments, the pool's bytes and capacity.
static obj_class_instantiator object_path_road_instantiator("object_path_road", alloc_object_path_road, types_object_path_road, bpo_index_object_path_road, classname_object_path_road, object_path_road::pool_remaining, 66080, MAX_ROADS); // 0x03538990

// 0x005DDBC0
const object_type object_path_road::get_type_static()
{
    return OT_PATH_ROAD;
}

// 0x005DDBD0
const object_sub_type object_path_road::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A16090 (.h:38)
const char* object_path_road::get_classname_static()
{
    return "object_path_road";
}

// 0x005DDBE0 (.h:38): World.collect_objects<object_path_road> reads it; a zero would search the wrong broadphase group.
int object_path_road::get_bpo_index_static()
{
    return 1;
}

// 0x00A673E0 (.h:38)
const char* object_path_road::get_classname() const
{
    return "object_path_road";
}

// 0x00A1A790 (:53)
unsigned int object_path_road::get_classname_hash()
{
    return object_path_road_instantiator.this_hash;
}

// 0x00A3CF50 (:53)
int object_path_road::pool_remaining()
{
    return object_path_roadpool.get_free();
}

// 0x00A92760 (:53)
void object_path_road::pool_free()
{
    object_path_roadpool.free(this);
}

// 0x00A3CF60
road_spline_data::road_spline_data()
{
}

// 0x00A673B0 (:54)
object_path_road::object_path_road()
{
}

// 0x00A673F0
object_path_road::~object_path_road()
{
}

// 0x00AAA5A0 (:66): road_spline_data holds file offsets into the data block until they are fixed up here.
bool object_path_road::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        return true;
    }
    memset(&road_flags, 0, sizeof(road_flags));
    pblock.get_buf_copy(PATH_ROAD_PROP_STRUCT, reinterpret_cast<unsigned char*>(&spline_data), sizeof(spline_data));
    const int points_file_offset = *reinterpret_cast<const int*>(&spline_data.points);
    const int connections_file_offset = *reinterpret_cast<const int*>(&spline_data.connections);
    const int size = spline_data.num_points * sizeof(road_spline_point) + spline_data.num_connections * sizeof(road_spline_connection);
    unsigned char* data = static_cast<unsigned char*>(Road_spline_mempool.static_mempool_base::alloc(size, 0));
    pblock.get_buf_copy(PATH_ROAD_PROP_DATA, data, size);
    if (points_file_offset != -1)
    {
        spline_data.points.from_pointer(reinterpret_cast<road_spline_point*>(data + points_file_offset));
    }
    if (connections_file_offset != -1)
    {
        spline_data.connections.from_pointer(reinterpret_cast<road_spline_connection*>(data + connections_file_offset));
    }
    heap_reference = static_cast<object_path_road_heap_reference*>(Road_spline_mempool.static_mempool_base::alloc(spline_data.num_points * sizeof(object_path_road_heap_reference), 0));
    unsigned int file_flags;
    if (pblock.get_optional(PATH_ROAD_PROP_FLAGS, file_flags))
    {
        // File bits 0-5 are ambient to special, 6 is no_reinforcements and 7 is gps_only.
        road_flags.ambient = (file_flags >> 0) & 1;
        road_flags.edf_only = (file_flags >> 1) & 1;
        road_flags.marauder = (file_flags >> 2) & 1;
        road_flags.guerrilla = (file_flags >> 3) & 1;
        road_flags.one_way = (file_flags >> 4) & 1;
        road_flags.special = (file_flags >> 5) & 1;
        road_flags.no_reinforcements = (file_flags >> 6) & 1;
        road_flags.gps_only = (file_flags >> 7) & 1;
    }
    else
    {
        road_flags.ambient = 1;
    }
    road_spline_point* points = spline_data.points.to_pointer();
    for (int i = 0; i < spline_data.num_points; i++)
    {
        // Magic number from rfg.exe: the file's handles are scaled for the road's Bezier.
        points[i].pt_handle.x *= 0.707f;
        points[i].pt_handle.y *= 0.707f;
        points[i].pt_handle.z *= 0.707f;
    }
    vector bmin, bmax;
    // Original quirk kept: reads a second point even for a road with one.
    bbox_create_from_points(bmin, bmax, points[0].pos, points[1].pos);
    for (int i = 2; i < spline_data.num_points; i++)
    {
        bbox_expand(bmin, bmax, points[i].pos);
    }
    last_known_bmax = bmax;
    last_known_bmin = bmin;
    heap_session_id = 0;
    num_breaks = 0;
    return true;
}

// 0x00A1A7A0 (:193)
bool object_path_road::serialize_restore_if_missing_in_save_file(obj_prop_block& pblock)
{
    return false;
}

// 0x00A1A7B0 (:198)
bool object_path_road::serialize_restore_if_resetting_destruction(obj_prop_block& pblock)
{
    return true;
}

// 0x00A1A7C0 (:204)
void object_path_road::save_breaks(cfile* f)
{
    cf_write_uint32(get_handle(), f);
    cf_write_int8(num_breaks, f);
    for (int i = 0; i < num_breaks; i++)
    {
        cf_write_int8(breaks[i], f);
    }
}

// 0x00A67420 (:215): the cubic Bezier between two road points, their handles the control points.
void object_path_road::get_point(vector& out, float u) const
{
    const int num_points = spline_data.num_points;
    if (num_points < 2)
    {
        out = ZERO_VECTOR;
        return;
    }
    const road_spline_point* points = spline_data.points.to_pointer();
    if (u <= 0.0f)
    {
        out = points[0].pos;
        return;
    }
    if ((float)(num_points - 1) <= u)
    {
        out = points[num_points - 1].pos;
        return;
    }
    const int index = (int)u;
    const float t = u - (float)index;
    const float t2 = t * t;
    const float t3 = t2 * t;
    const float b0 = (t2 - t) * 3.0f - t3 + 1.0f;
    const float b1 = (t3 - t2 - t2 + t) * 3.0f;
    const float b2 = (t2 - t3) * 3.0f;
    const road_spline_point& p0 = points[index];
    const road_spline_point& p1 = points[index + 1];
    out.x = p0.pos.x * b0 + p1.pos.x * t3 + (p0.pt_handle.x + p0.pos.x) * b1 + (p1.pos.x - p1.pt_handle.x) * b2;
    out.y = p0.pos.y * b0 + p1.pos.y * t3 + (p0.pt_handle.y + p0.pos.y) * b1 + (p1.pos.y - p1.pt_handle.y) * b2;
    out.z = p0.pos.z * b0 + p1.pos.z * t3 + (p0.pt_handle.z + p0.pos.z) * b1 + (p1.pos.z - p1.pt_handle.z) * b2;
}

// 0x00A67850 (:266): the derivative of get_point.
void object_path_road::get_tangent(vector& out, float u) const
{
    const int num_points = spline_data.num_points;
    if (num_points < 2)
    {
        out = ZERO_VECTOR;
        return;
    }
    const road_spline_point* points = spline_data.points.to_pointer();
    if (u <= 0.0f)
    {
        out = points[0].pt_handle * 3.0f;
        return;
    }
    if ((float)(num_points - 1) <= u)
    {
        out = points[num_points - 1].pt_handle * 3.0f;
        return;
    }
    const int index = (int)u;
    const float t = u - (float)index;
    const float t2 = t * t;
    const float d0 = (t2 * -3.0f + t * 6.0f) - 3.0f;
    const float d1 = t2 * 9.0f + t * -12.0f + 3.0f;
    const float d2 = t2 * -9.0f + t * 6.0f;
    const float d3 = t2 + t2 + t2;
    const road_spline_point& p0 = points[index];
    const road_spline_point& p1 = points[index + 1];
    out.x = p0.pos.x * d0 + p1.pos.x * d3 + (p0.pt_handle.x + p0.pos.x) * d1 + (p1.pos.x - p1.pt_handle.x) * d2;
    out.y = p0.pos.y * d0 + p1.pos.y * d3 + (p0.pt_handle.y + p0.pos.y) * d1 + (p1.pos.y - p1.pt_handle.y) * d2;
    out.z = p0.pos.z * d0 + p1.pos.z * d3 + (p0.pt_handle.z + p0.pos.z) * d1 + (p1.pos.z - p1.pt_handle.z) * d2;
}

// 0x00A67D20 (:314): smoothstep between the widths of the two points.
float object_path_road::get_width(float u) const
{
    const int index = (int)u;
    if (index > spline_data.num_points - 1)
    {
        return 0.0f;
    }
    const float t = (float)((double)u - (double)(int)floor((double)u));
    const float s = t * t * 3.0f - t * t * 2.0f * t;
    const road_spline_point* points = spline_data.points.to_pointer();
    // Original bug kept: at the last point this reads the width of the point after it.
    return points[index].width * (1.0f - s) + points[index + 1].width * s;
}

// 0x00A67E50 (:342): a closest segment, 5 refinements on the curve, then pushed off any break or road block.
float object_path_road::find_closest_point_on_road(const vector& p, vector& point, bool ignore_breaks) const
{
    const road_spline_point* points = spline_data.points.to_pointer();
    float best_dist = 3.4028235e+38f;
    int best_segment = INT_MIN; // rfg.exe converts a NaN here for a road without segments
    float best_param = 0.0f;
    for (int i = 0; i < spline_data.num_points - 1; i++)
    {
        float param;
        const float dist = vec_find_closest_dist_squared_to_linesegment(param, p, points[i].pos, points[i + 1].pos);
        if (dist < best_dist)
        {
            best_param = param;
            best_dist = dist;
            best_segment = i;
        }
    }
    float u = (float)best_segment + best_param;
    const float max_u = (float)(spline_data.num_points - 1);
    float step = 0.25f;
    for (int i = 0; i < 5; i++)
    {
        float hi = step + u;
        if (hi < 0.0f)
        {
            hi = 0.0f;
        }
        if (hi >= max_u)
        {
            hi = max_u;
        }
        float lo = u - step;
        if (lo < 0.0f)
        {
            lo = 0.0f;
        }
        if (lo >= max_u)
        {
            lo = max_u;
        }
        vector hi_point, lo_point;
        get_point(hi_point, hi);
        get_point(lo_point, lo);
        float param;
        vec_find_closest_dist_squared_to_linesegment(param, p, lo_point, hi_point);
        u = (hi - lo) * param + lo;
        step *= 0.5f;
    }
    if (!ignore_breaks)
    {
        const int index = (int)floor((double)u);
        const double frac = (double)u - (double)index;
        const double to_ceil = ceil((double)u) - (double)u;
        if (num_breaks == 0)
        {
            for (int i = 0; i < num_road_blocks; i++)
            {
                if (road_blocks[i] == (unsigned char)index)
                {
                    u = to_ceil <= frac ? (float)ceil((double)u) : (float)index;
                    break;
                }
            }
        }
        else if (is_broken_at_index((unsigned char)index) == ROAD_BROKEN_COMPLETELY)
        {
            if (frac < to_ceil)
            {
                // Towards the lower point, or past it when that is broken too.
                const int i = (unsigned char)index;
                u = (float)index;
                bool moved_up = false;
                if (is_broken_at_index(i - 1) == ROAD_BROKEN_COMPLETELY && is_broken_at_index(i + 1) != ROAD_BROKEN_COMPLETELY)
                {
                    u = (float)(i + 1);
                    moved_up = true;
                }
                if (!moved_up && is_broken_at_index(i - 1) != ROAD_BROKEN_COMPLETELY && is_broken_at_index(i + 1) == ROAD_BROKEN_COMPLETELY)
                {
                    u = (float)(i - 1);
                }
                if (is_broken_at_index(i - 1) == ROAD_BROKEN_COMPLETELY && is_broken_at_index(i + 1) == ROAD_BROKEN_COMPLETELY)
                {
                    if (i - 2 >= 0)
                    {
                        u = (float)(i - 2);
                    }
                    else if (i + 2 < spline_data.num_points)
                    {
                        u = (float)(i + 2);
                    }
                }
            }
            else
            {
                u = (float)ceil((double)u);
                const int next = (int)u;
                if (is_broken_at_index((unsigned char)next) == ROAD_BROKEN_COMPLETELY)
                {
                    u = (float)((next & 0xFF) + 1);
                }
            }
        }
    }
    get_point(point, u);
    return u;
}

// 0x00A3CF70 (:439)
bool object_path_road::break_road(int index, road_broken_state state)
{
    if (state == ROAD_BROKEN_NEITHER)
    {
        return false;
    }
    const road_broken_state current = is_broken_at_index(index);
    if (current == ROAD_BROKEN_COMPLETELY || current == state)
    {
        return true;
    }
    if (num_breaks >= MAX_ROAD_BREAKS)
    {
        return false;
    }
    if (current != ROAD_BROKEN_NEITHER)
    {
        // Broken on one side already, and now on the other.
        rejoin_road(index);
        state = ROAD_BROKEN_COMPLETELY;
    }
    breaks[num_breaks] = (unsigned char)((index & BREAK_INDEX_MASK) ^ (state << 6));
    num_breaks++;
    return true;
}

// 0x00A1A820 (:467)
bool object_path_road::rejoin_road(int index)
{
    for (int i = 0; i < num_breaks; i++)
    {
        if ((breaks[i] & BREAK_INDEX_MASK) == (unsigned char)index)
        {
            for (int j = i + 1; j < num_breaks; j++)
            {
                breaks[j - 1] = breaks[j];
            }
            num_breaks--;
            return true;
        }
    }
    return false;
}

// 0x00A1A8A0 (:491)
bool object_path_road::has_breaks() const
{
    return num_breaks != 0;
}

// 0x00A1A8B0 (:496)
road_broken_state object_path_road::is_broken_at_index(int index) const
{
    for (int i = 0; i < num_breaks; i++)
    {
        if ((breaks[i] & BREAK_INDEX_MASK) == index)
        {
            return (road_broken_state)(breaks[i] >> 6);
        }
    }
    return ROAD_BROKEN_NEITHER;
}

// 0x00A68500 (:511)
bool object_path_road::break_road_at_point(const vector& p, road_broken_state state)
{
    vector closest;
    return break_road((int)floor((double)find_closest_point_on_road(p, closest, true)), state);
}

// 0x00A68550 (:519)
bool object_path_road::rejoin_road_at_point(const vector& p)
{
    vector closest;
    return rejoin_road((int)floor((double)find_closest_point_on_road(p, closest, true)));
}

// 0x00A685A0 (:527)
road_broken_state object_path_road::is_broken_at_point(const vector& p)
{
    vector closest;
    return is_broken_at_index((int)floor((double)find_closest_point_on_road(p, closest, false)));
}

// 0x00A1A900 (:535)
bool object_path_road::has_road_blocks() const
{
    return num_road_blocks != 0;
}

// 0x00A1A910 (:540)
bool object_path_road::is_road_blocked_at_index(int index) const
{
    for (int i = 0; i < num_road_blocks; i++)
    {
        if (road_blocks[i] == index)
        {
            return true;
        }
    }
    return false;
}

// 0x00A685F0 (:555)
bool object_path_road::is_road_blocked_at_point(const vector& p)
{
    vector closest;
    return is_road_blocked_at_index((int)floor((double)find_closest_point_on_road(p, closest, false)));
}

// 0x00A1A950 (:563)
bool object_path_road::road_block_road(int index)
{
    if (num_road_blocks >= MAX_ROAD_BREAKS)
    {
        return false;
    }
    if (!is_road_blocked_at_index(index))
    {
        road_blocks[num_road_blocks] = (unsigned char)index;
        num_road_blocks++;
    }
    return true;
}

// 0x00A68680 (:579)
bool object_path_road::road_block_road_at_point(const vector& p)
{
    vector closest;
    return road_block_road((int)floor((double)find_closest_point_on_road(p, closest, false)));
}

// 0x00A1A990 (:587)
bool object_path_road::remove_road_block_at_index(int index)
{
    for (int i = 0; i < num_road_blocks; i++)
    {
        if (road_blocks[i] == index)
        {
            for (int j = i + 1; j < num_road_blocks; j++)
            {
                road_blocks[j - 1] = road_blocks[j];
            }
            num_road_blocks--;
            return true;
        }
    }
    return false;
}

// 0x00A68710 (:609)
bool object_path_road::remove_road_block_at_point(const vector& p)
{
    vector closest;
    return remove_road_block_at_index((int)floor((double)find_closest_point_on_road(p, closest, false)));
}

// 0x00A68760 (:618)
const vector& object_path_road::get_road_point(unsigned short point_index) const
{
    // Past the end clamps to the last point.
    const int index = point_index < spline_data.num_points ? point_index : spline_data.num_points - 1;
    return spline_data.points[index].pos;
}

// 0x00A1AA10 (:626)
bool object_path_road::can_travel_on_road(const npc* np) const
{
    if (road_flags.special && !np->npc_flags.can_use_special_roads)
    {
        return false;
    }
    if (road_flags.edf_only && np->current_team != HUMAN_TEAM_EDF)
    {
        return false;
    }
    if (road_flags.marauder && np->current_team != HUMAN_TEAM_MARAUDER)
    {
        return false;
    }
    if (road_flags.guerrilla && np->current_team != HUMAN_TEAM_GUERILLA)
    {
        return false;
    }
    if (!road_flags.ambient && np->get_manager() == NPC_MANAGER_AMBIENT)
    {
        return false;
    }
    return true;
}

// 0x00A1AA70 (:961)
void object_path_road_level_init_pre()
{
    Road_spline_mempool.clear();
}

// 0x00A1AA80 (:966)
void object_path_road_level_init_post()
{
}

// 0x00A687B0 (:1052)
bool object_path_road_query_for_path(object_path_road* road_spline, unsigned short point, object_path_road* arrival_road_spline, const unsigned short arrival_point, float arrival_cost, const vector& goal_pos, road_heap* nhp, bool reverse_cost, bool gps_path)
{
    road_heap_data* data = NULL;
    if (road_spline->heap_reference[point].heap_session_id == nhp->session_id)
    {
        data = &nhp->heap_data[road_spline->heap_reference[point].heap_data_index];
    }
    const float estimate_scale = gps_path ? 1.0f : 1.5f;
    const vector& point_pos = road_spline->spline_data.points[point].pos;
    if (data == NULL)
    {
        float estimate = vec_dist(point_pos, goal_pos) * estimate_scale;
        if (reverse_cost)
        {
            estimate = ROAD_COST_REVERSE - estimate;
        }
        return nhp->insert_road_spline(road_spline, point, arrival_road_spline, arrival_cost, estimate, arrival_point);
    }
    if (data->is_open && arrival_cost < data->arrival_cost)
    {
        data->arrival_cost = arrival_cost;
        data->arrival_spline = arrival_road_spline != NULL ? arrival_road_spline->get_handle() : 0xFFFFFFFF;
        data->arrival_point = arrival_point;
        const float estimate = vec_dist(point_pos, goal_pos) * estimate_scale;
        data->est_cost_to_goal = reverse_cost ? ROAD_COST_REVERSE - estimate : estimate;
        nhp->adjust_road_spline(data->heap_index);
    }
    return true;
}

// 0x00A92770 (:1106)
bool object_road_break_closest_road_at_point(const vector& point)
{
    vector closest;
    float u;
    object_path_road* road = object_road_get_closest_road(point, false, closest, u, CLOSEST_ROAD_RADIUS, true, NULL, NULL, false);
    if (road == NULL)
    {
        return false;
    }
    return road->break_road((int)floor((double)u), ROAD_BROKEN_COMPLETELY);
}

// 0x00A92860 (:1120)
bool object_road_rejoin_closest_road_at_point(const vector& point)
{
    vector closest;
    float u;
    object_path_road* road = object_road_get_closest_road(point, false, closest, u, CLOSEST_ROAD_RADIUS, true, NULL, NULL, false);
    if (road == NULL)
    {
        return false;
    }
    return road->rejoin_road((int)floor((double)u));
}

// 0x00A68940 (:1150)
object_path_road* object_road_get_closest_road_list(const base_array<object_path_road*>& roads_search_set, const vector& point, vector& closest_point, float& u_value, bool ignore_breaks, const vector* fvec, const object_path_road* predisposed_road)
{
    if (roads_search_set.size() < 1)
    {
        return NULL;
    }
    object_path_road* best = NULL;
    float best_dist_sq = 3.4028235e+38f;
    vector best_closest = ZERO_VECTOR;
    float best_u = -1.0f;
    float best_dot = 0.0f;
    bool best_on_road = false;
    for (int i = 0; i < roads_search_set.size(); i++)
    {
        object_path_road* road = roads_search_set[i];
        vector bmin, bmax;
        road->get_bbox(bmin, bmax);
        if (bbox_find_xz_distance_squared(bmin, bmax, point) > best_dist_sq)
        {
            continue;
        }
        vector closest;
        const float u = road->find_closest_point_on_road(point, closest, ignore_breaks);
        const float dist_sq = vec_dist_squared(point, closest);
        vector tangent;
        road->get_tangent(tangent, u);
        float dot = 0.0f;
        if (fvec != NULL)
        {
            tangent.normalize_safe();
            dot = fabsf(fvec->y * tangent.y + fvec->x * tangent.x + fvec->z * tangent.z);
        }
        bool on_road = false;
        const float width = road->get_width(u);
        if (dist_sq <= width * width)
        {
            on_road = true;
            // At either end of the road, a point beyond the end is not on it.
            const float along = (point.y - closest.y) * tangent.y + (point.x - closest.x) * tangent.x + (point.z - closest.z) * tangent.z;
            if (fabsf(u) < ROAD_END_U_TOLERANCE)
            {
                if (along < 0.0f)
                {
                    on_road = false;
                }
            }
            else if (fabsf(u - (float)(road->spline_data.num_points - 1)) < ROAD_END_U_TOLERANCE)
            {
                if (along > 0.0f)
                {
                    on_road = false;
                }
            }
        }
        bool take;
        if (best_dist_sq <= dist_sq)
        {
            // Not closer: only an on-road candidate with something else going for it.
            take = best != NULL && on_road && ((best->road_flags.one_way && !road->road_flags.one_way) || road == predisposed_road || (fvec != NULL && best_dot < dot));
        }
        else
        {
            // Closer: unless the current best has something going for it.
            take = best == NULL || ((best->road_flags.one_way || !road->road_flags.one_way) && (predisposed_road == NULL || best != predisposed_road) && (fvec == NULL || best_dot <= dot)) || !best_on_road;
        }
        if (take)
        {
            best = road;
            best_dist_sq = dist_sq;
            best_closest = closest;
            best_u = u;
            best_dot = dot;
            best_on_road = on_road;
        }
    }
    closest_point = best_closest;
    u_value = best_u;
    return best;
}

// 0x00A92910 (:1259)
object_path_road* object_road_get_closest_road(const vector& point, bool use_special_roads, float search_radius, bool ignore_breaks, bool collect_gps)
{
    vector closest;
    float u;
    return object_road_get_closest_road(point, use_special_roads, closest, u, search_radius, ignore_breaks, NULL, NULL, collect_gps);
}

// 0x00A68E00 (:1280)
object_path_road* object_road_get_closest_road(const vector& point, bool use_special_roads, vector& closest_point, float& u_value, float search_radius, bool ignore_breaks, const vector* fvec, const object_path_road* predisposed_road, bool collect_gps)
{
    farray<object_path_road*, MAX_ROADS> candidates;
    for (int i = 0; i < World.get_num_objects<object_path_road>(); i++)
    {
        object_path_road* road = World.get_object<object_path_road>(i);
        vector bmin, bmax;
        road->get_bbox(bmin, bmax);
        if (ix_sphere_in_box(point, search_radius, bmin, bmax) && (!road->road_flags.special || use_special_roads) && (!road->road_flags.gps_only || collect_gps))
        {
            candidates.add(road);
        }
    }
    object_path_road* best = object_road_get_closest_road_list(candidates, point, closest_point, u_value, ignore_breaks, fvec, predisposed_road);
    if (best != NULL && vec_dist_squared(point, closest_point) < search_radius * search_radius)
    {
        return best;
    }
    return NULL;
}

// 0x00A68FA0 (:1326)
float object_road_get_direction(object_path_road* road, const vector& fvec, float u)
{
    if (road == NULL)
    {
        return 0.0f;
    }
    vector tangent;
    road->get_tangent(tangent, u);
    return fvec.y * tangent.y + fvec.x * tangent.x + fvec.z * tangent.z < 0.0f ? -1.0f : 1.0f;
}

// 0x00A92950 (:1340)
bool object_road_get_roads_in_zone(const vector& point, farray<object_path_road const*, 322>& road_list, bool use_special_roads)
{
    return object_road_get_roads_in_zone(rfg_zonex_get_grid_xy(point), road_list, use_special_roads);
}

// The zone is the top byte of an object's handle.
static unsigned int road_zone(const object_path_road* road)
{
    return road->get_handle() >> 24;
}

// 0x00A69050 (:1349): Original quirk kept: use_special_roads asks for special roads only.
bool object_road_get_roads_in_zone(unsigned short zone_id_xy, farray<object_path_road const*, 322>& road_list, bool use_special_roads)
{
    if (zone_id_xy > 0xFF)
    {
        return false;
    }
    road_list.clear();
    for (int i = 0; i < World.get_num_objects<object_path_road>(); i++)
    {
        const object_path_road* road = World.get_object<object_path_road>(i);
        if (use_special_roads == (road->road_flags.special != 0) && road_zone(road) == zone_id_xy)
        {
            road_list.add(road);
        }
    }
    return road_list.size() > 0;
}

// 0x00A69100 (:1376)
const object_path_road* object_road_point_on_road(const vector& pos, bool use_special_roads)
{
    vector closest;
    float u;
    object_path_road* road = object_road_get_closest_road(pos, use_special_roads, closest, u, 10.0f, false, NULL, NULL, false);
    if (road != NULL)
    {
        const float width = road->get_width(u);
        if (vec_dist_squared(pos, closest) <= width * width)
        {
            return road;
        }
    }
    return NULL;
}

// 0x00A92970 (:1403)
bool object_road_in_zone(const vector& point, bool use_special_roads)
{
    return object_road_in_zone(rfg_zonex_get_grid_xy(point), use_special_roads);
}

// 0x00A69190 (:1411)
bool object_road_in_zone(unsigned short zone_id_xy, bool use_special_roads)
{
    if (zone_id_xy > 0xFF)
    {
        return false;
    }
    for (int i = 0; i < World.get_num_objects<object_path_road>(); i++)
    {
        const object_path_road* road = World.get_object<object_path_road>(i);
        if (use_special_roads == (road->road_flags.special != 0) && road_zone(road) == zone_id_xy)
        {
            return true;
        }
    }
    return false;
}

// 0x00A69210 (:1438)
object_path_road* object_road_get_closest_road_zone(const vector& point, bool use_special_roads, vector& closest_point, float& u_value, unsigned short zone_id_xy, bool ignore_breaks, bool collect_gps)
{
    if (zone_id_xy > 0xFF)
    {
        return NULL;
    }
    farray<object_path_road*, MAX_ROADS> candidates;
    for (int i = 0; i < World.get_num_objects<object_path_road>(); i++)
    {
        object_path_road* road = World.get_object<object_path_road>(i);
        if ((use_special_roads || !road->road_flags.special) && (collect_gps || !road->road_flags.gps_only) && zone_id_xy == road_zone(road))
        {
            candidates.add(road);
        }
    }
    return object_road_get_closest_road_list(candidates, point, closest_point, u_value, ignore_breaks, NULL, NULL);
}

// 0x00A69340 (:1485)
void road_data_save(cfile* f)
{
    int count = 0;
    for (int i = 0; i < World.get_num_objects<object_path_road>(); i++)
    {
        if (World.get_object<object_path_road>(i)->has_breaks())
        {
            count++;
        }
    }
    cf_write_int32(count, f);
    for (int i = 0; i < World.get_num_objects<object_path_road>(); i++)
    {
        object_path_road* road = World.get_object<object_path_road>(i);
        if (road->has_breaks())
        {
            road->save_breaks(f);
        }
    }
}

// 0x00A3CFF0 (:1508)
void road_data_load(cfile* f)
{
    const int count = cf_read_int32(f);
    for (int i = 0; i < count; i++)
    {
        object_path_road* road = World.object_from_handle<object_path_road>(cf_read_uint32(f));
        const int num_breaks = cf_read_int8(f);
        for (int j = 0; j < num_breaks; j++)
        {
            const unsigned char saved = cf_read_uint8(f);
            if (road != NULL && (saved >> 6) != ROAD_BROKEN_NEITHER)
            {
                road->break_road(saved & BREAK_INDEX_MASK, (road_broken_state)(saved >> 6));
            }
        }
    }
}
