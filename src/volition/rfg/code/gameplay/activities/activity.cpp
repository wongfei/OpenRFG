// Original: volition/rfg/code/gameplay/activities/activity.cpp
//
// The activity framework (gameflow.md GF4), whole. All sixteen activities are in All_activities, and only ACTIVITY_MODE_NONE's
// slot is null.

#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_internal.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/event_logging/definitions_events/events.h"
#include "volition/rfg/code/gameplay/activities/activity_air_strike_defense_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_assassin_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_car_bomb.h"
#include "volition/rfg/code/gameplay/activities/activity_raid_attack_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_raid_defend_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_area_defense_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_continuous_attack_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_convoy.h"
#include "volition/rfg/code/gameplay/activities/activity_convoy_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_courier_chase_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_delivery.h"
#include "volition/rfg/code/gameplay/activities/activity_delivery_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_demolitions_master_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_house_arrest.h"
#include "volition/rfg/code/gameplay/activities/activity_house_arrest_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_killing_spree_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_mayhem_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_mission_vrail_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_raid.h"
#include "volition/rfg/code/gameplay/activities/activity_raid_attack.h"
#include "volition/rfg/code/gameplay/activities/activity_raid_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_register_damage.h"
#include "volition/rfg/code/gameplay/activities/activity_tanks_a_lot_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_vrail_internal.h"
#include "volition/rfg/code/gameplay/activities/delivery_common.h"
#include "volition/rfg/code/gameplay/activities/raid_common.h"
#include "volition/rfg/code/gameplay/activities/vrail_data.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gameplay/activities/activity_spawn.h"
#include "volition/rfg/code/gameplay/activities/base_activity_data.h"
#include "volition/rfg/code/gameplay/activity_mission_menu.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/managed_layer_loading.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/restore_list.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/hud/hud_ui_district_liberation.h"
#include "volition/rfg/code/hud/hud_ui_handbook.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud_missions.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_meta_data.h"
#include "volition/rfg/code/objects/object_activity_spawn.h"
#include "volition/rfg/code/objects/object_air_strike_defense_node.h"
#include "volition/rfg/code/objects/object_area_defense_node.h"
#include "volition/rfg/code/objects/object_convoy.h"
#include "volition/rfg/code/objects/object_convoy_end_point.h"
#include "volition/rfg/code/objects/object_courier_end_point.h"
#include "volition/rfg/code/objects/object_delivery_node.h"
#include "volition/rfg/code/objects/object_demolitions_master_node.h"
#include "volition/rfg/code/objects/object_house_arrest_node.h"
#include "volition/rfg/code/objects/object_player_start.h"
#include "volition/rfg/code/objects/object_raid_node.h"
#include "volition/rfg/code/objects/object_restricted_area.h"
#include "volition/rfg/code/objects/object_riding_shotgun_node.h"
#include "volition/rfg/code/objects/object_safehouse.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/scripting/action_script/action_script.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/rfg/code/scripting/objective_highlight.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/rfg/code/scripting/target_outline.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/varray.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/gamewide/game_trace.h"

#include <stdlib.h>

extern mission_handle Mission_active_index; // 0x0165150C (gameplay.cpp)

// ---- public globals
unsigned int Activity_node_start_on_load = 0xFFFFFFFF; // 0x01651244
// 0x01651248
activity_base* All_activities[NUM_OF_ACTIVITY_MODES] = {
    nullptr, // ACTIVITY_MODE_NONE
    &Vehicle_rail_activity_data, // ACTIVITY_MODE_VEHICLE_RAIL
    &Mission_vrail_activity_data, // ACTIVITY_MODE_MISSION_VRAIL
    &Tanks_a_lot_activity, // ACTIVITY_MODE_TANKS_A_LOT
    &Delivery_activity_data, // ACTIVITY_MODE_DELIVERY
    &Killing_spree_data, // ACTIVITY_MODE_KILLING_SPREE
    &Courier_chase_data, // ACTIVITY_MODE_COURIER
    &Mayhem_activity, // ACTIVITY_MODE_MAYHEM
    &Defensive_raid, // ACTIVITY_MODE_RAID_DEFEND
    &Offensive_raid, // ACTIVITY_MODE_RAID_ATTACK
    &House_arrest_activity, // ACTIVITY_MODE_HOUSE_ARREST
    &Demolitions_master_activity, // ACTIVITY_MODE_DEMOLITIONS_MASTER
    &Assassin_activity_data, // ACTIVITY_MODE_ASSASSIN
    &Convoy_activity_data, // ACTIVITY_MODE_CONVOY
    &Continuous_attack_data, // ACTIVITY_MODE_CONTINUOUS_ATTACK
    &Car_bomb_data, // ACTIVITY_MODE_CAR_BOMB
    &Area_defense_activity_data, // ACTIVITY_MODE_AREA_DEFENSE
};
last_activity_spawned_info Last_activity_spawned; // 0x022258A8
timestamp Activity_naturally_spawned_timer; // 0x01651948, initial data ffffffff

// ---- file-static globals
// The activities of which only one may initialize at a time.
static const activity_base* const RESTRICTED_ACTIVITY_LIST[5] = {
    &Mission_vrail_activity_data, &Convoy_activity_data, &Courier_chase_data, &Defensive_raid, &Offensive_raid,
}; // 0x0165128C
static const char* ACTIVITY_NAMES[NUM_OF_ACTIVITY_MODES] = {nullptr, "Vehicle_Rail", "Mission_VRail", "Tanks_a_Lot", "Delivery", "Killing_Spree", "Courier", "Mayhem", "Raid Defend", "Raid Attack", "House_Arrest", "Demolitions_Master", "Assassination", "Convoy", "Cont_Attack", "Car_bomb", "Area_Defense"}; // 0x016512A0
static const char* ACTIVITY_STATE_NAMES[NUM_BASE_ACTIVITY_STATES] = {"Wait", "Load", "Ready", "Starting", "Running", "Shutdown"}; // 0x0165A254

// Game trace (new code, see game_trace.h): the names a trace line shows for a mode and a base state.
static const char* trace_activity_mode_name(int mode)
{
    return mode > ACTIVITY_MODE_NONE && mode < NUM_OF_ACTIVITY_MODES ? ACTIVITY_NAMES[mode] : "None";
}

static const char* trace_activity_state_name(int activity_state)
{
    return activity_state >= 0 && activity_state < NUM_BASE_ACTIVITY_STATES ? ACTIVITY_STATE_NAMES[activity_state] : "Invalid";
}
static int Activity_cooldown_base_period_ms = 180000; // 0x016512E4, 3 minutes
static tweak_table_item Tweak_table_item_Activity_cooldown_base_period_ms("Activity_cooldown_base_period_ms", &Activity_cooldown_base_period_ms); // 0x022487B8
static int Activity_cooldown_per_ready_ms = 60000; // 0x016512E8, 1 minute
static tweak_table_item Tweak_table_item_Activity_cooldown_per_ready_ms("Activity_cooldown_per_ready_ms", &Activity_cooldown_per_ready_ms); // 0x02246AF4
static hud_message_handle Activity_gps_message_handle = (hud_message_handle)0xFFFFFFFF; // 0x016512EC
static timestamp Next_cover_check_time; // 0x0165194C
static timestamp Activity_spawn_period; // 0x01651950
static timestamp Post_mission_cooldown; // 0x01651954
static timestamp Post_activity_cooldown; // 0x01651958
static timestamp Post_leave_safehouse_cooldown; // 0x0165195C
static activity_modes Current_activity_mode; // 0x02229590
static bool Last_cover_blow_result; // 0x02229594
static bool Activity_spawn_disable; // 0x02229595
static bool Activity_pending_autosave; // 0x02229596
static int Last_ready_activities; // 0x02229598

// 0x00755060 (:275). rfg.exe: LTCG, mode in EAX.
static activity_base* activity_get_activity_data(activity_modes mode)
{
    if ((unsigned int)mode < NUM_OF_ACTIVITY_MODES && !Multiplayer)
    {
        return All_activities[mode];
    }
    return nullptr;
}

// 0x00755080 (:287)
activity_base* activity_get_activity_data(int index)
{
    if ((unsigned int)index < NUM_OF_ACTIVITY_MODES && !Multiplayer)
    {
        return All_activities[index];
    }
    return nullptr;
}

// 0x007550B0 (:291)
bool should_render_icons(base_activity_states state)
{
    return state == BASE_ACTIVITY_STATE_READY || state == BASE_ACTIVITY_STATE_STARTING || state == BASE_ACTIVITY_STATE_RUNNING;
}

// 0x007550D0 (:297)
bool should_render_regions(base_activity_states state)
{
    return state == BASE_ACTIVITY_STATE_READY || state == BASE_ACTIVITY_STATE_RUNNING;
}

// 0x007550F0 (:321)
bool is_complete_state(activity_interface_states interface_state)
{
    return interface_state == ACTIVITY_INTERFACE_STATE_VICTORY || interface_state == ACTIVITY_INTERFACE_STATE_LOSS || interface_state == ACTIVITY_INTERFACE_STATE_KILLED;
}

// 0x00755110 (:326). rfg.exe: LTCG, other_activity in EDI.
static bool other_activities_loading(const activity_base* other_activity)
{
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        const activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && activity != other_activity && activity->get_activity_state() == BASE_ACTIVITY_STATE_LOADING)
        {
            return true;
        }
    }
    return false;
}

// 0x00755160 (:344). rfg.exe: LTCG, other_activity in EDI.
static bool other_activities_starting(const activity_base* other_activity)
{
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        const activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && activity != other_activity && activity->get_activity_state() == BASE_ACTIVITY_STATE_STARTING)
        {
            return true;
        }
    }
    return false;
}

// 0x00769320 (:364). rfg.exe: LTCG, objp in ECX.
static const base_activity_data* get_base_activity_data(const object* objp)
{
    if (objp == nullptr)
    {
        return nullptr;
    }
    switch (objp->get_obj_type())
    {
    case OT_RAID_NODE:
        return static_cast<const object_raid_node*>(objp)->get_base_data();
    case OT_HOUSE_ARREST_NODE:
        return static_cast<const object_house_arrest_node*>(objp)->get_base_data();
    case OT_CONVOY_END:
        return static_cast<const object_convoy_end_point*>(objp)->convoy_def->base_data;
    case OT_COURIER_END:
        return static_cast<const object_courier_end_point*>(objp)->courier_def->base_data;
    case OT_DEMOLITIONS_MASTER_NODE:
        return static_cast<const object_demolitions_master_node*>(objp)->base_demolitions_master_data->base_data;
    case OT_RIDING_SHOTGUN_NODE:
        return static_cast<const object_riding_shotgun_node*>(objp)->get_riding_shotgun_info()->base_data;
    case OT_AIR_STRIKE_DEFENSE_NODE:
        return static_cast<const object_air_strike_defense_node*>(objp)->get_definition()->base_data;
    case OT_DELIVERY_NODE:
        return static_cast<const object_delivery_node*>(objp)->get_delivery_def()->base_data;
    case OT_AREA_DEFENSE_NODE:
        return static_cast<const object_area_defense_node*>(objp)->get_base_data();
    default:
        return nullptr;
    }
}

// 0x007551B0 (:425). rfg.exe: LTCG, new_mode in ECX.
static bool activity_init_is_restricted(activity_modes new_mode)
{
    for (int i = 0; i < 5; i++)
    {
        if (RESTRICTED_ACTIVITY_LIST[i]->mode == new_mode)
        {
            return true;
        }
    }
    return false;
}

// 0x007551D0 (:440)
static bool activity_is_allowed_to_initialize(activity_modes new_mode)
{
    if (!activity_init_is_restricted(new_mode))
    {
        return true;
    }
    for (int i = 0; i < 5; i++)
    {
        const activity_base* activity = RESTRICTED_ACTIVITY_LIST[i];
        if (activity->mode != new_mode && (activity->was_initialized() || activity->is_active()))
        {
            return false;
        }
    }
    return true;
}

