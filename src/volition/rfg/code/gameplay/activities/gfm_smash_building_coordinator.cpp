// Original: volition/rfg/code/gameplay/activities/gfm_smash_building_coordinator.cpp
//
// gameflow.md GF2. EDF tanks sent to knock down buildings near the player, five orders at most.

#include "volition/rfg/code/gameplay/activities/gfm_smash_building_coordinator.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/squad_spawn.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_spawn_region.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector3.h"

#include <stdlib.h>

extern gfm_smash_building_coordinator Smash_building_coordinator; // 0x016572EC (gameplay.cpp)
extern cartographer Cartographer; // 0x02BB71F8 (minimap.cpp)

// Magic numbers: 150 to 350 m around a building, squared.
static const float MIN_TANK_SPAWN_DIST_SQRD = 22500.0f; // 0x01657310
static const float MAX_TANK_SPAWN_DIST_SQRD = 122500.0f; // 0x01657314
// Named for its squared use, but the road search grows the box by this many meters.
static const float NODE_VISIT_DISTANCE_SQRD = 100.0f; // 0x012DB87C
static const char* const DEFAULT_SMASH_BUILDING_VECHICLE_GROUP_NAME = "Tanks"; // 0x0165C930
gfm_orders_flags ACTIVITY_ORDERS_FLAGS(false, false); // 0x02231B21

// 0x0075B5D0 (:58), list_size in ESI
static int pick_rand_index(unsigned int list_size)
{
    if (list_size < 2)
    {
        return 0;
    }
    const unsigned int value = vrand();
    return list_size == 2 ? (int)(value & 1) : (int)(value % list_size);
}

// 0x00788B00 (:80)
static const object_spawn_region* find_spawn_region(const object* building_objp)
{
    const object_spawn_region* regions[32];
    int count = 0;
    const int num = World.get_num_objects<object_spawn_region>();
    for (int i = 0; i < num; i++)
    {
        const object_spawn_region* region = World.get_object<object_spawn_region>(i);
        if (!region->team_allowed_to_spawn(HUMAN_TEAM_EDF, true))
        {
            continue;
        }
        const float dist = vec_xz_dist_squared(building_objp->pos, region->pos);
        if (MIN_TANK_SPAWN_DIST_SQRD <= dist && dist <= MAX_TANK_SPAWN_DIST_SQRD && MIN_TANK_SPAWN_DIST_SQRD <= player_find_shortest_player_dist_sqrd(region->pos))
        {
            // rfg.exe's Int3 message for a full list is dropped: it just stops adding.
            if (count < 32)
            {
                regions[count++] = region;
            }
        }
    }
    return count > 0 ? regions[pick_rand_index((unsigned int)count)] : nullptr;
}

// 0x00771650 (:114), building_objp in EDI
static vector find_spawn_point_road(const object* building_objp, const farray<object const*, 16>& building_list)
{
    vector bmin;
    vector bmax;
    building_objp->get_bbox(bmin, bmax);
    for (int i = 0; i < building_list.size(); i++)
    {
        const object* other = building_list[i];
        if (other != building_objp)
        {
            vector other_min;
            vector other_max;
            other->get_bbox(other_min, other_max);
            bbox_expand(bmin, bmax, other_min, other_max);
        }
    }
    bbox_grow(bmin, bmax, vector(NODE_VISIT_DISTANCE_SQRD, 0.0f, NODE_VISIT_DISTANCE_SQRD));
    bbox_grow(bmin, bmax, vector(-NODE_VISIT_DISTANCE_SQRD, 0.0f, -NODE_VISIT_DISTANCE_SQRD));
    vector point = ZERO_VECTOR;
    if (!rfg_find_closest_pt_on_road_to_bbox(point, bmin, bmax, building_objp->pos))
    {
        // No road: one of the box's eight corners, on the ground.
        bbox_find_ith_vertex(point, rand() % 8, bmin, bmax);
        rfg_snap_to_ground(point);
    }
    return point;
}

