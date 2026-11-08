// Original: volition/rfg/code/ai/civilian/ambient_edf_action.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/civilian/ambient_edf_action.h"

#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/ambient_edf_action_manager.h"
#include "volition/rfg/code/ai/civilian/ambient_common.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_internal_shared.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/vlib/code/math/rand.h"

// Position 3 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG03")

static const int TIME_TO_KILL_MS = 1000; // 0x01645300

// 0x005E0B90 (:50)
bool assign_destination_info(ai_system* aip, const ambient_road_info& destination_info)
{
    if (destination_info.road == nullptr || destination_info.destination_pos.is_null() || destination_info.destination_index < 0)
    {
        return false;
    }
    aip->desired_destination = destination_info.destination_pos;
    aip->action_pos = destination_info.destination_pos;
    aip->desired_destination_object = destination_info.road->get_handle();
    aip->action_obj_handle = destination_info.road->get_handle();
    aip->action_substate = destination_info.destination_index;
    return true;
}

// 0x00653CF0 (:66). LTCG: aip EAX. rfg.pdb lists an npc too, which the code never takes.
static ai_results amb_edf_drive_new_route(ai_system* aip, const vehicle* vp)
{
    road_spline_identifier start_road_id;
    if (!find_route_start_point(start_road_id, vp, aip->last_road_route))
    {
        return AI_RESULT_FAILURE;
    }
    aip->road_route.clear();
    return build_new_waypoints_despawn(start_road_id, aip->road_route, aip->last_road_route) && 0 < aip->road_route.size() ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x006905A0 (:77). LTCG: aip EAX, vp EDI.
static ai_results amb_edf_drive_do_drive(npc* np, ai_system* aip, vehicle* vp)
{
    ambient_road_info destination;
    if (!aip->get_current_road_destination(destination))
    {
        return amb_edf_drive_new_route(aip, vp);
    }
    if (!assign_destination_info(aip, destination))
    {
        return AI_RESULT_FAILURE;
    }
    if (vec_dist_squared(vp->pos, aip->action_pos) < 225.0f)
    {
        aip->remove_current_road_destination();
    }
    vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
    if (!vp->vai.vai_flags.m_jumping)
    {
        vehicle_ai_set_speed_override(&vp->vai, 9.0f);
    }
    return ai_vehicle_travel_to_a_point_do_frame(np, vp, aip->action_pos, 0xFFFFFFFFFFFFFFFFull, true, 10.0f, true, false, false, true) != AI_RESULT_FAILURE ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x00690680 (:118). LTCG, and inlined in each do_action.
static ai_results amb_edf_drive_do_frame(npc* np, vehicle* vp)
{
    ai_system* aip = np->get_ai();
    return aip->road_route_valid() ? amb_edf_drive_do_drive(np, aip, vp) : amb_edf_drive_new_route(aip, vp);
}

static vehicle* get_current_vehicle(const npc* np)
{
    object* objp = np->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(np->vehicle_handle) : nullptr;
    return objp != nullptr && objp->get_obj_type() == OT_VEHICLE ? static_cast<vehicle*>(objp) : nullptr;
}

// The 100 or -1 of the actions a usable driver takes.
static int driver_cost(const npc* np)
{
    vehicle* vp = get_current_vehicle(np);
    return vp != nullptr && vehicle_passenger_in_seat(vp, VSI_DRIVER) == np && ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER) && ai_vehicle_is_seat_available(vp, np, VSI_DRIVER) ? 100 : -1;
}

// The route enter builds: to the action's destination, or away to despawn once the action is complete.
static bool build_action_route(const npc* np, ai_system* aip, const vehicle* vp)
{
    if (amb_edf_action_complete(np))
    {
        return ambient_build_road_waypoints(vp, aip->road_route, PED_NODE_SELECT_MODE_DESPAWN, aip->last_road_route);
    }
    const vector destination = amb_edf_action_destination(np);
    return !destination.is_null() && road_route_create(vp->pos, destination, aip->road_route);
}

// 0x005E0C20 (:136)
int amb_edf_fly_by_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return amb_edf_action_type(np) == AMBIENT_EDF_ACTION_TYPE_FLYER ? 100 : -1;
}

