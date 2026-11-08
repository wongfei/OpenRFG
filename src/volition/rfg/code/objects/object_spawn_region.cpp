// Original: volition/rfg/code/objects/object_spawn_region.cpp
//
// gameflow.md GF2: the reinforcement code walks these, and a type stand-in would have it walk humans instead. A box where
// squads of a team (or any team) may spawn. Registers the class, so zones now create these objects.

#include "volition/rfg/code/objects/object_spawn_region.h"

#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/vector3.h"

static object_pool<object_spawn_region, 32, 1> object_spawn_regionpool; // 0x03063BD0

// 0x00A1D0B0 (:46)
void types_object_spawn_region(signed char& type, signed char& subtype)
{
    type = OT_SPAWN_REGION;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A1D0C0 (:46)
int bpo_index_object_spawn_region()
{
    return 1;
}

// 0x00A1D0D0 (:46)
const char* classname_object_spawn_region()
{
    return "object_spawn_region";
}

// 0x00A6DD60 (:46)
object* alloc_object_spawn_region()
{
    return object_spawn_regionpool.alloc(0);
}

// Magic numbers: rfg.exe's arguments (0x0107C350), the pool's bytes and capacity.
static obj_class_instantiator object_spawn_region_instantiator("object_spawn_region", alloc_object_spawn_region, types_object_spawn_region, bpo_index_object_spawn_region, classname_object_spawn_region, object_spawn_region::pool_remaining, 0x1E60, 0x20);

// 0x00A1D0E0 (:46)
unsigned int object_spawn_region::get_classname_hash()
{
    return object_spawn_region_instantiator.this_hash;
}

// 0x00A3ED70 (:46)
int object_spawn_region::pool_remaining()
{
    return object_spawn_regionpool.get_free();
}

// 0x00A946D0 (:46)
void object_spawn_region::pool_free()
{
    object_spawn_regionpool.free(this);
}

// 0x00A1D0F0 (:47)
object_spawn_region::object_spawn_region()
{
}

// 0x00A1D120
object_spawn_region::~object_spawn_region()
{
}

// 0x00754090 (object_spawn_region.h:29)
const object_type object_spawn_region::get_type_static()
{
    return OT_SPAWN_REGION;
}

// 0x007540A0 (object_spawn_region.h:29)
const object_sub_type object_spawn_region::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A0D210 (object_spawn_region.h:29)
const char* object_spawn_region::get_classname_static()
{
    return "object_spawn_region";
}

// 0x00A0D220 (object_spawn_region.h:29)
int object_spawn_region::get_bpo_index_static()
{
    return 1;
}

// 0x00A1D110 (object_spawn_region.h:29)
const char* object_spawn_region::get_classname() const
{
    return "object_spawn_region";
}

// 0x00AACB60 (:54)
bool object_spawn_region::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set(SPAWN_REGION_PROP_BOX, local_bb);
        return true;
    }
    region_flags = {};
    local_bb.min = ZERO_VECTOR;
    local_bb.max = ZERO_VECTOR;
    bb = local_bb;
    near_bb = local_bb;
    team = HUMAN_TEAM_NONE;
    // Magic number: only objects of a zone (handle bits 16 to 23 at 0x80 or above) load their region.
    if (((get_handle() >> 16) & 0xFF) < 0x80)
    {
        return false;
    }
    pblock.get(SPAWN_REGION_PROP_BOX, local_bb);
    bool value = false;
    if (pblock.get_optional<bool>(SPAWN_REGION_NPC_ONLY, value))
    {
        region_flags.npc_only = value;
    }
    if (pblock.get_optional<bool>(SPAWN_REGION_VEHICLE_ONLY, value))
    {
        region_flags.vehicle_only = value;
    }
    // Both at once means neither.
    if (region_flags.npc_only && region_flags.vehicle_only)
    {
        region_flags.npc_only = false;
        region_flags.vehicle_only = false;
    }
    char team_name[256];
    if (pblock.get_optional(SPAWN_REGION_TEAM_ALLOWED, team_name))
    {
        team = human_team_from_name(team_name);
    }
    // A flat box is given a depth of 1 m. rfg.exe compares min to max, not the other way round, so an inverted box stays.
    if (local_bb.max.x <= local_bb.min.x)
    {
        local_bb.min.x = local_bb.max.x + 1.0f;
    }
    if (local_bb.max.y <= local_bb.min.y)
    {
        local_bb.min.y = local_bb.max.y + 1.0f;
    }
    if (local_bb.max.z <= local_bb.min.z)
    {
        local_bb.min.z = local_bb.max.z + 1.0f;
    }
    bbox_instance(bb, orient, pos, local_bb);
    // Original quirk kept: the near box subtracts each corner from itself (SUBSD XMM0,XMM0), so it is the position alone.
    bbox near_local;
    near_local.min = vector(local_bb.min.x - local_bb.min.x, local_bb.min.y - local_bb.min.y, local_bb.min.z - local_bb.min.z);
    near_local.max = vector(local_bb.max.x - local_bb.max.x, local_bb.max.y - local_bb.max.y, local_bb.max.z - local_bb.max.z);
    bbox_instance(near_bb, orient, pos, near_local);
    return true;
}

// 0x00A1D130 (:139)
void object_spawn_region::get_bbox(vector& bmin, vector& bmax) const
{
    bmin = bb.min;
    bmax = bb.max;
}

// 0x00A1D170 (:145)
void object_spawn_region::get_local_bbox(vector& bmin, vector& bmax) const
{
    bmin = local_bb.min;
    bmax = local_bb.max;
}

