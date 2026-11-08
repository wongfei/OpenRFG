// Original: volition/rfg/code/objects/object_guard_node.cpp
//
// Guard nodes. The reservations came first for ai_system (stub_impl.md S9), the class with its registration in stub_misc.md
// group 3, so zones now really create them.

#include "volition/rfg/code/objects/object_guard_node.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell_detour.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_cover_node.h"

#include <math.h>

static const int GUARD_NODE_RESERVATION_MS = 5000;
static const int GUARD_FACING_DEGREES_PER_INDEX = 60;
static const float GUARD_NODE_OCCUPIED_DIST_SQ = 0.25f; // 0.5 m
static object_pool<object_guard_node,512,1> object_guard_nodepool; // pool capacity from the instantiator below

// 0x005DBB50
const object_type object_guard_node::get_type_static()
{
    return OT_GUARD_NODE;
}

// 0x005DBB60
const object_sub_type object_guard_node::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x005DBB70
int object_guard_node::get_bpo_index_static()
{
    return 1;
}

// 0x005DBB80 (.h:87)
unsigned char object_guard_node::get_group_id() const
{
    return group_id;
}

// 0x005DBBA0 (.h:91)
bool object_guard_node::is_disabled() const
{
    return guard_node_flags.disabled_by_detour != 0 || guard_node_flags.disabled != 0;
}

// 0x00A3B100 (:299)
vector object_guard_node::get_facing_from_index(signed char idx) const
{
    int degrees = idx * GUARD_FACING_DEGREES_PER_INDEX;
    if (degrees >= angle_right_deg)
    {
        degrees = angle_right_deg;
    }
    if (degrees <= -(int)angle_left_deg)
    {
        degrees = -(int)angle_left_deg;
    }
    const float sine = -get_compressed_sine_int(degrees);
    const float cosine = get_compressed_cosine_int(degrees);
    vector facing;
    facing.x = orient.rvec.x * sine + orient.fvec.x * cosine;
    facing.y = orient.fvec.y * cosine + orient.rvec.y * sine;
    facing.z = orient.rvec.z * sine + orient.fvec.z * cosine;
    return facing;
}

// 0x00A3B060 (:284)
// The facing as an index of whole 60 degree steps, rounded, within the node's angles.
signed char object_guard_node::get_facing_index(const vector& cur_heading) const
{
    const vector local = cur_heading.copy_rotate(orient);
    // Magic number: degrees per radian, as a float constant.
    const int degrees = (int)((double)(float)atan2((double)-local.x, (double)local.z) * (double)57.29578f);
    if (degrees < 0)
    {
        const int left = -degrees < (int)angle_left_deg ? -degrees : (int)angle_left_deg;
        return (signed char)-((left + GUARD_FACING_DEGREES_PER_INDEX / 2) / GUARD_FACING_DEGREES_PER_INDEX);
    }
    const int right = degrees < (int)angle_right_deg ? degrees : (int)angle_right_deg;
    return (signed char)((right + GUARD_FACING_DEGREES_PER_INDEX / 2) / GUARD_FACING_DEGREES_PER_INDEX);
}

// 0x00A63960 (:442)
// Disables the guard nodes inside a destroyed box, grown by 4 m (BBOX_EXPAND_SIZE, 0x035B59D0, set on first use).
void guard_node_process_destruction_event(const rfg_destruction_event* eventp)
{
    if (eventp == nullptr)
    {
        return;
    }
    static const vector BBOX_EXPAND_SIZE(4.0f, 4.0f, 4.0f);
    vector local_min;
    local_min.x = eventp->bmin.x - BBOX_EXPAND_SIZE.x;
    local_min.y = eventp->bmin.y - BBOX_EXPAND_SIZE.y;
    local_min.z = eventp->bmin.z - BBOX_EXPAND_SIZE.z;
    vector local_max;
    local_max.x = BBOX_EXPAND_SIZE.x + eventp->bmax.x;
    local_max.y = eventp->bmax.y + BBOX_EXPAND_SIZE.y;
    local_max.z = eventp->bmax.z + BBOX_EXPAND_SIZE.z;
    vector world_min;
    vector world_max;
    bbox_instance(world_min, world_max, eventp->orient, eventp->pos, local_min, local_max);
    const int num_nodes = World.get_num_objects<object_guard_node>();
    for (int i = 0; i < num_nodes; i++)
    {
        object_guard_node* node = World.get_object<object_guard_node>(i);
        if (ix_point_in_box(node->pos, world_min, world_max))
        {
            node->disable_node();
        }
    }
}

