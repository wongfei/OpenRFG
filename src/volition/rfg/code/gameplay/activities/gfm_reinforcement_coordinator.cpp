// Original: volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator.cpp
//
// gameflow.md GF2. EDF reinforcements while the alert level is orange and red, by profile (reinforcement_profile.xtbl), the
// guerrillas that come to help the player (guerrilla_reinforcement.xtbl), and where any of them can spawn: on a road that
// leads to the player, at a squad node, or on a nav cell, cached per player position.
//
// Plain float where rfg.exe uses double chains, like the AI code.

#include "volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator.h"
#include "volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator_internal.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/percept.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_tech_level.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/road_distance_info.h"
#include "volition/rfg/code/gameplay/squad_spawn.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/level/rfg_zonex_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/pathfinding/nav_heap.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_pool.h"

#include <math.h>
#include <stdio.h>

extern gfm_alert_levels Current_alert_level; // 0x022278B4 (alert_level_manager.cpp)
extern mission_handle Mission_active_index; // 0x0165150C (gameplay.cpp)
extern int EDF_locked_mission_progression_level; // 0x02229650 (squad_progression.cpp)
extern bool Disable_reinforcement_spawn; // 0x022295CB (missions.cpp)
extern nav_heap Nav_heap; // 0x035BB618 (nav_heap.cpp)
bool should_rebuild_road_info(); // road_distance_info.cpp
void build_road_info(); // 0x00790120 (road_distance_info.cpp:251)

// Magic numbers: 80 to 160 m on foot and 150 to 250 m in a vehicle, around the player, squared.
static const float MIN_PED_REINFORCEMENT_SPAWN_DIST_SQRD = 6400.0f; // 0x01656498
static const float MAX_PED_REINFORCEMENT_SPAWN_DIST_SQRD = 25600.0f; // 0x0165649C
static const float MIN_VEH_REINFORCEMENT_SPAWN_DIST_SQRD = 22500.0f; // 0x016564A0
static const float MAX_VEH_REINFORCEMENT_SPAWN_DIST_SQRD = 62500.0f; // 0x016564A4
// Magic number: 15 m squared, how far off the nav mesh the player can be on foot before the group turns to flyers.
static const float NAV_MESH_NEAR_DIST_SQRD = 225.0f;
// Magic numbers: guerrillas come when at least 2 EDF are within 80 m; the warning shows within 60 m.
static const float NEAR_BY_EDF_DIST_SQRD = 6400.0f;
static const float GUERRILLA_WARNING_DIST_SQRD = 3600.0f;
// Magic number: a player driving at 4 m/s or more gets reinforcements from ahead.
static const float DIRECTIONAL_SPAWN_MIN_SPEED = 4.0f;
// Magic numbers: orders follow a witness for 20 s; green must last 10 s to clear them; a 5 m step resets the fix.
static const int ORDERS_LOS_TIMEOUT_MS = 20000;
static const int ORDERS_GREEN_RESET_MS = 10000;
static const float ORDERS_NEAR_DIST_SQRD = 25.0f;
// Magic number: a spawn delay in the table is capped at 10 minutes.
static const int MAX_SPAWN_DELAY_MS = 600000;
// Magic number: the string pool of the profile names.
static const unsigned int REINFORCEMENT_NAME_POOL_SIZE = 0x800;
// Magic numbers: a cached location is good while the player stays within 10 m of where it was found.
static const float CACHED_LOCATION_DIST_SQRD = 100.0f;
// Magic number: a cell in the cell search is taken within 20 m of the test point.
static const float CELL_TEST_RADIUS = 20.0f;

static master_spawn_info Master_veh_spawn_info(true, 150.0f, 250.0f); // 0x016564B8
static master_spawn_info Master_ped_spawn_info(false, 80.0f, 160.0f); // 0x01656940
static farray<guerrilla_reinforcement_info, 8> Guerrilla_reinforcement_profiles; // 0x01656DC8
static farray<guerrilla_reinforcement_info, 8> DLC_reinforcement_profiles; // 0x01656E38
static unsigned int Yellow_alert_orders_handle = 0xFFFFFFFF; // 0x01656EA8
static unsigned int Orange_alert_orders_handle = 0xFFFFFFFF; // 0x01656EAC
static reinforcement_allocation Orange_reinforcements(ALERT_LEVEL_ORANGE, SPAWN_SLOT_VEHICLE_EDF_REINFORCEMENT); // 0x01656EB0
static reinforcement_allocation Red_reinforcements(ALERT_LEVEL_RED, SPAWN_SLOT_VEHICLE_MULTI_PURPOSE); // 0x016570B8
static guerrilla_squad_info Guerrilla_reinforcement_squad; // 0x016572BC
// Deviation: external (declared in the internal header) for gfm_orders_manager.cpp, which rfg.exe's unity build lets read it.
cached_orders_info Orders_info; // 0x016572D0
static spawn_location_cache Foot_reinforcement_cache[4]; // 0x02245788, one per team
static spawn_location_cache Vehicle_reinforcement_cache[4]; // 0x022480B8
static object_pool<reinforcement_profile, 85, 1> Reinforcement_profile_pool; // 0x0224FD20
static bool Disable_guerrilla_reinforcement; // 0x022295CA
static char* Reinforcement_name_string_pool_data; // 0x022295F4
static gfm_alert_levels Last_alert_level_coordinator; // 0x022295F8
static bool Render_reinforcement_icon; // 0x022295FC, nothing sets it
static gfm_orders_flags NORMAL_REINFORCEMENT_FLAGS; // 0x02231B20

// Whether a road, node or cell may be used: the two activities that bring their own attackers stop all reinforcements.
// rfg.exe reads activity.cpp's static Current_activity_mode.
// 0x0075AFD0 (:256)
static bool activity_disable_reinforcements()
{
    const activity_modes mode = activity_get_mode();
    return mode == ACTIVITY_MODE_RAID_DEFEND || mode == ACTIVITY_MODE_RAID_ATTACK;
}

// 0x0075B000 (:267), xml_flag_root in EDI
static bool flag_exists_in_xml(xml_element* xml_flag_root, const char* flag_tag)
{
    if (xml_flag_root == nullptr)
    {
        return false;
    }
    for (xml_element* cur = xml_find(xml_flag_root, "Flag"); cur != nullptr; cur = xml_find_next(xml_flag_root, cur, "Flag"))
    {
        if (v_stricmp(flag_tag, cur->text) == 0)
        {
            return true;
        }
    }
    return false;
}

// 0x007870A0 (:284)
static void assign_reinforcement_squad(unsigned int orders_handle)
{
    object_squad* best = nullptr;
    float best_dist = MAX_PED_REINFORCEMENT_SPAWN_DIST_SQRD;
    const player* pp = player_get_local_player();
    const int num = World.get_num_objects<object_squad>();
    for (int i = 0; i < num; i++)
    {
        object_squad* squad = World.get_object<object_squad>(i);
        if (human_teams_get_disposition(HUMAN_TEAM_EDF, squad->get_squad_team()) != TEAM_DISPOSITION_ALLIED || !squad->can_be_reassigned())
        {
            continue;
        }
        const float dist = vec_dist_squared(pp->pos, squad->pos);
        if (dist < best_dist)
        {
            best = squad;
            best_dist = dist;
        }
    }
    if (best != nullptr)
    {
        best->assign_orders(orders_handle);
    }
}

// 0x0075B060 (:324), recomended_direction in ESI. Nothing in rfg.exe calls it; do_spawn_road inlines the same test.
static bool should_spawn_in_direction(vector& recomended_direction)
{
    const player* pp = player_get_local_player();
    if (pp->vehicle_handle == 0xFFFFFFFF)
    {
        return false;
    }
    recomended_direction = pp->velocity;
    recomended_direction.y = 0.0f;
    if (recomended_direction.is_null())
    {
        return false;
    }
    return DIRECTIONAL_SPAWN_MIN_SPEED <= recomended_direction.mag_and_normalize_safe();
}

// 0x00771160 (:348)
static bool spawn_object_used(unsigned int spawn_obj_handle)
{
    for (int i = 0; i < 4; i++)
    {
        const squad_reinforcement_allocation& orange = Orange_reinforcements.reinforcement_squads[i];
        if (orange.squad_info != nullptr && orange.squad_flags.spawned != 0 && orange.spawn_obj_handle == spawn_obj_handle)
        {
            return true;
        }
        const squad_reinforcement_allocation& red = Red_reinforcements.reinforcement_squads[i];
        if (red.squad_info != nullptr && red.squad_flags.spawned != 0 && red.spawn_obj_handle == spawn_obj_handle)
        {
            return true;
        }
    }
    return false;
}

// 0x0075B0C0 (:376), cell in EAX
// Original quirk kept: for a human, a cell only counts when its flag 8 is set; for a vehicle only when it is clear.
static bool can_use_nav_cell(const vector& cell_center, const nav_cell* cell, bool vehicle)
{
    if (!vehicle)
    {
        return (cell->flags >> 3 & 1) != 0;
    }
    if ((cell->flags & 8) != 0)
    {
        return false;
    }
    return bb_pf_get_nav_cell(cell_center, true, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr) != nullptr;
}