// 0x00A1D1B0 (:152)
void object_spawn_region::hierarchy_notify(hierarchy_notify_code code)
{
    if (code == HN_PARENT_DESTROYED)
    {
        World.destroy_object(this, false);
    }
}

// 0x00A0D230 (object_spawn_region.h:53)
human_teams object_spawn_region::get_team() const
{
    return team;
}

// 0x00A3EDA0 (:167)
bool object_spawn_region::team_allowed_to_spawn(human_teams other_team, bool is_vehicle_spawn) const
{
    if (is_disabled() || (team != other_team && team != HUMAN_TEAM_NONE))
    {
        return false;
    }
    if (is_vehicle_spawn)
    {
        return region_flags.vehicle_only || !region_flags.npc_only;
    }
    return region_flags.npc_only || !region_flags.vehicle_only;
}

// 0x00A3EE20 (:187)
bool object_spawn_region::is_inside(const vector& point) const
{
    return !is_disabled() && ix_point_in_box(point, bb.min, bb.max) && ix_point_in_box_oriented(point, pos, orient, local_bb.min, local_bb.max);
}

// 0x00A3EEB0 (:201)
bool object_spawn_region::is_near_by(const vector& point) const
{
    return !is_disabled() && ix_point_in_box(point, near_bb.min, near_bb.max);
}

// 0x00A1D1D0 (:215)
bool object_spawn_region::get_spawn_position(float x_dist, float z_dist, vector& spawn_pos) const
{
    const bool x_inside = !(x_dist < local_bb.min.x || local_bb.max.x < x_dist);
    const bool z_inside = !(z_dist < local_bb.min.z || local_bb.max.z < z_dist);
    spawn_pos.x = pos.x + orient.rvec.x * x_dist + orient.fvec.x * z_dist;
    spawn_pos.y = pos.y + orient.rvec.y * x_dist + orient.fvec.y * z_dist;
    spawn_pos.z = pos.z + orient.rvec.z * x_dist + orient.fvec.z * z_dist;
    return x_inside && z_inside;
}

// 0x00A1D360 (:229)
bool object_spawn_region::is_disabled() const
{
    if (region_flags.disabled)
    {
        return true;
    }
    const unsigned int parent_handle = get_parent();
    if (parent_handle == 0xFFFFFFFF)
    {
        return false;
    }
    const object* parent_objp = World.object_from_handle(parent_handle);
    return parent_objp == nullptr || parent_objp->is_killed();
}

// 0x00A6DD70 (:254)
bool object_spawn_region::get_spawn_list(float offset_size, farray<vector, 128>& spawn_point_list) const
{
    if (!(0.0f < offset_size) || is_disabled())
    {
        return false;
    }
    spawn_point_list.clear();
    const int num_x = (int)((local_bb.max.x - local_bb.min.x) / offset_size) + 1;
    const int num_z = (int)((local_bb.max.z - local_bb.min.z) / offset_size) + 1;
    for (int x = 0; x < num_x; x++)
    {
        for (int z = 0; z < num_z; z++)
        {
            const float z_dist = (float)z * offset_size + local_bb.min.z;
            const float x_dist = local_bb.min.x + (float)x * offset_size;
            spawn_point_list.add(vector(orient.rvec.x * x_dist + pos.x + z_dist * orient.fvec.x, orient.rvec.y * x_dist + pos.y + orient.fvec.y * z_dist, pos.z + orient.rvec.z * x_dist + orient.fvec.z * z_dist));
            if (spawn_point_list.capacity() <= spawn_point_list.size())
            {
                return true;
            }
        }
    }
    return spawn_point_list.size() > 0;
}

// 0x00B1FD00 (object_spawn_region.h:71)
void object_spawn_region::disable()
{
    region_flags.disabled = true;
}

// 0x00B1FD10 (object_spawn_region.h:74)
void object_spawn_region::set_continuous_attack_region(bool enabled)
{
    region_flags.cont_attack_region = enabled;
}

// 0x007540B0 (object_spawn_region.h:78)
void object_spawn_region::set_vrail_spawn_flag(bool spawn_flag)
{
    region_flags.vrail_spawn_region = spawn_flag;
}

// 0x007540D0 (object_spawn_region.h:79)
bool object_spawn_region::get_vrail_spawn_flag() const
{
    return region_flags.vrail_spawn_region;
}

// 0x00A6E040 (:307)
const object_spawn_region* spawn_region_find_region(human_teams team, bool vehicle_spawn, const vector& point, float min_dist_sqrd)
{
    const object_spawn_region* best = nullptr;
    float best_dist = 3.4028235e+38f;
    const int num = World.get_num_objects<object_spawn_region>();
    for (int i = 0; i < num; i++)
    {
        const object_spawn_region* region = World.get_object<object_spawn_region>(i);
        if (!region->team_allowed_to_spawn(team, vehicle_spawn))
        {
            continue;
        }
        const float dist = vec_dist_squared(point, region->pos);
        if (min_dist_sqrd <= dist && dist < best_dist)
        {
            best = region;
            best_dist = dist;
        }
    }
    return best;
}

// 0x00A6E150 (:334)
const object_spawn_region* spawn_region_find_near_region(human_teams team, bool vehicle_spawn, const vector& point)
{
    const int num = World.get_num_objects<object_spawn_region>();
    for (int i = 0; i < num; i++)
    {
        const object_spawn_region* region = World.get_object<object_spawn_region>(i);
        if (region->team_allowed_to_spawn(team, vehicle_spawn) && region->is_near_by(point))
        {
            return region;
        }
    }
    return nullptr;
}
