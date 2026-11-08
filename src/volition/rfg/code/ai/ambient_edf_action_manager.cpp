// Original: volition/rfg/code/ai/ambient_edf_action_manager.cpp
//
// The ambient EDF events: at most one at a time, spawned around the player in unliberated districts when nothing else
// is going on.

#include "volition/rfg/code/ai/ambient_edf_action_manager.h"

#include "volition/anim_lib/code/anim_lib_internal.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/ambient_edf_action_manager_internal.h"
#include "volition/rfg/code/ai/road_route.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gameflow_enums.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/road_distance_info.h"
#include "volition/rfg/code/gameplay/squad_spawn.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_safehouse.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/spawn/ambient_spawn_manager.h"
#include "volition/rfg/code/spawn/spawn_enums.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/rand.h"

#include <float.h>

ambient_edf_info Current_ambient_edf_info; // 0x01642228
bool Disable_ambient_edf = false; // 0x01D67DB6

static timestamp Dump_civilian_cooldown; // 0x0164224C
static timestamp Flyby_cooldown; // 0x01642250
static timestamp Harass_player_cooldown; // 0x01642254
static item_info* Civilian_id_item_info = nullptr; // 0x01D67DB8
static int Check_id_anim_trigger_id; // 0x0164154C

// 0x005DFB60
ambient_road_spawn_info::ambient_road_spawn_info()
{
}

// 0x0060CCF0 (:86). LTCG: road_handle ESI, spawn_list ECX.
static int find_road_info_index(unsigned int road_handle, const farray<ambient_road_spawn_info, 32>& spawn_list)
{
    for (int i = 0; i < spawn_list.size(); i++)
    {
        if (spawn_list[i].prp->get_handle() == road_handle)
        {
            return i;
        }
    }
    return -1;
}

// 0x006510A0 (:101). A road point 150 to 250 m from the player, the road's nearest by travel when several qualify.
static bool pick_drive_by_spawn_point(road_spline_identifier& spawn_road_id)
{
    farray<ambient_road_spawn_info, 32> spawn_list;
    const farray<road_distance_info, 256>& road_list = road_info_get_list();
    const player* local = player_get_local_player();
    for (int i = 0; i < road_list.size(); i++)
    {
        const road_distance_info& info = road_list[i];
        object_path_road* road = info.road_handle != 0xFFFFFFFF ? World.object_from_handle<object_path_road>(info.road_handle) : nullptr;
        if (road == nullptr || !road->road_flags.ambient)
        {
            continue;
        }
        ambient_road_spawn_info candidate;
        candidate.prp = road;
        candidate.index = info.index;
        candidate.destination_pos = road->get_road_point(info.index);
        candidate.dist_sqrd = vec_dist_squared(local->pos, candidate.destination_pos);
        if (candidate.dist_sqrd < 22500.0f || 62500.0f < candidate.dist_sqrd)
        {
            continue;
        }
        candidate.travel_dist = info.travel_distance;
        const int found = find_road_info_index(road->get_handle(), spawn_list);
        if (found < 0)
        {
            spawn_list.add(candidate);
        }
        else if (candidate.travel_dist < spawn_list[found].travel_dist)
        {
            spawn_list[found] = candidate;
        }
    }
    if (spawn_list.size() < 1)
    {
        return false;
    }
    const int pick = spawn_list.size() == 1 ? 0 : int_rand_range(0, spawn_list.size() - 1);
    spawn_road_id.road_spline = spawn_list[pick].prp;
    spawn_road_id.point = spawn_list[pick].index;
    return true;
}

// 0x005DFB70 (:178)
void ambient_edf_info::reset()
{
    ambient_squad_handle = 0xFFFFFFFF;
    supporting_squad_handle = 0xFFFFFFFF;
    action_type = AMBIENT_EDF_ACTION_TYPE_NONE;
    cooldown_timer.set_rand(500, 1000);
    target_obj_handle = 0xFFFFFFFF;
    target_pos.zero();
    action_flags.completed = 0;
    action_flags.slot_reserved = 0;
}