// 0x007711C0 (:419), reinforcement_prof in EAX
static bool reservation_requirements_met(const reinforcement_profile* reinforcement_prof, spawn_slot_types base_slot_type)
{
    if (Mission_active_index != INVALID_MISSION_HANDLE)
    {
        const bool reserved = reinforcement_prof->reservation_group == nullptr || srm_vehicle_group_reserved(reinforcement_prof->reservation_group);
        const bool required = reinforcement_prof->required_group == nullptr || srm_vehicle_group_reserved(reinforcement_prof->required_group);
        return reserved && required;
    }
    const bool required = reinforcement_prof->required_group == nullptr || srm_vehicle_group_reserved(reinforcement_prof->required_group);
    if (reinforcement_prof->reservation_group != nullptr && srm_get_availible_slots(base_slot_type, RESOURCE_SLOT_USER_ID_REINFORCEMENT) <= 0)
    {
        return false;
    }
    return required;
}

// 0x00771250 (:474)
static desired_reinforcement_group pick_desired_reinforcement_group()
{
    const player* pp = player_get_local_player();
    vector on_mesh;
    nav_cell* cell;
    if (!bb_pf_find_closest_point_on_nav_mesh_slow(pp->pos, on_mesh, cell) || !(vec_dist_squared(pp->pos, on_mesh) < NAV_MESH_NEAR_DIST_SQRD))
    {
        return REINFORCEMENT_GROUP_FLYER;
    }
    if (pp->vehicle_handle != 0xFFFFFFFF)
    {
        const object* objp = World.object_from_handle(pp->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            return REINFORCEMENT_GROUP_IN_VEHICLE;
        }
    }
    return REINFORCEMENT_GROUP_ON_FOOT;
}

// 0x00788A70 (:3174), progression_level in EDI
static void collect_reinforcement_profiles(farray<reinforcement_profile const*, 85>& profile_list, gfm_alert_levels current_alert, int progression_level, spawn_slot_types base_slot_type)
{
    profile_list.clear();
    if (progression_level < 0)
    {
        return;
    }
    csspinlock_scope lock(Reinforcement_profile_pool.thread_access);
    for (const reinforcement_profile* profile = Reinforcement_profile_pool.get_first(0); profile != nullptr; profile = Reinforcement_profile_pool.get_next(profile))
    {
        if (profile->required_alert_level == current_alert && profile->edf_progression_level == progression_level && reservation_requirements_met(profile, base_slot_type))
        {
            profile_list.add(profile);
        }
    }
}

// 0x0079A990 (:507)
static const reinforcement_profile* pick_reinforcement_profile(gfm_alert_levels current_alert, int progression_level, spawn_slot_types base_slot_type)
{
    if (progression_level < 0)
    {
        return nullptr;
    }
    farray<reinforcement_profile const*, 85> profiles;
    collect_reinforcement_profiles(profiles, current_alert, progression_level, base_slot_type);
    if (profiles.size() < 1)
    {
        return pick_reinforcement_profile(current_alert, progression_level - 1, base_slot_type);
    }
    const desired_reinforcement_group group = pick_desired_reinforcement_group();
    farray<reinforcement_profile const*, 85> matching;
    for (int i = 0; i < profiles.size(); i++)
    {
        const reinforcement_spawn_type& type = profiles[i]->reinforcement_type;
        if ((group == REINFORCEMENT_GROUP_ON_FOOT && type.on_foot != 0) || (group == REINFORCEMENT_GROUP_IN_VEHICLE && type.in_vehicle != 0) || (group == REINFORCEMENT_GROUP_FLYER && type.flyer != 0))
        {
            matching.add(profiles[i]);
        }
    }
    if (matching.size() > 0)
    {
        return matching.size() == 1 ? matching[0] : matching[int_rand_range(0, matching.size() - 1)];
    }
    return pick_reinforcement_profile(current_alert, progression_level - 1, base_slot_type);
}

// 0x007AF070 (:575), base_alert_level in EAX, orders_handle in ECX
static void do_frame_assigned_alert_orders(gfm_alert_levels current_alert, gfm_alert_levels base_alert_level, unsigned int& orders_handle)
{
    if (Disable_reinforcement_spawn || (int)base_alert_level > (int)current_alert || activity_disable_reinforcements())
    {
        if (orders_handle != 0xFFFFFFFF)
        {
            Orders_manager.destroy_orders(orders_handle);
            orders_handle = 0xFFFFFFFF;
        }
        return;
    }
    if (orders_handle == 0xFFFFFFFF)
    {
        gfm_orders_creation_params params;
        params.orders_type = GFM_ORDERS_TYPE_ATTACK;
        params.location_target = Orders_info.get_orders_location();
        params.ofp = NORMAL_REINFORCEMENT_FLAGS;
        params.team = HUMAN_TEAM_EDF;
        orders_handle = Orders_manager.create_new_orders(params);
        if (orders_handle != 0xFFFFFFFF)
        {
            assign_reinforcement_squad(orders_handle);
        }
        return;
    }
    gfm_orders* orders = Orders_manager.get_orders_from_handle(orders_handle);
    if (orders == nullptr)
    {
        orders_handle = 0xFFFFFFFF;
        return;
    }
    orders->update_orders_position(Orders_info.get_orders_location());
    if (orders->get_assigned_squad_object() == nullptr)
    {
        assign_reinforcement_squad(orders_handle);
    }
}

// 0x007712D0 (:627), road_spawn_list in EDX, road_handle in ESI, index in ECX
static bool find_road_spawn_info(const farray<reinforcement_road_spawn_info, 32>& road_spawn_list, unsigned int road_handle, int& index)
{
    if (road_handle == 0xFFFFFFFF)
    {
        return false;
    }
    for (index = 0; index < road_spawn_list.size(); index++)
    {
        if (road_spawn_list[index].road_id.road_spline->get_handle() == road_handle)
        {
            return true;
        }
    }
    return false;
}

// 0x00787170 (:641)
static void guerrilla_reinforcement_system_init(int dlc_id)
{
    const bool is_dlc = dlc_id != -1;
    DLC_reinforcement_profiles.clear();
    const char* filename = "guerrilla_reinforcement.xtbl";
    char dlc_name[256];
    if (is_dlc)
    {
        sprintf(dlc_name, "dlc%02d_%s", dlc_id, filename);
        if (!cf_exist(dlc_name))
        {
            return;
        }
        filename = dlc_name;
    }
    else
    {
        Guerrilla_reinforcement_profiles.clear();
    }
    xml_element* root = xml_table_parse(filename);
    if (root == nullptr)
    {
        return;
    }
    for (xml_element* cur = xml_find(root, "guerrilla_reinforcement_info"); cur != nullptr; cur = xml_find_next(root, cur, "guerrilla_reinforcement_info"))
    {
        guerrilla_reinforcement_info info;
        if (info.load(cur))
        {
            (is_dlc ? DLC_reinforcement_profiles : Guerrilla_reinforcement_profiles).add(info);
        }
    }
    xml_table_close(root);
}

// 0x007B8C40 (:688), current_alert_level in ESI
static void guerrilla_reinforcement_do_frame(gfm_alert_levels current_alert_level)
{
    Guerrilla_reinforcement_squad.do_frame(current_alert_level);
}

// 0x007AF1B0 (:694)
// A player on the marauder team counts how far the EDF has fallen instead of morale.
static float get_player_morale_level()
{
    const player* pp = player_get_local_player();
    if (pp == nullptr || pp->current_team != HUMAN_TEAM_MARAUDER)
    {
        return gfm_get_effective_morale_percent();
    }
    const district* dp = district_local_player();
    const float control_max = dp != nullptr ? dp->get_control_max() : 0.0f;
    const float control = dp != nullptr ? dp->get_control() : 0.0f;
    const float level = control_max <= 0.0f ? 0.0f : 1.0f - control / control_max;
    return 0.0f <= 0.0f - level ? 0.0f : level;
}

// 0x00771320 (:726), morale_percentage in XMM2, current_reinforcement_profiles in ECX
static const guerrilla_reinforcement_info* find_guerrilla_info(float morale_percentage, const base_array<guerrilla_reinforcement_info>& current_reinforcement_profiles)
{
    const guerrilla_reinforcement_info* best = nullptr;
    float best_morale = 0.0f;
    for (int i = 0; i < current_reinforcement_profiles.size(); i++)
    {
        const guerrilla_reinforcement_info& info = current_reinforcement_profiles[i];
        if (info.morale_level <= morale_percentage && best_morale < info.morale_level)
        {
            best = &info;
            best_morale = info.morale_level;
        }
    }
    return best;
}

// 0x00787290 (:755)
static int count_near_by_edf()
{
    const player* pp = player_get_local_player();
    int count = 0;
    const int num = World.get_num_objects<npc>();
    for (int i = 0; i < num; i++)
    {
        const npc* np = World.get_object<npc>(i);
        if (human_teams_get_disposition(np, HUMAN_TEAM_EDF) == TEAM_DISPOSITION_ALLIED && vec_dist_squared(np->pos, pp->pos) < NEAR_BY_EDF_DIST_SQRD)
        {
            count++;
        }
    }
    return count;
}

