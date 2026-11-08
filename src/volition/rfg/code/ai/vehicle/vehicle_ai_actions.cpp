// Original: volition/rfg/code/ai/vehicle/vehicle_ai_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/vehicle/vehicle_ai_actions.h"

#include "volition/rfg/code/ai/ai_building_logic.h"
#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/ambient_action_manager.h"
#include "volition/rfg/code/ai/civilian/pedestrian_actions.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/ai/patrol_actions.h"
#include "volition/rfg/code/ai/percept.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/self_preservation_goals.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_courier_chase.h"
#include "volition/rfg/code/gameplay/activities/vrail_system.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_bounding_box.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/pathfinding/nav_cell_detour.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_audio.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_flyer.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle_ai/air_traffic_control.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/rfg/code/vehicle_ai/vehicle_navmesh_common.h"
#include "volition/rfg/code/vehicle_ai/vehicle_navmesh_path_create.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>

// Position 33 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG33")

// The chase tweaks. The debug console command that sets them (unused_dcf_ai_continuous_attack_tweak, 0x005F4380) is not ported.
static float CHASE_TARGET_SPEED_BOOST = 30.0f; // 0x016415E0, .data: drive this much faster than the target vehicle
static float FLYING_CHASE_TARGET_SPEED_BOOST = 10.0f; // 0x016415E4, .data: fly this much faster than the target vehicle
static int CHASE_TARGET_BACKOFF_TIMER = 200; // 0x016415E8, .data: frames of backing off
static float CHASE_TARGET_BACKOFF_DISTANCE = 15.0f; // 0x016415EC, .data: back off when this close to the target
static float CHASE_TARGET_BACKOFF_SPEED = 10.0f; // 0x016415F0, .data: back off if the target is slower than this
static float CHASE_TARGET_BACKOFF_AMOUNT = 50.0f; // 0x016415F4, .data: back off to a point this far away
static float CHASE_TARGET_BACKOFF_RADIUS = 25.0f; // 0x016415F8, .data: turn back to the target at this distance

// Zero until tweak_table.xtbl loads.
int Ai_reuse_turret_after_ineffective_time; // 0x01D67724
static tweak_table_item Tweak_table_item_Ai_reuse_turret_after_ineffective_time("Ai_reuse_turret_after_ineffective_time", &Ai_reuse_turret_after_ineffective_time);

timestamp Yell_vehicle_timeout; // 0x016424E4, .data -1, also read by yell_at_vehicle_action

// 0x005F46A0 (:304)
static bool should_play_chase_line()
{
    return !mission_is_active() || mission_get_mode() != MISSION_MODE_HIGHWAY_TO_HELL;
}

// 0x005F46C0 (:314)
static bool allow_convoy_exit(const npc* np, const vehicle* vp)
{
    if (np == nullptr || vp == nullptr)
    {
        return true;
    }
    const vehicle_seat_index seat = vehicle_passenger_get_seat(vp, np);
    const turret* tp = vehicle_get_seat_turret(vp, seat);
    if (tp != nullptr && tp->is_functional(false))
    {
        return false;
    }
    if (seat == VSI_DRIVER)
    {
        const human* gunner = vehicle_get_human_in_gunner_seat(vp);
        if (gunner != nullptr)
        {
            return gunner->is_killed();
        }
    }
    return true;
}

// 0x005F4720 (:351): 10 m over what is under a 20 m box around the npc, else 30 m over the terrain; never under 30 m above the player.
static float ambient_flyer_height(const npc* np)
{
    const player* pl = player_get_local_player();
    float ground;
    if (!havok_terrain_height(ground, np->pos.x, np->pos.z, false, nullptr))
    {
        return pl->pos.y + 30.0f;
    }
    float box_height = 0.0f;
    const vector bmin(-10.0f, -10.0f, -10.0f);
    const vector bmax(10.0f, 10.0f, 10.0f);
    const float height = havok_terrain_height(box_height, bmin, bmax, np->pos, np->orient, false) ? box_height + 10.0f : ground + 30.0f;
    const float above_player = pl->pos.y + 30.0f;
    return height - above_player < 0.0f ? above_player : height;
}

// 0x005F4860 (:377): where a target moving at target_velocity meets something leaving starting_pos at desired_speed.
static bool generate_intercept_point(vector starting_pos, vector target_pos, vector target_velocity, float desired_speed, vector& intercept_point)
{
    intercept_point = ZERO_VECTOR;
    const vector to_target = target_pos - starting_pos;
    const float b = target_velocity.y * to_target.y + to_target.x * target_velocity.x + target_velocity.z * to_target.z;
    const float a = target_velocity.x * target_velocity.x + target_velocity.y * target_velocity.y + target_velocity.z * target_velocity.z - desired_speed * desired_speed;
    const float discriminant = b * b - (to_target.x * to_target.x + to_target.y * to_target.y + to_target.z * to_target.z) * a * 4.0f;
    if (0.0f < discriminant)
    {
        const float root = sqrtf(discriminant);
        const float t0 = (-b - root) / (a * 2.0f);
        const float t1 = (root - b) / (a * 2.0f);
        if (0.0f < t0)
        {
            intercept_point = target_pos + target_velocity * t0;
            return true;
        }
        if (0.0f < t1)
        {
            intercept_point = target_pos + target_velocity * t1;
            return true;
        }
    }
    return false;
}

// 0x006463F0 (:425)
int goto_vehicle_action::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (human_in_vehicle_interaction(np, nullptr))
    {
        return 0;
    }
    if (goal_setup_data.goal == man_flyer_goal::get_instance())
    {
        desired_conditions |= ai_plan_conditions_get_for_loaded_firearm(np);
    }
    return 100;
}

// 0x005F4B70 (:442)
void goto_vehicle_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x0068B850 (:448)
ai_results goto_vehicle_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (ai->desired_vehicle == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = static_cast<vehicle*>(World.object_from_handle(ai->desired_vehicle));
    if (vp == nullptr || vp->get_obj_type() != OT_VEHICLE)
    {
        return AI_RESULT_FAILURE;
    }
    if (!ai->can_teleport_into_vehicle(vp, ai->desired_vehicle_seat_type) && ai->desired_vehicle_seat_type != AI_VEHICLE_SEAT_TYPE_DRIVER)
    {
        if (!ai_vehicle_is_usable_and_available(vp, np, ai->desired_vehicle_seat_type))
        {
            return AI_RESULT_FAILURE;
        }
        // Tell an npc driver to think again, it may wait for this passenger.
        human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
        if (driver != nullptr && driver->get_obj_type() == OT_HUMAN && driver->get_sub_type() == OT_SUB_HUMAN_NPC && static_cast<human*>(np) != driver)
        {
            ai_system* driver_ai = static_cast<npc*>(driver)->get_ai();
            if (driver_ai->planner.get_current_goal() != wait_for_passengers_goal::get_instance())
            {
                driver_ai->planner.request_replan();
            }
        }
    }
    if (last_action != this)
    {
        ai->action_timer.invalidate();
        if (ai_vehicle_enter(np, vp, ai->desired_vehicle_seat_type, true) != AI_RESULT_SUCCESS)
        {
            return AI_RESULT_FAILURE;
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x006AC530 (:499)
ai_results goto_vehicle_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (ai->desired_vehicle == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = static_cast<vehicle*>(World.object_from_handle(ai->desired_vehicle));
    if (vp == nullptr || vp->get_obj_type() != OT_VEHICLE)
    {
        return AI_RESULT_FAILURE;
    }
    const ai_vehicle_seat_types seat_type = ai->desired_vehicle_seat_type;
    if (!ai_vehicle_is_usable(vp, np, seat_type) || !ai_vehicle_has_available_seat(vp, np, seat_type))
    {
        return AI_RESULT_FAILURE;
    }
    bool teleport = ai->can_teleport_into_vehicle(vp, ai->desired_vehicle_seat_type);
    if (!teleport)
    {
        // Close enough to get in: within 4 m of the box in xz and 1.2 m in y.
        vector local_pos = np->pos;
        vector bmin;
        vector bmax;
        vp->get_local_bbox(bmin, bmax);
        vec_change_to_local_coords(local_pos, vp->pos, vp->orient);
        const float xz_dist_sqrd = bbox_find_xz_distance_squared(bmin, bmax, local_pos);
        const float y_above = local_pos.y - bmax.y;
        const float y_below = bmin.y - local_pos.y;
        const float y_dist = y_above - y_below < 0.0f ? y_below : y_above;
        if (xz_dist_sqrd < 16.0f && y_dist < 1.2f)
        {
            return AI_RESULT_SUCCESS;
        }
        ai_combat_fire_at_target_if_available(np, false, false);
        vector dest = vp->pos;
        unsigned int bridge_navpoint;
        if (is_vehicle_on_bridge(vp, bridge_navpoint))
        {
            if (bridge_navpoint != 0xFFFFFFFF)
            {
                const object* navp = World.object_from_handle(bridge_navpoint);
                if (navp != nullptr && navp->get_obj_type() == OT_NAVPOINT)
                {
                    dest.y = navp->pos.y;
                }
            }
        }
        else
        {
            havok_terrain_height(dest.y, dest.x, dest.z, false, nullptr);
        }
        if (ai->is_pathing_to(dest, 0.25f, 1.0f))
        {
            return AI_RESULT_IN_PROGRESS;
        }
        nav_cell_detour_remove_convex_hull_for_request(vp->m_nav_cell_detour_request_handle);
        const ai_goal* goal = ai->planner.get_current_goal();
        human_movement_states move_state = HMS_RUN;
        if (goal == flee_to_safety_goal::get_instance() || goal == flee_enemies_goal::get_instance() || npc_is_defending_player(np))
        {
            move_state = HMS_SPRINT;
        }
        const ai_results result = ai_pf_to_result(ai->pathfind_to(dest, move_state, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false));
        if (result != AI_RESULT_FAILURE)
        {
            if (result == AI_RESULT_SUCCESS)
            {
                ai->action_timer.invalidate();
            }
            return AI_RESULT_IN_PROGRESS;
        }
        if (!ai->action_timer.valid())
        {
            ai->action_timer.set(4000);
            return AI_RESULT_IN_PROGRESS;
        }
        if (!ai->action_timer.elapsed())
        {
            return AI_RESULT_IN_PROGRESS;
        }
        int avoid_time = 10000;
        if (vrail_is_driver(np))
        {
            // No path for a rail driver: when nobody can see it, 20 from the player (squared), put it in.
            const float player_dist_sqrd = vec_dist_squared(player_get_local_player()->pos, np->pos);
            if (np->hflags.was_rendered == 0 && 20.0f < player_dist_sqrd)
            {
                teleport = true;
            }
            avoid_time = 1000;
        }
        if (!teleport)
        {
            ai->planner.avoid_action(this, avoid_time);
            return AI_RESULT_FAILURE;
        }
    }
    const ai_results result = ai_vehicle_enter_teleport(np, vp, ai->desired_vehicle_seat_type);
    if (result != AI_RESULT_SUCCESS)
    {
        return result;
    }
    ai->planner.request_replan();
    return AI_RESULT_SUCCESS;
}

// 0x00646480 (:613)
ai_results goto_vehicle_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action == this || next_action == enter_vehicle_action::get_instance() || np->get_ai()->desired_vehicle == 0xFFFFFFFF)
    {
        return AI_RESULT_SUCCESS;
    }
    vehicle* vp = static_cast<vehicle*>(World.object_from_handle(np->get_ai()->desired_vehicle));
    if (vp == nullptr || vp->get_obj_type() != OT_VEHICLE || human_in_vehicle(np, vp))
    {
        return AI_RESULT_SUCCESS;
    }
    if (human_in_vehicle_interaction(np, vp))
    {
        vi_exit_data exit_data;
        exit_data.flags.teleport = 1;
        vehicle_interaction_request_exit(np, &exit_data);
        return AI_RESULT_SUCCESS;
    }
    vehicle_interaction_cancel_reservation(np, vp);
    return AI_RESULT_SUCCESS;
}

// 0x0068B940 (:650)
ai_results enter_vehicle_common(npc* np)
{
    ai_system* ai = np->get_ai();
    if (ai->desired_vehicle == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = static_cast<vehicle*>(World.object_from_handle(ai->desired_vehicle));
    if (vp == nullptr || vp->get_obj_type() != OT_VEHICLE)
    {
        return AI_RESULT_FAILURE;
    }
    if (human_in_vehicle(np, vp))
    {
        return AI_RESULT_SUCCESS;
    }
    if (!ai_vehicle_is_usable(vp, np, ai->desired_vehicle_seat_type) || !ai_vehicle_has_available_seat(vp, np, ai->desired_vehicle_seat_type))
    {
        return AI_RESULT_FAILURE;
    }
    if (human_in_vehicle_transition(np, vp, VI_INTERACTION_ENTER) && ai_vehicle_get_entering_seat_type(np, vp, ai->desired_vehicle_seat_type) != ai->desired_vehicle_seat_type)
    {
        return AI_RESULT_FAILURE;
    }
    return ai_vehicle_enter(np, vp, ai->desired_vehicle_seat_type, false);
}

// 0x005F4B80 (:682): 0 for an active courier, 100 otherwise.
int enter_vehicle_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if (np->h_info->flags.guerrilla_tech && !mission_is_active())
    {
        return -1;
    }
    if (np->hflags.disallow_vehicle_entry != 0)
    {
        return -1;
    }
    return activity_cc_is_active_courier(np) ? 0 : 100;
}

// 0x0068BA00 (:705)
ai_results enter_vehicle_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        ai_system* ai = np->get_ai();
        if (ai->desired_vehicle == 0xFFFFFFFF)
        {
            return AI_RESULT_FAILURE;
        }
        vehicle* vp = static_cast<vehicle*>(World.object_from_handle(ai->desired_vehicle));
        if (vp == nullptr || vp->get_obj_type() != OT_VEHICLE)
        {
            return AI_RESULT_FAILURE;
        }
        ai->action_substate = 0;
        ai->action_timer.set(15000);
        ai_vehicle_maybe_adopt_vehicle(np, vp);
        const unsigned int reserved = np->reserved_vehicle_handle;
        if (reserved != 0xFFFFFFFF && reserved != ai->desired_vehicle)
        {
            object* reserved_objp = World.object_from_handle(reserved);
            vehicle_interaction_cancel_reservation(np, reserved_objp != nullptr && reserved_objp->get_obj_type() == OT_VEHICLE ? static_cast<vehicle*>(reserved_objp) : nullptr);
        }
        const human* target = human_combat_get_target(np);
        if (target != nullptr)
        {
            if (ai->perception.enemy_in_los(target->get_handle()))
            {
                ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
            }
            else
            {
                const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, target);
                if (belief != nullptr)
                {
                    ai->perception.pay_attention(belief->handle, belief->get_type(), ATTENTION_LOOK_AT_BELIEF, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
                }
            }
        }
    }
    return enter_vehicle_common(np) != AI_RESULT_FAILURE ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x0068BB10 (:759)
ai_results enter_vehicle_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const ai_results result = enter_vehicle_common(np);
    if (result == AI_RESULT_FAILURE || ai->action_timer.elapsed())
    {
        return AI_RESULT_FAILURE;
    }
    if (np->hflags.activity_ha_hostage != 0 && ai->action_substate == 0 && np->cur_state == HS_VEHICLE_ENTERING)
    {
        ai->action_substate = 1;
        np->play_voice_line(VOICE_LINE_HA_HOSTAGE_VEH_CHAT, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    }
    return result;
}

// 0x00646510 (:785)
ai_results enter_vehicle_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        if (human_in_vehicle_transition(np, nullptr, VI_INTERACTION_ENTER))
        {
            vi_exit_data exit_data;
            exit_data.flags.teleport = 0;
            vehicle_interaction_request_exit(np, &exit_data);
            return AI_RESULT_SUCCESS;
        }
        if (ai->desired_vehicle != 0xFFFFFFFF)
        {
            object* objp = World.object_from_handle(ai->desired_vehicle);
            if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
            {
                vehicle_interaction_cancel_reservation(np, static_cast<vehicle*>(objp));
            }
        }
        return AI_RESULT_SUCCESS;
    }
    if (ai->desired_vehicle_seat_type != ai_vehicle_get_seat_type(np, vp, ai->desired_vehicle_seat_type))
    {
        return AI_RESULT_FAILURE;
    }
    if (human_is_driving(np))
    {
        ai->planner.request_replan();
        return AI_RESULT_SUCCESS;
    }
    // A driver waiting for its passengers can go on.
    human* driver = vehicle_passenger_in_seat(np->vehicle_handle, VSI_DRIVER);
    if (driver != nullptr && driver->get_obj_type() == OT_HUMAN && driver->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        ai_planner& planner = static_cast<npc*>(driver)->get_ai()->planner;
        const ai_action* act = planner.get_current_action();
        if (act == wait_for_passengers_action::get_instance())
        {
            planner.request_replan();
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x0066FAB0 (:845)
int get_out_of_vehicle_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    const ai_system* ai = np->get_ai();
    if (!human_in_vehicle_interaction(np, nullptr) || goal_setup_data.goal == flush_out_target_goal::get_instance())
    {
        return -1;
    }
    if (ai->get_squad_handle() != 0xFFFFFFFF)
    {
        object_squad* squad = static_cast<object_squad*>(World.object_from_handle(ai->get_squad_handle()));
        if (squad != nullptr && squad->get_obj_type() == OT_SQUAD)
        {
            const gfm_orders* orders = squad->get_squad_orders();
            if (orders != nullptr && orders->get_orders_type() == GFM_ORDERS_TYPE_FODDER)
            {
                return -1;
            }
        }
    }
    if (np->vehicle_handle == 0xFFFFFFFF)
    {
        return -1;
    }
    vehicle* vp = static_cast<vehicle*>(World.object_from_handle(np->vehicle_handle));
    if (vp == nullptr || vp->get_obj_type() != OT_VEHICLE)
    {
        return -1;
    }
    if ((goal_setup_data.goal == move_with_human_goal::get_instance() || goal_setup_data.goal == flee_from_vehicle_goal::get_instance()) && vehicle_human_is_riding_with_non_exiting_player(np))
    {
        return -1;
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
    {
        return -1;
    }
    if (vp->is_killed())
    {
        return 100;
    }
    if (np->hflags.disallow_vehicle_exit != 0)
    {
        return -1;
    }
    if (activity_pursuit_npc(np))
    {
        // In a pursuit the driver and the gunner stay in while both are there.
        const human* driver = vehicle_passenger_in_entering_or_reserving_seat(vp, VSI_DRIVER);
        const turret* tp = ai_vehicle_get_first_manned_turret(vp);
        const human* gunner = tp == nullptr ? nullptr : World.object_from_handle<human>(tp->current_operator);
        if (driver != nullptr && gunner != nullptr && (driver == np || gunner == np))
        {
            return -1;
        }
    }
    if (np->hflags.limited_vehicle_exit != 0 && np->hflags.convoy_vehicle_exit != 0 && allow_convoy_exit(np, vp))
    {
        return 100;
    }
    if (np->hflags.driverless_exit_only != 0)
    {
        const human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
        if (driver == nullptr || driver->is_killed() || driver->cur_state == HS_VEHICLE_EXITING)
        {
            return 100;
        }
    }
    bool stay_with_ambient_driver = false;
    if (np->hflags.limited_vehicle_exit != 0 || np->hflags.driverless_exit_only != 0)
    {
        const ai_action* act = ai->planner.get_current_action();
        if (act != this)
        {
            if (np->npc_flags.ambient_spawn == 0 && np->npc_flags.ambient_vehicle_spawn == 0)
            {
                if (!is_player_my_target_or_is_my_target_with_player_in_stuck_vehicle(np))
                {
                    return -1;
                }
            }
            else
            {
                // Original quirk: no null check of the cast before the flags, human_is_dead answers for a null.
                const npc* driver = World.object_cast<npc>(vehicle_passenger_in_or_entering_seat(vp, VSI_DRIVER));
                if (!human_is_dead(driver) && (driver->npc_flags.ambient_spawn != 0 || driver->npc_flags.ambient_vehicle_spawn != 0) &&
                    goal_setup_data.goal != react_attached_projectile_goal::get_instance())
                {
                    stay_with_ambient_driver = true;
                }
            }
        }
    }
    if (vp->m_flags.m_on_fire != 0 || vehicle_is_flipped(vp))
    {
        return 100;
    }
    if (stay_with_ambient_driver)
    {
        return -1;
    }
    if (vp->m_flags.m_scripted_bail == 0 && vp->m_flags.m_scripted_abandon == 0)
    {
        // 20 m/s: not out of a fast vehicle, unless in the player's squad. In the air only at up to 1 m/s and when not supported.
        float speed;
        vehicle_get_speed(vp, speed);
        if (ai->is_in_player_squad(true) == nullptr && 20.0f <= speed)
        {
            return -1;
        }
        if (vp->m_flags.m_wheels_on_ground == 0)
        {
            if (0.0f < vp->orient.uvec.y && vp->get_supporting_rigid_body() == nullptr)
            {
                return -1;
            }
            if (1.0f < speed)
            {
                return -1;
            }
        }
    }
    return 100;
}

// 0x0066FD90 (:996)
ai_results get_out_of_vehicle_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (!human_in_vehicle_interaction(np, nullptr))
    {
        return AI_RESULT_FAILURE;
    }
    if (this == last_action)
    {
        return AI_RESULT_SUCCESS;
    }
    if (np->vehicle_handle == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = static_cast<vehicle*>(World.object_from_handle(np->vehicle_handle));
    if (vp == nullptr || vp->get_obj_type() != OT_VEHICLE)
    {
        return AI_RESULT_FAILURE;
    }
    if (human_is_driving(np))
    {
        // 0x200: the reaction a driver's sacrifice line is rate limited by.
        if (ai->should_react(0x200))
        {
            const ai_morale_levels morale = ai->get_district_morale_level();
            voice_lines line = VOICE_LINE_VEHICLE_SACRIFICE_RELUCTANT;
            if (1 < (int)morale || (0 < (int)morale && int_rand_chance(50)))
            {
                line = VOICE_LINE_VEHICLE_SACRIFICE;
            }
            np->play_voice_line(line, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        }
    }
    else
    {
        human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
        if (driver != nullptr && driver->get_obj_type() == OT_HUMAN && driver->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            static_cast<npc*>(driver)->get_ai()->planner.request_replan();
        }
    }
    const human_voice_persona* voice = np->voice;
    if (voice != nullptr)
    {
        // 10 s of no small talk after getting out.
        voice->set_line_cooldown(VOICE_LINE_AMBIENT_PLAYER_DONT_FOLLOW_ME, 10000);
        voice->set_line_cooldown(VOICE_LINE_AMBIENT_TALK_TO_SELF_LOW_MORALE, 10000);
        voice->set_line_cooldown(VOICE_LINE_AMBIENT_TALK_TO_SELF_MEDIUM_MORALE, 10000);
        voice->set_line_cooldown(VOICE_LINE_AMBIENT_TALK_TO_SELF_HIGH_MORALE, 10000);
        voice->set_line_cooldown(VOICE_LINE_AMBIENT_PLAYER_GREET_LOW_MORALE, 10000);
        voice->set_line_cooldown(VOICE_LINE_AMBIENT_PLAYER_GREET_MEDIUM_MORALE, 10000);
        voice->set_line_cooldown(VOICE_LINE_AMBIENT_PLAYER_GREET_HIGH_MORALE, 10000);
        voice->set_line_cooldown(VOICE_LINE_AMBIENT_PLAYER_GREET_LOW_MORALE_FAMOUS, 10000);
        voice->set_line_cooldown(VOICE_LINE_AMBIENT_PLAYER_GREET_MEDIUM_MORALE_FAMOUS, 10000);
        voice->set_line_cooldown(VOICE_LINE_AMBIENT_PLAYER_GREET_HIGH_MORALE_FAMOUS, 10000);
    }
    ai->action_substate = 0;
    if (ai->is_in_player_squad(true) == nullptr && ai->planner.get_current_goal() != go_into_safehouse_goal::get_instance())
    {
        ai->action_timer.invalidate();
        return AI_RESULT_SUCCESS;
    }
    ai->planner.lock_action(true);
    ai->action_timer.set(ai->planner.get_current_goal() == go_into_safehouse_goal::get_instance() ? 10000 : 3000);
    ai->action_substate = 1;
    return AI_RESULT_SUCCESS;
}

// 0x00646650 (:1071)
ai_results get_out_of_vehicle_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (!human_in_vehicle_interaction(np, nullptr))
    {
        return AI_RESULT_SUCCESS;
    }
    if (np->vehicle_handle == 0xFFFFFFFF)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    vehicle* vp = static_cast<vehicle*>(World.object_from_handle(np->vehicle_handle));
    if (vp == nullptr || vp->get_obj_type() != OT_VEHICLE)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (np->cur_state == HS_VEHICLE_DRIVING)
    {
        vehicle_set_controls_when_abandoned(vp);
        vehicle_ai* vai = vehicle_get_vehicle_ai(np->vehicle_handle);
        if (vai != nullptr)
        {
            vehicle_ai_free_all_paths(vai);
        }
    }
    if (!ai_vehicle_should_get_out_of_moving_vehicle(np, vp))
    {
        // 2.25: wait until the vehicle is under 1.5 m/s.
        vector velocity;
        vehicle_get_velocity(vp, velocity);
        if (2.25f < velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z)
        {
            return AI_RESULT_IN_PROGRESS;
        }
    }
    if (ai->planner.get_current_goal() == go_into_safehouse_goal::get_instance())
    {
        player* pl = player_get_local_player();
        vehicle* player_vehicle = human_in_vehicle(pl);
        if (player_vehicle != nullptr && player_vehicle->get_obj_type() == OT_VEHICLE && player_vehicle->get_sub_type() == OT_SUB_VEHICLE_AUTO)
        {
            automobile* ap = static_cast<automobile*>(player_vehicle);
            automobile_clear_accelerator(ap);
            automobile_set_brake_pedal(ap);
            pl->pflags.is_waiting_for_hostage_vehicle_exit = 1;
        }
    }
    vi_exit_data exit_data;
    exit_data.flags.teleport = 0;
    if (ai->action_substate == 1)
    {
        if (ai->action_timer.elapsed())
        {
            exit_data.flags.teleport = 1;
        }
    }
    else if (np->hflags.stuck_in_vehicle != 0 && !ai->action_timer.valid())
    {
        ai->action_timer.set(500);
    }
    if (ai->action_substate == 1 || !ai->action_timer.valid() || ai->action_timer.elapsed())
    {
        vehicle_interaction_request_exit(np, &exit_data);
        if (ai->action_substate == 0)
        {
            ai->action_timer.invalidate();
        }
    }
    ai->planner.lock_action(true);
    return AI_RESULT_IN_PROGRESS;
}

// 0x005F4BD0 (:1178)
ai_results get_out_of_vehicle_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        if (human_in_vehicle(np) == nullptr)
        {
            vehicle_interaction_cancel(np);
        }
        np->npc_flags.force_out_of_vehicle = 0;
    }
    return AI_RESULT_SUCCESS;
}