// 0x0060CD20 (:190)
void ambient_edf_info::release_and_reset()
{
    object_squad* squad = ambient_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(ambient_squad_handle) : nullptr;
    if (squad != nullptr)
    {
        squad->request_despawn_all_members(NPC_DESPAWN_TYPE_LOW, nullptr, true);
    }
    squad = supporting_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(supporting_squad_handle) : nullptr;
    if (squad != nullptr)
    {
        squad->request_despawn_all_members(action_type == AMBIENT_EDF_ACTION_TYPE_FLYER ? NPC_DESPAWN_TYPE_MEDUIM : NPC_DESPAWN_TYPE_LOW, nullptr, true);
    }
    if (action_flags.slot_reserved)
    {
        srm_release_all_slots(SPAWN_SLOT_VEHICLE_EDF_REINFORCEMENT, RESOURCE_SLOT_USER_ID_AMBIENT);
    }
    reset();
}

// 0x006512C0 (:222)
void ambient_edf_info::amb_do_frame_fly_by()
{
    if (ALERT_LEVEL_GREEN < gsm_get_alert_level())
    {
        release_and_reset();
        return;
    }
    if (action_flags.completed ? amb_do_frame_fly_by_complete() : amb_do_frame_fly_by_incomplete())
    {
        release_and_reset();
    }
}

// 0x0060CDD0 (:243). true ends the action: unsimulated flyers are kept within 400 m of the player.
bool ambient_edf_info::amb_do_frame_fly_by_complete()
{
    if (action_type != AMBIENT_EDF_ACTION_TYPE_FLYER)
    {
        return true;
    }
    const object_squad* squad = ambient_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(ambient_squad_handle) : nullptr;
    return squad == nullptr || squad->simulated() || 160000.0f < vec_xz_dist_squared(squad->pos, player_get_local_player()->pos);
}

// 0x0060CE30 (:279). true ends the action: the flyer has left 350 m around the player.
bool ambient_edf_info::amb_do_frame_fly_by_incomplete()
{
    if (action_type != AMBIENT_EDF_ACTION_TYPE_FLYER)
    {
        return true;
    }
    const object_squad* squad = ambient_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(ambient_squad_handle) : nullptr;
    if (squad == nullptr)
    {
        return true;
    }
    if (!squad->simulated())
    {
        action_flags.completed = 1;
    }
    return 122500.0f < vec_xz_dist_squared(player_get_local_player()->pos, squad->pos);
}

// 0x0060CEA0 (:314). true ends the action. Completes within 20 m of the destination, ends beyond 350 m of the player.
bool ambient_edf_info::do_frame_drive_by_player_incomplete_common()
{
    const object* squad = ambient_squad_handle != 0xFFFFFFFF ? World.object_from_handle(ambient_squad_handle) : nullptr;
    if (squad == nullptr || squad->get_obj_type() != OT_SQUAD)
    {
        return true;
    }
    if (vec_xz_dist_squared(squad->pos, target_pos) < 400.0f)
    {
        action_flags.completed = 1;
    }
    return 122500.0f < vec_dist_squared(player_get_local_player()->pos, squad->pos);
}

// 0x00651300 (:344)
bool ambient_edf_info::amb_do_frame_drive_by_complete()
{
    return action_type != AMBIENT_EDF_ACTION_TYPE_DRIVE_BY || do_frame_drive_by_player_complete_common();
}

// 0x0060CF10 (:357). true ends the action: beyond 200 m, or from 60 m on once the vehicle is out of sight.
bool ambient_edf_info::do_frame_drive_by_player_complete_common()
{
    const object_squad* squad = ambient_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(ambient_squad_handle) : nullptr;
    if (squad == nullptr || squad->simulated())
    {
        return true;
    }
    const player* local = player_get_local_player();
    const float dist_sq = vec_xz_dist_squared(squad->pos, local->pos);
    if (40000.0f < dist_sq)
    {
        return true;
    }
    if (dist_sq < 3600.0f)
    {
        return false;
    }
    const vehicle* vp = squad->squad_vehicle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(squad->squad_vehicle) : nullptr;
    return vp == nullptr || vp->get_handle() == local->last_vehicle_driven || vp->is_offscreen();
}

// 0x0060CFD0 (:407)
bool ambient_edf_info::amb_do_frame_drive_by_incomplete()
{
    return action_type != AMBIENT_EDF_ACTION_TYPE_DRIVE_BY || do_frame_drive_by_player_incomplete_common();
}

