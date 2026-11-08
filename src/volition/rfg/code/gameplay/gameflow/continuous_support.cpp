// Original: volition/rfg/code/gameplay/gameflow/continuous_support.cpp
//
// gameflow.md GF2. Squads that scripts keep alive: when one dies it respawns after a delay at a registered region or the
// closest region far enough from the player, with the orders the script last gave it.

#include "volition/rfg/code/gameplay/gameflow/continuous_support.h"
#include "volition/rfg/code/gameplay/gameflow/continuous_support_internal.h"

#include "volition/rfg/code/gameplay/squad_spawn.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/object_spawn_region.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"

// Magic numbers: a respawn keeps 30 m from the player by default; 16 squads at most.
static const float DEFAULT_MIN_SPAWN_DISTANCE_SQRD = 900.0f; // 0x012DBC58
static object_pool<continuous_support_squad, 16, 1> Support_squad_pool; // 0x02244990

// 0x0078BA60 (:34), spawn_node_handle in EDI
static continuous_support_squad* find_support_squad(unsigned int spawn_node_handle)
{
    csspinlock_scope lock(Support_squad_pool.thread_access);
    for (continuous_support_squad* squad = Support_squad_pool.get_first(0); squad != nullptr; squad = Support_squad_pool.get_next(squad))
    {
        if (spawn_node_handle == squad->spawn_node_handle)
        {
            return squad;
        }
    }
    return nullptr;
}

// 0x007A45C0 (:51)
static continuous_support_squad* create_support_squad()
{
    continuous_support_squad* squad = Support_squad_pool.alloc(0);
    if (squad != nullptr)
    {
        squad->reset();
    }
    return squad;
}

// 0x007A4610 (:64)
static void add_support_squad(const object_squad* squad, const object_squad_spawn_node* squad_node, int new_respawn_delay_ms)
{
    if (squad_node == nullptr)
    {
        return;
    }
    continuous_support_squad* support = find_support_squad(squad_node->get_handle());
    if (support == nullptr)
    {
        support = create_support_squad();
    }
    if (support != nullptr && !support->update_squad_info(squad, squad_node, new_respawn_delay_ms))
    {
        support->release_and_reset();
        Support_squad_pool.free(support);
    }
}

// 0x007A4570 (continuous_support_internal.h:23)
continuous_support_squad::continuous_support_squad()
{
    reset();
}

// 0x0079C790 (:94)
bool continuous_support_squad::pick_spawn_location(vector& spawn_pos, human_teams team, bool vehicle_spawn)
{
    if (spawn_region_handle != 0xFFFFFFFF)
    {
        const object* region = World.object_from_handle(spawn_region_handle);
        if (region != nullptr && region->get_obj_type() == OT_SPAWN_REGION)
        {
            const player* pp = player_get_local_player();
            if (pp != nullptr && min_region_distance_sqrd <= vec_dist_squared(pp->pos, region->pos))
            {
                spawn_pos = region->pos;
                return true;
            }
            return false;
        }
    }
    return pick_spawn_location_default(spawn_pos, team, vehicle_spawn);
}

// 0x0078BAC0 (:123)
bool continuous_support_squad::pick_spawn_location_default(vector& spawn_pos, human_teams team, bool vehicle_spawn)
{
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return false;
    }
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
        const float dist = vec_dist_squared(pp->pos, region->pos);
        if (min_region_distance_sqrd <= dist && dist < best_dist)
        {
            best = region;
            best_dist = dist;
        }
    }
    if (best == nullptr)
    {
        return false;
    }
    spawn_pos = best->pos;
    return true;
}

// 0x0079C820 (:162)
void continuous_support_squad::reset()
{
    spawn_node_handle = 0xFFFFFFFF;
    orders_type = GFM_ORDERS_TYPE_INVALID;
    spawn_region_handle = 0xFFFFFFFF;
    orders_target = 0xFFFFFFFF;
    min_region_distance_sqrd = DEFAULT_MIN_SPAWN_DISTANCE_SQRD;
    respawn_delay_ms = 0;
    base_squad_info.release_and_reset();
    dead_time.set(0);
}

// 0x0079C860 (:174)
void continuous_support_squad::release_and_reset()
{
    base_squad_info.release_and_reset();
    reset();
}

// 0x00773570 (:181)
bool continuous_support_squad::update_squad_info(const object_squad* squad, const object_squad_spawn_node* squad_node, int new_respawn_delay_ms)
{
    if (squad_node == nullptr)
    {
        return false;
    }
    spawn_node_handle = squad_node->get_handle();
    respawn_delay_ms = new_respawn_delay_ms < 0 ? 0 : new_respawn_delay_ms;
    orders_target = squad_node->get_handle();
    orders_type = GFM_ORDERS_TYPE_WAIT;
    if (squad != nullptr && base_squad_info.squad_handle == 0xFFFFFFFF)
    {
        const gfm_orders* orders = squad->get_orders();
        if (orders != nullptr)
        {
            base_squad_info.squad_handle = squad->get_handle();
            base_squad_info.vehicle_handle = squad->squad_vehicle;
            base_squad_info.orders_handle = orders->get_orders_handle();
            base_squad_info.target_handle = orders->get_target_object_handle();
        }
    }
    return true;
}

