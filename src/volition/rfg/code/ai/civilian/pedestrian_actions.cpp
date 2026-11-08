// Original: volition/rfg/code/ai/civilian/pedestrian_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/civilian/pedestrian_actions.h"

#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/ambient_action_manager.h"
#include "volition/rfg/code/ai/civilian/ambient_common.h"
#include "volition/rfg/code/ai/civilian/ambient_edf_action.h"
#include "volition/rfg/code/ai/civilian/civilian_common.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/gameplay/air_bombing.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_internal_shared.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/spawn/ambient_spawn_manager.h"
#include "volition/rfg/code/vehicle/vehicle_audio.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/os/frametime.h"

// Position 6 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG06")

extern timestamp Spawn_cooldown_period; // ambient_action_manager.cpp

static farray<unsigned int, 16> Last_action_npc_list; // 0x01642568
static timestamp Ambient_action_timer; // 0x01642560
static const float AI_VEHICLE_AT_CHECKPOINT_DIST_SQ = 25.0f; // 0x0164255C
static const unsigned char NAVPOINT_TYPE_CHECKPOINT_VALUE = 9; // the checkpoint navpoints the guards stand at
static const animation_action AMBIENT_PED_ANIM_LIST[5] = { (animation_action)822, (animation_action)846, (animation_action)847, (animation_action)848, (animation_action)849 }; // 0x012C8534

// 0x005E18E0 (:185)
static bool ped_should_pause(const npc* np)
{
    if (object_road_point_on_road(np->pos, false) != nullptr)
    {
        return false;
    }
    const ai_pedestrian_info& info = np->get_ai()->pedestrian_info;
    return info.paused ? !info.pause_timer.elapsed() : info.pause_timer.elapsed();
}

// 0x005E1920 (:199). LTCG: np EAX, is_paused CL.
static void ped_set_paused(npc* np, bool is_paused)
{
    ai_pedestrian_info& info = np->get_ai()->pedestrian_info;
    if (info.paused != is_paused)
    {
        info.paused = is_paused;
        if (is_paused)
        {
            info.pause_timer.set_rand(7000, 10000);
        }
        else
        {
            info.pause_timer.set_rand(20000, 30000);
        }
    }
}

// True when the squad is a player's, or defends one. Inlined in both goals below.
static bool squad_belongs_to_player(const ai_system* ai)
{
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr)
    {
        return false;
    }
    if (squad->get_player() != nullptr)
    {
        return true;
    }
    gfm_orders* orders = squad->get_squad_orders();
    if (orders == nullptr || squad->get_squad_orders_type() != GFM_ORDERS_TYPE_DEFEND)
    {
        return false;
    }
    const object* target = orders->get_target_object();
    return target != nullptr && object_is_player(target);
}

// 0x00675290 (:251)
int pedestrian_wander_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const ai_system* ai = np->get_ai();
    if (!np->npc_flags.ambient_spawn)
    {
        const object_squad* squad = ai->get_squad();
        if (squad == nullptr || squad->get_squad_orders_type() != GFM_ORDERS_TYPE_AMBIENT_PED)
        {
            return -1;
        }
    }
    return squad_belongs_to_player(ai) ? -1 : 80;
}

// 0x006561C0 (:271)
int pedestrian_action_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (ALERT_LEVEL_GREEN < gsm_get_alert_level() || !np->npc_flags.ambient_spawn || np->amb_prop_info != nullptr || human_in_vehicle_interaction(np, nullptr))
    {
        return -1;
    }
    const ai_system* ai = np->get_ai();
    if (!ai->pedestrian_info.animation_cooldown.elapsed())
    {
        return -1;
    }
    if (ai->planner.get_current_goal() == this)
    {
        return 90;
    }
    const unsigned int handle = np->get_handle();
    if (Last_action_npc_list.find(handle, false) > -1 || ai->is_in_player_squad(true) != nullptr)
    {
        return -1;
    }
    if (Ambient_action_timer.valid() && !Ambient_action_timer.elapsed())
    {
        return -1;
    }
    const float dist_sq = vec_dist_squared(player_get_local_player()->pos, np->pos);
    if (225.0f < dist_sq && (np->is_offscreen() || 625.0f < dist_sq))
    {
        return -1;
    }
    return 90;
}

// 0x005E1960 (:340)
bool pedestrian_action_goal::store_setup_data(ai_goal_setup_data&, const npc* np) const
{
    if (human_in_vehicle_interaction(np, nullptr))
    {
        return false;
    }
    return !(Ambient_action_timer.valid() && !Ambient_action_timer.elapsed() && np->get_ai()->planner.get_current_goal() != this);
}

// 0x006562D0 (:361)
void pedestrian_action_goal::commit_setup_data(npc* np, const ai_goal_setup_data&) const
{
    if (np->get_ai()->planner.get_current_goal() != this)
    {
        Ambient_action_timer.set_rand(5000, 8000);
    }
    if (Last_action_npc_list.full())
    {
        Last_action_npc_list.remove(Last_action_npc_list.size() - 1);
    }
    const unsigned int handle = np->get_handle();
    Last_action_npc_list.add_unique(handle);
}

// 0x005E19B0 (:382)
bool ai_should_use_panicked_goal()
{
    if (air_bomb_is_running())
    {
        return true;
    }
    if (ALERT_LEVEL_GREEN < gsm_get_alert_level() && mission_get_mode() == MISSION_MODE_SNIPER)
    {
        return true;
    }
    return ALERT_LEVEL_ORANGE < gsm_get_alert_level();
}

// 0x00656330 (:405)
int pedestrian_panicked_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (!np->npc_flags.ambient_spawn || !ai_should_use_panicked_goal() || squad_belongs_to_player(np->get_ai()))
    {
        return -1;
    }
    if (mission_get_mode() != MISSION_MODE_SNIPER && !air_bomb_is_running())
    {
        return np->amb_prop_info != nullptr ? 105 : 120;
    }
    return 5950;
}

// 0x005E19E0 (:439)
int ambient_action_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const ambient_ped_action_type type = np->get_ai()->amb_action_info.action_type;
    return np->npc_flags.ambient_spawn && AMBIENT_PED_ACTION_TYPE_NONE < type && type <= AMBIENT_PED_ACTION_TYPE_PASSENGER_DROP_OFF_WAIT ? 115 : -1;
}

// 0x005E1A10 (:458)
bool ambient_action_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const vector& destination = np->get_ai()->amb_action_info.destination_pos;
    if (!destination.is_null())
    {
        goal_setup_data.position = destination;
    }
    return true;
}