// 0x006906C0 (:154)
int amb_edf_drive_by_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return amb_edf_action_type(np) == AMBIENT_EDF_ACTION_TYPE_DRIVE_BY ? driver_cost(np) : -1;
}

// 0x006749D0 (:181)
ai_results amb_edf_drive_by_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    if (!amb_edf_action_participant(np))
    {
        return AI_RESULT_FAILURE;
    }
    ai_system* ai = np->get_ai();
    if (ai->road_route_valid())
    {
        return AI_RESULT_SUCCESS;
    }
    const vehicle* vp = get_current_vehicle(np);
    return vp != nullptr && build_action_route(np, ai, vp) ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x00690760 (:230)
ai_results amb_edf_drive_by_action::do_action(npc* np) const
{
    if (!amb_edf_action_participant(np))
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = get_current_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const ai_results result = amb_edf_drive_do_frame(np, vp);
    if (result == AI_RESULT_FAILURE)
    {
        np->get_ai()->planner.avoid_action(this, 5000);
    }
    return result;
}

// 0x00690810 (:259)
int amb_edf_harass_player_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return amb_edf_action_type(np) == AMBIENT_EDF_ACTION_TYPE_HARASS_PLAYER ? driver_cost(np) : -1;
}

// 0x00674AF0 (:286)
ai_results amb_edf_harass_player_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    if (!amb_edf_action_participant(np))
    {
        return AI_RESULT_FAILURE;
    }
    ai_system* ai = np->get_ai();
    if (ai->road_route_valid())
    {
        return AI_RESULT_SUCCESS;
    }
    const vehicle* vp = get_current_vehicle(np);
    if (vp == nullptr || !build_action_route(np, ai, vp))
    {
        return AI_RESULT_FAILURE;
    }
    ai->action_state = 0;
    return AI_RESULT_SUCCESS;
}