// 0x0079AB70 (:778)
static bool pick_guerrilla_spawn_location(vector& spawn_pos)
{
    if (should_rebuild_road_info())
    {
        build_road_info();
    }
    const object_path_road* best = nullptr;
    float best_travel = 3.4028235e+38f;
    vector best_pos;
    best_pos.zero();
    const player* pp = player_get_local_player();
    const bool base_game = World.dlc_bundle_id == -1;
    const farray<road_distance_info, 256>& list = road_info_get_list();
    for (int i = 0; i < list.size(); i++)
    {
        const road_distance_info& info = list[i];
        if (info.travel_distance > best_travel)
        {
            continue;
        }
        const object_path_road* road = info.road_handle != 0xFFFFFFFF ? World.object_from_handle<object_path_road>(info.road_handle) : nullptr;
        if (road == nullptr || (!base_game && road->road_flags.ambient == 0))
        {
            continue;
        }
        const vector point = road->get_road_point(info.index);
        const float dist = vec_dist_squared(pp->pos, point);
        if (MIN_VEH_REINFORCEMENT_SPAWN_DIST_SQRD < dist && dist < MAX_VEH_REINFORCEMENT_SPAWN_DIST_SQRD)
        {
            best = road;
            best_pos = point;
            best_travel = info.travel_distance;
        }
    }
    if (best == nullptr)
    {
        return false;
    }
    spawn_pos = best_pos;
    return true;
}

// 0x00771070 (gfm_reinforcement_coordinator_internal.h:85)
guerrilla_squad_info::guerrilla_squad_info()
{
    reset(0);
}

// 0x0075B110 (:855)
void guerrilla_squad_info::reset(int respawn_delay_ms)
{
    info = nullptr;
    squad_handle = 0xFFFFFFFF;
    lifetime.set(0);
    respawn_period.set(respawn_delay_ms);
    hud_warning = false;
}

// 0x007B8620 (:867)
void guerrilla_squad_info::do_frame(gfm_alert_levels alert_level)
{
    object_squad* squad = squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_handle) : nullptr;
    if (squad != nullptr)
    {
        do_frame_live(alert_level, squad);
    }
    else
    {
        do_frame_none(alert_level);
    }
}

// 0x0075B140 (:879)
void guerrilla_squad_info::release_squad(object_squad* squad)
{
    if (squad != nullptr)
    {
        squad->request_despawn_all_members(NPC_DESPAWN_TYPE_LOW, nullptr, true);
    }
    reset(info != nullptr ? info->respawn_delay_ms : 0);
}

// 0x0075B1B0 (:893)
void guerrilla_squad_info::do_frame_live(gfm_alert_levels alert_level, object_squad* squad)
{
    if (squad == nullptr)
    {
        return;
    }
    if (info != nullptr)
    {
        respawn_period.set(info->respawn_delay_ms);
    }
    if ((int)alert_level < 1)
    {
        release_squad(squad);
        return;
    }
    if (!hud_warning && vec_dist_squared(player_get_local_player()->pos, squad->pos) < GUERRILLA_WARNING_DIST_SQRD)
    {
        // Magic number: DLC 1 announces marauders instead (rfg.pdb's spelling of the message name).
        const char* id_name = dlc_get_current_id() == 1 ? "HM_MAURADER_REINFORCEMENTS" : "HM_GUERRILLA_REINFORCEMENTS";
        hud_message_add_minor(localization_get_display_string_req(id_name, nullptr), -1.0f, false);
        hud_warning = true;
    }
}

// 0x007B6EA0 (:937)
void guerrilla_squad_info::do_frame_none(gfm_alert_levels alert_level)
{
    if (Disable_guerrilla_reinforcement || (int)alert_level <= 1 || !respawn_period.elapsed() || player_get_local_player()->vehicle_handle != 0xFFFFFFFF || !activity_allow_guerrilla_reinforcements())
    {
        return;
    }
    const float morale = get_player_morale_level();
    const guerrilla_reinforcement_info* guerrilla_info = find_guerrilla_info(morale, DLC_reinforcement_profiles.size() > 0 ? DLC_reinforcement_profiles : Guerrilla_reinforcement_profiles);
    if (guerrilla_info != nullptr && count_near_by_edf() >= 2)
    {
        do_spawn(guerrilla_info);
    }
}

// 0x007B3890 (:995)
void guerrilla_squad_info::do_spawn(const guerrilla_reinforcement_info* guerrilla_info)
{
    if (guerrilla_info == nullptr)
    {
        return;
    }
    vector spawn_pos;
    if (!pick_guerrilla_spawn_location(spawn_pos))
    {
        return;
    }
    const player* pp = player_get_local_player();
    squad_spawn_parameters params;
    params.orders.orders_type = GFM_ORDERS_TYPE_DEFEND;
    params.orders.location_target = pp->pos;
    params.orders.object_target = pp->get_handle();
    params.squad.squad_def = guerrilla_info->squad_def;
    params.squad.squad_pos = spawn_pos;
    params.squad.veh_spawn_group = sog_default_vehicle_spawn_group(pp->current_team);
    params.squad.hflags.disallow_vehicle_entry = 1;
    params.spawn_squad();
    if (params.spawned_squad != nullptr && params.created_orders != nullptr)
    {
        reset(0);
        info = guerrilla_info;
        squad_handle = params.spawned_squad->get_handle();
        return;
    }
    params.destroy_squad();
    if (params.created_orders != nullptr)
    {
        Orders_manager.destroy_orders(params.created_orders);
    }
}

// 0x00771050 (gfm_reinforcement_coordinator_internal.h:69)
guerrilla_reinforcement_info::guerrilla_reinforcement_info()
{
    reset();
}

// 0x0075B260 (:1040)
void guerrilla_reinforcement_info::reset()
{
    squad_def = nullptr;
    morale_level = 1.0f;
    respawn_delay_ms = 0;
}

// 0x00771380 (:1048)
bool guerrilla_reinforcement_info::load(xml_element* xml_root)
{
    reset();
    const xml_element* name = xml_find(xml_root, "Name");
    if (name == nullptr)
    {
        return false;
    }
    squad_def = squad_definition_from_name(name->text);
    if (squad_def == nullptr)
    {
        return false;
    }
    xml_element* morale = xml_find(xml_root, "morale_level");
    if (morale == nullptr)
    {
        return false;
    }
    // Magic number: the table holds a percentage.
    const float level = (float)xml_get_int(morale, 0) / 100.0f;
    morale_level = level < 0.0f ? 0.0f : (1.0f < level ? 1.0f : level);
    xml_element* delay = xml_find(xml_root, "respawn_delay");
    if (delay == nullptr)
    {
        return false;
    }
    const int delay_ms = xml_get_int(delay, 0) * 1000;
    respawn_delay_ms = delay_ms < 0 ? 0 : delay_ms;
    return true;
}

// 0x00786F60 (gfm_reinforcement_coordinator_internal.h:113)
master_spawn_info::master_spawn_info(bool vehicle_flag, float min_spawn_dist, float max_spawn_dist)
    : VEHICLE_PF(vehicle_flag)
    , MIN_SPAWN_DISTANCE(min_spawn_dist)
    , MIN_SPAWN_DISTANCE_SQRD(min_spawn_dist * min_spawn_dist)
    , MAX_SPAWN_DISTANCE(max_spawn_dist)
    , MAX_SPAWN_DISTANCE_SQRD(max_spawn_dist * max_spawn_dist)
{
    reset();
}

// 0x00787070
master_spawn_info::~master_spawn_info()
{
}

// 0x0075B280 (:1083)
void master_spawn_info::invalidate()
{
    spawn_flags = {};
}

// 0x00771460 (:1089)
void master_spawn_info::reset()
{
    spawn_flags = {};
    road_spawn_list.clear();
}

// 0x0079AD00 (:1096)
void master_spawn_info::build_road_info()
{
    if (spawn_flags.road_info_valid != 0)
    {
        return;
    }
    spawn_flags.road_info_valid = 1;
    const player* pp = player_get_local_player();
    if (should_rebuild_road_info())
    {
        ::build_road_info();
    }
    road_spawn_list.clear();
    const farray<road_distance_info, 256>& list = road_info_get_list();
    for (int i = 0; i < list.size(); i++)
    {
        const road_distance_info& info = list[i];
        object_path_road* road = info.road_handle != 0xFFFFFFFF ? World.object_from_handle<object_path_road>(info.road_handle) : nullptr;
        if (road == nullptr)
        {
            continue;
        }
        reinforcement_road_spawn_info entry;
        entry.road_id.road_spline = road;
        entry.road_id.point = info.index;
        entry.travel_distance = info.travel_distance;
        entry.spawn_pos = road->get_road_point(info.index);
        entry.dist_to_player_sqrd = vec_dist_squared(pp->pos, entry.spawn_pos);
        if (MIN_SPAWN_DISTANCE_SQRD > entry.dist_to_player_sqrd || entry.dist_to_player_sqrd > MAX_SPAWN_DISTANCE_SQRD)
        {
            continue;
        }
        int index;
        if (find_road_spawn_info(road_spawn_list, road->get_handle(), index))
        {
            if (entry.travel_distance < road_spawn_list[index].travel_distance)
            {
                road_spawn_list[index] = entry;
            }
        }
        else if (road_spawn_list.size() < road_spawn_list.capacity())
        {
            road_spawn_list.add(entry);
        }
    }
}