// 0x00615C40 (:472)
void ambient_action_goal::commit_setup_data(npc* np, const ai_goal_setup_data&) const
{
    ai_system* ai = np->get_ai();
    ai_ambient_action_info& info = ai->amb_action_info;
    const object* vp = info.vehicle_target_handle != 0xFFFFFFFF ? World.object_from_handle(info.vehicle_target_handle) : nullptr;
    if (vp != nullptr && vp->get_obj_type() == OT_VEHICLE)
    {
        ai->desired_vehicle = info.vehicle_target_handle;
        if (info.action_type == AMBIENT_PED_ACTION_TYPE_TAKE_PARKED_VEHICLE)
        {
            ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_DRIVER;
        }
        else if (info.action_type == AMBIENT_PED_ACTION_TYPE_RIDE_PARKED_VEHICLE || info.action_type == AMBIENT_PED_ACTION_TYPE_PASSENGER_PICKUP_WAIT)
        {
            ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_PASSENGER;
        }
        ai->desired_vehicle_dist_to_goal_pos = 3.0f;
        ai->desired_vehicle_speed = 400.0f <= vec_dist_squared(vp->pos, info.destination_pos) ? 9.0f : 5.0f;
    }
    else
    {
        ai->desired_vehicle_speed = 8.0f;
    }
    info.reset_action_timeout();
}

// 0x005E1A50 (:505)
int pedestrian_pause_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return ped_should_pause(np) ? 4000 : -1;
}

// 0x00656400 (:520)
ai_results pedestrian_pause_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    ai_system* ai = np->get_ai();
    ped_set_paused(np, true);
    ai->stop_moving();
    human_set_stance(np, HST_STAND);
    ai->perception.set_attention_mode(ATTENTION_IDLE, np->amb_prop_info != nullptr ? ATTENTION_STEERING_PLANT_FEET : ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    return AI_RESULT_SUCCESS;
}

// 0x005E1AA0 (:542)
ai_results pedestrian_pause_action::do_action(npc*) const
{
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E1AB0 (:550)
int pedestrian_anim_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->get_ai()->planner.get_current_action() != this ? 3000 : 1000;
}

// 0x00656550 (:559)
ai_results pedestrian_anim_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        ai_system* ai = np->get_ai();
        human_set_stance(np, HST_STAND);
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        const animation_action anim = AMBIENT_PED_ANIM_LIST[int_rand_range(0, 4)];
        ai->desired_anim_action = anim;
        np->char_inst->play_action(anim, ANIM_ALIAS_PED_ACTION, 1.0f, 0);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E1AF0 (:581)
ai_results pedestrian_anim_action::do_action(npc* np) const
{
    return np->char_inst->is_playing_action(ANIM_ALIAS_PED_ACTION) ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x00615D30 (:590)
ai_results pedestrian_anim_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->get_ai()->pedestrian_info.animation_cooldown.set_rand(8000, 15000);
        if (np->char_inst->rdd.is_loaded())
        {
            np->char_inst->stop_action(ANIM_ALIAS_PED_ACTION, 5, false);
        }
    }
    return AI_RESULT_SUCCESS;
}

// Ends an ambient vehicle errand: the npc goes away. vehicle_spawn marks it to drive off as an ambient vehicle spawn, otherwise it may leave the vehicle again.
static void finish_ambient_vehicle_action(npc* np, bool vehicle_spawn)
{
    np->get_ai()->amb_action_info.reset(AMBIENT_PED_ACTION_TYPE_DESPAWN);
    if (vehicle_spawn)
    {
        np->npc_flags.ambient_vehicle_spawn = 1;
        np->hflags.limited_vehicle_exit = 1;
    }
    else
    {
        np->npc_flags.ambient_vehicle_spawn = 0;
        np->hflags.disallow_vehicle_exit = 0;
        np->hflags.limited_vehicle_exit = 0;
    }
    Spawn_cooldown_period.set(4000);
}

static vehicle* get_vehicle_from_handle(unsigned int handle)
{
    object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
    return objp != nullptr && objp->get_obj_type() == OT_VEHICLE ? static_cast<vehicle*>(objp) : nullptr;
}

// 0x00615D80 (:609)
int ambient_take_vehicle_action::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const ai_ambient_action_info& info = np->get_ai()->amb_action_info;
    if (info.action_type != AMBIENT_PED_ACTION_TYPE_TAKE_PARKED_VEHICLE && info.action_type != AMBIENT_PED_ACTION_TYPE_RIDE_PARKED_VEHICLE)
    {
        return -1;
    }
    if (player_get_local_player()->vehicle_handle == info.vehicle_target_handle)
    {
        return -1;
    }
    const vehicle* vp = get_vehicle_from_handle(info.vehicle_target_handle);
    if (vp == nullptr || vp->is_killed())
    {
        return -1;
    }
    if (!human_in_vehicle(np, vp))
    {
        desired_conditions = desired_conditions | ai_plan_conditions(49);
    }
    return 100;
}

// 0x005E1B20 (:653)
void ambient_take_vehicle_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// True while a live npc of the list is not yet in the vehicle.
static bool waiting_for_passengers(const ai_ambient_action_info& info, const vehicle* vp, bool want_in)
{
    for (int i = 0; i < info.npc_targets.size(); i++)
    {
        const unsigned int handle = info.npc_targets[i];
        const object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC || objp->is_killed())
        {
            continue;
        }
        if (human_in_vehicle(static_cast<const human*>(objp), vp) != want_in)
        {
            return true;
        }
    }
    return false;
}

// 0x00615E60 (:659)
ai_results ambient_take_vehicle_action::do_action(npc* np) const
{
    if (human_in_vehicle_transition(np, nullptr, VI_INTERACTION_ENTER))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    const ai_ambient_action_info& info = np->get_ai()->amb_action_info;
    if (np->vehicle_handle != info.vehicle_target_handle)
    {
        return AI_RESULT_FAILURE;
    }
    const vehicle* vp = get_vehicle_from_handle(np->vehicle_handle);
    if (vp == nullptr || vp->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    if (waiting_for_passengers(info, vp, true))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    finish_ambient_vehicle_action(np, true);
    return AI_RESULT_SUCCESS;
}

// 0x00615FC0 (:716)
int ambient_park_vehicle_action::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const ai_ambient_action_info& info = np->get_ai()->amb_action_info;
    if (info.action_type != AMBIENT_PED_ACTION_TYPE_PARK_VEHICLE)
    {
        return -1;
    }
    const vehicle* vp = get_vehicle_from_handle(np->vehicle_handle);
    if (vp == nullptr || info.destination_pos.is_null())
    {
        return -1;
    }
    if (9.0f < vec_dist_squared(vp->pos, info.destination_pos))
    {
        desired_conditions = desired_conditions | ai_plan_conditions(50);
    }
    return 100;
}

// 0x005E1B30 (:746)
void ambient_park_vehicle_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x006160D0 (:752)
ai_results ambient_park_vehicle_action::do_action(npc* np) const
{
    finish_ambient_vehicle_action(np, false);
    return AI_RESULT_SUCCESS;
}