// 0x006BBD60 (:339). State 0 follows the road until within 40 m, 1 drives at the player, 2 follows the road for good.
ai_results amb_edf_harass_player_action::do_action(npc* np) const
{
    if (!amb_edf_action_participant(np))
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = get_current_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai_system* ai = np->get_ai();
    player* enemy = player_get_local_player();
    const float dist_sq = vec_xz_dist_squared(enemy->pos, np->pos);
    ai_results result;
    if (1 < ai->action_state || (ai->action_state != 1 && 1600.0f < dist_sq))
    {
        result = amb_edf_drive_do_frame(np, vp);
    }
    else
    {
        if (ai->action_state == 0)
        {
            ai->action_state = 1;
        }
        vehicle_ai_set_speed_override(&vp->vai, 40.0f);
        result = ai_vehicle_travel_to_a_point_do_frame(np, vp, enemy->pos, 0xFFFFFFFFFFFFFFFFull, false, 0.0f, false, false, false, true);
    }
    if (result == AI_RESULT_FAILURE)
    {
        if (ai->action_state == 1)
        {
            ai->action_state = 2;
            vp->disable_for_npc_drivers_timer.set(0);
        }
        else
        {
            ai->planner.avoid_action(this, 5000);
        }
    }
    if (225.0f < dist_sq)
    {
        return result;
    }
    np->play_voice_line(VOICE_LINE_SEARCH_SPOT_ENEMY, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    const object_squad* squad = ai->get_squad();
    if (squad != nullptr)
    {
        for (int i = 0; i < squad->member_array.size(); i++)
        {
            const unsigned int member = squad->member_array[i];
            object* objp = member != 0xFFFFFFFF ? World.object_from_handle(member) : nullptr;
            if (objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_NPC)
            {
                // Declared in percept.h and defined in threaten_actions.cpp: found through its first argument.
                threaten_note_enemy(static_cast<npc*>(objp)->get_ai()->perception, enemy, true);
            }
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x00611F60 (:413)
int amb_edf_dump_civ_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if (np->current_team != HUMAN_TEAM_EDF || amb_edf_action_type(np) != AMBIENT_EDF_ACTION_TYPE_DUMP_CIV)
    {
        return -1;
    }
    vehicle* vp = get_current_vehicle(np);
    return vp != nullptr && vehicle_passenger_in_seat(vp, VSI_DRIVER) == np ? 100 : -1;
}

// 0x00674C20 (:440)
ai_results amb_edf_dump_civ_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    if (!amb_edf_action_participant(np))
    {
        return AI_RESULT_FAILURE;
    }
    ai_system* ai = np->get_ai();
    if (ai->road_route_valid())
    {
        return AI_RESULT_SUCCESS;
    }
    const vehicle* vp = get_current_vehicle(np);
    return vp != nullptr && build_action_route(np, ai, vp) ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x006908B0 (:489)
ai_results amb_edf_dump_civ_action::do_action(npc* np) const
{
    if (!amb_edf_action_participant(np))
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = get_current_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const ai_results result = amb_edf_drive_do_frame(np, vp);
    if (result == AI_RESULT_FAILURE)
    {
        np->get_ai()->planner.avoid_action(this, 5000);
    }
    return result;
}

// 0x005E0C60 (:518)
int amb_edf_civ_be_dumped_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->current_team == HUMAN_TEAM_CIVILIAN && amb_edf_action_type(np) == AMBIENT_EDF_ACTION_TYPE_DUMP_CIV ? 100 : -1;
}

// The vehicle of the EDF squad the civilian's orders point at.
static vehicle* get_dump_vehicle(const gfm_orders* orders)
{
    object* target = orders != nullptr ? orders->get_target_object() : nullptr;
    return target != nullptr && target->get_obj_type() == OT_SQUAD ? World.object_from_handle<vehicle>(static_cast<object_squad*>(target)->squad_vehicle) : nullptr;
}

// Teleports the civilian into the first passenger seat.
static void enter_dump_vehicle(npc* np, vehicle* vp)
{
    vi_enter_data data;
    data.specified_seat = VSI_PASSENGER_1;
    data.seat_selection_method = VI_SEAT_SELECTION_METHOD_BEST;
    data.destination_seat = VSI_INVALID;
    data.flags.teleport = 1;
    vehicle_interaction_request_enter(np, vp, &data);
}

// 0x00611FE0 (:535). Waits up to 2 s for the squad's orders.
ai_results amb_edf_civ_be_dumped_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action == this)
    {
        ai->action_timer.invalidate();
        return AI_RESULT_SUCCESS;
    }
    if (amb_edf_action_participant(np))
    {
        const object_squad* squad = ai->get_squad();
        vehicle* vp = squad != nullptr ? get_dump_vehicle(squad->get_squad_orders()) : nullptr;
        if (vp != nullptr)
        {
            enter_dump_vehicle(np, vp);
            np->hflags.disallow_vehicle_exit = 1;
            ai->action_timer.invalidate();
            ai->action_state = 0;
            // Picked in squared metres: 30 to 60 m from the player.
            ai->desired_vehicle_dist_to_goal_pos = fl_rand_range(900.0f, 3600.0f);
            ai->planner.lock_action(true);
            return AI_RESULT_SUCCESS;
        }
    }
    if (!ai->action_timer.valid())
    {
        ai->action_timer.set(2000);
        return AI_RESULT_IN_PROGRESS;
    }
    return ai->action_timer.elapsed() ? AI_RESULT_FAILURE : AI_RESULT_IN_PROGRESS;
}

// 0x00612190 (:614). Thrown out near the player, ragdolled, and killed a second later.
ai_results amb_edf_civ_be_dumped_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const player* local = player_get_local_player();
    if (local == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (human_in_vehicle(np) != nullptr)
    {
        ai->action_state = 1;
    }
    if (ai->action_state == 0 && np->cur_state != HS_VEHICLE_EXITING && human_in_vehicle(np) == nullptr)
    {
        vehicle* vp = get_dump_vehicle(ai->get_orders());
        if (vp != nullptr)
        {
            enter_dump_vehicle(np, vp);
        }
    }
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    float speed;
    vehicle_get_speed(vp, speed);
    const float dist_sq = vec_xz_dist_squared(local->pos, np->pos);
    if (dist_sq < 900.0f || (dist_sq < ai->desired_vehicle_dist_to_goal_pos && !np->is_offscreen()) || vehicle_passenger_in_seat(vp, VSI_DRIVER) == nullptr)
    {
        np->npc_flags.use_corpse_vehicle_exit = 1;
        np->hflags.disallow_vehicle_exit = 0;
    }
    if (np->npc_flags.use_corpse_vehicle_exit && np->cur_state != HS_VEHICLE_EXITING && speed < 0.1f)
    {
        vi_exit_data data;
        vehicle_interaction_request_exit(np, &data);
        np->hflags.invulnerable = 1;
    }
    if (np->cur_state != HS_VEHICLE_EXITING)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (!ai->action_timer.valid())
    {
        ai->action_timer.set(TIME_TO_KILL_MS);
        human_ragdoll_on_impact(np, 10.0f, 0.1f, false);
    }
    else if (ai->action_timer.elapsed())
    {
        np->hflags.invulnerable = 0;
        human_damage(np, nullptr, 100000.0f, DBT_NONE, nullptr, nullptr, true, false);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E0CB0 (:698)
ai_results amb_edf_civ_be_dumped_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this && (np->cur_state == HS_VEHICLE_EXITING || -1 < np->ragdoll_idx))
    {
        np->hflags.invulnerable = 0;
        human_damage(np, nullptr, 100000.0f, DBT_NONE, nullptr, nullptr, true, false);
    }
    return AI_RESULT_SUCCESS;
}