// 0x00651310 (:419)
void ambient_edf_info::amb_do_frame_drive_by()
{
    if (action_flags.completed ? amb_do_frame_drive_by_complete() : amb_do_frame_drive_by_incomplete())
    {
        release_and_reset();
    }
}

// 0x0060CFE0 (:433)
bool ambient_edf_info::amb_do_frame_dump_civ_complete()
{
    return action_type != AMBIENT_EDF_ACTION_TYPE_DUMP_CIV || do_frame_drive_by_player_complete_common();
}

// 0x0060CFF0 (:445)
bool ambient_edf_info::amb_do_frame_dump_civ_incomplete()
{
    return action_type != AMBIENT_EDF_ACTION_TYPE_DUMP_CIV || do_frame_drive_by_player_incomplete_common();
}

// 0x0060D000 (:457)
void ambient_edf_info::amb_do_frame_dump_civ()
{
    if (action_flags.completed ? amb_do_frame_dump_civ_complete() : amb_do_frame_dump_civ_incomplete())
    {
        release_and_reset();
    }
}

// 0x0060D040 (:471)
bool ambient_edf_info::amb_do_frame_harass_player_complete()
{
    return action_type != AMBIENT_EDF_ACTION_TYPE_HARASS_PLAYER || do_frame_drive_by_player_complete_common();
}

// 0x0060D050 (:483)
bool ambient_edf_info::amb_do_frame_harass_player_incomplete()
{
    return action_type != AMBIENT_EDF_ACTION_TYPE_HARASS_PLAYER || do_frame_drive_by_player_incomplete_common();
}

// 0x0060D060 (:495)
void ambient_edf_info::amb_do_frame_harass_player()
{
    if (action_flags.completed ? amb_do_frame_harass_player_complete() : amb_do_frame_harass_player_incomplete())
    {
        release_and_reset();
    }
}

// 0x00651350 (:510)
void ambient_edf_info::amb_do_frame()
{
    if (ALERT_LEVEL_YELLOW < gsm_get_alert_level() || safehouse_contains_player())
    {
        release_and_reset();
        return;
    }
    switch (action_type)
    {
    case AMBIENT_EDF_ACTION_TYPE_FLYER:
        amb_do_frame_fly_by();
        return;
    case AMBIENT_EDF_ACTION_TYPE_DRIVE_BY:
        amb_do_frame_drive_by();
        return;
    case AMBIENT_EDF_ACTION_TYPE_DUMP_CIV:
        amb_do_frame_dump_civ();
        return;
    case AMBIENT_EDF_ACTION_TYPE_HARASS_PLAYER:
        amb_do_frame_harass_player();
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        release_and_reset();
        return;
    }
}

// 0x005DFBC0 (:553). LTCG: random_direction ESI. One of the eight compass points. Original bug kept: diagonals 5 and
// 7 are not normalized as 1 and 3 are, so those flyers start about 424 m away, not 300.
static bool pick_flyer_spawn_point(vector& random_direction)
{
    switch (int_rand_range(0, 7))
    {
    case 0:
        random_direction = FORWARD_VECTOR;
        return true;
    case 1:
        random_direction = vector(RIGHT_VECTOR.x + FORWARD_VECTOR.x, FORWARD_VECTOR.y + RIGHT_VECTOR.y, FORWARD_VECTOR.z + RIGHT_VECTOR.z);
        random_direction.normalize_safe();
        return true;
    case 2:
        random_direction = RIGHT_VECTOR;
        return true;
    case 3:
        random_direction = vector(RIGHT_VECTOR.x - FORWARD_VECTOR.x, RIGHT_VECTOR.y - FORWARD_VECTOR.y, RIGHT_VECTOR.z - FORWARD_VECTOR.z);
        random_direction.normalize_safe();
        return true;
    case 4:
        random_direction = vector(-FORWARD_VECTOR.x, -FORWARD_VECTOR.y, -FORWARD_VECTOR.z);
        return true;
    case 5:
        random_direction = vector(-FORWARD_VECTOR.x - RIGHT_VECTOR.x, -FORWARD_VECTOR.y - RIGHT_VECTOR.y, -FORWARD_VECTOR.z - RIGHT_VECTOR.z);
        return true;
    case 6:
        random_direction = vector(-RIGHT_VECTOR.x, -RIGHT_VECTOR.y, -RIGHT_VECTOR.z);
        return true;
    case 7:
        random_direction = vector(FORWARD_VECTOR.x - RIGHT_VECTOR.x, FORWARD_VECTOR.y - RIGHT_VECTOR.y, FORWARD_VECTOR.z - RIGHT_VECTOR.z);
        return true;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return false;
    }
}

