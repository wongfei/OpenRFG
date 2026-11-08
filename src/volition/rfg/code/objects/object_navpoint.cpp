// Original: volition/rfg/code/objects/object_navpoint.cpp
//
// Nav points (stub_impl.md S9): the nodes of the patrol and ped path networks. Loading links a node to the handles in its
// link property; streaming attaches ped, road and transition nodes to the nav cell they stand in. Magic numbers below are
// the Navpoint_type_table values.

#include "volition/rfg/code/objects/object_navpoint.h"

#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/rfg_prop_name_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/pathfinding/nav_cell_detour.h"
#include "volition/rfg/code/pathfinding/nav_heap.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/hash_table.h"

static const signed char NAVPOINT_TYPE_GROUND = 0;
static const signed char NAVPOINT_TYPE_PATROL = 1;
static const signed char NAVPOINT_TYPE_PEDESTRIAN = 2;
static const signed char NAVPOINT_TYPE_FLOATING = 3;
static const signed char NAVPOINT_TYPE_ROAD = 5;
static const signed char NAVPOINT_TYPE_ROAD_JUMP = 6;
static const signed char NAVPOINT_TYPE_ROAD_JUMP_DEST = 7;
static const signed char NAVPOINT_TYPE_ROAD_JUMP_START = 8;
static const signed char NAVPOINT_TYPE_ROAD_CHECKPOINT = 9;
static const signed char NAVPOINT_TYPE_PATH = 10;
static const signed char NAVPOINT_TYPE_TRANSITION = 11;
static const signed char NAVPOINT_TYPE_LADDER_TRANSITION = 12;
static const signed char NAVPOINT_TYPE_LADDER = 13;
static const signed char NAVPOINT_TYPE_STAIRS = 14;
static const signed char NAVPOINT_TYPE_TRANSITION_JUMP = 15;
static const signed char NAVPOINT_TYPE_TRANSITION_LEAP_OVER = 16;
// A transition moved onto the nav mesh by stream(), which created a plain node on the mesh for it.
static const signed char NAVPOINT_TYPE_TRANSITION_OFF_MESH = 17;
static const signed char NAVPOINT_TYPE_ROAD_BRIDGE = 22;
static const signed char NAVPOINT_TYPE_PATH_DOOR = 23;
static const signed char NAVPOINT_TYPE_TRANSITION_DOOR = 24;

// Magic number: rfg.exe's pool.
static object_pool<navpoint,3272,1> navpointpool; // 0x03430A00
bool Navpoint_treat_all_as_enabled; // 0x0302F158
static unsigned short Navpoint_set; // 0x0302F15C
static int Dynamic_navpoint_count; // 0x0302F160
static str_data_hash_table<unsigned int, 32, 32, unsigned char> Navpoint_type_table( // 0x0306EF58
    "ground", NAVPOINT_TYPE_GROUND,
    "patrol", NAVPOINT_TYPE_PATROL,
    "pedestrian", NAVPOINT_TYPE_PEDESTRIAN,
    "floating", NAVPOINT_TYPE_FLOATING,
    "supply dropoff", 4,
    "road", NAVPOINT_TYPE_ROAD,
    "road jump", NAVPOINT_TYPE_ROAD_JUMP,
    "road jump dest", NAVPOINT_TYPE_ROAD_JUMP_DEST,
    "road jump start", NAVPOINT_TYPE_ROAD_JUMP_START,
    "road bridge", NAVPOINT_TYPE_ROAD_BRIDGE,
    "road checkpoint", NAVPOINT_TYPE_ROAD_CHECKPOINT,
    "path", NAVPOINT_TYPE_PATH,
    "transition", NAVPOINT_TYPE_TRANSITION,
    "ladder", NAVPOINT_TYPE_LADDER,
    "ladder transition", NAVPOINT_TYPE_LADDER_TRANSITION,
    "stairs", NAVPOINT_TYPE_STAIRS,
    "transition jump", NAVPOINT_TYPE_TRANSITION_JUMP,
    "transition leap over", NAVPOINT_TYPE_TRANSITION_LEAP_OVER,
    "path door", NAVPOINT_TYPE_PATH_DOOR,
    "transition door", NAVPOINT_TYPE_TRANSITION_DOOR,
    nullptr, 0);