amb_edf_drive_by_action amb_edf_drive_by_action::the_instance; // 0x01D9F7A0

// 0x006114B0 (.h:13)
amb_edf_drive_by_action::amb_edf_drive_by_action()
    : ai_action("amb edf drive by action", ai_plan_conditions_from_bits({9, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({100}))
{
    flags.has_enter = 1;
}

ai_action* amb_edf_drive_by_action::get_instance()
{
    return &the_instance;
}

// 0x005E0B40
amb_edf_drive_by_action::~amb_edf_drive_by_action()
{
}

// 0x006116F0 (.h:13)
int amb_edf_drive_by_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

amb_edf_dump_civ_action amb_edf_dump_civ_action::the_instance; // 0x01D75A08

// 0x00611730 (.h:14)
amb_edf_dump_civ_action::amb_edf_dump_civ_action()
    : ai_action("amb edf dump civilian", ai_plan_conditions_from_bits({9, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({100}))
{
    flags.has_enter = 1;
}

ai_action* amb_edf_dump_civ_action::get_instance()
{
    return &the_instance;
}

// 0x005E0B50
amb_edf_dump_civ_action::~amb_edf_dump_civ_action()
{
}

// 0x00611970 (.h:14)
int amb_edf_dump_civ_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

amb_edf_civ_be_dumped_action amb_edf_civ_be_dumped_action::the_instance; // 0x01DDB6BC

// 0x006119B0 (.h:15)
amb_edf_civ_be_dumped_action::amb_edf_civ_be_dumped_action()
    : ai_action("amb edf civ be dumped", ai_plan_conditions_from_bits({9, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({100}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* amb_edf_civ_be_dumped_action::get_instance()
{
    return &the_instance;
}

// 0x005E0B60
amb_edf_civ_be_dumped_action::~amb_edf_civ_be_dumped_action()
{
}

// 0x00611BF0 (.h:15)
int amb_edf_civ_be_dumped_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

amb_edf_fly_by_action amb_edf_fly_by_action::the_instance; // 0x01D753A0

// 0x00611C30 (.h:16)
amb_edf_fly_by_action::amb_edf_fly_by_action()
    : ai_production_rule("amb edf fly by action", ai_plan_conditions_from_bits({50}), ai_plan_conditions_from_bits({100}))
{
}

ai_action* amb_edf_fly_by_action::get_instance()
{
    return &the_instance;
}

// 0x005E0B70
amb_edf_fly_by_action::~amb_edf_fly_by_action()
{
}

// 0x00611CB0 (.h:16)
int amb_edf_fly_by_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

amb_edf_harass_player_action amb_edf_harass_player_action::the_instance; // 0x01DC9C74

// 0x00611CE0 (.h:17)
amb_edf_harass_player_action::amb_edf_harass_player_action()
    : ai_action("amb edf harass player", ai_plan_conditions_from_bits({9, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({100}))
{
    flags.has_enter = 1;
}

ai_action* amb_edf_harass_player_action::get_instance()
{
    return &the_instance;
}

// 0x005E0B80
amb_edf_harass_player_action::~amb_edf_harass_player_action()
{
}

// 0x00611F20 (.h:17)
int amb_edf_harass_player_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

