// Original: volition/rfg/code/gameplay/activities/reinforcement_squad.cpp
//
// gameflow.md GF2. One squad sent after a target: follows its orders, is told where the target is, and despawns once it
// has fallen too far behind or lost the target for too long.

#include "volition/rfg/code/gameplay/activities/reinforcement_squad.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/percept.h"
#include "volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/squad_spawn.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/vlib/code/math/vector3.h"

// Magic numbers: 325 m squared is the squad's leash; 50 m squared always counts as engaging.
static const float SQUAD_LEASH_DIST_SQRD = 105625.0f;
static const float SQUAD_ENGAGE_DIST_SQRD = 2500.0f;
// Magic numbers: the despawn timeout restarts at 20 s and is kept at 10 s or more while the squad is doing well.
static const int DESPAWN_TIMEOUT_MS = 20000;
static const int DESPAWN_TIMEOUT_MIN_MS = 10000;

// 0x00771B90 (:31), squad in EDI
static bool squad_has_been_partially_destroyed(const object_squad* squad)
{
    if (squad->simulated())
    {
        return false;
    }
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        const unsigned int handle = squad->member_array[i];
        const npc* np = handle != 0xFFFFFFFF ? World.object_from_handle<npc>(handle) : nullptr;
        if (np == nullptr || np->is_killed())
        {
            return true;
        }
    }
    return false;
}

// 0x0077E2D0 (reinforcement_squad.h:24)
reinforcement_squad::reinforcement_squad()
{
    reset();
}

// 0x007710E0
reinforcement_squad::~reinforcement_squad()
{
}

// 0x00754160 (reinforcement_squad.h:56)
bool reinforcement_squad::squad_in_use() const
{
    return squad_handle != 0xFFFFFFFF;
}

// 0x007B4210 (:57)
bool reinforcement_squad::spawn_squad(unsigned int new_target_handle, squad_spawn_parameters& spawn_params, bool use_reinforcement_updates, bool destroy_partial)
{
    if (squad_handle != 0xFFFFFFFF && World.object_from_handle<object_squad>(squad_handle) != nullptr)
    {
        return false;
    }
    if (spawn_params.spawned_squad != nullptr)
    {
        return false;
    }
    release_and_reset();
    reinforcement_squad_flags.squad_spawned = 1;
    object* target = World.object_from_handle(new_target_handle);
    if (target == nullptr)
    {
        return false;
    }
    target_handle = target->get_handle();
    reinforcement_squad_flags.target_is_player = player_get_local_player() == target ? 1 : 0;
    reinforcement_squad_flags.reinforcement_update = use_reinforcement_updates ? 1 : 0;
    reinforcement_squad_flags.destroy_partial_squads = destroy_partial ? 1 : 0;
    if (spawn_params.spawn_squad() != nullptr && spawn_params.created_orders != nullptr && spawn_params.spawned_squad != nullptr)
    {
        squad_handle = spawn_params.spawned_squad->get_handle();
        orders_handle = spawn_params.created_orders->get_orders_handle();
        squad_lifetime.set(0);
        squad_deathtime.invalidate();
        return true;
    }
    return false;
}

// 0x007A3130 (:111)
void reinforcement_squad::update_orders(const object* target_objp)
{
    if (target_objp == nullptr)
    {
        return;
    }
    gfm_orders* orders = orders_handle != 0xFFFFFFFF ? Orders_manager.get_orders_from_handle(orders_handle) : nullptr;
    if (orders != nullptr)
    {
        orders->update_orders_position(reinforcement_squad_flags.reinforcement_update != 0 ? reinforcement_get_player_pos() : target_objp->pos);
        return;
    }
    object_squad* squad = squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_handle) : nullptr;
    if (squad != nullptr)
    {
        despawn_squad(squad, NPC_DESPAWN_TYPE_LOW);
    }
}

