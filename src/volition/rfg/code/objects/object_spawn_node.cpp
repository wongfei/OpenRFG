// Original: volition/rfg/code/objects/object_spawn_node.cpp
//
// Spawn nodes (stub_impl.md S9, NPC.md NP4): placed objects that spawn a vehicle, an NPC, a squad or a turret from a spawn
// group when the player is near and streamed in.

#include "volition/rfg/code/objects/object_spawn_node.h"

#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/objects/turret_info.h"

#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/squad_progression.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/objects/object_debris.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/spawn/spawn_request.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/object_pool.inl"

#include <string.h>

#include "volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator.h"
#include "volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator_internal.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/gameflow/squad_progression_internal.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/safehouse_vip.h"
#include "volition/rfg/code/gameplay/squad_spawn.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/object_navpoint.h"

extern gfm_orders_manager Orders_manager; // 0x0222DE40 (gameplay.cpp)

// Magic numbers, squared: a vehicle node evaluates within 250 m of the player (50 m on an immediate spawn of the player's
// vehicle), an NPC node within 200 m (100 m for hostages); 6 m around the player's last vehicle stays clear; a child leaves
// its node beyond 4 m; a spawned object is found again within 10 m.
static const float VEHICLE_NODE_RANGE_SQUARED = 62500.0f;
static const float VEHICLE_NODE_IMMEDIATE_RANGE_SQUARED = 2500.0f;
static const float NPC_NODE_RANGE_SQUARED = 40000.0f;
static const float HOSTAGE_NODE_RANGE_SQUARED = 10000.0f;
static const float LAST_VEHICLE_CLEAR_RANGE_SQUARED = 36.0f;
static const float CHILD_ABANDON_RANGE_SQUARED = 16.0f;
static const float SPAWNED_OBJECT_RANGE_SQUARED = 100.0f;
// Magic numbers, squared: an object stays while the player is within 100 m, or when more than 20 m from its node.
static const float DESPAWN_PLAYER_RANGE_SQUARED = 10000.0f;
static const float DESPAWN_NODE_RANGE_SQUARED = 400.0f;

// Magic number: rfg.exe's pool.
static const int VEHICLE_SPAWN_NODE_POOL_SIZE = 128;
static const int VEHICLE_SPAWN_NODE_POOL_USAGE = 0x5AC0;

// ---- file-static globals
union object_spawn_node_flags
{
    struct
    {
        unsigned int spawn_next_frame_vehicle : 1; // 0x00:0
        unsigned int spawn_next_frame_npc : 1; // 0x00:1
        unsigned int spawn_next_frame_squad : 1; // 0x00:2
        unsigned int spawn_next_frame_turret : 1; // 0x00:3
    };
    unsigned int all;
};
static object_spawn_node_flags Spawn_node_flags; // 0x03024E84
static bool Team_squads_disabled[NUM_HUMAN_TEAMS]; // 0x03024E90
static bool Immediate_spawn_flag; // 0x0302F15E
static bool Spawn_squad_disabled; // 0x0302F15F
static gfm_alert_levels Last_recorded_alert_level; // 0x0302F20C
static const vehicle_info* Player_spawn_vehicle; // 0x0302F210
static timestamp Spawn_retry_period; // 0x016AE0D0
static game_clock_time_of_day Last_spawn_tod; // 0x016946FC
bool Allow_walker_spawn = true; // 0x01694700

// Whether the handle's zone byte marks an object of no zone (a handle's upper half is its zone, inferred).
static bool handle_in_high_zone(unsigned int handle)
{
    return ((handle >> 16) & 0xFF) > 0x7F;
}

// 0x00A6CA60 (:277)
static object_vehicle_spawn_node* get_nearest_player_vehicle_spawn_node()
{
    if (Player == nullptr)
    {
        return nullptr;
    }
    object_vehicle_spawn_node* nearest = nullptr;
    float nearest_distance = 0.0f;
    const int count = World.get_num_objects<object_vehicle_spawn_node>();
    for (int i = 0; i < count; i++)
    {
        object_vehicle_spawn_node* node = World.get_object<object_vehicle_spawn_node>(i);
        if (!node->is_player_vehicle_respawn())
        {
            continue;
        }
        const float distance = vec_dist_squared(Player->pos, node->pos);
        if (nearest == nullptr || distance < nearest_distance)
        {
            nearest = node;
            nearest_distance = distance;
        }
    }
    return nearest;
}

// 0x00A6CB20 (:307)
unsigned int object_spawn_node_get_player_vehicle_handle()
{
    object_vehicle_spawn_node* node = get_nearest_player_vehicle_spawn_node();
    return node != nullptr ? node->get_child_vehicle_handle() : 0xFFFFFFFF;
}

// 0x00A6CB40 (:320)
static bool is_nearest_player_vehicle_spawn_node(const object_vehicle_spawn_node* node)
{
    return get_nearest_player_vehicle_spawn_node() == node;
}

// 0x00A3E590 (:326)
// Original: reads Player unchecked.
static bool near_last_player_vehicle(const vector& pos)
{
    if (Player->last_vehicle_driven == 0xFFFFFFFF)
    {
        return false;
    }
    object* objp = World.object_from_handle(Player->last_vehicle_driven);
    return objp != nullptr && objp->get_obj_type() == OT_VEHICLE && vec_dist_squared(pos, objp->pos) < LAST_VEHICLE_CLEAR_RANGE_SQUARED;
}

// 0x00A1C5C0 (:345)
// Whether objp and all its ancestors are streamed in.
static bool parents_streamed_in(const object* objp)
{
    if (objp == nullptr)
    {
        return false;
    }
    while (objp->obj_flags.streamed)
    {
        objp = World.object_from_handle(objp->get_parent());
        if (objp == nullptr)
        {
            return true;
        }
    }
    return false;
}

// 0x00A1C5F0 (:364)
static bool team_squad_spawn_disabled(human_teams team)
{
    return (unsigned int)team < NUM_HUMAN_TEAMS && Team_squads_disabled[team];
}

// 0x00A1C670 (:391)
static bool use_daytime_spawning()
{
    return game_clock_get_time_of_day() != GC_NIGHT;
}

// 0x00A1C6E0 (:416)
// Magic number: the length of "Hostages".
static bool is_hostage_group(const char* group_name)
{
    return group_name != nullptr && v_strnicmp(group_name, "Hostages", 8) == 0;
}

// 0x00A1C700 (:518)
// LTCG: spawned_obj_handle in EAX.
static bool object_should_be_destroyed(const vector& object_pos, unsigned int spawned_obj_handle, float despawn_dist_sqrd)
{
    object* objp = World.object_from_handle(spawned_obj_handle);
    if (objp == nullptr || objp->is_killed())
    {
        return false;
    }
    if (Player == nullptr)
    {
        return true;
    }
    if (vec_dist_squared(Player->pos, objp->pos) < DESPAWN_PLAYER_RANGE_SQUARED)
    {
        return false;
    }
    return vec_dist_squared(object_pos, objp->pos) < despawn_dist_sqrd;
}

// 0x00A1C790 (:686)
static bool get_stay_in_vehicle_flag(gfm_orders_types orders_type, const object_squad_spawn_node* ossnp)
{
    return orders_type == GFM_ORDERS_TYPE_ROADBLOCK;
}

// 0x00A1C7A0 (:743)
static bool obj_specific_should_not_clear(const item* objp)
{
    return objp->get_parent() != 0xFFFFFFFF;
}

// 0x00A1C7B0 (:753)
static bool obj_specific_should_not_clear(const vehicle* objp)
{
    return vehicle_get_num_passengers(objp->get_handle()) > 0;
}

// 0x00A2ABB0 (:737)
static bool obj_specific_should_not_clear(const object_debris* objp)
{
    return false;
}

// 0x00A2ABC0 (:737)
static bool obj_specific_should_not_clear(const object_effect* objp)
{
    return false;
}

// 0x00A79430 (:764)
// LTCG: pos in ECX, orient in EDX. What is in the node's box goes, but for a vehicle with passengers.
static void clear_vehicle_obstructions(const vector& bmin, const vector& bmax, const vector& pos, const matrix& orient)
{
    // Magic number: rfg.exe's stack list.
    unsigned int handles[24];
    const int count = World.collect_objects<vehicle>(bmin, bmax, pos, orient, handles, 24, nullptr, nullptr);
    for (int i = 0; i < count; i++)
    {
        object* objp = handles[i] != 0xFFFFFFFF ? World.object_from_handle(handles[i]) : nullptr;
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && !obj_specific_should_not_clear((const vehicle*)objp))
        {
            World.destroy_object(handles[i], false);
        }
    }
}

// 0x00A794D0 (:764)
static void clear_item_obstructions(const vector& bmin, const vector& bmax, const vector& pos, const matrix& orient)
{
    unsigned int handles[24];
    const int count = World.collect_objects<item>(bmin, bmax, pos, orient, handles, 24, nullptr, nullptr);
    for (int i = 0; i < count; i++)
    {
        object* objp = handles[i] != 0xFFFFFFFF ? World.object_from_handle(handles[i]) : nullptr;
        if (objp != nullptr && objp->get_obj_type() == OT_ITEM && !obj_specific_should_not_clear((const item*)objp))
        {
            World.destroy_object(handles[i], false);
        }
    }
}

// 0x00A79560 (:764)
static void clear_debris_obstructions(const vector& bmin, const vector& bmax, const vector& pos, const matrix& orient)
{
    unsigned int handles[24];
    const int count = World.collect_objects<object_debris>(bmin, bmax, pos, orient, handles, 24, nullptr, nullptr);
    for (int i = 0; i < count; i++)
    {
        object* objp = handles[i] != 0xFFFFFFFF ? World.object_from_handle(handles[i]) : nullptr;
        if (objp != nullptr && objp->get_obj_type() == OT_DEBRIS && !obj_specific_should_not_clear((const object_debris*)objp))
        {
            World.destroy_object(handles[i], false);
        }
    }
}

// 0x00A795F0 (:764)
static void clear_effect_obstructions(const vector& bmin, const vector& bmax, const vector& pos, const matrix& orient)
{
    unsigned int handles[24];
    const int count = World.collect_objects<object_effect>(bmin, bmax, pos, orient, handles, 24, nullptr, nullptr);
    for (int i = 0; i < count; i++)
    {
        object* objp = handles[i] != 0xFFFFFFFF ? World.object_from_handle(handles[i]) : nullptr;
        if (objp != nullptr && objp->get_obj_type() == OT_EFFECT && !obj_specific_should_not_clear((const object_effect*)objp))
        {
            World.destroy_object(handles[i], false);
        }
    }
}

// 0x00A793B0 (:785)
// Original: only the first vehicle of this node is looked at.
static unsigned int find_spawned_vehicle(unsigned int spawn_node_handle, const vector& node_pos)
{
    const int count = World.get_num_objects<vehicle>();
    for (int i = 0; i < count; i++)
    {
        vehicle* vp = World.get_object<vehicle>(i);
        if (vp->spawn_node_handle != spawn_node_handle)
        {
            continue;
        }
        if (!vp->is_killed() && vec_dist_squared(node_pos, vp->pos) < SPAWNED_OBJECT_RANGE_SQUARED)
        {
            return vp->get_handle();
        }
        return 0xFFFFFFFF;
    }
    return 0xFFFFFFFF;
}



static object_pool<object_vehicle_spawn_node, VEHICLE_SPAWN_NODE_POOL_SIZE, 1> object_vehicle_spawn_nodepool; // 0x0319A6B0 (0x0107BEB0)

// 0x00A6CE20 (:909)
object* alloc_object_vehicle_spawn_node()
{
    return object_vehicle_spawn_nodepool.alloc(0);
}

// 0x00A1C7E0 (:909)
void types_object_vehicle_spawn_node(signed char& type, signed char& subtype)
{
    type = OT_VEHICLE_SPAWN_NODE;
    subtype = -1;
}

// 0x00A1C7F0 (:909)
int bpo_index_object_vehicle_spawn_node()
{
    return 1;
}

// 0x00A1C800 (:909)
const char* classname_object_vehicle_spawn_node()
{
    return "object_vehicle_spawn_node";
}