// 0x00646830 (:1197)
int goto_with_flyer_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if (np->vehicle_handle != 0xFFFFFFFF)
    {
        const object* objp = World.object_from_handle(np->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && objp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
        {
            return 100;
        }
    }
    return -1;
}

// 0x00646870 (:1210): 20 m/s and a height over the player for an ambient edf flyer, otherwise 30 m/s and the height air traffic control allows.
ai_results goto_with_flyer_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (np->vehicle_handle == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = static_cast<vehicle*>(World.object_from_handle(np->vehicle_handle));
    if (vp == nullptr || vp->get_obj_type() != OT_VEHICLE || vp->get_sub_type() != OT_SUB_VEHICLE_FLYER)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle_ai* vai = &vp->vai;
    vehicle_ai_set_state(vai, VS_AI_PATHFIND);
    float max_speed = 30.0f;
    if (np->npc_flags.ambient_edf_flyer != 0)
    {
        static_cast<flyer*>(vp)->flyer_flags.stay_at_constant_height = 1;
        max_speed = 20.0f;
        if (last_action != this)
        {
            vai->desired_flying_offset = ambient_flyer_height(np);
        }
    }
    else
    {
        vai->desired_flying_offset = Air_traffic_controller.request_altitude_clearance(np->vehicle_handle, vai->original_offset);
    }
    // 0.01: no speed asked for.
    if (ai->desired_vehicle_speed <= 0.01f || 0.0f <= ai->desired_vehicle_speed - max_speed)
    {
        ai->desired_vehicle_speed = max_speed;
    }
    const float clearance = Air_traffic_controller.request_speed_clearance(np->vehicle_handle);
    float speed = -clearance;
    if (0.0f <= ai->desired_vehicle_speed - -clearance)
    {
        speed = ai->desired_vehicle_speed;
    }
    if (0.0f <= speed - clearance)
    {
        speed = clearance;
    }
    ai->desired_vehicle_speed = speed;
    ai_vehicle_override_speed(vp, speed);
    ai_vehicle_ensure_seat_turret_manned(np, vp, nullptr);
    if (ai->get_squad_handle() != 0xFFFFFFFF)
    {
        object_squad* squad = static_cast<object_squad*>(World.object_from_handle(ai->get_squad_handle()));
        if (squad != nullptr && squad->get_obj_type() == OT_SQUAD)
        {
            const gfm_orders* orders = squad->get_squad_orders();
            if (orders != nullptr)
            {
                ai->desired_destination = orders->get_orders_pos();
                return AI_RESULT_SUCCESS;
            }
        }
    }
    return AI_RESULT_FAILURE;
}

// 0x006AC8D0 (:1263): flies to desired_destination, 15 m is there, smoothing on.
ai_results goto_with_flyer_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (np->vehicle_handle == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = static_cast<vehicle*>(World.object_from_handle(np->vehicle_handle));
    if (vp == nullptr || vp->get_obj_type() != OT_VEHICLE || vp->get_sub_type() != OT_SUB_VEHICLE_FLYER)
    {
        return AI_RESULT_FAILURE;
    }
    ai_vehicle_use_turret_if_available(np, vp, false, false, nullptr, ZERO_VECTOR);
    return ai_vehicle_travel_to_a_point_do_frame(np, vp, ai->desired_destination, 0xFFFFFFFFFFFFFFFFull, false, 15.0f, false, false, false, true);
}

// 0x005F4C10 (:1308): where to drive for orig_dest_pos: a gps box's rally point; else the nearest of a usable nav cell and a road within 60 m.
static bool adjust_goto_with_vehicle_pos(vector& adj_dest_pos, bool& start_road, bool& end_road, const npc* np, const vehicle* vp, const vector& orig_dest_pos)
{
    start_road = false;
    end_road = false;
    adj_dest_pos = orig_dest_pos;
    const bool special_roads = np->npc_flags.can_use_special_roads != 0;
    start_road = object_road_get_closest_road(vp->pos, special_roads, 60.0f, false, false) != nullptr;
    const object_bounding_box* gps = object_bbox_find_gps(orig_dest_pos);
    if (gps != nullptr && object_bbox_get_rally_point(gps, adj_dest_pos))
    {
        end_road = object_road_get_closest_road(adj_dest_pos, special_roads, 60.0f, false, false) != nullptr;
        return true;
    }
    vector road_point;
    float road_u;
    bool road_in_range = false;
    float road_dist_sqrd = 0.0f;
    if (object_road_get_closest_road(orig_dest_pos, special_roads, road_point, road_u, 60.0f, false, nullptr, nullptr, false) != nullptr)
    {
        road_dist_sqrd = vec_dist_squared(orig_dest_pos, road_point);
        road_in_range = road_dist_sqrd <= 3600.0f;
    }
    const nav_cell* cell = bb_pf_get_nav_cell(orig_dest_pos, true, nullptr, false, NAV_CELL_TRAVERSAL_ALL, nullptr);
    if (cell != nullptr)
    {
        vector cell_pos = orig_dest_pos;
        bool have_cell_pos = true;
        if ((cell->flags & (NAV_CELL_FLAG_DISABLED | NAV_CELL_FLAG_LIGHT)) != 0)
        {
            have_cell_pos = nav_cell_find_first_valid_cell(cell_pos, cell, orig_dest_pos, cell, vp->pos, true, nullptr);
        }
        if (have_cell_pos)
        {
            // The road only when the destination is over twice as far from the vehicle as from the road.
            if (!road_in_range || vec_dist_squared(vp->pos, orig_dest_pos) < road_dist_sqrd * 4.0f)
            {
                adj_dest_pos = cell_pos;
                return true;
            }
            end_road = true;
            adj_dest_pos = road_point;
            return true;
        }
    }
    if (!road_in_range)
    {
        return false;
    }
    end_road = true;
    adj_dest_pos = road_point;
    return true;
}

// 0x0068BB90 (:1417)
int goto_with_vehicle_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    vehicle* vp = ai_vehicle_find_available_seat_type(np, AI_VEHICLE_SEAT_TYPE_DRIVER, nullptr);
    if (vp == nullptr)
    {
        return -1;
    }
    const bool other_vehicle = vp->get_handle() != np->vehicle_handle;
    if (other_vehicle || ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED) != AI_VEHICLE_SEAT_TYPE_DRIVER)
    {
        desired_conditions |= ai_plan_conditions_from_bits({48, 49});
    }
    vector local_pos = np->pos;
    vector bmin = ZERO_VECTOR;
    vector bmax = ZERO_VECTOR;
    vector dest_pos = ZERO_VECTOR;
    matrix dest_orient(IDENTITY_MATRIX);
    if (goal_setup_data.position.is_null())
    {
        const object* defend = goal_setup_data.defend_object;
        if (defend == nullptr)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            return -1;
        }
        dest_pos = defend->pos;
        dest_orient = defend->orient;
        defend->get_local_bbox(bmin, bmax);
        vec_change_to_local_coords(local_pos, dest_pos, dest_orient);
    }
    else
    {
        dest_pos = goal_setup_data.position;
    }
    // Walking to the vehicle must not be most of the way there. Original quirk: for a goal position the box is empty and local_pos stays world.
    float vehicle_dist = 0.0f;
    if (other_vehicle && 0.0f < goal_setup_data.min_drive_dist)
    {
        vehicle_dist = vec_dist(np->pos, vp->pos);
        if (sqrtf(bbox_find_distance_squared(bmin, bmax, local_pos)) - vehicle_dist < goal_setup_data.min_drive_dist)
        {
            return -1;
        }
    }
    bool start_road;
    bool end_road;
    if (!adjust_goto_with_vehicle_pos(my_data.pos, start_road, end_road, np, vp, dest_pos))
    {
        return -1;
    }
    if (other_vehicle && 0.0f < goal_setup_data.min_drive_dist)
    {
        local_pos = np->pos;
        vec_change_to_local_coords(local_pos, dest_pos, dest_orient);
        if (sqrtf(bbox_find_distance_squared(bmin, bmax, local_pos)) - vehicle_dist < goal_setup_data.min_drive_dist)
        {
            return -1;
        }
    }
    my_data.use_target = vp;
    my_data.flags.use_road = start_road || end_road;
    return 5000;
}

// 0x005F4E30 (:1494)
void goto_with_vehicle_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_vehicle = setup_data.use_target->get_handle();
    ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_DRIVER;
    ai->desired_destination = setup_data.pos;
    ai->flags.vehicle_use_road = setup_data.flags.use_road;
    const object_bounding_box* gps = object_bbox_find_gps(setup_data.pos);
    if (gps != nullptr && object_bbox_get_rally_point(gps, ai->desired_destination))
    {
        return;
    }
    vector road_point;
    float road_u;
    if (object_road_get_closest_road(setup_data.pos, false, road_point, road_u, 80.0f, false, nullptr, nullptr, false) != nullptr)
    {
        // Go by the road when it is on the way and over 10 m from the npc.
        if (vec_dist_squared(road_point, setup_data.pos) < vec_dist_squared(np->pos, setup_data.pos) && 100.0f < vec_dist_squared(np->pos, road_point))
        {
            ai->desired_destination = road_point;
        }
    }
}

// 0x0066FF90 (:1544)
ai_results goto_with_vehicle_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || !ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER))
    {
        return AI_RESULT_FAILURE;
    }
    vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
    if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
    {
        // 30 m/s at most.
        const float speed = ai->desired_vehicle_speed;
        ai->desired_vehicle_speed = (speed < 0.0f || 0.0f <= speed - 30.0f) ? 30.0f : speed;
    }
    ai_vehicle_override_speed(vp, ai->desired_vehicle_speed);
    ai_vehicle_ensure_seat_turret_manned(np, vp, nullptr);
    ai->flags.destination_on_road = object_road_point_on_road(ai->desired_destination, false) != nullptr;
    return AI_RESULT_SUCCESS;
}

// 0x006AC980 (:1586)
ai_results goto_with_vehicle_action::do_action(npc* np) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || !ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER))
    {
        return AI_RESULT_FAILURE;
    }
    ai_system* ai = np->get_ai();
    ai_vehicle_use_turret_if_available(np, vp, false, false, nullptr, ZERO_VECTOR);
    vector dest = ai->desired_destination;
    float dist = ai->desired_vehicle_dist_to_goal_pos;
    const ai_goal* goal = ai->planner.get_current_goal();
    bool use_road = ai->flags.vehicle_use_road;
    bool use_roads = use_road;
    const bool destination_on_road = ai->flags.destination_on_road;
    if (goal == move_with_human_goal::get_instance())
    {
        const object* defend = ai->get_defend_object();
        if (defend != nullptr && defend->get_obj_type() == OT_HUMAN)
        {
            dest = defend->pos;
            use_road = true;
            use_roads = true;
            dist = 0.0f;
        }
    }
    vp->vai.vai_flags.m_ignore_road_directionality = goal == reinforcement_goal::get_instance() ? 1 : 0;
    if (use_road && !destination_on_road)
    {
        // The last 15 m off the road.
        use_roads = 225.0f <= vec_dist_squared(vp->pos, dest);
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
    {
        use_roads = false;
    }
    if (goal == ambient_action_goal::get_instance())
    {
        const float speed = ai->amb_action_info.get_desired_speed(vp);
        ai->desired_vehicle_speed = speed;
        ai_vehicle_override_speed(vp, speed);
    }
    const ai_results result = ai_vehicle_travel_to_a_point_do_frame(np, vp, dest, 0xFFFFFFFFFFFFFFFFull, use_roads, dist, false, false, false, true);
    if (result != AI_RESULT_IN_PROGRESS && goal != move_with_human_goal::get_instance() && !ai->planner.plan_continues_after_current_action())
    {
        ai->planner.avoid_action(this, 15000);
    }
    return result;
}

// 0x005F4F30 (:1654)
ai_results goto_with_vehicle_action::cleanup(npc* np, const ai_action*) const
{
    return human_in_vehicle(np) != nullptr ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x0068BF50 (:1667)
int chase_target_in_vehicle_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    vehicle* vp = ai_vehicle_find_available_seat_type(np, AI_VEHICLE_SEAT_TYPE_DRIVER, nullptr);
    if (vp == nullptr)
    {
        return -1;
    }
    if (vp->get_handle() != np->vehicle_handle || ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED) != AI_VEHICLE_SEAT_TYPE_DRIVER)
    {
        desired_conditions |= ai_plan_conditions_from_bits({48, 49});
    }
    const turret* driver_turret = vehicle_get_seat_turret(vp, VSI_DRIVER);
    if ((driver_turret != nullptr && driver_turret->turret_flags.fixed_turret) || ai_vehicle_has_manned_turret(vp))
    {
        return -1;
    }
    if (np->hflags.disallow_vehicle_exit == 0 && np->hflags.limited_vehicle_exit == 0)
    {
        const human* target;
        if (activity_pursuit_npc(np))
        {
            const object* selected = goal_setup_data.selected_target;
            if (selected == nullptr || selected->get_obj_type() != OT_HUMAN)
            {
                return -1;
            }
            target = static_cast<const human*>(selected);
        }
        else
        {
            target = human_combat_get_target(np);
        }
        if (target == nullptr)
        {
            return -1;
        }
        // At least 20 m from the target or its vehicle.
        const vehicle* target_vehicle = World.object_from_handle<vehicle>(target->vehicle_handle);
        const vector target_pos = target_vehicle != nullptr ? target_vehicle->pos : target->pos;
        if (vec_dist_squared(target_pos, vp->pos) < 400.0f)
        {
            return -1;
        }
    }
    my_data.use_target = vp;
    return 3000;
}

// 0x005F4F50 (:1716)
void chase_target_in_vehicle_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_vehicle = setup_data.use_target->get_handle();
    ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_DRIVER;
}