// 0x00A18070 (:320)
bool object_guard_node::is_reserved(unsigned int npc_handle) const
{
    return guard_node_flags.disabled == 0 && npc_handle != 0xFFFFFFFF && npc_handle == reserved_handle && !reservation_timer.elapsed();
}

// 0x00A904A0 (:336)
bool object_guard_node::can_reserve(unsigned int npc_handle) const
{
    if (is_disabled() || npc_handle == 0xFFFFFFFF)
    {
        return false;
    }
    object* objp = World.object_from_handle(npc_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return false;
    }
    if (static_cast<npc*>(objp)->get_ai()->outside_defensive_perimeter(pos, false, nullptr))
    {
        return false;
    }
    if (is_reserved(npc_handle))
    {
        return true;
    }
    if (reserved_handle != 0xFFFFFFFF && !reservation_timer.elapsed())
    {
        return false;
    }
    return !is_occupied(npc_handle);
}

// 0x00A63880 (:375)
bool object_guard_node::is_occupied(unsigned int requesting_handle) const
{
    if (requesting_handle == 0xFFFFFFFF)
    {
        return false;
    }
    object* objp = World.object_from_handle(requesting_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return false;
    }
    if (vec_dist_squared(pos, objp->pos) < GUARD_NODE_OCCUPIED_DIST_SQ)
    {
        return false; // the requester itself is on the node
    }
    const human* requester = static_cast<const human*>(objp);
    const int count = World.get_num_objects<npc>();
    for (int i = 0; i < count; i++)
    {
        const npc* other = World.get_object<npc>(i);
        if (other != requester && !other->is_killed() && !human_teams_are_unfriendly(other, requester) && vec_dist_squared(pos, other->pos) < GUARD_NODE_OCCUPIED_DIST_SQ)
        {
            return true;
        }
    }
    return false;
}

// 0x00A180B0 (:415)
void object_guard_node::reserve_node(unsigned int npc_handle)
{
    if (guard_node_flags.disabled == 0)
    {
        reserved_handle = npc_handle;
        reservation_timer.set(GUARD_NODE_RESERVATION_MS);
    }
}

// 0x00A180E0 (:426)
void object_guard_node::release_node(unsigned int npc_handle)
{
    // Original: another npc's handle, or an expired or disabled reservation, is what releases it; the owner of a
    // live reservation does not (the test reads inverted, kept).
    if (guard_node_flags.disabled == 0 && npc_handle != 0xFFFFFFFF && npc_handle == reserved_handle && !reservation_timer.elapsed())
    {
        return;
    }
    reserved_handle = 0xFFFFFFFF;
    reservation_timer.set(0);
}

// 0x00A18030 (:274): from whole 60 degree steps to the left, rounded to the nearest.
signed char object_guard_node::get_min_facing_index() const
{
    return (signed char)(-(angle_left_deg + 30) / 60);
}

// 0x00A18050 (:279)
signed char object_guard_node::get_max_facing_index() const
{
    return (signed char)((angle_right_deg + 30) / 60);
}

// 0x00A3B240 (:310)
npc* object_guard_node::get_reserving_npc() const
{
    if (guard_node_flags.disabled == 0 && !reservation_timer.elapsed())
    {
        return World.object_from_handle<npc>(reserved_handle);
    }
    return nullptr;
}