static obj_class_instantiator object_vehicle_spawn_node_instantiator("object_vehicle_spawn_node", alloc_object_vehicle_spawn_node, types_object_vehicle_spawn_node, bpo_index_object_vehicle_spawn_node, classname_object_vehicle_spawn_node, object_vehicle_spawn_node::pool_remaining, VEHICLE_SPAWN_NODE_POOL_USAGE, VEHICLE_SPAWN_NODE_POOL_SIZE); // 0x03537E7C (0x0107BED0)

// 0x00A1C810 (:909)
unsigned int object_vehicle_spawn_node::get_classname_hash()
{
    return object_vehicle_spawn_node_instantiator.this_hash;
}

// 0x00A3E5F0 (:909)
int object_vehicle_spawn_node::pool_remaining()
{
    return object_vehicle_spawn_nodepool.get_free();
}

// 0x00A94100 (:909)
void object_vehicle_spawn_node::pool_free()
{
    object_vehicle_spawn_nodepool.free(this);
}

// 0x00A1C820 (:910)
object_vehicle_spawn_node::object_vehicle_spawn_node()
{
}

// 0x00A1C850
object_vehicle_spawn_node::~object_vehicle_spawn_node()
{
}

// 0x005DF900 (.h:61)
const object_type object_vehicle_spawn_node::get_type_static()
{
    return OT_VEHICLE_SPAWN_NODE;
}

// 0x005DF910 (.h:61)
const object_sub_type object_vehicle_spawn_node::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A0CFF0 (.h:61)
const char* object_vehicle_spawn_node::get_classname_static()
{
    return "object_vehicle_spawn_node";
}

// 0x00A0D000 (.h:61)
int object_vehicle_spawn_node::get_bpo_index_static()
{
    return 1;
}

// 0x00A1C840 (.h:61)
const char* object_vehicle_spawn_node::get_classname() const
{
    return "object_vehicle_spawn_node";
}

// Reads a one byte property (rfg_prop_block::get_internal of type 5), false when it is missing.
static bool read_flag_property(obj_prop_block& pblock, const prop_name& name)
{
    bool value = false;
    return pblock.get_optional<bool>(name, value) && value;
}

static float clamp_probability(float value)
{
    if (value - 0.0f < 0.0f)
    {
        value = 0.0f;
    }
    if (0.0f <= value - 1.0f)
    {
        value = 1.0f;
    }
    return value;
}

// 0x00AABB30 (:915)
bool object_vehicle_spawn_node::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        return true;
    }
    memset(&node_flags, 0, sizeof(node_flags));
    v_spawn_group = nullptr;
    spawn_prob = 1.0f;
    night_spawn_prob = 1.0f;
    child_vehicle_handle = find_spawned_vehicle(get_handle(), pos);
    char set_name[256];
    set_name[0] = '\0';
    pblock.get(SPAWN_NODE_PROP_SET_NAME, set_name);
    node_flags.special_spawn_node = read_flag_property(pblock, SPAWN_NODE_PROP_SPECIAL_VEHICLE);
    v_spawn_group = sog_get_vehicle_group(set_name);
    if (v_spawn_group == nullptr)
    {
        return false;
    }
    if (Multiplayer)
    {
        srm_reserve_vehicle_spawn_group_mp(v_spawn_group);
    }
    pblock.get_optional<float>(SPAWN_NODE_PROP_SPAWN_PROB, spawn_prob);
    spawn_prob = clamp_probability(spawn_prob);
    if (!pblock.get_optional<float>(SPAWN_NODE_PROP_NIGHT_SPAWN_PROB, night_spawn_prob))
    {
        night_spawn_prob = spawn_prob;
    }
    night_spawn_prob = clamp_probability(night_spawn_prob);
    // Original: leaves the flag as it was (clear) when the property is missing.
    bool no_parking_value = false;
    if (pblock.get_optional<bool>(SPAWN_NODE_PROP_NO_PARKING, no_parking_value))
    {
        node_flags.no_parking = no_parking_value;
    }
    object* parent_objp = World.object_from_handle(get_parent());
    node_flags.ge_node = parent_objp != nullptr && parent_objp->get_obj_type() == OT_DELIVERY_NODE;
    node_flags.player_vehicle_respawn = read_flag_property(pblock, SPAWN_NODE_PROP_RESPAWN);
    for (int i = 0; i < v_spawn_group->v_info_list.size(); i++)
    {
        if (v_spawn_group->v_info_list[i]->vehicle_class == VCL_WALKER)
        {
            node_flags.walker_spawn = 1;
            break;
        }
    }
    return true;
}

// 0x00A1C860 (:1005)
bool object_vehicle_spawn_node::in_spawn_range(const player* pp) const
{
    const float distance = vec_dist_squared(pp->pos, pos);
    return distance < (Immediate_spawn_flag && node_flags.player_vehicle_respawn ? VEHICLE_NODE_IMMEDIATE_RANGE_SQUARED : VEHICLE_NODE_RANGE_SQUARED);
}

// 0x00A1C8D0 (:1025)
void object_vehicle_spawn_node::do_respawn()
{
}

// 0x00A94110 (:1031)
void object_vehicle_spawn_node::evaluate_child_relation()
{
    if (!allow_ambient_action())
    {
        return;
    }
    if (child_vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(child_vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            evaluate_abandon_child((vehicle*)objp);
            return;
        }
    }
    evaluate_adopt_child();
}

// 0x00A1C8E0 (:1054)
void object_vehicle_spawn_node::evaluate_abandon_child(vehicle* vp)
{
    if (CHILD_ABANDON_RANGE_SQUARED <= vec_dist_squared(vp->pos, pos))
    {
        child_vehicle_handle = 0xFFFFFFFF;
        vp->spawn_node_handle = 0xFFFFFFFF;
    }
}

// 0x00A6CE30 (:1070)
// A free, empty vehicle of the node's group parked on the node becomes its child.
void object_vehicle_spawn_node::evaluate_adopt_child()
{
    const int count = World.get_num_objects<vehicle>();
    for (int i = 0; i < count; i++)
    {
        vehicle* vp = World.get_object<vehicle>(i);
        if (vp->is_killed() || vp->spawn_node_handle != 0xFFFFFFFF || vehicle_get_num_passengers(vp) >= 1)
        {
            continue;
        }
        if (v_spawn_group->v_info_list.find(vp->info, false) > -1 && vec_dist_squared(vp->pos, pos) <= CHILD_ABANDON_RANGE_SQUARED)
        {
            child_vehicle_handle = vp->get_handle();
            vp->spawn_node_handle = get_handle();
            return;
        }
    }
}

// 0x00A1C920 (:1116)
void object_vehicle_spawn_node::force_abandon_child(vehicle* vp)
{
    if (vp != nullptr)
    {
        child_vehicle_handle = 0xFFFFFFFF;
        vp->spawn_node_handle = 0xFFFFFFFF;
    }
}

// 0x00A3E620 (:1128)
void object_vehicle_spawn_node::evaluate_spawn()
{
    node_flags.evaluated_spawn = 1;
    if (child_vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(child_vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            return;
        }
        child_vehicle_handle = 0xFFFFFFFF;
    }
    if (Multiplayer && !multi_is_server())
    {
        return;
    }
    const float probability = use_daytime_spawning() ? spawn_prob : night_spawn_prob;
    if (fl_rand() <= probability)
    {
        node_flags.spawn_next_frame = 1;
        Spawn_node_flags.spawn_next_frame_vehicle = 1;
    }
}

// 0x00A1C940 (:1157)
void object_vehicle_spawn_node::destroy_notify()
{
    object::destroy_notify();
}

// 0x00A3E6B0 (:1167)
bool object_vehicle_spawn_node::stream(bool load)
{
    if (load)
    {
        if (!Multiplayer)
        {
            evaluate_spawn();
        }
        else if (multi_is_server())
        {
            node_flags.spawn_next_frame = 1;
            Spawn_node_flags.spawn_next_frame_vehicle = 1;
        }
        return true;
    }
    if (obj_flags.destroy_on_stream)
    {
        World.destroy_object(this, false);
    }
    if (!Multiplayer && object_should_be_destroyed(pos, child_vehicle_handle, DESPAWN_NODE_RANGE_SQUARED))
    {
        object* objp = child_vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(child_vehicle_handle) : nullptr;
        if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE || !vehicle_should_avoid_despawning((vehicle*)objp))
        {
            World.destroy_object(child_vehicle_handle, false);
            child_vehicle_handle = 0xFFFFFFFF;
        }
    }
    node_flags.evaluated_spawn = 0;
    return true;
}

// 0x00A1C950 (:1214)
void object_vehicle_spawn_node::hierarchy_notify(hierarchy_notify_code code)
{
    if (code == HN_PARENT_DESTROYED)
    {
        World.destroy_object(this, false);
    }
}

// 0x00A94190 (:1227)
void object_vehicle_spawn_node::free_this()
{
    object::free_this();
}

// 0x00A1C970 (:1233)
const unsigned int object_vehicle_spawn_node::get_mp_child() const
{
    return Multiplayer ? child_vehicle_handle : 0xFFFFFFFF;
}

// 0x00A1C990 (:1239)
void object_vehicle_spawn_node::set_mp_child(unsigned int mp_child)
{
    if (Multiplayer)
    {
        child_vehicle_handle = mp_child;
    }
}

// 0x00A941A0 (:1245)
bool object_vehicle_spawn_node::spawn_mp_child()
{
    if (!Multiplayer || !multi_is_server())
    {
        return false;
    }
    vehicle_spawn_params params;
    if (child_vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(child_vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && !objp->is_killed())
        {
            return false;
        }
    }
    if (!params.pick_vehicle_info(v_spawn_group))
    {
        return false;
    }
    params.spawn_pos = pos;
    params.spawn_orient = orient;
    params.veh_spawn_flags.no_save = 1;
    params.veh_spawn_flags.one_attempt = 1;
    params.parent_handle = get_handle();
    params.building_handle = get_parent();
    params.veh_spawn_flags.interior_spawn = World.object_from_handle(params.building_handle) != nullptr;
    params.veh_spawn_flags.force_to_ground = 1;
    if (object_spawn_vehicle(params) == SPAWN_STATUS_SUCCESS)
    {
        if (params.vp != nullptr)
        {
            child_vehicle_handle = params.vp->get_handle();
            return true;
        }
        return false;
    }
    vector bmin;
    vector bmax;
    get_local_bbox(bmin, bmax);
    clear_vehicle_obstructions(bmin, bmax, pos, orient);
    clear_item_obstructions(bmin, bmax, pos, orient);
    clear_debris_obstructions(bmin, bmax, pos, orient);
    clear_effect_obstructions(bmin, bmax, pos, orient);
    return false;
}

// 0x00A3E790 (:1302)
bool object_vehicle_spawn_node::allow_ambient_action() const
{
    if (node_flags.disabled || node_flags.mission_disabled || node_flags.no_parking || node_flags.ge_node || handle_in_high_zone(get_handle()))
    {
        return false;
    }
    const unsigned int parent_handle = get_parent();
    if (parent_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(parent_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_SAFEHOUSE)
        {
            return false;
        }
    }
    return true;
}

// 0x00A1C9B0 (:1328)
float object_vehicle_spawn_node::get_spawn_prob() const
{
    return game_clock_get_time_of_day() != GC_NIGHT ? spawn_prob : night_spawn_prob;
}

// 0x00A1C9D0 (:1339)
bool object_vehicle_spawn_node::can_spawn_child() const
{
    if (this == nullptr || !obj_flags.streamed)
    {
        return false;
    }
    object* objp = World.object_from_handle(get_parent());
    return objp == nullptr || parents_streamed_in(objp);
}