// 0x007693B0 (:462)
static const object* select_activity_spawn_object(const farray<object const*, 32>& spawn_objects)
{
    if (spawn_objects.size() < 1)
    {
        return nullptr;
    }
    const player* pp = player_get_local_player();
    const object* best = nullptr;
    float best_dist = 3.4028235e+38f;
    for (int i = 0; i < spawn_objects.size(); i++)
    {
        const float dist = vec_dist_squared(pp->pos, spawn_objects[i]->pos);
        if (dist < best_dist)
        {
            best = spawn_objects[i];
            best_dist = dist;
        }
    }
    if (best == nullptr)
    {
        return nullptr;
    }
    // Original: the roll's result goes unused, but it still advances the generator.
    vrand();
    if (!mission_final_mission_complete() && (int)difficulty_get_level() < 2)
    {
        const object* first = best->child_ptr;
        if (first != nullptr)
        {
            const object* child = first;
            while (true)
            {
                if (best->get_obj_type() == child->get_obj_type())
                {
                    return child;
                }
                child = child->child_next;
                if (child == first)
                {
                    break;
                }
                if (child == nullptr)
                {
                    return best;
                }
            }
        }
    }
    else
    {
        const object* parent = World.object_from_handle(best->get_parent());
        if (parent != nullptr && best->get_obj_type() == parent->get_obj_type())
        {
            return parent;
        }
    }
    return best;
}

// 0x0077EE10 (:513). rfg.exe: LTCG, activity in ECX.
static const object* get_local_spawn_object(activity_base* activity, const vector& player_pos, unsigned int district_handle)
{
    farray<object const*, 32> spawn_objects;
    activity->collect_spawn_objects(spawn_objects, player_pos, district_handle);
    if (spawn_objects.size() < 1)
    {
        return nullptr;
    }
    return select_activity_spawn_object(spawn_objects);
}

// Inlined several times in rfg.exe: the local player's district, or the first district without a player.
static district* activity_player_district()
{
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return World.get_object<district>(0);
    }
    return district_get_from_pos(pp->pos);
}

// 0x007A80B0 (:529). rfg.exe: LTCG, district_handle in EAX.
static void get_spawn_set_from_props(unsigned int district_handle, farray<activity_modes, 17>& allowed_activity_set)
{
    activity_spawn_properties* props = get_spawn_props_from_handle(district_handle);
    if (props == nullptr)
    {
        return;
    }
    const district* dp = activity_player_district();
    float control_percent = 0.0f;
    if (dp != nullptr && dp->get_control_max() != 0.0f)
    {
        const float control_max = dp->get_control_max();
        dp = activity_player_district();
        control_percent = (dp == nullptr ? 0.0f : dp->get_control()) / control_max;
    }
    for (int i = 0; i < props->activation_levels.size(); i++)
    {
        const activity_spawn_property& prop = props->activation_levels[i];
        if (prop.activity_flags.completed || control_percent > prop.control_percentage)
        {
            continue;
        }
        bool found = false;
        for (int j = 0; j < allowed_activity_set.size(); j++)
        {
            if (allowed_activity_set[j] == prop.desired_mode)
            {
                found = true;
                break;
            }
        }
        if (found)
        {
            continue;
        }
        activity_base* activity = activity_get_activity_data((int)prop.desired_mode);
        if (activity != nullptr && !activity->is_active() && activity->can_initialize())
        {
            allowed_activity_set.add_unique(activity->mode);
        }
    }
}

// 0x00795B50 (:574)
static void get_spawn_set_from_activity(farray<activity_modes, 17>& allowed_activity_set)
{
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && activity->activity_props.modal && activity->activity_props.can_always_start && !activity->is_active() && activity->can_initialize())
        {
            allowed_activity_set.add_unique(activity->mode);
        }
    }
}

// 0x007A8230 (:606)
static int count_ready_activities(unsigned int district_handle)
{
    farray<activity_modes, 17> ready;
    activity_spawn_properties* props = get_spawn_props_from_handle(district_handle);
    if (props == nullptr)
    {
        return 0;
    }
    const district* dp = activity_player_district();
    const float control = dp == nullptr ? 0.0f : dp->get_control();
    dp = activity_player_district();
    // Original: divides by zero with no district, unlike get_spawn_set_from_props; the clamp below takes the result.
    float control_percent = control / (dp == nullptr ? 0.0f : dp->get_control_max());
    if (control_percent - 0.0f < 0.0f)
    {
        control_percent = 0.0f;
    }
    if (0.0f <= control_percent - 1.0f)
    {
        control_percent = 1.0f;
    }
    for (int i = 0; i < props->activation_levels.size(); i++)
    {
        const activity_spawn_property& prop = props->activation_levels[i];
        if (prop.activity_flags.completed || control_percent > prop.control_percentage)
        {
            continue;
        }
        const activity_base* activity = activity_get_activity_data((int)prop.desired_mode);
        if (activity != nullptr && activity->activity_props.modal && (activity->is_active() || activity_is_allowed_to_initialize(prop.desired_mode)))
        {
            ready.add_unique(prop.desired_mode);
        }
    }
    return ready.size();
}

// 0x00755240 (:650)
static bool any_activity_shutting_down()
{
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        const activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && activity->get_activity_state() == BASE_ACTIVITY_STATE_SHUTDOWN)
        {
            return true;
        }
    }
    return false;
}

// 0x00755280 (:668). rfg.exe: LTCG, activity in EAX.
static bool has_special_start_up(const activity_base* activity)
{
    return activity->mode == ACTIVITY_MODE_RAID_ATTACK || activity->mode == ACTIVITY_MODE_HOUSE_ARREST;
}

// 0x007552A0 (:684)
static bool special_activity_starting()
{
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        const activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && has_special_start_up(activity))
        {
            const int state = (int)activity->get_activity_state();
            if (state >= BASE_ACTIVITY_STATE_LOADING && state <= BASE_ACTIVITY_STATE_STARTING)
            {
                return true;
            }
        }
    }
    return false;
}

// 0x007A83F0 (:712)
static void do_activity_spawn_frame()
{
    farray<activity_modes, 17> allowed_activity_set;
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    if (Mission_active_index != INVALID_MISSION_HANDLE)
    {
        Post_mission_cooldown.set_rand(60000, 90000); // Magic number: one to one and a half minutes
        return;
    }
    if (mission_prevent_activity_spawn())
    {
        return;
    }
    if (safehouse_contains_player())
    {
        Post_leave_safehouse_cooldown.set_rand(30000, 60000); // Magic number: half a minute to a minute
        return;
    }
    if (!Post_mission_cooldown.elapsed() || !Post_activity_cooldown.elapsed() || !Post_leave_safehouse_cooldown.elapsed())
    {
        return;
    }
    if ((int)gsm_get_alert_level() >= ALERT_LEVEL_ORANGE)
    {
        return;
    }
    const vector& player_pos = pp->pos;
    if (restricted_area_in_any_region(player_pos, true))
    {
        return;
    }
    district* dp_local = district_local_player();
    Last_ready_activities = dp_local == nullptr ? 0 : count_ready_activities(dp_local->get_handle());
    // Original: alert level was tested just above, so the test against red here is always true.
    if ((int)gsm_get_alert_level() >= ALERT_LEVEL_RED || Activity_spawn_disable || !activity_none() || special_activity_starting() || any_activity_shutting_down() || other_activities_starting(nullptr) || other_activities_loading(nullptr))
    {
        return;
    }
    allowed_activity_set.clear();
    get_spawn_set_from_activity(allowed_activity_set);
    if (dp_local != nullptr)
    {
        get_spawn_set_from_props(dp_local->get_handle(), allowed_activity_set);
    }
    while (allowed_activity_set.size() > 0)
    {
        const int pick = allowed_activity_set.size() == 1 ? 0 : int_rand_range(0, allowed_activity_set.size() - 1);
        const activity_modes mode = allowed_activity_set[pick];
        allowed_activity_set.remove_unsorted(pick);
        activity_base* activity = activity_get_activity_data(mode);
        if (activity == nullptr || !activity->can_initialize() || activity->is_active())
        {
            continue;
        }
        // Original: dereferences a null dp_local here, when the set came from get_spawn_set_from_activity alone.
        const object* objp = get_local_spawn_object(activity, player_pos, dp_local->get_handle());
        if (objp == nullptr)
        {
            continue;
        }
        object_convoy_end_point* convoy_node = mode == ACTIVITY_MODE_CONVOY ? const_cast<object_convoy_end_point*>(static_cast<const object_convoy_end_point*>(objp)) : nullptr;
        if (convoy_node != nullptr && convoy_node->get_completed_flag() && !convoy_node->last_used_time.elapsed())
        {
            continue;
        }
        const base_activity_data* data = get_base_activity_data(objp);
        if (data != nullptr && data->activity_flags.disabled)
        {
            continue;
        }
        const district* dp = activity_int_get_district_from_node(objp);
        if (dp == nullptr)
        {
            dp = dp_local;
        }
        if (data != nullptr && gfm_get_edf_control_percent(dp) > data->control_limit)
        {
            continue;
        }
        if (activity->start_activity_spawn(objp, dp))
        {
            Activity_naturally_spawned_timer.set(5000); // Magic number: the tool tips wait five seconds
            if (convoy_node != nullptr && convoy_node->get_completed_flag())
            {
                convoy_node->last_used_time.set(600000); // Magic number: a finished convoy returns after ten minutes
            }
            return;
        }
        activity->activity_kill();
    }
}

// ---- activity_base

// 0x00755300 (:919)
void activity_base::system_init()
{
}

// 0x007694A0 (:925). Original: answers the title of the activity table's entry of this mode, not its own name.
const char* activity_base::get_name() const
{
    if ((int)mode > 0)
    {
        const activity_base* activity = activity_get_activity_data((int)mode);
        if (activity != nullptr)
        {
            return activity->get_title();
        }
    }
    return nullptr;
}

// 0x00755310 (:940)
bool activity_base::primary_vehicle_slot_is_free() const
{
    if (base_data != nullptr && base_data->v_spawn_group != nullptr)
    {
        return srm_no_owner(SPAWN_SLOT_VEHICLE_ACTIVITY);
    }
    return true;
}

// 0x00755330 (:954)
bool activity_base::reserve_primary_vehicle_slot()
{
    if (!base_activity_flags.primary_vehicle_reserved && base_data != nullptr && base_data->v_spawn_group != nullptr)
    {
        if (!srm_reserve_slot(SPAWN_SLOT_VEHICLE_ACTIVITY, (resource_slot_user_id)mode, base_data->v_spawn_group))
        {
            return false;
        }
        base_activity_flags.primary_vehicle_reserved = 1;
    }
    return true;
}

// 0x00755370 (:978)
bool activity_base::reserve_secondary_vehicle_slot()
{
    if (!base_activity_flags.secondary_vehicle_reserved && base_data != nullptr && base_data->v_opt_spawn_group != nullptr)
    {
        if (!srm_reserve_slot(SPAWN_SLOT_VEHICLE_MULTI_PURPOSE, (resource_slot_user_id)mode, base_data->v_opt_spawn_group))
        {
            return false;
        }
        base_activity_flags.secondary_vehicle_reserved = 1;
    }
    return true;
}

// 0x007553B0 (:1002)
void activity_base::release_all_vehicle_slots()
{
    if (base_activity_flags.primary_vehicle_reserved)
    {
        srm_release_all_slots(SPAWN_SLOT_VEHICLE_ACTIVITY, (resource_slot_user_id)mode);
    }
    if (base_activity_flags.secondary_vehicle_reserved)
    {
        srm_release_all_slots(SPAWN_SLOT_VEHICLE_MULTI_PURPOSE, (resource_slot_user_id)mode);
    }
    base_activity_flags.primary_vehicle_reserved = 0;
    base_activity_flags.secondary_vehicle_reserved = 0;
}

// 0x007553F0 (:1018)
bool activity_base::should_reserve_primary_vehicle_at_init() const
{
    return base_data != nullptr && base_data->v_spawn_group != nullptr && !base_data->activity_flags.reserve_vehicle_at_start;
}

// 0x00755410 (:1030)
bool activity_base::should_reserve_primary_vehicle_at_start() const
{
    return base_data != nullptr && base_data->v_spawn_group != nullptr && base_data->activity_flags.reserve_vehicle_at_start;
}