// 0x007AF540 (:140)
void reinforcement_squad::do_frame()
{
    if (squad_handle == 0xFFFFFFFF)
    {
        return;
    }
    if (reinforcement_squad_flags.squad_despawned != 0)
    {
        if (!squad_deathtime.valid() && squad_despawned())
        {
            squad_deathtime.set(0);
        }
        return;
    }
    object* target = World.object_from_handle(target_handle);
    if (target == nullptr || target->is_killed())
    {
        release_and_reset();
    }
    object_squad* squad = squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_handle) : nullptr;
    if (squad == nullptr)
    {
        swap_to_dead_state();
        return;
    }
    if (reinforcement_squad_flags.destroy_partial_squads != 0 && squad_has_been_partially_destroyed(squad))
    {
        despawn_squad(NPC_DESPAWN_TYPE_LOW);
        return;
    }
    update_orders(target);
    if (squad->simulated())
    {
        do_frame_simulated(squad, target);
    }
    else
    {
        do_frame_spawned(squad, target);
    }
}

// 0x007A9DB0 (:190)
void reinforcement_squad::swap_to_dead_state()
{
    squad_handle = 0xFFFFFFFF;
    if (orders_handle != 0xFFFFFFFF)
    {
        gfm_orders* orders = Orders_manager.get_orders_from_handle(orders_handle);
        if (orders != nullptr)
        {
            Orders_manager.destroy_orders(orders);
        }
    }
    reinforcement_squad_flags.squad_despawned = 1;
    squad_lifetime.set(0);
    despawn_timeout.invalidate();
}

// 0x0079B400 (:209)
void reinforcement_squad::do_frame_spawned(object_squad* squad, object* target_objp)
{
    if (squad == nullptr || target_objp == nullptr)
    {
        return;
    }
    if (squad->veh_spawn_group != nullptr)
    {
        const vehicle* vp = squad->squad_vehicle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(squad->squad_vehicle) : nullptr;
        if (vp == nullptr)
        {
            reinforcement_squad_flags.vehicle_killed = 1;
        }
        else
        {
            vehicle_handle = vp->get_handle();
            reinforcement_squad_flags.vehicle_killed = vp->is_killed() ? 1 : 0;
        }
    }
    if (target_objp->is_killed())
    {
        despawn_squad(squad, NPC_DESPAWN_TYPE_LOW);
        return;
    }
    update_target_beliefs(squad, target_objp);
    if (vec_dist_squared(squad->pos, target_objp->pos) <= SQUAD_LEASH_DIST_SQRD)
    {
        if (!despawn_timeout.valid())
        {
            despawn_timeout.set(DESPAWN_TIMEOUT_MS);
            return;
        }
        if (can_engage_target(squad, target_objp))
        {
            if (despawn_timeout.time_until() < DESPAWN_TIMEOUT_MIN_MS)
            {
                despawn_timeout.set(DESPAWN_TIMEOUT_MIN_MS);
            }
            dist_to_target_sqrd = vec_dist_squared(squad->pos, target_objp->pos);
            return;
        }
        if (is_closing_to_target(squad->pos, target_objp->pos))
        {
            if (despawn_timeout.time_until() < DESPAWN_TIMEOUT_MIN_MS)
            {
                despawn_timeout.set(DESPAWN_TIMEOUT_MIN_MS);
            }
            return;
        }
        if (!despawn_timeout.elapsed())
        {
            return;
        }
    }
    despawn_squad(squad, NPC_DESPAWN_TYPE_LOW);
}

// 0x0079B570 (:270)
void reinforcement_squad::despawn_squad(npc_despawn_type despawn_type)
{
    object_squad* squad = squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_handle) : nullptr;
    if (squad != nullptr)
    {
        despawn_squad(squad, despawn_type);
    }
}