// 0x00787320 (:1155)
void master_spawn_info::build_node_info()
{
    if (spawn_flags.node_info_valid != 0)
    {
        return;
    }
    spawn_flags.node_info_valid = 1;
    const player* pp = player_get_local_player();
    node_spawn_list.clear();
    const int num = World.get_num_objects<object_squad_spawn_node>();
    for (int i = 0; i < num; i++)
    {
        const object_squad_spawn_node* node = World.get_object<object_squad_spawn_node>(i);
        if (node->get_info() == nullptr || human_teams_get_disposition(HUMAN_TEAM_EDF, node->get_info()->squad_team) != TEAM_DISPOSITION_ALLIED)
        {
            continue;
        }
        const float dist = vec_dist_squared(pp->pos, node->pos);
        if (dist < MIN_SPAWN_DISTANCE_SQRD || MAX_SPAWN_DISTANCE_SQRD < dist)
        {
            continue;
        }
        node_spawn_list.add(node);
        if (node_spawn_list.size() >= node_spawn_list.capacity())
        {
            break;
        }
    }
}

// 0x00787430 (:1197)
void master_spawn_info::build_cell_info()
{
    if (spawn_flags.cell_info_valid != 0)
    {
        return;
    }
    spawn_flags.cell_info_valid = 1;
    // The four directions around the player. rfg.exe keeps them in a local static, filled from these vectors.
    const vector directions[4] = { FORWARD_VECTOR, RIGHT_VECTOR, -FORWARD_VECTOR, -RIGHT_VECTOR }; // 0x02273DB8
    const player* pp = player_get_local_player();
    const float mid_dist = (MAX_SPAWN_DISTANCE + MIN_SPAWN_DISTANCE) * 0.5f;
    cell_spawn_list.clear();
    for (const vector& direction : directions)
    {
        const vector test(pp->pos.x + direction.x * mid_dist, pp->pos.y + direction.y * mid_dist, pp->pos.z + direction.z * mid_dist);
        if (!World.is_zone_streamed(test) || World.is_zone_border_zone(test))
        {
            continue;
        }
        // Magic number: 128 cells at most from the collector.
        nav_cell* cells[129];
        const int num = havok_collect_nav_cells(test, CELL_TEST_RADIUS, cells, 128, VEHICLE_PF);
        for (int i = 0; i < num; i++)
        {
            if ((cells[i]->flags & 8) == 0)
            {
                cell_spawn_list.add(cells[i]);
                break;
            }
        }
    }
}

// 0x00771120 (gfm_reinforcement_coordinator_internal.h:424)
cached_orders_info::cached_orders_info()
{
    reset();
}

// 0x0075AFB0 (gfm_reinforcement_coordinator_internal.h:457)
bool cached_orders_info::in_use() const
{
    return last_framecount != 0;
}

// 0x0075B2A0 (:1269)
void cached_orders_info::reset()
{
    witness_handle = 0xFFFFFFFF;
    player_pos.zero();
    los_timeout.invalidate();
    green_reset_timeout.invalidate();
    last_framecount = 0;
}

// 0x00771470 (:1279)
void cached_orders_info::full_update_player_location(unsigned int npc_handle, const player* pp)
{
    los_timeout.set(ORDERS_LOS_TIMEOUT_MS);
    partial_update_player_location(npc_handle, pp);
}

// 0x0075B2D0 (:1287)
void cached_orders_info::partial_update_player_location(unsigned int npc_handle, const player* pp)
{
    witness_handle = npc_handle;
    player_pos = pp->pos;
    last_framecount = Framecount;
}

// 0x007876E0 (:1295)
bool cached_orders_info::allow_full_player_update(unsigned int& new_witness_handle)
{
    new_witness_handle = 0xFFFFFFFF;
    if (Framecount == last_framecount)
    {
        return false;
    }
    const npc* witness = witness_handle != 0xFFFFFFFF ? World.object_from_handle<npc>(witness_handle) : nullptr;
    if (witness != nullptr && witness->get_ai()->perception.any_enemy_player_in_los())
    {
        new_witness_handle = witness->get_handle();
        return true;
    }
    const int num = World.get_num_objects<npc>();
    for (int i = 0; i < num; i++)
    {
        const npc* np = World.get_object<npc>(i);
        if (human_teams_get_disposition(np, HUMAN_TEAM_EDF) == TEAM_DISPOSITION_ALLIED && np->get_ai()->perception.any_enemy_player_in_los())
        {
            new_witness_handle = np->get_handle();
            return true;
        }
    }
    return player_pos.is_null();
}

// 0x0075B300 (:1350)
bool cached_orders_info::allow_partial_player_update()
{
    if (los_timeout.valid() && !los_timeout.elapsed())
    {
        return true;
    }
    return vec_dist_squared(player_get_local_player()->pos, player_pos) <= ORDERS_NEAR_DIST_SQRD;
}

// 0x007877E0 (:1374)
const vector& cached_orders_info::get_orders_location()
{
    if (Framecount != last_framecount)
    {
        const player* pp = player_get_local_player();
        unsigned int new_witness;
        if (allow_full_player_update(new_witness))
        {
            full_update_player_location(new_witness, pp);
        }
        else if (allow_partial_player_update())
        {
            partial_update_player_location(0xFFFFFFFF, pp);
        }
    }
    return player_pos;
}

// 0x0075AF90 (gfm_reinforcement_coordinator_internal.h:360). Left uninitialized, as in rfg.exe.
road_search_info::road_search_info()
{
}

// 0x0075B350 (:1404)
void road_search_info::reset()
{
    road = nullptr;
    connection_pos.zero();
    connection_dist = 0.0f;
    base_travel_dist = 3.4028235e+38f;
}

// 0x0075AFA0 (gfm_reinforcement_coordinator_internal.h:380). Left uninitialized, as in rfg.exe.
road_node_search_info::road_node_search_info()
{
}

// 0x0075B380 (:1416)
void road_node_search_info::reset()
{
    road = nullptr;
    road_index = -1;
    node_pos.zero();
    destination_pos.zero();
    travel_dist = 3.4028235e+38f;
    total_dist_sqrd = 3.4028235e+38f;
}

// 0x007710F0 (gfm_reinforcement_coordinator_internal.h:406)
spawn_location_cache::spawn_location_cache()
{
    reset();
}

// 0x0075B3C0 (:1430)
void spawn_location_cache::reset()
{
    player_pos.zero();
    spawn_pos.zero();
    spawn_orient.make_identity();
    spawn_obj_handle = 0xFFFFFFFF;
    road_handle = 0xFFFFFFFF;
}

// 0x0075B290 (gfm_reinforcement_coordinator_internal.h). Left uninitialized, as in rfg.exe.
reinforcement_road_spawn_info::reinforcement_road_spawn_info()
{
}

// 0x007AF040 (gfm_reinforcement_coordinator_internal.h:291)
reinforcement_allocation::reinforcement_allocation(gfm_alert_levels alert_level, spawn_slot_types slot_type)
    : base_alert_level(alert_level)
    , base_slot_type(slot_type)
{
    reset();
}

// 0x0079A960
reinforcement_allocation::~reinforcement_allocation()
{
}

// 0x007A9A40 (:1445)
void reinforcement_allocation::release_and_reset()
{
    if (reinforcement_flags.resource_reserved != 0)
    {
        srm_release_all_slots(base_slot_type, RESOURCE_SLOT_USER_ID_REINFORCEMENT);
    }
    for (int i = 0; i < 4; i++)
    {
        squad_reinforcement_allocation& squad = reinforcement_squads[i];
        if (squad.squad_info != nullptr)
        {
            squad.base_squad_info.despawn_squad(NPC_DESPAWN_TYPE_LOW);
        }
    }
    reset();
}

// 0x007A2CF0 (:1466)
void reinforcement_allocation::reset()
{
    current_profile = nullptr;
    for (int i = 0; i < 4; i++)
    {
        reinforcement_squads[i].reset();
    }
    reinforcement_flags = {};
}

// 0x007B9620 (:1480)
void reinforcement_allocation::do_frame(gfm_alert_levels alert_level)
{
    if ((int)base_alert_level <= (int)alert_level)
    {
        do_frame_active();
    }
    else
    {
        do_frame_inactive();
    }
}

// 0x007A2D90 (:1489)
void reinforcement_allocation::render_minimap()
{
    for (int i = 0; i < 4; i++)
    {
        if (reinforcement_squads[i].squad_info != nullptr)
        {
            reinforcement_squads[i].render_minimap();
        }
    }
}

// 0x00787870 (:1537)
bool reinforcement_allocation::all_reinforcements_eliminated() const
{
    if (reinforcement_flags.spawned == 0)
    {
        return true;
    }
    for (int i = 0; i < 4; i++)
    {
        if (reinforcement_squads[i].squad_info != nullptr && !reinforcement_squads[i].base_squad_info.squad_despawned())
        {
            return false;
        }
    }
    return true;
}