// 0x00755430 (:1043)
bool activity_base::should_reserve_secondary_vehicle_at_start() const
{
    return base_data != nullptr && base_data->v_opt_spawn_group != nullptr;
}

// 0x00755450 (:1056)
bool activity_base::should_reserve_human_at_init() const
{
    return base_data != nullptr && base_data->h_spawn_group != nullptr && !base_data->activity_flags.reserve_human_at_start;
}

// 0x00755470 (:1068)
bool activity_base::should_reserve_human_at_start() const
{
    return base_data != nullptr && base_data->h_spawn_group != nullptr && base_data->activity_flags.reserve_human_at_start;
}

// 0x00755490 (:1081)
bool activity_base::reserve_human_slot()
{
    if (base_data != nullptr && base_data->h_spawn_group != nullptr && !base_activity_flags.human_reserved)
    {
        if (!srm_reserve_slot(SPAWN_SLOT_HUMAN_ACTIVITY, (resource_slot_user_id)mode, base_data->h_spawn_group))
        {
            return false;
        }
        base_activity_flags.human_reserved = 1;
    }
    return true;
}

// 0x007554D0 (:1107)
void activity_base::release_human_slots()
{
    if (base_activity_flags.human_reserved)
    {
        srm_release_all_slots(SPAWN_SLOT_HUMAN_ACTIVITY, (resource_slot_user_id)mode);
    }
    base_activity_flags.human_reserved = 0;
}

// 0x00755500 (:1117)
bool activity_base::should_start_music_score() const
{
    return base_data != nullptr && base_data->special_score_name == nullptr && base_data->desired_music_type != GAMEMUSIC_THRESHOLD_INVALID;
}

// 0x00755520 (:1137)
void activity_base::play_activity_music()
{
    if (base_data == nullptr || base_activity_flags.music_started)
    {
        return;
    }
    if (base_data->special_score_name != nullptr)
    {
        gamemusic_start_special_music(base_data->special_score_name);
        base_activity_flags.music_started = 1;
    }
    else if (base_data->desired_music_type != GAMEMUSIC_THRESHOLD_INVALID)
    {
        gamemusic_set_minimum_music_level(base_data->desired_music_type);
        base_activity_flags.music_started = 1;
    }
}

// 0x00755580 (:1160)
void activity_base::stop_activity_music()
{
    if (!base_activity_flags.music_started)
    {
        return;
    }
    if (base_data->special_score_name != nullptr)
    {
        if (mode == ACTIVITY_MODE_DEMOLITIONS_MASTER)
        {
            gamemusic_event_occurred(GAMEMUSIC_ACTIVITY_ENDED);
        }
        gamemusic_restore_original_music();
    }
    if (base_data->desired_music_type != GAMEMUSIC_THRESHOLD_INVALID)
    {
        gamemusic_set_minimum_music_level(GAMEMUSIC_THRESHOLD_SILENCE);
    }
    base_activity_flags.music_started = 0;
}

// 0x007555E0 (:1179)
void activity_base::release_activity_slot()
{
    release_all_vehicle_slots();
    release_human_slots();
}

// 0x007A03A0 (:1191)
bool activity_base::can_initialize() const
{
    if (is_disabled() || (state != BASE_ACTIVITY_STATE_WAIT && state != BASE_ACTIVITY_STATE_LOADING))
    {
        return false;
    }
    if (!resource_slot_free())
    {
        return false;
    }
    return activity_is_allowed_to_initialize(mode);
}

// 0x00755640 (:1221)
bool activity_base::can_start() const
{
    return state == BASE_ACTIVITY_STATE_READY;
}

// 0x00755650 (:1228)
bool activity_base::accept_new_squad(object_squad* squad)
{
    return false;
}

// 0x00755660 (:1234)
void activity_base::change_activity_state(base_activity_states new_state)
{
    if (state == new_state)
    {
        return;
    }
    GAME_TRACE("activity", "%s (mode %d) %s node=0x%08X district=0x%08X: %s -> %s", trace_activity_mode_name(mode), (int)mode, get_instance_name(), spawn_object_handle, district_handle, trace_activity_state_name(state), trace_activity_state_name(new_state));
    state = new_state;
    state_duration.set(0);
    if (spawn_object_handle != 0xFFFFFFFF)
    {
        activity_status_event data;
        data.mode = mode;
        data.node_handle = spawn_object_handle;
        data.activity_state = new_state;
        gem_post_event(data);
    }
}

// 0x007556C0 (:1282)
bool activity_base::should_restore_objects_at_init() const
{
    return base_data != nullptr && !base_data->activity_flags.restore_at_start;
}

// 0x007556E0 (:1294)
bool activity_base::should_restore_objects_at_start() const
{
    return base_data != nullptr && base_data->activity_flags.restore_at_start;
}

// 0x0077EE80 (:1307)
bool activity_base::object_restoration_completed() const
{
    if (base_data == nullptr)
    {
        return true;
    }
    return base_activity_flags.object_restored && spawn_object_handle != 0xFFFFFFFF && restore_list_restore_complete(spawn_object_handle);
}

// 0x0077EEB0 (:1321)
void activity_base::start_object_restoration()
{
    if (spawn_object_handle != 0xFFFFFFFF)
    {
        restore_list_do_restore(spawn_object_handle);
    }
    base_activity_flags.object_restored = 1;
}

// 0x00795BC0 (:1332)
void activity_base::start_managed_layer_unloading()
{
    if (base_data != nullptr && base_data->managed_layers != nullptr)
    {
        base_data->managed_layers->start_layer_unloading();
    }
}

// 0x007A03F0 (:1345)
bool activity_base::start_managed_layer_loading()
{
    if (base_data != nullptr && base_data->managed_layers != nullptr)
    {
        return base_data->managed_layers->start_layer_loading();
    }
    return true;
}

// 0x00755700 (:1360)
bool activity_base::should_create_layer_at_init() const
{
    return base_data != nullptr && base_data->managed_layers != nullptr && !base_data->activity_flags.load_layer_at_start;
}

// 0x00755720 (:1373)
bool activity_base::should_create_layer_at_start() const
{
    return base_data != nullptr && base_data->managed_layers != nullptr && base_data->activity_flags.load_layer_at_start;
}

// 0x00795BE0 (:1389)
bool activity_base::create_managed_layer()
{
    if (base_data == nullptr || base_data->managed_layers == nullptr)
    {
        return true;
    }
    managed_layer_set* layers = base_data->managed_layers;
    if (!layers->layer_resources_loaded())
    {
        return false;
    }
    if (layers->layer_resources_serialized())
    {
        GAME_TRACE("activity", "%s (mode %d) %s: layer already serialized, %d objects kept", trace_activity_mode_name(mode), (int)mode, get_instance_name(), layer_objects.size());
        return true;
    }
    layer_objects.clear();
    if (!layers->serialize_layer_objects())
    {
        GAME_TRACE("activity", "%s (mode %d) %s: layer objects not serialized", trace_activity_mode_name(mode), (int)mode, get_instance_name());
        return false;
    }
    for (int i = 0; i < All_manager_layer_objects.size(); i++)
    {
        const unsigned int handle = All_manager_layer_objects[i];
        if (layers->member_of_layer(handle) && layer_objects.add(handle) < 0)
        {
            GAME_TRACE("activity", "%s (mode %d) %s: layer object list full", trace_activity_mode_name(mode), (int)mode, get_instance_name());
            return false;
        }
    }
    GAME_TRACE("activity", "%s (mode %d) %s: layer created, %d objects of %d managed", trace_activity_mode_name(mode), (int)mode, get_instance_name(), layer_objects.size(), All_manager_layer_objects.size());
    return true;
}

// 0x00795C70 (:1435)
bool activity_base::activity_initialize(const object* spawn_objp, unsigned int new_district_handle)
{
    if (!can_initialize())
    {
        return false;
    }
    base_activity_flags.cleaned_up = 0;
    if (base_data != nullptr && base_data->managed_layers != nullptr && !base_data->managed_layers->layer_resources_loaded())
    {
        return false;
    }
    district_handle = new_district_handle;
    base_data = const_cast<base_activity_data*>(get_base_activity_data(spawn_objp));
    if (should_create_layer_at_init() && !create_managed_layer())
    {
        return false;
    }
    load_managed_script_group();
    change_interface_state(ACTIVITY_INTERFACE_STATE_READY);
    change_activity_state(BASE_ACTIVITY_STATE_READY);
    switch (mode)
    {
    case ACTIVITY_MODE_VEHICLE_RAIL:
        hud_ui_handbook_unlock_entry("Riding Shotgun");
        break;
    case ACTIVITY_MODE_COURIER:
        hud_ui_handbook_unlock_entry("Catch the Courier");
        break;
    case ACTIVITY_MODE_RAID_DEFEND:
        hud_ui_handbook_unlock_entry("Defensive Raid");
        break;
    case ACTIVITY_MODE_RAID_ATTACK:
        hud_ui_handbook_unlock_entry("Offensive Raid");
        break;
    case ACTIVITY_MODE_HOUSE_ARREST:
        hud_ui_handbook_unlock_entry("House Arrest");
        break;
    case ACTIVITY_MODE_CONVOY:
        hud_ui_handbook_unlock_entry("Convoy_handbook");
        break;
    case ACTIVITY_MODE_AREA_DEFENSE:
        hud_ui_handbook_unlock_entry("Area Defense");
        break;
    default:
        break;
    }
    return true;
}

// 0x007A0410 (:1502)
bool activity_base::start_activity_spawn(const object* spawn_objp, const district* dp)
{
    if (spawn_objp == nullptr || spawn_object_handle != 0xFFFFFFFF)
    {
        GAME_TRACE("activity", "%s (mode %d) spawn refused: node=0x%08X busy with node=0x%08X", trace_activity_mode_name(mode), (int)mode, spawn_objp != nullptr ? spawn_objp->get_handle() : 0xFFFFFFFF, spawn_object_handle);
        return false;
    }
    activity_reset();
    if (!resource_slot_free())
    {
        GAME_TRACE("activity", "%s (mode %d) spawn refused: node=0x%08X no free layer slot", trace_activity_mode_name(mode), (int)mode, spawn_objp->get_handle());
        return false;
    }
    if (dp != nullptr && dp->is_liberated() && !mission_final_mission_complete())
    {
        switch (mode)
        {
        case ACTIVITY_MODE_VEHICLE_RAIL:
        case ACTIVITY_MODE_COURIER:
        case ACTIVITY_MODE_RAID_DEFEND:
        case ACTIVITY_MODE_RAID_ATTACK:
        case ACTIVITY_MODE_HOUSE_ARREST:
            GAME_TRACE("activity", "%s (mode %d) spawn refused: node=0x%08X district liberated", trace_activity_mode_name(mode), (int)mode, spawn_objp->get_handle());
            return false;
        default:
            break;
        }
    }
    if (other_activities_loading(this))
    {
        GAME_TRACE("activity", "%s (mode %d) spawn refused: node=0x%08X another activity is loading", trace_activity_mode_name(mode), (int)mode, spawn_objp->get_handle());
        return false;
    }
    spawn_object_handle = spawn_objp->get_handle();
    base_data = const_cast<base_activity_data*>(get_base_activity_data(spawn_objp));
    GAME_TRACE("activity", "%s (mode %d) spawn: node=0x%08X at (%.1f %.1f %.1f) data=%s layers=%s scripts=%d", trace_activity_mode_name(mode), (int)mode, spawn_object_handle, spawn_objp->pos.x, spawn_objp->pos.y, spawn_objp->pos.z, get_instance_name(), base_data == nullptr ? "(no data)" : base_data->managed_layers == nullptr ? "none" : base_data->activity_flags.load_layer_at_start ? "at start" : "at init", base_data != nullptr ? base_data->managed_script_list.size() : 0);
    if (!should_reserve_voice_at_init() || reserve_voice())
    {
        change_activity_state(BASE_ACTIVITY_STATE_LOADING);
        district_handle = dp == nullptr ? 0xFFFFFFFF : dp->get_handle();
        if (start_managed_layer_loading())
        {
            if (should_restore_objects_at_init())
            {
                start_object_restoration();
            }
            return true;
        }
        GAME_TRACE("activity", "%s (mode %d) spawn aborted: %s layer loading refused", trace_activity_mode_name(mode), (int)mode, get_instance_name());
    }
    else
    {
        GAME_TRACE("activity", "%s (mode %d) spawn aborted: %s voice not reserved", trace_activity_mode_name(mode), (int)mode, get_instance_name());
    }
    abort_spawn();
    return false;
}