// 0x0078A2C0 (:281)
void reinforcement_squad::despawn_squad(object_squad* squad, npc_despawn_type despawn_type)
{
    if (squad == nullptr || squad_handle != squad->get_handle())
    {
        return;
    }
    despawning_members.clear();
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        if (squad->member_array[i] != 0xFFFFFFFF)
        {
            despawning_members.add(squad->member_array[i]);
        }
    }
    squad->request_despawn_all_members(despawn_type, nullptr, true);
    reinforcement_squad_flags.squad_despawned = 1;
}

// 0x00771C00 (:303)
void reinforcement_squad::update_target_beliefs(object_squad* squad, human* target_hp)
{
    if (human_teams_get_disposition(target_hp, squad->get_squad_team()) != TEAM_DISPOSITION_HOSTILE)
    {
        return;
    }
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        const unsigned int handle = squad->member_array[i];
        npc* np = handle != 0xFFFFFFFF ? World.object_from_handle<npc>(handle) : nullptr;
        if (np != nullptr && !np->is_killed())
        {
            np->get_ai()->perception.force_search(target_hp, target_hp->pos);
        }
    }
}

// 0x00771C90 (:326)
void reinforcement_squad::update_target_beliefs(object_squad* squad, object_squad* target_squad)
{
    // Original quirk kept: the leader of the squad itself, not of its target, counts as the target when it has one.
    npc* leader = squad->get_leader();
    if (leader != nullptr)
    {
        update_target_beliefs(squad, static_cast<human*>(leader));
        return;
    }
    for (int i = 0; i < target_squad->member_array.size(); i++)
    {
        const unsigned int handle = target_squad->member_array[i];
        human* hp = handle != 0xFFFFFFFF ? World.object_from_handle<human>(handle) : nullptr;
        if (hp != nullptr)
        {
            update_target_beliefs(squad, hp);
            return;
        }
    }
}

// 0x0079B5A0 (:348)
void reinforcement_squad::release_and_reset()
{
    object_squad* squad = squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_handle) : nullptr;
    if (squad != nullptr)
    {
        despawn_squad(squad, NPC_DESPAWN_TYPE_LOW);
    }
    reset();
}

// 0x0075BAB0 (:355)
int reinforcement_squad::get_deathtime() const
{
    if (reinforcement_squad_flags.squad_spawned == 0)
    {
        return 0x7FFFFFFF;
    }
    return squad_deathtime.valid() ? squad_deathtime.time_since() : 0;
}

// 0x00771D10 (:373)
bool reinforcement_squad::get_squad_position(vector& squad_pos) const
{
    const object_squad* squad = squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_handle) : nullptr;
    if (squad != nullptr)
    {
        squad_pos = squad->pos;
        return true;
    }
    for (int i = 0; i < despawning_members.size(); i++)
    {
        const unsigned int handle = despawning_members[i];
        const npc* np = handle != 0xFFFFFFFF ? World.object_from_handle<npc>(handle) : nullptr;
        if (np != nullptr)
        {
            squad_pos = np->pos;
            return true;
        }
    }
    return false;
}

// 0x00771DA0 (:399)
bool reinforcement_squad::squad_despawned() const
{
    if (squad_deathtime.valid())
    {
        return true;
    }
    if (squad_handle != 0xFFFFFFFF && World.object_from_handle<object_squad>(squad_handle) != nullptr)
    {
        return false;
    }
    for (int i = 0; i < despawning_members.size(); i++)
    {
        const unsigned int handle = despawning_members[i];
        const npc* np = handle != 0xFFFFFFFF ? World.object_from_handle<npc>(handle) : nullptr;
        if (np != nullptr && !np->is_killed())
        {
            return false;
        }
    }
    return true;
}

// 0x00771E30 (:430)
void reinforcement_squad::update_target_beliefs(object_squad* squad, object* target_objp)
{
    if (!belief_refresh.elapsed())
    {
        return;
    }
    // Magic numbers: 250 to 500 ms between refreshes.
    belief_refresh.set_rand(250, 500);
    if (target_objp == nullptr)
    {
        return;
    }
    if (target_objp->get_obj_type() == OT_HUMAN)
    {
        update_target_beliefs(squad, static_cast<human*>(target_objp));
    }
    else if (target_objp->get_obj_type() == OT_SQUAD)
    {
        update_target_beliefs(squad, static_cast<object_squad*>(target_objp));
    }
}