// 0x005DFF10 (:608). Within 100 m of the player on each axis.
bool pick_flyer_destination_pos(vector& destination_pos, const player* pp)
{
    if (pp == nullptr)
    {
        return false;
    }
    const float x_offset = fl_rand();
    destination_pos.x = bool_rand() ? pp->pos.x + x_offset * 100.0f : pp->pos.x - x_offset * 100.0f;
    destination_pos.y = pp->pos.y;
    const float z_offset = fl_rand();
    destination_pos.z = bool_rand() ? pp->pos.z + z_offset * 100.0f : pp->pos.z - z_offset * 100.0f;
    return true;
}

// Flat, normalized, and FORWARD_VECTOR when it comes out null. The orientation matrix the spawn functions build.
static void make_flat_orient(matrix& orient, const vector& from, const vector& to)
{
    vector dir(to.x - from.x, 0.0f, to.z - from.z);
    dir.normalize_safe();
    if (dir.is_null())
    {
        dir = FORWARD_VECTOR;
    }
    orient.make(dir);
}

// 0x0060D0A0 (:637). The squad flies 1 km past a point near the player, from 300 m away.
void ambient_edf_info::amb_do_spawn_fly_by()
{
    static const squad_definition* FLYER_SQUAD_DEF = squad_definition_from_name("basic_squad_1"); // 0x01DDBAD8
    static const vehicle_spawn_group_info* FLYER_VEH_GROUP = sog_get_vehicle_group("EDF_flyer"); // 0x01DDBAD4
    if (!Flyby_cooldown.elapsed())
    {
        return;
    }
    Flyby_cooldown.set_rand(120000, 240000);
    if (FLYER_SQUAD_DEF == nullptr || FLYER_VEH_GROUP == nullptr || ALERT_LEVEL_GREEN < gsm_get_alert_level() || srm_get_availible_slots(SPAWN_SLOT_VEHICLE_EDF_REINFORCEMENT, RESOURCE_SLOT_USER_ID_AMBIENT) < 1)
    {
        return;
    }
    vector direction;
    if (!pick_flyer_spawn_point(direction))
    {
        return;
    }
    const player* pp = player_get_local_player();
    const vector spawn_pos(pp->pos.x + direction.x * 300.0f, pp->pos.y + direction.y * 300.0f, pp->pos.z + direction.z * 300.0f);
    if (World.is_zone_border_zone(spawn_pos) || !srm_reserve_slot(SPAWN_SLOT_VEHICLE_EDF_REINFORCEMENT, RESOURCE_SLOT_USER_ID_AMBIENT, FLYER_VEH_GROUP))
    {
        return;
    }
    action_flags.slot_reserved = 1;
    squad_spawn_parameters params;
    vector destination;
    if (!pick_flyer_destination_pos(destination, pp))
    {
        return;
    }
    vector fly_dir(destination.x - spawn_pos.x, destination.y - spawn_pos.y, destination.z - spawn_pos.z);
    fly_dir.normalize_safe();
    if (fly_dir.is_null())
    {
        fly_dir = FORWARD_VECTOR;
    }
    params.orders.location_target = vector(fly_dir.x * 1000.0f + spawn_pos.x, fly_dir.y * 1000.0f + spawn_pos.y, fly_dir.z * 1000.0f + spawn_pos.z);
    params.orders.orders_type = GFM_ORDERS_TYPE_FLY_BY;
    params.orders.object_target = pp->get_handle();
    params.squad.npc_flags.no_turret_attacks = 1;
    params.squad.npc_flags.ambient_edf_flyer = 1;
    params.squad.hflags.ambient_edf = 1;
    params.squad.squad_pos = spawn_pos;
    params.squad.veh_spawn_group = FLYER_VEH_GROUP;
    params.squad.squad_def = FLYER_SQUAD_DEF;
    make_flat_orient(params.squad.squad_orient, spawn_pos, params.orders.location_target);
    params.spawn_squad();
    if (params.spawned_squad != nullptr && params.created_orders != nullptr)
    {
        reset();
        action_type = AMBIENT_EDF_ACTION_TYPE_FLYER;
        ambient_squad_handle = params.spawned_squad->get_handle();
        target_obj_handle = pp->get_handle();
        target_pos = pp->pos;
        action_flags.slot_reserved = 1;
        return;
    }
    params.destroy_squad();
    params.destroy_orders();
    release_and_reset();
}