// 0x00616160 (:778)
int ambient_driver_pick_up_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    if (np->get_ai()->amb_action_info.action_type != AMBIENT_PED_ACTION_TYPE_DRIVER_PICKUP_PASSENGER)
    {
        return -1;
    }
    const vehicle* vp = get_vehicle_from_handle(np->vehicle_handle);
    return vp != nullptr && vehicle_passenger_in_seat(vp, VSI_DRIVER) == np ? 10 : -1;
}

// 0x005E1B40 (:805)
void ambient_driver_pick_up_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x006161C0 (:811)
ai_results ambient_driver_pick_up_action::do_action(npc* np) const
{
    ai_ambient_action_info& info = np->get_ai()->amb_action_info;
    if (np->vehicle_handle != info.vehicle_target_handle)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = get_vehicle_from_handle(np->vehicle_handle);
    if (vp == nullptr || vp->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    vehicle_set_controls_when_abandoned(vp);
    info.reset_action_timeout();
    if (waiting_for_passengers(info, vp, true))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    finish_ambient_vehicle_action(np, true);
    return AI_RESULT_SUCCESS;
}

// 0x00616340 (:865)
int ambient_passenger_pick_up_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const ai_ambient_action_info& info = np->get_ai()->amb_action_info;
    if (info.action_type != AMBIENT_PED_ACTION_TYPE_PASSENGER_PICKUP_WAIT)
    {
        return -1;
    }
    const vehicle* vp = get_vehicle_from_handle(info.vehicle_target_handle);
    return vp != nullptr && !vp->is_killed() ? 100 : -1;
}

// 0x005E1B50 (:888)
void ambient_passenger_pick_up_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x00690F90 (:894)
ai_results ambient_passenger_pick_up_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const vehicle* vp = get_vehicle_from_handle(ai->amb_action_info.vehicle_target_handle);
    if (vp == nullptr || vp->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    if (human_in_vehicle_transition(np, nullptr, VI_INTERACTION_ENTER))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (human_in_vehicle(np, vp))
    {
        finish_ambient_vehicle_action(np, true);
        return AI_RESULT_SUCCESS;
    }
    ai->perception.pay_attention(vp->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    return AI_RESULT_IN_PROGRESS;
}

// 0x00616390 (:939)
int ambient_driver_drop_off_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    if (np->get_ai()->amb_action_info.action_type != AMBIENT_PED_ACTION_TYPE_DRIVER_DROP_OFF_PASSENGER)
    {
        return -1;
    }
    const vehicle* vp = get_vehicle_from_handle(np->vehicle_handle);
    return vp != nullptr && vehicle_passenger_in_seat(vp, VSI_DRIVER) == np ? 100 : -1;
}

// 0x005E1B60 (:965)
void ambient_driver_drop_off_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x006163F0 (:971)
ai_results ambient_driver_drop_off_action::do_action(npc* np) const
{
    const ai_ambient_action_info& info = np->get_ai()->amb_action_info;
    if (np->vehicle_handle != info.vehicle_target_handle)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = get_vehicle_from_handle(np->vehicle_handle);
    if (vp == nullptr || vp->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    vehicle_set_controls_when_abandoned(vp);
    if (waiting_for_passengers(info, vp, false))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    finish_ambient_vehicle_action(np, true);
    return AI_RESULT_SUCCESS;
}

// 0x00616540 (:1023)
int ambient_passenger_drop_off_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return np->get_ai()->amb_action_info.action_type == AMBIENT_PED_ACTION_TYPE_PASSENGER_DROP_OFF_WAIT && get_vehicle_from_handle(np->vehicle_handle) != nullptr ? 100 : -1;
}

// 0x005E1B70 (:1045)
void ambient_passenger_drop_off_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x00616590 (:1051)
ai_results ambient_passenger_drop_off_action::do_action(npc* np) const
{
    finish_ambient_vehicle_action(np, false);
    return AI_RESULT_SUCCESS;
}

// 0x005E1B80 (:1077)
int ambient_cough_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if (np->get_ai()->planner.get_current_action() == this)
    {
        return 1000;
    }
    if (!district_get_from_pos(np->pos)->get_allow_ambient_cough())
    {
        return -1;
    }
    return 33 < vrand() % 100 ? 4000 : 2000;
}

// 0x006565F0 (:1093)
ai_results ambient_cough_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        if (np->get_current_voice_line_handle() != INVALID_VOICE_LINE_HANDLE)
        {
            return AI_RESULT_FAILURE;
        }
        ai_system* ai = np->get_ai();
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        np->char_inst->play_action(ANIMATION_ACTION_COUGH, ANIM_ALIAS_NONE, 1.0f, 0);
        ai->planner.lock_action(true);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E1BF0 (:1118)
ai_results ambient_cough_action::do_action(npc* np) const
{
    return np->char_inst->is_playing_action(ANIMATION_ACTION_COUGH) ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x005E1C20 (:1129)
ai_results ambient_cough_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->get_ai()->pedestrian_info.animation_cooldown.set_rand(8000, 15000);
    }
    return AI_RESULT_SUCCESS;
}

static bool ped_node_near(const vector& pos)
{
    bbox box;
    box.min = vector(pos.x - 30.0f, pos.y - 30.0f, pos.z - 30.0f);
    box.max = vector(pos.x + 30.0f, pos.y + 30.0f, pos.z + 30.0f);
    return ped_node_near_box(box);
}

// 0x005E1C50 (:1142)
int pedestrian_wander_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if (ped_should_pause(np))
    {
        return -1;
    }
    return ped_node_near(np->pos) ? 5000 : -1;
}