// 0x00771EA0 (:456)
bool reinforcement_squad::can_engage_target(object_squad* squad, human* target_hp) const
{
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        const unsigned int handle = squad->member_array[i];
        const npc* np = handle != 0xFFFFFFFF ? World.object_from_handle<npc>(handle) : nullptr;
        if (np != nullptr && np->get_ai()->perception.enemy_in_visual_range(target_hp))
        {
            return true;
        }
    }
    return false;
}

// 0x00771F20 (:477)
bool reinforcement_squad::can_engage_target(object_squad* squad, object_squad* target_squad) const
{
    // Original quirk kept: the squad's own leader again, as in update_target_beliefs.
    npc* leader = squad->get_leader();
    if (leader != nullptr)
    {
        return can_engage_target(squad, static_cast<human*>(leader));
    }
    for (int i = 0; i < target_squad->member_array.size(); i++)
    {
        const unsigned int handle = target_squad->member_array[i];
        human* hp = handle != 0xFFFFFFFF ? World.object_from_handle<human>(handle) : nullptr;
        if (hp != nullptr)
        {
            return can_engage_target(squad, hp);
        }
    }
    return false;
}

// 0x0075BAE0 (:499)
bool reinforcement_squad::is_closing_to_target(const vector& squad_pos, const vector& target_pos)
{
    const float dist = vec_dist_squared(squad_pos, target_pos);
    if (dist_to_target_sqrd < dist)
    {
        return false;
    }
    dist_to_target_sqrd = dist;
    return true;
}

// 0x00771FA0 (:516)
bool reinforcement_squad::can_engage_player(object_squad* squad) const
{
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        const unsigned int handle = squad->member_array[i];
        const npc* np = handle != 0xFFFFFFFF ? World.object_from_handle<npc>(handle) : nullptr;
        if (np != nullptr && np->get_ai()->perception.any_enemy_player_in_los())
        {
            return true;
        }
    }
    return false;
}

// 0x00772010 (:537)
bool reinforcement_squad::can_engage_target(object_squad* squad, object* target_objp) const
{
    if (vec_dist_squared(squad->pos, target_objp->pos) < SQUAD_ENGAGE_DIST_SQRD)
    {
        return true;
    }
    if (reinforcement_squad_flags.target_is_player != 0)
    {
        return can_engage_player(squad);
    }
    if (target_objp->get_obj_type() == OT_HUMAN)
    {
        return can_engage_target(squad, static_cast<human*>(target_objp));
    }
    if (target_objp->get_obj_type() == OT_SQUAD)
    {
        return can_engage_target(squad, static_cast<object_squad*>(target_objp));
    }
    return true;
}

// 0x0078A330 (:569)
void reinforcement_squad::do_frame_simulated(object_squad* squad, object* target_objp)
{
    if (squad == nullptr)
    {
        return;
    }
    belief_refresh.set(0);
    despawn_timeout.invalidate();
    if (SQUAD_LEASH_DIST_SQRD < vec_dist_squared(squad->pos, target_objp->pos))
    {
        despawn_squad(squad, NPC_DESPAWN_TYPE_LOW);
    }
}

// 0x007720A0 (:587)
void reinforcement_squad::reset()
{
    dist_to_target_sqrd = 3.4028235e+38f;
    squad_handle = 0xFFFFFFFF;
    vehicle_handle = 0xFFFFFFFF;
    orders_handle = 0xFFFFFFFF;
    target_handle = 0xFFFFFFFF;
    despawn_timeout.invalidate();
    squad_lifetime.invalidate();
    squad_deathtime.set(0);
    belief_refresh.set(0);
    despawning_members.clear();
    reinforcement_squad_flags = {};
}