// 0x007717A0 (:157), building_list in EDI
static const object* select_target_building(const farray<object const*, 16>& building_list)
{
    return building_list.size() == 0 ? nullptr : building_list[pick_rand_index((unsigned int)building_list.size())];
}

// 0x007AF2B0 (gfm_smash_building_coordinator.h:40)
gfm_smash_building_coordinator::gfm_smash_building_coordinator()
{
    level_init();
}

// 0x00771640
gfm_smash_building_coordinator::~gfm_smash_building_coordinator()
{
}

// 0x007B39B0 (:177)
void gfm_smash_building_coordinator::add_new_orders(int index, const farray<object const*, 16>& building_list)
{
    (void)index;
    static const vehicle_spawn_group_info* DEFAULT_SMASH_BUILDING_VEHICLE_GROUP = sog_get_vehicle_group(DEFAULT_SMASH_BUILDING_VECHICLE_GROUP_NAME); // 0x02273E34
    squad_spawn_parameters params;
    if (DEFAULT_SMASH_BUILDING_VEHICLE_GROUP == nullptr)
    {
        return;
    }
    const object* building = select_target_building(building_list);
    if (building == nullptr)
    {
        return;
    }
    params.orders.orders_type = GFM_ORDERS_TYPE_SMASH;
    params.orders.location_target = building->pos;
    params.orders.ofp = ACTIVITY_ORDERS_FLAGS;
    params.orders.object_target = building->get_handle();
    params.squad.squad_def = squad_definition_from_type(GFM_ORDERS_TYPE_SMASH, HUMAN_TEAM_EDF);
    params.squad.veh_spawn_group = DEFAULT_SMASH_BUILDING_VEHICLE_GROUP;
    params.squad.osrp = find_spawn_region(building);
    params.squad.squad_flags.disallow_reassignment = 1;
    params.squad.squad_pos = params.squad.osrp != nullptr ? params.squad.osrp->pos : find_spawn_point_road(building, building_list);
    if (params.spawn_squad() == nullptr)
    {
        return;
    }
    if (params.created_orders == nullptr)
    {
        params.destroy_squad();
        return;
    }
    if (params.spawned_squad != nullptr)
    {
        params.spawned_squad->spawn_members(true, params.squad.osrp);
    }
    orders_list.add(params.created_orders->get_orders_handle());
    params.created_orders->populate_building_list(building_list);
}

// 0x007A9BB0 (:233)
void gfm_smash_building_coordinator::destroy_old_orders()
{
    const int num = orders_list.size();
    if (num == 0)
    {
        return;
    }
    // The first orders with no squad, else the newest.
    const int last = num - 1;
    int index = last;
    for (int i = 0; i < num; i++)
    {
        const gfm_orders* orders = Orders_manager.get_orders_from_handle(orders_list[i]);
        if (orders == nullptr || orders->get_assigned_squad_object() == nullptr)
        {
            index = i;
            break;
        }
    }
    const unsigned int handle = orders_list[index];
    if (index != last)
    {
        orders_list[index] = orders_list[last];
    }
    Orders_manager.destroy_orders(handle);
    orders_list.remove_unsorted(last);
}

// 0x007A9C40 (:283)
void gfm_smash_building_coordinator::level_init()
{
    for (int i = 0; i < orders_list.size(); i++)
    {
        if (orders_list[i] != 0xFFFFFFFF)
        {
            Orders_manager.destroy_orders(orders_list[i]);
        }
    }
    orders_list.clear();
    for (int i = 0; i < orders_list.capacity(); i++)
    {
        orders_list.elements()[i] = 0xFFFFFFFF;
    }
}