// 0x00646AB0 (:1725)
ai_results chase_target_in_vehicle_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || vehicle_passenger_in_seat(vp, VSI_DRIVER) != static_cast<human*>(np))
    {
        return AI_RESULT_FAILURE;
    }
    ai_vehicle_ensure_seat_turret_manned(np, vp, nullptr);
    vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
    if (last_action != this)
    {
        ai->action_state = 0;
        ai->action_substate = 0;
    }
    if (np->current_team != HUMAN_TEAM_EDF)
    {
        return AI_RESULT_SUCCESS;
    }
    const human* target;
    if (activity_pursuit_npc(np))
    {
        if (ai->desired_follow_target == 0xFFFFFFFF)
        {
            return AI_RESULT_SUCCESS;
        }
        const object* objp = World.object_from_handle(ai->desired_follow_target);
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN)
        {
            return AI_RESULT_SUCCESS;
        }
        target = static_cast<const human*>(objp);
    }
    else
    {
        target = human_combat_get_target(np);
    }
    // A target in a vehicle within 50 m gets the megaphone.
    if (target != nullptr && vec_dist_squared(np->pos, target->pos) < 2500.0f && human_in_vehicle(target) != nullptr && should_play_chase_line())
    {
        const voice_line_handle line_handle = voice_line_handle_from_line(VOICE_LINE_EDF_VEHICLE_CHASE);
        human_voice_persona* radio = human_voice_radio_persona();
        if (radio != nullptr)
        {
            automobile_play_megaphone(line_handle, radio, vp->get_handle());
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x006ACBD0 (:1784): action_state 1 is the back-off: action_substate holds the frame it started, desired_destination the point backed off
// to, custom_behavior_value whether by road.
ai_results chase_target_in_vehicle_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const human* target;
    if (activity_pursuit_npc(np))
    {
        if (ai->desired_follow_target == 0xFFFFFFFF)
        {
            return AI_RESULT_IN_PROGRESS;
        }
        const object* objp = World.object_from_handle(ai->desired_follow_target);
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN)
        {
            return AI_RESULT_IN_PROGRESS;
        }
        target = static_cast<const human*>(objp);
    }
    else
    {
        target = human_combat_get_target(np);
    }
    if (target == nullptr)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || !ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER))
    {
        return AI_RESULT_FAILURE;
    }
    ai_vehicle_use_turret_if_available(np, vp, false, false, nullptr, ZERO_VECTOR);
    vehicle* target_vehicle = nullptr;
    if (target->vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(target->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            target_vehicle = static_cast<vehicle*>(objp);
        }
    }
    const vector target_pos = target_vehicle != nullptr ? target_vehicle->pos : target->pos;
    if (target_vehicle != nullptr && target_vehicle->get_sub_type() == OT_SUB_VEHICLE_WALKER)
    {
        // Don't drive under a walker: within 20 m skid to a stop.
        vector to_walker = target_vehicle->pos - vp->pos;
        if (to_walker.mag_and_normalize() < 20.0f)
        {
            float speed = 0.0f;
            vehicle_get_speed(vp, speed);
            if (5.0f < speed)
            {
                automobile_set_steering(vp->get_handle(), -1.0f);
                automobile_set_handbrake(vp->get_handle(), true);
                automobile_set_forward_or_reverse_accelerator(vp->get_handle(), 0.01f);
                automobile_set_brake_pedal_input(vp->get_handle(), 0.01f);
                return AI_RESULT_IN_PROGRESS;
            }
            automobile_set_controls_when_abandoned(vp->get_handle());
            return AI_RESULT_SUCCESS;
        }
    }
    bool use_smoothing = true;
    bool use_roads = false;
    bool dont_stop = true;
    if (target_vehicle == nullptr && !np->vehicle_exit_banned() && vec_dist_squared(target_pos, vp->pos) < 400.0f)
    {
        // Close to a target on foot: the action is done for now.
        ai->planner.avoid_action(this, 5000);
        return AI_RESULT_SUCCESS;
    }
    vector dest = target->pos;
    if (ai->action_state == 1)
    {
        const int frames = Framecount - ai->action_substate;
        const vector to_target = target->pos - vp->pos;
        if (-1 < frames && frames <= CHASE_TARGET_BACKOFF_TIMER && to_target.mag_squared() <= CHASE_TARGET_BACKOFF_RADIUS * CHASE_TARGET_BACKOFF_RADIUS)
        {
            dest = ai->desired_destination;
            ai_vehicle_override_speed(vp, 1000.0f);
            use_smoothing = true;
            use_roads = ai->custom_behavior_value != 0;
        }
        else
        {
            ai->action_state = 0;
            ai->action_substate = 0;
        }
    }
    uint64_t hint = 0xFFFFFFFFFFFFFFFFull;
    if (ai->action_state == 0 && ai_vehicle_should_update_travel_to_point(vp))
    {
        if (activity_pursuit_npc(np) && !ai->perception.enemy_in_los(target->get_handle()))
        {
            np->dest_point = ZERO_VECTOR;
        }
        if (target_vehicle != nullptr)
        {
            if (target_vehicle->vai.current_path_info.flags.road_spline_path != 0 && 75.0f < vec_dist(target_pos, np->pos) && !ai->perception.enemy_in_los(target->get_handle()))
            {
                use_roads = true;
            }
            const vector& target_vehicle_pos = target_vehicle->pos;
            vector to_vehicle = target_vehicle_pos - vp->pos;
            use_smoothing = false;
            const float dist_to_vehicle = to_vehicle.mag_and_normalize();
            dest = target_vehicle_pos + target_vehicle->orient.fvec * (vehicle_get_dist_to_front_bumper(vp) + 1.0f);
            float target_speed;
            vehicle_get_speed(target_vehicle, target_speed);
            vector target_velocity;
            vehicle_get_velocity(target_vehicle, target_velocity);
            bool lateral_offset = false;
            bool have_dest = false;
            vector adjusted;
            // 5 m/s faster than the target; the intercept point counts if the target can see that far along the nav mesh, at least 10 m.
            if (generate_intercept_point(vp->pos, target_vehicle_pos, target_velocity, target_speed + 5.0f, dest))
            {
                const nav_cell* cell = bb_pf_get_nav_cell_from_hint(&adjusted, target_vehicle->m_navcell_info.last_visited_nav_cell_handle, target_vehicle_pos, true, nullptr, false, false, NAV_CELL_TRAVERSAL_ALL);
                if (cell != nullptr)
                {
                    vector los_end = ZERO_VECTOR;
                    const nav_cell* los_cell = nullptr;
                    if (nav_cell_find_longest_line_of_sight_no_detour(los_end, los_cell, adjusted, cell, dest, true, nullptr) || 100.0f < vec_dist_squared(los_end, adjusted))
                    {
                        dest = los_end;
                        hint = nav_cell_get_handle(los_cell);
                        lateral_offset = true;
                        have_dest = true;
                    }
                }
            }
            if (!have_dest)
            {
                // Ahead of the target by half the distance to it plus its speed.
                dest = target_vehicle_pos + target_vehicle->orient.fvec * (dist_to_vehicle * 0.5f + target_speed);
                uint64_t found_handle = 0xFFFFFFFFFFFFFFFFull;
                adjusted = ZERO_VECTOR;
                const nav_cell* cell = bb_pf_get_nav_cell(dest, true, &found_handle, true, NAV_CELL_TRAVERSAL_ALL, &adjusted);
                bool on_mesh = false;
                if (cell != nullptr)
                {
                    if ((cell->flags & (NAV_CELL_FLAG_DISABLED | NAV_CELL_FLAG_LIGHT)) == 0)
                    {
                        on_mesh = true;
                        lateral_offset = true;
                    }
                    else
                    {
                        vector valid_pos = ZERO_VECTOR;
                        const nav_cell* valid_cell = nullptr;
                        if (nav_cell_find_first_valid_cell(valid_pos, valid_cell, adjusted, cell, target_vehicle_pos, true, nullptr))
                        {
                            dest = valid_pos;
                            hint = nav_cell_get_handle(valid_cell);
                            on_mesh = true;
                        }
                    }
                }
                if (!on_mesh)
                {
                    // Nothing ahead: wherever the target itself is on the mesh, or the nearest point of the last cell it was on.
                    uint64_t target_handle = 0xFFFFFFFFFFFFFFFFull;
                    vector target_on_mesh = ZERO_VECTOR;
                    if (bb_pf_get_nav_cell_from_hint(&target_on_mesh, target_vehicle->m_navcell_info.last_visited_nav_cell_handle, target_vehicle_pos, true, &target_handle, false, false, NAV_CELL_TRAVERSAL_ALL) != nullptr)
                    {
                        dest = target_on_mesh;
                        hint = target_handle;
                        target_vehicle->m_navcell_info.last_visited_nav_cell_handle = target_handle;
                        lateral_offset = true;
                    }
                    else
                    {
                        const nav_cell* last_cell = nav_cell_from_handle(target_vehicle->m_navcell_info.last_visited_nav_cell_handle);
                        if (last_cell == nullptr)
                        {
                            lateral_offset = true;
                        }
                        else if (nav_cell_is_point_in_cell_column(last_cell, target_vehicle_pos))
                        {
                            dest = target_vehicle_pos;
                            hint = nav_cell_get_handle(last_cell);
                        }
                        else
                        {
                            const vector2 flat_target(target_vehicle_pos.x, target_vehicle_pos.z);
                            float best = -1.0f;
                            vector edge_point = ZERO_VECTOR;
                            for (int edge = 0; edge < 3; edge++)
                            {
                                vector vert0;
                                vector vert1;
                                nav_cell_get_edge_verts(&vert0, &vert1, last_cell, edge, nullptr);
                                vector2 closest;
                                const float edge_dist = vec2_find_closest_dist_squared_to_linesegment(closest, flat_target, vector2(vert0.x, vert0.z), vector2(vert1.x, vert1.z));
                                if (best < 0.0f || edge_dist < best)
                                {
                                    edge_point.x = closest.x;
                                    edge_point.z = closest.y;
                                    best = edge_dist;
                                }
                            }
                            // Original quirk: the forced point is not used, the point stays on the edge.
                            nav_cell_force_point_to_cell(last_cell, edge_point);
                            havok_terrain_height(edge_point.y, edge_point.x, edge_point.z, false, nullptr);
                            dest = edge_point;
                            hint = nav_cell_get_handle(last_cell);
                        }
                    }
                }
            }
            if (lateral_offset && dist_to_vehicle < 10.0f && 0.9f < vp->orient.fvec * target_vehicle->orient.fvec)
            {
                // Alongside the target, heading the same way: swerve 5 m to its side.
                const float side = to_vehicle * vp->orient.rvec <= 0.0f ? -5.0f : 5.0f;
                dest += vp->orient.rvec * side;
            }
            float speed = CHASE_TARGET_SPEED_BOOST + target_speed;
            if (target_vehicle->get_sub_type() == OT_SUB_VEHICLE_WALKER && dist_to_vehicle < 40.0f)
            {
                speed = target_speed + 10.0f;
            }
            ai_vehicle_override_speed(vp, speed);
            if ((np->hflags.limited_vehicle_exit != 0 || np->hflags.driverless_exit_only != 0) && dist_to_vehicle < CHASE_TARGET_BACKOFF_DISTANCE && fabsf(target_speed) < CHASE_TARGET_BACKOFF_SPEED)
            {
                // Riding into a slow target: back off to a random point at a distance, on a road when there is one.
                const float random = fl_rand();
                const matrix rotation(0.0f, 0.0f, random * 3.1415927f + random * 3.1415927f);
                vector rotated;
                rotated.x = rotation.rvec.x * FORWARD_VECTOR.x + rotation.uvec.x * FORWARD_VECTOR.y + rotation.fvec.x * FORWARD_VECTOR.z;
                rotated.y = rotation.rvec.y * FORWARD_VECTOR.x + rotation.uvec.y * FORWARD_VECTOR.y + rotation.fvec.y * FORWARD_VECTOR.z;
                rotated.z = rotation.rvec.z * FORWARD_VECTOR.x + rotation.uvec.z * FORWARD_VECTOR.y + rotation.fvec.z * FORWARD_VECTOR.z;
                vector backoff = target_vehicle_pos + rotated * CHASE_TARGET_BACKOFF_AMOUNT;
                vector road_point;
                float road_u;
                if (object_road_get_closest_road(backoff, true, road_point, road_u, 60.0f, false, nullptr, nullptr, false) != nullptr)
                {
                    backoff = road_point;
                    use_roads = true;
                }
                ai->action_state = 1;
                ai->action_substate = Framecount;
                ai->desired_destination = backoff;
                hint = 0xFFFFFFFFFFFFFFFFull;
                dest = backoff;
                ai_vehicle_override_speed(vp, 1000.0f);
                dont_stop = true;
                use_smoothing = true;
                ai->custom_behavior_value = use_roads ? 1 : 0;
            }
        }
        else
        {
            if (object_road_get_closest_road(target_pos, np->npc_flags.can_use_special_roads != 0, 60.0f, false, false) != nullptr && 75.0f < vec_dist(target_pos, np->pos) &&
                !ai->perception.enemy_in_los(target->get_handle()))
            {
                use_roads = true;
            }
            ai_vehicle_override_speed(vp, -1.0f);
        }
    }
    if (target_vehicle == nullptr && vec_xz_dist_squared(vp->pos, dest) < 10000.0f)
    {
        // Within 100 m of a target on foot: stop when there.
        dont_stop = false;
    }
    const ai_results result = ai_vehicle_travel_to_a_point_do_frame(np, vp, dest, hint, use_roads, 3.0f, dont_stop, false, false, use_smoothing);
    if (result == AI_RESULT_FAILURE && !np->vehicle_exit_banned() && vp->disable_for_npc_drivers_timer.valid() && !vp->disable_for_npc_drivers_timer.elapsed())
    {
        ai->planner.avoid_action(this, 5000);
    }
    return result != AI_RESULT_FAILURE ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x0068C140 (:2128): stop and let the gunner work when the target is slow (3 m/s) and within 30 m.
int chase_target_hold_position_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    vehicle* vp = ai_vehicle_find_available_seat_type(np, AI_VEHICLE_SEAT_TYPE_DRIVER, nullptr);
    if (vp == nullptr || vp->get_sub_type() == OT_SUB_VEHICLE_FLYER || vehicle_is_tank(vp) || vp->get_handle() != np->vehicle_handle ||
        ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED) != AI_VEHICLE_SEAT_TYPE_DRIVER)
    {
        return -1;
    }
    const human* target;
    if (activity_pursuit_npc(np))
    {
        const object* selected = goal_setup_data.selected_target;
        if (selected == nullptr || selected->get_obj_type() != OT_HUMAN)
        {
            return -1;
        }
        target = static_cast<const human*>(selected);
    }
    else
    {
        target = human_combat_get_target(np);
    }
    if (target == nullptr)
    {
        return -1;
    }
    const vehicle* target_vehicle = human_in_vehicle(target);
    if (target_vehicle != nullptr)
    {
        vector velocity;
        vehicle_get_velocity(target_vehicle, velocity);
        if (9.0f < velocity.mag_squared())
        {
            return -1;
        }
    }
    if (vec_dist_squared(target->pos, np->pos) > 900.0f)
    {
        return -1;
    }
    const turret* tp = ai_vehicle_get_first_manned_turret(vp);
    if (tp == nullptr)
    {
        return -1;
    }
    npc* gunner = World.object_from_handle<npc>(tp->current_operator);
    if (gunner != nullptr)
    {
        vector effective_pos;
        if (!gunner->get_ai()->perception.get_effective_target_pos(effective_pos, target, nullptr, nullptr, false) || !tp->can_hit_target(effective_pos) ||
            !gunner->get_ai()->perception.firing_los_clear(target, HST_CURRENT, nullptr))
        {
            return -1;
        }
    }
    my_data.use_target = vp;
    return 25;
}

// 0x005F4F80 (:2206)
void chase_target_hold_position_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_vehicle = setup_data.use_target->get_handle();
    ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_DRIVER;
}

// 0x00646C80 (:2215)
ai_results chase_target_hold_position_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || vehicle_passenger_in_seat(vp, VSI_DRIVER) != static_cast<human*>(np))
    {
        return AI_RESULT_FAILURE;
    }
    const human* target;
    if (activity_pursuit_npc(np))
    {
        if (ai->desired_follow_target == 0xFFFFFFFF)
        {
            return AI_RESULT_FAILURE;
        }
        const object* objp = World.object_from_handle(ai->desired_follow_target);
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN)
        {
            return AI_RESULT_FAILURE;
        }
        target = static_cast<const human*>(objp);
    }
    else
    {
        target = human_combat_get_target(np);
    }
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai_vehicle_ensure_seat_turret_manned(np, vp, nullptr);
    vehicle_ai_free_all_paths(&vp->vai);
    vehicle_ai_set_state(&vp->vai, VS_AI_IDLE);
    ai->action_state = 0;
    ai->action_substate = 0;
    ai->action_timer.set_rand(1000, 3500);
    return AI_RESULT_SUCCESS;
}

// 0x006ADD40 (:2253): every 1 to 3.5 s: gives up for a target over 9 m/s, over 900 m away, or out of the gunner's line of fire.
ai_results chase_target_hold_position_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || vehicle_passenger_in_seat(vp, VSI_DRIVER) != static_cast<human*>(np))
    {
        return AI_RESULT_FAILURE;
    }
    const human* target;
    if (activity_pursuit_npc(np))
    {
        if (ai->desired_follow_target == 0xFFFFFFFF)
        {
            return AI_RESULT_FAILURE;
        }
        const object* objp = World.object_from_handle(ai->desired_follow_target);
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN)
        {
            return AI_RESULT_FAILURE;
        }
        target = static_cast<const human*>(objp);
    }
    else
    {
        target = human_combat_get_target(np);
    }
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle_set_controls_when_abandoned(vp);
    ai_vehicle_use_turret_if_available(np, vp, false, false, nullptr, ZERO_VECTOR);
    if (!ai->action_timer.elapsed())
    {
        return AI_RESULT_IN_PROGRESS;
    }
    const vehicle* target_vehicle = human_in_vehicle(target);
    if (target_vehicle != nullptr)
    {
        vector velocity;
        vehicle_get_velocity(target_vehicle, velocity);
        if (81.0f < velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z)
        {
            return AI_RESULT_FAILURE;
        }
    }
    if (810000.0f < vec_dist_squared(target->pos, np->pos))
    {
        return AI_RESULT_FAILURE;
    }
    const turret* tp = ai_vehicle_get_first_manned_turret(vp);
    if (tp != nullptr)
    {
        npc* gunner = World.object_from_handle<npc>(tp->current_operator);
        if (gunner != nullptr && !gunner->get_ai()->perception.firing_los_clear(target, HST_CURRENT, nullptr))
        {
            return AI_RESULT_FAILURE;
        }
    }
    ai->action_timer.set_rand(1000, 3500);
    return AI_RESULT_IN_PROGRESS;
}

// 0x0068C330 (:2320)
int chase_target_with_fixed_turret::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    vehicle* vp = ai_vehicle_find_available_seat_type(np, AI_VEHICLE_SEAT_TYPE_DRIVER, nullptr);
    if (vp == nullptr || vp->get_sub_type() != OT_SUB_VEHICLE_AUTO)
    {
        return -1;
    }
    if (vp->get_handle() != np->vehicle_handle || ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED) != AI_VEHICLE_SEAT_TYPE_DRIVER)
    {
        desired_conditions |= ai_plan_conditions_from_bits({48, 49});
    }
    const turret* tp = vehicle_get_seat_turret(vp, VSI_DRIVER);
    if (tp == nullptr || !tp->turret_flags.fixed_turret)
    {
        return -1;
    }
    my_data.use_target = vp;
    return 3000;
}

// 0x005F4FB0 (:2349)
void chase_target_with_fixed_turret::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_vehicle = setup_data.use_target->get_handle();
    ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_DRIVER;
}

// 0x005F4FE0 (:2358)
ai_results chase_target_with_fixed_turret::enter(npc* np, const ai_action*) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp != nullptr && ai_vehicle_ensure_seat_turret_manned(np, vp, nullptr))
    {
        vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
        return AI_RESULT_SUCCESS;
    }
    return AI_RESULT_FAILURE;
}

// 0x006ADF60 (:2374): the gun points where the vehicle points, so it has to face the target: drives straight at it, and when too close
// (30 m on foot, 50 m in a vehicle) past it and turns back.
ai_results chase_target_with_fixed_turret::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const human* target;
    if (activity_pursuit_npc(np))
    {
        if (ai->desired_follow_target == 0xFFFFFFFF)
        {
            return AI_RESULT_IN_PROGRESS;
        }
        const object* objp = World.object_from_handle(ai->desired_follow_target);
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN)
        {
            return AI_RESULT_IN_PROGRESS;
        }
        target = static_cast<const human*>(objp);
    }
    else
    {
        target = human_combat_get_target(np);
    }
    if (target == nullptr)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || !ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER))
    {
        return AI_RESULT_FAILURE;
    }
    ai_vehicle_use_turret_if_available(np, vp, false, false, nullptr, ZERO_VECTOR);
    vector target_pos = target->pos;
    vector target_velocity = target->actual_velocity;
    matrix target_orient(target->orient);
    float target_speed = target->actual_velocity_mag;
    const vehicle* target_vehicle = nullptr;
    if (target->vehicle_handle != 0xFFFFFFFF)
    {
        const object* objp = World.object_from_handle(target->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            target_vehicle = static_cast<const vehicle*>(objp);
        }
    }
    float chase_speed = -1.0f;
    if (target_vehicle != nullptr)
    {
        target_pos = target_vehicle->pos;
        vehicle_get_velocity(target_vehicle->get_handle(), target_velocity);
        target_orient = target_vehicle->orient;
        vehicle_get_speed(target_vehicle, target_speed);
        chase_speed = CHASE_TARGET_SPEED_BOOST + target_speed;
    }
    vector dest = target_pos;
    vector to_target = target_pos - vp->pos;
    const float dist_to_target = to_target.mag_and_normalize_safe();
    const float facing_target = vp->orient.fvec * to_target;
    const float facing_same_way = vp->orient.fvec * target_orient.fvec;
    const float turn_range = target_vehicle == nullptr ? 30.0f : 50.0f;
    bool use_smoothing = true;
    if (!ai_vehicle_should_update_travel_to_point(vp))
    {
        dest = vp->vai.current_path_info.end_pos;
    }
    else
    {
        // Original quirk: the nav cell hints are the chaser's, the position the target's.
        const uint64_t hint = target_vehicle != nullptr ? vp->m_navcell_info.last_visited_nav_cell_handle : np->pf.last_visited_nav_cell_handle;
        vector start_on_mesh;
        vector los_end;
        if (5.0f <= target_speed)
        {
            use_smoothing = false;
            if (!generate_intercept_point(vp->pos, target_pos, target_velocity, chase_speed, dest))
            {
                dest = target_pos + target_orient.fvec * (target_speed * 0.5f);
            }
            else
            {
                const nav_cell* cell = bb_pf_get_nav_cell_from_hint(&start_on_mesh, hint, target_pos, true, nullptr, false, false, NAV_CELL_TRAVERSAL_ALL);
                if (cell != nullptr)
                {
                    los_end = ZERO_VECTOR;
                    const nav_cell* los_cell = nullptr;
                    if (nav_cell_find_longest_line_of_sight_no_detour(los_end, los_cell, start_on_mesh, cell, dest, true, nullptr))
                    {
                        dest = los_end;
                    }
                }
            }
        }
        else
        {
            if (activity_pursuit_npc(np) && !ai->perception.enemy_in_los(target->get_handle()))
            {
                np->dest_point = ZERO_VECTOR;
            }
            if ((0.0f <= facing_target && 20.0f <= dist_to_target) || turn_range <= dist_to_target)
            {
                dest = target_pos;
            }
            else
            {
                // Too close: 10 m beyond the turning range, ahead, else behind, else straight at the target.
                const float past = turn_range + 10.0f - dist_to_target;
                dest = vp->pos + vp->orient.fvec * past;
                const nav_cell* cell = bb_pf_get_nav_cell_from_hint(&start_on_mesh, hint, target_pos, true, nullptr, false, false, NAV_CELL_TRAVERSAL_ALL);
                if (cell != nullptr)
                {
                    los_end = ZERO_VECTOR;
                    const nav_cell* los_cell = nullptr;
                    if (nav_cell_find_longest_line_of_sight_no_detour(los_end, los_cell, start_on_mesh, cell, dest, true, nullptr))
                    {
                        dest = los_end;
                    }
                    else
                    {
                        dest = vp->pos + vp->orient.fvec * -past;
                        dest = nav_cell_find_longest_line_of_sight_no_detour(los_end, los_cell, start_on_mesh, cell, dest, true, nullptr) ? los_end : target_pos;
                    }
                }
            }
        }
    }
    float speed = -1.0f;
    if (5.0f <= target_speed)
    {
        // Close behind and facing it, 0.707 = within 45 degrees: match the target's speed.
        speed = (facing_target <= 0.707f || 20.0f <= dist_to_target || facing_same_way <= -0.707f) ? chase_speed : target_speed;
    }
    ai_vehicle_override_speed(vp, speed);
    const ai_results result = ai_vehicle_travel_to_a_point_do_frame(np, vp, dest, 0xFFFFFFFFFFFFFFFFull, false, 3.0f, true, false, false, use_smoothing);
    return result != AI_RESULT_FAILURE ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x0068C450 (:2541)
int chase_target_with_manned_turret::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    vehicle* vp = ai_vehicle_locate_tank(np);
    if (vp == nullptr)
    {
        vp = ai_vehicle_find_available_seat_type(np, AI_VEHICLE_SEAT_TYPE_DRIVER, nullptr);
        if (vp == nullptr)
        {
            return -1;
        }
    }
    if (vp->get_sub_type() != OT_SUB_VEHICLE_AUTO)
    {
        return -1;
    }
    if (vp->get_handle() != np->vehicle_handle || ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED) != AI_VEHICLE_SEAT_TYPE_DRIVER)
    {
        desired_conditions |= ai_plan_conditions_from_bits({48, 49});
    }
    const turret* tp = vehicle_get_seat_turret(vp, VSI_DRIVER);
    if ((tp != nullptr && tp->turret_flags.fixed_turret) || !ai_vehicle_has_manned_turret(vp))
    {
        return -1;
    }
    my_data.use_target = vp;
    return 3000;
}

// 0x005F5060 (:2581)
void chase_target_with_manned_turret::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_vehicle = setup_data.use_target->get_handle();
    ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_DRIVER;
}

static bool chase_in_middle = true; // 0x016415DE, .data: every other pursuit vehicle keeps to the middle.