// 0x007B94A0 (:1565)
void reinforcement_allocation::do_frame_active()
{
    for (int i = 0; i < 4; i++)
    {
        reinforcement_squads[i].base_squad_info.do_frame();
    }
    bool eliminated = true;
    if (reinforcement_flags.spawned != 0)
    {
        for (int i = 0; i < 4; i++)
        {
            if (reinforcement_squads[i].squad_info != nullptr && !reinforcement_squads[i].base_squad_info.squad_despawned())
            {
                eliminated = false;
                if (current_profile != nullptr)
                {
                    respawn_period.set(current_profile->spawn_delay_ms);
                }
                break;
            }
        }
    }
    if (eliminated && respawn_period.valid() && !respawn_period.elapsed())
    {
        return;
    }
    if (current_profile == nullptr || eliminated)
    {
        release_and_reset();
        current_profile = pick_new_profile();
        if (current_profile == nullptr)
        {
            return;
        }
        if (current_profile->reservation_group != nullptr && Mission_active_index == INVALID_MISSION_HANDLE)
        {
            if (srm_get_availible_slots(base_slot_type, RESOURCE_SLOT_USER_ID_REINFORCEMENT) < 1 || !srm_reserve_slot(base_slot_type, RESOURCE_SLOT_USER_ID_REINFORCEMENT, current_profile->reservation_group))
            {
                return;
            }
            reinforcement_flags.resource_reserved = 1;
        }
    }
    if (reinforcement_flags.spawned == 0 && !Disable_reinforcement_spawn && !activity_disable_reinforcements())
    {
        do_frame_spawn();
    }
}

// 0x007B8C80 (:1625)
bool reinforcement_allocation::do_spawn_node(squad_reinforcement_allocation& squad_info, master_spawn_info& master_info)
{
    master_info.build_node_info();
    const int num = master_info.node_spawn_list.size();
    if (num < 1)
    {
        return false;
    }
    squad_info.spawn_squad(master_info.node_spawn_list[num == 1 ? 0 : int_rand_range(0, num - 1)]);
    return true;
}

// 0x007B92F0 (:1650)
bool reinforcement_allocation::do_spawn_road(squad_reinforcement_allocation& squad_info, master_spawn_info& master_info)
{
    const player* pp = player_get_local_player();
    if (pp->vehicle_handle != 0xFFFFFFFF)
    {
        vector direction(pp->velocity.x, 0.0f, pp->velocity.z);
        if (!direction.is_null() && DIRECTIONAL_SPAWN_MIN_SPEED <= direction.mag_and_normalize_safe())
        {
            return do_spawn_road_directional(squad_info, master_info, direction);
        }
    }
    return do_spawn_road_near(squad_info, master_info);
}

// 0x007B90C0 (:1663)
bool reinforcement_allocation::do_spawn_road_directional(squad_reinforcement_allocation& squad_info, master_spawn_info& master_info, const vector& direction)
{
    // Magic number: within 60 degrees of where the player is heading (the double is rfg.exe's __real@3ff0c13fc0000000).
    static const float MIN_DIRECTION_DOT = (float)cos(1.0471975803375244); // 0x02273E58
    master_info.build_road_info();
    if (master_info.road_spawn_list.size() < 1)
    {
        return false;
    }
    const player* pp = player_get_local_player();
    const reinforcement_road_spawn_info* best = nullptr;
    float best_travel = 3.4028235e+38f;
    for (int i = 0; i < master_info.road_spawn_list.size(); i++)
    {
        const reinforcement_road_spawn_info& entry = master_info.road_spawn_list[i];
        if (entry.road_id.road_spline->road_flags.no_reinforcements != 0 || entry.travel_distance > best_travel)
        {
            continue;
        }
        vector to_road(entry.spawn_pos.x - pp->pos.x, 0.0f, entry.spawn_pos.z - pp->pos.z);
        if (to_road.is_null())
        {
            continue;
        }
        to_road.normalize_safe();
        if (MIN_DIRECTION_DOT <= direction.y * to_road.y + direction.x * to_road.x + direction.z * to_road.z)
        {
            best = &entry;
            best_travel = entry.travel_distance;
        }
    }
    if (best == nullptr)
    {
        return do_spawn_road_near(squad_info, master_info);
    }
    squad_info.spawn_squad(best->road_id);
    return true;
}

// 0x007B8CD0 (:1733)
bool reinforcement_allocation::do_spawn_road_near(squad_reinforcement_allocation& squad_info, master_spawn_info& master_info)
{
    master_info.build_road_info();
    const int num = master_info.road_spawn_list.size();
    if (num < 1)
    {
        return false;
    }
    float best_travel = 3.4028235e+38f;
    int best = -1;
    for (int i = 0; i < num; i++)
    {
        const reinforcement_road_spawn_info& entry = master_info.road_spawn_list[i];
        const object_path_road* road = entry.road_id.road_spline;
        if (road->road_flags.no_reinforcements == 0 && entry.travel_distance <= best_travel && !spawn_object_used(road->get_handle()))
        {
            best = i;
            best_travel = entry.travel_distance;
        }
    }
    if (best < 0)
    {
        return false;
    }
    squad_info.spawn_squad(master_info.road_spawn_list[best].road_id);
    return true;
}

// 0x007B8DB0 (:1791)
bool reinforcement_allocation::do_spawn_cell(squad_reinforcement_allocation& squad_info, master_spawn_info& master_info)
{
    master_info.build_cell_info();
    const int num = master_info.cell_spawn_list.size();
    if (num < 1)
    {
        return false;
    }
    squad_info.spawn_squad(master_info.cell_spawn_list[num == 1 ? 0 : int_rand_range(0, num - 1)]);
    return true;
}

// 0x007B9380 (:1816)
void reinforcement_allocation::do_spawn(squad_reinforcement_allocation& squad_info)
{
    if (squad_info.squad_info->veh_spawn_group == nullptr)
    {
        if (!do_spawn_node(squad_info, Master_ped_spawn_info) && !do_spawn_road(squad_info, Master_ped_spawn_info))
        {
            do_spawn_cell(squad_info, Master_ped_spawn_info);
        }
    }
    else if (!do_spawn_road(squad_info, Master_veh_spawn_info))
    {
        do_spawn_cell(squad_info, Master_veh_spawn_info);
    }
}

// 0x007B9420 (:1850)
void reinforcement_allocation::do_frame_spawn()
{
    if (current_profile == nullptr)
    {
        return;
    }
    reinforcement_flags.spawned = 1;
    for (int i = 0; i < 4; i++)
    {
        reinforcement_squads[i].reset();
    }
    for (int i = 0; i < current_profile->squad_info_list.size(); i++)
    {
        reinforcement_squads[i].reset();
        reinforcement_squads[i].squad_info = &current_profile->squad_info_list[i];
        do_spawn(reinforcement_squads[i]);
    }
}

// 0x007B6F20 (:1885)
void reinforcement_allocation::do_frame_inactive()
{
    if (current_profile == nullptr)
    {
        return;
    }
    if (reinforcement_flags.resource_reserved != 0)
    {
        srm_release_all_slots(base_slot_type, RESOURCE_SLOT_USER_ID_REINFORCEMENT);
        reinforcement_flags.resource_reserved = 0;
    }
    for (int i = 0; i < 4; i++)
    {
        reinforcement_squad& squad = reinforcement_squads[i].base_squad_info;
        squad.do_frame();
        if (!squad.squad_despawned())
        {
            // Original quirk kept: it lets go of the first squad still alive and waits for the next frame.
            squad.release_and_reset();
            return;
        }
    }
    if (Mission_active_index == INVALID_MISSION_HANDLE)
    {
        srm_release_all_slots(base_slot_type, RESOURCE_SLOT_USER_ID_REINFORCEMENT);
    }
    release_and_reset();
}

// 0x007714B0 (:1922)
const vehicle_spawn_group_info* reinforcement_allocation::pick_spawn_group()
{
    if (current_profile == nullptr)
    {
        return nullptr;
    }
    const vehicle_spawn_group_info* default_group = sog_default_vehicle_spawn_group(HUMAN_TEAM_EDF);
    if (default_group == nullptr)
    {
        return nullptr;
    }
    for (int i = 0; i < current_profile->squad_info_list.size(); i++)
    {
        const vehicle_spawn_group_info* group = current_profile->squad_info_list[i].veh_spawn_group;
        if (group != nullptr && default_group != group)
        {
            return group;
        }
    }
    return nullptr;
}

// 0x0079AE90 (:1953)
const reinforcement_profile* reinforcement_allocation::pick_new_profile()
{
    if (Disable_reinforcement_spawn || activity_disable_reinforcements())
    {
        return nullptr;
    }
    return pick_reinforcement_profile(base_alert_level, edf_progression_level(), base_slot_type);
}

// 0x007710B0 (gfm_reinforcement_coordinator_internal.h:161)
squad_reinforcement_profile::squad_reinforcement_profile()
{
    reset();
}

// 0x0075AF60 (gfm_reinforcement_coordinator_internal.h:177)
void squad_reinforcement_profile::reset()
{
    squad_def = nullptr;
    casual_squad_def = nullptr;
    destroy_partial = false;
    veh_spawn_group = nullptr;
}

