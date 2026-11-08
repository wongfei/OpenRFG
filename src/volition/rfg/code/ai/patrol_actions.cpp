// Original: volition/rfg/code/ai/patrol_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/patrol_actions.h"

#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/alerts.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"
#include "volition/rfg/code/gameplay/activities/activity_courier_chase.h"
#include "volition/rfg/code/gameplay/activities/activity_courier_chase_internal.h"
#include "volition/rfg/code/gameplay/activities/vrail_system.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_convoy.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_patrol.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/scripting/action_script/global_event.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_flyer.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/rand.h"

#include <math.h>

// Position 26 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG26")

// Conditions the vehicle actions ask of the plan: 48 and 49 reach the vehicle, 50 the rail vehicle (from their uses).
static const unsigned int COND_REACH_VEHICLE = 48;
static const unsigned int COND_IN_VEHICLE = 49;
static const unsigned int COND_REACH_RAIL_POS = 50;

// 0x005DBE50 (global_event_data.h:250)
patrol_squad_reached_node_event::patrol_squad_reached_node_event()
    : event_data(EVENT_PATROL_SQUAD_REACHED_NODE)
{
}

// 0x005ECC10 (:113): the convoy waits for its stragglers. From the asm, the decompile hides the x87 results: stops when
// squads next to each other are more than 50 m apart, half speed from 30 m.
float select_convoy_speed(const object_convoy* convoy)
{
    const float speed = convoy->get_move_speed();
    if (convoy->flags.attacked)
    {
        return speed;
    }
    int index = 0;
    const object_squad* last = nullptr;
    for (; index < 12; index++) // 0xC member squads
    {
        last = convoy->get_member_squad(index);
        if (last != nullptr)
        {
            break;
        }
    }
    if (last == nullptr)
    {
        return speed;
    }
    float max_gap_sq = 0.0f;
    for (index++; index < 12; index++)
    {
        const object_squad* squad = convoy->get_member_squad(index);
        if (squad != nullptr)
        {
            const float gap_sq = vec_dist_squared(last->pos, squad->pos);
            if (gap_sq > max_gap_sq)
            {
                max_gap_sq = gap_sq;
            }
            last = squad;
        }
    }
    if (max_gap_sq > 2500.0f)
    {
        return 0.0f;
    }
    if (max_gap_sq > 900.0f)
    {
        return speed * 0.5f;
    }
    return speed;
}

// 0x005ECD10 (:180)
int patrol_check_in_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (human_teams_get_disposition(np, HUMAN_TEAM_EDF) != TEAM_DISPOSITION_ALLIED || activity_cc_is_courier(np))
    {
        return -1;
    }
    if (goal_setup_data.selected_patrol == nullptr || goal_setup_data.selected_patrol->patrol_flags.no_check_in != 0 || human_in_vehicle(np) != nullptr)
    {
        return -1;
    }
    const ai_system* ai = np->get_ai();
    if (ai->get_district_control_level() <= 0)
    {
        return -1;
    }
    const ai_action* current = ai->planner.get_current_action();
    if (current != this && current != default_patrol_action::get_instance())
    {
        return -1;
    }
    return ai->action_timer.valid() && ai->action_timer.elapsed() ? 750 : -1;
}