// 0x00656690 (:1155)
ai_results pedestrian_wander_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        ai_system* ai = np->get_ai();
        ped_set_paused(np, false);
        ai->perception.set_attention_mode(ATTENTION_IDLE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        ai->action_obj_handle = 0xFFFFFFFF;
        ai->desired_destination.zero();
        if (!ai->pedestrian_info.near_ped_nodes(np->pos) && !amb_spawn_build_ped_waypoints(np, ai->pedestrian_info, PED_NODE_SELECT_MODE_NORMAL))
        {
            return AI_RESULT_FAILURE;
        }
        if (civilian_pick_next_ped_node(np) == nullptr)
        {
            return AI_RESULT_FAILURE;
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x006910A0 (:1173)
ai_results pedestrian_wander_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if ((np->get_handle() & 1) == (unsigned int)(Framecount % 2))
    {
        ai->yield_next_frame();
    }
    ai->perception.maybe_look_away_from_corpse();
    return civilian_ped_do_action_common(np, HMS_WALK, PED_NODE_SELECT_MODE_NORMAL) ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x005E1D70 (:1192)
int pedestrian_follow_player_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const player* local = player_get_local_player();
    if (local->current_team == np->current_team || np->current_team != HUMAN_TEAM_MARAUDER || !np->get_ai()->persona_flags.follow_player || local->is_killed())
    {
        return -1;
    }
    return vec_dist_squared(np->pos, local->pos) <= 1600.0f ? 3000 : -1;
}

// 0x00691100 (:1213)
ai_results pedestrian_follow_player_action::enter(npc* np, const ai_action* last_action) const
{
    const player* local = player_get_local_player();
    if (local == nullptr || local->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        ai_system* ai = np->get_ai();
        ai->perception.pay_attention(local->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        ai->action_timer.set(0);
    }
    return AI_RESULT_SUCCESS;
}

// 0x006B5CC0 (:1242)
ai_results pedestrian_follow_player_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    player* local = player_get_local_player();
    if (local == nullptr || local->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    if (ai->action_timer.valid() && !ai->action_timer.elapsed())
    {
        return AI_RESULT_IN_PROGRESS;
    }
    const float xz_dist_sq = vec_xz_dist_squared(np->pos, local->pos);
    const float y_dist = fabsf(np->pos.y - local->pos.y);
    bool stop = xz_dist_sq < 36.0f && y_dist < 5.0f;
    bool pathfind = false;
    if (!stop)
    {
        if (144.0f <= xz_dist_sq || 10.0f <= y_dist)
        {
            if (ai->is_pathing_to(local->pos, 144.0f, 10.0f))
            {
                if (np->movement_mode == HMM_PATHFINDING)
                {
                    np->cur_movement_state = HMS_WALK;
                }
            }
            else
            {
                pathfind = true;
            }
        }
        else
        {
            if (ai->action_timer.valid())
            {
                vector head;
                human_get_head_pos(local, head);
                stop = ai->perception.has_los_to_position(head, false);
                if (!stop)
                {
                    ai->action_timer.invalidate();
                }
            }
            pathfind = !stop;
        }
    }
    if (stop)
    {
        ai->stop_moving();
        human_set_stance(np, HST_STAND);
    }
    if (pathfind)
    {
        const ai_pathfind_results result = ai->pathfind_to(local, HMS_WALK, HST_STAND, AI_PATHFIND_PRIORITY_LOW, nullptr);
        if (result != AI_PF_SUCCESS && result <= AI_PF_RAGDOLLED)
        {
            return AI_RESULT_IN_PROGRESS;
        }
    }
    ai->action_timer.set_rand(3000, 5000);
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E1DE0 (:1316)
ai_results pedestrian_follow_player_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this && ped_should_pause(np))
    {
        np->get_ai()->stop_moving();
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E1E40 (:1333)
int wander_panicked_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return ped_node_near(np->pos) ? 2000 : -1;
}

// 0x00656800 (:1342)
ai_results wander_panicked_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        ai_system* ai = np->get_ai();
        np->play_voice_line(VOICE_LINE_PANIC, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        ai->action_obj_handle = 0xFFFFFFFF;
        ai->desired_destination.zero();
        if (!ai->pedestrian_info.near_ped_nodes(np->pos) && !amb_spawn_build_ped_waypoints(np, ai->pedestrian_info, PED_NODE_SELECT_MODE_NORMAL))
        {
            return AI_RESULT_FAILURE;
        }
        if (civilian_pick_next_ped_node(np) == nullptr)
        {
            return AI_RESULT_FAILURE;
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x00691170 (:1361)
ai_results wander_panicked_action::do_action(npc* np) const
{
    if ((np->get_handle() & 1) == (unsigned int)(Framecount % 2))
    {
        np->get_ai()->yield_next_frame();
    }
    return civilian_ped_do_action_common(np, HMS_SPRINT, PED_NODE_SELECT_MODE_NORMAL) ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x00675330 (:1382)
int ambient_veh_ride_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if (!np->npc_flags.ambient_vehicle_spawn)
    {
        return -1;
    }
    const vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return -1;
    }
    const ai_vehicle_seat_types seat = ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED);
    if (!ai_vehicle_is_usable(vp, np, seat) || seat == AI_VEHICLE_SEAT_TYPE_DRIVER)
    {
        return -1;
    }
    return vehicle_passenger_in_seat(vp, VSI_DRIVER) != nullptr ? 1000 : -1;
}

// 0x005E1F10 (:1418)
ai_results ambient_veh_ride_action::do_action(npc* np) const
{
    const vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    return vehicle_passenger_in_seat(vp, VSI_DRIVER) != nullptr ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// The usable driver's seat on a road, with or without a checkpoint ahead. Inlined in the get_cost functions below.
static bool ambient_can_drive_on_road(const npc* np, const vehicle* vp)
{
    const ai_vehicle_seat_types seat = ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED);
    return ai_vehicle_is_usable(vp, np, seat) && seat == AI_VEHICLE_SEAT_TYPE_DRIVER && object_road_get_closest_road(vp->pos, np->npc_flags.can_use_special_roads, 100.0f, false, false) != nullptr;
}

static bool vehicle_has_checkpoint(const vehicle* vp)
{
    const navpoint* checkpoint = World.object_from_handle<navpoint>(vp->vai.checkpoint_handle);
    return checkpoint != nullptr && checkpoint->type == NAVPOINT_TYPE_CHECKPOINT_VALUE;
}

// The driver may go on when no checkpoint is ahead; cost is the action's own.
static int ambient_drive_cost(const npc* np, int cost)
{
    if (!np->npc_flags.ambient_vehicle_spawn)
    {
        return -1;
    }
    const vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || !ambient_can_drive_on_road(np, vp) || vehicle_has_checkpoint(vp))
    {
        return -1;
    }
    return cost;
}

// 0x006753B0 (:1443)
int ambient_veh_drive_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return ambient_drive_cost(np, 1000);
}

// 0x006568C0 (:1482)
ai_results ambient_veh_drive_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (vp->vai.override_speed != 9.0f && !vp->vai.vai_flags.m_jumping)
    {
        vehicle_ai_set_speed_override(&vp->vai, 9.0f);
    }
    if (last_action != this)
    {
        ai->guard_node_watch_period.invalidate();
    }
    if (ai->road_route_valid() || ambient_build_road_waypoints(vp, ai->road_route, PED_NODE_SELECT_MODE_RANDOM, ai->last_road_route))
    {
        if (last_action == this)
        {
            return AI_RESULT_SUCCESS;
        }
        if (ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED) != AI_VEHICLE_SEAT_TYPE_DRIVER)
        {
            return AI_RESULT_FAILURE;
        }
        vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
        ambient_road_info destination;
        if (ai->get_current_road_destination(destination))
        {
            if (!assign_destination_info(ai, destination))
            {
                return AI_RESULT_FAILURE;
            }
            if (!vp->vai.checkpoint_timer.valid() || vp->vai.checkpoint_timer.elapsed())
            {
                vp->vai.checkpoint_timer.set(1000);
            }
            ai->flags.played_stuck_line = false;
            return AI_RESULT_SUCCESS;
        }
        ai->road_route.clear();
    }
    ai->planner.avoid_action(this, 1000);
    return AI_RESULT_FAILURE;
}