// 0x005F5090 (:2590)
ai_results chase_target_with_manned_turret::enter(npc* np, const ai_action* last_action) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
    ai_vehicle_ensure_seat_turret_manned(np, vp, nullptr);
    if (last_action != this)
    {
        np->dest_point = ZERO_VECTOR;
        vp->vai.change_sides_timer.invalidate();
        if (np->npc_flags.activity_pursuit_vr != 0)
        {
            vp->vai.vai_flags.middle = chase_in_middle ? 1 : 0;
            chase_in_middle = !chase_in_middle;
        }
    }
    ai_system* ai = np->get_ai();
    ai->action_timer.set(0);
    // Half the diagonal of the vehicle's footprint.
    vector bmin;
    vector bmax;
    vp->get_local_bbox(bmin, bmax);
    ai->desired_vehicle_dist_to_goal_pos = sqrtf((bmax.x - bmin.x) * (bmax.x - bmin.x) + (bmax.z - bmin.z) * (bmax.z - bmin.z)) * 0.5f;
    return AI_RESULT_SUCCESS;
}

// The circling destination around a slow target. np->dest_point holds it, ZERO_VECTOR when there is none, and npc_flags.going_left the direction.
static void manned_turret_pick_circle_point(npc* np, const vehicle* vp, const vector& target_pos, const vector& to_target, float dist_to_target, float range, float near_range, vector& dest)
{
    vector& circle_point = np->dest_point;
    if (dist_to_target < range)
    {
        // Far enough from the last point still (at least the weapon's minimum range, 15 m): keep going there.
        float min_dist = 0.0f;
        const turret* tp = World.object_from_handle<turret>(vp->m_turret_mounts[0].attached_turret);
        if (tp != nullptr)
        {
            const weapon* wp = tp->get_weapon_inst();
            if (wp != nullptr)
            {
                min_dist = wp->w_info->min_engagement_dist;
            }
        }
        if (min_dist - 15.0f < 0.0f)
        {
            min_dist = 15.0f;
        }
        if (circle_point != ZERO_VECTOR && min_dist * min_dist < vec_dist_squared(circle_point, vp->pos))
        {
            return;
        }
        if (circle_point == ZERO_VECTOR)
        {
            np->npc_flags.going_left = fl_rand_chance(0.5f) ? 1 : 0;
        }
        vector offset = -to_target;
        float turn = fl_rand_range(0.25f, 0.5f);
        if (near_range * 0.5f <= dist_to_target)
        {
            offset = offset * near_range;
        }
        else
        {
            offset = offset * ((range - near_range) * 0.5f + near_range);
            turn = fl_rand_range(0.05f, 0.25f);
        }
        const float angle = (np->npc_flags.going_left == 0 ? -1.0f : 1.0f) * turn * 3.1415927f;
        vector rotated = offset;
        rotated.rotate_around_y_axis(angle);
        circle_point = rotated + target_pos;
        vector first_end = ZERO_VECTOR;
        vector second_end = ZERO_VECTOR;
        const nav_cell* out_cell = nullptr;
        vector start_on_mesh;
        const vector start_pos = vp->pos;
        const nav_cell* cell = bb_pf_get_nav_cell_from_hint(&start_on_mesh, vp->m_navcell_info.last_visited_nav_cell_handle, start_pos, true, nullptr, false, false, NAV_CELL_TRAVERSAL_ALL);
        if (cell == nullptr)
        {
            dest = target_pos;
            circle_point = ZERO_VECTOR;
            return;
        }
        const bool first_clear = nav_cell_find_longest_line_of_sight_no_detour(first_end, out_cell, start_on_mesh, cell, circle_point, true, nullptr);
        const float first_dist = vec_dist_squared(vp->pos, first_end);
        if (first_clear)
        {
            circle_point = first_end;
            return;
        }
        // 625: under 25 m is too short a run, try the other way around.
        if (625.0f <= first_dist)
        {
            circle_point = (first_end - vp->pos) * 0.5f + vp->pos;
            return;
        }
        rotated = offset;
        rotated.rotate_around_y_axis(angle * -1.0f);
        circle_point = rotated + target_pos;
        const bool second_clear = nav_cell_find_longest_line_of_sight_no_detour(second_end, out_cell, start_on_mesh, cell, circle_point, true, nullptr);
        const float second_dist = vec_dist_squared(vp->pos, second_end);
        if (second_clear)
        {
            circle_point = second_end;
            np->npc_flags.going_left = np->npc_flags.going_left == 0 ? 1 : 0;
        }
        else if (625.0f <= second_dist)
        {
            circle_point = (second_end - vp->pos) * 0.5f + vp->pos;
            np->npc_flags.going_left = np->npc_flags.going_left == 0 ? 1 : 0;
        }
        else if (first_dist <= 100.0f && second_dist <= 100.0f)
        {
            dest = target_pos;
            circle_point = ZERO_VECTOR;
        }
        else
        {
            circle_point = first_dist <= second_dist ? second_end : first_end;
        }
        return;
    }
    if (circle_point == ZERO_VECTOR || vec_dist_squared(circle_point, vp->pos) <= 25.0f)
    {
        // Too far: run in to 3/4 of the range.
        vector start_on_mesh;
        const vector start_pos = vp->pos;
        const nav_cell* cell = bb_pf_get_nav_cell_from_hint(&start_on_mesh, vp->m_navcell_info.last_visited_nav_cell_handle, start_pos, true, nullptr, false, false, NAV_CELL_TRAVERSAL_ALL);
        if (cell == nullptr)
        {
            dest = target_pos;
            circle_point = ZERO_VECTOR;
            return;
        }
        vector end = ZERO_VECTOR;
        circle_point = vp->pos + to_target * (dist_to_target - range * 0.75f);
        const nav_cell* out_cell = nullptr;
        // Original quirk: a squared distance is compared with the range.
        if (nav_cell_find_longest_line_of_sight_no_detour(end, out_cell, start_on_mesh, cell, circle_point, true, nullptr) ||
            (25.0f < vec_dist_squared(start_on_mesh, end) && vec_dist_squared(start_on_mesh, target_pos) < range))
        {
            circle_point = end;
        }
        else
        {
            dest = target_pos;
            circle_point = ZERO_VECTOR;
        }
    }
}

// Beside a fast target, so the gunner can shoot: 20 m to one side of the intercept point, changing sides at most every 5 s within 40 m.
static void manned_turret_pick_side_point(vehicle* vp, const matrix& target_orient, const vector& to_target, float dist_to_target, const vector& los_end, const nav_cell* los_cell, vector& dest)
{
    const float target_side = target_orient.rvec.x * to_target.x + target_orient.rvec.y * to_target.y + target_orient.rvec.z * to_target.z;
    const float own_side = to_target * vp->orient.rvec;
    const float own_front = to_target * vp->orient.fvec;
    bool want_left;
    if (0.0f <= target_side && own_side <= 0.0f)
    {
        want_left = true;
    }
    else if (target_side < 0.0f && 0.0f < own_side)
    {
        want_left = false;
    }
    else
    {
        // On no clear side of either. 0.966: within 15 degrees ahead.
        want_left = !(own_front <= 0.966f ? target_side < 0.0f : 0.0f < own_side);
    }
    vehicle_ai& vai = vp->vai;
    if (vai.change_sides_timer.valid())
    {
        if (40.0f < dist_to_target)
        {
            vai.change_sides_timer.invalidate();
            vai.vai_flags.left_side = want_left ? 1 : 0;
        }
        if (vai.change_sides_timer.elapsed())
        {
            vai.change_sides_timer.set(5000);
            vai.vai_flags.left_side = want_left ? 0 : 1;
        }
    }
    else
    {
        if (dist_to_target <= 40.0f)
        {
            vai.change_sides_timer.set(5000);
        }
        vai.vai_flags.left_side = want_left ? 1 : 0;
    }
    vector side_point = los_end + target_orient.rvec * (vai.vai_flags.left_side != 0 ? -20.0f : 20.0f);
    vector end = ZERO_VECTOR;
    const nav_cell* out_cell = nullptr;
    if (nav_cell_find_longest_line_of_sight_no_detour(end, out_cell, los_end, los_cell, side_point, true, nullptr) || 100.0f < vec_dist_squared(end, los_end))
    {
        dest = end;
        return;
    }
    // Original quirk: the other side is measured from the point already moved, so this tries los_end itself.
    side_point += target_orient.rvec * (vai.vai_flags.left_side == 0 ? -20.0f : 20.0f);
    if (nav_cell_find_longest_line_of_sight_no_detour(end, out_cell, los_end, los_cell, side_point, true, nullptr) || 100.0f < vec_dist_squared(end, los_end))
    {
        dest = end;
        vai.change_sides_timer.set(5000);
        return;
    }
    dest = los_end;
}

// 0x006AE8E0 (:2627)
ai_results chase_target_with_manned_turret::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const human* target;
    if (activity_pursuit_npc(np))
    {
        if (ai->desired_follow_target == 0xFFFFFFFF)
        {
            return AI_RESULT_IN_PROGRESS;
        }
        const object* objp = World.object_from_handle(ai->desired_follow_target);
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN)
        {
            return AI_RESULT_IN_PROGRESS;
        }
        target = static_cast<const human*>(objp);
    }
    else
    {
        target = human_combat_get_target(np);
    }
    if (target == nullptr)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || !ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER))
    {
        return AI_RESULT_FAILURE;
    }
    vector target_pos = target->pos;
    vector target_velocity = target->actual_velocity;
    matrix target_orient(target->orient);
    float target_speed = target->actual_velocity_mag;
    const vehicle* target_vehicle = nullptr;
    if (target->vehicle_handle != 0xFFFFFFFF)
    {
        const object* objp = World.object_from_handle(target->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            target_vehicle = static_cast<const vehicle*>(objp);
        }
    }
    float chase_speed = -1.0f;
    const bool use_light = np->npc_flags.activity_pursuit_vr != 0;
    bool dont_stop = false;
    if (target_vehicle != nullptr)
    {
        target_pos = target_vehicle->pos;
        vehicle_get_velocity(target_vehicle->get_handle(), target_velocity);
        target_orient = target_vehicle->orient;
        vehicle_get_speed(target_vehicle, target_speed);
        chase_speed = CHASE_TARGET_SPEED_BOOST + target_speed;
        if (object_is_player(target) && human_is_driving(target) && !vehicle_is_tank(target_vehicle))
        {
            dont_stop = true;
        }
    }
    vector dest = target_pos;
    vector to_target = target_pos - vp->pos;
    const float dist_to_target = to_target.mag_and_normalize_safe();
    // Original quirk: the length of the already normalized vector in xz, compared below with half the vehicle's diagonal, in metres.
    const float flat_length = sqrtf(to_target.x * to_target.x + to_target.z * to_target.z);
    const float facing_same_way = vp->orient.fvec.y * target_orient.fvec.y + target_orient.fvec.x * vp->orient.fvec.x + vp->orient.fvec.z * target_orient.fvec.z;
    const bool tank = vehicle_is_tank(vp);
    const float range = (target_vehicle != nullptr || tank) ? 75.0f : 50.0f;
    const float near_range = (target_vehicle != nullptr || tank) ? 50.0f : 30.0f;
    bool use_smoothing = true;
    bool use_roads = false;
    if (tank && 3000 < ai->action_timer.time_since())
    {
        ai_update_combat_target_building(np);
        ai->action_timer.set(0);
    }
    ai_vehicle_use_turret_if_available(np, vp, false, false, nullptr, ZERO_VECTOR);
    if (tank && target_vehicle == nullptr && ai->perception.enemy_in_los(target->get_handle()))
    {
        // A tank holds at range against a target on foot without explosives.
        const weapon* wp = target->get_equipped_weapon();
        const weapon_info* wi = wp != nullptr ? wp->w_info : nullptr;
        if ((wi == nullptr || wi->ammo_type == WEAPON_AMMO_TYPE_THROWN || !wi->is_explosive()) && dist_to_target < range && near_range * 0.5f < dist_to_target)
        {
            vehicle_set_controls_when_abandoned(vp);
            np->dest_point = ZERO_VECTOR;
            return AI_RESULT_IN_PROGRESS;
        }
    }
    bool on_target_road = false;
    if (activity_pursuit_npc(np) && target_vehicle != nullptr && ai->perception.enemy_in_los(target->get_handle()) && target_vehicle->vai.current_path_info.flags.road_spline_path != 0)
    {
        on_target_road = true;
    }
    if (!ai_vehicle_should_update_travel_to_point(vp))
    {
        dest = vp->vai.current_path_info.end_pos;
    }
    else
    {
        const bool road_near = target_vehicle == nullptr ? object_road_get_closest_road(target_pos, np->npc_flags.can_use_special_roads != 0, 60.0f, false, false) != nullptr
                                                         : target_vehicle->vai.current_path_info.flags.road_spline_path != 0;
        if (road_near && 75.0f < vec_dist(target_pos, np->pos) && !ai->perception.enemy_in_los(target->get_handle()))
        {
            use_roads = true;
        }
        if ((target_speed < 5.0f && !on_target_road) || (target_vehicle == nullptr && tank))
        {
            if (activity_pursuit_npc(np) && !ai->perception.enemy_in_los(target->get_handle()))
            {
                np->dest_point = ZERO_VECTOR;
            }
            manned_turret_pick_circle_point(np, vp, target_pos, to_target, dist_to_target, range, near_range, dest);
            if (np->dest_point != ZERO_VECTOR)
            {
                dest = np->dest_point;
            }
            else
            {
                // No point: the target's place on the nav mesh, or the nearest good one.
                vector adjusted = ZERO_VECTOR;
                uint64_t handle = 0xFFFFFFFFFFFFFFFFull;
                nav_cell* cell = bb_pf_get_nav_cell(dest, true, &handle, true, NAV_CELL_TRAVERSAL_ALL, &adjusted);
                if (cell == nullptr)
                {
                    nav_cell* found = nullptr;
                    vector closest;
                    if (bb_pf_find_closest_point_on_nav_mesh(dest, closest, found, true))
                    {
                        np->dest_point = closest;
                        dest = closest;
                    }
                }
                else if ((cell->flags & NAV_CELL_FLAG_DISABLED) == 0)
                {
                    dest = adjusted;
                    np->dest_point = adjusted;
                }
                else
                {
                    vector valid;
                    bool have_valid = find_first_valid_nav_cell_along_path(adjusted, vp->pos, cell, valid) != nullptr;
                    if (!have_valid)
                    {
                        nav_cell* found = nullptr;
                        have_valid = bb_pf_find_closest_point_on_nav_mesh(adjusted, valid, found, true);
                    }
                    if (have_valid)
                    {
                        np->dest_point = valid;
                        dest = valid;
                    }
                }
            }
        }
        else
        {
            np->dest_point = ZERO_VECTOR;
            use_smoothing = false;
            if (!generate_intercept_point(vp->pos, target_pos, target_velocity, chase_speed, dest))
            {
                dest = target_pos + target_orient.fvec * (target_speed * 0.5f);
            }
            else
            {
                // Original quirk: as in the fixed turret chase, the nav cell hint is the chaser's and the position the target's.
                const uint64_t hint = target_vehicle != nullptr ? vp->m_navcell_info.last_visited_nav_cell_handle : np->pf.last_visited_nav_cell_handle;
                vector start_on_mesh;
                const nav_cell* cell = bb_pf_get_nav_cell_from_hint(&start_on_mesh, hint, target_pos, true, nullptr, false, false, NAV_CELL_TRAVERSAL_ALL);
                if (cell != nullptr)
                {
                    vector los_end = ZERO_VECTOR;
                    const nav_cell* los_cell = nullptr;
                    // 10000: 100 m.
                    if (nav_cell_find_longest_line_of_sight_no_detour(los_end, los_cell, start_on_mesh, cell, dest, true, nullptr) || 10000.0f < vec_dist_squared(los_end, start_on_mesh))
                    {
                        dest = los_end;
                        if (!dont_stop && vp->vai.vai_flags.middle == 0)
                        {
                            manned_turret_pick_side_point(vp, target_orient, to_target, dist_to_target, los_end, los_cell, dest);
                        }
                    }
                }
            }
        }
    }
    float speed;
    if (ai->desired_vehicle_dist_to_goal_pos <= flat_length)
    {
        if (5.0f <= target_speed || on_target_road)
        {
            speed = chase_speed;
            if (!dont_stop)
            {
                if (20.0f <= dist_to_target || facing_same_way <= -0.707f)
                {
                    if (tank && chase_speed == -1.0f)
                    {
                        speed = 5.0f;
                    }
                }
                else
                {
                    speed = target_speed;
                }
            }
        }
        else
        {
            speed = tank ? 15.0f : -1.0f;
            if (dist_to_target <= range)
            {
                speed = tank ? 10.0f : 20.0f;
            }
        }
    }
    else
    {
        speed = 15.0f;
    }
    ai_vehicle_override_speed(vp, speed);
    if (tank && target_vehicle == nullptr && ai->perception.enemy_in_los(target->get_handle()))
    {
        // Stuck for 2 to 5 s with the target in sight: a tank can just shoot from here.
        const unsigned int stuck_time = (unsigned int)vp->vai.stuck.stuck_active_time.time_since() - 2001;
        if (stuck_time < 2999)
        {
            vehicle_set_controls_when_abandoned(vp);
            return AI_RESULT_IN_PROGRESS;
        }
    }
    const ai_results result = ai_vehicle_travel_to_a_point_do_frame(np, vp, dest, 0xFFFFFFFFFFFFFFFFull, use_roads, 3.0f, dont_stop, use_light, false, use_smoothing);
    if (result != AI_RESULT_FAILURE)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (np->hflags.disallow_vehicle_exit == 0 && np->hflags.limited_vehicle_exit == 0 && vp->disable_for_npc_drivers_timer.valid() && !vp->disable_for_npc_drivers_timer.elapsed())
    {
        ai->planner.avoid_action(this, 5000);
    }
    return AI_RESULT_FAILURE;
}

// 0x0068C590 (:3114)
int chase_target_with_flying_turret::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    vehicle* vp = ai_vehicle_find_available_seat_type(np, AI_VEHICLE_SEAT_TYPE_DRIVER, nullptr);
    if (vp == nullptr || vp->get_sub_type() != OT_SUB_VEHICLE_FLYER)
    {
        return -1;
    }
    if (vp->get_handle() != np->vehicle_handle || ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED) != AI_VEHICLE_SEAT_TYPE_DRIVER)
    {
        desired_conditions |= ai_plan_conditions_from_bits({48, 49});
    }
    if (vehicle_get_seat_turret(vp, VSI_DRIVER) == nullptr && !ai_vehicle_has_manned_turret(vp))
    {
        return -1;
    }
    my_data.use_target = vp;
    return 3000;
}

// 0x005F5210 (:3142)
void chase_target_with_flying_turret::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_vehicle = setup_data.use_target->get_handle();
    ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_DRIVER;
}

// 0x005F5240 (:3151)
ai_results chase_target_with_flying_turret::enter(npc* np, const ai_action* last_action) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
    ai_vehicle_ensure_seat_turret_manned(np, vp, nullptr);
    if (last_action != this)
    {
        np->dest_point = ZERO_VECTOR;
        np->npc_flags.going_left = 0;
        np->npc_flags.going_left_last_time = 0;
        vehicle_ai_free_all_paths(&vp->vai);
        vehicle_ai_flush_route(vp->vai.route_info);
        ai_system* ai = np->get_ai();
        ai->action_timer.set(0);
        ai->action_substate = 1;
    }
    return AI_RESULT_SUCCESS;
}

// A point on the 50 m circle around the target, angle radians from the line out to the flyer, towards the side of the given up vector.
static vector flyer_circle_point(const vector& target_pos, const vector& away, const vector& up, float angle)
{
    return target_pos + away * (cosf(angle) * 50.0f) + (up ^ away) * (sinf(angle) * 50.0f);
}