// 0x00A1A040 (:74)
unsigned short get_next_navpoint_set_id()
{
    Navpoint_set++;
    if (Navpoint_set == 0)
    {
        Navpoint_set = 1;
    }
    return Navpoint_set;
}

// 0x00A67300 (:93)
object* alloc_navpoint()
{
    return navpointpool.alloc(0);
}

// 0x00A1A070 (:93)
void types_navpoint(signed char& type, signed char& subtype)
{
    type = OT_NAVPOINT;
    subtype = -1;
}

// 0x00A1A080 (:93)
int bpo_index_navpoint()
{
    return 2;
}

// 0x00A1A090 (:93)
const char* classname_navpoint()
{
    return "navpoint";
}

static obj_class_instantiator navpoint_instantiator("navpoint", alloc_navpoint, types_navpoint, bpo_index_navpoint, classname_navpoint, navpoint::pool_remaining, 0xCD510, 3272);

// 0x00A1A0A0 (:93)
unsigned int navpoint::get_classname_hash()
{
    return navpoint_instantiator.this_hash;
}

// 0x00A3CE90 (:93)
int navpoint::pool_remaining()
{
    return navpointpool.get_free();
}

// 0x00A921B0 (:93)
void navpoint::pool_free()
{
    navpointpool.free(this);
}

// 0x00A1A0B0 (:94)
navpoint::navpoint()
{
}

// 0x00A1A0E0
navpoint::~navpoint()
{
}

// 0x005DBAB0 (.h:83)
const object_type navpoint::get_type_static()
{
    return OT_NAVPOINT;
}

// 0x005DBAC0 (.h:83)
const object_sub_type navpoint::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A059B0 (.h:83)
const char* navpoint::get_classname_static()
{
    return "navpoint";
}

// 0x005DBAD0 (.h:83)
int navpoint::get_bpo_index_static()
{
    return 2;
}

// 0x00A1A0D0 (.h:83)
const char* navpoint::get_classname() const
{
    return "navpoint";
}

// 0x00A059C0 (.h:125)
void navpoint::set_disabled_by(navpoint_disabled_by_reason reason)
{
    heap_data.disabled_by |= (unsigned char)(1 << reason);
}

// 0x00A059E0 (.h:126)
void navpoint::clear_disabled_by(navpoint_disabled_by_reason reason)
{
    heap_data.disabled_by &= (unsigned char)~(1 << reason);
}

// 0x00AE9ED0 (.h:127)
bool navpoint::get_disabled_by(navpoint_disabled_by_reason reason) const
{
    return (heap_data.disabled_by & (unsigned char)(1 << reason)) != 0;
}

// 0x005DBAE0 (.h:128)
bool navpoint::get_disabled() const
{
    return !Navpoint_treat_all_as_enabled && heap_data.disabled_by != 0;
}

// 0x00A05A00 (.h:130)
void navpoint::set_link_disabled_by(int link_index, navpoint_disabled_by_reason reason)
{
    link_disabled_by[link_index] |= (unsigned char)(1 << reason);
}

// 0x00A05A20 (.h:131)
void navpoint::clear_link_disabled_by(int link_index, navpoint_disabled_by_reason reason)
{
    link_disabled_by[link_index] &= (unsigned char)~(1 << reason);
}

// 0x00AE9EF0 (.h:132)
bool navpoint::get_link_disabled_by(int link_index, navpoint_disabled_by_reason reason) const
{
    return (link_disabled_by[link_index] & (unsigned char)(1 << reason)) != 0;
}

// 0x005DBB00 (.h:133)
bool navpoint::get_link_disabled(int link_index) const
{
    return !Navpoint_treat_all_as_enabled && link_disabled_by[link_index] != 0;
}

// 0x005DBB30 (.h:135)
unsigned int navpoint::get_next_navpoint()
{
    return next_navpoint_internal;
}

// 0x00A05A40 (.h:137)
void navpoint::clear_next_navpoint()
{
    next_navpoint_internal = 0xFFFFFFFF;
}