// 0x007A9CC0 (:299)
bool gfm_smash_building_coordinator::do_frame()
{
    if (orders_list.size() <= 0)
    {
        return false;
    }
    int i = 0;
    while (i < orders_list.size())
    {
        gfm_orders* orders = orders_list[i] != 0xFFFFFFFF ? Orders_manager.get_orders_from_handle(orders_list[i]) : nullptr;
        if (orders->any_valid_building())
        {
            i++;
            continue;
        }
        if (orders_list[i] != 0xFFFFFFFF)
        {
            Orders_manager.destroy_orders(orders_list[i]);
        }
        orders_list.remove_unsorted(i);
    }
    // rfg.exe renders the global coordinator, the only one there is.
    Smash_building_coordinator.render_icons();
    return false;
}

// 0x007A2F40 (:327)
int gfm_smash_building_coordinator::get_filled_orders_count() const
{
    int count = 0;
    for (int i = 0; i < orders_list.size(); i++)
    {
        const gfm_orders* orders = orders_list[i] != 0xFFFFFFFF ? Orders_manager.get_orders_from_handle(orders_list[i]) : nullptr;
        const unsigned int handle = orders->get_orders_handle();
        const int num = World.get_num_objects<object_squad>();
        for (int j = 0; j < num; j++)
        {
            const object_squad* squad = World.get_object<object_squad>(j);
            if (squad->base_orders_handle == handle || squad->assigned_orders_handle == handle)
            {
                count++;
                break;
            }
        }
    }
    return count;
}

// 0x007A3020 (:346)
bool gfm_smash_building_coordinator::is_something_else_attacking(unsigned int mover_handle, gfm_orders* ignore_orders)
{
    for (int i = 0; i < orders_list.size(); i++)
    {
        gfm_orders* orders = orders_list[i] != 0xFFFFFFFF ? Orders_manager.get_orders_from_handle(orders_list[i]) : nullptr;
        if (ignore_orders == orders)
        {
            continue;
        }
        const unsigned int handle = orders->get_orders_handle();
        const int num = World.get_num_objects<object_squad>();
        for (int j = 0; j < num; j++)
        {
            const object_squad* squad = World.get_object<object_squad>(j);
            if (squad->base_orders_handle == handle || squad->assigned_orders_handle == handle)
            {
                const object_squad* assigned = orders->get_assigned_squad_object();
                const npc* leader = assigned != nullptr ? assigned->get_leader() : nullptr;
                if (leader != nullptr && leader->get_ai()->desired_destination_object == mover_handle)
                {
                    return true;
                }
                break;
            }
        }
    }
    return false;
}

// 0x0079B040 (:379)
void gfm_smash_building_coordinator::render_icons()
{
    for (int i = 0; i < orders_list.size(); i++)
    {
        const gfm_orders* orders = orders_list[i] != 0xFFFFFFFF ? Orders_manager.get_orders_from_handle(orders_list[i]) : nullptr;
        if (orders == nullptr)
        {
            continue;
        }
        const unsigned int handle = orders->get_orders_handle();
        const int num = World.get_num_objects<object_squad>();
        for (int j = 0; j < num; j++)
        {
            const object_squad* squad = World.get_object<object_squad>(j);
            if (squad->base_orders_handle == handle || squad->assigned_orders_handle == handle)
            {
                const vehicle* vp = squad->get_vehicle();
                if (vp != nullptr && !vp->is_killed())
                {
                    Cartographer.add_temporary_icon(vp->pos, MAP_ELEMENT_STAR_RED_ICON, true, 0xFFFFFFFF, Default_map_item_color, false, 0xFFFFFFFF);
                }
                break;
            }
        }
    }
}

// 0x007A9D90 (:401)
void smc_level_init_pre()
{
    Smash_building_coordinator.level_init();
}

// 0x007A9DA0 (:407)
bool smc_do_frame()
{
    return Smash_building_coordinator.do_frame();
}

// 0x0079B130 (:413)
void smc_render_icons()
{
    Smash_building_coordinator.render_icons();
}