// The closest road search of the three road events: an ambient road within 60 m of the player.
static const object_path_road* find_ambient_road(const vector& player_pos, float& u_value)
{
    vector closest_point;
    const object_path_road* road = object_road_get_closest_road(player_pos, false, closest_point, u_value, 60.0f, false, nullptr, nullptr, false);
    return road != nullptr && !road->road_flags.no_reinforcements && road->road_flags.ambient ? road : nullptr;
}

// The rest of their road part: the road's nearest point must be within 30 m of the player, and a spawn point must exist.
static bool pick_road_spawn(const object_path_road* road, float u_value, const vector& player_pos, vector& near_point, vector& spawn_pos)
{
    near_point = road->get_road_point((unsigned short)fl_round(u_value));
    road_spline_identifier spawn_road_id;
    if (900.0f < vec_xz_dist_squared(player_pos, near_point) || !pick_drive_by_spawn_point(spawn_road_id))
    {
        return false;
    }
    spawn_pos = spawn_road_id.road_spline->get_road_point(spawn_road_id.point);
    return true;
}

// Fills in the EDF squad of a road event.
static void setup_road_event_squad(squad_spawn_parameters& params, const vector& spawn_pos, const vector& near_point, const object_path_road* road, const squad_definition* squad_def, const vehicle_spawn_group_info* veh_group)
{
    params.squad.squad_pos = spawn_pos;
    params.orders.location_target = near_point;
    params.orders.orders_type = GFM_ORDERS_TYPE_DRIVE_BY;
    params.orders.object_target = road->get_handle();
    params.squad.squad_def = squad_def;
    params.squad.veh_spawn_group = veh_group;
    params.squad.hflags.ambient_edf = 1;
    make_flat_orient(params.squad.squad_orient, spawn_pos, near_point);
}

// 0x006513E0 (:749)
void ambient_edf_info::amb_do_spawn_drive_by()
{
    static const squad_definition* DRIVE_BY_SQUAD_DEF = squad_definition_from_name("basic_squad_2"); // 0x01DDBB58
    if (DRIVE_BY_SQUAD_DEF == nullptr)
    {
        return;
    }
    const vector& player_pos = player_get_local_player()->pos;
    float u_value;
    const object_path_road* road = find_ambient_road(player_pos, u_value);
    vector near_point;
    vector spawn_pos;
    if (road == nullptr || !pick_road_spawn(road, u_value, player_pos, near_point, spawn_pos))
    {
        return;
    }
    squad_spawn_parameters params;
    setup_road_event_squad(params, spawn_pos, near_point, road, DRIVE_BY_SQUAD_DEF, sog_default_vehicle_spawn_group(HUMAN_TEAM_EDF));
    params.spawn_squad();
    if (params.spawned_squad != nullptr && params.created_orders != nullptr)
    {
        reset();
        action_type = AMBIENT_EDF_ACTION_TYPE_DRIVE_BY;
        ambient_squad_handle = params.spawned_squad->get_handle();
        target_obj_handle = road->get_handle();
        target_pos = near_point;
        return;
    }
    params.destroy_squad();
    params.destroy_orders();
}