// 0x006B04A0 (:3178): np->dest_point is the point it is flying to and action_substate the attack run over a slow target, for
// action_timer: 0 circle (15 s), 1 hover behind and stare (5 s), 2 pass over the target (15 s), 3 strafe across it (15 s); an explosive
// turret never picks 3. -1 is none yet.
ai_results chase_target_with_flying_turret::do_action(npc* np) const
{
    // pi/4, pi/6, pi/2: the angles around the target; read from .rdata doubles and float constants at each __CIsin/__CIcos.
    static const float FLYER_ANGLE_45 = 0.7853981852531433f;
    static const float FLYER_ANGLE_30 = 0.5235987901687622f;
    static const float FLYER_ANGLE_90 = 1.5707963705062866f;
    ai_system* ai = np->get_ai();
    const human* target;
    if (activity_pursuit_npc(np))
    {
        if (ai->desired_follow_target == 0xFFFFFFFF)
        {
            return AI_RESULT_IN_PROGRESS;
        }
        const object* objp = World.object_from_handle(ai->desired_follow_target);
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN)
        {
            return AI_RESULT_IN_PROGRESS;
        }
        target = static_cast<const human*>(objp);
    }
    else
    {
        target = human_combat_get_target(np);
    }
    if (target == nullptr)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (np->vehicle_handle == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    object* vehicle_objp = World.object_from_handle(np->vehicle_handle);
    if (vehicle_objp == nullptr || vehicle_objp->get_obj_type() != OT_VEHICLE || vehicle_objp->get_sub_type() != OT_SUB_VEHICLE_FLYER)
    {
        return AI_RESULT_FAILURE;
    }
    flyer* fp = static_cast<flyer*>(vehicle_objp);
    vehicle_ai& vai = fp->vai;
    ai_vehicle_use_turret_if_available(np, fp, false, false, nullptr, ZERO_VECTOR);
    bool explosive_turret = false;
    const turret* seat_turret = vehicle_get_seat_turret(fp, np->vehicle_seat_idx);
    if (seat_turret != nullptr && seat_turret->t_info != nullptr && seat_turret->t_info->w_info != nullptr && seat_turret->t_info->w_info->is_explosive())
    {
        explosive_turret = true;
    }
    vector target_pos = target->pos;
    vector target_velocity = target->actual_velocity;
    matrix target_orient(target->orient);
    float target_speed = target->actual_velocity_mag;
    const vehicle* target_vehicle = nullptr;
    if (target->vehicle_handle != 0xFFFFFFFF)
    {
        const object* objp = World.object_from_handle(target->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            target_vehicle = static_cast<const vehicle*>(objp);
        }
    }
    float speed = 20.0f;
    if (target_vehicle != nullptr)
    {
        target_pos = target_vehicle->pos;
        vehicle_get_velocity(target_vehicle->get_handle(), target_velocity);
        target_orient = target_vehicle->orient;
        vehicle_get_speed(target_vehicle, target_speed);
        // Original quirk: the test is of the chaser, which is always a flyer here, so the car boost is never used.
        speed = (fp->get_sub_type() == OT_SUB_VEHICLE_FLYER ? FLYING_CHASE_TARGET_SPEED_BOOST : CHASE_TARGET_SPEED_BOOST) + target_speed;
    }
    vector dest = target_pos;
    const vector& own_pos = fp->pos;
    vector to_target = target_pos - own_pos;
    vector look_at = to_target;
    look_at.normalize_safe();
    if (!fp->disable_for_npc_drivers_timer.elapsed())
    {
        fp->set_request_lookat_vector(look_at);
        return AI_RESULT_IN_PROGRESS;
    }
    const float dist_to_target = to_target.flatten_mag_and_normalize_safe();
    const float facing_same_way = fp->orient.fvec.y * target_orient.fvec.y + target_orient.fvec.x * fp->orient.fvec.x + fp->orient.fvec.z * target_orient.fvec.z;
    float ground = 0.0f;
    havok_terrain_height(ground, own_pos.x, own_pos.z, false, nullptr);
    const float target_height = target_pos.y - ground;
    vai.target_offset = target_height - 0.0f < 0.0f ? 0.0f : target_height;
    bool dont_stop = true;
    bool use_smoothing = false;
    bool flying_to_point = false;
    vector& circle_point = np->dest_point;
    if (!ai_vehicle_should_update_travel_to_point(fp))
    {
        if (5.0f <= target_speed)
        {
            if (50.0f <= dist_to_target || facing_same_way <= -0.707f)
            {
                fp->clear_requested_lookat_vector();
                use_smoothing = true;
            }
            else
            {
                fp->set_request_lookat_vector(look_at);
            }
        }
        else
        {
            dont_stop = false;
            switch (ai->action_substate)
            {
            case -1:
                use_smoothing = true;
                fp->clear_requested_lookat_vector();
                break;
            case 0:
                if (100.0f <= dist_to_target || dist_to_target < 20.0f || to_target * fp->orient.fvec <= 0.0f)
                {
                    fp->clear_requested_lookat_vector();
                    use_smoothing = true;
                }
                else
                {
                    fp->set_request_lookat_vector(look_at);
                    use_smoothing = false;
                }
                break;
            case 1:
            {
                fp->set_request_lookat_vector(look_at);
                vector point = target_pos - to_target * 50.0f;
                flying_to_point = true;
                havok_terrain_height(point.y, point.x, point.z, false, nullptr);
                point.y = vai.desired_flying_offset + vai.building_offset + vai.target_offset + point.y;
                fp->set_request_flyto_point(point, true);
                vehicle_ai_free_all_paths(&vai);
                vehicle_ai_flush_route(vai.route_info);
                break;
            }
            case 2:
                use_smoothing = true;
                fp->set_request_lookat_vector(look_at);
                break;
            case 3:
                fp->clear_requested_lookat_vector();
                use_smoothing = true;
                break;
            default:
                break;
            }
        }
        dest = vai.current_path_info.end_pos;
    }
    else
    {
        // Flyers within 50 m in xz. Original bug kept: the array has 7 entries and the count is not checked.
        vector others[7];
        int num_others = 0;
        const int num_flyers = World.get_num_objects<flyer>();
        for (int i = 0; i < num_flyers; i++)
        {
            const flyer* other = World.get_object<flyer>(i);
            if (other != fp && other->flyer_flags.on_ground == 0 && vec_xz_dist_squared(other->pos, own_pos) <= 2500.0f)
            {
                others[num_others++] = other->pos;
            }
        }
        if (5.0f <= target_speed)
        {
            // A moving target: to the intercept point, 20 m to the side the flyer is already on.
            ai->action_timer.invalidate();
            circle_point = ZERO_VECTOR;
            vai.desired_flying_offset = 20.0f;
            if (generate_intercept_point(own_pos, target_pos, target_velocity, speed, dest))
            {
                const float target_side = to_target.x * target_orient.rvec.x + target_orient.rvec.y * to_target.y + target_orient.rvec.z * to_target.z;
                const float own_side = fp->orient.rvec.y * to_target.y + fp->orient.rvec.x * to_target.x + fp->orient.rvec.z * to_target.z;
                bool negate;
                if (0.0f <= target_side && own_side <= 0.0f)
                {
                    negate = true;
                }
                else if (target_side < 0.0f && 0.0f < own_side)
                {
                    negate = false;
                }
                else if (fp->orient.fvec.y * to_target.y + to_target.x * fp->orient.fvec.x + fp->orient.fvec.z * to_target.z <= 0.966f)
                {
                    negate = 0.0f <= target_side;
                }
                else
                {
                    negate = own_side <= 0.0f;
                }
                dest += target_orient.rvec * (negate ? -20.0f : 20.0f);
            }
            else
            {
                dest = target_pos + target_orient.fvec * (target_speed * 0.5f);
            }
            if (50.0f <= dist_to_target || facing_same_way <= -0.707f)
            {
                fp->clear_requested_lookat_vector();
                use_smoothing = true;
            }
            else
            {
                speed = target_speed;
                fp->set_request_lookat_vector(look_at);
            }
        }
        else
        {
            if (activity_pursuit_npc(np) && !ai->perception.enemy_in_los(target->get_handle()))
            {
                circle_point = ZERO_VECTOR;
            }
            dont_stop = false;
            if (100.0f <= dist_to_target)
            {
                // Far away: come in to 50 m on this side.
                dest = target_pos - to_target * 50.0f;
                circle_point = dest;
                speed = 15.0f;
                use_smoothing = true;
                np->npc_flags.going_left = fl_rand() <= 0.5f ? 1 : 0;
                np->npc_flags.going_left_last_time = np->npc_flags.going_left;
                vai.desired_flying_offset = 20.0f;
                fp->clear_requested_lookat_vector();
                ai->action_timer.invalidate();
                ai->action_substate = -1;
            }
            else if (dist_to_target < 20.0f && ai->action_substate != 2)
            {
                // Too close: on past the target, 50 m along the way the flyer is moving.
                vector direction;
                vehicle_get_velocity(fp, direction);
                if (direction.flatten_mag_and_normalize_safe() < 1.0f)
                {
                    direction = fp->orient.fvec;
                }
                dest = target_pos + direction * 50.0f;
                circle_point = dest;
                speed = 30.0f;
                vai.desired_flying_offset = 20.0f;
                fp->clear_requested_lookat_vector();
                use_smoothing = true;
                ai->action_timer.invalidate();
                ai->action_substate = -1;
            }
            else
            {
                timestamp& timer = ai->action_timer;
                if (timer.valid())
                {
                    if (timer.elapsed())
                    {
                        const int substate = ai->action_substate;
                        if (substate == -1)
                        {
                            ai->action_substate = int_rand_range(0, explosive_turret ? 2 : 3);
                        }
                        else if (!explosive_turret)
                        {
                            ai->action_substate = (int_rand_range(1, 3) + substate) % 4;
                        }
                        else
                        {
                            ai->action_substate = (int_rand_range(1, 2) + substate) % 3;
                        }
                        timer.invalidate();
                        circle_point = ZERO_VECTOR;
                    }
                }
                else
                {
                    ai->action_substate = int_rand_range(0, explosive_turret ? 2 : 3);
                    circle_point = ZERO_VECTOR;
                }
                switch (ai->action_substate)
                {
                case 0:
                {
                    if (!timer.valid())
                    {
                        timer.set(15000);
                    }
                    if (to_target * fp->orient.fvec <= 0.0f)
                    {
                        fp->clear_requested_lookat_vector();
                        use_smoothing = true;
                    }
                    else
                    {
                        fp->set_request_lookat_vector(look_at);
                        use_smoothing = false;
                    }
                    speed = 15.0f;
                    const bool need_point = circle_point == ZERO_VECTOR || vec_xz_dist_squared(own_pos, circle_point) < 100.0f;
                    if (!need_point && num_others < 1)
                    {
                        dest = circle_point;
                    }
                    else if (need_point && num_others == 0)
                    {
                        // The next point around, alone in the sky. Every so often turn the other way and change height, 15 m or 20 m.
                        bool same_way_again = false;
                        if (circle_point == ZERO_VECTOR)
                        {
                            np->npc_flags.going_left = fl_rand_chance(0.5f) ? 1 : 0;
                            np->npc_flags.going_left_last_time = np->npc_flags.going_left == 0 ? 1 : 0;
                            vai.desired_flying_offset = 15.0f;
                        }
                        else if (np->npc_flags.going_left_last_time == np->npc_flags.going_left && fl_rand_chance(0.5f))
                        {
                            np->npc_flags.going_left = np->npc_flags.going_left == 0 ? 1 : 0;
                            np->npc_flags.going_left_last_time = np->npc_flags.going_left == 0 ? 1 : 0;
                            if (vai.desired_flying_offset == 15.0f)
                            {
                                vai.desired_flying_offset = 20.0f;
                            }
                            else if (vai.desired_flying_offset == 20.0f)
                            {
                                vai.desired_flying_offset = 15.0f;
                            }
                        }
                        else
                        {
                            if (np->npc_flags.going_left_last_time != np->npc_flags.going_left)
                            {
                                np->npc_flags.going_left_last_time = np->npc_flags.going_left;
                            }
                            same_way_again = true;
                        }
                        vector away(own_pos.x - target_pos.x, 0.0f, own_pos.z - target_pos.z);
                        away.normalize();
                        const vector up = np->npc_flags.going_left == 0 ? UP_VECTOR : -UP_VECTOR;
                        dest = flyer_circle_point(target_pos, away, up, same_way_again ? FLYER_ANGLE_45 : FLYER_ANGLE_30);
                        havok_terrain_height(dest.y, dest.x, dest.z, false, nullptr);
                        circle_point = dest;
                    }
                    else
                    {
                        // Others near: a side their flight paths leave free.
                        vector away(own_pos.x - target_pos.x, 0.0f, own_pos.z - target_pos.z);
                        away.normalize();
                        vector right_point = flyer_circle_point(target_pos, away, UP_VECTOR, FLYER_ANGLE_45);
                        vector left_point = flyer_circle_point(target_pos, away, -UP_VECTOR, FLYER_ANGLE_45);
                        bool right_free = true;
                        bool left_free = true;
                        for (int i = 0; i < num_others && (right_free || left_free); i++)
                        {
                            if (100.0f <= vec_xz_dist_squared(own_pos, others[i]))
                            {
                                vector closest;
                                if (right_free)
                                {
                                    ix_closest_point_on_line_segment(closest, others[i], own_pos, right_point);
                                    if (vec_xz_dist_squared(others[i], closest) < 100.0f)
                                    {
                                        right_free = false;
                                    }
                                }
                                if (left_free)
                                {
                                    ix_closest_point_on_line_segment(closest, others[i], own_pos, left_point);
                                    if (vec_xz_dist_squared(others[i], closest) < 100.0f)
                                    {
                                        left_free = false;
                                    }
                                }
                            }
                            else if ((others[i] - own_pos) * fp->orient.rvec <= 0.0f)
                            {
                                left_free = false;
                            }
                            else
                            {
                                right_free = false;
                            }
                        }
                        if (left_free || right_free || num_others < 1)
                        {
                            if (vai.desired_flying_offset == 15.0f)
                            {
                                vai.desired_flying_offset = 20.0f;
                            }
                            else if (vai.desired_flying_offset == 20.0f)
                            {
                                vai.desired_flying_offset = 15.0f;
                            }
                        }
                        // Original quirk: with neither side free rfg.exe sums an escape vector away from the others (40 m minus the
                        // distance to each) into dest and dest_point, then overwrites both with the right point just below; not kept
                        // as its result is never read. The inlined test leaves right_point in that case too.
                        if (!left_free || (right_free && fl_rand() <= 0.5f))
                        {
                            np->npc_flags.going_left = 1;
                            np->npc_flags.going_left_last_time = 0;
                            havok_terrain_height(right_point.y, right_point.x, right_point.z, false, nullptr);
                            dest = right_point;
                        }
                        else
                        {
                            np->npc_flags.going_left = 0;
                            np->npc_flags.going_left_last_time = 1;
                            havok_terrain_height(left_point.y, left_point.x, left_point.z, false, nullptr);
                            dest = left_point;
                        }
                        circle_point = dest;
                    }
                    break;
                }
                case 1:
                {
                    if (!timer.valid())
                    {
                        timer.set(5000);
                        vai.desired_flying_offset = 20.0f;
                        vehicle_ai_free_all_paths(&vai);
                        vehicle_ai_flush_route(vai.route_info);
                    }
                    vector point = target_pos - to_target * 50.0f;
                    havok_terrain_height(point.y, point.x, point.z, false, nullptr);
                    point.y = vai.desired_flying_offset + vai.building_offset + vai.target_offset + point.y;
                    circle_point = point;
                    fp->set_request_lookat_vector(look_at);
                    flying_to_point = true;
                    fp->set_request_flyto_point(point, true);
                    break;
                }
                case 2:
                    if (!timer.valid())
                    {
                        timer.set(15000);
                        vai.desired_flying_offset = 20.0f;
                    }
                    use_smoothing = true;
                    fp->set_request_lookat_vector(look_at);
                    speed = 20.0f;
                    if (circle_point == ZERO_VECTOR || vec_xz_dist_squared(circle_point, own_pos) < 100.0f)
                    {
                        dest = target_pos + to_target * 50.0f;
                        circle_point = dest;
                    }
                    else
                    {
                        dest = circle_point;
                    }
                    break;
                case 3:
                    if (!timer.valid())
                    {
                        timer.set(15000);
                        vai.desired_flying_offset = 20.0f;
                        np->npc_flags.going_left = 0.0f < fp->orient.rvec * to_target ? 0 : 1;
                    }
                    fp->clear_requested_lookat_vector();
                    use_smoothing = true;
                    speed = 20.0f;
                    if (circle_point != ZERO_VECTOR && 100.0f <= vec_xz_dist_squared(circle_point, own_pos))
                    {
                        dest = circle_point;
                    }
                    else
                    {
                        vector away(own_pos.x - target_pos.x, 0.0f, own_pos.z - target_pos.z);
                        away.normalize();
                        const vector up = np->npc_flags.going_left == 0 ? UP_VECTOR : -UP_VECTOR;
                        dest = flyer_circle_point(target_pos, away, up, FLYER_ANGLE_90);
                        havok_terrain_height(dest.y, dest.x, dest.z, false, nullptr);
                        circle_point = dest;
                    }
                    break;
                default:
                    break;
                }
            }
        }
        vai.desired_flying_offset = Air_traffic_controller.request_altitude_clearance(np->vehicle_handle, own_pos.y - ground);
        const float clearance = Air_traffic_controller.request_speed_clearance(np->vehicle_handle);
        float cleared_speed = 0.0f <= speed - -clearance ? speed : -clearance;
        if (0.0f <= cleared_speed - clearance)
        {
            cleared_speed = clearance;
        }
        ai_vehicle_override_speed(fp, cleared_speed);
    }
    if (!flying_to_point && ai_vehicle_travel_to_a_point_do_frame(np, fp, dest, 0xFFFFFFFFFFFFFFFFull, false, 3.0f, dont_stop, true, true, use_smoothing) == AI_RESULT_FAILURE)
    {
        circle_point = own_pos;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00646D80 (:3789)
ai_results chase_target_with_flying_turret::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this && np->vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(np->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            static_cast<vehicle*>(objp)->vai.target_offset = 0.0f;
        }
    }
    return AI_RESULT_SUCCESS;
}

// Smash building is switched off in rfg.exe: the four bodies below are all it has.

// 0x005F5320 (:3804)
int smash_building_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return -1;
}

// 0x005F5330 (:3824)
void smash_building_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x005F5340 (:3831)
ai_results smash_building_action::enter(npc*, const ai_action*) const
{
    return AI_RESULT_FAILURE;
}

// 0x005F5350 (:3858)
ai_results smash_building_action::do_action(npc*) const
{
    return AI_RESULT_FAILURE;
}

// 0x005F5360 (:4009): true with immediate when the turret has been off target for 3 s; true without when it is no use for a target at
// the muzzle's distance, which the vehicle's own box shields.
bool should_stop_man_turret_in_vehicle(const npc* np, const vehicle* vp, bool& immediate)
{
    immediate = false;
    if (vehicle_passenger_in_seat(vp, VSI_DRIVER) != nullptr)
    {
        return false;
    }
    turret* tp = human_combat_get_manned_turret(np);
    if (tp == nullptr || !tp->is_functional(false))
    {
        return true;
    }
    if (tp->divergence_time.valid() && 2999 < tp->divergence_time.time_since())
    {
        immediate = true;
        return true;
    }
    const human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return false;
    }
    const weapon* wp = tp->get_weapon_inst();
    if (wp == nullptr)
    {
        return true;
    }
    vector muzzle_pos;
    wp->get_muzzle_transform(muzzle_pos, nullptr);
    const float muzzle_dist = vec_dist(tp->pos, muzzle_pos);
    const float reach = muzzle_dist + 2.0f;
    if (vec_dist_squared(target->pos, tp->pos) <= reach * reach)
    {
        vector local_pos = target->pos;
        vector bmin;
        vector bmax;
        vp->get_local_bbox(bmin, bmax);
        bmax.y += muzzle_dist;
        vec_change_to_local_coords(local_pos, vp->pos, vp->orient);
        const float box_dist_sqrd = bbox_find_distance_squared(bmin, bmax, local_pos);
        if (box_dist_sqrd <= 0.0f)
        {
            return true;
        }
        if (box_dist_sqrd <= 1.0f)
        {
            vector head_pos;
            human_get_head_pos(target, head_pos);
            if (head_pos.y < muzzle_pos.y && !tp->can_hit_target(head_pos))
            {
                return true;
            }
        }
    }
    return false;
}

// 0x0068C6B0 (:4077)
int drive_for_manned_turret_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const unsigned int squad_handle = np->get_ai()->get_squad_handle();
    if (squad_handle == 0xFFFFFFFF)
    {
        return -1;
    }
    const object_squad* squad = static_cast<const object_squad*>(World.object_from_handle(squad_handle));
    if (squad == nullptr || squad->get_obj_type() != OT_SQUAD || squad->squad_vehicle == 0xFFFFFFFF)
    {
        return -1;
    }
    vehicle* vp = static_cast<vehicle*>(World.object_from_handle(squad->squad_vehicle));
    if (vp == nullptr || vp->get_obj_type() != OT_VEHICLE)
    {
        return -1;
    }
    const human* driver = vehicle_get_first_passenger(vp, VSI_DRIVER);
    if (driver != nullptr && driver != static_cast<const human*>(np))
    {
        return -1;
    }
    const human* gunner = vehicle_get_human_in_gunner_seat(vp);
    if (gunner == nullptr || gunner == static_cast<const human*>(np) || (gunner->get_obj_type() == OT_HUMAN && gunner->get_sub_type() == OT_SUB_HUMAN_PLAYER))
    {
        return -1;
    }
    if (!ai_vehicle_is_usable_and_available(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER))
    {
        return -1;
    }
    my_data.use_target = vp;
    return 55;
}

// 0x005F5550 (:4125)
void drive_for_manned_turret_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_vehicle = setup_data.use_target->get_handle();
    ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_DRIVER;
}

// 0x005F5580 (:4133)
ai_results drive_for_manned_turret_action::enter(npc* np, const ai_action*) const
{
    const vehicle* vp = human_in_vehicle(np);
    if (vp != nullptr)
    {
        const human* gunner = vehicle_get_human_in_gunner_seat(vp);
        if (gunner != nullptr && gunner != static_cast<const human*>(np))
        {
            return vehicle_passenger_get_seat(vp, np) == VSI_DRIVER ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
        }
    }
    return AI_RESULT_FAILURE;
}

// 0x005F55D0 (:4158): the driver just holds still for the gunner.
ai_results drive_for_manned_turret_action::do_action(npc* np) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp != nullptr)
    {
        const human* gunner = vehicle_get_human_in_gunner_seat(vp);
        if (gunner != nullptr && gunner != static_cast<const human*>(np) && vehicle_passenger_get_seat(vp, np) == VSI_DRIVER)
        {
            vehicle_set_controls_when_abandoned(vp);
            return AI_RESULT_IN_PROGRESS;
        }
    }
    return AI_RESULT_FAILURE;
}

// 0x0068C760 (:4191)
int man_turret_in_vehicle_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node* next_action) const
{
    vehicle* vp = nullptr;
    const object* leader = goal_setup_data.selected_leader;
    if (leader != nullptr)
    {
        if (leader->get_obj_type() != OT_HUMAN)
        {
            return -1;
        }
        const unsigned int leader_vehicle = static_cast<const human*>(leader)->vehicle_handle;
        if (leader_vehicle != 0xFFFFFFFF)
        {
            object* objp = World.object_from_handle(leader_vehicle);
            if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
            {
                vp = static_cast<vehicle*>(objp);
            }
        }
    }
    if (vp == nullptr)
    {
        object* use_target = goal_setup_data.use_target;
        if (use_target != nullptr && use_target->get_obj_type() == OT_VEHICLE)
        {
            vp = static_cast<vehicle*>(use_target);
        }
        else
        {
            vp = ai_vehicle_locate_for_man_turret_in_vehicle(np);
            if (vp == nullptr)
            {
                return -1;
            }
        }
    }
    if (!ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_GUNNER) || vp == nullptr || gunner_seat_available(vp, np, false, nullptr) == nullptr)
    {
        return -1;
    }
    if (vp->get_handle() != np->vehicle_handle || ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_GUNNER) != AI_VEHICLE_SEAT_TYPE_GUNNER)
    {
        desired_conditions |= ai_plan_conditions_from_bits({48, 49});
    }
    for (const ai_planner_search_node* node = next_action; node != nullptr; node = node->next_node)
    {
        if (node->next_action == shoot_rail_rule::get_instance())
        {
            // Shooting from the rail needs someone else to drive.
            const human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
            if (driver == nullptr || driver == static_cast<const human*>(np))
            {
                return -1;
            }
            break;
        }
    }
    my_data.use_target = vp;
    return 105;
}

// 0x005F5620 (:4244)
void man_turret_in_vehicle_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_vehicle = setup_data.use_target->get_handle();
    ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_GUNNER;
}

// 0x005F5650 (:4251)
ai_results man_turret_in_vehicle_action::enter(npc* np, const ai_action* last_action) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    turret* tp = vehicle_get_seat_turret(vp, np->vehicle_seat_idx);
    if (tp == nullptr || !tp->is_functional(false))
    {
        return AI_RESULT_FAILURE;
    }
    if (np->combat.reserved_turret == 0xFFFFFFFF)
    {
        tp->attach_new_operator(np);
    }
    ai_vehicle_ensure_seat_turret_manned(np, vp, tp);
    if (last_action != this)
    {
        np->get_ai()->action_timer.invalidate();
    }
    return AI_RESULT_SUCCESS;
}