// 0x0075B400 (:1972)
bool squad_reinforcement_profile::load(xml_element* xml_squad_root)
{
    reset();
    const xml_element* e = xml_find(xml_squad_root, "squad_def");
    if (e == nullptr)
    {
        return false;
    }
    squad_def = squad_definition_from_name(e->text);
    if (squad_def == nullptr)
    {
        return false;
    }
    e = xml_find(xml_squad_root, "casual_squad_def");
    if (e == nullptr)
    {
        casual_squad_def = squad_def;
    }
    else
    {
        casual_squad_def = squad_definition_from_name(e->text);
        if (casual_squad_def == nullptr)
        {
            return false;
        }
    }
    xml_element* partial = xml_find(xml_squad_root, "destroy_partial");
    if (partial == nullptr)
    {
        return false;
    }
    destroy_partial = xml_get_bool(partial, false);
    e = xml_find(xml_squad_root, "vehicle_group");
    veh_spawn_group = e != nullptr ? sog_get_vehicle_group(e->text) : nullptr;
    return true;
}

// 0x00787010 (gfm_reinforcement_coordinator_internal.h:189)
reinforcement_profile::reinforcement_profile()
{
    reset();
}

// 0x0075AF70 (gfm_reinforcement_coordinator_internal.h:219)
bool reinforcement_profile::has_vehicle_reservation() const
{
    return reservation_group != nullptr;
}

// 0x007710C0 (gfm_reinforcement_coordinator_internal.h:222)
void reinforcement_profile::reset()
{
    reinforcement_type = {};
    reservation_group = nullptr;
    required_group = nullptr;
    edf_progression_level = 0;
    required_alert_level = ALERT_LEVEL_GREEN;
    squad_info_list.clear();
}

// 0x00771500 (:2013)
bool reinforcement_profile::all_slots_reserved() const
{
    for (int i = 0; i < squad_info_list.size(); i++)
    {
        const vehicle_spawn_group_info* group = squad_info_list[i].veh_spawn_group;
        if (group != nullptr && !srm_vehicle_group_reserved(group))
        {
            return false;
        }
    }
    return true;
}

// 0x007878B0 (:2029)
bool reinforcement_profile::load(xml_element* xml_reinforcement_root)
{
    reset();
    const xml_element* e = xml_find(xml_reinforcement_root, "Name");
    if (e == nullptr)
    {
        return false;
    }
    name = Squad_name_string_pool.add_unique(e->text);
    // rfg.exe also looks up the default group here and drops the answer.
    sog_default_vehicle_spawn_group(HUMAN_TEAM_EDF);
    e = xml_find(xml_reinforcement_root, "alert_level");
    if (e == nullptr)
    {
        return false;
    }
    required_alert_level = alm_resolve_alert_level(e->text);
    if ((int)required_alert_level < 0 || (int)required_alert_level >= 4)
    {
        return false;
    }
    xml_element* level = xml_find(xml_reinforcement_root, "progression_level");
    if (level == nullptr)
    {
        return false;
    }
    const int progression = xml_get_int(level, 0);
    edf_progression_level = progression < 0 ? 0 : progression;
    xml_element* type = xml_find(xml_reinforcement_root, "reinforcement_type");
    if (type == nullptr)
    {
        return false;
    }
    reinforcement_type.on_foot = flag_exists_in_xml(type, "On Foot") ? 1 : 0;
    reinforcement_type.in_vehicle = flag_exists_in_xml(type, "In Vehicle") ? 1 : 0;
    reinforcement_type.flyer = flag_exists_in_xml(type, "Flyer") ? 1 : 0;
    e = xml_find(xml_reinforcement_root, "vehicle_reservation");
    reservation_group = e != nullptr ? sog_get_vehicle_group(e->text) : nullptr;
    xml_element* delay = xml_find(xml_reinforcement_root, "spawn_delay");
    if (delay == nullptr)
    {
        return false;
    }
    const int delay_ms = xml_get_int(delay, 0) * 1000;
    spawn_delay_ms = delay_ms < 0 ? 0 : (delay_ms > MAX_SPAWN_DELAY_MS ? MAX_SPAWN_DELAY_MS : delay_ms);
    e = xml_find(xml_reinforcement_root, "required_group");
    required_group = e != nullptr ? sog_get_vehicle_group(e->text) : nullptr;
    xml_element* list = xml_find(xml_reinforcement_root, "squad_list");
    if (list == nullptr)
    {
        return false;
    }
    for (xml_element* cur = xml_find(list, "squad_info"); cur != nullptr; cur = xml_find_next(list, cur, "squad_info"))
    {
        squad_reinforcement_profile squad;
        if (!squad.load(cur))
        {
            // rfg.exe stops here with its Int3 message.
            return false;
        }
        squad_info_list.add(squad);
    }
    return squad_info_list.size() > 0;
}

// 0x007A9A10 (gfm_reinforcement_coordinator_internal.h:235)
squad_reinforcement_allocation::squad_reinforcement_allocation()
{
    reset();
}

// 0x0077B000
squad_reinforcement_allocation::~squad_reinforcement_allocation()
{
}

// 0x0075AF80 (gfm_reinforcement_coordinator_internal.h:262)
bool squad_reinforcement_allocation::is_in_use() const
{
    return squad_info != nullptr;
}

// 0x007A2CD0 (gfm_reinforcement_coordinator_internal.h:273)
void squad_reinforcement_allocation::reset()
{
    squad_info = nullptr;
    spawn_obj_handle = 0xFFFFFFFF;
    base_squad_info.release_and_reset();
}

// 0x007B39A0 (:2113)
void squad_reinforcement_allocation::do_frame()
{
    base_squad_info.do_frame();
}

// 0x007B8670 (:2119)
void squad_reinforcement_allocation::spawn_squad(const road_spline_identifier& road_id)
{
    squad_spawn_parameters params;
    const object_path_road* road = road_id.road_spline;
    if (road == nullptr)
    {
        return;
    }
    const vector spawn_pos = road->get_road_point(road_id.point);
    params.squad.squad_pos = spawn_pos;
    // Face along the road, towards the side that leads to the player.
    unsigned short next_point;
    if (road_id.point == 0)
    {
        next_point = 1;
    }
    else if ((int)road_id.point >= road->spline_data.num_points - 1)
    {
        next_point = (unsigned short)(road->spline_data.num_points - 2);
    }
    else if (3.4028235e+38f <= road_info_player_travel_distance(road->get_handle(), road_id.point))
    {
        next_point = bool_rand() ? road_id.point + 1 : road_id.point - 1;
    }
    else
    {
        const float forward = road_info_player_travel_distance(road->get_handle(), road_id.point + 1);
        const float back = road_info_player_travel_distance(road->get_handle(), road_id.point - 1);
        next_point = forward <= back ? road_id.point + 1 : road_id.point - 1;
    }
    const vector& next = road->get_road_point(next_point);
    const vector direction(next.x - spawn_pos.x, 0.0f, next.z - spawn_pos.z);
    if (!direction.is_null())
    {
        params.squad.squad_orient.make(direction);
    }
    spawn_obj_handle = road->get_handle();
    squad_flags.road_spawn = 1;
    squad_flags.cell_spawn = 0;
    squad_flags.node_spawn = 0;
    spawn_squad(params);
}

// 0x007B8850 (:2181)
void squad_reinforcement_allocation::spawn_squad(const object_squad_spawn_node* squad_spawn_node)
{
    if (squad_spawn_node == nullptr)
    {
        return;
    }
    const player* pp = player_get_local_player();
    squad_spawn_parameters params;
    params.squad.squad_pos = squad_spawn_node->pos;
    const vector direction(pp->pos.x - squad_spawn_node->pos.x, 0.0f, pp->pos.z - squad_spawn_node->pos.z);
    if (!direction.is_null())
    {
        params.squad.squad_orient.make(direction);
    }
    squad_flags.road_spawn = 0;
    squad_flags.cell_spawn = 0;
    squad_flags.node_spawn = 1;
    spawn_obj_handle = squad_spawn_node->get_handle();
    spawn_squad(params);
}

// 0x007B8950 (:2212)
void squad_reinforcement_allocation::spawn_squad(const nav_cell* spawn_cell)
{
    if (spawn_cell == nullptr)
    {
        return;
    }
    const player* pp = player_get_local_player();
    squad_spawn_parameters params;
    vector centroid;
    nav_cell_get_centroid(&centroid, spawn_cell);
    params.squad.squad_pos = centroid;
    const vector direction(pp->pos.x - centroid.x, 0.0f, pp->pos.z - centroid.z);
    if (!direction.is_null())
    {
        params.squad.squad_orient.make(direction);
    }
    squad_flags.road_spawn = 0;
    squad_flags.cell_spawn = 1;
    squad_flags.node_spawn = 0;
    spawn_obj_handle = 0xFFFFFFFF;
    spawn_squad(params);
}