// 0x006911C0 (:1546)
ai_results ambient_veh_drive_action::do_action(npc* np) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED) != AI_VEHICLE_SEAT_TYPE_DRIVER)
    {
        return AI_RESULT_FAILURE;
    }
    ai_system* ai = np->get_ai();
    ambient_road_info destination;
    if (ai->get_current_road_destination(destination))
    {
        if (!assign_destination_info(ai, destination))
        {
            return AI_RESULT_FAILURE;
        }
        if (vec_dist_squared(np->pos, ai->desired_destination) < 400.0f)
        {
            ai->remove_current_road_destination();
        }
        if (ai->road_route.size() != 0 || ambient_build_road_waypoints(vp, ai->road_route, PED_NODE_SELECT_MODE_RANDOM, ai->last_road_route))
        {
            return civilian_drive_do_action_common(np, vp, true) ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
        }
    }
    ai->planner.avoid_action(this, 1000);
    return AI_RESULT_FAILURE;
}

static void release_horn_and_speed(npc* np)
{
    if (vehicle_get_honked_horn(np->vehicle_handle))
    {
        automobile_release_horn(np->vehicle_handle);
    }
    vehicle* vp = human_in_vehicle(np);
    if (vp != nullptr && !vp->vai.vai_flags.m_jumping)
    {
        vehicle_ai_clear_speed_override(&vp->vai);
    }
}

// 0x00616620 (:1598)
ai_results ambient_veh_drive_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        release_horn_and_speed(np);
    }
    return AI_RESULT_SUCCESS;
}

// 0x00675460 (:1620)
int ambient_veh_despawn_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return ambient_drive_cost(np, 100);
}

// 0x00656A30 (:1658)
ai_results ambient_veh_despawn_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (!vp->vai.vai_flags.m_jumping)
    {
        float speed = ai->desired_vehicle_speed;
        if (vp->is_ambient() && speed <= 0.0f)
        {
            speed = 9.0f;
        }
        if (0.0f < speed)
        {
            vehicle_ai_set_speed_override(&vp->vai, speed);
        }
        else
        {
            vehicle_ai_clear_speed_override(&vp->vai);
        }
    }
    if (!ai->road_route_valid() && !ambient_build_road_waypoints(vp, ai->road_route, PED_NODE_SELECT_MODE_DESPAWN, ai->last_road_route))
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    ai->guard_node_watch_period.invalidate();
    if (ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED) != AI_VEHICLE_SEAT_TYPE_DRIVER)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
    ambient_road_info destination;
    if (!ai->get_current_road_destination(destination) || !assign_destination_info(ai, destination))
    {
        return AI_RESULT_FAILURE;
    }
    if (!vp->vai.checkpoint_timer.valid() || vp->vai.checkpoint_timer.elapsed())
    {
        vp->vai.checkpoint_timer.set(1000);
    }
    ai->flags.played_stuck_line = false;
    return AI_RESULT_SUCCESS;
}

// Follows the current road destination towards a despawn, building more route when it runs out. False on no road.
static bool ambient_update_despawn_route(npc* np, vehicle* vp)
{
    ai_system* ai = np->get_ai();
    ambient_road_info destination;
    if (!ai->get_current_road_destination(destination) || !assign_destination_info(ai, destination))
    {
        return false;
    }
    if (vec_dist_squared(np->pos, ai->desired_destination) < 400.0f)
    {
        ai->remove_current_road_destination();
    }
    return ai->road_route.size() != 0 || ambient_build_road_waypoints(vp, ai->road_route, PED_NODE_SELECT_MODE_DESPAWN, ai->last_road_route);
}

// 0x006912B0 (:1726)
ai_results ambient_veh_despawn_action::do_action(npc* np) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED) != AI_VEHICLE_SEAT_TYPE_DRIVER || !ambient_update_despawn_route(np, vp))
    {
        return AI_RESULT_FAILURE;
    }
    return civilian_drive_do_action_common(np, vp, true) ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x00616680 (:1776)
ai_results ambient_veh_despawn_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        release_horn_and_speed(np);
    }
    return AI_RESULT_SUCCESS;
}

// 0x00675510 (:1798)
int ambient_veh_flee_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return ambient_drive_cost(np, 100);
}

// 0x00656BB0 (:1836)
ai_results ambient_veh_flee_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (!vp->vai.vai_flags.m_jumping)
    {
        vehicle_ai_clear_speed_override(&vp->vai);
    }
    if (!ai->road_route_valid() && !ambient_build_road_waypoints(vp, ai->road_route, PED_NODE_SELECT_MODE_DESPAWN, ai->last_road_route))
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    if (ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED) != AI_VEHICLE_SEAT_TYPE_DRIVER)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
    ambient_road_info destination;
    return ai->get_current_road_destination(destination) && assign_destination_info(ai, destination) ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x00691390 (:1879)
ai_results ambient_veh_flee_action::do_action(npc* np) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED) != AI_VEHICLE_SEAT_TYPE_DRIVER || !ambient_update_despawn_route(np, vp))
    {
        return AI_RESULT_FAILURE;
    }
    ai_system* ai = np->get_ai();
    if (vp->vai.ambient_wait_timer.valid())
    {
        if (!vp->vai.ambient_wait_timer.elapsed())
        {
            const human* blocker = World.object_from_handle<human>(vp->vai.ambient_wait_object);
            if (blocker != nullptr && object_is_local_player(blocker))
            {
                vehicle_set_controls_when_abandoned(vp);
                civilian_horn_do_honk(np, true);
                return AI_RESULT_IN_PROGRESS;
            }
        }
        ai->flags.played_stuck_line = false;
        vp->vai.stuck.extended_stuck_time.invalidate();
        vp->vai.stuck.stuck_threshold_time.invalidate();
        vp->vai.ambient_wait_timer.invalidate();
    }
    // Pushes before 0x006914E4: action_pos (ADD EDI,0x4A0), no hint, on roads, 10 m (0x012C8500), smoothing on.
    const ai_results result = ai_vehicle_travel_to_a_point_do_frame(np, vp, ai->action_pos, 0xFFFFFFFFFFFFFFFFull, true, 10.0f, false, false, false, true);
    return result != AI_RESULT_FAILURE ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x006166E0 (:1958)
ai_results ambient_veh_flee_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        release_horn_and_speed(np);
    }
    return AI_RESULT_SUCCESS;
}