// 0x006516A0 (:836). A civilian squad comes along with the EDF one, ordered to follow it.
void ambient_edf_info::amb_do_spawn_dump_civ()
{
    static const vehicle_spawn_group_info* DUMP_CIV_VEH_SPAWN_GROUP_INFO = sog_get_vehicle_group("Default EDF NoTurret"); // 0x01DDBB68
    static const squad_definition* DUMP_CIV_SQUAD_DEF = squad_definition_from_name("basic_squad_1"); // 0x01DDBB64
    static const squad_definition* CIV_SQUAD_DEF = squad_definition_from_name("civilian_squad_1"); // 0x01DDBB60
    if (DUMP_CIV_SQUAD_DEF == nullptr || !Dump_civilian_cooldown.elapsed())
    {
        return;
    }
    Dump_civilian_cooldown.set_rand(120000, 240000);
    const vector& player_pos = player_get_local_player()->pos;
    const district* current_district = district_get_from_pos(player_pos);
    if (current_district == nullptr || !current_district->get_allow_amb_edf_civ_dump())
    {
        return;
    }
    float u_value;
    const object_path_road* road = find_ambient_road(player_pos, u_value);
    if (road == nullptr)
    {
        return;
    }
    // Needs a pedestrian node within 50 m. The height does not count.
    bbox area;
    area.min = vector(player_pos.x - 50.0f, -FLT_MAX, player_pos.z - 50.0f);
    area.max = vector(player_pos.x + 50.0f, FLT_MAX, player_pos.z + 50.0f);
    if (!ped_node_near_box(area))
    {
        return;
    }
    vector near_point;
    vector spawn_pos;
    if (!pick_road_spawn(road, u_value, player_pos, near_point, spawn_pos))
    {
        return;
    }
    squad_spawn_parameters params;
    setup_road_event_squad(params, spawn_pos, near_point, road, DUMP_CIV_SQUAD_DEF, DUMP_CIV_VEH_SPAWN_GROUP_INFO);
    params.spawn_squad();
    if (params.spawned_squad == nullptr || params.created_orders == nullptr)
    {
        params.destroy_squad();
        params.destroy_orders();
        return;
    }
    squad_spawn_parameters civ_params;
    civ_params.squad.squad_pos = spawn_pos;
    civ_params.orders.location_target = near_point;
    civ_params.orders.orders_type = GFM_ORDERS_TYPE_DRIVE_BY;
    civ_params.orders.object_target = params.spawned_squad->get_handle();
    civ_params.squad.squad_def = CIV_SQUAD_DEF;
    civ_params.spawn_squad();
    if (civ_params.spawned_squad != nullptr && civ_params.created_orders != nullptr)
    {
        reset();
        action_type = AMBIENT_EDF_ACTION_TYPE_DUMP_CIV;
        ambient_squad_handle = params.spawned_squad->get_handle();
        supporting_squad_handle = civ_params.spawned_squad->get_handle();
        target_obj_handle = road->get_handle();
        target_pos = near_point;
        return;
    }
    params.destroy_squad();
    params.destroy_orders();
    civ_params.destroy_squad();
    civ_params.destroy_orders();
}

// 0x00651B70 (:979)
void ambient_edf_info::amb_do_spawn_harass_player()
{
    static const squad_definition* HARASS_SQUAD_DEF = squad_definition_from_name("basic_squad_2"); // 0x01DDBB74
    static const vehicle_spawn_group_info* HARASS_VEH_SPAWN_GROUP_INFO = sog_get_vehicle_group("Default EDF NoTurret"); // 0x01DDBB70
    if (!Harass_player_cooldown.elapsed())
    {
        return;
    }
    Harass_player_cooldown.set_rand(240000, 480000);
    if (HARASS_SQUAD_DEF == nullptr)
    {
        return;
    }
    const vector& player_pos = player_get_local_player()->pos;
    float u_value;
    const object_path_road* road = find_ambient_road(player_pos, u_value);
    vector near_point;
    vector spawn_pos;
    if (road == nullptr || !pick_road_spawn(road, u_value, player_pos, near_point, spawn_pos))
    {
        return;
    }
    squad_spawn_parameters params;
    setup_road_event_squad(params, spawn_pos, near_point, road, HARASS_SQUAD_DEF, HARASS_VEH_SPAWN_GROUP_INFO);
    params.spawn_squad();
    if (params.spawned_squad != nullptr && params.created_orders != nullptr)
    {
        reset();
        action_type = AMBIENT_EDF_ACTION_TYPE_HARASS_PLAYER;
        ambient_squad_handle = params.spawned_squad->get_handle();
        target_obj_handle = road->get_handle();
        target_pos = near_point;
        return;
    }
    params.destroy_squad();
    params.destroy_orders();
}