// 0x00A17FD0 (:264)
float object_guard_node::get_left_angle_dot() const
{
    return (float)cos((double)((float)angle_left_deg * 0.017453f)); // degrees to radians
}

// 0x00A17E10 (:38)
void types_object_guard_node(signed char& type, signed char& subtype)
{
    type = OT_GUARD_NODE;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A17E20 (:38)
int bpo_index_object_guard_node()
{
    return 1;
}

// 0x00A17E30 (:38)
const char* classname_object_guard_node()
{
    return "object_guard_node";
}

// 0x00A63870 (:38)
object* alloc_object_guard_node()
{
    return object_guard_nodepool.alloc(0);
}

// Magic numbers: rfg.exe's arguments (0x010774F0), the pool's bytes and capacity.
static obj_class_instantiator object_guard_node_instantiator("object_guard_node", alloc_object_guard_node, types_object_guard_node, bpo_index_object_guard_node, classname_object_guard_node, object_guard_node::pool_remaining, 0x15A40, 0x200);

// 0x00A17E40 (:38)
unsigned int object_guard_node::get_classname_hash()
{
    return object_guard_node_instantiator.this_hash;
}

// 0x00A3B030 (:38)
int object_guard_node::pool_remaining()
{
    return object_guard_nodepool.get_free();
}

// 0x00A901D0 (:38)
void object_guard_node::pool_free()
{
    object_guard_nodepool.free(this);
}

// 0x00A17E50 (:39)
object_guard_node::object_guard_node()
{
}

// 0x00A17E90
object_guard_node::~object_guard_node()
{
}

// 0x00A17DF0 (.h:26)
const char* object_guard_node::get_classname_static()
{
    return "object_guard_node";
}

// 0x00A17E80 (.h:26)
const char* object_guard_node::get_classname() const
{
    return "object_guard_node";
}

// 0x00AA7060 (:45)
bool object_guard_node::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (load)
    {
        angle_left_deg = 0;
        angle_right_deg = 0;
        group_id = 0xFF;
        reserved_handle = 0xFFFFFFFF;
        reservation_timer.set(0);
        memset(&guard_node_flags, 0, sizeof(guard_node_flags));
        // Deviation: rfg.exe reads an uninitialized stack slot when a property is missing or has another size; here the
        // value stays as just set.
        int group = group_id;
        pblock.get(GUARD_NODE_PROP_GROUP_ID, group);
        group_id = (unsigned char)group;
        float left = 0.0f;
        pblock.get(GUARD_NODE_PROP_ANGLE_LEFT, left);
        angle_left_deg = (unsigned char)(int)left;
        float right = 0.0f;
        pblock.get(GUARD_NODE_PROP_ANGLE_RIGHT, right);
        angle_right_deg = (unsigned char)(int)right;
        bool no_defensive_combat = false;
        pblock.get(GUARD_NODE_PROP_NO_DEFENSIVE_COMBAT, no_defensive_combat);
        guard_node_flags.no_defensive_combat = no_defensive_combat;
        // Magic number: 180 degrees to a side at most.
        if (angle_left_deg >= 180)
        {
            angle_left_deg = 180;
        }
        if (angle_right_deg >= 180)
        {
            angle_right_deg = 180;
        }
        if (!obj_flags.original)
        {
            obj_flags.no_save = 1;
        }
    }
    else
    {
        pblock.set(GUARD_NODE_PROP_GROUP_ID, (int)group_id);
        pblock.set(GUARD_NODE_PROP_ANGLE_LEFT, (float)angle_left_deg);
        pblock.set(GUARD_NODE_PROP_ANGLE_RIGHT, (float)angle_right_deg);
        const bool no_defensive_combat = guard_node_flags.no_defensive_combat != 0;
        pblock.set(GUARD_NODE_PROP_NO_DEFENSIVE_COMBAT, no_defensive_combat);
    }
    return true;
}