// 0x00A6CF20 (:1347)
unsigned int object_vehicle_spawn_node::spawn_child_object(unsigned int parent_override)
{
    vehicle_spawn_params params;
    node_flags.spawn_next_frame = 0;
    if (node_flags.disabled || node_flags.mission_disabled || obj_flags.destroyed)
    {
        return child_vehicle_handle;
    }
    const bool high_zone = handle_in_high_zone(get_handle());
    // A walker node in the world waits for walkers to be allowed; in either case the player's last vehicle keeps its place.
    if (!high_zone && ((node_flags.walker_spawn && !Allow_walker_spawn) || near_last_player_vehicle(pos)))
    {
        return child_vehicle_handle;
    }
    if (child_vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(child_vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && !objp->is_killed() && !objp->obj_flags.destroyed)
        {
            return child_vehicle_handle;
        }
        child_vehicle_handle = 0xFFFFFFFF;
    }
    const unsigned int parent_handle = get_parent();
    object* parent_objp = nullptr;
    if (parent_handle != 0xFFFFFFFF)
    {
        parent_objp = World.object_from_handle(parent_handle);
        if (parent_objp == nullptr || parent_objp->is_killed())
        {
            return child_vehicle_handle;
        }
    }
    squad_progression_member_vehicle_info(v_spawn_group->get_team(), v_spawn_group);
    srm_allocate_ambient_spawn_slots_vehicle();
    if (node_flags.player_vehicle_respawn)
    {
        if (!Immediate_spawn_flag || Player_spawn_vehicle == nullptr || !is_nearest_player_vehicle_spawn_node(this))
        {
            return 0xFFFFFFFF;
        }
        params.veh_info = Player_spawn_vehicle;
    }
    else if (node_flags.special_spawn_node)
    {
        if (!params.pick_special_vehicle_info(v_spawn_group))
        {
            return child_vehicle_handle;
        }
    }
    else if (!params.pick_vehicle_info(v_spawn_group))
    {
        return child_vehicle_handle;
    }
    params.spawn_pos = pos;
    params.spawn_orient = orient;
    params.building_handle = parent_override != 0xFFFFFFFF ? parent_override : parent_handle;
    params.veh_spawn_flags.no_save = 1;
    params.veh_spawn_flags.one_attempt = 1;
    params.veh_spawn_flags.interior_spawn = parent_objp != nullptr;
    params.veh_spawn_flags.high_priority_clean_up = high_zone;
    params.veh_spawn_flags.force_to_ground = 1;
    params.veh_spawn_flags.static_spawn_placement = 1;
    params.vflags.immediate_spawn = Immediate_spawn_flag;
    const spawn_status_result result = object_spawn_vehicle(params);
    if (result == SPAWN_STATUS_FAILURE)
    {
        return child_vehicle_handle;
    }
    if (result == SPAWN_STATUS_STREAMING)
    {
        node_flags.spawn_next_frame = 1;
        return child_vehicle_handle;
    }
    if (params.vp != nullptr)
    {
        child_vehicle_handle = params.vp->get_handle();
        params.vp->obj_flags.no_save = 1;
        params.vp->obj_flags.destroy_on_stream = 1;
        params.vp->spawn_node_handle = get_handle();
    }
    if (Immediate_spawn_flag && node_flags.player_vehicle_respawn && Player_spawn_vehicle != nullptr && is_nearest_player_vehicle_spawn_node(this) && params.vp != nullptr)
    {
        if (Player != nullptr)
        {
            Player->last_vehicle_driven = child_vehicle_handle;
        }
        Player_spawn_vehicle = nullptr;
    }
    return child_vehicle_handle;
}

// 0x005DF920 (.h:86)
const vehicle_spawn_group_info* object_vehicle_spawn_node::get_info() const
{
    return v_spawn_group;
}

// 0x00754F00 (.h:113)
bool object_vehicle_spawn_node::should_spawn() const
{
    return node_flags.spawn_next_frame != 0;
}

// 0x00A0D010 (.h:116)
bool object_vehicle_spawn_node::should_evaluate_spawn() const
{
    return node_flags.evaluated_spawn == 0;
}

// 0x00B42EF0 (.h:119)
bool object_vehicle_spawn_node::use_special_slot_vehicle() const
{
    return node_flags.special_spawn_node != 0;
}

// 0x00A0D020 (.h:125)
void object_vehicle_spawn_node::reevaluate_spawn()
{
    node_flags.evaluated_spawn = 0;
}

// 0x00A0D030 (.h:128)
bool object_vehicle_spawn_node::walker_spawn_node() const
{
    return node_flags.walker_spawn != 0;
}

// 0x00754F10 (.h:139)
void object_vehicle_spawn_node::disable_spawn()
{
    node_flags.disabled = 1;
}

// 0x00754F20 (.h:140)
void object_vehicle_spawn_node::enable_spawn()
{
    node_flags.disabled = 0;
}

// 0x00B1FCA0 (.h:142)
void object_vehicle_spawn_node::disable_mission_spawn()
{
    node_flags.mission_disabled = 1;
}

// 0x00A0D040 (.h:143)
void object_vehicle_spawn_node::enable_mission_spawn()
{
    node_flags.mission_disabled = 0;
}

// 0x00754F30 (.h:146)
bool object_vehicle_spawn_node::is_disabled() const
{
    return node_flags.disabled || node_flags.mission_disabled;
}

// 0x00A0D050 (.h:150)
bool object_vehicle_spawn_node::is_player_vehicle_respawn() const
{
    return node_flags.player_vehicle_respawn != 0;
}

// The NPC and squad nodes' members the resource manager reads (the classes join with the NPC pass).
// A zero stand-in here would make World.get_object<object_squad> walk the humans.
// 0x005DF940 (.h:193)
const object_type object_npc_spawn_node::get_type_static()
{
    return OT_NPC_SPAWN_NODE;
}

// 0x005DF950 (.h:193)
const object_sub_type object_npc_spawn_node::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00754F50 (.h:218)
const human_spawn_group_info* object_npc_spawn_node::get_info() const
{
    return h_spawn_group;
}

// 0x005DF960 (.h:317)
const object_type object_squad_spawn_node::get_type_static()
{
    return OT_SQUAD_SPAWN_NODE;
}

// 0x005DF970 (.h:317)
const object_sub_type object_squad_spawn_node::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00754F70 (.h:347)
const squad_definition* object_squad_spawn_node::get_info() const
{
    return squad_def;
}

// 0x00754F80 (.h:348)
const vehicle_spawn_group_info* object_squad_spawn_node::get_veh_info() const
{
    return veh_spawn_group;
}

// 0x00A79940 (:831)
static void do_vehicle_node_spawning()
{
    const int count = World.get_num_objects<object_vehicle_spawn_node>();
    for (int i = 0; i < count; i++)
    {
        object_vehicle_spawn_node* node = World.get_object<object_vehicle_spawn_node>(i);
        if (node->should_spawn() && node->obj_flags.streamed)
        {
            object* parent_objp = World.object_from_handle(node->get_parent());
            if (parent_objp == nullptr || parents_streamed_in(parent_objp))
            {
                node->spawn_child_object(0xFFFFFFFF);
            }
        }
    }
}

// 0x00A4E2C0 (:843)
static void evaluate_vehicle_node_spawning(const player* pp)
{
    const int count = World.get_num_objects<object_vehicle_spawn_node>();
    for (int i = 0; i < count; i++)
    {
        object_vehicle_spawn_node* node = World.get_object<object_vehicle_spawn_node>(i);
        if (!node->in_spawn_range(pp))
        {
            node->reevaluate_spawn();
        }
        else if (node->should_evaluate_spawn() || Immediate_spawn_flag)
        {
            node->evaluate_spawn();
        }
    }
}

// 0x00A4E470 (:3005)
static bool check_vehicle_node_spawning_flags()
{
    const int count = World.get_num_objects<object_vehicle_spawn_node>();
    for (int i = 0; i < count; i++)
    {
        if (World.get_object<object_vehicle_spawn_node>(i)->should_spawn())
        {
            return true;
        }
    }
    return false;
}

// 0x00A6DC70 (:3024)
void object_spawn_node_level_init_post()
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return;
    }
    Last_recorded_alert_level = ALERT_LEVEL_GREEN;
    Immediate_spawn_flag = false;
    if (Player_spawn_vehicle == nullptr)
    {
        object_vehicle_spawn_node* node = get_nearest_player_vehicle_spawn_node();
        if (node != nullptr && node->get_info() != nullptr)
        {
            Player_spawn_vehicle = node->get_info()->get_random_element();
        }
    }
    if (Player_spawn_vehicle != nullptr)
    {
        srm_reserve_slot(SPAWN_SLOT_VEHICLE_PLAYER, RESOURCE_SLOT_USER_ID_NONE, Player_spawn_vehicle);
    }
    srm_do_frame_special_slot();
    if (Player != nullptr)
    {
        const vehicle_spawn_group_info* player_group = sog_default_vehicle_spawn_group(Player->current_team);
        const vehicle_spawn_group_info* edf_group = sog_default_vehicle_spawn_group(HUMAN_TEAM_EDF);
        if (player_group != nullptr)
        {
            squad_progression_member_vehicle_info(Player->current_team, player_group);
        }
        if (edf_group != nullptr)
        {
            squad_progression_member_vehicle_info(HUMAN_TEAM_EDF, edf_group);
        }
    }
}

// 0x00A1CF90 (:3080)
void object_spawn_node_level_init()
{
    Spawn_node_flags.all = 0xFFFFFFFF;
    // Magic number: a second.
    Spawn_retry_period.set(1000);
    Last_spawn_tod = game_clock_get_time_of_day();
    Spawn_squad_disabled = false;
    Allow_walker_spawn = true;
    for (int i = 0; i < NUM_HUMAN_TEAMS; i++)
    {
        Team_squads_disabled[i] = false;
    }
}

// 0x00AB46D0 (:3105)
void object_spawn_do_spawn()
{
    Immediate_spawn_flag = true;
    Spawn_retry_period.set(0);
    object_spawn_node_do_frame();
    Immediate_spawn_flag = false;
    squad_do_immediate_spawns();
    for (int i = 0; i < World.get_num_objects<vehicle>(); i++)
    {
        vehicle_fade_in(World.get_object<vehicle>(i), 0);
    }
    Player_spawn_vehicle = nullptr;
}

// 0x00A1CFD0 (:3138)
void object_spawn_set_player_vehicle_info(const vehicle_info* player_vehicle_info)
{
    Player_spawn_vehicle = player_vehicle_info;
}

// ---- NPC and squad nodes (stub_impl.md S9). The turret node stays unregistered: no turret class yet.
static const float DEFAULT_SPAWN_PROB_MAX = 1.0f;
// Magic numbers: rfg.exe's pools.
static object_pool<object_npc_spawn_node,64,1> object_npc_spawn_nodepool; // 0x03060E20
static object_pool<object_squad_spawn_node,256,1> object_squad_spawn_nodepool; // 0x033C67E0
static const char* ORDERS_NAMES[NUM_GFM_ORDERS_TYPES] = { // 0x016946B0
    "Default", "Defend", "Attack", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "Fodder", "Terrorize", "Wait", "Roadblock", "Trapped", "Trapped Jail", "Drive By", "Fly By", "Drop-off", "Tied Up"
};
static const char* RESPAWN_TAGS[4] = { "None", "Slow", "Medium", "Fast" }; // 0x016946A0
// Magic numbers: never, 5 min, 2 min, 30 s.
static const int RESPAWN_DELAYS_MS[4] = { 0x7FFFFFFF, 300000, 120000, 30000 }; // 0x01311AF4
static const gfm_orders_flags DEFAULT_ORDERS_FLAGS(true, false); // 0x02270D39
static const gfm_orders_flags FODDER_ORDERS_FLAGS(false, true); // 0x02231B23
static bool Destream_orders_removal = true; // 0x01694613

static float clamp_spawn_prob(float prob)
{
    return prob < 0.0f ? 0.0f : (prob > DEFAULT_SPAWN_PROB_MAX ? DEFAULT_SPAWN_PROB_MAX : prob);
}

// 0x00A1C560 (:207)
static bool check_roadblock_parenting(const object* objp)
{
    while (objp != nullptr)
    {
        // Magic number: object type 0x1E is the roadblock.
        if (objp->get_obj_type() == 0x1E)
        {
            return true;
        }
        objp = World.object_from_handle(objp->get_parent());
    }
    return false;
}

// 0x00A1C590 (:226)
// Magic numbers: 0xFF in a handle's zone byte is a managed layer's object; object type 0x1E is the roadblock node.
static bool child_of_roadblock(const object* objp)
{
    if (((objp->get_handle() >> 16) & 0xFF) != 0xFF)
    {
        return false;
    }
    return objp->get_obj_type() == 0x1E || check_roadblock_parenting(World.object_from_handle(objp->get_parent()));
}