// 0x00664090 (:221)
ai_results patrol_check_in_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        ai->action_timer.set(0);
        ai->stop_moving();
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        np->char_inst->play_action(ANIMATION_ACTION_RADIO_CHECK_IN, ANIM_ALIAS_NONE, 1.0f, 0);
        np->play_voice_line(VOICE_LINE_PATROL_CHECK_IN, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005ECDC0 (:241)
ai_results patrol_check_in_action::do_action(npc* np) const
{
    if (!np->char_inst->is_playing_action(ANIMATION_ACTION_RADIO_CHECK_IN) && np->get_current_voice_line() != VOICE_LINE_PATROL_CHECK_IN)
    {
        np->get_ai()->action_timer.invalidate();
        return AI_RESULT_SUCCESS;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x0063D460 (:255)
ai_results patrol_check_in_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        if (np->char_inst->rdd.is_loaded())
        {
            np->char_inst->stop_action(ANIMATION_ACTION_RADIO_CHECK_IN, 5, true);
        }
        if (np->get_current_voice_line() == VOICE_LINE_PATROL_CHECK_IN)
        {
            np->stop_voice(VOICE_LINE_PRIORITY_HIGH);
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x005ECE10 (:272)
int default_patrol_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (goal_setup_data.selected_patrol == nullptr)
    {
        return -1;
    }
    return activity_cc_is_courier(np) ? -1 : 1000; // (-x & 0xFFFFFC17) + 1000
}

// 0x00664130 (:288)
ai_results default_patrol_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const object_patrol* patrol = World.object_from_handle<object_patrol>(ai->desired_destination_object);
    if (patrol == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai->perception.set_attention_mode(ATTENTION_SEARCH, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    if (last_action != this)
    {
        ai->action_timer.set_rand(12000, 20000);
        const patrol_node_handle* node = ai->get_patrol_node();
        if (node == nullptr)
        {
            return AI_RESULT_FAILURE;
        }
        if (!patrol->is_member(*node))
        {
            if (!ai->has_patrol_type_orders())
            {
                return AI_RESULT_FAILURE;
            }
            const patrol_node_handle next = patrol->get_next_patrol_point_handle(np->pos);
            ai->update_patrol_node(&next);
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x006A51D0 (:331)
ai_results default_patrol_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const object_patrol* patrol = World.object_from_handle<object_patrol>(ai->desired_destination_object);
    if (patrol == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (patrol->patrol_flags.is_paused != 0)
    {
        ai->stop_moving();
        return AI_RESULT_IN_PROGRESS;
    }
    human_set_combat_ready(np, patrol->patrol_flags.combat_ready != 0, 0);
    const patrol_node_handle* node = ai->get_patrol_node();
    vector node_pos;
    if (node == nullptr || !patrol->get_node_pos(*node, node_pos))
    {
        return AI_RESULT_FAILURE;
    }
    // near_patrol_point (0x005ECBB0, :91) inlined: 2 m across and 1.5 m in height.
    if (vec_xz_dist_squared(np->pos, node_pos) > 4.0f || fabsf(np->pos.y - node_pos.y) > 1.5f)
    {
        if (!ai->is_pathing_to(node_pos, 0.25f, 1.0f))
        {
            const human_movement_states m_state = np->current_team == HUMAN_TEAM_MARAUDER ? HMS_RUN : patrol->get_foot_speed(*node);
            if (ai_pf_to_result(ai->pathfind_to(node_pos, m_state, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false)) == AI_RESULT_FAILURE)
            {
                return AI_RESULT_FAILURE;
            }
        }
    }
    else
    {
        patrol_squad_reached_node_event event;
        event.squad_handle = ai->get_squad_handle();
        event.patrol_node = node;
        gem_post_event(event);
        if (!ai->has_patrol_type_orders())
        {
            return AI_RESULT_FAILURE;
        }
        const patrol_node_handle next = patrol->get_next_patrol_point_handle(*node, np->pos);
        ai->update_patrol_node(&next);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x006A53A0 (:411)
int lead_convoy_patrol_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    const object_squad* squad = np->get_ai()->get_squad();
    if (squad == nullptr || squad->get_leader() != np || goal_setup_data.selected_convoy == nullptr)
    {
        return -1;
    }
    vehicle* vp = squad->get_vehicle();
    if (vp == nullptr || !ai_vehicle_is_usable_and_available(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER))
    {
        return -1;
    }
    if (vehicle_passenger_in_seat(vp, VSI_DRIVER) != np)
    {
        desired_conditions |= ai_plan_conditions(COND_REACH_VEHICLE) | ai_plan_conditions(COND_IN_VEHICLE);
    }
    my_data.use_target = vp;
    return 500;
}

// 0x005ECE50 (:447)
void lead_convoy_patrol_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_vehicle = setup_data.use_target->get_handle();
    ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_DRIVER;
}

// 0x006803E0 (:455)
ai_results lead_convoy_patrol_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || vehicle_passenger_in_entering_or_reserving_seat(vp, VSI_DRIVER) != np)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
    if (vehicle_passenger_in_seat(vp, VSI_DRIVER) != np)
    {
        return AI_RESULT_FAILURE;
    }
    const object_convoy* convoy = ai->get_convoy_object();
    if (convoy == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr || squad->get_leader() != np)
    {
        return AI_RESULT_FAILURE;
    }
    ai_vehicle_ensure_seat_turret_manned(np, vp, nullptr);
    const float speed = select_convoy_speed(convoy);
    if (speed <= 0.0f)
    {
        vehicle_set_controls_when_abandoned(vp);
    }
    else
    {
        ai_vehicle_override_speed(vp, speed);
    }
    return AI_RESULT_SUCCESS;
}

// 0x006B7480 (:506)
ai_results lead_convoy_patrol_action::do_action(npc* np) const
{
    object_convoy* convoy = np->get_ai()->get_convoy_object();
    if (convoy == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    vector dest;
    if (!convoy->get_next_destination(np->pos, dest))
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || vehicle_passenger_in_seat(vp, VSI_DRIVER) != np)
    {
        return AI_RESULT_FAILURE;
    }
    ai_vehicle_use_turret_if_available(np, vp, false, false, nullptr, ZERO_VECTOR);
    ai_results result = AI_RESULT_IN_PROGRESS;
    const float speed = select_convoy_speed(convoy);
    if (speed > 0.0f)
    {
        ai_vehicle_override_speed(vp, speed);
        // From the asm: the decompile shifts these one to the left. 10 m is the arrival distance.
        result = ai_vehicle_travel_to_a_point_do_frame(np, vp, dest, 0xFFFFFFFFFFFFFFFFull, true, 10.0f, false, false, false, true);
    }
    else
    {
        vehicle_set_controls_when_abandoned(vp);
    }
    return convoy->is_at_final_destination() ? AI_RESULT_SUCCESS : result;
}

// 0x0063D4B0 (:574). LTCG: np EAX.
static void maybe_update_courier_patrol_speed(const npc* np, vehicle* vp)
{
    const object_squad* squad = np->get_ai()->get_squad();
    if (squad == nullptr)
    {
        return;
    }
    const activity_courier_chase_data* courier = squad->get_courier_data();
    if (courier == nullptr)
    {
        return;
    }
    const float speed = courier->get_move_speed();
    // From the asm: byte vai + 0xD bit 1.
    if (!vp->vai.vai_flags.m_jumping)
    {
        if (speed <= 0.0f)
        {
            vehicle_ai_clear_speed_override(&vp->vai);
        }
        else
        {
            vehicle_ai_set_speed_override(&vp->vai, speed);
        }
    }
}

// 0x006A54C0 (:585)
int patrol_in_vehicle_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (goal_setup_data.selected_patrol == nullptr)
    {
        return -1;
    }
    const object_squad* squad = np->get_ai()->get_squad();
    if (squad == nullptr)
    {
        return -1;
    }
    vehicle* vp = squad->get_vehicle();
    if (vp == nullptr || !ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER) || !ai_vehicle_is_seat_available(vp, np, VSI_DRIVER))
    {
        return -1;
    }
    my_data.use_target = vp;
    const vehicle* current = human_in_vehicle(np);
    if (current != nullptr && vp != current)
    {
        desired_conditions |= ai_plan_conditions(COND_REACH_VEHICLE) | ai_plan_conditions(COND_IN_VEHICLE);
    }
    return 500;
}

// 0x005ECE80 (:614)
void patrol_in_vehicle_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_vehicle = setup_data.use_target->get_handle();
    ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_DRIVER;
}

// 0x00664330 (:621)
ai_results patrol_in_vehicle_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
    const object_patrol* patrol = World.object_from_handle<object_patrol>(ai->desired_destination_object);
    if (patrol == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (patrol->patrol_flags.override_patrol != 0)
    {
        vehicle_ai* vai = vehicle_get_vehicle_ai(vp->get_handle());
        if (vai != nullptr)
        {
            vai->vai_flags.m_should_not_swerve = 1;
        }
    }
    if (np->vehicle_handle == vp->get_handle() && np->npc_flags.no_turret_attacks == 0)
    {
        turret* tp = vehicle_get_seat_turret(vp, np->vehicle_seat_idx);
        if (tp != nullptr && !tp->turret_flags.locked_turret)
        {
            human_combat_man_turret(np, tp);
        }
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
    {
        flyer* fp = static_cast<flyer*>(vp);
        fp->clear_requested_lookat_vector();
        if (last_action != this)
        {
            fp->flyer_flags.stay_at_constant_height = 1;
            float terrain_y = 0.0f;
            havok_terrain_height(terrain_y, vp->pos.x, vp->pos.z, false, nullptr);
            vehicle_ai* vai = vehicle_get_vehicle_ai(vp->get_handle());
            if (vai != nullptr)
            {
                float offset = vai->original_offset;
                if (offset - 20.0f < 0.0f)
                {
                    offset = 20.0f;
                }
                vai->desired_flying_offset = offset + terrain_y;
            }
            ai->action_timer.invalidate();
        }
    }
    if (last_action != this)
    {
        const patrol_node_handle* node = ai->get_patrol_node();
        if (node == nullptr)
        {
            return AI_RESULT_FAILURE;
        }
        if (activity_cc_is_courier(np))
        {
            vp->vai.vai_flags.is_courier = 1;
        }
        if (vrail_is_riding_shotgun_driver(np))
        {
            vp->vai.vai_flags.is_riding_shotgun = 1;
        }
        if (patrol->is_member(*node))
        {
            ai->update_patrol_speed(vp, node, patrol);
            ai->update_road_usage(vp, node);
        }
        else
        {
            if (!ai->has_patrol_type_orders())
            {
                return AI_RESULT_FAILURE;
            }
            const patrol_node_handle next = patrol->get_next_patrol_point_handle(np->pos);
            ai->update_patrol_node(&next);
            ai->update_patrol_speed(vp, &next, patrol);
            ai->update_road_usage(vp, &next);
            ai->desired_target_recalc_timestamp.invalidate();
        }
    }
    if (activity_cc_is_courier(np))
    {
        maybe_update_courier_patrol_speed(np, vp);
    }
    return AI_RESULT_SUCCESS;
}

// A random point on a building being attacked by a patrol ordered to (override_patrol), inlined in do_action.
vector pick_building_target_pos(rfg_mover* building)
{
    vector target = ZERO_VECTOR;
    rfg_destroyable_shape* shape = building->get_shape();
    if (shape == nullptr)
    {
        vector bmin;
        vector bmax;
        building->get_local_bbox(bmin, bmax);
        // One of the eight octants of the box. Index 8 can come up and falls in the last one but for x, y and z tests below.
        const int octant = int_rand_range(0, 8);
        target.x = octant == 0 || octant == 1 || octant == 4 || octant == 5 ? fl_rand_range(bmin.x, 0.0f) : fl_rand_range(0.0f, bmax.x);
        target.y = octant < 4 ? fl_rand_range(bmin.y, 0.0f) : fl_rand_range(0.0f, bmax.y);
        target.z = octant == 0 || octant == 2 || octant == 4 || octant == 6 ? fl_rand_range(bmin.z, 0.0f) : fl_rand_range(0.0f, bmax.z);
        // Original bug kept: the terrain is read at the box-local x and z, before the building's position is added.
        float terrain_y = 0.0f;
        havok_terrain_height(terrain_y, target.x, target.z, false, nullptr);
        if (target.y < terrain_y)
        {
            target.y = terrain_y + 0.5f;
        }
        target.y += building->pos.y;
        target.x += building->pos.x;
        target.z += building->pos.z;
        return target;
    }

    const int pick = int_rand_range(0, shape->num_existing_child_shapes - 1);
    const rfg_subpiece* piece = nullptr;
    const unsigned short first = shape->child_obj_list;
    if (first != 0xFFFF)
    {
        int index = 0;
        unsigned short current = first;
        do
        {
            piece = shape->objects + current;
            if (index == pick)
            {
                break;
            }
            current = piece->next;
            piece = nullptr;
            index++;
        } while (current != first);
    }
    matrix orient;
    shape->get_subpiece_pos_orient_uncached(piece, target, orient);
    float terrain_y = 0.0f;
    havok_terrain_height(terrain_y, target.x, target.z, false, nullptr);
    if (target.y < terrain_y)
    {
        target.y = terrain_y + 0.5f;
    }
    return target;
}

// 0x006B7590 (:717)
ai_results patrol_in_vehicle_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const object_patrol* patrol = World.object_from_handle<object_patrol>(ai->desired_destination_object);
    if (patrol == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const patrol_node_handle* node = ai->get_patrol_node();
    vector node_pos;
    if (node == nullptr || !patrol->get_node_pos(*node, node_pos))
    {
        return AI_RESULT_FAILURE;
    }
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }

    object* alternate_target = nullptr;
    vector alternate_pos = ZERO_VECTOR;
    bool fire_at_alternate = false;
    if (patrol->patrol_flags.override_patrol != 0)
    {
        alternate_target = World.object_from_handle(ai->desired_target);
        if (alternate_target != nullptr)
        {
            np->combat.target_handle = 0xFFFFFFFF;
            np->combat.targets_building_handle = 0xFFFFFFFF;
            fire_at_alternate = true;
            if (alternate_target->get_sub_type() == OT_SUB_MOVER_RFG)
            {
                if (!ai->desired_target_recalc_timestamp.valid() || ai->desired_target_recalc_timestamp.elapsed())
                {
                    ai->desired_target_pos = pick_building_target_pos(static_cast<rfg_mover*>(alternate_target));
                    ai->desired_target_recalc_timestamp.set(3000);
                }
                alternate_pos = ai->desired_target_pos;
                alternate_target = nullptr;
            }
            else if (alternate_target->get_obj_type() == OT_VEHICLE)
            {
                if (static_cast<vehicle*>(alternate_target)->m_flags.m_corpse) // 0x3F24 & 0x400
                {
                    ai->desired_target = 0xFFFFFFFF;
                }
                if (ai->flags.clear_desired_target_after_timeout && ai->desired_target_recalc_timestamp.valid() && ai->desired_target_recalc_timestamp.elapsed())
                {
                    ai->flags.clear_desired_target_after_timeout = false;
                    ai->desired_target = 0xFFFFFFFF;
                }
            }
        }
    }
    ai_vehicle_use_turret_if_available(np, vp, false, fire_at_alternate, alternate_target, alternate_pos);
    if (!human_is_driving(np))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (patrol->patrol_flags.is_paused != 0)
    {
        vehicle_set_controls_when_abandoned(vp);
        return AI_RESULT_IN_PROGRESS;
    }

    const bool use_roads = !node->dont_follow_road();
    bool use_light = false;
    bool use_disabled = false;
    if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
    {
        use_light = true;
        use_disabled = true;
        if (ai->action_timer.valid())
        {
            float speed = 10.0f;
            const navpoint* nav = World.object_from_handle<navpoint>(node->nav_handle);
            if (nav != nullptr)
            {
                speed = nav->speed_limit;
            }
            if (ai->action_timer.elapsed())
            {
                ai->action_timer.invalidate();
            }
            else
            {
                speed += 10.0f;
            }
            ai_vehicle_override_speed(vp, speed);
        }
        if (vehicle_is_bomber(vp) && np->npc_flags.bomb != 0 && (!ai->bomb_timer.valid() || ai->bomb_timer.elapsed()))
        {
            matrix orient;
            orient.make_quick(vp->orient.fvec);
            vector velocity;
            vehicle_get_velocity(vp, velocity);
            obj_item_flags item_flags = {};
            item_flags.cast_drop_shadows = 1;
            item_flags.cast_shadows = 1;
            item_flags.simulate = 1;
            // Bays 1.25 m left or right of the centre line, 1.5 m ahead and 5 m below, taking turns.
            const float side = np->npc_flags.going_left == 0 ? 1.25f : -1.25f;
            vector bomb_pos;
            bomb_pos.x = vp->orient.rvec.x * side + vp->orient.fvec.x * 1.5f - vp->orient.uvec.x * 5.0f + vp->pos.x;
            bomb_pos.y = vp->orient.rvec.y * side + vp->orient.fvec.y * 1.5f - vp->orient.uvec.y * 5.0f + vp->pos.y;
            bomb_pos.z = vp->orient.rvec.z * side + vp->orient.fvec.z * 1.5f - vp->orient.uvec.z * 5.0f + vp->pos.z;
            np->npc_flags.going_left = !np->npc_flags.going_left;
            static const weapon_info* aircraft_bomb_info = weapons_get_weapon_info("aircraft_bomb");
            if (aircraft_bomb_info != nullptr)
            {
                obj_projectile_props props(0xFFFFFFFF, aircraft_bomb_info->projectile_info.obj_item_info, item_flags, aircraft_bomb_info, bomb_pos, orient, velocity, ZERO_VECTOR, 0xFFFFFFFF);
                World.create_object<projectile>(props, 0xFFFFFFFF);
                ai->bomb_timer.set(500);
            }
        }
    }

    bool dont_stop = false;
    bool wait = false;
    if (vp->vai.vai_flags.is_courier != 0 || vp->vai.vai_flags.is_riding_shotgun != 0)
    {
        dont_stop = true;
    }
    else if (!vehicle_is_tank(vp) && np->npc_flags.mission_related == 0 && vp->vai.ambient_wait_timer.valid())
    {
        if (vp->vai.ambient_wait_timer.elapsed())
        {
            ai->flags.played_stuck_line = false;
            vp->vai.ambient_wait_timer.invalidate();
            vp->vai.stuck.stuck_threshold_time.invalidate();
            vp->vai.stuck.extended_stuck_time.invalidate();
        }
        else
        {
            wait = true;
        }
    }
    if (wait)
    {
        // Blocked: ask whoever is in the way to move, once per wait.
        if (!ai->flags.played_stuck_line)
        {
            const vehicle* blocking_vehicle = World.object_from_handle<vehicle>(vp->vai.ambient_wait_object);
            const human* blocking_human = World.object_from_handle<human>(vp->vai.ambient_wait_object);
            bool shout = true;
            if (blocking_vehicle != nullptr)
            {
                if (vehicle_is_corpse(blocking_vehicle))
                {
                    shout = false;
                }
                else
                {
                    // Original bug kept: the driver of this vehicle, np itself, not of the one blocking it.
                    blocking_human = vehicle_passenger_in_seat(vp, VSI_DRIVER);
                }
            }
            if (shout && blocking_human != nullptr && !human_is_dead(blocking_human) && blocking_human->h_info->default_team != HUMAN_TEAM_EDF)
            {
                np->play_voice_line(VOICE_LINE_PATROL_REQUEST_MOVE, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
                alert_shout_threaten alert(np, blocking_human);
                npc_sound_info::queue(alert);
            }
            ai->flags.played_stuck_line = true;
        }
        vehicle_set_controls_when_abandoned(vp);
        return AI_RESULT_IN_PROGRESS;
    }

    // From the asm: the decompile shifts these one to the left. 10 m is the arrival distance.
    ai_results result = ai_vehicle_travel_to_a_point_do_frame(np, vp, node_pos, 0xFFFFFFFFFFFFFFFFull, use_roads, 10.0f, dont_stop, use_light, use_disabled, true);
    if (result == AI_RESULT_SUCCESS)
    {
        patrol_squad_reached_node_event event;
        event.squad_handle = ai->get_squad_handle();
        event.patrol_node = node;
        gem_post_event(event);
        if (!ai->has_patrol_type_orders())
        {
            return AI_RESULT_FAILURE;
        }
        const patrol_node_handle next = patrol->get_next_patrol_point_handle(*node, np->pos);
        ai->update_patrol_node(&next);
        ai->update_patrol_speed(vp, &next, patrol);
        ai->update_road_usage(vp, &next);
        result = AI_RESULT_IN_PROGRESS;
    }
    if (activity_cc_is_courier(np))
    {
        maybe_update_courier_patrol_speed(np, vp);
    }
    return result;
}

// 0x005ECEC0 (:998)
ai_results patrol_in_vehicle_action::cleanup(npc* np, const ai_action* next_action) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (vp->vai.vai_flags.m_should_not_swerve != 0)
    {
        vp->vai.vai_flags.m_should_not_swerve = 0;
    }
    if (next_action != this && vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
    {
        static_cast<flyer*>(vp)->flyer_flags.stay_at_constant_height = 0;
    }
    return AI_RESULT_SUCCESS;
}

// 0x0063D530 (:1021)
int drive_on_rail_enter_rule::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const vehicle* vp = World.object_from_handle<vehicle>(np->vehicle_handle);
    unsigned int rail_vehicle;
    vrail_get_patrol_vehicle(rail_vehicle);
    if (vp == nullptr)
    {
        return 5000;
    }
    if (vp->get_handle() == rail_vehicle)
    {
        return -1;
    }
    desired_conditions |= ai_plan_conditions(COND_REACH_VEHICLE);
    return 5000;
}

// 0x006A55D0 (:1044)
int drive_on_rail_wait_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (goal_setup_data.selected_patrol == nullptr)
    {
        return -1;
    }
    vehicle* vp = World.object_from_handle<vehicle>(np->vehicle_handle);
    if (vp == nullptr || !ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER) || !ai_vehicle_is_seat_available(vp, np, VSI_DRIVER))
    {
        return -1;
    }
    unsigned int rail_vehicle;
    vrail_get_patrol_vehicle(rail_vehicle);
    if (vp->get_handle() != rail_vehicle)
    {
        return -1;
    }
    if (!mission_is_active() && vec_dist_squared(vp->pos, goal_setup_data.position) > 100.0f)
    {
        desired_conditions |= ai_plan_conditions(COND_REACH_RAIL_POS);
    }
    my_data.use_target = vp;
    return 5000;
}