// 0x00755740 (:1578)
bool activity_base::was_initialized() const
{
    return (int)state > BASE_ACTIVITY_STATE_LOADING && (int)state < BASE_ACTIVITY_STATE_SHUTDOWN;
}

// 0x00755760 (:1584)
void activity_base::system_shutdown()
{
}

// 0x00755770 (:1589)
void activity_base::change_interface_state(activity_interface_states new_interface_state)
{
    if (interface_state != new_interface_state)
    {
        interface_state = new_interface_state;
        time_in_interface_state.set(0);
    }
}

// 0x007694E0 (:1598)
bool activity_base::resources_ready() const
{
    if (base_data != nullptr && base_data->managed_layers != nullptr)
    {
        return base_data->managed_layers->layer_resources_loaded();
    }
    return true;
}

// 0x007557A0 (:1609)
bool activity_base::resources_streaming_complete() const
{
    if (should_reserve_primary_vehicle_at_init() && base_activity_flags.primary_vehicle_reserved && !srm_slots_streamed(SPAWN_SLOT_VEHICLE_ACTIVITY, (resource_slot_user_id)mode))
    {
        return false;
    }
    if (should_reserve_human_at_init() && base_activity_flags.human_reserved && !srm_slots_streamed(SPAWN_SLOT_HUMAN_ACTIVITY, (resource_slot_user_id)mode))
    {
        return false;
    }
    return true;
}

// 0x00795DF0 (:1634)
bool activity_base::resource_slot_free() const
{
    if (base_data != nullptr && base_data->managed_layers != nullptr)
    {
        return base_data->managed_layers->resource_slot_free();
    }
    return true;
}

// 0x0077EED0 (:1645)
bool activity_base::resources_loading() const
{
    if (base_data != nullptr && base_data->managed_layers != nullptr && !base_data->managed_layers->layer_resources_loaded())
    {
        return base_data->managed_layers->layer_loading_started();
    }
    return true;
}

// 0x00755820 (:1656)
void activity_base::activity_reset()
{
    spawn_object_handle = 0xFFFFFFFF;
    state = BASE_ACTIVITY_STATE_WAIT;
    state_duration.set(0);
    interface_state = ACTIVITY_INTERFACE_STATE_NONE;
    time_in_interface_state.set(0);
    base_data = nullptr;
    reset_flags();
}

// 0x00755870 (:1669)
void activity_base::level_init()
{
    activity_disable_flags = 0;
    activity_suspend_flags = 0;
    activity_reset();
}

// 0x00755890 (:1676)
void activity_base::level_shutdown()
{
}

// 0x007558A0 (:1681)
bool activity_base::allow_guerrilla_reinforcements() const
{
    if (base_data != nullptr)
    {
        return base_data->activity_flags.allow_guerrilla_reinforcement;
    }
    return true;
}

// 0x0077EF00 (:1694)
bool activity_base::activity_begin_start_up()
{
    if (is_disabled() || Multiplayer || state != BASE_ACTIVITY_STATE_READY)
    {
        return false;
    }
    if (should_reserve_voice_at_start() && !reserve_voice())
    {
        return false;
    }
    if (should_reserve_primary_vehicle_at_start() && !reserve_primary_vehicle_slot())
    {
        return false;
    }
    if (should_reserve_secondary_vehicle_at_start() && !reserve_secondary_vehicle_slot())
    {
        return false;
    }
    if (should_restore_objects_at_start())
    {
        start_object_restoration();
    }
    change_activity_state(BASE_ACTIVITY_STATE_STARTING);
    gamelog_mark_activity_start_time(mode, activity_props.modal);
    return true;
}

// 0x007AC9A0 (:1743)
bool activity_base::activity_start()
{
    if (is_disabled() || (activity_props.modal && state != BASE_ACTIVITY_STATE_STARTING) || Multiplayer)
    {
        return false;
    }
    if (should_create_layer_at_start() && !create_managed_layer())
    {
        return false;
    }
    if (should_reserve_human_at_start() && !reserve_human_slot())
    {
        return false;
    }
    if (base_data != nullptr)
    {
        if (base_data->activity_flags.ratchet_alert_level)
        {
            alm_set_alert_level_ratcheting(true);
        }
        if (should_start_music_score())
        {
            play_activity_music();
        }
    }
    gamelog_mark_activity_start_time(mode, activity_props.modal);
    if (base_data != nullptr)
    {
        gsm_set_alert_level_cap(base_data->min_alert_cap, base_data->max_alert_cap);
    }
    load_managed_script_group();
    change_activity_state(BASE_ACTIVITY_STATE_RUNNING);
    change_interface_state(ACTIVITY_INTERFACE_STATE_RUNNING);
    hud_message_remove(Activity_gps_message_handle);
    return true;
}

// 0x007558C0 (:1804)
bool activity_base::is_disabled_for_reason(activity_disable_reason disable_reason)
{
    if ((unsigned int)disable_reason > ADR_OVERRIDE_ALL_REASONS)
    {
        return false;
    }
    if (disable_reason == ADR_OVERRIDE_ALL_REASONS)
    {
        return is_disabled();
    }
    return (activity_disable_flags & (1u << disable_reason)) != 0;
}

// 0x00755910 (:1818)
void activity_base::disable(activity_disable_reason disable_reason)
{
    if ((unsigned int)disable_reason < NUM_ACTIVITY_DISABLE_REASONS)
    {
        if (!is_disabled())
        {
            activity_kill();
        }
        activity_disable_flags |= 1u << disable_reason;
    }
}

// 0x00755950 (:1832)
bool activity_base::enable(activity_disable_reason disable_reason)
{
    if ((unsigned int)disable_reason > ADR_OVERRIDE_ALL_REASONS)
    {
        return false;
    }
    if (disable_reason == ADR_OVERRIDE_ALL_REASONS)
    {
        activity_disable_flags = 0;
        return true;
    }
    activity_disable_flags &= ~(1u << disable_reason);
    return activity_disable_flags == 0;
}

// 0x00755990 (:1849)
void activity_base::suspend(activity_disable_reason disable_reason)
{
    if ((unsigned int)disable_reason < NUM_ACTIVITY_DISABLE_REASONS)
    {
        if (!is_disabled())
        {
            activity_kill();
        }
        activity_suspend_flags |= 1u << disable_reason;
    }
}

// 0x007559D0 (:1862)
void activity_base::reinstate(activity_disable_reason disable_reason)
{
    if ((unsigned int)disable_reason < NUM_ACTIVITY_DISABLE_REASONS)
    {
        if (disable_reason == ADR_OVERRIDE_ALL_REASONS)
        {
            activity_suspend_flags = 0;
            return;
        }
        activity_suspend_flags &= ~(1u << disable_reason);
    }
}

// 0x00795E10 (:1877)
void activity_base::activity_end_common()
{
    if (state == BASE_ACTIVITY_STATE_WAIT)
    {
        return;
    }
    if (activity_props.modal)
    {
        alm_set_alert_level_ratcheting(false);
    }
    stop_activity_music();
    release_activity_slot();
    if (Mission_active_index == INVALID_MISSION_HANDLE && mode != ACTIVITY_MODE_KILLING_SPREE && mode != ACTIVITY_MODE_MAYHEM)
    {
        Cartographer.remove_player_marker(true);
    }
    const base_activity_states old_state = state;
    if (old_state == BASE_ACTIVITY_STATE_RUNNING && base_data != nullptr)
    {
        gsm_release_alert_level_cap(); // rfg.exe inlines it
    }
    layer_objects.clear();
    start_managed_layer_unloading();
    if (!is_complete_state(interface_state))
    {
        change_interface_state(ACTIVITY_INTERFACE_STATE_KILLED);
    }
    unload_managed_script_group();
    change_activity_state(BASE_ACTIVITY_STATE_SHUTDOWN);

    int salvage = 0;
    player_meta_data* meta = player_get_meta_data();
    if (meta != nullptr)
    {
        salvage = meta->get_salvage();
    }
    if (old_state != BASE_ACTIVITY_STATE_RUNNING)
    {
        return;
    }
    char activity_exit_status = 1; // Magic number: the reason a victory logs
    player* pp = player_get_local_player();
    if (!base_activity_flags.victory_recorded && activity_props.modal)
    {
        if (pp != nullptr && pp->is_killed())
        {
            activity_exit_status = 2; // Magic number: the reason a player death logs
        }
        else
        {
            activity_exit_status = (char)gamelog_get_activity_failed_reason();
        }
    }
    // Magic number: interface state 3 of demolitions master is its success.
    if (mode == ACTIVITY_MODE_DEMOLITIONS_MASTER && static_cast<activity_demolitions_master*>(this)->get_alt_interface_state() == 3)
    {
        activity_exit_status = 1;
    }
    const vector player_pos = gamelog_get_human_pos(pp);
    const int duration = gamelog_get_activity_duration(mode);
    const char difficulty = gamelog_get_difficulty();
    const int locale_id = gamelog_get_locale();
    const char pct_game_complete = gamelog_get_pct_complete();
    const int time_played = gamelog_get_time_played();
    const int activity_crc = gamelog_get_activity_crc(this);
    const int time = gamelog_get_logging_time();
    const int player_checksum = gamelog_get_player_checksum();
    log_event_activity_attempt(player_checksum, time, activity_crc, time_played, pct_game_complete, activity_exit_status, salvage, locale_id, difficulty, duration, player_pos);
}

// 0x00796020 (:2057)
void activity_base::activity_kill()
{
    activity_end_common();
}

// 0x00796030 (:2062)
void activity_base::activity_end()
{
    activity_end_common();
}

// 0x00755A00 (:2068)
bool activity_base::activity_clean_up_completed() const
{
    return base_activity_flags.cleaned_up;
}

// 0x0077EFE0 (:2075)
bool activity_base::activity_starting_completed() const
{
    if (should_reserve_voice_at_start() && !voice_loaded())
    {
        return false;
    }
    if (should_restore_objects_at_start() && !object_restoration_completed())
    {
        return false;
    }
    if (base_activity_flags.primary_vehicle_reserved && !srm_slots_streamed(SPAWN_SLOT_VEHICLE_ACTIVITY, (resource_slot_user_id)mode))
    {
        return false;
    }
    if (base_activity_flags.secondary_vehicle_reserved && !srm_slots_streamed(SPAWN_SLOT_VEHICLE_MULTI_PURPOSE, (resource_slot_user_id)mode))
    {
        return false;
    }
    if (base_activity_flags.human_reserved && !srm_slots_streamed(SPAWN_SLOT_HUMAN_ACTIVITY, (resource_slot_user_id)mode))
    {
        return false;
    }
    return true;
}

// 0x0077F0D0 (:2118)
void activity_base::activity_complete()
{
    if (!activity_clean_up_completed())
    {
        return;
    }
    release_voice();
    change_activity_state(BASE_ACTIVITY_STATE_WAIT);
    base_data = nullptr;
    spawn_object_handle = 0xFFFFFFFF;
    reset_flags();
    activity_reset();
}