// 0x00A1C610 (:372)
static gfm_orders_types get_orders_from_name(const char* orders_name)
{
    if (orders_name == nullptr)
    {
        return GFM_ORDERS_TYPE_NONE;
    }
    for (int i = 0; i < NUM_GFM_ORDERS_TYPES; i++)
    {
        if (ORDERS_NAMES[i] != nullptr && v_stricmp(orders_name, ORDERS_NAMES[i]) == 0)
        {
            return (gfm_orders_types)i;
        }
    }
    // Original: Int3 critical function here.
    return GFM_ORDERS_TYPE_NONE;
}

// 0x00A1C680 (:397)
static spawn_node_respawn_speed get_respawn_speed(const char* respawn_name)
{
    if (respawn_name == nullptr)
    {
        return RESPAWN_SPEED_NONE;
    }
    for (int i = 0; i < 4; i++)
    {
        if (v_stricmp(respawn_name, RESPAWN_TAGS[i]) == 0)
        {
            return (spawn_node_respawn_speed)i;
        }
    }
    // Original: Int3 critical function here.
    return RESPAWN_SPEED_NONE;
}

// 0x00A6CB50 (:566)
// 0x00A6CBA0 (:578)
// 0x00A6CBF0 (:590)
// Orders at the parent itself: create_orders_from_parent_action_node, _ped_node and _patrol as one.
static bool create_orders_at_object(const object_squad_spawn_node* node, gfm_orders_creation_params& params, gfm_orders_types orders_type, const object* target)
{
    params.orders_type = orders_type;
    params.team = node->get_team();
    params.location_target = target->pos;
    params.object_target = target->get_handle();
    params.ofp = DEFAULT_ORDERS_FLAGS;
    return true;
}

// 0x00A6CC40 (:603)
// 0x00A6CC90 (:615)
// Defends the parent from the node's position: two of rfg.exe's functions as one.
static bool create_orders_defend_from_node(const object_squad_spawn_node* node, gfm_orders_creation_params& params, const object* target)
{
    params.orders_type = GFM_ORDERS_TYPE_DEFEND;
    params.team = node->get_team();
    params.location_target = node->pos;
    params.object_target = target->get_handle();
    params.ofp = DEFAULT_ORDERS_FLAGS;
    return true;
}

// 0x00A6CCE0 (:629)
static bool create_orders_from_parent(const object_squad_spawn_node* node, gfm_orders_creation_params& params, const object* parent)
{
    if (parent == nullptr)
    {
        return false;
    }
    switch (parent->get_obj_type())
    {
    case OT_PATROL:
        return create_orders_at_object(node, params, GFM_ORDERS_TYPE_PATROL, parent);
    case OT_ACTION_NODE:
        return create_orders_at_object(node, params, GFM_ORDERS_TYPE_AMBIENT_ACTION, parent);
    case OT_BOUNDING_BOX:
        return create_orders_defend_from_node(node, params, parent);
    default:
        break;
    }
    if (parent->get_obj_type() == OT_MOVER && parent->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        return create_orders_defend_from_node(node, params, parent);
    }
    // Magic number: nav point type 2 is "pedestrian".
    if (parent->get_obj_type() == OT_NAVPOINT && ((const navpoint*)parent)->type == 2)
    {
        return create_orders_at_object(node, params, GFM_ORDERS_TYPE_AMBIENT_PED, parent);
    }
    // Defends itself.
    return create_orders_at_object(node, params, GFM_ORDERS_TYPE_DEFEND, node);
}

// 0x00A6CD80 (:697)
static bool create_orders_from_type(const object_squad_spawn_node* node, gfm_orders_types orders_type, gfm_orders_creation_params& params, const object* parent)
{
    params.orders_type = orders_type;
    params.team = node->get_team();
    params.ofp = DEFAULT_ORDERS_FLAGS;
    switch (orders_type)
    {
    case GFM_ORDERS_TYPE_FODDER:
        params.ofp = FODDER_ORDERS_FLAGS;
        break;
    case GFM_ORDERS_TYPE_TERRORIZE:
    case GFM_ORDERS_TYPE_WAIT:
    case GFM_ORDERS_TYPE_ROADBLOCK:
    case GFM_ORDERS_TIED_UP:
        params.object_target = node->get_handle();
        params.location_target = node->pos;
        return true;
    default:
        break;
    }
    if (parent == nullptr)
    {
        return false;
    }
    params.object_target = parent->get_handle();
    params.location_target = parent->pos;
    return true;
}

// 0x00A4DD50 (:785)
static unsigned int find_spawned_npc(unsigned int spawn_node_handle, const vector& node_pos)
{
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        npc* np = World.get_object<npc>(i);
        if (np->spawn_node_handle == spawn_node_handle)
        {
            return !np->is_killed() && vec_dist_squared(node_pos, np->pos) < SPAWNED_OBJECT_RANGE_SQUARED ? np->get_handle() : 0xFFFFFFFF;
        }
    }
    return 0xFFFFFFFF;
}

// 0x00A4DF70 (:812)
static unsigned int find_spawned_squad(unsigned int spawn_node_handle)
{
    for (int i = 0; i < World.get_num_objects<object_squad>(); i++)
    {
        object_squad* squad = World.get_object<object_squad>(i);
        if (squad->spawn_node_handle == spawn_node_handle)
        {
            return !squad->is_killed() ? squad->get_handle() : 0xFFFFFFFF;
        }
    }
    return 0xFFFFFFFF;
}

// 0x00A1CA10 (:1531)
void types_object_npc_spawn_node(signed char& type, signed char& subtype)
{
    type = OT_NPC_SPAWN_NODE;
    subtype = -1;
}

// 0x00A1CA20 (:1531)
int bpo_index_object_npc_spawn_node()
{
    return 1;
}

// 0x00A1CA30 (:1531)
const char* classname_object_npc_spawn_node()
{
    return "object_npc_spawn_node";
}

// 0x00A6D230 (:1531)
object* alloc_object_npc_spawn_node()
{
    return object_npc_spawn_nodepool.alloc(0);
}

static obj_class_instantiator object_npc_spawn_node_instantiator("object_npc_spawn_node", alloc_object_npc_spawn_node, types_object_npc_spawn_node, bpo_index_object_npc_spawn_node, classname_object_npc_spawn_node, object_npc_spawn_node::pool_remaining, 0x2D80, 64);

// 0x00A1CA40 (:1531)
unsigned int object_npc_spawn_node::get_classname_hash()
{
    return object_npc_spawn_node_instantiator.this_hash;
}

// 0x00A3E7F0 (:1531)
int object_npc_spawn_node::pool_remaining()
{
    return object_npc_spawn_nodepool.get_free();
}

// 0x00A94320 (:1531)
void object_npc_spawn_node::pool_free()
{
    object_npc_spawn_nodepool.free(this);
}

// 0x00A1CA50 (:1532)
object_npc_spawn_node::object_npc_spawn_node()
{
}

// 0x00A1CA80
object_npc_spawn_node::~object_npc_spawn_node()
{
}

// 0x00A0D060 (.h:193)
const char* object_npc_spawn_node::get_classname_static()
{
    return "object_npc_spawn_node";
}

// 0x00A0D070 (.h:193)
int object_npc_spawn_node::get_bpo_index_static()
{
    return 1;
}

// 0x00A1CA70 (.h:193)
const char* object_npc_spawn_node::get_classname() const
{
    return "object_npc_spawn_node";
}

// 0x00AABDF0 (:1537)
bool object_npc_spawn_node::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        return true;
    }
    *(unsigned int*)&node_flags = 0;
    h_spawn_group = nullptr;
    spawn_prob = 1.0f;
    night_spawn_prob = 1.0f;
    child_npc_handle = find_spawned_npc(get_handle(), pos);
    char group_name[256];
    group_name[0] = '\0';
    pblock.get(SPAWN_NODE_PROP_SET_NAME, group_name);
    h_spawn_group = sog_get_human_group(group_name);
    if (h_spawn_group == nullptr)
    {
        return false;
    }
    pblock.get_optional(SPAWN_NODE_PROP_SPAWN_PROB, spawn_prob);
    spawn_prob = clamp_spawn_prob(spawn_prob);
    if (!pblock.get_optional(SPAWN_NODE_PROP_NIGHT_SPAWN_PROB, night_spawn_prob))
    {
        night_spawn_prob = spawn_prob;
    }
    night_spawn_prob = clamp_spawn_prob(night_spawn_prob);
    bool value = false;
    pblock.get_optional(SPAWN_NODE_PROP_DEAD_BODY, value);
    node_flags.spawn_dead_body = value;
    value = false;
    node_flags.special_spawn_node = pblock.get_optional(SPAWN_NODE_PROP_SPECIAL_NPC, value) && value;
    if (is_hostage_group(h_spawn_group->group_name))
    {
        node_flags.is_hostage = 1;
        h_spawn_group = sog_default_human_spawn_group(h_spawn_group->get_team());
        spawn_prob = 0.0f;
        night_spawn_prob = 0.0f;
    }
    else
    {
        node_flags.is_hostage = 0;
    }
    return true;
}

// 0x00A1CA90 (:1607)
bool object_npc_spawn_node::in_spawn_range(const player* pp) const
{
    return vec_dist_squared(pp->pos, pos) < (node_flags.is_hostage ? HOSTAGE_NODE_RANGE_SQUARED : NPC_NODE_RANGE_SQUARED);
}

// 0x00A1CAF0 (:1625)
float object_npc_spawn_node::get_spawn_prob() const
{
    return game_clock_get_time_of_day() != GC_NIGHT ? spawn_prob : night_spawn_prob;
}

// 0x00A1CB10 (:1637)
void object_npc_spawn_node::do_respawn()
{
}

// 0x00A3E820 (:1644)
void object_npc_spawn_node::evaluate_spawn()
{
    node_flags.evaluated_spawn = 1;
    if (child_npc_handle != 0xFFFFFFFF)
    {
        if (World.object_from_handle<npc>(child_npc_handle) != nullptr)
        {
            return;
        }
        child_npc_handle = 0xFFFFFFFF;
    }
    if (Multiplayer && !multi_is_server())
    {
        return;
    }
    if (h_spawn_group != nullptr && h_spawn_group->get_team() == HUMAN_TEAM_EDF && !handle_in_high_zone(get_handle()) && district_get_from_pos(pos)->is_liberated() && !mission_final_mission_complete())
    {
        return;
    }
    if (fl_rand() <= get_spawn_prob())
    {
        node_flags.spawn_next_frame = 1;
        Spawn_node_flags.spawn_next_frame_npc = 1;
    }
}

// 0x00A1CB20 (:1684)
void object_npc_spawn_node::destroy_notify()
{
    remove_constraints();
}

// 0x00A3E900 (:1694)
bool object_npc_spawn_node::stream(bool load)
{
    if (load)
    {
        evaluate_spawn();
        return true;
    }
    if (obj_flags.destroy_on_stream)
    {
        World.destroy_object(this, false);
    }
    // Magic number: 30 m from the node.
    if (object_should_be_destroyed(pos, child_npc_handle, 900.0f))
    {
        World.destroy_object(child_npc_handle, false);
    }
    node_flags.evaluated_spawn = 0;
    return true;
}

// 0x00A3E970 (:1727)
npc* object_npc_spawn_node::get_child_npc() const
{
    return child_npc_handle != 0xFFFFFFFF ? World.object_from_handle<npc>(child_npc_handle) : nullptr;
}

// 0x00A1CB30 (:1733)
void object_npc_spawn_node::hierarchy_notify(hierarchy_notify_code code)
{
    if (code == HN_PARENT_DESTROYED)
    {
        World.destroy_object(this, false);
    }
}

// 0x00A94330 (:1744)
void object_npc_spawn_node::free_this()
{
    object::free_this();
}

// 0x00A1CB50 (:1751)
human_teams object_npc_spawn_node::get_team() const
{
    return h_spawn_group != nullptr ? h_spawn_group->get_team() : HUMAN_TEAM_NONE;
}