// 0x006B2940 (:4280)
ai_results man_turret_in_vehicle_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (ai == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    bool immediate;
    timestamp& timer = ai->action_timer;
    if (should_stop_man_turret_in_vehicle(np, vp, immediate))
    {
        if (immediate)
        {
            timer.set(0);
        }
        else if (!timer.valid())
        {
            timer.set(2000);
        }
    }
    else
    {
        timer.invalidate();
    }
    turret* tp = human_combat_get_manned_turret(np);
    if (tp != nullptr && !tp->turret_flags.full360)
    {
        const human* target = ai->get_human_target();
        vector effective_pos;
        if (target != nullptr && ai->perception.get_effective_target_pos(effective_pos, target, nullptr, nullptr, false) && vehicle_passenger_in_seat(vp, VSI_DRIVER) == nullptr && !tp->can_hit_target(effective_pos))
        {
            ai->planner.avoid_action(this, 5000);
            return AI_RESULT_FAILURE;
        }
    }
    if (timer.valid() && timer.elapsed())
    {
        if (tp != nullptr)
        {
            tp->disable_for_npcs_time.set(Ai_reuse_turret_after_ineffective_time * 1000);
        }
        ai->planner.avoid_action(this, 10000);
        return AI_RESULT_FAILURE;
    }
    ai_vehicle_use_turret_if_available(np, vp, false, false, nullptr, ZERO_VECTOR);
    return AI_RESULT_IN_PROGRESS;
}

// 0x00646DC0 (:4343): 50 for the driver.
int wait_passenger_clear_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if ((np->npc_flags.passenger_wait != 0 || np->npc_flags.player_exit_wait != 0) && np->vehicle_handle != 0xFFFFFFFF)
    {
        const vehicle* vp = static_cast<const vehicle*>(World.object_from_handle(np->vehicle_handle));
        if (vp != nullptr && vp->get_obj_type() == OT_VEHICLE)
        {
            return static_cast<const human*>(np) != vehicle_passenger_in_seat(vp, VSI_DRIVER) ? -1 : 50;
        }
    }
    return -1;
}

// 0x006700C0 (:4365): stands until nobody on foot is in the vehicle's spawn radius, unless the player hit the driver.
ai_results wait_passenger_clear_action::do_action(npc* np) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || !human_is_driving(np))
    {
        return AI_RESULT_FAILURE;
    }
    if (np->hflags.damaged_by_player == 0)
    {
        vehicle_set_controls_when_abandoned(vp);
        const float clear_radius_sqrd = vp->info->spawn_size * vp->info->spawn_size;
        if (np->npc_flags.player_exit_wait != 0)
        {
            const int num_players = World.get_num_objects<player>();
            for (int i = 0; i < num_players; i++)
            {
                const player* pl = World.get_object<player>(i);
                if (vp->get_handle() == pl->vehicle_handle || (pl->vehicle_handle == 0xFFFFFFFF && vec_xz_dist_squared(pl->pos, vp->pos) < clear_radius_sqrd))
                {
                    return AI_RESULT_IN_PROGRESS;
                }
            }
        }
        if (np->npc_flags.passenger_wait != 0)
        {
            const int num_humans = World.get_num_objects<human>();
            for (int i = 0; i < num_humans; i++)
            {
                const human* hp = World.get_object<human>(i);
                if (!hp->is_killed() && hp->vehicle_handle == 0xFFFFFFFF && vec_xz_dist_squared(hp->pos, vp->pos) < clear_radius_sqrd)
                {
                    return AI_RESULT_IN_PROGRESS;
                }
            }
        }
    }
    np->npc_flags.passenger_wait = 0;
    np->npc_flags.player_exit_wait = 0;
    return AI_RESULT_SUCCESS;
}

// 0x005F56F0 (:4456)
int wait_for_passengers_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 50;
}

// 0x005F5700 (:4461)
ai_results wait_for_passengers_action::enter(npc* np, const ai_action*) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai_vehicle_ensure_seat_turret_manned(np, vp, nullptr);
    return AI_RESULT_SUCCESS;
}

// 0x006B2AE0 (:4473)
ai_results wait_for_passengers_action::do_action(npc* np) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || !human_is_driving(np))
    {
        return AI_RESULT_FAILURE;
    }
    vehicle_set_controls_when_abandoned(vp);
    ai_vehicle_use_turret_if_available(np, vp, false, false, nullptr, ZERO_VECTOR);
    return AI_RESULT_IN_PROGRESS;
}

// 0x0068C920 (:4496)
int drop_off_passengers_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if (np->vehicle_handle != 0xFFFFFFFF)
    {
        const vehicle* vp = static_cast<const vehicle*>(World.object_from_handle(np->vehicle_handle));
        if (vp != nullptr && vp->get_obj_type() == OT_VEHICLE && ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER) && ai_vehicle_is_seat_available(vp, np, VSI_DRIVER))
        {
            return 50;
        }
    }
    return -1;
}

// 0x005F5770 (:4512)
ai_results drop_off_passengers_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        ai_system* ai = np->get_ai();
        ai->action_timer.invalidate();
        ai->action_substate = 0;
        ai->desired_goal_pos = ZERO_VECTOR;
    }
    return AI_RESULT_SUCCESS;
}

// 0x0068C980 (:4527): stuck passengers: drive 10 m somewhere random on the nav mesh, ten tries; otherwise stop and wait at least 5 s.
ai_results drop_off_passengers_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || !human_is_driving(np))
    {
        return AI_RESULT_FAILURE;
    }
    if (!vehicle_interaction_passengers_stuck(vp))
    {
        vehicle_set_controls_when_abandoned(vp);
        if (!ai_vehicle_should_stop_to_let_passenger_out(vp))
        {
            Yell_vehicle_timeout.set(10000);
            if (!ai->action_timer.valid())
            {
                ai->action_timer.set(5000);
            }
            if (ai->action_timer.elapsed())
            {
                return AI_RESULT_SUCCESS;
            }
        }
        return AI_RESULT_IN_PROGRESS;
    }
    if (10 < ai->action_substate)
    {
        ai->planner.avoid_action(this, 5000);
        return AI_RESULT_FAILURE;
    }
    vector& point = ai->desired_goal_pos;
    if (point == ZERO_VECTOR)
    {
        ai->action_substate++;
        vector try_point = FORWARD_VECTOR * 10.0f;
        try_point.rotate_around_y_axis(fl_rand_range(0.0f, 6.2831855f));
        try_point += np->pos;
        nav_cell* found = nullptr;
        if (!bb_pf_find_closest_point_on_nav_mesh_fast(point, found, try_point, nullptr, nullptr, true, 5.0f))
        {
            point = ZERO_VECTOR;
            return AI_RESULT_IN_PROGRESS;
        }
    }
    // Original quirk: the point is cleared before it is driven to, when within sqrt(10) m.
    if (vec_xz_dist_squared(vp->pos, point) < 10.0f)
    {
        point = ZERO_VECTOR;
    }
    if (ai_vehicle_travel_to_a_point_do_frame(np, vp, point, 0xFFFFFFFFFFFFFFFFull, false, 3.0f, false, false, false, true) == AI_RESULT_FAILURE)
    {
        vp->disable_for_npc_drivers_timer.set(0);
        point = ZERO_VECTOR;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00670280 (:4604)
int acquire_visual_in_vehicle_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const ai_system* ai = np->get_ai();
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed() || ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, target->get_handle()) == nullptr)
    {
        return -1;
    }
    const vehicle* vp = human_in_vehicle(np);
    return vp != nullptr && human_is_driving(np) && ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER) ? 100 : -1;
}

// 0x005F57C0 (:4632)
ai_results acquire_visual_in_vehicle_action::enter(npc* np, const ai_action* last_action) const
{
    if (human_combat_get_target(np) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || !human_is_driving(np))
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        np->get_ai()->action_timer.invalidate();
        np->dest_point = ZERO_VECTOR;
        np->npc_flags.going_left = fl_rand() <= 0.5f ? 1 : 0;
        vehicle_ai_free_all_paths(&vp->vai);
    }
    return AI_RESULT_SUCCESS;
}

// 0x006B2B60 (:4666): to the enemy's believed position; a building there is circled by its corners 10 m out, 40 m away until within 50 m.
ai_results acquire_visual_in_vehicle_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr)
    {
        return AI_RESULT_SUCCESS;
    }
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, target);
    if (belief == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || !ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER))
    {
        return AI_RESULT_FAILURE;
    }
    nav_cell_traversal_rule traversal = NAV_CELL_TRAVERSAL_ENABLED;
    bool use_light = false;
    bool use_disabled = false;
    const bool flyer_vehicle = vp->get_sub_type() == OT_SUB_VEHICLE_FLYER;
    if (flyer_vehicle || vehicle_is_tank(vp))
    {
        traversal = NAV_CELL_TRAVERSAL_ALL;
        use_light = true;
        use_disabled = true;
    }
    float speed = 10.0f;
    if (flyer_vehicle)
    {
        vector look_at = belief->pos - vp->pos;
        look_at.normalize_safe();
        static_cast<flyer*>(vp)->set_request_lookat_vector(look_at);
        speed = 20.0f;
        if (!vp->disable_for_npc_drivers_timer.elapsed())
        {
            return AI_RESULT_IN_PROGRESS;
        }
    }
    const vector& belief_pos = belief->pos;
    vector dest = belief_pos;
    bool fire_even_if_no_los = true;
    if (ai_vehicle_should_update_travel_to_point(vp))
    {
        if (flyer_vehicle)
        {
            vp->vai.desired_flying_offset = Air_traffic_controller.request_altitude_clearance(np->vehicle_handle, vp->vai.original_offset);
        }
        unsigned int buildings[6];
        const int num_buildings = World.collect_objects<rfg_mover>(belief_pos, 30.0f, buildings, 6, collect_buildings_callback, np);
        rfg_mover* building = nullptr;
        float best_dist = 100.0f;
        for (int i = 0; i < num_buildings; i++)
        {
            rfg_mover* candidate = World.object_from_handle<rfg_mover>(buildings[i]);
            if (candidate == nullptr)
            {
                continue;
            }
            const float dist = ai_building_find_bbox_dist_squared(belief_pos, candidate);
            if (dist <= 0.0f)
            {
                building = candidate;
                break;
            }
            if (dist < best_dist)
            {
                best_dist = dist;
                building = candidate;
            }
        }
        if (building == nullptr)
        {
            vector adjusted = ZERO_VECTOR;
            nav_cell* cell = bb_pf_get_nav_cell(belief_pos, true, nullptr, false, traversal, &adjusted);
            if (cell == nullptr && bb_pf_find_closest_point_on_nav_mesh(belief_pos, adjusted, cell, true))
            {
                dest = adjusted;
            }
        }
        else
        {
            if (human_teams_are_allies(np, building->team))
            {
                fire_even_if_no_los = false;
            }
            vector& point = np->dest_point;
            if (point != ZERO_VECTOR && 100.0f <= vec_xz_dist_squared(point, vp->pos))
            {
                dest = point;
            }
            else
            {
                vector bmin;
                vector bmax;
                building->get_bbox(bmin, bmax);
                if (bbox_find_distance_squared(bmin, bmax, np->pos) <= 2500.0f)
                {
                    bbox_grow(bmin, bmax, vector(10.0f, 0.0f, 10.0f));
                    bbox_grow(bmin, bmax, vector(-10.0f, 0.0f, -10.0f));
                    // The corners in order around the box, at y 0 until the nav mesh gives them a height.
                    vector corners[4] = {vector(bmin.x, 0.0f, bmax.z), vector(bmin.x, 0.0f, bmin.z), vector(bmax.x, 0.0f, bmin.z), vector(bmax.x, 0.0f, bmax.z)};
                    bool reachable[4] = {false, false, false, false};
                    uint64_t handle = 0xFFFFFFFFFFFFFFFFull;
                    nav_cell* cell = bb_pf_get_nav_cell(building->pos, true, &handle, false, NAV_CELL_TRAVERSAL_ALL, nullptr);
                    if (cell != nullptr)
                    {
                        for (int i = 0; i < 4; i++)
                        {
                            vector valid_pos = ZERO_VECTOR;
                            const nav_cell* valid_cell = find_first_valid_nav_cell_along_path(building->pos, corners[i], cell, valid_pos);
                            if (valid_cell != nullptr)
                            {
                                vector los_end = ZERO_VECTOR;
                                const nav_cell* los_cell = nullptr;
                                if (nav_cell_find_longest_line_of_sight_no_detour(los_end, los_cell, valid_pos, valid_cell, corners[i], false, nullptr) || 25.0f < vec_dist_squared(valid_pos, los_end))
                                {
                                    corners[i] = los_end;
                                    reachable[i] = true;
                                }
                            }
                        }
                    }
                    int index = -1;
                    float best = FLT_MAX;
                    for (int i = 0; i < 4; i++)
                    {
                        if (reachable[i])
                        {
                            const float dist = vec_xz_dist_squared(corners[i], vp->pos);
                            if (dist < best)
                            {
                                index = i;
                                best = dist;
                            }
                        }
                    }
                    bool any_reachable = true;
                    if (index == -1)
                    {
                        float nearest = FLT_MAX;
                        for (int i = 0; i < 4; i++)
                        {
                            const float dist = vec_xz_dist_squared(corners[i], vp->pos);
                            if (dist < nearest)
                            {
                                index = i;
                                nearest = dist;
                            }
                        }
                        any_reachable = false;
                    }
                    // Original quirk: with no reachable corner best is still FLT_MAX, so the search does not move on from the nearest.
                    if (best <= 100.0f)
                    {
                        const int step = np->npc_flags.going_left != 0 ? -1 : 1;
                        do
                        {
                            index += step;
                            if (index < 0)
                            {
                                index = 3;
                            }
                            else if (3 < index)
                            {
                                index = 0;
                            }
                        } while (any_reachable && !reachable[index]);
                    }
                    dest = corners[index];
                    point = dest;
                }
                else
                {
                    vector away(vp->pos.x - building->pos.x, 0.0f, vp->pos.z - building->pos.z);
                    away.normalize_safe();
                    dest = building->pos + away * 40.0f;
                    point = dest;
                }
            }
        }
    }
    float cleared_speed = speed;
    if (flyer_vehicle)
    {
        const float clearance = Air_traffic_controller.request_speed_clearance(np->vehicle_handle);
        cleared_speed = 0.0f <= speed - -clearance ? speed : -clearance;
        if (0.0f <= cleared_speed - clearance)
        {
            cleared_speed = clearance;
        }
    }
    ai_vehicle_override_speed(vp, cleared_speed);
    ai_vehicle_use_turret_if_available(np, vp, fire_even_if_no_los, false, nullptr, ZERO_VECTOR);
    const ai_results result = ai_vehicle_travel_to_a_point_do_frame(np, vp, dest, 0xFFFFFFFFFFFFFFFFull, false, 3.0f, false, use_light, use_disabled, true);
    return result != AI_RESULT_FAILURE ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x00670310 (:4946)
int pwn_in_tank_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const ai_system* ai = np->get_ai();
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed() || ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, target->get_handle()) == nullptr)
    {
        return -1;
    }
    const vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || !human_is_driving(np) || !ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER))
    {
        return -1;
    }
    return vehicle_is_tank(vp) ? 99 : -1;
}

// 0x005F5880 (:4979)
ai_results pwn_in_tank_action::enter(npc* np, const ai_action* last_action) const
{
    if (human_combat_get_target(np) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || !human_is_driving(np) || !vehicle_is_tank(vp))
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        vehicle_ai_free_all_paths(&vp->vai);
        ai_system* ai = np->get_ai();
        ai->action_timer.set(0);
        np->dest_point = ZERO_VECTOR;
        ai->action_state = 1;
        ai->desired_target_recalc_timestamp.invalidate();
        ai->action_substate = 0;
    }
    return AI_RESULT_SUCCESS;
}

// 0x006B3510 (:5018): shells the building the target hides in from 50 m away, for up to 2 minutes. action_state 1 is the first 10 s, aimed
// just over the target, 2 random pieces of the building. action_substate 1 uses the secondary gun, for an important friendly building.
ai_results pwn_in_tank_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr)
    {
        return AI_RESULT_SUCCESS;
    }
    if (120000 < ai->action_timer.time_since())
    {
        ai->planner.avoid_action(this, 5000);
        return AI_RESULT_FAILURE;
    }
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, target);
    if (belief == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || !ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER) || !vehicle_is_tank(vp))
    {
        return AI_RESULT_FAILURE;
    }
    if (ai_vehicle_should_update_travel_to_point(vp))
    {
        unsigned int buildings[6];
        const int num_buildings = World.collect_objects<rfg_mover>(belief->pos, 30.0f, buildings, 6, collect_buildings_callback, np);
        rfg_mover* building = nullptr;
        float best_dist = 100.0f;
        for (int i = 0; i < num_buildings; i++)
        {
            rfg_mover* candidate = World.object_from_handle<rfg_mover>(buildings[i]);
            if (candidate == nullptr)
            {
                continue;
            }
            const float dist = ai_building_find_bbox_dist_squared(belief->pos, candidate);
            if (dist <= 0.0f)
            {
                building = candidate;
                break;
            }
            if (dist < best_dist)
            {
                building = candidate;
                best_dist = dist;
            }
        }
        if (building == nullptr)
        {
            ai->planner.avoid_action(this, 5000);
            return AI_RESULT_FAILURE;
        }
        if (human_teams_are_allies(np, building->team) && 1 < (int)building->get_importance_level())
        {
            ai->action_substate = 1;
            if (vp->info->vehicle_classification == BVT_ART_TANK)
            {
                ai->planner.avoid_action(this, 5000);
                return AI_RESULT_FAILURE;
            }
        }
        else
        {
            ai->action_substate = 0;
        }
        ai->desired_target = building->get_handle();
        vector away = vp->pos - belief->pos;
        away.flatten_normalize_safe();
        vector point = belief->pos + away * 50.0f;
        vector adjusted = ZERO_VECTOR;
        nav_cell* cell = bb_pf_get_nav_cell(belief->pos, true, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, &adjusted);
        if (cell == nullptr && bb_pf_find_closest_point_on_nav_mesh(belief->pos, adjusted, cell, true))
        {
            point = adjusted;
        }
        np->dest_point = point;
    }
    const ai_belief* target_belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, np->combat.target_handle);
    if (target_belief == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    turret* tp = vehicle_get_seat_turret(vp, np->vehicle_seat_idx);
    if (tp == nullptr || !tp->can_hit_target(target_belief->pos))
    {
        return AI_RESULT_FAILURE;
    }
    weapon* wp = ai->action_substate == 1 ? tp->get_secondary_weapon_inst() : tp->get_weapon_inst();
    if (wp == nullptr)
    {
        ai->planner.avoid_action(this, 5000);
        return AI_RESULT_FAILURE;
    }
    if (ai->action_state == 1)
    {
        ai->desired_target_pos = target_belief->pos + UP_VECTOR;
    }
    else if (!ai->desired_target_recalc_timestamp.valid() || ai->desired_target_recalc_timestamp.elapsed())
    {
        const rfg_mover* building = World.object_from_handle<rfg_mover>(ai->desired_target);
        rfg_destroyable_shape* shape = building != nullptr ? building->get_shape() : nullptr;
        vector aim_pos = ZERO_VECTOR;
        if (shape == nullptr)
        {
            aim_pos = target_belief->pos + UP_VECTOR;
        }
        else
        {
            // A random piece; half a metre over the ground if it lies below it.
            const int piece = int_rand_range(0, shape->num_existing_child_shapes - 1);
            rfg_subpiece* sp = nullptr;
            const unsigned short first = shape->child_obj_list;
            if (first != 0xFFFF)
            {
                int count = 0;
                unsigned short index = first;
                do
                {
                    rfg_subpiece* candidate = shape->get_subpiece(index);
                    if (count == piece)
                    {
                        sp = candidate;
                        break;
                    }
                    index = candidate->next;
                    count++;
                } while (index != first);
            }
            matrix orient;
            shape->get_subpiece_pos_orient_uncached(sp, aim_pos, orient);
            float ground = 0.0f;
            havok_terrain_height(ground, aim_pos.x, aim_pos.z, false, nullptr);
            if (aim_pos.y < ground)
            {
                aim_pos.y = ground + 0.5f;
            }
        }
        ai->desired_target_pos = aim_pos;
        ai->desired_target_recalc_timestamp.set(3000);
    }
    tp->aim_turret(ai->desired_target_pos);
    if (wp->ready_to_fire())
    {
        ai_combat_fire_turret_at_pos(np, tp, ai->desired_target_pos, ai->action_substate == 1);
    }
    if (ai->action_state == 1 && 10000 < ai->action_timer.time_since())
    {
        ai->action_state = 2;
    }
    // 20 to 100 m away is close enough to stand.
    const float dist_to_aim = vec_xz_dist_squared(vp->pos, ai->desired_target_pos);
    if (dist_to_aim < 10000.0f && 400.0f < dist_to_aim)
    {
        vehicle_set_controls_when_abandoned(vp);
        return AI_RESULT_IN_PROGRESS;
    }
    ai_vehicle_override_speed(vp, 10.0f);
    const ai_results result = ai_vehicle_travel_to_a_point_do_frame(np, vp, np->dest_point, 0xFFFFFFFFFFFFFFFFull, false, 3.0f, false, true, false, true);
    return result != AI_RESULT_FAILURE ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x0068CC60 (:5220): the cost is that of leaving the squad's vehicle behind; none while within 40 m of a leader it can still ride with.
int retreat_def_perimeter_vehicle_rule::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const ai_system* ai = np->get_ai();
    if (activity_cc_is_courier(np))
    {
        return -1;
    }
    if (ai->passenger_of_squadmate() == nullptr && !human_is_driving(np) && ai->get_squad_handle() != 0xFFFFFFFF)
    {
        object_squad* squad = static_cast<object_squad*>(World.object_from_handle(ai->get_squad_handle()));
        if (squad != nullptr && squad->get_obj_type() == OT_SQUAD)
        {
            npc* leader = squad->get_leader();
            if (leader != nullptr && leader != np && vec_dist_squared(np->pos, leader->pos) <= 1600.0f)
            {
                if (human_in_vehicle(leader) == nullptr)
                {
                    return leader->get_ai()->near_orders_location(0.0f) ? 100 : -1;
                }
                const vehicle* leader_vehicle = World.object_from_handle<vehicle>(leader->vehicle_handle);
                return !ai_vehicle_is_usable_and_available(leader_vehicle, np, AI_VEHICLE_SEAT_TYPE_PASSENGER) ? 100 : -1;
            }
        }
    }
    return 100;
}