// 0x006755C0 (:1983)
int ambient_veh_stop_at_checkpoint_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED) != AI_VEHICLE_SEAT_TYPE_DRIVER || !ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER))
    {
        return -1;
    }
    if (object_road_get_closest_road(vp->pos, np->npc_flags.can_use_special_roads, 100.0f, false, false) == nullptr)
    {
        return -1;
    }
    return vehicle_has_checkpoint(vp) ? 1000 : -1;
}

// 0x00656C80 (:2025)
ai_results ambient_veh_stop_at_checkpoint_action::enter(npc* np, const ai_action* last_action) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (!vp->vai.vai_flags.m_jumping)
    {
        vehicle_ai_set_speed_override(&vp->vai, 5.0f);
    }
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    if (ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED) != AI_VEHICLE_SEAT_TYPE_DRIVER)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
    const object* checkpoint = vp->vai.checkpoint_handle != 0xFFFFFFFF ? World.object_from_handle(vp->vai.checkpoint_handle) : nullptr;
    if (checkpoint == nullptr || checkpoint->get_obj_type() != OT_NAVPOINT)
    {
        return AI_RESULT_FAILURE;
    }
    vp->vai.checkpoint_timer.invalidate();
    return AI_RESULT_SUCCESS;
}

// 0x00691500 (:2055)
ai_results ambient_veh_stop_at_checkpoint_action::do_action(npc* np) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (vp->vai.ambient_wait_timer.valid())
    {
        if (!vp->vai.ambient_wait_timer.elapsed())
        {
            vehicle_set_controls_when_abandoned(vp);
            return AI_RESULT_IN_PROGRESS;
        }
        vp->vai.ambient_wait_timer.invalidate();
        vp->vai.stuck.stuck_threshold_time.invalidate();
        vp->vai.stuck.extended_stuck_time.invalidate();
    }
    const object* checkpoint = vp->vai.checkpoint_handle != 0xFFFFFFFF ? World.object_from_handle(vp->vai.checkpoint_handle) : nullptr;
    if (checkpoint == nullptr || checkpoint->get_obj_type() != OT_NAVPOINT)
    {
        return AI_RESULT_FAILURE;
    }
    if (vec_xz_dist_squared(checkpoint->pos, vp->pos) <= AI_VEHICLE_AT_CHECKPOINT_DIST_SQ)
    {
        ai_vehicle_stop(vp);
        if (!vp->vai.checkpoint_timer.valid())
        {
            vp->vai.checkpoint_timer.set(5000);
        }
    }
    else if (ai_vehicle_travel_to_a_point_do_frame(np, vp, checkpoint->pos, 0xFFFFFFFFFFFFFFFFull, true, 5.0f, false, false, false, true) == AI_RESULT_FAILURE) // pushes before 0x006915C8, 5 m from 0x012C8410
    {
        return AI_RESULT_FAILURE;
    }
    if (vp->vai.checkpoint_timer.valid() && vp->vai.checkpoint_timer.elapsed())
    {
        vp->vai.checkpoint_handle = 0xFFFFFFFF;
        return AI_RESULT_SUCCESS;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00616740 (:2103)
ai_results ambient_veh_stop_at_checkpoint_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        vehicle* vp = human_in_vehicle(np);
        if (vp != nullptr)
        {
            if (!vp->vai.vai_flags.m_jumping)
            {
                vehicle_ai_clear_speed_override(&vp->vai);
            }
            vp->vai.checkpoint_handle = 0xFFFFFFFF;
            vp->vai.checkpoint_timer.set(10000);
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x006167A0 (:2128)
int return_to_orders_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->get_ai()->get_squad() != nullptr ? 100 : -1;
}

// 0x00656D30 (:2140)
ai_results return_to_orders_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        np->get_ai()->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    }
    return AI_RESULT_SUCCESS;
}

// 0x00691640 (:2153)
ai_results return_to_orders_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const object_squad* squad = ai->get_squad();
    const gfm_orders* orders = squad != nullptr ? squad->get_squad_orders() : nullptr;
    if (orders == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const vector pos = orders->get_orders_pos();
    if (vec_dist_squared(np->pos, pos) < 100.0f)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    const ai_pathfind_results result = ai->pathfind_to(pos, HMS_WALK, HST_STAND, AI_PATHFIND_PRIORITY_LOW, 0xFFFFFFFFFFFFFFFFull, false);
    return result != AI_PF_SUCCESS && AI_PF_RAGDOLLED < result ? AI_RESULT_FAILURE : AI_RESULT_IN_PROGRESS;
}

// 0x005E1F40 (:2191)
int reaction_to_honk::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->npc_flags.honked_at ? 4000 : -1;
}

// 0x005E1F60 (:2200). Steps off the road to the side the npc already faces, one metre beyond its edge.
ai_results reaction_to_honk::enter(npc* np, const ai_action* last_action) const
{
    const object_path_road* road = object_road_point_on_road(np->pos, false);
    if (road != nullptr)
    {
        if (last_action == this)
        {
            return AI_RESULT_SUCCESS;
        }
        vector closest;
        const float u = road->find_closest_point_on_road(np->pos, closest, false);
        vector tangent;
        road->get_tangent(tangent, u);
        const float offset = road->get_width(u) + 1.0f;
        vector side(tangent.z, 0.0f, -tangent.x);
        side.normalize_safe();
        if (!side.is_null())
        {
            const vector plus(closest.x + offset * side.x, closest.y + offset * side.y, closest.z + offset * side.z);
            const vector minus(closest.x - offset * side.x, closest.y - offset * side.y, closest.z - offset * side.z);
            vector to_plus(plus.x - np->pos.x, plus.y - np->pos.y, plus.z - np->pos.z);
            to_plus.normalize_safe();
            ai_system* ai = np->get_ai();
            ai->desired_destination = np->orient.fvec.y * to_plus.y + np->orient.fvec.x * to_plus.x + np->orient.fvec.z * to_plus.z <= 0.0f ? minus : plus;
            if (np->npc_flags.honked_at_friendly)
            {
                ai->desired_voice_line = VOICE_LINE_HONKED_AT_MOVE;
                ai->action_substate = 1;
            }
            else if (np->npc_flags.honked_at_hostile)
            {
                ai->desired_voice_line = VOICE_LINE_HONKED_AT_RESIST;
                ai->action_substate = 1;
            }
            else
            {
                ai->desired_voice_line = VOICE_LINE_NONE;
                ai->action_substate = 0;
            }
            return AI_RESULT_SUCCESS;
        }
    }
    np->npc_flags.honked_at = 0;
    return AI_RESULT_FAILURE;
}