// 0x00769500 (:2137)
void activity_base::activity_do_frame(bool game_paused)
{
    if (game_paused)
    {
        activity_do_frame_paused();
        return;
    }
    if (is_disabled())
    {
        if (state == BASE_ACTIVITY_STATE_READY || state == BASE_ACTIVITY_STATE_RUNNING)
        {
            activity_kill();
        }
        if (state != BASE_ACTIVITY_STATE_SHUTDOWN)
        {
            return;
        }
    }
    switch (state)
    {
    case BASE_ACTIVITY_STATE_WAIT:
        activity_do_frame_inactive();
        if (time_in_interface_state.time_since() > 10000) // Magic number: the end state shows for ten seconds
        {
            change_interface_state(ACTIVITY_INTERFACE_STATE_NONE);
        }
        break;
    case BASE_ACTIVITY_STATE_LOADING:
    {
        const activity_load_state load_state = activity_do_frame_loading();
        if (load_state == ACTIVITY_LOAD_STATE_COMPLETE)
        {
            if (!activity_initialize(World.object_from_handle(spawn_object_handle), district_handle))
            {
                GAME_TRACE("activity", "%s (mode %d) %s node=0x%08X: initialize failed, killed", trace_activity_mode_name(mode), (int)mode, get_instance_name(), spawn_object_handle);
                activity_kill();
            }
        }
        else if (load_state == ACTIVITY_LOAD_STATE_FAILED)
        {
            GAME_TRACE("activity", "%s (mode %d) %s node=0x%08X: load failed after %d ms, killed", trace_activity_mode_name(mode), (int)mode, get_instance_name(), spawn_object_handle, state_duration.time_since());
            activity_kill();
        }
        break;
    }
    case BASE_ACTIVITY_STATE_READY:
        activity_do_frame_ready();
        break;
    case BASE_ACTIVITY_STATE_STARTING:
        activity_do_frame_starting();
        if (activity_starting_completed() && !activity_start())
        {
            GAME_TRACE("activity", "%s (mode %d) %s node=0x%08X: start failed, killed", trace_activity_mode_name(mode), (int)mode, get_instance_name(), spawn_object_handle);
            activity_kill();
        }
        break;
    case BASE_ACTIVITY_STATE_RUNNING:
        activity_do_frame_active();
        break;
    case BASE_ACTIVITY_STATE_SHUTDOWN:
        activity_do_frame_clean_up();
        if (activity_clean_up_completed())
        {
            activity_complete();
        }
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        activity_kill();
        break;
    }
    if (should_render_icons(state))
    {
        render_icons();
    }
    if (should_render_regions(state))
    {
        render_regions();
    }
}

// 0x0077F1A0 (:2247)
void activity_base::load_managed_script_group()
{
    if (base_data == nullptr || base_data->managed_script_list.size() < 1 || base_data->activity_flags.scripts_loaded)
    {
        return;
    }
    if (base_data->managed_layers != nullptr && !base_data->managed_layers->layer_resources_serialized())
    {
        return;
    }
    for (int i = 0; i < base_data->managed_script_list.size(); i++)
    {
        script_activate_managed_group(base_data->managed_script_list[i], true);
    }
    base_data->activity_flags.scripts_loaded = 1;
}

// 0x007696A0 (:2277)
void activity_base::unload_managed_script_group()
{
    if (base_data == nullptr || base_data->managed_script_list.size() < 1)
    {
        return;
    }
    for (int i = 0; i < base_data->managed_script_list.size(); i++)
    {
        script_terminate_managed_group(base_data->managed_script_list[i], false);
    }
    base_data->activity_flags.scripts_loaded = 0;
}

// 0x00796040 (:2292)
void activity_base::abort_spawn()
{
    release_voice();
    layer_objects.clear();
    start_managed_layer_unloading();
    spawn_object_handle = 0xFFFFFFFF;
    district_handle = 0xFFFFFFFF;
    base_data = nullptr;
}

// 0x00755A10 (:2308)
const char* activity_base::get_instance_name() const
{
    if (base_data != nullptr && base_data->name != nullptr)
    {
        return base_data->name;
    }
    return "unknown";
}

// 0x007696E0 (:2325)
bool activity_base::reserve_voice()
{
    if (base_data == nullptr)
    {
        return true;
    }
    if (spawn_object_handle == 0xFFFFFFFF)
    {
        return false;
    }
    if (!base_data->reserve_voice(spawn_object_handle))
    {
        return false;
    }
    base_activity_flags.voice_reserved = 1;
    return true;
}

// 0x00769720 (:2341)
bool activity_base::voice_loaded() const
{
    if (base_data == nullptr)
    {
        return true;
    }
    return spawn_object_handle != 0xFFFFFFFF && base_data->voice_loaded(spawn_object_handle);
}

// 0x00769760 (:2352)
bool activity_base::release_voice()
{
    if (base_data == nullptr || !base_activity_flags.voice_reserved)
    {
        return true;
    }
    base_activity_flags.voice_reserved = 0;
    return spawn_object_handle != 0xFFFFFFFF && base_data->release_voice(spawn_object_handle);
}

// 0x007697A0 (:2369)
bool activity_base::can_reserve_voice() const
{
    if (base_data == nullptr)
    {
        return true;
    }
    return spawn_object_handle != 0xFFFFFFFF && base_data->can_reserve_voice(spawn_object_handle);
}

// 0x00755A30 (:2380)
bool activity_base::should_reserve_voice_at_init() const
{
    return base_data != nullptr && base_data->desired_voice != nullptr && !base_data->activity_flags.persona_at_start;
}

// 0x00755A50 (:2393)
bool activity_base::should_reserve_voice_at_start() const
{
    return base_data != nullptr && base_data->desired_voice != nullptr && base_data->activity_flags.persona_at_start;
}

// 0x00755A70 (:2409)
void activity_base::activity_do_frame_inactive()
{
}

// 0x0077F210 (:2416)
activity_load_state activity_base::activity_do_frame_loading()
{
    if (state_duration.time_since() > 20000) // Magic number: a load has twenty seconds
    {
        return ACTIVITY_LOAD_STATE_FAILED;
    }
    if (other_activities_loading(this) || !can_initialize() || !resources_loading())
    {
        return ACTIVITY_LOAD_STATE_FAILED;
    }
    if (should_reserve_voice_at_init() && !voice_loaded())
    {
        return ACTIVITY_LOAD_STATE_LOADING;
    }
    if (should_restore_objects_at_init() && !object_restoration_completed())
    {
        return ACTIVITY_LOAD_STATE_LOADING;
    }
    if (!resources_ready())
    {
        return ACTIVITY_LOAD_STATE_LOADING;
    }
    if (should_reserve_primary_vehicle_at_init() && !reserve_primary_vehicle_slot())
    {
        return ACTIVITY_LOAD_STATE_LOADING;
    }
    if (should_reserve_human_at_init() && !reserve_human_slot())
    {
        return ACTIVITY_LOAD_STATE_LOADING;
    }
    return resources_streaming_complete() ? ACTIVITY_LOAD_STATE_COMPLETE : ACTIVITY_LOAD_STATE_LOADING;
}

// 0x007697D0 (:2489)
void activity_base::activity_do_frame_ready()
{
    if (!Activity_naturally_spawned_timer.valid() || !Activity_naturally_spawned_timer.elapsed())
    {
        return;
    }
    if (!ui_tool_tips_tip_has_been_played("New_Activity", false))
    {
        ui_tool_tips_add_tip("New_Activity", 0, nullptr);
    }
    else if (!ui_tool_tips_tip_has_been_played("Guerrilla_Action_GPS", false))
    {
        ui_tool_tips_add_tip("Guerrilla_Action_GPS", 0, nullptr);
    }
    else
    {
        const char* icon = get_icon_image_name();
        if (icon != nullptr)
        {
            const wchar_t* format = localization_get_display_string_req("HUD_GPS_HINT", nullptr);
            if (format != nullptr)
            {
                wchar_t wide_icon[128];
                wchar_t text[256];
                copy_to_wide(wide_icon, icon, 128);
                _snprintf_safe(text, 256, format, wide_icon);
                Activity_gps_message_handle = hud_message_add(text, 4.0f, nullptr, nullptr, nullptr); // Magic number: four seconds
            }
        }
        if (mission_final_mission_complete() && !ui_tool_tips_tip_has_been_played("Guerrilla_Action_Final", false))
        {
            ui_tool_tips_add_tip("Guerrilla_Action_Final", 0, nullptr);
        }
    }
    Activity_naturally_spawned_timer.invalidate();
}

// 0x00755A80 (:2532)
void activity_base::activity_do_frame_starting()
{
}

// 0x00755A90 (:2539)
void activity_base::activity_do_frame_active()
{
}

// 0x00755AA0 (:2547)
void activity_base::activity_clean_up_once()
{
    base_activity_flags.cleaned_up = 1;
}

// 0x00755AB0 (:2558)
void activity_base::activity_do_frame_clean_up()
{
    if (!base_activity_flags.cleaned_up)
    {
        activity_clean_up_once();
    }
}

// 0x00755AD0 (:2572)
void activity_base::activity_do_frame_paused()
{
}

// 0x00755AE0 (:2578)
bool activity_base::render_hud(int x_pos, int y_pos)
{
    return false;
}

// 0x00755AF0 (:2584)
void activity_base::report_recruitment(npc* np)
{
}

// 0x00755B00 (:2590)
void activity_base::report_patrol_reached(unsigned int squad_handle, const patrol_node_handle* patrol_node)
{
}

// 0x00755B10 (:2596)
void activity_base::report_action_node_start(unsigned int action_node_handle)
{
}

// 0x00755B20 (:2602)
void activity_base::report_action_node_end(unsigned int action_node_handle)
{
}

// 0x00755B30 (:2610)
bool activity_base::is_player_cover_blown() const
{
    return false;
}

// 0x00755B40 (:2616)
bool activity_base::lock_alert_level() const
{
    return false;
}

// 0x00755B50 (:2622)
bool activity_base::on_alert(const npc* np) const
{
    return false;
}

// 0x00755B60 (:2628)
bool activity_base::outside_defensive_perimeter(const npc* np, const vector& pos, bool& result, const float* buffer_dist) const
{
    return false;
}

// 0x00769920 (:2636)
bool activity_base::collect_spawn_objects(farray<object const*, 32>& spawn_objects, const vector& player_pos, unsigned int district_handlep) const
{
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    spawn_objects.clear();
    return false;
}

// 0x00755B70 (:2644)
void activity_base::record_victory()
{
    base_activity_flags.victory_recorded = 1;
    if (mode != ACTIVITY_MODE_DEMOLITIONS_MASTER)
    {
        audiolib_cue_play(Ga_cue_ids.activity_complete_sound, nullptr, nullptr);
    }
    change_interface_state(ACTIVITY_INTERFACE_STATE_VICTORY);
    kiosk_unlock_from_activity(base_data);
}

// 0x00755BD0 (:2660)
void activity_base::record_loss()
{
    if (mode != ACTIVITY_MODE_DEMOLITIONS_MASTER)
    {
        audiolib_cue_play(Ga_cue_ids.activity_fail_sound, nullptr, nullptr);
    }
    change_interface_state(ACTIVITY_INTERFACE_STATE_LOSS);
}

// 0x00755C10 (:2671)
void activity_base::save_activity_state(cfile* save_file)
{
    cf_write_uint32(activity_disable_flags, save_file);
    cf_write_uint32(activity_suspend_flags, save_file);
}

// 0x00755C40 (:2678)
void activity_base::load_activity_state(cfile* save_file)
{
    activity_disable_flags = cf_read_uint32(save_file);
    activity_suspend_flags = cf_read_uint32(save_file);
}

// 0x0077F2F0 (:2685)
void activity_base::collect_layer_objects(base_array<object*>& obj_list)
{
    obj_list.clear();
    for (int i = 0; i < layer_objects.size(); i++)
    {
        object* objp = World.object_from_handle(layer_objects[i]);
        if (objp != nullptr)
        {
            if (obj_list.size() < obj_list.capacity())
            {
                obj_list.add(objp);
            }
            else
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            }
        }
    }
}

// 0x0077F360 (:2700)
void activity_base::collect_layer_objects(base_array<object const*>& obj_list)
{
    obj_list.clear();
    for (int i = 0; i < layer_objects.size(); i++)
    {
        const object* objp = World.object_from_handle(layer_objects[i]);
        if (objp != nullptr)
        {
            if (obj_list.size() < obj_list.capacity())
            {
                obj_list.add(objp);
            }
            else
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            }
        }
    }
}

// ---- activity_base_modal

// 0x00755C70 (:2719)
bool activity_base_modal::is_active() const
{
    return mode == Current_activity_mode;
}