// 0x007B7090 (:220)
void continuous_support_squad::do_frame()
{
    base_squad_info.do_frame();
    if (base_squad_info.squad_handle == 0xFFFFFFFF)
    {
        if (respawn_delay_ms <= dead_time.time_since())
        {
            respawn_squad();
        }
    }
    else
    {
        dead_time.set(0);
    }
}

// 0x007B4540 (:244)
void continuous_support_squad::respawn_squad()
{
    squad_spawn_parameters params;
    base_squad_info.release_and_reset();
    if (spawn_node_handle == 0xFFFFFFFF)
    {
        return;
    }
    object_squad_spawn_node* node = World.object_from_handle<object_squad_spawn_node>(spawn_node_handle);
    if (node == nullptr)
    {
        return;
    }
    const object* target = World.object_from_handle(orders_target);
    const squad_definition* def = node->get_info();
    if (target == nullptr || def == nullptr)
    {
        return;
    }
    vector spawn_pos;
    if (!pick_spawn_location(spawn_pos, def->squad_team, node->get_veh_info() != nullptr))
    {
        return;
    }
    params.orders.orders_type = orders_type;
    params.orders.object_target = orders_target;
    params.orders.location_target = target->pos;
    params.squad.veh_spawn_group = node->get_veh_info();
    params.squad.squad_flags.disallow_reassignment = 1;
    params.squad.squad_pos = spawn_pos;
    vector dir(target->pos.x - spawn_pos.x, 0.0f, target->pos.z - spawn_pos.z);
    dir.normalize_safe();
    params.squad.squad_orient.make_quick(dir);
    params.squad.squad_def = def;
    base_squad_info.spawn_squad(orders_target, params, false, false);
    if (base_squad_info.squad_handle != 0xFFFFFFFF)
    {
        node->adopt_child_squad(base_squad_info.squad_handle);
        dead_time.set(0);
    }
}

// 0x007A4660 (:307)
void cont_support_level_init_pre()
{
    Support_squad_pool.clear(0);
}

// 0x007B70D0 (:314)
void cont_support_do_frame()
{
    csspinlock_scope lock(Support_squad_pool.thread_access);
    for (continuous_support_squad* squad = Support_squad_pool.get_first(0); squad != nullptr; squad = Support_squad_pool.get_next(squad))
    {
        squad->do_frame();
    }
}

// 0x007A46A0 (:327)
void cont_support_add_squad(const object_squad_spawn_node* squad_node, int new_respawn_delay_ms)
{
    if (squad_node == nullptr)
    {
        return;
    }
    const object_squad* squad = squad_node->get_child_squad_handle() != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_node->get_child_squad_handle()) : nullptr;
    add_support_squad(squad, squad_node, new_respawn_delay_ms);
}

// 0x007A4720 (:340)
void cont_support_add_squad(const object_squad* squad, int new_respawn_delay_ms)
{
    if (squad == nullptr || squad->spawn_node_handle == 0xFFFFFFFF)
    {
        return;
    }
    const object_squad_spawn_node* node = World.object_from_handle<object_squad_spawn_node>(squad->spawn_node_handle);
    if (node != nullptr)
    {
        add_support_squad(squad, node, new_respawn_delay_ms);
    }
}

// 0x0079C8B0 (:354)
void cont_support_remove_squad(unsigned int squad_node_handle)
{
    csspinlock_scope lock(Support_squad_pool.thread_access);
    continuous_support_squad* squad = Support_squad_pool.get_first(0);
    while (squad != nullptr)
    {
        continuous_support_squad* next = Support_squad_pool.get_next(squad);
        if (squad_node_handle == squad->spawn_node_handle)
        {
            squad->release_and_reset();
            Support_squad_pool.free(squad);
        }
        squad = next;
    }
}

// 0x007A4760 (:370)
void cont_support_remove_all_squads()
{
    {
        csspinlock_scope lock(Support_squad_pool.thread_access);
        for (continuous_support_squad* squad = Support_squad_pool.get_first(0); squad != nullptr; squad = Support_squad_pool.get_next(squad))
        {
            squad->release_and_reset();
        }
    }
    Support_squad_pool.clear(0);
}

// 0x0078BBC0 (:384)
void cont_support_register_region(const object_squad_spawn_node* squad_node, unsigned int spawn_region_handle, float min_spawn_distance)
{
    if (squad_node == nullptr)
    {
        return;
    }
    continuous_support_squad* squad = find_support_squad(squad_node->get_handle());
    if (squad != nullptr)
    {
        squad->spawn_region_handle = spawn_region_handle;
        squad->min_region_distance_sqrd = min_spawn_distance * min_spawn_distance;
    }
}

// 0x0078BC00 (:402)
void cont_support_update_orders(const object_squad_spawn_node* squad_node, gfm_orders_types orders_type, unsigned int target_handle, unsigned int orders_handle)
{
    // Magic number: 18 kinds of orders from GFM_ORDERS_TYPE_DEFEND on.
    if (squad_node == nullptr || (unsigned int)(orders_type - GFM_ORDERS_TYPE_DEFEND) >= 0x12 || target_handle == 0xFFFFFFFF)
    {
        return;
    }
    continuous_support_squad* squad = find_support_squad(squad_node->get_handle());
    if (squad != nullptr)
    {
        squad->orders_type = orders_type;
        squad->orders_target = target_handle;
        squad->base_squad_info.target_handle = target_handle;
        if (orders_handle != 0xFFFFFFFF)
        {
            squad->base_squad_info.orders_handle = orders_handle;
        }
    }
}