// 0x00651E70 (:1074)
void ambient_edf_info::amb_do_spawn()
{
    if (Disable_ambient_edf || ALERT_LEVEL_GREEN < gsm_get_alert_level() || safehouse_contains_player() || !activity_none())
    {
        return;
    }
    const district* current_district = district_get_from_pos(player_get_local_player()->pos);
    if (current_district != nullptr && current_district->is_liberated())
    {
        return;
    }
    switch (int_rand_range(2, 5))
    {
    case 2:
        amb_do_spawn_fly_by();
        return;
    case 3:
        amb_do_spawn_drive_by();
        return;
    case 4:
        amb_do_spawn_dump_civ();
        return;
    case 5:
        amb_do_spawn_harass_player();
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
}

// 0x005DFFB0 (:1141)
bool ambient_edf_info::is_active() const
{
    return action_type != AMBIENT_EDF_ACTION_TYPE_NONE;
}

// 0x0060D530 (:1157)
void amb_edf_action_manager_level_init_post()
{
    Current_ambient_edf_info.reset();
    Current_ambient_edf_info.cooldown_timer.set(20000);
    Dump_civilian_cooldown.set_rand(120000, 240000);
    Harass_player_cooldown.set_rand(240000, 480000);
    Flyby_cooldown.set_rand(120000, 240000);
    Civilian_id_item_info = object_item_get_info("amb_idcard");
    Check_id_anim_trigger_id = animlib_get_trigger_id("impact end");
}

// 0x00651F30 (:1174)
void amb_edf_action_manager_do_frame()
{
    if (Multiplayer || wrecking_crew_is_active() || mission_is_active())
    {
        return;
    }
    if (Current_ambient_edf_info.action_type != AMBIENT_EDF_ACTION_TYPE_NONE)
    {
        Current_ambient_edf_info.amb_do_frame();
    }
    else if (Current_ambient_edf_info.cooldown_timer.elapsed())
    {
        Current_ambient_edf_info.amb_do_spawn();
        Current_ambient_edf_info.cooldown_timer.set_rand(500, 1000);
    }
}

// 0x005DFFC0 (:1194)
bool amb_edf_action_participant(const npc* np)
{
    if (Current_ambient_edf_info.ambient_squad_handle == 0xFFFFFFFF)
    {
        return false;
    }
    const unsigned int squad = np->get_ai()->get_squad_handle();
    return squad == Current_ambient_edf_info.ambient_squad_handle || squad == Current_ambient_edf_info.supporting_squad_handle;
}

// 0x005E0000 (:1209)
bool amb_edf_action_support_participant(const npc* np)
{
    return np != nullptr && Current_ambient_edf_info.ambient_squad_handle != 0xFFFFFFFF && Current_ambient_edf_info.supporting_squad_handle != 0xFFFFFFFF && np->get_ai()->get_squad_handle() == Current_ambient_edf_info.supporting_squad_handle;
}

// 0x005E0030 (:1230)
ambient_edf_action_type amb_edf_action_type(const npc* np)
{
    return amb_edf_action_participant(np) ? Current_ambient_edf_info.action_type : AMBIENT_EDF_ACTION_TYPE_NONE;
}

// 0x005E0060 (:1240)
bool amb_edf_action_complete(const npc* np)
{
    return !amb_edf_action_participant(np) || Current_ambient_edf_info.action_flags.completed;
}

// 0x005E00A0 (:1250)
const vector& amb_edf_action_destination(const npc* np)
{
    return amb_edf_action_participant(np) ? Current_ambient_edf_info.target_pos : ZERO_VECTOR;
}

// 0x005E00E0 (:1260)
void amb_edf_set_dump_civ_cooldown()
{
    Dump_civilian_cooldown.set_rand(120000, 240000);
}

// 0x005E0100 (:1265)
void amb_edf_set_flyby_cooldown()
{
    Flyby_cooldown.set_rand(120000, 240000);
}

// 0x005E0120 (:1270)
void amb_edf_set_harass_player_cooldown()
{
    Harass_player_cooldown.set_rand(240000, 480000);
}

// 0x005E0140 (:1276)
item_info* amb_edf_get_civ_id_item()
{
    return Civilian_id_item_info;
}

// 0x005E0150 (:1282)
int amb_edf_get_civ_id_anim_trigger_id()
{
    return Check_id_anim_trigger_id;
}

// 0x0060CC20 (.h:41)
ambient_edf_info::ambient_edf_info()
{
    reset();
}