// 0x00AAA020 (:106)
bool navpoint::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (load)
    {
        set = 0;
        num_links = 0;
        *(unsigned char*)&nav_flags = 0;
        World.set_object_name(this, "");
        int* combined;
        if (pblock.get_buf_optional(NAV_PROP_COMBINED, *(unsigned char**)&combined) < 1)
        {
            if (!pblock.get_optional(NAV_PROP_TYPE, type))
            {
                pblock.get_str_as_enum(NAV_PROP_TYPE_STR, type, Navpoint_type_table);
            }
            if (!pblock.get_optional(NAV_PROP_RADIUS, radius))
            {
                radius = 0.5f;
            }
            if (!pblock.get_optional(NAV_PROP_SPEED_LIMIT, speed_limit))
            {
                speed_limit = 10.0f;
            }
            nav_flags.dont_follow_road = 0;
            if (type == NAVPOINT_TYPE_PATROL)
            {
                bool value = false;
                if (pblock.get_optional(NAV_PROP_DONT_FOLLOW_ROAD, value))
                {
                    nav_flags.dont_follow_road = value;
                }
                value = false;
                if (pblock.get_optional(NAV_PROP_IGNORE_LANES, value))
                {
                    nav_flags.ignore_lanes = value;
                }
            }
        }
        else
        {
            // The combined property, as dwords: version, type, dont_follow_road, radius, speed limit, then from version 2
            // ignore_lanes and from version 3 the branch type.
            type = (signed char)combined[1];
            radius = *(float*)&combined[3];
            speed_limit = *(float*)&combined[4];
            nav_flags.dont_follow_road = 0;
            nav_flags.bridge = 0;
            nav_flags.end_branch = 0;
            nav_flags.start_branch = 0;
            nav_flags.ignore_lanes = 0;
            if (type == NAVPOINT_TYPE_PATROL)
            {
                nav_flags.dont_follow_road = (char)combined[2] != 0;
                if (combined[0] > 1)
                {
                    nav_flags.ignore_lanes = (char)combined[5] != 0;
                    if (combined[0] > 2)
                    {
                        // Magic numbers: branch type 1 start, 2 start over a bridge, 3 end.
                        if (combined[6] == 1)
                        {
                            nav_flags.start_branch = 1;
                        }
                        else if (combined[6] == 2)
                        {
                            nav_flags.start_branch = 1;
                            nav_flags.bridge = 1;
                        }
                        else if (combined[6] == 3)
                        {
                            nav_flags.end_branch = 1;
                        }
                    }
                }
            }
        }
        if (type == NAVPOINT_TYPE_PATH_DOOR)
        {
            nav_flags.door = 1;
            type = NAVPOINT_TYPE_PATH;
        }
        if (type == NAVPOINT_TYPE_TRANSITION_DOOR)
        {
            nav_flags.door = 1;
            type = NAVPOINT_TYPE_TRANSITION;
        }
        for (int i = 0; i < 10; i++)
        {
            link_disabled_by[i] = 0;
        }
        unsigned int* link_handles;
        int link_count = pblock.get_buf_optional(PROP_LINKS, *(unsigned char**)&link_handles);
        if (link_count == 0)
        {
            link_count = pblock.get_buf_optional(PROP_OLD_LINKS, *(unsigned char**)&link_handles);
        }
        if (link_count == 0)
        {
            link_count = pblock.get_buf_optional(NAV_PROP_LINKS, *(unsigned char**)&link_handles);
        }
        link_count = (unsigned int)link_count >> 2;
        nav_flags.single_node_link = link_count == 1;
        if ((type > NAVPOINT_TYPE_ROAD_CHECKPOINT || type == NAVPOINT_TYPE_GROUND || type == NAVPOINT_TYPE_PATROL || type == NAVPOINT_TYPE_PEDESTRIAN || type > NAVPOINT_TYPE_FLOATING ||
             type == NAVPOINT_TYPE_ROAD_BRIDGE) &&
            link_count != 0)
        {
            bool linked = false;
            for (int i = 0; i < link_count; i++)
            {
                if (link_handles[i] == 0xFFFFFFFF)
                {
                    continue;
                }
                navpoint* other = World.object_from_handle<navpoint>(link_handles[i]);
                if (other != nullptr)
                {
                    link_navpoints(other, this, false);
                    linked = true;
                    if (set == 0 || (other->set < set && other->set != 0))
                    {
                        set = other->set;
                    }
                }
            }
            if (linked)
            {
                propogate_set_info();
            }
            else
            {
                set = get_next_navpoint_set_id();
            }
            next_path_navpoint = nullptr;
            prev_path_navpoint = nullptr;
        }
        if (type == NAVPOINT_TYPE_ROAD_CHECKPOINT)
        {
            pblock.get_optional(NAV_PROP_ORIENT, orient);
        }
        nav_cell_ptr = nullptr;
        next_navpoint_internal = 0xFFFFFFFF;
        heap_data.arrival_cost = 0.0f;
        heap_data.est_cost_to_goal = 0.0f;
        heap_data.heap_index = 0;
        heap_data.session_id = 0;
        heap_data.flags.is_open = 0;
        if (!obj_flags.original)
        {
            Dynamic_navpoint_count++;
        }
    }
    else
    {
        int combined[7];
        ((unsigned char*)&combined[0])[0] = 3;
        ((unsigned char*)&combined[0])[1] = 0;
        ((unsigned char*)&combined[0])[2] = 0;
        ((unsigned char*)&combined[0])[3] = 0;
        combined[1] = type;
        combined[2] = nav_flags.dont_follow_road;
        *(float*)&combined[3] = radius;
        *(float*)&combined[4] = speed_limit;
        combined[5] = nav_flags.ignore_lanes;
        if (nav_flags.bridge)
        {
            combined[6] = 2;
        }
        else if (nav_flags.start_branch)
        {
            combined[6] = 1;
        }
        else
        {
            combined[6] = nav_flags.end_branch ? 3 : 0;
        }
        pblock.set_buf(NAV_PROP_COMBINED, (const unsigned char*)combined, sizeof(combined));
        unsigned int link_handles[10];
        for (int i = 0; i < num_links; i++)
        {
            link_handles[i] = links[i] != nullptr ? links[i]->get_handle() : 0xFFFFFFFF;
        }
        pblock.set_buf(PROP_LINKS, (const unsigned char*)link_handles, num_links * 4);
    }
    return true;
}