// 0x005F5940 (:5272)
int man_flyer_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 50;
}

// 0x00646E20 (:5277)
ai_results man_flyer_action::enter(npc* np, const ai_action*) const
{
    if (np->vehicle_handle != 0xFFFFFFFF)
    {
        const object* objp = World.object_from_handle(np->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && objp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
        {
            return AI_RESULT_SUCCESS;
        }
    }
    return AI_RESULT_FAILURE;
}

// 0x00646E60 (:5287): takes off to 12 m above the ground.
ai_results man_flyer_action::do_action(npc* np) const
{
    if (np->vehicle_handle == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    object* objp = World.object_from_handle(np->vehicle_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE || objp->get_sub_type() != OT_SUB_VEHICLE_FLYER)
    {
        return AI_RESULT_FAILURE;
    }
    flyer* fp = static_cast<flyer*>(objp);
    if (player_is_sabotaging_flyer(player_get_local_player(), fp))
    {
        return AI_RESULT_FAILURE;
    }
    float ground = 0.0f;
    havok_terrain_height(ground, fp->pos.x, fp->pos.z, false, nullptr);
    if (12.0f < fp->pos.y - ground)
    {
        fp->flyer_flags.on_ground = 0;
        return AI_RESULT_SUCCESS;
    }
    vector direction(fp->orient.fvec.x, 5.0f, fp->orient.fvec.z);
    direction.normalize_safe();
    fp->set_request_flyto_direction(direction * 5.0f);
    return AI_RESULT_IN_PROGRESS;
}

// 0x00646FB0 (:5322)
int loiter_flyer_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if (np->vehicle_handle != 0xFFFFFFFF)
    {
        const object* objp = World.object_from_handle(np->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && objp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
        {
            return 100;
        }
    }
    return -1;
}

// 0x00646FF0 (:5332)
ai_results loiter_flyer_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (np->vehicle_handle == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    object* objp = World.object_from_handle(np->vehicle_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE || objp->get_sub_type() != OT_SUB_VEHICLE_FLYER)
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        flyer* fp = static_cast<flyer*>(objp);
        fp->set_request_halt();
        ai->action_timer.set(int_rand_range(5000, 15000));
        ai->action_pos = np->pos;
        fp->vai.desired_flying_offset = Air_traffic_controller.request_altitude_clearance(np->vehicle_handle, fp->vai.original_offset);
    }
    return AI_RESULT_SUCCESS;
}

// 0x006470B0 (:5357): every 5 to 15 s drifts to a point 5 to 10 m from where it started, 20 m above the ground, until it knows of an enemy.
ai_results loiter_flyer_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (np->vehicle_handle == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    object* objp = World.object_from_handle(np->vehicle_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE || objp->get_sub_type() != OT_SUB_VEHICLE_FLYER)
    {
        return AI_RESULT_FAILURE;
    }
    if (ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION) != nullptr)
    {
        return AI_RESULT_SUCCESS;
    }
    if (ai->action_timer.elapsed())
    {
        vector direction = FORWARD_VECTOR;
        direction.rotate_around_y_axis(fl_rand_range(0.0f, 6.2831855f));
        vector point = ai->action_pos + direction * fl_rand_range(5.0f, 10.0f);
        havok_terrain_height(point.y, point.x, point.z, false, nullptr);
        point.y += 20.0f;
        static_cast<flyer*>(objp)->set_request_flyto_point(point, true);
        ai->action_timer.set(int_rand_range(5000, 15000));
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x0068CD50 (:5402)
int fodder_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const unsigned int squad_handle = np->get_ai()->get_squad_handle();
    if (squad_handle != 0xFFFFFFFF)
    {
        object_squad* squad = static_cast<object_squad*>(World.object_from_handle(squad_handle));
        if (squad != nullptr && squad->get_obj_type() == OT_SQUAD)
        {
            const gfm_orders* orders = squad->get_squad_orders();
            if (orders != nullptr && orders->get_orders_type() == GFM_ORDERS_TYPE_FODDER)
            {
                return 50;
            }
        }
    }
    return -1;
}

// 0x0068CDA0 (:5413)
ai_results fodder_action::enter(npc* np, const ai_action* last_action) const
{
    if (!human_is_driving(np) || np->vehicle_handle == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    object* objp = World.object_from_handle(np->vehicle_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = static_cast<vehicle*>(objp);
    vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
    ai_system* ai = np->get_ai();
    if (!ai->has_orders_of_type(GFM_ORDERS_TYPE_FODDER))
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        np->dest_point = vp->pos;
        ai->action_state = 2;
        ai->action_substate = -1;
    }
    // 1000 m/s: flat out.
    vehicle_ai_set_speed_override(&vp->vai, 1000.0f);
    return AI_RESULT_SUCCESS;
}

// 0x006472C0 (:5442)
ai_results fodder_action::cleanup(npc* np, const ai_action*) const
{
    if (human_is_driving(np) && np->vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(np->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            vehicle_ai_clear_speed_override(&static_cast<vehicle*>(objp)->vai);
            return AI_RESULT_SUCCESS;
        }
    }
    return AI_RESULT_FAILURE;
}

// 0x0068CE70 (:5458): drives down roads past the player to be blown up, ahead of the player and coming towards them. action_state 0 is
// on the way along a road to its end (action_substate holds the road), 1 towards a road that joins the player's, 2 pick the next road.
ai_results fodder_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (ai->get_squad_handle() == 0xFFFFFFFF)
    {
        return AI_RESULT_SUCCESS;
    }
    object_squad* squad = static_cast<object_squad*>(World.object_from_handle(ai->get_squad_handle()));
    if (squad == nullptr || squad->get_obj_type() != OT_SQUAD)
    {
        return AI_RESULT_SUCCESS;
    }
    const gfm_orders* orders = squad->get_squad_orders();
    if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_FODDER)
    {
        return AI_RESULT_SUCCESS;
    }
    if (np->vehicle_handle == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = static_cast<vehicle*>(World.object_from_handle(np->vehicle_handle));
    if (vp == nullptr || vp->get_obj_type() != OT_VEHICLE || !human_is_driving(np))
    {
        return AI_RESULT_FAILURE;
    }
    if (np->npc_flags.orders_active == 0)
    {
        vehicle_set_controls_when_abandoned(vp);
        return AI_RESULT_IN_PROGRESS;
    }
    const player* pl = player_get_local_player();
    const object* center = pl;
    if (np->npc_flags.fodder_on_object != 0)
    {
        center = orders->get_target_object();
    }
    const vector target_pos = center->pos;
    const vector& player_forward = pl->orient.fvec;
    vector forward = pl->orient.fvec;
    const vehicle* player_vehicle = World.object_from_handle<vehicle>(pl->vehicle_handle);
    if (player_vehicle != nullptr)
    {
        forward = player_vehicle->orient.fvec;
    }
    if (ai->action_state == 0 && 2500.0f < vec_dist_squared(target_pos, vp->pos) && (vp->pos.x - target_pos.x) * forward.x + (vp->pos.y - target_pos.y) * forward.y + (vp->pos.z - target_pos.z) * forward.z < 0.0f)
    {
        // Over 50 m behind the player: the squad starts again at its point, 10 m up, facing a point 100 m ahead of the player.
        vector spawn_pos = np->dest_point + UP_VECTOR * 10.0f;
        vector facing = player_forward * 100.0f + target_pos - spawn_pos;
        facing.normalize_safe();
        matrix orient;
        orient.make_quick(facing);
        ai->get_squad()->update_pos_and_orient(spawn_pos, orient, true);
        ai->get_squad()->despawn_members();
        return AI_RESULT_IN_PROGRESS;
    }
    bool use_roads = ai->action_state == 1 || ai->action_state == 2;
    vp->vai.vai_flags.m_ignore_lanes = 1;
    if (ai_vehicle_should_update_travel_to_point(vp))
    {
        object_path_road* road = nullptr;
        float road_u = 1.0f;
        vector road_point;
        if (use_roads || vec_dist_squared(np->dest_point, vp->pos) < 100.0f)
        {
            road = object_road_get_closest_road(vp->pos, true, road_point, road_u, 60.0f, false, nullptr, nullptr, false);
        }
        use_roads = false;
        const float direction = object_road_get_direction(road, vp->orient.fvec, road_u);
        vector target_road_point;
        float target_road_u;
        object_path_road* target_road = object_road_get_closest_road(target_pos, true, target_road_point, target_road_u, 60.0f, false, nullptr, nullptr, false);
        if (target_road == nullptr)
        {
            ai->action_state = 2;
            vehicle_set_controls_when_abandoned(vp);
            return AI_RESULT_IN_PROGRESS;
        }
        if (ai->action_state == 0)
        {
            ai->action_state = road != nullptr && (unsigned int)ai->action_substate == road->get_handle() ? 1 : 2;
        }
        if (road != nullptr && ai->action_state == 1)
        {
            // On to the end of this road when that end joins the player's road, or the player is within 100 m.
            ai->action_state = 2;
            bool found = false;
            int best = 1000;
            const road_spline_point* points = road->spline_data.points.to_pointer();
            const road_spline_connection* connections = road->spline_data.connections.to_pointer();
            for (int i = 0; i < road->spline_data.num_connections; i++)
            {
                if (connections[i].handle != target_road->get_handle())
                {
                    continue;
                }
                const int offset = (int)connections[i].from_pt - fl_round(road_u - 1.0f);
                if (direction == fl_sign((float)offset) && abs(offset) < best)
                {
                    vp->vai.vai_flags.m_ignore_lanes = 1;
                    use_roads = true;
                    found = true;
                    np->dest_point = points[direction == 1.0f ? road->spline_data.num_points - 1 : 0].pos;
                    ai->action_state = 1;
                    best = abs(offset);
                }
            }
            if (!found && vec_dist_squared(vp->pos, target_pos) < 10000.0f &&
                ((direction == 1.0f && fl_round(road_u) < road->spline_data.num_points) || (direction == -1.0f && 1.0f < road_u)))
            {
                vp->vai.vai_flags.m_ignore_lanes = 1;
                use_roads = true;
                np->dest_point = points[direction == 1.0f ? road->spline_data.num_points - 1 : 0].pos;
                ai->action_state = 1;
            }
            if (road == target_road)
            {
                ai->action_state = 2;
            }
        }
        if (ai->action_state == 2)
        {
            // The nearest road off the player's road, ahead of the player and at least 100 m from them; else 25 m in front of the player.
            object_path_road* next_road = nullptr;
            const float target_direction = object_road_get_direction(target_road, player_forward, target_road_u);
            int best = 1000;
            const road_spline_connection* connections = target_road->spline_data.connections.to_pointer();
            for (int i = 0; i < target_road->spline_data.num_connections; i++)
            {
                const int offset = (int)connections[i].from_pt - fl_round(target_road_u);
                if (0 < offset && target_direction == fl_sign((float)offset) && abs(offset) < best && 10000.0f <= vec_xz_dist_squared(target_road->get_road_point(connections[i].from_pt), pl->pos))
                {
                    next_road = World.object_from_handle<object_path_road>(connections[i].handle);
                    best = abs(offset);
                }
            }
            if (next_road != nullptr)
            {
                vp->vai.vai_flags.m_ignore_lanes = 0;
                use_roads = false;
                const road_spline_point* next_points = next_road->spline_data.points.to_pointer();
                const vector& first = next_points[0].pos;
                const vector& last = next_points[next_road->spline_data.num_points - 1].pos;
                float to_first = vec_dist_squared(vp->pos, first);
                float to_last = vec_dist_squared(vp->pos, last);
                // Under 4 m from an end: the other end, otherwise the far end.
                if ((to_first < to_last ? to_first : to_last) < 16.0f)
                {
                    const float swap = to_first;
                    to_first = to_last;
                    to_last = swap;
                }
                np->dest_point = to_last <= to_first ? last : first;
                ai->action_state = 0;
                ai->action_substate = (int)next_road->get_handle();
            }
            else
            {
                vp->vai.vai_flags.m_ignore_lanes = 1;
                np->dest_point = target_pos + player_forward * 25.0f;
                use_roads = true;
            }
        }
        np->dest_cell_hint_handle = 0xFFFFFFFFFFFFFFFFull;
    }
    const vector dest = np->dest_point;
    np->npc_flags.can_use_special_roads = 1;
    const vector to_dest = dest - vp->pos;
    // Not within sqrt(10) m; a failure is forgiven at once and a new path tried in 2 s.
    if (10.0f <= to_dest.mag_squared() && ai_vehicle_travel_to_a_point_do_frame(np, vp, dest, np->dest_cell_hint_handle, use_roads, 3.0f, true, false, false, false) == AI_RESULT_FAILURE)
    {
        vp->disable_for_npc_drivers_timer.set(0);
        vp->pf_failure_timeout.set(0);
        vp->vai.navmesh_path_update.set(2000);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005F5950 (:5838)
int get_back_on_navmesh_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 1;
}

// 0x00647310 (:5843)
ai_results get_back_on_navmesh_action::enter(npc* np, const ai_action* last_action) const
{
    if (np->vehicle_handle == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    object* objp = World.object_from_handle(np->vehicle_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE || !human_is_driving(np))
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = static_cast<vehicle*>(objp);
    if (vp->vai.vai_flags.fallen_off_navmesh == 0)
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        np->get_ai()->action_timer.set_rand(0, 1000);
        np->dest_point = vp->pos;
        np->pf.last_dest_check_pos = vp->pos;
        np->pf.dest_check_timer.invalidate();
        vehicle_ai_free_all_paths(&vp->vai);
        vehicle_ai_flush_route(vp->vai.route_info);
    }
    return AI_RESULT_SUCCESS;
}

// The straight path of 2 points to dest, as ai_vehicle_pathfind_to starts one.
static void get_back_on_navmesh_start_path(vehicle* vp, const vector& dest, uint64_t dest_hint)
{
    pf_calc_point points[2] = {pf_calc_point(vp->pos, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF), pf_calc_point(dest, dest_hint, 0xFFFFFFFF)};
    vehicle_path_info path_info;
    navmesh_lane_spline_node* path = vehicle_generate_direct_path_between_points(2, points, path_info, vp);
    vehicle_ai_free_all_paths(&vp->vai);
    vehicle_navmesh_common_init_path_follow(&vp->vai, path);
    vp->vai.vai_flags.m_jumping = 0;
    vp->vai.current_path_info = path_info;
    if (path_info.flags.jumping != 0)
    {
        vp->vai.vai_flags.m_jumping = 1;
    }
    vp->vai.navmesh_path_update.set_rand(500, 1000);
}

// 0x006B3D10 (:5876): drives straight at the nav mesh: 10 m past the nearest edge of the last cell when within 10 m of it, else along
// the first of backwards, forwards, left and right that reaches a usable cell in 30 m, else to the nearest cell within 50 m. Not moving 2 m
// in 5 s (or nothing found) sets the vehicle on fire, unless the player is in it.
ai_results get_back_on_navmesh_action::do_action(npc* np) const
{
    if (np->vehicle_handle == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    object* objp = World.object_from_handle(np->vehicle_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE || !human_is_driving(np))
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = static_cast<vehicle*>(objp);
    ai_vehicle_use_turret_if_available(np, vp, false, false, nullptr, ZERO_VECTOR);
    if (ai_vehicle_should_stop_to_let_passenger_out(vp) && !vehicle_interaction_passengers_stuck(vp))
    {
        vehicle_set_controls_when_abandoned(vp);
        return AI_RESULT_IN_PROGRESS;
    }
    ai_system* ai = np->get_ai();
    if (vehicle_is_on_fire(vp))
    {
        vp->vai.vai_flags.fallen_off_navmesh = 0;
        ai->planner.request_replan();
    }
    unsigned int bridge_navpoint;
    if (is_vehicle_on_bridge(vp, bridge_navpoint))
    {
        vp->vai.vai_flags.fallen_off_navmesh = 0;
        return AI_RESULT_SUCCESS;
    }
    if (vp->vai.vai_flags.fallen_off_navmesh == 0)
    {
        return AI_RESULT_SUCCESS;
    }
    const nav_cell* last_cell = nav_cell_from_handle(vp->m_navcell_info.last_visited_nav_cell_handle);
    if (last_cell != nullptr && nav_cell_is_point_in_cell_column(last_cell, vp->pos))
    {
        vp->vai.vai_flags.fallen_off_navmesh = 0;
        return AI_RESULT_SUCCESS;
    }
    if (ai->action_timer.valid() && ai->action_timer.elapsed())
    {
        uint64_t handle;
        if (bb_pf_get_nav_cell(vp->pos, true, &handle, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr) != nullptr)
        {
            vp->vai.vai_flags.fallen_off_navmesh = 0;
            vp->m_navcell_info.last_visited_nav_cell_handle = handle;
            return AI_RESULT_SUCCESS;
        }
    }
    bool not_moving = false;
    if (np->pf.dest_check_timer.valid() && np->pf.dest_check_timer.elapsed())
    {
        ai_update_combat_target_building(np);
        const float moved_sqrd = vec_dist_squared(np->pf.last_dest_check_pos, vp->pos);
        if (4.0f <= moved_sqrd)
        {
            np->pf.last_dest_check_pos = vp->pos;
        }
        else
        {
            vehicle_ai_free_all_paths(&vp->vai);
        }
        not_moving = moved_sqrd < 4.0f;
        np->pf.dest_check_timer.set(5000);
    }
    if (vp->vai.route_info.num_route_segments != 0)
    {
        if (ai->action_timer.valid() && ai->action_timer.elapsed())
        {
            ai->action_timer.set_rand(2000, 3000);
        }
        const vector* knots = vp->vai.route_info.route[0].cb_spline.knots;
        if (vec_dist_squared(knots[0], knots[1]) < 1e-10f)
        {
            vehicle_ai_free_all_paths(&vp->vai);
            vehicle_ai_flush_route(vp->vai.route_info);
            vp->vai.vai_flags.m_jumping = 0;
            return AI_RESULT_IN_PROGRESS;
        }
        if (vehicle_ai_process_stuck(vp, true))
        {
            return AI_RESULT_IN_PROGRESS;
        }
        vehicle_navmesh_common_follow_path(vp);
    }
    else
    {
        if (ai->action_timer.valid() && !ai->action_timer.elapsed())
        {
            return AI_RESULT_IN_PROGRESS;
        }
        ai->action_timer.set_rand(4000, 6000);
        bool new_path = false;
        vector dest;
        uint64_t dest_hint = 0xFFFFFFFFFFFFFFFFull;
        const vector& pos = vp->pos;
        bool search_cells = last_cell == nullptr;
        if (last_cell != nullptr)
        {
            if (nav_cell_is_point_in_cell_column(last_cell, pos))
            {
                vp->vai.vai_flags.fallen_off_navmesh = 0;
                return AI_RESULT_SUCCESS;
            }
            const vector2 flat_pos(pos.x, pos.z);
            float best = -1.0f;
            vector edge_point = ZERO_VECTOR;
            for (int edge = 0; edge < 3; edge++)
            {
                vector vert0;
                vector vert1;
                nav_cell_get_edge_verts(&vert0, &vert1, last_cell, edge, nullptr);
                vector2 closest;
                const float edge_dist = vec2_find_closest_dist_squared_to_linesegment(closest, flat_pos, vector2(vert0.x, vert0.z), vector2(vert1.x, vert1.z));
                if (best < 0.0f || edge_dist < best)
                {
                    edge_point.x = closest.x;
                    edge_point.z = closest.y;
                    best = edge_dist;
                }
            }
            havok_terrain_height(edge_point.y, edge_point.x, edge_point.z, false, nullptr);
            if (100.0f <= best)
            {
                search_cells = true;
            }
            else
            {
                vector to_edge = edge_point - pos;
                const float dist = to_edge.flatten_mag_and_normalize_safe();
                dest = pos + to_edge * (dist + 10.0f);
                havok_terrain_height(dest.y, dest.x, dest.z, false, nullptr);
                dest_hint = vp->m_navcell_info.last_visited_nav_cell_handle;
                new_path = true;
            }
        }
        if (search_cells)
        {
            uint64_t handle = 0xFFFFFFFFFFFFFFFFull;
            nav_cell* cell = bb_pf_get_nav_cell(pos, true, &handle, false, NAV_CELL_TRAVERSAL_ALL, nullptr);
            nav_cell* found_cell = nullptr;
            vector found_pos;
            if (cell != nullptr)
            {
                if ((cell->flags & (NAV_CELL_FLAG_DISABLED | NAV_CELL_FLAG_LIGHT)) == 0)
                {
                    vp->m_navcell_info.last_visited_nav_cell_handle = handle;
                    return AI_RESULT_SUCCESS;
                }
                const vector directions[4] = {-vp->orient.fvec, vp->orient.fvec, -vp->orient.rvec, vp->orient.rvec};
                for (int i = 0; i < 4 && found_cell == nullptr; i++)
                {
                    const vector end = pos + directions[i] * 30.0f;
                    found_cell = find_first_valid_nav_cell_along_path(pos, end, cell, found_pos);
                }
            }
            if (found_cell == nullptr)
            {
                if (!rfg_pf_find_closest_point_on_nav_mesh_using_collect(pos, true, found_pos, found_cell, 50.0f))
                {
                    not_moving = true;
                }
            }
            if (found_cell != nullptr)
            {
                vector to_cell = found_pos - pos;
                const float dist = to_cell.flatten_mag_and_normalize_safe();
                dest = pos + to_cell * (dist + 10.0f);
                dest_hint = nav_cell_get_handle(found_cell);
                havok_terrain_height(dest.y, dest.x, dest.z, false, nullptr);
                new_path = true;
            }
        }
        if (new_path)
        {
            get_back_on_navmesh_start_path(vp, dest, dest_hint);
            np->dest_point = dest;
            np->dest_cell_hint_handle = dest_hint;
            np->pf.dest_check_timer.set(5000);
            np->pf.last_dest_check_pos = vp->pos;
        }
    }
    if (not_moving && !vehicle_is_on_fire(vp))
    {
        const player* pl = player_get_local_player();
        if (pl == nullptr || !vehicle_human_in_vehicle(vp, pl))
        {
            vehicle_catch_fire(vp, false);
            vp->vai.vai_flags.fallen_off_navmesh = 0;
            ai->planner.request_replan();
        }
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x006473E0 (:6207)
ai_results get_back_on_navmesh_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this && np->vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(np->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            vehicle* vp = static_cast<vehicle*>(objp);
            vp->vai.vai_flags.fallen_off_navmesh = 0;
            vp->disable_for_npc_drivers_timer.set(0);
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F5960 (:6226)
int pull_off_road_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 1;
}

// 0x00647430 (:6231)
ai_results pull_off_road_action::enter(npc* np, const ai_action* last_action) const
{
    if (np->vehicle_handle == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    object* objp = World.object_from_handle(np->vehicle_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE || !human_is_driving(np))
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = static_cast<vehicle*>(objp);
    if (vp->vai.vai_flags.pull_off == 0 || vp->vai.pull_off_timer.elapsed())
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        vehicle_ai_free_all_paths(&vp->vai);
        vehicle_ai_flush_route(vp->vai.route_info);
    }
    return AI_RESULT_SUCCESS;
}

// The point on the road's edge for pull_off_road_action: param moved step along the road, past the end of the road by the rest times overshoot_scale.
static vector pull_off_side_point(const object_path_road* road, bool with_road, float step, float overshoot_scale, float& param, float& overshoot, bool& past_end, float& width)
{
    if (with_road)
    {
        param += step;
        const float last = (float)(road->spline_data.num_points - 1);
        if (last < param)
        {
            overshoot += param - last;
            param = last;
            past_end = true;
        }
    }
    else
    {
        param -= step;
        if (param < 0.0f)
        {
            overshoot -= param;
            param = 0.0f;
            past_end = true;
        }
    }
    vector point;
    vector tangent;
    road->get_point(point, param);
    road->get_tangent(tangent, param);
    if (past_end)
    {
        point += tangent * (overshoot * overshoot_scale);
    }
    tangent.flatten_normalize_safe();
    width = road->get_width(param);
    return point;
}

// 0x006703B0 (:6258): the road is wanted clear for the rail vehicle: leaves it, or stands where it is, until pull_off_timer runs out
// and the rail vehicle is over 20 m away.
ai_results pull_off_road_action::do_action(npc* np) const
{
    if (np->vehicle_handle == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = static_cast<vehicle*>(World.object_from_handle(np->vehicle_handle));
    if (vp == nullptr || vp->get_obj_type() != OT_VEHICLE || !human_is_driving(np))
    {
        return AI_RESULT_FAILURE;
    }
    vehicle_ai& vai = vp->vai;
    if (vai.vai_flags.pull_off == 0)
    {
        return AI_RESULT_SUCCESS;
    }
    if (vai.pull_off_timer.elapsed())
    {
        unsigned int patrol_handle;
        if (!vrail_any_ready() || !vrail_get_patrol_vehicle(patrol_handle))
        {
            return AI_RESULT_SUCCESS;
        }
        const vehicle* patrol_vehicle = World.object_from_handle<vehicle>(patrol_handle);
        if (patrol_vehicle == nullptr || 400.0f < vec_xz_dist_squared(vp->pos, patrol_vehicle->pos))
        {
            return AI_RESULT_SUCCESS;
        }
        vai.pull_off_timer.set(1000);
    }
    if (vai.ambient_wait_timer.valid())
    {
        if (!vai.ambient_wait_timer.elapsed())
        {
            vehicle_set_controls_when_abandoned(vp);
            return AI_RESULT_IN_PROGRESS;
        }
        vai.ambient_wait_timer.invalidate();
        vai.stuck.stuck_threshold_time.invalidate();
        vai.stuck.extended_stuck_time.invalidate();
    }
    unsigned int bridge_navpoint;
    const bool on_bridge = is_vehicle_on_bridge(vp, bridge_navpoint);
    vector closest = ZERO_VECTOR;
    float road_u = 0.0f;
    const vector& pos = vp->pos;
    object_path_road* road = object_road_get_closest_road(pos, np->npc_flags.can_use_special_roads != 0, closest, road_u, 60.0f, false, nullptr, nullptr, false);
    if (road == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const float dist_to_road = vec_dist(pos, closest);
    const float road_width = road->get_width(road_u);
    vector bmin;
    vector bmax;
    vp->get_local_bbox(bmin, bmax);
    const float vehicle_width = bmax.x - bmin.x;
    const float half_vehicle_width = vehicle_width * 0.5f;
    if (road_width + half_vehicle_width < dist_to_road)
    {
        // Off the road: wait.
        vehicle_set_controls_when_abandoned(vp);
        return AI_RESULT_IN_PROGRESS;
    }
    if (vai.route_info.num_route_segments != 0 && (vai.current_path_info.flags.on_bridge == 0 || on_bridge))
    {
        vehicle_ai_set_speed_override(&vai, 15.0f);
        if (!vehicle_ai_process_stuck(vp, true))
        {
            vehicle_navmesh_common_follow_path(vp);
        }
        return AI_RESULT_IN_PROGRESS;
    }
    vector tangent;
    road->get_tangent(tangent, road_u);
    const bool with_road = 0.0f < tangent * vp->orient.fvec;
    if (on_bridge)
    {
        // Can't leave a bridge: a lane of one segment along the road, kept to its side by a road offset.
        int to_index;
        int from_index;
        if (!with_road)
        {
            to_index = (int)ceil((double)(road_u - 1.25f));
            if (to_index < 0)
            {
                to_index = 0;
            }
            from_index = to_index + 1;
        }
        else
        {
            to_index = (int)floor((double)(road_u + 1.25f));
            if (road->spline_data.num_points - 1 < to_index)
            {
                to_index = road->spline_data.num_points - 1;
            }
            from_index = to_index - 1;
        }
        vehicle_path_info path_info;
        navmesh_lane_spline_node* node = get_free_ls_node();
        const road_spline_point* points = road->spline_data.points.to_pointer();
        vector* knots = node->lane.knots.to_pointer();
        vector* in_handles = node->lane.in_handles.to_pointer();
        vector* out_handles = node->lane.out_handles.to_pointer();
        knots[0] = points[from_index].pos;
        knots[1] = points[to_index].pos;
        node->lane.num_knots = 2;
        if (to_index < from_index)
        {
            in_handles[1] = knots[1] + points[to_index].pt_handle;
            out_handles[0] = knots[0] - points[from_index].pt_handle;
        }
        else
        {
            in_handles[1] = knots[1] - points[to_index].pt_handle;
            out_handles[0] = knots[0] + points[from_index].pt_handle;
        }
        cubic_bezier_spline spline;
        spline.set_points(2, knots, in_handles, out_handles);
        const float first_half = spline.get_length_approx(5, 0.0f, 0.5f);
        const float second_half = spline.get_length_approx(5, 0.5f, 1.0f);
        node->next = node;
        node->prev = node;
        // 0x80: a spline made here, not one of a road.
        node->lane.spline_flags = 0x80;
        node->lane.arc_length[0] = first_half + second_half;
        node->lane.arc_length[1] = first_half;
        path_info.start_pos = pos;
        path_info.flags.ixc_can_control = 0;
        path_info.start_nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
        path_info.end_pos = points[to_index].pos;
        path_info.flags.jumping = 0;
        path_info.flags.partial_path = 0;
        path_info.flags.off_road = 0;
        path_info.flags.on_bridge = 1;
        path_info.flags.road_spline_path = 1;
        path_info.flags.use_road_offset = 1;
        path_info.end_nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
        if (road->road_flags.one_way == 0)
        {
            vehicle_ai_set_road_offset(&vai, road_width <= vehicle_width ? road_width - half_vehicle_width : road_width * 0.5f);
        }
        else
        {
            vehicle_ai_clear_road_offset(&vai);
        }
        vehicle_ai_free_all_paths(&vai);
        vehicle_navmesh_common_init_path_follow(&vai, node);
        vai.vai_flags.m_jumping = 0;
        vai.current_path_info = path_info;
        if (path_info.flags.jumping != 0)
        {
            vai.vai_flags.m_jumping = 1;
        }
        return AI_RESULT_IN_PROGRESS;
    }
    // Three points along the road: a quarter segment on to the edge, 0.1 further to just beyond it, a quarter further on off the road.
    const float side_sign = with_road ? -1.0f : 1.0f;
    float param = road_u;
    float overshoot = 0.0f;
    bool past_end = false;
    float width;
    vector side_point = pull_off_side_point(road, with_road, 0.25f, 25.0f, param, overshoot, past_end, width);
    vector side_tangent;
    road->get_tangent(side_tangent, param);
    side_tangent.flatten_normalize_safe();
    side_point += (side_tangent ^ UP_VECTOR) * side_sign * (road_width <= vehicle_width ? width - half_vehicle_width : width * 0.5f);
    vector off_point = pull_off_side_point(road, with_road, 0.1f, 10.0f, param, overshoot, past_end, width);
    road->get_tangent(side_tangent, param);
    side_tangent.flatten_normalize_safe();
    off_point += (side_tangent ^ UP_VECTOR) * side_sign * (width + half_vehicle_width + 1.0f);
    vector end_point = pull_off_side_point(road, with_road, 0.25f, 25.0f, param, overshoot, past_end, width);
    road->get_tangent(side_tangent, param);
    side_tangent.flatten_normalize_safe();
    end_point += (side_tangent ^ UP_VECTOR) * side_sign * (width + half_vehicle_width + 1.0f);
    pf_calc_point points[4] = {pf_calc_point(pos, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF), pf_calc_point(side_point, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF),
                               pf_calc_point(off_point, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF), pf_calc_point(end_point, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF)};
    vehicle_path_info path_info;
    navmesh_lane_spline_node* path = vehicle_generate_direct_path_between_points(4, points, path_info, vp);
    vehicle_ai_free_all_paths(&vai);
    vehicle_navmesh_common_init_path_follow(&vai, path);
    vai.vai_flags.m_jumping = 0;
    vai.current_path_info = path_info;
    if (path_info.flags.jumping != 0)
    {
        vai.vai_flags.m_jumping = 1;
    }
    vai.navmesh_path_update.set_rand(500, 1000);
    np->dest_point = end_point;
    np->pf.dest_check_timer.set(5000);
    np->pf.last_dest_check_pos = pos;
    return AI_RESULT_IN_PROGRESS;
}

// 0x006474C0 (:6620)
ai_results pull_off_road_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this && np->vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(np->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            vehicle* vp = static_cast<vehicle*>(objp);
            vp->vai.vai_flags.pull_off = 0;
            vp->vai.pull_off_timer.invalidate();
        }
    }
    return AI_RESULT_SUCCESS;
}

// The rest, constructors and macro get_cost overloads, from tools/headless/ai_action_gen.py.

goto_vehicle_action goto_vehicle_action::the_instance; // 0x01DDAEA4

// 0x00622980 (.h:13)
goto_vehicle_action::goto_vehicle_action()
    : ai_action("goto vehicle", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({48}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* goto_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005E4070
goto_vehicle_action::~goto_vehicle_action()
{
}

// 0x00622C20 (.h:13)
int goto_vehicle_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

enter_vehicle_action enter_vehicle_action::the_instance; // 0x01D684AC

// 0x00622C50 (.h:14)
enter_vehicle_action::enter_vehicle_action()
    : ai_action("enter vehicle", ai_plan_conditions_from_bits({9, 47, 48, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({49}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* enter_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005E4090
enter_vehicle_action::~enter_vehicle_action()
{
}

// 0x00622F50 (.h:14)
int enter_vehicle_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

get_out_of_vehicle_action get_out_of_vehicle_action::the_instance; // 0x01DC9894

// 0x00622F90 (.h:15)
get_out_of_vehicle_action::get_out_of_vehicle_action()
    : ai_action("get out of vehicle", ai_plan_conditions(), ai_plan_conditions_from_bits({47}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* get_out_of_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005E40B0
get_out_of_vehicle_action::~get_out_of_vehicle_action()
{
}

// 0x00622FE0 (.h:15)
int get_out_of_vehicle_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

goto_with_vehicle_action goto_with_vehicle_action::the_instance; // 0x01DA0624

// 0x00623020 (.h:16)
goto_with_vehicle_action::goto_with_vehicle_action()
    : ai_action("goto with vehicle", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({50}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* goto_with_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005E40C0
goto_with_vehicle_action::~goto_with_vehicle_action()
{
}

// 0x00623080 (.h:16)
int goto_with_vehicle_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

goto_with_flyer_action goto_with_flyer_action::the_instance; // 0x01D75798

// 0x006230B0 (.h:17)
goto_with_flyer_action::goto_with_flyer_action()
    : ai_action("goto with flyer", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({50}))
{
    flags.has_enter = 1;
}

ai_action* goto_with_flyer_action::get_instance()
{
    return &the_instance;
}

// 0x005E40D0
goto_with_flyer_action::~goto_with_flyer_action()
{
}

// 0x00623110 (.h:17)
int goto_with_flyer_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

man_turret_in_vehicle_action man_turret_in_vehicle_action::the_instance; // 0x01DCA500

// 0x00623420 (.h:23)
man_turret_in_vehicle_action::man_turret_in_vehicle_action()
    : ai_action("man turret", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({46, 78}))
{
    flags.has_enter = 1;
}

ai_action* man_turret_in_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005E4130
man_turret_in_vehicle_action::~man_turret_in_vehicle_action()
{
}

// 0x006234E0 (.h:23)
int man_turret_in_vehicle_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

drive_for_manned_turret_action drive_for_manned_turret_action::the_instance; // 0x01DCA648

// 0x00623510 (.h:24)
drive_for_manned_turret_action::drive_for_manned_turret_action()
    : ai_action("drive for manned turret", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({78}))
{
    flags.has_enter = 1;
}

ai_action* drive_for_manned_turret_action::get_instance()
{
    return &the_instance;
}

// 0x005E4140
drive_for_manned_turret_action::~drive_for_manned_turret_action()
{
}

// 0x00623570 (.h:24)
int drive_for_manned_turret_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

chase_target_in_vehicle_action chase_target_in_vehicle_action::the_instance; // 0x01DDB02C

// 0x00623150 (.h:18)
chase_target_in_vehicle_action::chase_target_in_vehicle_action()
    : ai_action("chase target in vehicle", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({51}))
{
    flags.has_enter = 1;
}

ai_action* chase_target_in_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005E40E0
chase_target_in_vehicle_action::~chase_target_in_vehicle_action()
{
}

// 0x006231B0 (.h:18)
int chase_target_in_vehicle_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

chase_target_hold_position_action chase_target_hold_position_action::the_instance; // 0x01DA3E44

// 0x006231E0 (.h:19)
chase_target_hold_position_action::chase_target_hold_position_action()
    : ai_action("chase target hold pos", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({51}))
{
    flags.has_enter = 1;
}

ai_action* chase_target_hold_position_action::get_instance()
{
    return &the_instance;
}

// 0x005E40F0
chase_target_hold_position_action::~chase_target_hold_position_action()
{
}

// 0x00623240 (.h:19)
int chase_target_hold_position_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

chase_target_with_manned_turret chase_target_with_manned_turret::the_instance; // 0x01DDAFA8

// 0x00623300 (.h:21)
chase_target_with_manned_turret::chase_target_with_manned_turret()
    : ai_action("chase target manned trt", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({51}))
{
    flags.has_enter = 1;
}

ai_action* chase_target_with_manned_turret::get_instance()
{
    return &the_instance;
}

// 0x005E4110
chase_target_with_manned_turret::~chase_target_with_manned_turret()
{
}

// 0x00623360 (.h:21)
int chase_target_with_manned_turret::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

chase_target_with_fixed_turret chase_target_with_fixed_turret::the_instance; // 0x01D7500C

// 0x00623270 (.h:20)
chase_target_with_fixed_turret::chase_target_with_fixed_turret()
    : ai_action("chase target fixed trt", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({51}))
{
    flags.has_enter = 1;
}

ai_action* chase_target_with_fixed_turret::get_instance()
{
    return &the_instance;
}

// 0x005E4100
chase_target_with_fixed_turret::~chase_target_with_fixed_turret()
{
}

// 0x006232D0 (.h:20)
int chase_target_with_fixed_turret::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

chase_target_with_flying_turret chase_target_with_flying_turret::the_instance; // 0x01DDB79C

// 0x00623390 (.h:22)
chase_target_with_flying_turret::chase_target_with_flying_turret()
    : ai_action("chs tgt flying trt", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({51}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* chase_target_with_flying_turret::get_instance()
{
    return &the_instance;
}

// 0x005E4120
chase_target_with_flying_turret::~chase_target_with_flying_turret()
{
}

// 0x006233F0 (.h:22)
int chase_target_with_flying_turret::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

smash_building_action smash_building_action::the_instance; // 0x01D9F76C

// 0x00623AA0 (.h:43)
smash_building_action::smash_building_action()
    : ai_action("smash building", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({53}))
{
    flags.has_enter = 1;
}

ai_action* smash_building_action::get_instance()
{
    return &the_instance;
}

// 0x005E41E0
smash_building_action::~smash_building_action()
{
}

// 0x00623B00 (.h:43)
int smash_building_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

wait_for_passengers_action wait_for_passengers_action::the_instance; // 0x01D7237C

// 0x006238C0 (.h:40)
wait_for_passengers_action::wait_for_passengers_action()
    : ai_action("wait for passenger", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({44}))
{
    flags.has_enter = 1;
}

ai_action* wait_for_passengers_action::get_instance()
{
    return &the_instance;
}

// 0x005E41B0
wait_for_passengers_action::~wait_for_passengers_action()
{
}

// 0x00623920 (.h:40)
int wait_for_passengers_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

wait_passenger_clear_action wait_passenger_clear_action::the_instance; // 0x01D72348

// 0x00623960 (.h:41)
wait_passenger_clear_action::wait_passenger_clear_action()
    : ai_action("wait passenger clear", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({57}))
{
}

ai_action* wait_passenger_clear_action::get_instance()
{
    return &the_instance;
}

// 0x005E41C0
wait_passenger_clear_action::~wait_passenger_clear_action()
{
}

// 0x006239C0 (.h:41)
int wait_passenger_clear_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

drop_off_passengers_action drop_off_passengers_action::the_instance; // 0x01DA61DC

// 0x00623A00 (.h:42)
drop_off_passengers_action::drop_off_passengers_action()
    : ai_action("drop off passengers", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({45}))
{
    flags.has_enter = 1;
}

ai_action* drop_off_passengers_action::get_instance()
{
    return &the_instance;
}

// 0x005E41D0
drop_off_passengers_action::~drop_off_passengers_action()
{
}

// 0x00623A60 (.h:42)
int drop_off_passengers_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

acquire_visual_in_vehicle_action acquire_visual_in_vehicle_action::the_instance; // 0x01DCA858

// 0x00623B30 (.h:45)
acquire_visual_in_vehicle_action::acquire_visual_in_vehicle_action()
    : ai_action("acquire vis vehicle", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({23}))
{
    flags.has_enter = 1;
}

ai_action* acquire_visual_in_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005E41F0
acquire_visual_in_vehicle_action::~acquire_visual_in_vehicle_action()
{
}

// 0x00623B90 (.h:45)
int acquire_visual_in_vehicle_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

pwn_in_tank_action pwn_in_tank_action::the_instance; // 0x01D68080

// 0x00623BD0 (.h:46)
pwn_in_tank_action::pwn_in_tank_action()
    : ai_action("pwn in tank", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({23}))
{
    flags.has_enter = 1;
}

ai_action* pwn_in_tank_action::get_instance()
{
    return &the_instance;
}

// 0x005E4200
pwn_in_tank_action::~pwn_in_tank_action()
{
}

// 0x00623C30 (.h:46)
int pwn_in_tank_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

retreat_def_perimeter_vehicle_rule retreat_def_perimeter_vehicle_rule::the_instance; // 0x01DDADFC

// 0x00623C70 (.h:48)
retreat_def_perimeter_vehicle_rule::retreat_def_perimeter_vehicle_rule()
    : ai_production_rule("vehicle > def perimeter", ai_plan_conditions_from_bits({50}), ai_plan_conditions_from_bits({152}))
{
}

ai_action* retreat_def_perimeter_vehicle_rule::get_instance()
{
    return &the_instance;
}

// 0x005E4210
retreat_def_perimeter_vehicle_rule::~retreat_def_perimeter_vehicle_rule()
{
}

// 0x00623CE0 (.h:48)
int retreat_def_perimeter_vehicle_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

man_flyer_action man_flyer_action::the_instance; // 0x01DC9D10

// 0x006235A0 (.h:27)
man_flyer_action::man_flyer_action()
    : ai_action("man flyer action", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({55}))
{
    flags.has_enter = 1;
}

ai_action* man_flyer_action::get_instance()
{
    return &the_instance;
}

// 0x005E4150
man_flyer_action::~man_flyer_action()
{
}

// 0x00623600 (.h:27)
int man_flyer_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

loiter_flyer_action loiter_flyer_action::the_instance; // 0x01DC9C40

// 0x00623640 (.h:28)
loiter_flyer_action::loiter_flyer_action()
    : ai_action("loiter flyer action", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({23}))
{
    flags.has_enter = 1;
}

ai_action* loiter_flyer_action::get_instance()
{
    return &the_instance;
}

// 0x005E4160
loiter_flyer_action::~loiter_flyer_action()
{
}

// 0x006236A0 (.h:28)
int loiter_flyer_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

fodder_action fodder_action::the_instance; // 0x01DDAF0C

// 0x00623820 (.h:37)
fodder_action::fodder_action()
    : ai_action("fodder action", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({42}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* fodder_action::get_instance()
{
    return &the_instance;
}

// 0x005E4190
fodder_action::~fodder_action()
{
}

// 0x00623880 (.h:37)
int fodder_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

get_back_on_navmesh_action get_back_on_navmesh_action::the_instance; // 0x01DC9930

// 0x006236E0 (.h:31)
get_back_on_navmesh_action::get_back_on_navmesh_action()
    : ai_action("get back on navmesh", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({66}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* get_back_on_navmesh_action::get_instance()
{
    return &the_instance;
}

// 0x005E4170
get_back_on_navmesh_action::~get_back_on_navmesh_action()
{
}

// 0x00623740 (.h:31)
int get_back_on_navmesh_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

pull_off_road_action pull_off_road_action::the_instance; // 0x01DCA050

// 0x00623780 (.h:34)
pull_off_road_action::pull_off_road_action()
    : ai_action("pull off road", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({56}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* pull_off_road_action::get_instance()
{
    return &the_instance;
}

// 0x005E4180
pull_off_road_action::~pull_off_road_action()
{
}

// 0x006237E0 (.h:34)
int pull_off_road_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

