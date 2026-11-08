// Original: volition/rfg/code/gameplay/marauder_raid.cpp
//
// Marauder raids at night: each evaluated part of the night may pick the nearest raid route, spawn a marauder squad on it
// and cancel it when the squad reaches the last node. Howls play once a night, near the player, in the grid cells that ask
// for them.

#include "volition/rfg/code/gameplay/marauder_raid.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gameplay/gameflow/gameflow_enums.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/mission_info_shared.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/gameplay/squad_spawn.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_patrol.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

#include <float.h>

extern mission_handle Mission_active_index; // 0x0165150C (gameplay.cpp)
extern gfm_alert_levels Current_alert_level; // 0x022278B4 (alert_level_manager.cpp)

// ---- file statics
// Not static: gameplay_do_frame inlines the raid test and reads both.
unsigned int Raid_patrol_handle = 0xFFFFFFFF; // 0x016514F0
unsigned int Raid_squad_handle = 0xFFFFFFFF; // 0x016514F4
static int Marauder_howl_sound; // 0x016514F8
static timestamp Spawn_timeout; // 0x01658048
static timestamp Squad_spawn_timeout; // 0x0165804C
static timestamp Howl_delay_timer; // 0x01658050
static bool Played_howl_tonight; // 0x0222964A
static game_clock_tod_partitioned Last_tod_evaluated; // 0x0222965C
static game_clock_tod_partitioned Last_howl_tod_evaluated; // 0x02229660
static patrol_node_handle Last_patrol_node_handle; // 0x0224FD14

// Tweak table entry, written by the table's load (0x0222DE28).
static float Marauder_raid_prob = 0.5f; // 0x016514FC
static tweak_table_item Tweak_table_item_Marauder_raid_prob("Marauder_raid_prob", &Marauder_raid_prob);

// 0x0078DC00 (:85)
// The streamed raid route with a node nearest the player, within 100 m of it.
static const object_patrol* find_nearest_raid_route()
{
    farray<navpoint*,512> nodes;
    const object_patrol* best = nullptr;
    float best_dist = FLT_MAX;
    const player* pp = player_get_local_player();
    const int count = World.get_num_objects<object_patrol>();
    for (int i = 0; i < count; i++)
    {
        object_patrol* patrol = World.get_object<object_patrol>(i);
        if (!patrol->marauder_raid_route() || !World.is_zone_streamed(patrol->get_handle()))
        {
            continue;
        }
        nodes.clear();
        if (!patrol->get_all_patrol_nodes(nodes) || nodes.size() <= 0)
        {
            continue;
        }
        float nearest_node = vec_dist_squared(nodes[0]->pos, pp->pos);
        for (int j = 1; j < nodes.size(); j++)
        {
            const float dist = vec_dist_squared(nodes[j]->pos, pp->pos);
            if (dist < nearest_node)
            {
                nearest_node = dist;
            }
        }
        if (nearest_node < best_dist)
        {
            best_dist = nearest_node;
            best = patrol;
        }
    }
    // Magic number: 100 m squared.
    return best_dist > 10000.0f ? nullptr : best;
}

// 0x0079D630 (:144)
void cancel_current_raid()
{
    if (Raid_patrol_handle == 0xFFFFFFFF && Raid_squad_handle == 0xFFFFFFFF)
    {
        return;
    }
    if (Raid_squad_handle != 0xFFFFFFFF)
    {
        object_squad* squad = World.object_from_handle<object_squad>(Raid_squad_handle);
        if (squad != nullptr)
        {
            gfm_orders* orders = squad->get_squad_orders();
            if (orders != nullptr)
            {
                Orders_manager.master_orders_pool.free(orders);
            }
            squad->request_despawn_all_members(NPC_DESPAWN_TYPE_HIGH, nullptr, false);
        }
    }
    Raid_patrol_handle = 0xFFFFFFFF;
    Raid_squad_handle = 0xFFFFFFFF;
    Spawn_timeout.invalidate();
    Squad_spawn_timeout.invalidate();
    Last_patrol_node_handle.reset();
}

// 0x0075E940 (:173): empty in rfg.exe.
static void do_frame_run_existing_raid()
{
}

// 0x007B4830 (:179)
// Spawns the squad on the selected route while the spawn timeout runs, and cancels the raid when anything goes wrong.
void do_frame_spawn_selected_raid()
{
    static const squad_definition* MARAUDER_SQUAD_DEFl = squad_definition_from_name("marauder_squad_4"); // 0x02273E3C
    if (MARAUDER_SQUAD_DEFl == nullptr)
    {
        return;
    }
    object_patrol* patrol = Raid_patrol_handle != 0xFFFFFFFF ? World.object_from_handle<object_patrol>(Raid_patrol_handle) : nullptr;
    if (patrol != nullptr && Spawn_timeout.valid() && !Spawn_timeout.elapsed())
    {
        // Wait for the slots, and let the timeout cancel the raid if they never come.
        if (srm_get_availible_slots(SPAWN_SLOT_VEHICLE_MULTI_PURPOSE, RESOURCE_SLOT_USER_ID_SQUAD) < 1 || srm_get_availible_slots(SPAWN_SLOT_HUMAN_MARAUDER, RESOURCE_SLOT_USER_ID_SQUAD) < 1)
        {
            return;
        }
        squad_spawn_parameters params;
        params.orders.orders_type = GFM_ORDERS_TYPE_PATROL;
        params.orders.location_target = patrol->pos;
        params.orders.object_target = patrol->get_handle();
        params.squad.squad_pos = patrol->pos;
        params.squad.squad_orient = patrol->orient;
        params.squad.squad_def = MARAUDER_SQUAD_DEFl;
        params.squad.veh_spawn_group = sog_default_vehicle_spawn_group(HUMAN_TEAM_MARAUDER);
        params.squad.hflags.disallow_vehicle_exit = 1;
        params.squad.squad_flags.disallow_reassignment = 1;
        const object_squad* squad = params.spawn_squad();
        if (squad != nullptr)
        {
            Raid_squad_handle = squad->get_handle();
            // Magic number: 5 s.
            Squad_spawn_timeout.set(5000);
            if (patrol->get_last_patrol_node_handle(Last_patrol_node_handle))
            {
                return;
            }
        }
    }
    cancel_current_raid();
}

