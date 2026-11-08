// Original: volition/rfg/code/gameplay/vehicle_drop_off.cpp
//
// Vehicle drop off: a guerrilla squad brings a vehicle to a player sprinting on foot for 10 s.

#include "volition/rfg/code/gameplay/vehicle_drop_off.h"

#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/mission_info_shared.h"
#include "volition/rfg/code/gameplay/squad_spawn.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_restricted_area.h"
#include "volition/rfg/code/objects/object_safehouse.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

#include <float.h>

extern mission_handle Mission_active_index; // 0x0165150C (gameplay.cpp)
extern gfm_alert_levels Current_alert_level; // 0x022278B4 (alert_level_manager.cpp)

// ---- file statics
static unsigned int Drop_off_squad_handle = 0xFFFFFFFF; // 0x0165157C
static timestamp Sprint_duration; // 0x01659960
static timestamp Drop_off_polling_period; // 0x01659964
static timestamp Respawn_period; // 0x01659968
static bool Disable_vehicle_drop_off; // 0x02229682, a debug switch nothing in rfg.exe writes

// 0x0075F7B0 (:61)
static void reset_local_data()
{
    Drop_off_squad_handle = 0xFFFFFFFF;
    Sprint_duration.set(0);
    Drop_off_polling_period.set(0);
    Respawn_period.set(0);
}

// 0x00776590 (:70)
// LTCG: the player in EAX. The centre of the free nav cell nearest the point 60 m behind the camera, within 30 m of it.
static bool find_spawn_pos(const player* pp, vector& spawn_pos)
{
    if (pp == nullptr)
    {
        return false;
    }
    vector behind(-Rfg_camera.real_orient.fvec.x, 0.0f, -Rfg_camera.real_orient.fvec.z);
    behind.normalize_safe();
    if (behind.is_null())
    {
        return false;
    }
    vector search(pp->pos.x + behind.x * 60.0f, pp->pos.y + behind.y * 60.0f, pp->pos.z + behind.z * 60.0f);
    if (!rfg_snap_to_ground(search))
    {
        return false;
    }
    // Magic number: room for 0x80 cells.
    nav_cell* cells[0x81];
    const int count = havok_collect_nav_cells(search, 30.0f, cells, 0x80, true);
    const nav_cell* best = nullptr;
    vector best_center;
    best_center.zero();
    float best_dist = FLT_MAX;
    for (int i = 0; i < count; i++)
    {
        // Magic number: 8, the cell flag the drop off keeps away from.
        if ((cells[i]->flags & 8) != 0)
        {
            continue;
        }
        vector center;
        nav_cell_get_centroid(&center, cells[i]);
        const float dist = vec_dist_squared(search, center);
        if (dist < best_dist)
        {
            best = cells[i];
            best_center = center;
            best_dist = dist;
        }
    }
    if (best == nullptr)
    {
        return false;
    }
    spawn_pos = best_center;
    return true;
}

// 0x007B0E50 (:140)
// LTCG: the player in ESI. Read from the asm, because the decompile loses the parameter block.
static void do_drop_off_spawn(const player* pp)
{
    static const squad_definition* guerrilla_squad_def = squad_definition_from_name("guerilla_squad_1"); // 0x02273E14, the table's spelling
    if (guerrilla_squad_def == nullptr)
    {
        return;
    }
    vector spawn_pos;
    if (!find_spawn_pos(pp, spawn_pos))
    {
        return;
    }
    squad_spawn_parameters params;
    // Magic number: 0x11, the drop off's orders type, which gameflow_enums.h does not name yet.
    params.orders.orders_type = (gfm_orders_types)0x11;
    params.orders.location_target = pp->pos;
    params.orders.object_target = pp->get_handle();
    params.squad.squad_pos = spawn_pos;
    params.squad.squad_def = guerrilla_squad_def;
    params.squad.veh_spawn_group = sog_default_vehicle_spawn_group(pp->get_team());
    // The squad faces the player, level, or along the world forward when it spawns right on the player.
    vector facing(pp->pos.x - spawn_pos.x, 0.0f, pp->pos.z - spawn_pos.z);
    facing.normalize_safe();
    if (facing.is_null())
    {
        facing = FORWARD_VECTOR;
    }
    params.squad.squad_orient.make_quick(facing);
    params.spawn_squad();
    if (params.spawned_squad != nullptr && params.created_orders != nullptr)
    {
        Drop_off_squad_handle = params.spawned_squad->get_handle();
        return;
    }
    params.destroy_squad();
    if (params.created_orders != nullptr)
    {
        Orders_manager.master_orders_pool.free(params.created_orders);
    }
}

// 0x007B1010 (:199)
static void do_frame_spawn()
{
    if (Disable_vehicle_drop_off || !Respawn_period.elapsed() || Mission_active_index != INVALID_MISSION_HANDLE || !activity_none())
    {
        return;
    }
    // Magic numbers: at alert level 1 and above no drop off, and 10 s of sprinting.
    if ((int)Current_alert_level >= 1 || Sprint_duration.time_since() < 10000)
    {
        return;
    }
    // Magic number: morale of at least 30%.
    if (gfm_get_effective_morale_percent() < 0.3f)
    {
        return;
    }
    const player* pp = player_get_local_player();
    if (pp->vehicle_handle != 0xFFFFFFFF || safehouse_contains_player() || restricted_area_in_any_region(pp->pos, true))
    {
        return;
    }
    // No vehicle within 50 m. Magic number: 2500 = 50 squared.
    const int num_vehicles = World.get_num_objects<automobile>();
    for (int i = 0; i < num_vehicles; i++)
    {
        if (vec_dist_squared(pp->pos, World.get_object<automobile>(i)->pos) < 2500.0f)
        {
            return;
        }
    }
    const nav_cell* cell = bb_pf_get_nav_cell(pp->pos, true, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
    if (cell != nullptr && (cell->flags & 8) == 0)
    {
        do_drop_off_spawn(pp);
        // Magic number: a minute until the next one.
        Respawn_period.set(60000);
    }
}

// 0x0075F7F0 (:283)
static void do_sprint_tracking()
{
    const player* pp = player_get_local_player();
    if (!pp->pflags.do_sprint)
    {
        Sprint_duration.set(0);
    }
}

// 0x0075F810 (:301)
void vehicle_drop_off_level_init_pre()
{
    reset_local_data();
}

// 0x007B1170 (:307)
void vehicle_drop_off_do_frame()
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return;
    }
    do_sprint_tracking();
    if (!Drop_off_polling_period.elapsed())
    {
        return;
    }
    // Magic numbers: polled every 1-2 s.
    Drop_off_polling_period.set_rand(1000, 2000);
    if (Drop_off_squad_handle != 0xFFFFFFFF && World.object_from_handle<object_squad>(Drop_off_squad_handle) == nullptr)
    {
        Drop_off_squad_handle = 0xFFFFFFFF;
        return;
    }
    do_frame_spawn();
}