// 0x00A6D240 (:1758)
bool object_npc_spawn_node::has_human_info(const human_info* h_info) const
{
    for (int i = 0; i < h_spawn_group->h_info_list.size(); i++)
    {
        if (h_spawn_group->h_info_list[i] == h_info)
        {
            return true;
        }
    }
    return false;
}

// 0x00A1CB70 (:1764)
bool object_npc_spawn_node::can_spawn_child() const
{
    if (this == nullptr || !obj_flags.streamed)
    {
        return false;
    }
    const object* parent_objp = World.object_from_handle(get_parent());
    return parent_objp == nullptr || parents_streamed_in(parent_objp);
}

// 0x00A94340 (:1772)
unsigned int object_npc_spawn_node::spawn_child_object()
{
    node_flags.spawn_next_frame = 0;
    if (node_flags.disabled || node_flags.mission_disabled || obj_flags.destroyed)
    {
        return child_npc_handle;
    }
    if (child_npc_handle != 0xFFFFFFFF)
    {
        npc* child = World.object_from_handle<npc>(child_npc_handle);
        if (child != nullptr && (!child->is_killed() || node_flags.spawn_dead_body))
        {
            return child_npc_handle;
        }
        child_npc_handle = 0xFFFFFFFF;
    }
    object* parent_objp = nullptr;
    if (get_parent() != 0xFFFFFFFF)
    {
        parent_objp = World.object_from_handle(get_parent());
        if (parent_objp == nullptr || parent_objp->is_killed())
        {
            return child_npc_handle;
        }
    }
    const human_info* h_info = srm_pick_loaded_human_info(h_spawn_group);
    if (h_info == nullptr)
    {
        squad_progression_set member;
        member.reset();
        if (!squad_progression_member_human_info(member, h_spawn_group->get_team(), nullptr, false, false))
        {
            return child_npc_handle;
        }
        h_info = srm_pick_loaded_human_info(h_spawn_group);
        if (h_info == nullptr)
        {
            return child_npc_handle;
        }
    }
    npc_spawn_param_list list(NPC_MANAGER_SPAWN_NODE);
    npc_spawn_params params;
    params.reset();
    params.hum_info = h_info;
    list.npc_spawn_info.add(params);
    list.spawn_flags.spawn_on_node = true;
    list.spawn_pos = pos;
    list.spawn_orient = orient;
    list.npc_flags.immediate_spawn = Immediate_spawn_flag;
    list.assign_spawn_parent(parent_objp);
    list.hflags.activity_ha_hostage = node_flags.is_hostage;
    spawn_status_result result = object_spawn_do_spawn(list);
    if (result == SPAWN_STATUS_STREAMING)
    {
        node_flags.spawn_next_frame = 1;
    }
    else if (result != SPAWN_STATUS_FAILURE)
    {
        npc* np = list.npc_spawn_info[0].np;
        if (np != nullptr)
        {
            child_npc_handle = np->get_handle();
            np->obj_flags.no_save = 1;
            np->spawn_node_handle = get_handle();
            if (node_flags.spawn_dead_body)
            {
                np->npc_flags.spawned_as_corpse = 1;
            }
        }
    }
    return child_npc_handle;
}

// 0x00A0D080 (.h:230)
unsigned int object_npc_spawn_node::get_child_npc_handle() const
{
    return child_npc_handle;
}

// 0x00A0D090 (.h:243)
bool object_npc_spawn_node::should_spawn() const
{
    return node_flags.spawn_next_frame;
}

// 0x00A0D0A0 (.h:246)
bool object_npc_spawn_node::should_evaluate_spawn() const
{
    return !node_flags.evaluated_spawn;
}

// 0x00A0D0B0 (.h:252)
void object_npc_spawn_node::reevaluate_spawn()
{
    node_flags.evaluated_spawn = 0;
}

// 0x00B42F00 (.h:255)
bool object_npc_spawn_node::use_special_slot_npc() const
{
    return node_flags.special_spawn_node;
}

// 0x00754F60 (.h:258)
bool object_npc_spawn_node::is_hostage_node() const
{
    return node_flags.is_hostage;
}

// 0x00B1FCB0 (.h:269)
void object_npc_spawn_node::disable_spawn()
{
    node_flags.disabled = 1;
}

// 0x00B1FCC0 (.h:272)
void object_npc_spawn_node::disable_mission_spawn()
{
    node_flags.mission_disabled = 1;
}

// 0x00A0D0C0 (.h:273)
void object_npc_spawn_node::enable_mission_spawn()
{
    node_flags.mission_disabled = 0;
}

// 0x00A0D0D0 (.h:276)
bool object_npc_spawn_node::is_disabled() const
{
    return node_flags.disabled || node_flags.mission_disabled;
}

// 0x00A1CBB0 (:1896)
void types_object_squad_spawn_node(signed char& type, signed char& subtype)
{
    type = OT_SQUAD_SPAWN_NODE;
    subtype = -1;
}

// 0x00A1CBC0 (:1896)
int bpo_index_object_squad_spawn_node()
{
    return 1;
}

// 0x00A1CBD0 (:1896)
const char* classname_object_squad_spawn_node()
{
    return "object_squad_spawn_node";
}

// 0x00A6D290 (:1896)
object* alloc_object_squad_spawn_node()
{
    return object_squad_spawn_nodepool.alloc(0);
}

static obj_class_instantiator object_squad_spawn_node_instantiator("object_squad_spawn_node", alloc_object_squad_spawn_node, types_object_squad_spawn_node, bpo_index_object_squad_spawn_node, classname_object_squad_spawn_node, object_squad_spawn_node::pool_remaining, 0xE540, 256);

// 0x00A1CBE0 (:1896)
unsigned int object_squad_spawn_node::get_classname_hash()
{
    return object_squad_spawn_node_instantiator.this_hash;
}

// 0x00A3E9B0 (:1896)
int object_squad_spawn_node::pool_remaining()
{
    return object_squad_spawn_nodepool.get_free();
}

// 0x00A945A0 (:1896)
void object_squad_spawn_node::pool_free()
{
    object_squad_spawn_nodepool.free(this);
}

// 0x00A1CBF0 (:1897)
object_squad_spawn_node::object_squad_spawn_node()
{
}

// 0x00A1CC30
object_squad_spawn_node::~object_squad_spawn_node()
{
}

// 0x00A0D0F0 (.h:317)
const char* object_squad_spawn_node::get_classname_static()
{
    return "object_squad_spawn_node";
}

// 0x00A0D100 (.h:317)
int object_squad_spawn_node::get_bpo_index_static()
{
    return 1;
}

// 0x00A1CC20 (.h:317)
const char* object_squad_spawn_node::get_classname() const
{
    return "object_squad_spawn_node";
}

// 0x00AAC060 (:1902)
bool object_squad_spawn_node::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        return true;
    }
    *(unsigned int*)&node_flags = 0;
    spawn_prob = 1.0f;
    night_spawn_prob = 1.0f;
    default_orders_type = GFM_ORDERS_TYPE_NONE;
    squad_def = nullptr;
    veh_spawn_group = nullptr;
    child_squad_handle = 0xFFFFFFFF;
    child_vehicle_handle = 0xFFFFFFFF;
    original_orders_handle = 0xFFFFFFFF;
    respawn_timer.invalidate();
    respawn_speed = RESPAWN_SPEED_NONE;
    local_bbox.min.x = last_known_bmin.x - pos.x;
    local_bbox.min.y = last_known_bmin.y - pos.y;
    local_bbox.min.z = last_known_bmin.z - pos.z;
    local_bbox.max.x = last_known_bmax.x - pos.x;
    local_bbox.max.y = last_known_bmax.y - pos.y;
    local_bbox.max.z = last_known_bmax.z - pos.z;
    char name[256];
    name[0] = '\0';
    pblock.get(SPAWN_NODE_SQUAD_DEF_NAME, name);
    bool value = false;
    node_flags.raid_spawn = pblock.get_optional(SPAWN_NODE_PROP_RAID_FLAG, value) && value;
    node_flags.hands_off_raid_squad = pblock.get_optional(SPAWN_NODE_PROP_HANDS_OFF_RAID_FLAG, value) && value;
    node_flags.miner_persona = pblock.get_optional(SPAWN_NODE_PROP_MINER_PERSONA_FLAG, value) && value;
    node_flags.safehouse_vip = pblock.get_optional(SPAWN_NODE_PROP_SAFEHOUSE_VIP_FLAG, value) && value;
    squad_def = squad_definition_from_name(name);
    if (squad_def == nullptr)
    {
        return false;
    }
    if (pblock.get_optional(SQUAD_PROP_SQUAD_VEHICLE, value) && value)
    {
        name[0] = '\0';
        if (pblock.get_optional(SQUAD_PROP_VEHICLE_TYPE, name))
        {
            veh_spawn_group = sog_get_vehicle_group(name);
        }
    }
    node_flags.special_spawn_vehicle = pblock.get_optional(SPAWN_NODE_PROP_SPECIAL_VEHICLE, value) && value;
    if (node_flags.special_spawn_vehicle && veh_spawn_group == nullptr)
    {
        node_flags.special_spawn_vehicle = 0;
    }
    node_flags.special_spawn_npc = pblock.get_optional(SPAWN_NODE_PROP_SPECIAL_NPC, value) && value;
    if (pblock.get_optional(SPAWN_NODE_DEFAULT_ORDERS, name))
    {
        default_orders_type = get_orders_from_name(name);
    }
    else if (!pblock.get_optional(SPAWN_NODE_DEFAULT_ORDERS_ID, default_orders_type))
    {
        default_orders_type = GFM_ORDERS_TYPE_NONE;
    }
    if ((int)default_orders_type < 0 || (int)default_orders_type >= NUM_GFM_ORDERS_TYPES)
    {
        return false;
    }
    if (!handle_in_high_zone(get_handle()))
    {
        node_flags.no_reassignment = pblock.get_optional(SPAWN_NODE_NO_REASSIGNMENT, value) && value;
        node_flags.disable_during_alert = squad_def->squad_team == HUMAN_TEAM_CIVILIAN;
    }
    else
    {
        node_flags.no_reassignment = 1;
        node_flags.ignore_tod_spawning = 1;
    }
    node_flags.roadblock_spawn_node = child_of_roadblock(this);
    const object* parent_objp = World.object_from_handle(get_parent());
    // Magic number: object type 0x15 parents a node that only a script spawns.
    if (parent_objp != nullptr && parent_objp->get_obj_type() == 0x15)
    {
        spawn_prob = 0.0f;
        night_spawn_prob = 0.0f;
    }
    else
    {
        pblock.get_optional(SPAWN_NODE_PROP_SPAWN_PROB, spawn_prob);
        spawn_prob = clamp_spawn_prob(spawn_prob);
        if (!pblock.get_optional(SPAWN_NODE_PROP_NIGHT_SPAWN_PROB, night_spawn_prob))
        {
            night_spawn_prob = spawn_prob;
        }
        night_spawn_prob = clamp_spawn_prob(night_spawn_prob);
    }
    respawn_speed = pblock.get_optional(SPAWN_NODE_RESPAWN_SPEED, name) ? get_respawn_speed(name) : RESPAWN_SPEED_NONE;
    node_flags.radio_operator = pblock.get_optional(SPAWN_NODE_PROP_RADIO_FLAG, value) && value;
    if (handle_in_high_zone(get_handle()))
    {
        node_flags.no_reassignment = 1;
    }
    child_squad_handle = find_spawned_squad(get_handle());
    // Magic number: 0xFF in a handle's zone byte is a managed layer's object.
    if (((get_handle() >> 16) & 0xFF) == 0xFF)
    {
        obj_flags.no_save = 1;
    }
    if (node_flags.radio_operator)
    {
        node_flags.miner_persona = 0;
        node_flags.special_spawn_npc = 0;
        node_flags.special_spawn_vehicle = 0;
        veh_spawn_group = nullptr;
    }
    if (node_flags.safehouse_vip)
    {
        node_flags.raid_spawn = 0;
        node_flags.miner_persona = 0;
        node_flags.special_spawn_npc = 0;
        node_flags.special_spawn_vehicle = 0;
        node_flags.no_reassignment = 1;
        veh_spawn_group = nullptr;
        spawn_prob = 0.0f;
        night_spawn_prob = 0.0f;
    }
    if (squad_def->special_npc_info != nullptr && squad_def->special_npc_info->flags.guerrilla_tech)
    {
        respawn_speed = RESPAWN_SPEED_FAST;
    }
    return true;
}