// 0x006916F0 (:2269)
ai_results reaction_to_honk::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (vec_dist_squared(np->pos, ai->desired_destination) < 1.0f)
    {
        return AI_RESULT_SUCCESS;
    }
    const ai_pathfind_results result = ai->pathfind_to(ai->desired_destination, HMS_WALK, HST_STAND, AI_PATHFIND_PRIORITY_LOW, 0xFFFFFFFFFFFFFFFFull, false);
    if (result != AI_PF_SUCCESS && AI_PF_RAGDOLLED < result)
    {
        return AI_RESULT_FAILURE;
    }
    np->move_speed = 3.0f;
    if (ai->action_substate != 0 && ai->desired_voice_line != VOICE_LINE_NONE)
    {
        np->play_voice_line(ai->desired_voice_line, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    }
    ai->action_substate = 0;
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E22A0 (:2306)
ai_results reaction_to_honk::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        const ai_system* ai = np->get_ai();
        if (ai->desired_voice_line != VOICE_LINE_NONE && ai->desired_voice_line == np->get_current_voice_line())
        {
            np->stop_voice(VOICE_LINE_PRIORITY_HIGH);
        }
        np->npc_flags.honked_at = 0;
        np->npc_flags.honked_at_hostile = 0;
        np->npc_flags.honked_at_friendly = 0;
    }
    return AI_RESULT_SUCCESS;
}

pedestrian_action_goal pedestrian_action_goal::the_instance; // 0x016424EC

// 0x00602DA0 (.h:14)
pedestrian_action_goal::pedestrian_action_goal()
    : ai_goal(ai_plan_conditions(128), "ped action")
{
}

ai_goal* pedestrian_action_goal::get_instance()
{
    return &the_instance;
}

// 0x005DDC40
pedestrian_action_goal::~pedestrian_action_goal()
{
}

pedestrian_wander_goal pedestrian_wander_goal::the_instance; // 0x01642508

// 0x00602D00 (.h:13)
pedestrian_wander_goal::pedestrian_wander_goal()
    : ai_goal(ai_plan_conditions(130), "ped wander")
{
}

ai_goal* pedestrian_wander_goal::get_instance()
{
    return &the_instance;
}

// 0x005DDC30
pedestrian_wander_goal::~pedestrian_wander_goal()
{
}

pedestrian_panicked_goal pedestrian_panicked_goal::the_instance; // 0x01642524

// 0x00602E40 (.h:15)
pedestrian_panicked_goal::pedestrian_panicked_goal()
    : ai_goal(ai_plan_conditions(131), "ped panicked")
{
}

// 0x005DDC60
pedestrian_panicked_goal::~pedestrian_panicked_goal()
{
}

ambient_action_goal ambient_action_goal::the_instance; // 0x01642540

// 0x00602EE0 (.h:16)
ambient_action_goal::ambient_action_goal()
    : ai_goal(ai_plan_conditions(132), "ambient action")
{
}

ai_goal* ambient_action_goal::get_instance()
{
    return &the_instance;
}

// 0x005DDC80
ambient_action_goal::~ambient_action_goal()
{
}

wander_panicked_action wander_panicked_action::the_instance; // 0x01D750C4