// 0x00A1A0F0 (:326)
obj_serialize_mode navpoint::serialize_mode()
{
    return obj_flags.original && obj_flags.force_full_save ? OSM_FULL : OSM_RESTORE;
}

// 0x00A1A110 (:337)
void navpoint::hierarchy_notify(hierarchy_notify_code code)
{
    if (code == HN_PARENT_DESTROYED)
    {
        World.destroy_object(this, false);
    }
}

// 0x00A67310 (:352)
void navpoint::remove_navcell_reference()
{
    if (nav_cell_ptr != nullptr)
    {
        unsigned int cell_handle = nav_cell_ptr->navp;
        if (cell_handle == get_handle())
        {
            nav_cell_ptr->navp = next_navpoint_internal;
            nav_cell_ptr = nullptr;
            return;
        }
        navpoint* prev = nullptr;
        unsigned int next = cell_handle;
        for (;;)
        {
            prev = next != 0xFFFFFFFF ? World.object_from_handle<navpoint>(next) : nullptr;
            if (prev == nullptr)
            {
                nav_cell_ptr = nullptr;
                return;
            }
            next = prev->next_navpoint_internal;
            if (next == get_handle())
            {
                break;
            }
        }
        prev->next_navpoint_internal = 0xFFFFFFFF;
        if (next_navpoint_internal != 0xFFFFFFFF)
        {
            prev->set_next_navpoint(next_navpoint_internal);
        }
    }
    nav_cell_ptr = nullptr;
}