// 0x005ECF10 (:1091)
void drive_on_rail_wait_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_vehicle = setup_data.use_target->get_handle();
    ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_DRIVER;
}

// 0x0063D5F0 (:1099)
ai_results drive_on_rail_wait_action::enter(npc* np, const ai_action*) const
{
    if (World.object_from_handle<object_patrol>(np->get_ai()->desired_destination_object) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    unsigned int rail_vehicle;
    vrail_get_patrol_vehicle(rail_vehicle);
    return vp->get_handle() == rail_vehicle ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x0063D650 (:1124)
ai_results drive_on_rail_wait_action::do_action(npc* np) const
{
    if (World.object_from_handle<object_patrol>(np->get_ai()->desired_destination_object) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle_set_controls_when_abandoned(vp);
    return AI_RESULT_IN_PROGRESS;
}

// 0x005ECF40 (:1146)
ai_results drive_on_rail_wait_action::cleanup(npc* np, const ai_action*) const
{
    return human_in_vehicle(np) != nullptr ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x0063D6A0 (:1162)
int drive_on_rail_rule::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (goal_setup_data.selected_patrol == nullptr)
    {
        return -1;
    }
    const player* hp = player_get_local_player();
    if (hp == nullptr)
    {
        return -1;
    }
    const vehicle* vp = human_in_vehicle(hp);
    if (vp == nullptr || vehicle_passenger_in_seat(vp, VSI_DRIVER) != np)
    {
        return -1;
    }
    unsigned int rail_vehicle;
    vrail_get_patrol_vehicle(rail_vehicle);
    if (vp->get_handle() != rail_vehicle)
    {
        desired_conditions |= ai_plan_conditions(COND_REACH_VEHICLE);
    }
    return vrail_get_patrol_state() != VRAIL_PATROL_STATE_PATROL ? -1 : 500; // (-x & 0xFFFFFE0B) + 500
}

// 0x005ECF60 (:1203)
int passenger_rail_rule::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 100000;
}

// The singletons, in rfg.exe's construction order: they fill action_pool, and that order breaks ties in the planner.
default_patrol_action default_patrol_action::the_instance; // 0x01D75218

// 0x0063C7E0 (.h:12)
default_patrol_action::default_patrol_action()
    : ai_action("default patrol", ai_plan_conditions_from_bits({4, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({38}))
{
    flags.has_enter = 1;
}

ai_action* default_patrol_action::get_instance()
{
    return &the_instance;
}

// 0x005ECB10
default_patrol_action::~default_patrol_action()
{
}

// 0x0063CAE0 (.h:12)
int default_patrol_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

patrol_check_in_action patrol_check_in_action::the_instance; // 0x01DDB804

// 0x0063CB20 (.h:13)
patrol_check_in_action::patrol_check_in_action()
    : ai_action("patrol check in", ai_plan_conditions_from_bits({4, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({38}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* patrol_check_in_action::get_instance()
{
    return &the_instance;
}

// 0x005ECB20
patrol_check_in_action::~patrol_check_in_action()
{
}

// 0x0063CE20 (.h:13)
int patrol_check_in_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

patrol_in_vehicle_action patrol_in_vehicle_action::the_instance; // 0x01DA0458

// 0x0063CE60 (.h:14)
patrol_in_vehicle_action::patrol_in_vehicle_action()
    : ai_action("patrol vehicle", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({38}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* patrol_in_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005ECB30
patrol_in_vehicle_action::~patrol_in_vehicle_action()
{
}

// 0x0063CEC0 (.h:14)
int patrol_in_vehicle_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

drive_on_rail_wait_action drive_on_rail_wait_action::the_instance; // 0x01DDB86C

// 0x0063D170 (.h:19)
drive_on_rail_wait_action::drive_on_rail_wait_action()
    : ai_action("vrail wait ", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({141}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* drive_on_rail_wait_action::get_instance()
{
    return &the_instance;
}

// 0x005ECB60
drive_on_rail_wait_action::~drive_on_rail_wait_action()
{
}

// 0x0063D1D0 (.h:19)
int drive_on_rail_wait_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

drive_on_rail_enter_rule drive_on_rail_enter_rule::the_instance; // 0x01D759A0

// 0x0063D0D0 (.h:18)
drive_on_rail_enter_rule::drive_on_rail_enter_rule()
    : ai_production_rule("vrail enter", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({141}))
{
}

ai_action* drive_on_rail_enter_rule::get_instance()
{
    return &the_instance;
}

// 0x005ECB50
drive_on_rail_enter_rule::~drive_on_rail_enter_rule()
{
}

// 0x0063D140 (.h:18)
int drive_on_rail_enter_rule::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 0;
}

drive_on_rail_rule drive_on_rail_rule::the_instance; // 0x01D9F6B0

// 0x0063D200 (.h:20)
drive_on_rail_rule::drive_on_rail_rule()
    : ai_production_rule("vrail drive", ai_plan_conditions_from_bits({38, 49}), ai_plan_conditions_from_bits({141}))
{
}

ai_action* drive_on_rail_rule::get_instance()
{
    return &the_instance;
}

// 0x005ECB70
drive_on_rail_rule::~drive_on_rail_rule()
{
}

// 0x0063D2E0 (.h:20)
int drive_on_rail_rule::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 0;
}

shoot_rail_rule shoot_rail_rule::the_instance; // 0x01DC9998

// 0x0063D310 (.h:21)
shoot_rail_rule::shoot_rail_rule()
    : ai_production_rule("shoot rail", ai_plan_conditions_from_bits({46}), ai_plan_conditions_from_bits({51}))
{
}

ai_action* shoot_rail_rule::get_instance()
{
    return &the_instance;
}

// 0x005ECB90
shoot_rail_rule::~shoot_rail_rule()
{
}

passenger_rail_rule passenger_rail_rule::the_instance; // 0x01D74E64

// 0x0063D3B0 (.h:22)
passenger_rail_rule::passenger_rail_rule()
    : ai_production_rule("passenger rail", ai_plan_conditions_from_bits({54}), ai_plan_conditions_from_bits({51}))
{
}

ai_action* passenger_rail_rule::get_instance()
{
    return &the_instance;
}

// 0x005ECBA0
passenger_rail_rule::~passenger_rail_rule()
{
}

// 0x0063D430 (.h:22)
int passenger_rail_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

lead_convoy_patrol_action lead_convoy_patrol_action::the_instance; // 0x01DC9CDC

// 0x0063CEF0 (.h:16)
lead_convoy_patrol_action::lead_convoy_patrol_action()
    : ai_action("lead convoy", ai_plan_conditions_from_bits({9, 49, 58, 136, 137}), ai_plan_conditions_from_bits({147}))
{
    flags.has_enter = 1;
}

ai_action* lead_convoy_patrol_action::get_instance()
{
    return &the_instance;
}

// 0x005ECB40
lead_convoy_patrol_action::~lead_convoy_patrol_action()
{
}

// 0x0063D0A0 (.h:16)
int lead_convoy_patrol_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

