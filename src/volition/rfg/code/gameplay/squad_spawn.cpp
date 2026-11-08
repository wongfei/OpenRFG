// Original: volition/rfg/code/gameplay/squad_spawn.cpp
//
// What a spawn node or script fills in to create a squad and its orders (stub_impl.md S9).

#include "volition/rfg/code/gameplay/squad_spawn.h"

#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/object_spawn_region.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"

extern gfm_orders_manager Orders_manager; // 0x0222DE40 (gameplay.cpp)

// 0x00790AE0 (:35): the largest building whose box holds a point, among those within 10 m.
const rfg_mover* find_local_building(const vector& pos)
{
    unsigned int handles[32];
    int count = World.collect_objects<rfg_mover>(pos, 10.0f, handles, 32, nullptr, nullptr);
    const rfg_mover* largest = nullptr;
    float largest_size = 0.0f;
    for (int i = 0; i < count; i++)
    {
        const rfg_mover* mover = handles[i] != 0xFFFFFFFF ? World.object_from_handle<rfg_mover>(handles[i]) : nullptr;
        // Original: no null check, reads through a stale handle.
        vector bmin, bmax;
        mover->get_local_bbox(bmin, bmax);
        if (ix_point_in_box_oriented(pos, mover->pos, mover->orient, bmin, bmax))
        {
            float size = vec_dist_squared(bmin, bmax);
            if (largest_size < size)
            {
                largest = mover;
                largest_size = size;
            }
        }
    }
    return largest;
}

// 0x0060CC70 (.h:26)
squad_creation_parameters::squad_creation_parameters()
{
    reset();
}

// 0x00775590 (:163)
squad_creation_parameters::squad_creation_parameters(const vector& new_squad_pos, const squad_definition* new_squad_def)
{
    reset();
    squad_pos = new_squad_pos;
    squad_def = new_squad_def;
}

// 0x0075F3C0 (:174)
void squad_creation_parameters::reset()
{
    squad_pos.zero();
    squad_orient = IDENTITY_MATRIX;
    squad_def = nullptr;
    npc_flags.reset();
    memset(&hflags, 0, sizeof(hflags));
    memset(&vflags, 0, sizeof(vflags));
    vehicle_hp_mult = 1.0f;
    osrp = nullptr;
    orders_handle = 0xFFFFFFFF;
    veh_spawn_group = nullptr;
    parent_handle = 0xFFFFFFFF;
    spawn_node_handle = 0xFFFFFFFF;
    squad_map_icon = MAP_ELEMENT_INVALID;
    squad_flags.reset();
}

// 0x0075F460 (:193)
bool squad_creation_parameters::is_valid() const
{
    return (!squad_pos.is_null() || osrp != nullptr) && squad_def != nullptr;
}

// 0x007755E0 (:208)
object_squad* squad_creation_parameters::create_squad() const
{
    if (!is_valid())
    {
        return nullptr;
    }
    vector pos = squad_pos;
    if (osrp != nullptr)
    {
        pos = osrp->pos;
        // Magic number: a flyer's squad starts 20 m above the region.
        if (veh_spawn_group != nullptr && veh_spawn_group->v_info_list[0] != nullptr && veh_spawn_group->v_info_list[0]->vehicle_class == VCL_FLYER)
        {
            pos.y += 20.0f;
        }
    }
    int index = squad_definition_index_from_definition(squad_def);
    if (index == -1)
    {
        return nullptr;
    }
    object_squad_props props(pos, squad_orient, squad_def->name, index, orders_handle);
    if (veh_spawn_group != nullptr)
    {
        props.set(SQUAD_PROP_VEHICLE_REF, veh_spawn_group);
    }
    if (spawn_node_handle != 0xFFFFFFFF)
    {
        props.set(SQUAD_PROP_SPAWN_NODE_HANDLE, spawn_node_handle);
    }
    if (squad_map_icon != MAP_ELEMENT_INVALID)
    {
        props.set(SQUAD_PROP_MAP_ICON, squad_map_icon);
    }
    props.set_buf(SQUAD_PROP_SQUAD_FLAGS, (const unsigned char*)&squad_flags, sizeof(squad_flags));
    props.set_buf(SQUAD_PROP_NPC_FLAGS, (const unsigned char*)&npc_flags, sizeof(npc_flags));
    props.set_buf(SQUAD_PROP_HUMAN_FLAGS, (const unsigned char*)&hflags, sizeof(hflags));
    props.set_buf(SQUAD_PROP_VEHICLE_FLAGS, (const unsigned char*)&vflags, sizeof(vflags));
    props.set(SQUAD_PROP_VEHICLE_HP_MULT, vehicle_hp_mult);
    return World.create_object<object_squad>(props, parent_handle);
}

// 0x0060CCA0 (.h:96)
squad_spawn_parameters::squad_spawn_parameters()
{
    reset();
}

// 0x0075F490 (:272)
void squad_spawn_parameters::reset()
{
    squad.reset();
    spawned_squad = nullptr;
    orders.reset();
    created_orders = nullptr;
}

// 0x007B0E00 (:282)
object_squad* squad_spawn_parameters::spawn_squad()
{
    if (spawned_squad != nullptr)
    {
        return spawned_squad;
    }
    if (!squad.is_valid() || create_orders() == nullptr)
    {
        return nullptr;
    }
    spawned_squad = squad.create_squad();
    if (spawned_squad == nullptr)
    {
        destroy_orders();
    }
    return spawned_squad;
}

// 0x007A5E60 (:309)
void squad_spawn_parameters::destroy_squad()
{
    destroy_orders();
    if (spawned_squad != nullptr)
    {
        World.destroy_object(spawned_squad, false);
        spawned_squad = nullptr;
    }
}

// 0x007AC170 (:323)
gfm_orders* squad_spawn_parameters::create_orders()
{
    if (squad.orders_handle != 0xFFFFFFFF)
    {
        created_orders = Orders_manager.get_orders_from_handle(squad.orders_handle);
    }
    if (created_orders != nullptr)
    {
        return created_orders;
    }
    if (squad.squad_def != nullptr && (int)orders.team < 0)
    {
        orders.team = squad_definition_get_team(squad.squad_def);
    }
    if (!orders.is_valid())
    {
        return nullptr;
    }
    squad.orders_handle = Orders_manager.create_new_orders(orders);
    created_orders = squad.orders_handle != 0xFFFFFFFF ? Orders_manager.get_orders_from_handle(squad.orders_handle) : nullptr;
    return created_orders;
}

// 0x0079E580 (:344)
void squad_spawn_parameters::destroy_orders()
{
    if (created_orders != nullptr)
    {
        Orders_manager.destroy_orders(created_orders);
        created_orders = nullptr;
    }
}

// 0x0075F4E0 (:357)
bool squad_spawn_parameters::spawn_squad_members()
{
    return spawned_squad != nullptr && spawned_squad->spawn_members(true, squad.osrp);
}