// 0x00A901E0 (:133)
// On load the node works out what is around it: inside its parent's box with a roof over the point 5 m ahead (interior),
// next to a transition navpoint of its parent, on the ground floor (within 1.5 m of the ground).
bool object_guard_node::stream(bool load)
{
    if (!load)
    {
        if (obj_flags.destroy_on_stream)
        {
            World.destroy_object(this, false);
        }
        nav_cell_detour_guard_node_removed(this);
        return true;
    }
    guard_node_flags.interior = 0;
    guard_node_flags.near_trans = 0;
    guard_node_flags.ground_floor = 0;
    guard_node_flags.important = 0;
    object* parent_objp = World.object_from_handle(get_parent());
    if (parent_objp != nullptr)
    {
        const float ahead_x = pos.x + orient.fvec.x * 5.0f;
        const float ahead_y = pos.y + orient.fvec.y * 5.0f;
        const float ahead_z = pos.z + orient.fvec.z * 5.0f;
        vector parent_min;
        vector parent_max;
        parent_objp->get_local_bbox(parent_min, parent_max);
        if (ix_point_in_box_oriented(pos, parent_objp->pos, parent_objp->orient, parent_min, parent_max))
        {
            raycast_input input;
            input.start.x = ahead_x;
            input.start.y = (float)((double)ahead_y + 0.5);
            input.start.z = ahead_z;
            input.end.x = ahead_x;
            input.end.y = (float)((double)ahead_y + 5.0);
            input.end.z = ahead_z;
            input.radius = 0.0f;
            input.filter_params = nullptr;
            input.filter_callback = nullptr;
            input.m_filter_info = COLLIDE_RAY_DEFAULT;
            input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
            input.m_flags = 0;
            raycast_output output;
            if (havok_raycast(input, output))
            {
                guard_node_flags.interior = 1;
            }
        }
        for (const object* child = parent_objp->child_ptr; child != nullptr; child = child->child_next)
        {
            // Magic numbers: navpoint type 11, as rfg.exe compares it; 2 m (4 squared).
            if (child->get_obj_type() == OT_NAVPOINT && static_cast<const navpoint*>(child)->type == 11 && vec_dist_squared(pos, child->pos) <= 4.0f)
            {
                guard_node_flags.near_trans = 1;
                break;
            }
            if (child->child_next == parent_objp->child_ptr)
            {
                break; // the children are a ring
            }
        }
    }
    vector ground = pos;
    if (rfg_snap_to_ground(ground))
    {
        // Magic number: 1.5 m.
        guard_node_flags.ground_floor = pos.y - ground.y < 1.5f;
    }
    guard_node_flags.important = !guard_node_flags.ground_floor || guard_node_flags.near_trans;
    return true;
}

// 0x00A17EA0 (:220)
void object_guard_node::get_bbox(vector& bmin, vector& bmax) const
{
    get_local_bbox(bmin, bmax);
    bmin.x += pos.x;
    bmin.y += pos.y;
    bmin.z += pos.z;
    bmax.x += pos.x;
    bmax.y += pos.y;
    bmax.z += pos.z;
}

// 0x00A17F70 (:230): Magic number: a 1 m cube.
void object_guard_node::get_local_bbox(vector& bmin, vector& bmax) const
{
    bmin.x = -0.5f;
    bmin.y = -0.5f;
    bmin.z = -0.5f;
    bmax.x = 0.5f;
    bmax.y = 0.5f;
    bmax.z = 0.5f;
}

// 0x00A17FB0 (:239)
void object_guard_node::hierarchy_notify(hierarchy_notify_code code)
{
    if (code == HN_PARENT_DESTROYED)
    {
        World.destroy_object(this, false);
    }
}

// 0x00A90490 (:254): object::free_this, inlined in rfg.exe.
void object_guard_node::free_this()
{
    object::free_this();
}

// 0x00A18000 (:269)
float object_guard_node::get_right_angle_dot() const
{
    return (float)cos((double)((float)angle_right_deg * 0.017453f)); // degrees to radians
}