// 0x007B6FC0 (:2247)
void squad_reinforcement_allocation::spawn_squad(squad_spawn_parameters& squad_params)
{
    const player* pp = player_get_local_player();
    squad_flags.spawned = 1;
    if (squad_info == nullptr)
    {
        return;
    }
    base_squad_info.release_and_reset();
    squad_params.squad.squad_def = difficulty_get_level() == DIFFICULTY_LEVEL_EASY ? squad_info->casual_squad_def : squad_info->squad_def;
    squad_params.squad.squad_flags.reinforcement_squad = 1;
    squad_params.squad.veh_spawn_group = squad_info->veh_spawn_group;
    squad_params.orders.orders_type = GFM_ORDERS_TYPE_ATTACK;
    squad_params.orders.object_target = pp->get_handle();
    squad_params.orders.location_target = pp->pos;
    squad_params.orders.ofp = NORMAL_REINFORCEMENT_FLAGS;
    if (base_squad_info.spawn_squad(pp->get_handle(), squad_params, true, squad_info->destroy_partial))
    {
        spawned_pos = squad_params.squad.squad_pos;
    }
    else
    {
        spawned_pos.zero();
    }
}

// 0x0079AEF0 (:2287)
void squad_reinforcement_allocation::render_minimap()
{
    if (!Render_reinforcement_icon || base_squad_info.squad_handle == 0xFFFFFFFF)
    {
        return;
    }
    const object_squad* squad = World.object_from_handle<object_squad>(base_squad_info.squad_handle);
    if (squad == nullptr)
    {
        return;
    }
    if (squad->simulated())
    {
        Cartographer.add_temporary_icon(squad->pos, MAP_ELEMENT_STAR_RED_ICON, true, 0xFFFFFFFF, Default_map_item_color, false, 0xFFFFFFFF);
        return;
    }
    farray<vehicle const*, 10> vehicles;
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        const unsigned int handle = squad->member_array[i];
        const npc* np = handle != 0xFFFFFFFF ? World.object_from_handle<npc>(handle) : nullptr;
        if (np != nullptr && np->vehicle_handle != 0xFFFFFFFF)
        {
            const vehicle* vp = World.object_from_handle<vehicle>(np->vehicle_handle);
            if (vp != nullptr)
            {
                vehicles.add_unique(vp);
            }
        }
    }
    for (int i = 0; i < vehicles.size(); i++)
    {
        Cartographer.add_temporary_icon(vehicles[i]->pos, MAP_ELEMENT_STAR_RED_ICON, true, 0xFFFFFFFF, Default_map_item_color, false, 0xFFFFFFFF);
    }
}

// 0x00771540 (:2395)
void rfc_dlc_shutdown()
{
    DLC_reinforcement_profiles.clear();
}

// 0x007A2DC0 (:2402)
void rfc_system_init(int dlc_id)
{
    if (Reinforcement_name_string_pool_data == nullptr)
    {
        Reinforcement_name_string_pool_data = static_cast<char*>(Permanent_mempool.alloc(REINFORCEMENT_NAME_POOL_SIZE, 0));
        Squad_name_string_pool.init(Reinforcement_name_string_pool_data, REINFORCEMENT_NAME_POOL_SIZE, "Reinforcement String Pool", 0, 0);
    }
    Reinforcement_profile_pool.clear(0);
    xml_element* root = xml_table_parse("reinforcement_profile.xtbl");
    if (root == nullptr)
    {
        return;
    }
    for (xml_element* cur = xml_find(root, "ReinforcementProfile"); cur != nullptr; cur = xml_find_next(root, cur, "ReinforcementProfile"))
    {
        reinforcement_profile* profile = Reinforcement_profile_pool.alloc(0);
        if (profile == nullptr)
        {
            break;
        }
        profile->reset();
        if (!profile->load(cur))
        {
            Reinforcement_profile_pool.free(profile);
        }
    }
    xml_table_close(root);
    guerrilla_reinforcement_system_init(dlc_id);
}

// 0x007A9AB0 (:2447)
void rfc_level_init_pre()
{
    Yellow_alert_orders_handle = 0xFFFFFFFF;
    Orange_alert_orders_handle = 0xFFFFFFFF;
    Last_alert_level_coordinator = ALERT_LEVEL_GREEN;
    Orange_reinforcements.release_and_reset();
    Red_reinforcements.release_and_reset();
    for (int i = 0; i < 4; i++)
    {
        Foot_reinforcement_cache[i].reset();
        Vehicle_reinforcement_cache[i].reset();
    }
    Orders_info.reset();
    Disable_reinforcement_spawn = false;
    Disable_guerrilla_reinforcement = false;
    Guerrilla_reinforcement_squad.reset(0);
}

// 0x007B9640 (:2473)
void rfc_do_frame()
{
    static timestamp polling_period; // 0x02273E60
    const gfm_alert_levels current_alert = gsm_get_alert_level();
    if (Last_alert_level_coordinator == current_alert && polling_period.valid() && !polling_period.elapsed())
    {
        return;
    }
    Last_alert_level_coordinator = current_alert;
    // Magic numbers: every 500 ms to 1.5 s.
    polling_period.set_rand(500, 1500);
    if ((int)current_alert < 1 && Orders_info.last_framecount != 0)
    {
        if (!Orders_info.green_reset_timeout.valid())
        {
            Orders_info.green_reset_timeout.set(ORDERS_GREEN_RESET_MS);
        }
        else if (Orders_info.green_reset_timeout.elapsed())
        {
            Orders_info.reset();
        }
    }
    Master_veh_spawn_info.invalidate();
    Master_ped_spawn_info.invalidate();
    do_frame_assigned_alert_orders(current_alert, ALERT_LEVEL_YELLOW, Yellow_alert_orders_handle);
    do_frame_assigned_alert_orders(current_alert, ALERT_LEVEL_ORANGE, Orange_alert_orders_handle);
    Orange_reinforcements.do_frame(current_alert);
    Red_reinforcements.do_frame(current_alert);
    guerrilla_reinforcement_do_frame(current_alert);
}

// 0x00787B10 (:2529), spawn_info in ECX
// Walks the roads connected to the closest one and takes the point in range with the shortest travel to the player.
static bool find_spawn_location_road(team_spawn_location& spawn_info, float min_spawn_sqrd, float max_spawn_sqrd)
{
    spawn_info.reset();
    const vector& player_pos = player_get_local_player()->pos;
    vector closest;
    float closest_u;
    const object_path_road* closest_road = object_road_get_closest_road_zone(player_pos, false, closest, closest_u, rfg_zonex_get_grid_xy(player_pos), false, false);
    if (closest_road == nullptr || vec_dist_squared(player_pos, closest) > max_spawn_sqrd)
    {
        return false;
    }
    farray<road_search_info, 322> search;
    farray<object_path_road const*, 322> visited;
    road_search_info first;
    first.reset();
    first.road = closest_road;
    first.connection_pos = closest;
    first.base_travel_dist = vec_dist(player_pos, closest);
    first.connection_dist = closest_u;
    search.add(first);
    visited.add(closest_road);

    road_node_search_info best;
    best.reset();
    for (int s = 0; s < search.size(); s++)
    {
        const road_search_info current = search[s];
        const object_path_road* road = current.road;
        const road_spline_data& data = road->spline_data;
        const road_spline_point* points = data.points.to_pointer();
        for (int i = 0; i < data.num_points; i++)
        {
            const vector& point = points[i].pos;
            const float dist = vec_dist_squared(player_pos, point);
            if (dist < min_spawn_sqrd || max_spawn_sqrd < dist)
            {
                continue;
            }
            const float travel = vec_dist(current.connection_pos, point) + current.base_travel_dist;
            if (travel > best.travel_dist)
            {
                continue;
            }
            // The squad faces the way that leads back to where this road was entered.
            int destination;
            if (i == 0)
            {
                destination = 1;
            }
            else if (data.num_points - 1 == i || current.connection_dist <= (float)i)
            {
                destination = i - 1;
            }
            else
            {
                destination = i + 1;
            }
            best.road = road;
            best.road_index = i;
            best.node_pos = point;
            best.destination_pos = points[destination].pos;
            best.travel_dist = travel;
            best.total_dist_sqrd = dist;
        }
        const road_spline_connection* connections = data.connections.to_pointer();
        for (int c = 0; c < data.num_connections; c++)
        {
            const object_path_road* other = connections[c].handle != 0xFFFFFFFF ? World.object_from_handle<object_path_road>(connections[c].handle) : nullptr;
            if (other == nullptr || visited.find(other, false) > -1)
            {
                continue;
            }
            visited.add(other);
            const vector& connection_pos = points[connections[c].from_pt].pos;
            if (vec_dist_squared(player_pos, connection_pos) > max_spawn_sqrd)
            {
                continue;
            }
            const float travel = vec_dist(current.connection_pos, connection_pos) + current.base_travel_dist;
            if (travel > best.travel_dist)
            {
                continue;
            }
            road_search_info next;
            next.road = other;
            next.connection_pos = connection_pos;
            // Original quirk kept: the index of the connection on this road, later read as a position on the other road.
            next.connection_dist = (float)connections[c].from_pt;
            next.base_travel_dist = travel;
            search.add(next);
        }
    }
    if (best.road == nullptr)
    {
        return false;
    }
    spawn_info.road = best.road;
    spawn_info.spawn_pos = best.node_pos;
    const vector direction(best.destination_pos.x - best.node_pos.x, 0.0f, best.destination_pos.z - best.node_pos.z);
    if (!direction.is_null())
    {
        spawn_info.spawn_orient.make(direction);
    }
    return true;
}