// 0x0077F3D0 (:2733)
void activity_base_modal::record_victory()
{
    if (get_victor_record_flag())
    {
        return;
    }
    activity_base::record_victory();
    stats_count_completed_activities();
    Activity_pending_autosave = true;
    if (activity_props.can_always_start)
    {
        return;
    }
    activity_spawn_properties* props = get_spawn_props_from_handle(get_district_handle());
    if (props == nullptr)
    {
        return;
    }
    // The first uncompleted level of this mode, or a later one with a higher control level.
    int best = -1;
    for (int i = 0; i < props->activation_levels.size(); i++)
    {
        const activity_spawn_property& prop = props->activation_levels[i];
        if (prop.desired_mode != mode || prop.activity_flags.completed)
        {
            continue;
        }
        if (best < 0 || props->activation_levels[best].control_percentage < prop.control_percentage)
        {
            best = i;
        }
    }
    if (best >= 0)
    {
        props->activation_levels[best].activity_flags.completed = 1;
    }
}

// 0x00755C80 (:2794)
void activity_base_modal::system_init()
{
}

// 0x007A0530 (:2801)
bool activity_base_modal::can_initialize() const
{
    const base_activity_states current = get_activity_state();
    if (is_disabled() || (current != BASE_ACTIVITY_STATE_WAIT && current != BASE_ACTIVITY_STATE_LOADING))
    {
        return false;
    }
    if (!activity_base::can_initialize())
    {
        return false;
    }
    if (!activity_cooldown_period.elapsed())
    {
        return false;
    }
    // Original: only the delivery may spawn at a red alert.
    if (gsm_get_alert_level() == ALERT_LEVEL_RED && mode != ACTIVITY_MODE_DELIVERY)
    {
        return false;
    }
    // Original: asks a second time, with the same answer.
    return activity_is_allowed_to_initialize(mode);
}

// 0x0077F4D0 (:2821)
bool activity_base_modal::activity_begin_start_up()
{
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        if (i == (int)mode)
        {
            continue;
        }
        activity_base* other = activity_get_activity_data(i);
        if (other != nullptr && other->activity_props.modal && other->get_activity_state() != BASE_ACTIVITY_STATE_WAIT && other->get_activity_state() != BASE_ACTIVITY_STATE_SHUTDOWN)
        {
            other->activity_kill();
        }
    }
    return activity_base::activity_begin_start_up();
}

// 0x007960C0 (:2848)
bool activity_base_modal::activity_initialize(const object* spawn_objp, unsigned int new_district_handle)
{
    if (!activity_is_allowed_to_initialize(mode) || !activity_base::activity_initialize(spawn_objp, new_district_handle))
    {
        return false;
    }
    Last_activity_spawned.mode = mode;
    shutdown_period.invalidate();
    Last_activity_spawned.spawn_node_handle = spawn_objp->get_handle();
    return true;
}

// 0x00755C90 (:2869)
bool activity_base_modal::was_initialized() const
{
    const int current = (int)get_activity_state();
    return current > BASE_ACTIVITY_STATE_LOADING && current < BASE_ACTIVITY_STATE_SHUTDOWN;
}

// 0x00755CB0 (:2880)
void activity_base_modal::level_init()
{
    activity_base::level_init();
    activity_cooldown_period.set(0);
    if (mode == Current_activity_mode)
    {
        Current_activity_mode = ACTIVITY_MODE_NONE;
    }
}

// 0x007ACAC0 (:2894)
bool activity_base_modal::activity_start()
{
    if (is_active() || Current_activity_mode != ACTIVITY_MODE_NONE)
    {
        return false;
    }
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        if (i == (int)mode)
        {
            continue;
        }
        activity_base* other = activity_get_activity_data(i);
        if (other != nullptr && other->activity_props.modal && other->get_activity_state() != BASE_ACTIVITY_STATE_WAIT && other->get_activity_state() != BASE_ACTIVITY_STATE_SHUTDOWN)
        {
            other->activity_kill();
        }
    }
    if (!activity_base::activity_start())
    {
        return false;
    }
    Current_activity_mode = mode;
    return true;
}

// 0x00796120 (:2931)
void activity_base_modal::activity_end()
{
    activity_base::activity_end();
    activity_end_common();
}

// 0x00796130 (:2940)
void activity_base_modal::activity_kill()
{
    activity_base::activity_kill();
    activity_end_common();
}

// 0x00769950 (:2949)
void activity_base_modal::activity_end_common()
{
    if (mode == Current_activity_mode)
    {
        Current_activity_mode = ACTIVITY_MODE_NONE;
    }
    Post_activity_cooldown.set_rand(90000, 120000); // Magic number: one and a half to two minutes
    int cooldown = Activity_cooldown_per_ready_ms * Last_ready_activities + Activity_cooldown_base_period_ms;
    if (cooldown < 0)
    {
        cooldown = 0;
    }
    else if (cooldown > 600000) // Magic number: at most ten minutes
    {
        cooldown = 600000;
    }
    activity_cooldown_period.set(cooldown);
    objective_highlight_remove_all();
    target_outline_remove_all();
}

// 0x00755D00 (:2973)
void activity_base_modal::activity_clean_up_once()
{
    activity_base::activity_clean_up_once();
    shutdown_period.set(1000); // Magic number: one second
}

// 0x00755D20 (:2984)
void activity_base_modal::activity_shutdown_period_override(unsigned int override_ms)
{
    shutdown_period.set(override_ms);
}

// 0x0077F530 (:2993)
void activity_base_modal::activity_complete()
{
    activity_base::activity_complete();
    shutdown_period.invalidate();
}

// 0x00755D30 (:3003)
bool activity_base_modal::activity_clean_up_completed() const
{
    return activity_base::activity_clean_up_completed() && shutdown_period.valid() && shutdown_period.elapsed();
}

// 0x00755D60 (:3017)
void activity_base_modal::activity_do_frame_active()
{
}

// 0x00755D70 (:3023)
void activity_base_modal::activity_reset()
{
    activity_base::activity_reset();
    shutdown_period.invalidate();
    activity_cooldown_period.set(0);
}

// ---- activity_base_free

// 0x00755DE0 (:3040)
bool activity_base_free::is_active() const
{
    return active;
}

// 0x00755DF0 (:3054). Original: a free activity is not expected to win.
void activity_base_free::record_victory()
{
    if (!get_victor_record_flag())
    {
        activity_base::record_victory();
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
}

// 0x00755E20 (:3068)
void activity_base_free::system_init()
{
}

// 0x00755E30 (:3074)
void activity_base_free::activity_reset()
{
    activity_base::activity_reset();
    active = false;
}

// 0x00755E90 (:3082)
void activity_base_free::level_init()
{
    activity_base::level_init();
    active = false;
}

// 0x007ACB30 (:3088)
bool activity_base_free::activity_start()
{
    if (!was_initialized())
    {
        if (!can_initialize())
        {
            return false;
        }
        const district* dp = activity_player_district();
        if (!activity_initialize(nullptr, dp == nullptr ? 0xFFFFFFFF : dp->get_handle()))
        {
            return false;
        }
    }
    if (!activity_base::activity_start())
    {
        return false;
    }
    active = true;
    return true;
}

// 0x00796140 (:3111)
void activity_base_free::activity_end()
{
    activity_base::activity_end();
    active = false;
}

// 0x00796160 (:3118)
void activity_base_free::activity_kill()
{
    activity_base::activity_kill();
    active = false;
}

// ---- the system

// 0x00755EC0 (:3134)
bool activity_draw_hud(int x_pos, int y_pos)
{
    bool drew = false;
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && activity->is_active() && activity->render_hud(x_pos, y_pos))
        {
            drew = true;
        }
    }
    return drew;
}

// 0x00755F30 (:3159)
bool activity_pursuit_npc(const npc* np)
{
    return np->npc_flags.activity_pursuit_vr || np->npc_flags.activity_pursuit_ca;
}

// 0x00755F50 (:3165)
bool activity_is_player_cover_blown()
{
    if (!Next_cover_check_time.elapsed())
    {
        return Last_cover_blow_result;
    }
    Next_cover_check_time.set(1000); // Magic number: asks once a second
    Last_cover_blow_result = false;
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        const activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && !activity->is_disabled() && activity->is_player_cover_blown())
        {
            Last_cover_blow_result = true;
            break;
        }
    }
    return Last_cover_blow_result;
}

// 0x007699C0 (:3191). The raid and the house arrest may still be getting ready: they count as running.
bool activity_none()
{
    if (Current_activity_mode != ACTIVITY_MODE_NONE)
    {
        return false;
    }
    const activity_base* const getting_ready[2] = {&Offensive_raid, &House_arrest_activity};
    for (int i = 0; i < 2; i++)
    {
        const base_activity_states state = getting_ready[i]->get_activity_state();
        if (state == BASE_ACTIVITY_STATE_LOADING || state == BASE_ACTIVITY_STATE_READY || state == BASE_ACTIVITY_STATE_STARTING)
        {
            return false;
        }
    }
    return true;
}

// 0x00755FD0 (:3212)
bool activity_running_or_shutting_down(activity_modes mode)
{
    const activity_base* activity = activity_get_activity_data(mode);
    return activity != nullptr && (activity->get_activity_state() == BASE_ACTIVITY_STATE_RUNNING || activity->get_activity_state() == BASE_ACTIVITY_STATE_SHUTDOWN);
}

// 0x00756010 (:3225)
bool activity_is_active(activity_modes mode)
{
    const activity_base* activity = activity_get_activity_data(mode);
    return activity != nullptr && activity->is_active();
}

// 0x00756040 (:3237)
unsigned int activity_get_active_node_handle()
{
    const activity_base* activity = activity_get_activity_data(Current_activity_mode);
    if (activity != nullptr && activity->is_active())
    {
        return activity->get_spawn_object_handle();
    }
    return 0xFFFFFFFF;
}

// 0x00756080 (:3255)
bool activity_is_ready(activity_modes mode)
{
    const activity_base* activity = activity_get_activity_data(mode);
    return activity != nullptr && activity->was_initialized();
}

// 0x007560B0 (:3267)
bool activity_is_loading(activity_modes mode)
{
    const activity_base* activity = activity_get_activity_data(mode);
    return activity != nullptr && activity->get_activity_state() == BASE_ACTIVITY_STATE_LOADING;
}

// 0x007560F0 (:3279)
const char* activity_subtitle(activity_modes mode)
{
    const activity_base* activity = activity_get_activity_data(mode);
    return activity == nullptr ? nullptr : activity->get_subtitle();
}

// 0x00756120 (:3291)
const char* activity_title(activity_modes mode)
{
    const activity_base* activity = activity_get_activity_data(mode);
    return activity == nullptr ? nullptr : activity->get_title();
}

// 0x00756150 (:3304)
activity_interface_states activity_get_interface_mode(activity_modes mode)
{
    const activity_base* activity = activity_get_activity_data(mode);
    return activity == nullptr ? ACTIVITY_INTERFACE_STATE_INVALID : activity->get_interface_state();
}

// 0x00756180 (:3316)
int activity_interface_mode_duration(activity_modes mode)
{
    const activity_base* activity = activity_get_activity_data(mode);
    return activity == nullptr ? -1 : activity->get_interface_state_duration();
}

// 0x007561C0 (:3328)
const char* activity_get_name(activity_modes mode)
{
    if ((int)mode > 0)
    {
        const activity_base* activity = activity_get_activity_data(mode);
        if (activity != nullptr)
        {
            return activity->get_title();
        }
    }
    return nullptr;
}

// 0x00756200 (:3358)
bool activity_get_ui_text(char* value, int& time_til_end, int buffer_size, activity_modes mode)
{
    if (value == nullptr)
    {
        return false;
    }
    const activity_base* activity = activity_get_activity_data(mode);
    return activity != nullptr && activity->get_ui_display_info(value, time_til_end, buffer_size);
}

// 0x00756250 (:3373)
activity_modes activity_get_mode_from_name(const char* activity_name)
{
    if (activity_name == nullptr)
    {
        return ACTIVITY_MODE_INVALID;
    }
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        if (ACTIVITY_NAMES[i] != nullptr && v_stricmp(activity_name, ACTIVITY_NAMES[i]) == 0)
        {
            return (activity_modes)i;
        }
    }
    return ACTIVITY_MODE_INVALID;
}