// 0x00A1CC40 (:2122)
void object_squad_spawn_node::disable_spawn()
{
    node_flags.disabled = 1;
}

// 0x00A1CC50 (:2127)
void object_squad_spawn_node::enable_spawn()
{
    node_flags.disabled = 0;
}

// 0x00A1CC60 (:2134)
float object_squad_spawn_node::get_spawn_prob() const
{
    return game_clock_get_time_of_day() != GC_NIGHT ? spawn_prob : night_spawn_prob;
}

// 0x00A1CC80 (:2145)
bool object_squad_spawn_node::has_respawn_speed() const
{
    return (int)respawn_speed > 0 && (int)respawn_speed < 4;
}

// 0x00A6D2A0 (:2152)
void object_squad_spawn_node::evaluate_alert_level_spawn()
{
    if (has_respawn_speed() && fl_rand() <= get_spawn_prob())
    {
        do_respawn(true);
    }
}

// 0x00A6D2F0 (:2176)
void object_squad_spawn_node::change_tod_status()
{
    if (node_flags.ignore_tod_spawning)
    {
        return;
    }
    if (fl_rand() <= get_spawn_prob())
    {
        do_respawn(true);
        return;
    }
    object_squad* squad = child_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(child_squad_handle) : nullptr;
    if (squad != nullptr)
    {
        squad->request_despawn_all_members(NPC_DESPAWN_TYPE_LOW, nullptr, true);
    }
}

// 0x00A3E9E0 (:2207)
void object_squad_spawn_node::do_respawn(bool force_respawn)
{
    if (force_respawn)
    {
        respawn_timer.set(0);
    }
    else if (mission_is_active() || respawn_speed == RESPAWN_SPEED_NONE)
    {
        return;
    }
    if (child_squad_handle != 0xFFFFFFFF && World.object_from_handle<object_squad>(child_squad_handle) != nullptr)
    {
        return;
    }
    if (respawn_timer.valid())
    {
        if (respawn_timer.elapsed())
        {
            node_flags.evaluated_spawn = 0;
            node_flags.respawn = 1;
        }
    }
    else if ((int)respawn_speed < 4 && (int)respawn_speed > 0)
    {
        respawn_timer.set(RESPAWN_DELAYS_MS[respawn_speed]);
    }
    else
    {
        respawn_timer.invalidate();
    }
}

// 0x00A1CCA0 (:2255)
bool object_squad_spawn_node::in_spawn_range(const player* pp) const
{
    return obj_flags.streamed;
}

// 0x00A6D370 (:2263)
void object_squad_spawn_node::evaluate_spawn()
{
    node_flags.evaluated_spawn = 1;
    if (child_squad_handle != 0xFFFFFFFF)
    {
        if (World.object_from_handle<object_squad>(child_squad_handle) != nullptr)
        {
            return;
        }
        child_squad_handle = 0xFFFFFFFF;
    }
    if (Multiplayer && !multi_is_server())
    {
        return;
    }
    if (squad_def != nullptr && squad_def->squad_team == HUMAN_TEAM_EDF && !handle_in_high_zone(get_handle()))
    {
        district* dp = district_get_from_pos(pos);
        if (dp != nullptr && dp->is_liberated() && !mission_final_mission_complete())
        {
            return;
        }
    }
    if (mission_is_active() && squad_def != nullptr && squad_def->special_npc_info != nullptr && !handle_in_high_zone(get_handle()) &&
        (squad_def->special_npc_info->flags.guerrilla_tech || squad_def->special_npc_info->flags.guerrilla_rfc))
    {
        return;
    }
    if (!Spawn_squad_disabled && fl_rand() <= get_spawn_prob())
    {
        node_flags.spawn_next_frame = 1;
        Spawn_node_flags.spawn_next_frame_squad = 1;
    }
}

// 0x00A3EAA0 (:2317)
void object_squad_spawn_node::destroy_notify()
{
    remove_constraints();
    object_squad* squad = child_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(child_squad_handle) : nullptr;
    if (squad != nullptr && squad->simulated())
    {
        World.destroy_object(squad, false);
    }
    Orders_manager.destroy_orders(original_orders_handle);
    original_orders_handle = 0xFFFFFFFF;
}

// 0x00A3EB00 (:2349)
void object_squad_spawn_node::adopt_child_squad(unsigned int new_squad_handle)
{
    object_squad* squad = new_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(new_squad_handle) : nullptr;
    if (squad != nullptr)
    {
        squad->spawn_node_handle = get_handle();
        child_squad_handle = new_squad_handle;
    }
}

// 0x00A945B0 (:2364)
void object_squad_spawn_node::abandon_child_squad()
{
    if (handle_in_high_zone(get_handle()))
    {
        return;
    }
    object_squad* squad = child_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(child_squad_handle) : nullptr;
    child_squad_handle = 0xFFFFFFFF;
    if (squad != nullptr)
    {
        if (original_orders_handle != 0xFFFFFFFF)
        {
            squad->clear_orders(original_orders_handle);
        }
        if (squad->simulated())
        {
            World.destroy_object(squad, false);
        }
    }
}

// 0x00A94630 (:2404)
bool object_squad_spawn_node::stream(bool load)
{
    if (!load && obj_flags.destroy_on_stream)
    {
        World.destroy_object(this, false);
    }
    if (node_flags.safehouse_vip)
    {
        safehouse_vip_notify(this, load);
    }
    if (load)
    {
        evaluate_spawn();
        return true;
    }
    node_flags.evaluated_spawn = 0;
    if (!obj_flags.persistent || Destream_orders_removal)
    {
        abandon_child_squad();
    }
    return true;
}

// 0x00A1CCB0 (:2439)
void object_squad_spawn_node::get_bbox(vector& bmin, vector& bmax) const
{
    get_local_bbox(bmin, bmax);
    bmin.x += pos.x;
    bmin.y += pos.y;
    bmin.z += pos.z;
    bmax.x += pos.x;
    bmax.y += pos.y;
    bmax.z += pos.z;
}

// 0x00A1CDB0 (:2447)
void object_squad_spawn_node::get_local_bbox(vector& bmin, vector& bmax) const
{
    bmin = local_bbox.min;
    bmax = local_bbox.max;
}

// 0x00A6D490 (:2457)
void object_squad_spawn_node::hierarchy_notify(hierarchy_notify_code code)
{
    if (code == HN_PARENT_DESTROYED)
    {
        World.destroy_object(this, false);
        return;
    }
    if (code == HN_CHILDREN_STREAMED)
    {
        bbox bounds;
        bounds.min = last_known_bmin;
        bounds.max = last_known_bmax;
        farray<object const*,256> children;
        world_collect_descendants(this, children, nullptr, nullptr);
        for (int i = 0; i < children.size(); i++)
        {
            vector child_min, child_max;
            children[i]->get_bbox(child_min, child_max);
            bbox_expand(bounds, child_min);
            bbox_expand(bounds, child_max);
        }
        local_bbox.min.x = bounds.min.x - pos.x;
        local_bbox.min.y = bounds.min.y - pos.y;
        local_bbox.min.z = bounds.min.z - pos.z;
        local_bbox.max.x = bounds.max.x - pos.x;
        local_bbox.max.y = bounds.max.y - pos.y;
        local_bbox.max.z = bounds.max.z - pos.z;
    }
}

// 0x00A946A0 (:2494)
void object_squad_spawn_node::free_this()
{
    object::free_this();
}

// 0x00A1CDF0 (:2501)
void object_squad_spawn_node::assign_default_orders(gfm_orders_types new_orders_type)
{
    if ((unsigned int)new_orders_type < NUM_GFM_ORDERS_TYPES)
    {
        default_orders_type = new_orders_type;
    }
}

// 0x00A1CE10 (:2511)
bool object_squad_spawn_node::can_spawn_child() const
{
    return true;
}

// 0x00A6D670 (:2519)
unsigned int object_squad_spawn_node::spawn_child_object(minimap_elements spawn_icon, map_icon_types map_icon_type)
{
    node_flags.spawn_next_frame = 0;
    if (node_flags.disabled || node_flags.mission_disabled || obj_flags.destroyed)
    {
        return child_squad_handle;
    }
    if (node_flags.disable_during_alert && (int)gsm_get_alert_level() > 1)
    {
        return child_squad_handle;
    }
    if (!handle_in_high_zone(get_handle()) && squad_def != nullptr && team_squad_spawn_disabled(squad_def->squad_team))
    {
        return child_squad_handle;
    }
    if (child_squad_handle != 0xFFFFFFFF)
    {
        object_squad* child = World.object_from_handle<object_squad>(child_squad_handle);
        if (child != nullptr && !child->is_killed())
        {
            return child_squad_handle;
        }
        child_squad_handle = 0xFFFFFFFF;
    }
    squad_spawn_parameters params;
    const object* parent_objp = World.object_from_handle(get_parent());
    if ((int)default_orders_type < 0 || (int)default_orders_type > GFM_ORDERS_TIED_UP)
    {
        return child_squad_handle;
    }
    if (default_orders_type == GFM_ORDERS_TYPE_NONE)
    {
        if (!create_orders_from_parent(this, params.orders, parent_objp))
        {
            return child_squad_handle;
        }
    }
    else
    {
        if (!create_orders_from_type(this, default_orders_type, params.orders, parent_objp))
        {
            return child_squad_handle;
        }
        if (default_orders_type == GFM_ORDERS_TIED_UP)
        {
            params.squad.npc_flags.tied_up = 1;
        }
    }
    if (!node_flags.respawn || Immediate_spawn_flag)
    {
        params.squad.squad_pos = pos;
        params.squad.squad_orient = orient;
    }
    else
    {
        team_spawn_location location;
        if (!reinforcement_find_spawn_location(pos, orient, get_team(), veh_spawn_group != nullptr, location))
        {
            return child_squad_handle;
        }
        params.squad.squad_pos = location.spawn_pos;
        params.squad.squad_orient = location.spawn_orient;
    }
    node_flags.respawn = 0;
    params.squad.veh_spawn_group = veh_spawn_group;
    params.squad.squad_def = squad_def;
    params.squad.squad_flags.disallow_reassignment = node_flags.no_reassignment;
    params.squad.spawn_node_handle = get_handle();
    params.squad.squad_map_icon = spawn_icon;
    params.squad.squad_flags.stay_in_vehicle = get_stay_in_vehicle_flag(default_orders_type, this);
    params.squad.npc_flags.mission_related = mission_is_active() && handle_in_high_zone(get_handle());
    params.squad.hflags.activity_raid_required = node_flags.raid_spawn;
    params.squad.squad_flags.hands_off_raid_squad = node_flags.hands_off_raid_squad;
    params.squad.hflags.miner_persona_lines = node_flags.miner_persona;
    params.squad.squad_flags.squad_icon_is_sticky = map_icon_type == MAP_ICON_TYPE_STICKY || map_icon_type == MAP_ICON_TYPE_CLOSEST_STICKY;
    params.squad.squad_flags.squad_icon_is_closest = map_icon_type == MAP_ICON_TYPE_CLOSEST_STICKY;
    if (node_flags.safehouse_vip)
    {
        params.squad.hflags.invulnerable = 1;
        params.squad.hflags.safehouse_vip = 1;
    }
    if (node_flags.radio_operator)
    {
        params.squad.hflags.invulnerable = 1;
        params.squad.hflags.radio_operator = 1;
        params.squad.hflags.do_not_play_ambient_or_greet_lines = 1;
        params.squad.hflags.disallow_flinches_and_ragdolls = 1;
        params.squad.hflags.only_use_action_nodes = 1;
    }
    object_squad* squad = params.spawn_squad();
    if (squad == nullptr)
    {
        return child_squad_handle;
    }
    respawn_timer.invalidate();
    obj_flags.no_save = 1;
    child_squad_handle = squad->get_handle();
    if (obj_flags.persistent)
    {
        const object* squad_parent = squad->get_parent() != 0xFFFFFFFF ? World.object_from_handle(squad->get_parent()) : nullptr;
        if (squad->get_parent() == 0xFFFFFFFF || (squad_parent != nullptr && squad_parent->obj_flags.persistent))
        {
            squad->obj_flags.persistent = 1;
        }
    }
    if (params.created_orders != nullptr)
    {
        original_orders_handle = params.created_orders->get_orders_handle();
    }
    return child_squad_handle;
}