// 0x00602F80 (.h:18)
wander_panicked_action::wander_panicked_action()
    : ai_action("wander panicked", ai_plan_conditions_from_bits({7, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({131}))
{
    flags.has_enter = 1;
}

ai_action* wander_panicked_action::get_instance()
{
    return &the_instance;
}

// 0x005DDC90
wander_panicked_action::~wander_panicked_action()
{
}

// 0x00603280 (.h:18)
int wander_panicked_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

pedestrian_anim_action pedestrian_anim_action::the_instance; // 0x01D68894

// 0x006032C0 (.h:21)
pedestrian_anim_action::pedestrian_anim_action()
    : ai_action("ped anim action", ai_plan_conditions_from_bits({7, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({128}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* pedestrian_anim_action::get_instance()
{
    return &the_instance;
}

// 0x005DDCA0
pedestrian_anim_action::~pedestrian_anim_action()
{
}

// 0x006035C0 (.h:21)
int pedestrian_anim_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

ambient_cough_action ambient_cough_action::the_instance; // 0x01D68478

// 0x00603600 (.h:22)
ambient_cough_action::ambient_cough_action()
    : ai_action("ambient cough", ai_plan_conditions_from_bits({7, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({128}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* ambient_cough_action::get_instance()
{
    return &the_instance;
}

// 0x005DDCB0
ambient_cough_action::~ambient_cough_action()
{
}

// 0x00603900 (.h:22)
int ambient_cough_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

pedestrian_pause_action pedestrian_pause_action::the_instance; // 0x01DC9B68

// 0x00603940 (.h:24)
pedestrian_pause_action::pedestrian_pause_action()
    : ai_action("ped pause action", ai_plan_conditions_from_bits({7, 9, 47, 58, 136, 145, 153}), ai_plan_conditions_from_bits({130}))
{
    flags.has_enter = 1;
}

ai_action* pedestrian_pause_action::get_instance()
{
    return &the_instance;
}

// 0x005DDCC0
pedestrian_pause_action::~pedestrian_pause_action()
{
}

// 0x00603BD0 (.h:24)
int pedestrian_pause_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

pedestrian_wander_action pedestrian_wander_action::the_instance; // 0x01DC9F6C

// 0x00603C10 (.h:25)
pedestrian_wander_action::pedestrian_wander_action()
    : ai_action("ped wander", ai_plan_conditions_from_bits({7, 9, 47, 58, 136, 145, 153}), ai_plan_conditions_from_bits({130}))
{
    flags.has_enter = 1;
}

ai_action* pedestrian_wander_action::get_instance()
{
    return &the_instance;
}

// 0x005DDCD0
pedestrian_wander_action::~pedestrian_wander_action()
{
}

// 0x00603EA0 (.h:25)
int pedestrian_wander_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

pedestrian_follow_player_action pedestrian_follow_player_action::the_instance; // 0x01D67F14

// 0x00603EE0 (.h:26)
pedestrian_follow_player_action::pedestrian_follow_player_action()
    : ai_action("ped follow player", ai_plan_conditions_from_bits({4, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({130}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* pedestrian_follow_player_action::get_instance()
{
    return &the_instance;
}

// 0x005DDCF0
pedestrian_follow_player_action::~pedestrian_follow_player_action()
{
}

// 0x006041E0 (.h:26)
int pedestrian_follow_player_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

ambient_veh_flee_action ambient_veh_flee_action::the_instance; // 0x01D75338

// 0x00604220 (.h:28)
ambient_veh_flee_action::ambient_veh_flee_action()
    : ai_action("ambient vehicle flee", ai_plan_conditions_from_bits({9, 49, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({120}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* ambient_veh_flee_action::get_instance()
{
    return &the_instance;
}

// 0x005DDD00
ambient_veh_flee_action::~ambient_veh_flee_action()
{
}

// 0x006044C0 (.h:28)
int ambient_veh_flee_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

ambient_veh_despawn_action ambient_veh_despawn_action::the_instance; // 0x01DA0334

// 0x00604500 (.h:29)
ambient_veh_despawn_action::ambient_veh_despawn_action()
    : ai_action("ambient despawn veh drive", ai_plan_conditions_from_bits({9, 49, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({70}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* ambient_veh_despawn_action::get_instance()
{
    return &the_instance;
}

// 0x005DDD10
ambient_veh_despawn_action::~ambient_veh_despawn_action()
{
}

// 0x006047A0 (.h:29)
int ambient_veh_despawn_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

ambient_veh_drive_action ambient_veh_drive_action::the_instance; // 0x01DC9BF8

// 0x006047E0 (.h:30)
ambient_veh_drive_action::ambient_veh_drive_action()
    : ai_action("ambient vehicle drive", ai_plan_conditions_from_bits({9, 49, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({130}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* ambient_veh_drive_action::get_instance()
{
    return &the_instance;
}

// 0x005DDD20
ambient_veh_drive_action::~ambient_veh_drive_action()
{
}

// 0x00604A80 (.h:30)
int ambient_veh_drive_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

ambient_veh_ride_action ambient_veh_ride_action::the_instance; // 0x01DC9D94

// 0x00604AC0 (.h:31)
ambient_veh_ride_action::ambient_veh_ride_action()
    : ai_action("ambient vehicle ride", ai_plan_conditions_from_bits({9, 49, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({130}))
{
}

ai_action* ambient_veh_ride_action::get_instance()
{
    return &the_instance;
}

// 0x005DDD30
ambient_veh_ride_action::~ambient_veh_ride_action()
{
}

// 0x00604D50 (.h:31)
int ambient_veh_ride_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

ambient_veh_stop_at_checkpoint_action ambient_veh_stop_at_checkpoint_action::the_instance; // 0x01D9F6E4

// 0x00604D90 (.h:32)
ambient_veh_stop_at_checkpoint_action::ambient_veh_stop_at_checkpoint_action()
    : ai_action("stop at checkpoint", ai_plan_conditions_from_bits({9, 49, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({130}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* ambient_veh_stop_at_checkpoint_action::get_instance()
{
    return &the_instance;
}

// 0x005DDD40
ambient_veh_stop_at_checkpoint_action::~ambient_veh_stop_at_checkpoint_action()
{
}

// 0x00605030 (.h:32)
int ambient_veh_stop_at_checkpoint_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

ambient_take_vehicle_action ambient_take_vehicle_action::the_instance; // 0x01DC9D60

// 0x00605070 (.h:35)
ambient_take_vehicle_action::ambient_take_vehicle_action()
    : ai_action("ambient take vehicle", ai_plan_conditions(), ai_plan_conditions_from_bits({132}))
{
}

ai_action* ambient_take_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005DDD50
ambient_take_vehicle_action::~ambient_take_vehicle_action()
{
}

// 0x006050C0 (.h:35)
int ambient_take_vehicle_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

ambient_park_vehicle_action ambient_park_vehicle_action::the_instance; // 0x01D756C0

// 0x006050F0 (.h:36)
ambient_park_vehicle_action::ambient_park_vehicle_action()
    : ai_action("ambient park vehicle", ai_plan_conditions(), ai_plan_conditions_from_bits({132}))
{
}

ai_action* ambient_park_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005DDD60
ambient_park_vehicle_action::~ambient_park_vehicle_action()
{
}

// 0x00605140 (.h:36)
int ambient_park_vehicle_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

ambient_driver_pick_up_action ambient_driver_pick_up_action::the_instance; // 0x01D684FC

// 0x00605170 (.h:37)
ambient_driver_pick_up_action::ambient_driver_pick_up_action()
    : ai_action("ambient driver pick up", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({44, 132}))
{
}

ai_action* ambient_driver_pick_up_action::get_instance()
{
    return &the_instance;
}

// 0x005DDD70
ambient_driver_pick_up_action::~ambient_driver_pick_up_action()
{
}

// 0x00605210 (.h:37)
int ambient_driver_pick_up_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

ambient_passenger_pick_up_action ambient_passenger_pick_up_action::the_instance; // 0x01DC9A20

// 0x00605240 (.h:38)
ambient_passenger_pick_up_action::ambient_passenger_pick_up_action()
    : ai_action("ambient passenger pick up", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({132}))
{
}

ai_action* ambient_passenger_pick_up_action::get_instance()
{
    return &the_instance;
}

// 0x005DDD80
ambient_passenger_pick_up_action::~ambient_passenger_pick_up_action()
{
}

// 0x00605290 (.h:38)
int ambient_passenger_pick_up_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

ambient_driver_drop_off_action ambient_driver_drop_off_action::the_instance; // 0x01D720CC

// 0x006052C0 (.h:39)
ambient_driver_drop_off_action::ambient_driver_drop_off_action()
    : ai_action("ambient driver drop off", ai_plan_conditions(), ai_plan_conditions_from_bits({132}))
{
}

ai_action* ambient_driver_drop_off_action::get_instance()
{
    return &the_instance;
}

// 0x005DDD90
ambient_driver_drop_off_action::~ambient_driver_drop_off_action()
{
}

// 0x00605310 (.h:39)
int ambient_driver_drop_off_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

ambient_passenger_drop_off_action ambient_passenger_drop_off_action::the_instance; // 0x01D681CC

// 0x00605340 (.h:40)
ambient_passenger_drop_off_action::ambient_passenger_drop_off_action()
    : ai_action("ambient passenger drop off", ai_plan_conditions(), ai_plan_conditions_from_bits({132}))
{
}

ai_action* ambient_passenger_drop_off_action::get_instance()
{
    return &the_instance;
}

// 0x005DDDA0
ambient_passenger_drop_off_action::~ambient_passenger_drop_off_action()
{
}

// 0x00605390 (.h:40)
int ambient_passenger_drop_off_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

return_to_orders_action return_to_orders_action::the_instance; // 0x01D9F718

// 0x006053C0 (.h:44)
return_to_orders_action::return_to_orders_action()
    : ai_action("return to orders", ai_plan_conditions_from_bits({9, 47, 58, 136, 145, 153}), ai_plan_conditions_from_bits({43}))
{
    flags.has_enter = 1;
}

ai_action* return_to_orders_action::get_instance()
{
    return &the_instance;
}

// 0x005DDDB0
return_to_orders_action::~return_to_orders_action()
{
}

// 0x00605600 (.h:44)
int return_to_orders_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

reaction_to_honk reaction_to_honk::the_instance; // 0x01D72238

// 0x00605640 (.h:47)
reaction_to_honk::reaction_to_honk()
    : ai_action("honk reaction friendly", ai_plan_conditions_from_bits({9, 47, 58, 136, 145, 153}), ai_plan_conditions_from_bits({140}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* reaction_to_honk::get_instance()
{
    return &the_instance;
}

// 0x005DDDC0
reaction_to_honk::~reaction_to_honk()
{
}

// 0x00605880 (.h:47)
int reaction_to_honk::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