// 0x007562A0 (:3392)
activity_modes activity_get_mode()
{
    return Current_activity_mode;
}

// 0x007562B0 (:3398)
const char* activity_get_current_instance_name()
{
    if ((int)Current_activity_mode > 0)
    {
        const activity_base* activity = activity_get_activity_data(Current_activity_mode);
        if (activity != nullptr)
        {
            return activity->get_instance_name(); // rfg.exe inlines it
        }
    }
    return nullptr;
}

// 0x00756300 (:3411)
static bool get_activity_lock_status()
{
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        const activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && !activity->is_disabled() && activity->is_active() && activity->lock_alert_level())
        {
            return true;
        }
    }
    return false;
}

// 0x00756370 (:3429)
bool activity_lock_alert_level()
{
    static timestamp lock_timeout; // 0x02273D44
    static bool last_lock_status; // 0x02273D40
    if (lock_timeout.valid() && !lock_timeout.elapsed())
    {
        return last_lock_status;
    }
    last_lock_status = get_activity_lock_status();
    lock_timeout.set(1000); // Magic number: asks once a second
    return last_lock_status;
}

// 0x007563D0 (:3445)
bool activity_on_alert(const npc* np)
{
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        const activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && !activity->is_disabled() && activity->is_active() && activity->on_alert(np))
        {
            return true;
        }
    }
    return false;
}

// 0x007ACBB0 (:3468)
void activity_system_do_frame(bool game_paused)
{
    if (Multiplayer)
    {
        return;
    }
    if (!game_paused)
    {
        if (Activity_node_start_on_load != 0xFFFFFFFF && !World.sl_flags.pending_load_game_from_memory)
        {
            object* activity_node = World.object_from_handle(Activity_node_start_on_load);
            if (activity_node != nullptr)
            {
                activity_force_start_generic(activity_node, false);
            }
            Activity_node_start_on_load = 0xFFFFFFFF;
        }
        // Original: an orange or red alert ends the activity that waits for the player.
        if ((int)gsm_get_alert_level() >= ALERT_LEVEL_ORANGE)
        {
            activity_end_ready_activity(AERR_ALERT_LEVEL_TOO_HIGH);
        }
        if (Activity_spawn_period.elapsed())
        {
            do_activity_spawn_frame();
            Activity_spawn_period.set_rand(4000, 6000); // Magic number: four to six seconds
        }
    }
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr)
        {
            activity->activity_do_frame(game_paused);
        }
    }
    if (!game_paused)
    {
        activity_register_damage_do_frame(); // rfg.exe inlines it
    }
    if (Activity_pending_autosave && !hud_ui_missions_get_mission_screen_active() && gameseq_get_state() == GS_GAMEPLAY && !hud_ui_district_liberation_screen_active() && !upgrade_unlock_from_activities() && dialogue_manager_get_num_dialogues(DIALOGUE_PRIORITY_UNDEFINED) < 1)
    {
        World.save_and_continue(nullptr, false, true);
        Activity_pending_autosave = false;
    }
}

// 0x00756450 (:3543)
bool activity_is_modal(activity_modes mode)
{
    const activity_base* activity = activity_get_activity_data(mode);
    return activity != nullptr && activity->activity_props.modal;
}

// 0x00756480 (:3556)
event_result activity_report_recruitment(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_RECRUITMENT)
    {
        return EVENT_IGNORED;
    }
    const recruitment_event& data = static_cast<const recruitment_event&>(in_event);
    if (data.handle == 0xFFFFFFFF)
    {
        return EVENT_IGNORED;
    }
    npc* np = static_cast<npc*>(World.object_from_handle(data.handle));
    if (np == nullptr)
    {
        return EVENT_IGNORED;
    }
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && !activity->is_disabled())
        {
            activity->report_recruitment(np);
        }
    }
    return EVENT_OK;
}

// 0x007564F0 (:3578)
event_result activity_report_patrol_reached(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_PATROL_SQUAD_REACHED_NODE)
    {
        return EVENT_IGNORED;
    }
    const patrol_squad_reached_node_event& data = static_cast<const patrol_squad_reached_node_event&>(in_event);
    if (data.squad_handle == 0xFFFFFFFF)
    {
        return EVENT_IGNORED;
    }
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && !activity->is_disabled())
        {
            activity->report_patrol_reached(data.squad_handle, data.patrol_node);
        }
    }
    return EVENT_OK;
}

// 0x00769A20 (:3597)
event_result activity_report_action_node(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_HUMAN_USING_ACTION_NODE)
    {
        return EVENT_IGNORED;
    }
    const human_using_action_node_event& data = static_cast<const human_using_action_node_event&>(in_event);
    if (data.action_node_handle == 0xFFFFFFFF || data.human_handle == 0xFFFFFFFF)
    {
        return EVENT_IGNORED;
    }
    const object* user = World.object_from_handle(data.human_handle);
    // Original: only an npc counts.
    if (user == nullptr || user->get_obj_type() != OT_HUMAN || user->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return EVENT_IGNORED;
    }
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && !activity->is_disabled() && activity->was_initialized_or_active())
        {
            if (data.start)
            {
                activity->report_action_node_start(data.action_node_handle);
            }
            else
            {
                activity->report_action_node_end(data.action_node_handle);
            }
        }
    }
    return EVENT_OK;
}

// 0x00756550 (:3644). Original: the delivery and demolitions master carry on.
void activity_suspend_activities_special(activity_disable_reason disable_reason)
{
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && activity->activity_props.modal && activity->mode != ACTIVITY_MODE_DELIVERY && activity->mode != ACTIVITY_MODE_DEMOLITIONS_MASTER)
        {
            activity->suspend(disable_reason);
        }
    }
}

// 0x007565D0 (:3673)
void activity_suspend_activities(activity_disable_reason disable_reason)
{
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && activity->activity_props.modal)
        {
            activity->suspend(disable_reason);
        }
    }
}

// 0x00756640 (:3694). Original: continuous attack and the mission rail carry on.
void activity_suspend_diversions(activity_disable_reason disable_reason)
{
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && !activity->activity_props.modal && activity->mode != ACTIVITY_MODE_CONTINUOUS_ATTACK && activity->mode != ACTIVITY_MODE_MISSION_VRAIL)
        {
            activity->suspend(disable_reason);
        }
    }
}

// 0x007566C0 (:3723)
void activity_reinstate_activities(activity_disable_reason disable_reason)
{
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && activity->activity_props.modal)
        {
            activity->reinstate(disable_reason);
        }
    }
}

// 0x00756700 (:3736)
void activity_reinstate_diversions(activity_disable_reason disable_reason)
{
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && !activity->activity_props.modal)
        {
            activity->reinstate(disable_reason);
        }
    }
}

// 0x00756740 (:3750)
void activity_end_ready_activity(activity_end_ready_reasons reason)
{
    if (Current_activity_mode != ACTIVITY_MODE_NONE)
    {
        return;
    }
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        activity_base* activity = activity_get_activity_data(i);
        if (activity == nullptr || !activity->activity_props.modal)
        {
            continue;
        }
        // Original: a high alert does not end a delivery that waits.
        if (reason == AERR_ALERT_LEVEL_TOO_HIGH && i == ACTIVITY_MODE_DELIVERY)
        {
            continue;
        }
        if (activity->get_activity_state() != BASE_ACTIVITY_STATE_STARTING && activity->was_initialized() && !activity->is_active())
        {
            activity->activity_kill();
        }
    }
}

// 0x007567C0 (:3783)
void activity_enable_all_after_mission()
{
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        if (All_activities[i] != nullptr)
        {
            All_activities[i]->enable(ADR_MISSION); // rfg.exe clears the flag's bit directly
        }
    }
}

// 0x007567F0 (:3792)
void activity_disable_mayhem_for_tutorial()
{
    Mayhem_activity.disable(ADR_MISSION);
}

// 0x00756820 (:3797)
void activity_kill_all_except(activity_modes mode)
{
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && activity->mode != mode && activity->get_activity_state() != BASE_ACTIVITY_STATE_WAIT)
        {
            activity->activity_kill();
        }
    }
}

// 0x00756870 (:3812)
bool activity_is_diversion(activity_modes mode)
{
    const activity_base* activity = activity_get_activity_data(mode);
    return activity != nullptr && !activity->activity_props.modal;
}

// 0x007B5550 (:3831)
void activity_system_init()
{
    activity_data_system_init();
    load_raid_data(-1);
    house_arrest_system_init(-1);
    convoy_definitions_init();
    courier_definitions_init();
    vrail_data_system_init(-1);
    demolitions_master_system_init(-1);
    Current_activity_mode = ACTIVITY_MODE_NONE;
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        if (All_activities[i] != nullptr)
        {
            All_activities[i]->system_init();
        }
    }
}

// 0x007568B0 (:3892)
void activity_system_shutdown()
{
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        if (All_activities[i] != nullptr)
        {
            All_activities[i]->system_shutdown();
        }
    }
}

// 0x00769AB0 (:3907)
void activity_system_level_init_pre()
{
    Activity_node_start_on_load = 0xFFFFFFFF;
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr)
        {
            activity->level_init();
        }
    }
    Last_activity_spawned.spawn_node_handle = 0xFFFFFFFF;
    Last_activity_spawned.mode = ACTIVITY_MODE_NONE;
    Current_activity_mode = ACTIVITY_MODE_NONE;
    Next_cover_check_time.set(0);
    Post_mission_cooldown.set(0);
    Post_activity_cooldown.set(0);
    Post_leave_safehouse_cooldown.set(0);
    Last_cover_blow_result = false;
    Activity_pending_autosave = false;
    notify_info recruitment;
    recruitment.data = nullptr;
    recruitment.callback = activity_report_recruitment;
    recruitment.type = EVENT_RECRUITMENT;
    gem_register_for_event(recruitment);
    notify_info action_node;
    action_node.data = nullptr;
    action_node.callback = activity_report_action_node;
    action_node.type = EVENT_HUMAN_USING_ACTION_NODE;
    gem_register_for_event(action_node);
}

// 0x007B72B0 (:3953)
void activity_system_level_init_post()
{
    activity_spawn_level_init_post();
    activity_mission_menu_init_post();
    Activity_spawn_period.set(15000); // Magic number: fifteen seconds before the first spawn
    activity_register_damage_level_init(); // rfg.exe inlines it
    Post_activity_cooldown.set_rand(90000, 120000); // Magic number: one and a half to two minutes
    activity_raid_attack_level_init_post();
    house_arrest_level_init_post();
}

// 0x007568D0 (:3980)
bool activity_allow_guerrilla_reinforcements()
{
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        const activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && activity->is_active() && !activity->allow_guerrilla_reinforcements())
        {
            return false;
        }
    }
    return true;
}

// 0x00756930 (:4001)
void activity_system_level_shutdown()
{
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && (activity->was_initialized() || activity->is_active()))
        {
            activity->activity_kill();
        }
    }
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr)
        {
            activity->level_shutdown();
        }
    }
}

// 0x007569B0 (:4037)
bool activity_outside_defensive_perimeter(const npc* np, const vector& pos, bool& result, const float* buffer_dist)
{
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        const activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && activity->outside_defensive_perimeter(np, pos, result, buffer_dist))
        {
            return true;
        }
    }
    return false;
}

// 0x007B55B0 (:4057). Original: ends a raid that only waits for the player when one of its reinforcement squads goes.
void activity_squad_destroy_notify(const object_squad* squad)
{
    if (squad == nullptr)
    {
        return;
    }
    activity_raid_data* const raids[2] = {&Offensive_raid, &Defensive_raid};
    for (int r = 0; r < 2; r++)
    {
        activity_raid_data* raid = raids[r];
        if (raid->get_activity_state() != BASE_ACTIVITY_STATE_READY)
        {
            continue;
        }
        // Original: keeps walking after it ends the raid, so a squad listed twice ends it twice.
        for (int i = 0; i < raid->reinforcement_squad_list.size(); i++)
        {
            if (squad->get_handle() == raid->reinforcement_squad_list[i].squad_handle)
            {
                raid->activity_end();
            }
        }
    }
}