// 0x0075E950 (:249)
static bool spawn_new_marauder_raid(const object_patrol* patrol)
{
    if (Raid_patrol_handle != 0xFFFFFFFF)
    {
        return false;
    }
    Raid_patrol_handle = patrol->get_handle();
    // Magic number: 5 s to spawn the squad.
    Spawn_timeout.set(5000);
    return true;
}

// 0x0078DDA0 (:259)
// A howl 30 m from the player, to a random side in the forward quadrant. Original: played_tonight is set even if the
// player's cell does not ask for one.
static void play_marauder_howl()
{
    vector dir = FORWARD_VECTOR;
    Played_howl_tonight = true;
    Last_howl_tod_evaluated = GC_EARLY_DAY;
    Howl_delay_timer.invalidate();
    // Magic number: PI_OVER_2.
    dir.rotate_around_y_axis(fl_rand_range(0.0f, 1.5707964f));
    const player* pp = player_get_local_player();
    if (pp != nullptr && district_play_howl_at_pos(pp->pos))
    {
        const vector pos(pp->pos.x + dir.x * 30.0f, pp->pos.y + dir.y * 30.0f, pp->pos.z + dir.z * 30.0f);
        audiolib_cue_play_3d(Marauder_howl_sound, &pos, nullptr, nullptr, nullptr);
    }
}

// 0x0078DF10 (:286)
// Half a chance each part of the night, certain in the late night.
static void do_frame_marauder_howl(game_clock_tod_partitioned current_tod)
{
    if (Played_howl_tonight)
    {
        return;
    }
    if (Howl_delay_timer.valid() && !Howl_delay_timer.elapsed())
    {
        return;
    }
    Howl_delay_timer.invalidate();
    if (current_tod != Last_howl_tod_evaluated)
    {
        const float chance = current_tod == GC_LATE_NIGHT ? 1.0f : 0.5f;
        Last_howl_tod_evaluated = current_tod;
        if (fl_rand() <= chance)
        {
            play_marauder_howl();
        }
    }
}

// 0x0078DF90 (:316)
// Only at night; a new part of the night rolls for a raid, and otherwise the howl gets its turn. A route starting within
// 70 m of the player is not used.
void do_frame_spawn_new_raid()
{
    if (game_clock_get_time_of_day() != GC_NIGHT)
    {
        Played_howl_tonight = false;
        return;
    }
    const game_clock_tod_partitioned tod = game_clock_get_tod_partitioned();
    if (tod == Last_tod_evaluated)
    {
        do_frame_marauder_howl(tod);
        return;
    }
    Last_tod_evaluated = tod;
    if (fl_rand() <= Marauder_raid_prob)
    {
        const object_patrol* patrol = find_nearest_raid_route();
        if (patrol != nullptr)
        {
            const player* pp = player_get_local_player();
            // Magic number: 70 m squared.
            if (vec_dist_squared(pp->pos, patrol->pos) >= 4900.0f)
            {
                spawn_new_marauder_raid(patrol);
            }
        }
    }
}

// 0x007A5840 (:370)
void marauder_raid_level_init_post()
{
    Raid_patrol_handle = 0xFFFFFFFF;
    Raid_squad_handle = 0xFFFFFFFF;
    Last_tod_evaluated = GC_EARLY_DAY;
    Spawn_timeout.invalidate();
    Squad_spawn_timeout.invalidate();
    // Magic numbers: the first howl waits 30-180 s.
    Howl_delay_timer.set_rand(30000, 180000);
    notify_info info;
    info.data = nullptr;
    info.callback = marauder_raid_report_patrol;
    info.type = EVENT_PATROL_SQUAD_REACHED_NODE;
    gem_register_for_event(info);
    audiolib_cue_get_id("AMB_MARAUDER_HOWL", &Marauder_howl_sound);
}

// 0x0079D6B0 (:390)
event_result marauder_raid_report_patrol(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() == EVENT_PATROL_SQUAD_REACHED_NODE)
    {
        const patrol_squad_reached_node_event& event = (const patrol_squad_reached_node_event&)in_event;
        if (event.squad_handle != 0 && Raid_squad_handle != 0xFFFFFFFF && Raid_patrol_handle != 0xFFFFFFFF && Raid_squad_handle == event.squad_handle && event.patrol_node->nav_handle == Last_patrol_node_handle.nav_handle)
        {
            cancel_current_raid();
            return EVENT_OK;
        }
    }
    return EVENT_IGNORED;
}

// 0x007B49D0 (:419)
void marauder_raid_do_frame()
{
    // Magic number: above alert level 2 raids stop. rfg.exe's test also has a redundant range check on the level.
    if ((int)Current_alert_level > 2 || Mission_active_index != INVALID_MISSION_HANDLE)
    {
        cancel_current_raid();
        return;
    }
    if (Raid_patrol_handle == 0xFFFFFFFF)
    {
        do_frame_spawn_new_raid();
    }
    else if (Raid_squad_handle == 0xFFFFFFFF)
    {
        do_frame_spawn_selected_raid();
    }
    else
    {
        do_frame_run_existing_raid();
    }
}

// 0x0075E980 (:445)
void marauder_raid_set_howl_delay()
{
    Howl_delay_timer.set_rand(30000, 180000);
}