// Inlined in navpoint::stream: appends a node to the list of nodes standing in a nav cell.
static void add_to_cell_list(nav_cell* cell, navpoint* navp)
{
    if (cell->navp == 0xFFFFFFFF)
    {
        cell->navp = navp->get_handle();
        return;
    }
    navpoint* node = World.object_from_handle<navpoint>(cell->navp);
    if (node == navp)
    {
        return;
    }
    if (node == nullptr)
    {
        cell->navp = navp->get_handle();
        return;
    }
    while (node->get_next_navpoint() != 0xFFFFFFFF)
    {
        if (node == navp)
        {
            return;
        }
        node = World.object_from_handle<navpoint>(node->get_next_navpoint());
        if (node == nullptr)
        {
            return;
        }
    }
    node->set_next_navpoint(navp->get_handle());
}

// 0x00A921C0 (:386)
bool navpoint::stream(bool load)
{
    if (!load)
    {
        if (obj_flags.destroy_on_stream)
        {
            World.destroy_object(this, false);
        }
        return true;
    }
    bool is_ped_cell_type = type == NAVPOINT_TYPE_LADDER_TRANSITION || type == NAVPOINT_TYPE_TRANSITION_LEAP_OVER || type == NAVPOINT_TYPE_TRANSITION_JUMP || type == NAVPOINT_TYPE_PEDESTRIAN;
    bool is_road_cell_type = type == NAVPOINT_TYPE_ROAD || type == NAVPOINT_TYPE_ROAD_JUMP || type == NAVPOINT_TYPE_ROAD_JUMP_DEST || type == NAVPOINT_TYPE_ROAD_JUMP_START || type == NAVPOINT_TYPE_ROAD_BRIDGE;
    if ((!is_ped_cell_type && !is_road_cell_type && type != NAVPOINT_TYPE_TRANSITION) || nav_cell_ptr != nullptr)
    {
        return true;
    }
    if (is_ped_cell_type)
    {
        nav_cell_ptr = bb_pf_get_nav_cell(pos, false, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
    }
    next_navpoint_internal = 0xFFFFFFFF;
    if (is_road_cell_type)
    {
        nav_cell_ptr = bb_pf_get_nav_cell(pos, true, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
        if (nav_cell_ptr != nullptr && type != NAVPOINT_TYPE_ROAD)
        {
            add_to_cell_list(nav_cell_ptr, this);
        }
        return true;
    }
    if (type != NAVPOINT_TYPE_TRANSITION)
    {
        return true;
    }
    vector closest = ZERO_VECTOR;
    nav_cell_ptr = bb_pf_get_nav_cell(pos, false, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
    if (nav_cell_ptr != nullptr)
    {
        add_to_cell_list(nav_cell_ptr, this);
        return true;
    }
    // A transition off the mesh: a new node goes on the mesh, 1 m beyond the closest point, linked to this one.
    nav_cell* closest_cell = nullptr;
    if (!bb_pf_find_closest_point_on_nav_mesh(pos, closest, closest_cell, false) && !bb_pf_find_closest_point_on_nav_mesh_slow(pos, closest, closest_cell))
    {
        return true;
    }
    vector to_mesh;
    to_mesh.x = closest.x - pos.x;
    to_mesh.y = 0.0f;
    to_mesh.z = closest.z - pos.z;
    to_mesh.normalize_safe();
    closest.x += to_mesh.x;
    closest.y += to_mesh.y;
    closest.z += to_mesh.z;
    vector on_cell = nav_cell_force_point_to_cell(closest_cell, closest);
    vector fvec(1.0f, 0.0f, 0.0f);
    matrix new_orient;
    new_orient.make(fvec);
    obj_nav_props props(on_cell, new_orient, NAVPOINT_TYPE_GROUND);
    navpoint* node = (navpoint*)World.create_object_internal(navpoint_instantiator.this_hash, props, nullptr, get_parent());
    if (node != nullptr)
    {
        link_navpoints(this, node, true);
        node->set = set;
        node->type = NAVPOINT_TYPE_TRANSITION;
        node->nav_cell_ptr = closest_cell;
        // Magic number: down to the ground within 100 m either way.
        raycast_input in;
        in.start = node->pos;
        in.start.y += 100.0f;
        in.end = node->pos;
        in.end.y -= 100.0f;
        in.radius = 0.0f;
        in.filter_params = nullptr;
        in.filter_callback = nullptr;
        in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
        in.m_flags = 0;
        in.m_filter_info = COLLIDE_RFG_WORLD_ONLY;
        raycast_output out;
        if (havok_raycast(in, out))
        {
            node->pos.y = out.m_hits[0].hit_pos.y;
        }
        add_to_cell_list(closest_cell, node);
        type = NAVPOINT_TYPE_TRANSITION_OFF_MESH;
    }
    return true;
}

// 0x00A1A140 (:597)
bool navpoint::serialize_restore_if_missing_in_save_file(obj_prop_block& pblock)
{
    return false;
}

// 0x00A1A150 (:602)
bool navpoint::serialize_restore_if_resetting_destruction(obj_prop_block& pblock)
{
    return true;
}

// 0x00A926F0 (:618)
void navpoint::free_this()
{
    remove_navcell_reference();
    nav_cell_detour_navpoint_removed(this);
    navpoint* old_links[10];
    int count = num_links;
    for (int i = 0; i < count; i++)
    {
        old_links[i] = links[i];
    }
    for (int i = 0; i < count; i++)
    {
        if (old_links[i] != nullptr)
        {
            remove_navpoint_link(old_links[i], this);
        }
    }
    Dynamic_navpoint_count--;
    object::free_this();
}

// 0x00A1A160 (:649)
void navpoint::get_bbox(vector& bmin, vector& bmax) const
{
    get_local_bbox(bmin, bmax);
    bmin.x += pos.x;
    bmin.y += pos.y;
    bmin.z += pos.z;
    bmax.x += pos.x;
    bmax.y += pos.y;
    bmax.z += pos.z;
}

// 0x00A1A230 (:657)
void navpoint::get_local_bbox(vector& bmin, vector& bmax) const
{
    bmin.x = -radius;
    bmin.y = -0.5f;
    bmin.z = -radius;
    bmax.x = radius;
    bmax.y = 0.5f;
    bmax.z = radius;
}

// 0x00A1A280 (:663)
void navpoint::propogate_set_info()
{
    for (int i = 0; i < num_links; i++)
    {
        if (links[i] != nullptr && links[i]->set != set)
        {
            links[i]->set = set;
            links[i]->propogate_set_info();
        }
    }
}

// 0x00A3CEC0 (:674)
void navpoint::set_next_navpoint(unsigned int navp_handle)
{
    if (next_navpoint_internal == 0xFFFFFFFF && navp_handle != 0xFFFFFFFF)
    {
        navpoint* next = World.object_from_handle<navpoint>(navp_handle);
        if (next != nullptr && next != this)
        {
            next_navpoint_internal = navp_handle;
        }
    }
}

// 0x00A1A2E0 (:705)
float navpoint_pathfinding_cost(navpoint* navp)
{
    return navp->heap_data.est_cost_to_goal + navp->heap_data.arrival_cost;
}

// 0x00A1A310 (:718)
void navpoint_query_for_path(navpoint* navp, navpoint* arrival_navpoint, float arrival_cost, const vector& goal_pos, navpoint_heap* nhp)
{
    if (!Navpoint_treat_all_as_enabled && navp->heap_data.disabled_by != 0)
    {
        return;
    }
    if (nhp->session_id != navp->heap_data.session_id)
    {
        navp->heap_data.session_id = nhp->session_id;
        if (arrival_navpoint == nullptr)
        {
            navp->heap_data.flags.is_open = 0;
            arrival_cost = 0.0f;
            navp->prev_path_navpoint = nullptr;
        }
        else
        {
            navp->heap_data.flags.is_open = 1;
            navp->prev_path_navpoint = arrival_navpoint;
        }
        navp->heap_data.arrival_cost = arrival_cost;
        navp->heap_data.est_cost_to_goal = vec_dist(navp->pos, goal_pos);
        nhp->insert_cell(navp);
        return;
    }
    if (navp->heap_data.flags.is_open && arrival_cost < navp->heap_data.arrival_cost)
    {
        navp->heap_data.arrival_cost = arrival_cost;
        navp->heap_data.est_cost_to_goal = vec_dist(navp->pos, goal_pos);
        navp->prev_path_navpoint = arrival_navpoint;
        nhp->adjust_cell(navp);
    }
}

// 0x00A3CF00 (:753)
void delete_all_navpoints_in_building(rfg_mover* mover)
{
    if (mover == nullptr)
    {
        return;
    }
    object* child = mover->child_ptr;
    while (child != nullptr)
    {
        object* next = child->child_next != mover->child_ptr ? child->child_next : nullptr;
        if (child->get_obj_type() == OT_NAVPOINT)
        {
            World.destroy_object(child, false);
        }
        child = next;
    }
}

// 0x00A1A400 (:770)
void link_navpoints(navpoint* navpoint1, navpoint* navpoint2, bool mark_as_modified)
{
    if ((navpoint1->type != NAVPOINT_TYPE_GROUND || navpoint2->type == NAVPOINT_TYPE_GROUND) && navpoint1->num_links < 10 && navpoint2->num_links < 10)
    {
        navpoint1->links[navpoint1->num_links] = navpoint2;
        navpoint1->link_disabled_by[navpoint1->num_links] = 0;
        navpoint1->num_links++;
        navpoint2->links[navpoint2->num_links] = navpoint1;
        navpoint2->link_disabled_by[navpoint2->num_links] = 0;
        navpoint2->num_links++;
        if (mark_as_modified)
        {
            navpoint1->obj_flags.force_full_save = 1;
            navpoint2->obj_flags.force_full_save = 1;
        }
    }
}

// 0x00A1A490 (:791)
static void remove_link(navpoint* navp, int index)
{
    if (index < navp->num_links)
    {
        navp->num_links--;
        navp->links[index] = navp->links[navp->num_links];
        navp->link_disabled_by[index] = navp->link_disabled_by[navp->num_links];
        navp->links[navp->num_links] = nullptr;
        navp->link_disabled_by[navp->num_links] = 0;
        navp->obj_flags.force_full_save = 1;
    }
}

static int find_link(const navpoint* navp, const navpoint* other)
{
    for (int i = 0; i < navp->num_links; i++)
    {
        if (navp->links[i] == other)
        {
            return i;
        }
    }
    return -1;
}

// 0x00A1A500 (:809)
void remove_navpoint_link(navpoint* navpoint1, navpoint* navpoint2)
{
    int index = find_link(navpoint1, navpoint2);
    if (index >= 0)
    {
        remove_link(navpoint1, index);
    }
    index = find_link(navpoint2, navpoint1);
    if (index >= 0)
    {
        remove_link(navpoint2, index);
    }
}

// 0x00A1A620 (:827)
void disable_navpoint_link(navpoint* navpoint1, navpoint* navpoint2, navpoint_disabled_by_reason reason)
{
    int index = find_link(navpoint1, navpoint2);
    if (index >= 0)
    {
        navpoint1->set_link_disabled_by(index, reason);
    }
    index = find_link(navpoint2, navpoint1);
    if (index >= 0)
    {
        navpoint2->set_link_disabled_by(index, reason);
    }
}

// 0x00A1A690 (:845)
void enable_navpoint_link(navpoint* navpoint1, navpoint* navpoint2, navpoint_disabled_by_reason reason)
{
    int index = find_link(navpoint1, navpoint2);
    if (index >= 0)
    {
        navpoint1->clear_link_disabled_by(index, reason);
    }
    index = find_link(navpoint2, navpoint1);
    if (index >= 0)
    {
        navpoint2->clear_link_disabled_by(index, reason);
    }
}

// 0x00A1A710 (:1142)
int navpoint_get_dynamic_navpoint_count()
{
    return Dynamic_navpoint_count;
}

// 0x00A1A720 (:1147)
void navpoint_level_init_pre()
{
    Navpoint_treat_all_as_enabled = false;
}

// 0x00A1A730 (:1152)
void navpoint_set_treat_all_as_enabled(bool value)
{
    Navpoint_treat_all_as_enabled = value;
}

// 0x00A517C0 (.h:152)
obj_nav_props::obj_nav_props(const vector& pos, const matrix& orient, signed char type)
    : obj_prop_block(pos, orient, nullptr)
{
    set(NAV_PROP_TYPE, type);
}

// 0x00A1A130
obj_nav_props::~obj_nav_props()
{
}