// 0x00788280 (:2679)
static bool find_spawn_location_squad_spawn(human_teams team, bool vehicle_spawn, team_spawn_location& spawn_info, float min_spawn_sqrd, float max_spawn_sqrd)
{
    const player* pp = player_get_local_player();
    const int num = World.get_num_objects<object_squad_spawn_node>();
    for (int i = 0; i < num; i++)
    {
        const object_squad_spawn_node* node = World.get_object<object_squad_spawn_node>(i);
        if (human_teams_get_disposition(team, node->get_team()) <= TEAM_DISPOSITION_NONE || (vehicle_spawn && node->get_veh_info() == nullptr))
        {
            continue;
        }
        const float dist = vec_dist_squared(pp->pos, node->pos);
        if (min_spawn_sqrd < dist && dist < max_spawn_sqrd)
        {
            spawn_info.spawn_objp = node;
            spawn_info.spawn_pos = node->pos;
            spawn_info.spawn_orient = node->orient;
            return true;
        }
    }
    return false;
}

// 0x007883A0 (:2723), team in ECX
static bool find_spawn_location_generic(const vector& desired_pos, human_teams team, bool vehicle_spawn, team_spawn_location& spawn_info, float min_spawn_sqrd, float max_spawn_sqrd)
{
    const player* pp = player_get_local_player();
    if (vehicle_spawn)
    {
        if (find_spawn_location_road(spawn_info, min_spawn_sqrd, max_spawn_sqrd) || find_spawn_location_squad_spawn(team, vehicle_spawn, spawn_info, min_spawn_sqrd, max_spawn_sqrd))
        {
            return true;
        }
    }
    else if (find_spawn_location_squad_spawn(team, false, spawn_info, min_spawn_sqrd, max_spawn_sqrd) || find_spawn_location_road(spawn_info, min_spawn_sqrd, max_spawn_sqrd))
    {
        return true;
    }
    nav_cell* start = bb_pf_get_nav_cell(desired_pos, vehicle_spawn, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
    if (start == nullptr)
    {
        start = bb_pf_get_nav_cell(pp->pos, vehicle_spawn, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
        if (start == nullptr)
        {
            return false;
        }
    }
    // A breadth first walk over the mesh, out to the far edge of the range.
    static farray<nav_cell*, 2048> open_cells;
    static farray<nav_cell*, 2048> good_cells;
    open_cells.clear();
    good_cells.clear();
    const unsigned int session = Nav_heap.start_session();
    start->heap_session_id = session;
    open_cells.add(start);
    for (int i = 0; i < open_cells.size(); i++)
    {
        nav_cell* cell = open_cells[i];
        vector centroid;
        nav_cell_get_centroid(&centroid, cell);
        const float dist = vec_dist_squared(pp->pos, centroid);
        if ((cell->flags & 8) == 0 && min_spawn_sqrd < dist && dist < max_spawn_sqrd)
        {
            good_cells.add(cell);
            continue;
        }
        if (open_cells.size() >= open_cells.capacity() || !(dist < max_spawn_sqrd))
        {
            continue;
        }
        for (int a = 0; a < 3; a++)
        {
            nav_cell* adjacent = cell->adjacent_cells[a].to_pointer();
            if (adjacent != nullptr && adjacent->heap_session_id != session && open_cells.size() < open_cells.capacity())
            {
                adjacent->heap_session_id = session;
                open_cells.add(adjacent);
            }
        }
    }
    Nav_heap.end_session();
    if (good_cells.size() < 1)
    {
        return false;
    }
    nav_cell_get_centroid(&spawn_info.spawn_pos, good_cells[good_cells.size() == 1 ? 0 : int_rand_range(0, good_cells.size() - 1)]);
    return true;
}

// 0x00771550 (:2832)
bool can_use_cached_location(team_spawn_location& spawn_info, const spawn_location_cache& cached_location)
{
    if (cached_location.player_pos.is_null() || !(vec_dist_squared(player_get_local_player()->pos, cached_location.player_pos) <= CACHED_LOCATION_DIST_SQRD))
    {
        return false;
    }
    spawn_info.reset();
    if (cached_location.spawn_obj_handle != 0xFFFFFFFF)
    {
        spawn_info.spawn_objp = World.object_from_handle(cached_location.spawn_obj_handle);
        if (spawn_info.spawn_objp == nullptr)
        {
            return false;
        }
    }
    if (cached_location.road_handle != 0xFFFFFFFF)
    {
        spawn_info.road = World.object_from_handle<object_path_road>(cached_location.road_handle);
        if (spawn_info.road == nullptr)
        {
            return false;
        }
    }
    spawn_info.spawn_pos = cached_location.spawn_pos;
    spawn_info.spawn_orient = cached_location.spawn_orient;
    return true;
}

// 0x0075B4C0 (:2874), spawn_info in EAX, cached_location in EDI
static void save_to_cache(const team_spawn_location& spawn_info, spawn_location_cache& cached_location)
{
    cached_location.reset();
    cached_location.player_pos = player_get_local_player()->pos;
    cached_location.spawn_pos = spawn_info.spawn_pos;
    cached_location.spawn_orient = spawn_info.spawn_orient;
    if (spawn_info.road != nullptr)
    {
        cached_location.road_handle = spawn_info.road->get_handle();
    }
    if (spawn_info.spawn_objp != nullptr)
    {
        cached_location.spawn_obj_handle = spawn_info.spawn_objp->get_handle();
    }
}

// 0x007886A0 (:2903)
bool reinforcement_find_spawn_location(const vector& desired_pos, const matrix& desired_orient, human_teams team, bool vehicle, team_spawn_location& spawn_info)
{
    if ((int)team <= -1 || (int)team >= 4)
    {
        return false;
    }
    spawn_info.reset();
    const float dist = vec_dist_squared(player_get_local_player()->pos, desired_pos);
    const float min_dist = vehicle ? MIN_VEH_REINFORCEMENT_SPAWN_DIST_SQRD : MIN_PED_REINFORCEMENT_SPAWN_DIST_SQRD;
    const float max_dist = vehicle ? MAX_VEH_REINFORCEMENT_SPAWN_DIST_SQRD : MAX_PED_REINFORCEMENT_SPAWN_DIST_SQRD;
    if (min_dist < dist)
    {
        spawn_info.spawn_pos = desired_pos;
        spawn_info.spawn_orient = desired_orient;
        return true;
    }
    spawn_location_cache& cache = vehicle ? Vehicle_reinforcement_cache[team] : Foot_reinforcement_cache[team];
    if (can_use_cached_location(spawn_info, cache))
    {
        return true;
    }
    if (!find_spawn_location_generic(desired_pos, team, vehicle, spawn_info, min_dist, max_dist))
    {
        return false;
    }
    save_to_cache(spawn_info, cache);
    return true;
}

// 0x00788880 (:2969)
bool reinforcement_find_destination_location(const vector& squad_pos, const vector& desired_dest_pos, bool vehicle, vector& destination_pos)
{
    nav_cell* start = bb_pf_get_nav_cell(desired_dest_pos, false, nullptr, false, NAV_CELL_TRAVERSAL_ALL, nullptr);
    if (start == nullptr)
    {
        return false;
    }
    farray<nav_cell const*, 256> cells;
    cells.add(start);
    const nav_cell* best = nullptr;
    vector best_pos;
    best_pos.zero();
    float best_dist = 3.4028235e+38f;
    for (int i = 0; i < cells.size(); i++)
    {
        const nav_cell* cell = cells[i];
        vector centroid;
        nav_cell_get_centroid(&centroid, cell);
        const float dist = vec_dist_squared(squad_pos, centroid);
        if (!(dist <= best_dist))
        {
            continue;
        }
        if (can_use_nav_cell(centroid, cell, vehicle))
        {
            best_pos = centroid;
            best_dist = dist;
            best = cell;
            continue;
        }
        for (int a = 0; a < 3 && cells.size() < cells.capacity(); a++)
        {
            const nav_cell* adjacent = cell->adjacent_cells[a].to_pointer();
            if (adjacent != nullptr)
            {
                cells.add_unique(adjacent);
            }
        }
    }
    if (best == nullptr)
    {
        return false;
    }
    destination_pos = best_pos;
    return true;
}

// 0x0075B570 (:3038)
void reinforcement_set_disable(const bool disabled)
{
    Disable_reinforcement_spawn = disabled;
}

// 0x007A2EF0 (:3044)
void rfc_render_minimap()
{
    Orange_reinforcements.render_minimap();
    Red_reinforcements.render_minimap();
}

// 0x00788A60 (:3051)
const vector& reinforcement_get_player_pos()
{
    return Orders_info.get_orders_location();
}

// 0x0075B580 (:3057)
bool reinforcement_should_render_icon()
{
    return Render_reinforcement_icon;
}

// 0x0075B590 (:3063)
void reinforcement_guerrilla_disabled(bool disabled)
{
    Disable_guerrilla_reinforcement = disabled;
}

// 0x00A1CE20 (team_spawn_location's constructor)
team_spawn_location::team_spawn_location()
{
    reset();
}

// 0x0075B5A0 (:3074)
void team_spawn_location::reset()
{
    spawn_objp = nullptr;
    road = nullptr;
    spawn_pos.zero();
    spawn_orient.make_identity();
}