// 0x00A6DA60 (:2705)
void object_squad_spawn_node::force_despawn_squad()
{
    object_squad* squad = child_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(child_squad_handle) : nullptr;
    if (squad != nullptr)
    {
        squad->request_despawn_all_members(NPC_DESPAWN_TYPE_HIGH, nullptr, true);
    }
    node_flags.evaluated_spawn = 0;
}

// 0x00A3EB40 (:2722)
human_teams object_squad_spawn_node::get_team() const
{
    return squad_def != nullptr ? squad_def->squad_team : HUMAN_TEAM_NONE;
}

// 0x00A3EB60 (:2728)
unsigned int object_squad_spawn_node::get_child_vehicle_handle() const
{
    object_squad* squad = child_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(child_squad_handle) : nullptr;
    return squad != nullptr ? squad->squad_vehicle : child_vehicle_handle;
}

// 0x00754F90 (.h:351)
unsigned int object_squad_spawn_node::get_child_squad_handle() const
{
    return child_squad_handle;
}

// 0x00A0D110 (.h:375)
bool object_squad_spawn_node::should_spawn() const
{
    return node_flags.spawn_next_frame;
}

// 0x00A0D120 (.h:378)
bool object_squad_spawn_node::should_evaluate_spawn() const
{
    return !node_flags.evaluated_spawn;
}

// 0x00B1FCD0 (.h:389)
void object_squad_spawn_node::disable_mission_spawn()
{
    node_flags.mission_disabled = 1;
}

// 0x00A0D130 (.h:390)
void object_squad_spawn_node::enable_mission_spawn()
{
    node_flags.mission_disabled = 0;
}

// 0x00B42F10 (.h:393)
bool object_squad_spawn_node::use_special_slot_npc() const
{
    return node_flags.special_spawn_npc;
}

// 0x00B42F20 (.h:394)
bool object_squad_spawn_node::use_special_slot_vehicle() const
{
    return node_flags.special_spawn_vehicle;
}

// 0x00754FA0 (.h:397)
bool object_squad_spawn_node::is_hands_off_raid_squad() const
{
    return node_flags.hands_off_raid_squad;
}

// 0x00A0D140 (.h:400)
bool object_squad_spawn_node::is_disabled() const
{
    return node_flags.disabled || node_flags.mission_disabled;
}

// 0x00A0D160 (.h:409)
void object_squad_spawn_node::reevaluate_spawn()
{
    node_flags.evaluated_spawn = 0;
}

// 0x00B1FCE0 (.h:412)
void object_squad_spawn_node::respawn_child()
{
    node_flags.respawn = 1;
}

// 0x00A0D170 (.h:430)
void object_squad_spawn_node::update_vehicle_handle(unsigned int new_vehicle_handle)
{
    child_vehicle_handle = new_vehicle_handle;
}

// 0x00A0D180 (.h:436)
bool object_squad_spawn_node::disabled_at_high_alert() const
{
    return node_flags.disable_during_alert;
}

// 0x00754FB0 (.h:439)
bool object_squad_spawn_node::is_roadblock_spawn_node() const
{
    return node_flags.roadblock_spawn_node;
}

// 0x00754FC0 (.h:442)
bool object_squad_spawn_node::is_safehouse_vip() const
{
    return node_flags.safehouse_vip;
}

// ---- turret nodes
static object_pool<object_turret_spawn_node, 64, 1> object_turret_spawn_nodepool; // 0x03197820

// 0x00A1CE40 (:2747)
void types_object_turret_spawn_node(signed char& type, signed char& subtype)
{
    type = OT_TURRET_SPAWN_NODE;
    subtype = -1;
}

// 0x00A1CE50 (:2747)
int bpo_index_object_turret_spawn_node()
{
    return 1;
}

// 0x00A1CE60 (:2747)
const char* classname_object_turret_spawn_node()
{
    return "object_turret_spawn_node";
}

// 0x00A6DAB0 (:2747)
object* alloc_object_turret_spawn_node()
{
    return object_turret_spawn_nodepool.alloc(0);
}

// Magic numbers: the pool usage and size, as registered at 0x0107C230.
static obj_class_instantiator object_turret_spawn_node_instantiator("object_turret_spawn_node", alloc_object_turret_spawn_node, types_object_turret_spawn_node, bpo_index_object_turret_spawn_node, classname_object_turret_spawn_node, object_turret_spawn_node::pool_remaining, 0x2E80, 64); // 0x03538994

// 0x00A1CE70 (:2747)
unsigned int object_turret_spawn_node::get_classname_hash()
{
    return object_turret_spawn_node_instantiator.this_hash;
}

// 0x00A3EBB0 (:2747)
int object_turret_spawn_node::pool_remaining()
{
    return object_turret_spawn_nodepool.get_free();
}

// 0x00A946B0 (:2747)
void object_turret_spawn_node::pool_free()
{
    object_turret_spawn_nodepool.free(this);
}

// 0x00A1CE80 (:2748)
object_turret_spawn_node::object_turret_spawn_node()
{
}

// 0x00A1CEB0
object_turret_spawn_node::~object_turret_spawn_node()
{
}

// 0x00754FD0 (.h:504)
const object_type object_turret_spawn_node::get_type_static()
{
    return OT_TURRET_SPAWN_NODE;
}

// 0x00754FE0 (.h:504)
const object_sub_type object_turret_spawn_node::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A0D190 (.h:504)
const char* object_turret_spawn_node::get_classname_static()
{
    return "object_turret_spawn_node";
}

// 0x00A0D1A0 (.h:504)
int object_turret_spawn_node::get_bpo_index_static()
{
    return 1;
}

// 0x00A1CEA0 (.h:504)
const char* object_turret_spawn_node::get_classname() const
{
    return "object_turret_spawn_node";
}

// 0x00A4E180 (:785)
// Original: only the first turret of this node is looked at.
static unsigned int find_spawned_turret(unsigned int spawn_node_handle, const vector& node_pos)
{
    const int count = World.get_num_objects<turret>();
    for (int i = 0; i < count; i++)
    {
        turret* tp = World.get_object<turret>(i);
        if (tp->spawn_node_handle != spawn_node_handle)
        {
            continue;
        }
        if (!tp->is_killed() && vec_dist_squared(node_pos, tp->pos) < SPAWNED_OBJECT_RANGE_SQUARED)
        {
            return tp->get_handle();
        }
        return 0xFFFFFFFF;
    }
    return 0xFFFFFFFF;
}

// 0x00AAC7C0 (:2752)
bool object_turret_spawn_node::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (load)
    {
        memset(&node_flags, 0, sizeof(node_flags));
        spawn_prob = 1.0f;
        night_spawn_prob = 1.0f;
        t_info = nullptr;
        child_turret_handle = 0xFFFFFFFF;
        char name[256];
        pblock.get(TURRET_PROP_TINFO_NAME, name);
        pblock.get_optional<float>(TURRET_PROP_ROTATION_LIMIT, rotation_limit);
        pblock.get_optional<float>(SPAWN_NODE_PROP_SPAWN_PROB, spawn_prob);
        spawn_prob = clamp_probability(spawn_prob);
        night_spawn_prob = spawn_prob;
        pblock.get_optional<float>(SPAWN_NODE_PROP_NIGHT_SPAWN_PROB, night_spawn_prob);
        night_spawn_prob = clamp_probability(night_spawn_prob);
        t_info = turret_find_info(name);
        if (t_info == nullptr)
        {
            return false;
        }
        child_turret_handle = find_spawned_turret(get_handle(), pos);
        // Magic number: 0xFF in a handle's zone byte is a managed layer's object.
        if (((get_handle() >> 16) & 0xFF) == 0xFF)
        {
            obj_flags.no_save = true;
            node_flags.roadblock_node = 1;
        }
    }
    return true;
}

// 0x00A1CEC0 (:2802)
bool object_turret_spawn_node::in_spawn_range(const player* pp) const
{
    return obj_flags.streamed;
}

// 0x00A1CED0 (:2809)
void object_turret_spawn_node::do_respawn()
{
}

// 0x00A1CEE0 (:2815)
void object_turret_spawn_node::disable_spawn()
{
    node_flags.disabled = 1;
}

// 0x00A1CEF0 (:2820)
void object_turret_spawn_node::enable_spawn()
{
    node_flags.disabled = 0;
}

// 0x00A1CF00 (:2829)
float object_turret_spawn_node::get_spawn_prob() const
{
    return game_clock_get_time_of_day() != GC_NIGHT ? spawn_prob : night_spawn_prob;
}

// 0x00A3EBE0 (:2841)
void object_turret_spawn_node::evaluate_spawn()
{
    node_flags.evaluated_spawn = 1;
    if (child_turret_handle != 0xFFFFFFFF)
    {
        if (World.object_from_handle<turret>(child_turret_handle) != nullptr)
        {
            return;
        }
        child_turret_handle = 0xFFFFFFFF;
    }
    if (Multiplayer && !multi_is_server())
    {
        return;
    }
    if (fl_rand() <= get_spawn_prob())
    {
        node_flags.spawn_next_frame = 1;
        Spawn_node_flags.spawn_next_frame_turret = 1;
    }
}

// 0x00A1CF20 (:2869)
void object_turret_spawn_node::destroy_notify()
{
    object::destroy_notify();
}

// 0x00A3EC70 (:2878)
bool object_turret_spawn_node::stream(bool load)
{
    if (!load)
    {
        if (obj_flags.destroy_on_stream)
        {
            World.destroy_object(this, false);
        }
        node_flags.evaluated_spawn = 0;
        return true;
    }
    if (!Multiplayer)
    {
        evaluate_spawn();
    }
    else if (multi_is_server())
    {
        node_flags.spawn_next_frame = 1;
        Spawn_node_flags.spawn_next_frame_turret = 1;
    }
    return true;
}

// 0x00A946C0 (:2906)
void object_turret_spawn_node::free_this()
{
    object::free_this();
}

// 0x00A1CF30 (:2913)
void object_turret_spawn_node::hierarchy_notify(hierarchy_notify_code code)
{
    if (code == HN_PARENT_DESTROYED)
    {
        World.destroy_object(this, false);
    }
}

// 0x00A1CF50 (:2924)
bool object_turret_spawn_node::can_spawn_child() const
{
    if (!obj_flags.streamed)
    {
        return false;
    }
    const object* parent_objp = World.object_from_handle(get_parent());
    return parent_objp == nullptr || parents_streamed_in(parent_objp);
}

// 0x00A6DAC0 (:2933)
// Magic number: -1 as the attach tag, for a turret on no vehicle.
unsigned int object_turret_spawn_node::spawn_child_object(unsigned int parent_override)
{
    node_flags.spawn_next_frame = 0;
    if (node_flags.disabled || node_flags.mission_disabled || obj_flags.destroyed)
    {
        return child_turret_handle;
    }
    if (child_turret_handle != 0xFFFFFFFF)
    {
        turret* child = World.object_from_handle<turret>(child_turret_handle);
        if (child != nullptr)
        {
            if (!child->is_killed())
            {
                return child_turret_handle;
            }
            World.destroy_object(child, false);
        }
        child_turret_handle = 0xFFFFFFFF;
    }
    if (parent_override == 0xFFFFFFFF)
    {
        parent_override = get_parent();
    }
    if (parent_override != 0xFFFFFFFF)
    {
        object* parent_objp = World.object_from_handle(parent_override);
        if (parent_objp == nullptr || parent_objp->is_killed())
        {
            return child_turret_handle;
        }
    }
    obj_prop_block props(pos, orient, nullptr);
    props.set<const turret_info*>(TURRET_PROP_TINFO, t_info);
    props.set<int>(TURRET_PROP_ATTACH_TAG, -1);
    props.set<float>(TURRET_PROP_ROTATION_LIMIT, rotation_limit);
    turret* child = World.create_object<turret>(props, parent_override);
    if (child != nullptr)
    {
        child->obj_flags.no_save = true;
        child->obj_flags.destroy_on_stream = true;
        child_turret_handle = child->get_handle();
        child->spawn_node_handle = get_handle();
        // Magic number: 0xFF in a handle's zone byte is a managed layer's object.
        if (((get_handle() >> 16) & 0xFF) == 0xFF)
        {
            child->set_roadblock_flag();
        }
    }
    return child_turret_handle;
}