// 0x00769B80 (:4065)
void activity_system_save(cfile* save_file)
{
    if (!cf_write_uint32(Activity_node_start_on_load, save_file))
    {
        return;
    }
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr)
        {
            activity->save_activity_state(save_file);
        }
    }
}

// 0x00769BE0 (:4096)
void activity_system_load(cfile* save_file)
{
    Activity_node_start_on_load = cf_read_uint32(save_file);
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr)
        {
            activity->load_activity_state(save_file);
        }
    }
}

// 0x00769C30 (:4113)
void activity_release_squad(unsigned int spawn_node_handle)
{
    if (spawn_node_handle == 0xFFFFFFFF)
    {
        return;
    }
    object* node = World.object_from_handle(spawn_node_handle);
    if (node == nullptr || node->get_obj_type() != OT_SQUAD_SPAWN_NODE)
    {
        return;
    }
    const unsigned int squad_handle = static_cast<object_squad_spawn_node*>(node)->get_child_squad_handle();
    if (squad_handle == 0xFFFFFFFF)
    {
        return;
    }
    object* squad_objp = World.object_from_handle(squad_handle);
    if (squad_objp == nullptr || squad_objp->get_obj_type() != OT_SQUAD)
    {
        return;
    }
    object_squad* squad = static_cast<object_squad*>(squad_objp);
    squad->squad_flags.hands_off_raid_squad = 0;
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && activity->was_initialized_or_active() && activity->accept_new_squad(squad))
        {
            return;
        }
    }
}

// 0x00756A10 (:4148)
bool activity_disable_ambient_ped_spawn()
{
    return Current_activity_mode == ACTIVITY_MODE_RAID_DEFEND || Current_activity_mode == ACTIVITY_MODE_RAID_ATTACK;
}

// 0x00756A50 (:4171)
unsigned int activity_get_last_spawned()
{
    const activity_base* latest = nullptr;
    int latest_time = 0x7FFFFFFF;
    for (int i = 0; i < NUM_OF_ACTIVITY_MODES; i++)
    {
        const activity_base* activity = activity_get_activity_data(i);
        if (activity != nullptr && activity->activity_props.modal && activity->get_activity_state() == BASE_ACTIVITY_STATE_READY)
        {
            const int time = activity->get_state_duration();
            if (time < latest_time)
            {
                latest = activity;
                latest_time = time;
            }
        }
    }
    return latest == nullptr ? 0xFFFFFFFF : latest->get_spawn_object_handle();
}

// 0x00756AD0 (:4211)
bool activity_disable_ambient_veh_spawn()
{
    return Current_activity_mode == ACTIVITY_MODE_RAID_DEFEND || Current_activity_mode == ACTIVITY_MODE_RAID_ATTACK;
}

// 0x00756B10 (:4237)
base_activity_states activity_state_from_name(const char* name)
{
    for (int i = 0; i < NUM_BASE_ACTIVITY_STATES; i++)
    {
        if (v_stricmp(ACTIVITY_STATE_NAMES[i], name) == 0)
        {
            return (base_activity_states)i;
        }
    }
    return BASE_ACTIVITY_STATE_INVALID;
}

// 0x00769CF0 (:4261)
player_start* find_parented_player_start(object* objp)
{
    object* first = objp->child_ptr;
    object* child = first;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_PLAYER_START && static_cast<player_start*>(child)->activity_respawn)
        {
            return static_cast<player_start*>(child);
        }
        child = child->child_next;
        if (child == first)
        {
            return nullptr;
        }
    }
    return nullptr;
}

// 0x0077F550 (:4279)
object_activity_spawn* find_activity_spawn_node(object* activity_obj)
{
    for (int i = 0; i < World.get_num_objects<object_activity_spawn>(); i++)
    {
        object_activity_spawn* node = World.get_object<object_activity_spawn>(i);
        if (activity_obj->get_obj_type() == node->get_activity_type() && node->get_activity_handle() == activity_obj->get_handle())
        {
            return node;
        }
    }
    return nullptr;
}

// 0x0077F5C0 (:4295)
player_start* activity_get_player_start_from_spawn(object* activity_obj)
{
    object_activity_spawn* node = find_activity_spawn_node(activity_obj);
    if (node != nullptr)
    {
        return find_parented_player_start(node);
    }
    object* parent = World.object_from_handle(activity_obj->get_parent());
    if (parent != nullptr && parent->get_obj_type() == activity_obj->get_obj_type())
    {
        node = find_activity_spawn_node(parent);
        if (node != nullptr)
        {
            return find_parented_player_start(node);
        }
    }
    object* first = activity_obj->child_ptr;
    object* child = first;
    while (child != nullptr)
    {
        if (child->get_obj_type() == activity_obj->get_obj_type())
        {
            node = find_activity_spawn_node(child);
            if (node != nullptr)
            {
                return find_parented_player_start(node);
            }
        }
        child = child->child_next;
        if (child == first)
        {
            return nullptr;
        }
    }
    return nullptr;
}

// 0x0077F690 (:4330)
object* activity_get_player_start(unsigned int activity_handle)
{
    if (activity_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(activity_handle);
    if (objp == nullptr)
    {
        return nullptr;
    }
    if (objp->get_obj_type() == OT_AREA_DEFENSE_NODE)
    {
        object_activity_spawn* start = static_cast<object_area_defense_node*>(objp)->get_start_node();
        if (start != nullptr)
        {
            return start;
        }
    }
    player_start* start = activity_get_player_start_from_spawn(objp);
    if (start != nullptr)
    {
        return start;
    }
    // Magic number: picks among at most 10 children, the respawn ones if any.
    object* candidates[10];
    int count = 0;
    object* first = objp->child_ptr;
    for (object* child = first; child != nullptr; )
    {
        if (child->get_obj_type() == OT_PLAYER_START && static_cast<player_start*>(child)->activity_respawn)
        {
            candidates[count++] = child;
            if (count > 9)
            {
                break;
            }
        }
        child = child->child_next;
        if (child == first)
        {
            break;
        }
    }
    if (count == 0)
    {
        // Original: no limit on this walk, so more than 10 plain player starts overrun the array.
        for (object* child = first; child != nullptr; )
        {
            if (child->get_obj_type() == OT_PLAYER_START)
            {
                candidates[count++] = child;
            }
            child = child->child_next;
            if (child == first)
            {
                break;
            }
        }
        if (count == 0)
        {
            return objp;
        }
    }
    return candidates[rand() % count];
}

// 0x0077F750 (:4411). rfg.exe: LTCG, activity_node in ESI.
static object* find_activity_start_object(object* activity_node)
{
    if (activity_node == nullptr)
    {
        return nullptr;
    }
    object* start = activity_get_player_start(activity_node->get_handle());
    return start == nullptr ? activity_node : start;
}

// 0x0077F770 (:4426)
void activity_warp_and_start(object* activity_node, bool force_reload)
{
    if (Multiplayer)
    {
        return;
    }
    player* pp = player_get_local_player();
    if (pp == nullptr || Mission_active_index != INVALID_MISSION_HANDLE)
    {
        return;
    }
    vi_exit_data exit_data;
    exit_data.flags.teleport = 1;
    vehicle_interaction_request_exit(pp, &exit_data);
    object* start = find_activity_start_object(activity_node);
    // Original: dereferences a null start when activity_node is null.
    const vector& pos = start->pos;
    if (obj_zone_streamed(pos) && !force_reload)
    {
        pp->update_pos_and_orient(pos, start->orient, true);
        human_controller_set_vel(pp, ZERO_VECTOR);
        pp->adjust_child_transforms(false, true);
        activity_force_start_generic(activity_node, true);
        return;
    }
    Activity_node_start_on_load = activity_node->get_handle();
    World.save_and_warp(pos, start->orient);
}

// 0x007A8710 (:4457). Original: the riding shotgun and area defense nodes, and a house arrest node with its start state
// cleared, only reach the warp: nothing starts them here. Nor does a demolitions master that is not active.
void activity_force_start_generic(object* activity_node, bool reload_on_fail)
{
    bool failed = false;
    switch (activity_node->get_obj_type())
    {
    case OT_CONVOY_END:
        activity_kill_all_except(ACTIVITY_MODE_CONVOY);
        failed = !activity_convoy_start(static_cast<object_convoy_end_point*>(activity_node));
        break;
    case OT_RAID_NODE:
    {
        object_raid_node* node = static_cast<object_raid_node*>(activity_node);
        if (node->is_defensive_raid())
        {
            activity_kill_all_except(ACTIVITY_MODE_RAID_DEFEND);
            failed = !activity_raid_start_raid(node);
        }
        break;
    }
    case OT_HOUSE_ARREST_NODE:
        activity_kill_all_except(ACTIVITY_MODE_HOUSE_ARREST);
        static_cast<object_house_arrest_node*>(activity_node)->enable(); // rfg.exe inlines it
        failed = true;
        break;
    case OT_DEMOLITIONS_MASTER_NODE:
        activity_kill_all_except(ACTIVITY_MODE_DEMOLITIONS_MASTER);
        if (activity_is_active(ACTIVITY_MODE_DEMOLITIONS_MASTER))
        {
            Demolitions_master_activity.do_activity_kill();
            return;
        }
        break;
    case OT_RIDING_SHOTGUN_NODE:
        activity_kill_all_except(ACTIVITY_MODE_VEHICLE_RAIL);
        failed = true;
        break;
    case OT_DELIVERY_NODE:
    {
        activity_kill_all_except(ACTIVITY_MODE_DELIVERY);
        object_delivery_node* delivery_node = static_cast<object_delivery_node*>(activity_node);
        object_vehicle_spawn_node* spawn_node = activity_delivery_get_vehicle_spawn(delivery_node);
        if (spawn_node != nullptr)
        {
            delivery_node->set_disabled(false);
            vehicle* vp = World.object_from_handle<vehicle>(spawn_node->get_child_vehicle_handle());
            if (vp != nullptr)
            {
                vector diff = vp->pos - spawn_node->pos;
                if (diff.mag() > 5.0f) // Magic number: five metres from its node
                {
                    World.destroy_object(vp, false);
                }
            }
            spawn_node->spawn_child_object(0xFFFFFFFF);
            if (!activity_delivery_start_activity(delivery_node) && reload_on_fail)
            {
                activity_warp_and_start(activity_node, true);
            }
        }
        break;
    }
    case OT_COURIER_END:
        activity_kill_all_except(ACTIVITY_MODE_COURIER);
        Courier_chase_data.spawned_from_console = true;
        failed = !Courier_chase_data.activity_base::start_activity_spawn(activity_node, district_local_player());
        break;
    case OT_AREA_DEFENSE_NODE:
        activity_kill_all_except(ACTIVITY_MODE_AREA_DEFENSE);
        failed = true;
        break;
    default:
        break;
    }
    if (failed && reload_on_fail)
    {
        activity_warp_and_start(activity_node, true);
    }
}

// 0x00796180 (:4627)
const district* activity_int_get_district_from_node(const object* objp)
{
    if (objp == nullptr)
    {
        return nullptr;
    }
    vector pos;
    if (objp->get_obj_type() != OT_CONVOY_END || !static_cast<const object_convoy_end_point*>(objp)->get_end_point(pos))
    {
        pos = objp->pos;
    }
    return district_get_from_pos(pos);
}

// 0x007961E0 (:4648)
unsigned int activity_int_get_district_handle_from_node(const object* objp)
{
    const district* dp = activity_int_get_district_from_node(objp); // rfg.exe inlines it
    return dp == nullptr ? 0xFFFFFFFF : dp->get_handle();
}

// 0x00756B50 (:4660)
bool activity_set_node_start_on_load(unsigned int handle)
{
    Activity_node_start_on_load = handle;
    return true;
}

// 0x00756B60 (:4667)
unsigned int activity_get_node_start_on_load()
{
    return Activity_node_start_on_load;
}

// 0x00756B70 (:4673)
void activity_auto_save()
{
    Activity_pending_autosave = true;
}

// 0x00756B80 (:4678)
void activity_kill_gps_message()
{
    hud_message_remove(Activity_gps_message_handle);
}