// 0x00754FF0 (.h:527)
unsigned int object_turret_spawn_node::get_child_turret_handle() const
{
    return child_turret_handle;
}

// 0x00A0D1B0 (.h:533)
bool object_turret_spawn_node::should_spawn() const
{
    return node_flags.spawn_next_frame;
}

// 0x00A0D1C0 (.h:536)
bool object_turret_spawn_node::should_evaluate_spawn() const
{
    return !node_flags.evaluated_spawn;
}

// 0x00A0D1D0 (.h:542)
void object_turret_spawn_node::reevaluate_spawn()
{
    node_flags.evaluated_spawn = 0;
}

// 0x00B1FCF0 (.h:557)
void object_turret_spawn_node::disable_mission_spawn()
{
    node_flags.mission_disabled = 1;
}

// 0x00A0D1E0 (.h:558)
void object_turret_spawn_node::enable_mission_spawn()
{
    node_flags.mission_disabled = 0;
}

// 0x00A0D1F0 (.h:561)
bool object_turret_spawn_node::is_disabled() const
{
    return node_flags.disabled || node_flags.mission_disabled;
}

// 0x00A99530 (:831)
static void do_npc_node_spawning()
{
    for (int i = 0; i < World.get_num_objects<object_npc_spawn_node>(); i++)
    {
        object_npc_spawn_node* node = World.get_object<object_npc_spawn_node>(i);
        if (node->should_spawn() && node->obj_flags.streamed)
        {
            object* parent_objp = World.object_from_handle(node->get_parent());
            if (parent_objp == nullptr || parents_streamed_in(parent_objp))
            {
                node->spawn_child_object();
            }
        }
    }
}

// 0x00A799D0 (:831)
static void do_squad_node_spawning()
{
    for (int i = 0; i < World.get_num_objects<object_squad_spawn_node>(); i++)
    {
        object_squad_spawn_node* node = World.get_object<object_squad_spawn_node>(i);
        if (node->should_spawn())
        {
            node->spawn_child_object(MAP_ELEMENT_INVALID, MAP_ICON_TYPE_NORMAL);
        }
    }
}

// 0x00A4E200 (:843)
static void evaluate_npc_node_spawning(const player* pp)
{
    for (int i = 0; i < World.get_num_objects<object_npc_spawn_node>(); i++)
    {
        object_npc_spawn_node* node = World.get_object<object_npc_spawn_node>(i);
        if (!node->in_spawn_range(pp))
        {
            node->reevaluate_spawn();
        }
        else if (node->should_evaluate_spawn() || Immediate_spawn_flag)
        {
            node->evaluate_spawn();
        }
    }
}

// 0x00A798C0 (:843)
static void evaluate_squad_node_spawning(const player* pp)
{
    for (int i = 0; i < World.get_num_objects<object_squad_spawn_node>(); i++)
    {
        object_squad_spawn_node* node = World.get_object<object_squad_spawn_node>(i);
        if (!node->obj_flags.streamed)
        {
            node->reevaluate_spawn();
            continue;
        }
        if (node->should_evaluate_spawn() || Immediate_spawn_flag)
        {
            node->evaluate_spawn();
        }
        node->do_respawn(false);
    }
}

// 0x00A4E410 (:3005)
static bool check_npc_node_spawning_flags()
{
    for (int i = 0; i < World.get_num_objects<object_npc_spawn_node>(); i++)
    {
        if (World.get_object<object_npc_spawn_node>(i)->should_spawn())
        {
            return true;
        }
    }
    return false;
}

// 0x00A4E4D0 (:3005)
static bool check_squad_node_spawning_flags()
{
    for (int i = 0; i < World.get_num_objects<object_squad_spawn_node>(); i++)
    {
        if (World.get_object<object_squad_spawn_node>(i)->should_spawn())
        {
            return true;
        }
    }
    return false;
}

// 0x00A940A0 (:888)
static void update_tod_spawning(game_clock_time_of_day new_spawn_tod)
{
    if (Last_spawn_tod == new_spawn_tod)
    {
        return;
    }
    Last_spawn_tod = new_spawn_tod;
    for (int i = 0; i < World.get_num_objects<object_squad_spawn_node>(); i++)
    {
        World.get_object<object_squad_spawn_node>(i)->change_tod_status();
    }
}

// 0x00A6C9D0 (:238)
// Civilian squads go when the alert is high, but for those of a managed layer.
static void do_alert_despawn()
{
    const int count = World.get_num_objects<object_squad>();
    for (int i = 0; i < count; i++)
    {
        object_squad* squad = World.get_object<object_squad>(i);
        if (squad->get_squad_team() == HUMAN_TEAM_CIVILIAN && !squad->managed_layer_created())
        {
            squad->request_despawn_all_members(NPC_DESPAWN_TYPE_LOW, nullptr, true);
        }
    }
}

// 0x00A93FF0 (:261)
static void do_alert_respawn()
{
    const int count = World.get_num_objects<object_squad_spawn_node>();
    for (int i = 0; i < count; i++)
    {
        object_squad_spawn_node* node = World.get_object<object_squad_spawn_node>(i);
        if (!node->disabled_at_high_alert())
        {
            node->evaluate_alert_level_spawn();
        }
    }
}

// 0x00A79A40 (:831)
static void do_turret_node_spawning()
{
    const int count = World.get_num_objects<object_turret_spawn_node>();
    for (int i = 0; i < count; i++)
    {
        object_turret_spawn_node* node = World.get_object<object_turret_spawn_node>(i);
        if (node->should_spawn() && node->can_spawn_child())
        {
            node->spawn_child_object(0xFFFFFFFF);
        }
    }
}

// 0x00A4E390 (:843)
static void evaluate_turret_node_spawning(const player* pp)
{
    const int count = World.get_num_objects<object_turret_spawn_node>();
    for (int i = 0; i < count; i++)
    {
        object_turret_spawn_node* node = World.get_object<object_turret_spawn_node>(i);
        if (!node->in_spawn_range(pp))
        {
            node->reevaluate_spawn();
        }
        else if (node->should_evaluate_spawn() || Immediate_spawn_flag)
        {
            node->evaluate_spawn();
        }
    }
}

// 0x00A4E530 (:3005)
static bool check_turret_node_spawning_flags()
{
    for (int i = 0; i < World.get_num_objects<object_turret_spawn_node>(); i++)
    {
        if (World.get_object<object_turret_spawn_node>(i)->should_spawn())
        {
            return true;
        }
    }
    return false;
}

// 0x00AAC9D0 (:3146)
void object_spawn_node_do_frame()
{
    update_tod_spawning(game_clock_get_time_of_day());
    if (!Spawn_retry_period.elapsed())
    {
        return;
    }
    // Magic numbers: a quarter to half a second.
    Spawn_retry_period.set_rand(250, 500);
    const gfm_alert_levels alert = gsm_get_alert_level();
    // Magic number: below alert level 2 a change of level brings squads back, from 2 on civilian squads despawn.
    if ((int)alert < 2)
    {
        if (alert != Last_recorded_alert_level)
        {
            do_alert_respawn();
        }
    }
    else
    {
        do_alert_despawn();
    }
    Last_recorded_alert_level = alert;
    for (int i = 0; i < World.get_num_objects<object_vehicle_spawn_node>(); i++)
    {
        World.get_object<object_vehicle_spawn_node>(i)->evaluate_child_relation();
    }
    if (Player != nullptr)
    {
        evaluate_npc_node_spawning(Player);
        evaluate_vehicle_node_spawning(Player);
        evaluate_squad_node_spawning(Player);
        evaluate_turret_node_spawning(Player);
    }
    if (Spawn_node_flags.spawn_next_frame_npc)
    {
        do_npc_node_spawning();
        Spawn_node_flags.spawn_next_frame_npc = check_npc_node_spawning_flags();
    }
    if (Spawn_node_flags.spawn_next_frame_vehicle)
    {
        do_vehicle_node_spawning();
        Spawn_node_flags.spawn_next_frame_vehicle = check_vehicle_node_spawning_flags();
    }
    if (Spawn_node_flags.spawn_next_frame_squad)
    {
        do_squad_node_spawning();
        Spawn_node_flags.spawn_next_frame_squad = check_squad_node_spawning_flags();
    }
    if (Spawn_node_flags.spawn_next_frame_turret)
    {
        do_turret_node_spawning();
        Spawn_node_flags.spawn_next_frame_turret = check_turret_node_spawning_flags();
    }
}

// 0x00A1CFE0 (:3246)
void object_squad_spawn_node_set_disabled(bool disabled)
{
    Spawn_squad_disabled = disabled;
}

// 0x00A4E590 (:3253)
static void enable_vehicle_mission_spawns()
{
    for (int i = 0; i < World.get_num_objects<object_vehicle_spawn_node>(); i++)
    {
        World.get_object<object_vehicle_spawn_node>(i)->enable_mission_spawn();
    }
}

// 0x00A4E5E0 (:3253)
static void enable_npc_mission_spawns()
{
    for (int i = 0; i < World.get_num_objects<object_npc_spawn_node>(); i++)
    {
        World.get_object<object_npc_spawn_node>(i)->enable_mission_spawn();
    }
}

// 0x00A4E630 (:3253)
static void enable_squad_mission_spawns()
{
    for (int i = 0; i < World.get_num_objects<object_squad_spawn_node>(); i++)
    {
        World.get_object<object_squad_spawn_node>(i)->enable_mission_spawn();
    }
}

// 0x00A4E680 (:3253)
static void enable_turret_mission_spawns()
{
    for (int i = 0; i < World.get_num_objects<object_turret_spawn_node>(); i++)
    {
        World.get_object<object_turret_spawn_node>(i)->enable_mission_spawn();
    }
}

// 0x00A6DD30 (:3264)
void object_spawn_node_enable_mission_spawns()
{
    enable_vehicle_mission_spawns();
    enable_npc_mission_spawns();
    enable_squad_mission_spawns();
    enable_turret_mission_spawns();
    for (int i = 0; i < NUM_HUMAN_TEAMS; i++)
    {
        Team_squads_disabled[i] = false;
    }
}

// 0x00A1CFF0 (:3280)
void object_squad_spawn_disable_for_team(human_teams team)
{
    if (team < NUM_HUMAN_TEAMS && mission_is_active())
    {
        Team_squads_disabled[team] = true;
    }
}

// 0x00A1D010 (:3289)
void object_squad_spawn_enable_for_team(human_teams team)
{
    if (team < NUM_HUMAN_TEAMS && mission_is_active())
    {
        Team_squads_disabled[team] = false;
    }
}

// 0x00A1D030 (:3302)
void object_spawn_set_walker_spawn_status(bool enabled)
{
    Allow_walker_spawn = enabled;
}

// 0x00A3ECE0 (:3308)
// The player's last vehicle by name, unless a flyer or of a team that does not lend vehicles (vehicle class 2, team 3 or 4).
void object_spawn_node_system_save(cfile* save_file)
{
    const char* name = "";
    if (Player->last_vehicle_driven != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(Player->last_vehicle_driven);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && !objp->is_killed())
        {
            const vehicle_info* info = ((vehicle*)objp)->info;
            if (info->vehicle_class != 2 && info->default_team != 3 && info->default_team != 4)
            {
                name = info->name;
            }
        }
    }
    cf_write_string(name, save_file);
    cf_write_bool(false, save_file);
}

// 0x00A1D040 (:3335)
void object_spawn_node_system_load(cfile* load_file)
{
    char name[256];
    name[0] = '\0';
    cf_read_string(name, sizeof(name), load_file);
    cf_read_bool(load_file);
    Player_spawn_vehicle = vehicle_get_info_from_name(name);
}
