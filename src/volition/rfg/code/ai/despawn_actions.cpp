// Original: volition/rfg/code/ai/despawn_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/despawn_actions.h"

#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/civilian/civilian_common.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/combat/combat_maneuver.h"
#include "volition/rfg/code/ai/self_preservation_goals.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/spawn/ambient_spawn_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_flyer.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/math/ix.h"

// Position 19 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG19")

static float NEAR_DESPAWN_OBJECT_DIST = 2.0f; // 0x016415D4, .data, never written

static const unsigned int COND_REACH_VEHICLE = 48;
static const unsigned int COND_IN_VEHICLE = 49;

// rfg.exe inlines this in every action that puts a driver or passenger in a vehicle.
static void man_seat_turret(npc* np, const vehicle* vp)
{
    if (np->vehicle_handle == vp->get_handle() && np->npc_flags.no_turret_attacks == 0)
    {
        turret* tp = vehicle_get_seat_turret(vp, np->vehicle_seat_idx);
        if (tp != nullptr && !tp->turret_flags.locked_turret)
        {
            human_combat_man_turret(np, tp);
        }
    }
}

// 0x005E8D20 (:121). LTCG: np ESI, despawn_pos EDI. Away from the player, 300 m out.
static bool flyer_despawn_location(const npc* np, vector& despawn_pos)
{
    if (np == nullptr)
    {
        return false;
    }
    const player* pl = player_get_local_player();
    vector dir;
    dir.x = np->pos.x - pl->pos.x;
    dir.y = 0.0f;
    dir.z = np->pos.z - pl->pos.z;
    dir.normalize_safe();
    if (dir.is_null())
    {
        dir = FORWARD_VECTOR;
    }
    despawn_pos.x = np->pos.x + dir.x * 300.0f;
    despawn_pos.y = np->pos.y + dir.y * 300.0f;
    despawn_pos.z = np->pos.z + dir.z * 300.0f;
    return true;
}

// 0x0069E780 (:319). LTCG: np EDI.
static vehicle* get_despawn_vehicle(const npc* np, ai_vehicle_seat_types seat_type)
{
    vehicle* vp = human_in_vehicle(np);
    if (vp != nullptr && ai_vehicle_is_usable(vp, np, seat_type) && ai_vehicle_has_available_seat(vp, np, seat_type))
    {
        return vp;
    }
    const object_squad* squad = np->get_ai()->get_squad();
    if (squad != nullptr)
    {
        vp = squad->get_vehicle();
        if (vp != nullptr && ai_vehicle_is_usable(vp, np, seat_type) && ai_vehicle_has_available_seat(vp, np, seat_type))
        {
            return vp;
        }
    }
    return nullptr;
}

// 0x005E8F00 (:347)
ai_results despawn_on_foot_common_enter(npc* np, bool first_frame)
{
    ai_system* ai = np->get_ai();
    if (first_frame)
    {
        ai->action_timer.set(0);
        ai->desired_destination = np->pos;
        ai->flags.cannot_retreat = false;
        ai->action_substate = 0;
    }
    return AI_RESULT_SUCCESS;
}

// 0x0069E830 (:366)
ai_results despawn_on_foot_common_do_action(npc* np, human_movement_states m_state)
{
    ai_system* ai = np->get_ai();
    const player* pl = player_get_local_player();
    if (pl == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    vector away;
    away.y = np->pos.y - pl->pos.y;
    away.z = np->pos.z - pl->pos.z;
    away.x = np->pos.x - pl->pos.x;
    away.flatten_mag_and_normalize_safe();
    if (!ai->action_timer.elapsed() && (np->movement_mode == HMM_PATHFINDING || ai->flags.cannot_retreat))
    {
        return AI_RESULT_IN_PROGRESS;
    }

    // Not on the mesh surface: back to it first.
    const nav_cell* ncp = nav_cell_from_handle(np->pf.last_visited_nav_cell_handle);
    bool on_mesh = ncp != nullptr && (ncp->flags & NAV_CELL_FLAG_DISABLED) == 0 && nav_cell_is_point_in_cell_column(ncp, np->pos);
    if (on_mesh)
    {
        float height;
        on_mesh = havok_terrain_height(height, np->pos.x, np->pos.z, false, nullptr) && height + 1.25f >= np->pos.y;
    }
    if (!on_mesh)
    {
        vector closest;
        nav_cell* closest_cell;
        if (bb_pf_find_closest_point_on_nav_mesh(np->pos, closest, closest_cell, false))
        {
            ai->pathfind_to(closest, HMS_WALK, HST_CURRENT, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false);
            return AI_RESULT_IN_PROGRESS;
        }
    }

    uint64_t hint = 0xFFFFFFFFFFFFFFFFull;
    vector dir;
    vector end_pos;
    const ai_results result = combat_maneuver_get_retreat_dir_unified(dir, end_pos, np, away, 4.0f, 20.0f, ai->action_substate, &hint);
    if (result == AI_RESULT_IN_PROGRESS)
    {
        ai->action_substate++;
        return AI_RESULT_IN_PROGRESS;
    }
    if (result == AI_RESULT_SUCCESS)
    {
        ai->desired_destination = end_pos;
        // From the asm: the decompile shifts these one to the left. HST_STAND is the 0 pushed.
        ai->set_direct_movement_to_pos(end_pos, hint, m_state, HST_STAND, np->cur_movement_state < 2);
        ai->flags.cannot_retreat = false;
    }
    else
    {
        ai->stop_moving();
        ai->flags.cannot_retreat = true;
    }
    ai->action_substate = 0;
    ai->action_timer.set_rand(2000, 3000);
    return AI_RESULT_IN_PROGRESS;
}

// 0x0062EB70 (:439)
int despawn_flyer_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const vehicle* vp = World.object_from_handle<vehicle>(np->vehicle_handle);
    if (vp == nullptr || vp->get_sub_type() != OT_SUB_VEHICLE_FLYER)
    {
        return -1;
    }
    return vehicle_passenger_get_seat(vp, np) != VSI_DRIVER ? -1 : 120; // (-x & 0xFFFFFF87) + 0x78
}

// 0x0062EBC0 (:457)
ai_results despawn_flyer_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    vehicle* vp = World.object_from_handle<vehicle>(np->vehicle_handle);
    if (vp == nullptr || vp->get_sub_type() != OT_SUB_VEHICLE_FLYER)
    {
        return AI_RESULT_FAILURE;
    }
    if (vp->vai.vai_flags.m_jumping == 0)
    {
        vehicle_ai_clear_speed_override(&vp->vai);
    }
    ai_system* ai = np->get_ai();
    if (!flyer_despawn_location(np, ai->desired_destination))
    {
        return AI_RESULT_FAILURE;
    }
    ai->action_timer.set_rand(3000, 9000);
    return AI_RESULT_SUCCESS;
}

// 0x0069EAA0 (:485)
ai_results despawn_flyer_action::do_action(npc* np) const
{
    vehicle* vp = World.object_from_handle<vehicle>(np->vehicle_handle);
    if (vp == nullptr || vp->get_sub_type() != OT_SUB_VEHICLE_FLYER)
    {
        return AI_RESULT_FAILURE;
    }
    ai_system* ai = np->get_ai();
    if (ai->action_timer.elapsed() && !flyer_despawn_location(np, ai->desired_destination))
    {
        return AI_RESULT_FAILURE;
    }
    // From the asm: the decompile shifts these one to the left. 3 m is the arrival distance.
    const ai_results result = ai_vehicle_travel_to_a_point_do_frame(np, vp, ai->desired_destination, 0xFFFFFFFFFFFFFFFFull, false, 3.0f, false, false, false, true);
    return result != AI_RESULT_FAILURE ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x0062EC60 (:516)
int despawn_at_object_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if (np->get_despawn_object() == nullptr || World.object_from_handle<vehicle>(np->vehicle_handle) != nullptr)
    {
        return -1;
    }
    return 100;
}

// 0x0065D030 (:535)
ai_results despawn_at_object_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        np->get_ai()->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        human_set_combat_ready(np, false, 0);
    }
    return AI_RESULT_SUCCESS;
}

// 0x0069EB40 (:548)
ai_results despawn_at_object_action::do_action(npc* np) const
{
    object* target = np->get_despawn_object();
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai_system* ai = np->get_ai();
    const ai_pathfind_results result = ai->pathfind_to(target->pos, HMS_RUN, HST_STAND, AI_PATHFIND_PRIORITY_LOW, 0xFFFFFFFFFFFFFFFFull, false);
    if (result != AI_PF_SUCCESS && result > AI_PF_RAGDOLLED)
    {
        return AI_RESULT_FAILURE;
    }
    vector bmin;
    vector bmax;
    target->get_local_bbox(bmin, bmax);
    bmin.x -= NEAR_DESPAWN_OBJECT_DIST;
    bmin.y -= NEAR_DESPAWN_OBJECT_DIST;
    bmin.z -= NEAR_DESPAWN_OBJECT_DIST;
    bmax.x += NEAR_DESPAWN_OBJECT_DIST;
    bmax.y += NEAR_DESPAWN_OBJECT_DIST;
    bmax.z += NEAR_DESPAWN_OBJECT_DIST;
    vector local_pos = np->pos;
    vec_change_to_local_coords(local_pos, target->pos, target->orient);
    if (ix_point_in_box(local_pos, bmin, bmax))
    {
        ai->do_despawn(true);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x0067AE40 (:594)
int building_despawn_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const ai_perception& perception = np->get_ai()->perception;
    if (!perception.is_in_closest_building(np, nullptr, 0.0f, 1.25f))
    {
        return -1;
    }
    const player* pl = player_get_local_player();
    if (pl != nullptr && perception.is_in_closest_building(pl, nullptr, 0.0f, 1.25f))
    {
        return -1;
    }
    return 450;
}

// 0x0065D090 (:611)
ai_results building_despawn_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        ai->perception.set_attention_mode(ATTENTION_IDLE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        human_set_combat_ready(np, false, 0);
        ai->stop_moving();
    }
    return AI_RESULT_SUCCESS;
}

// 0x0062ECA0 (:624): do_despawn(false) inlined.
ai_results building_despawn_action::do_action(npc* np) const
{
    np->get_ai()->do_despawn(false);
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E8F50 (:634)
int despawn_ped_network_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if (np->vehicle_handle != 0xFFFFFFFF)
    {
        return -1;
    }
    if (np->current_team != HUMAN_TEAM_CIVILIAN && human_combat_get_target(np) != nullptr)
    {
        return -1;
    }
    bbox area;
    area.min.x = np->pos.x - 30.0f;
    area.min.y = np->pos.y - 30.0f;
    area.min.z = np->pos.z - 30.0f;
    area.max.x = np->pos.x + 30.0f;
    area.max.y = np->pos.y + 30.0f;
    area.max.z = np->pos.z + 30.0f;
    return ped_node_near_box(area) ? 250 : -1; // (-x & 0xFB) - 1
}

// 0x0065D1A0 (:656)
ai_results despawn_ped_network_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    ai_system* ai = np->get_ai();
    ai->action_obj_handle = 0xFFFFFFFF;
    ai->desired_destination.zero();
    if (!ai->pedestrian_info.near_ped_nodes(np->pos) && !amb_spawn_build_ped_waypoints(np, ai->pedestrian_info, PED_NODE_SELECT_MODE_DESPAWN))
    {
        return AI_RESULT_FAILURE;
    }
    return civilian_pick_next_ped_node(np) != nullptr ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x0069ECD0 (:671)
ai_results despawn_ped_network_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    // Every other frame, by the handle's lowest bit.
    if ((np->get_handle() & 1) == (unsigned int)(Framecount % 2))
    {
        ai->flags.yield_next_frame = true;
    }
    human_movement_states m_state = HMS_WALK;
    if (human_team_is_non_combatant(np) && ai->perception.get_belief(AI_BELIEF_COMBAT_SOUND) != nullptr)
    {
        m_state = HMS_SPRINT;
    }
    return civilian_ped_do_action_common(np, m_state, PED_NODE_SELECT_MODE_DESPAWN) ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x005E9050 (:696)
int default_despawn_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 10000;
}

// 0x0065D220 (:701)
ai_results default_despawn_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        np->get_ai()->perception.set_attention_mode(ATTENTION_IDLE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        human_set_combat_ready(np, false, 0);
    }
    if (human_in_vehicle(np) != nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    return despawn_on_foot_common_enter(np, last_action != this);
}

// 0x0069ED50 (:722)
ai_results default_despawn_action::do_action(npc* np) const
{
    const ai_system* ai = np->get_ai();
    human_movement_states m_state = HMS_WALK;
    if (human_team_is_non_combatant(np) && ai != nullptr && ai->perception.get_belief(AI_BELIEF_COMBAT_SOUND) != nullptr)
    {
        m_state = HMS_SPRINT;
    }
    return despawn_on_foot_common_do_action(np, m_state);
}

// 0x0062ED10 (:737)
int retreat_despawn_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const ai_perception& perception = np->get_ai()->perception;
    if (human_combat_get_target(np) == nullptr && perception.get_belief(AI_BELIEF_ENEMY_POSITION) == nullptr && perception.get_belief(AI_BELIEF_ENEMY_EXISTS) == nullptr)
    {
        return -1;
    }
    return 200;
}

// 0x0069EDA0 (:756)
ai_results retreat_despawn_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const human* target = human_combat_get_target(np);
    if (target == nullptr && ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION) == nullptr && ai->perception.get_belief(AI_BELIEF_ENEMY_EXISTS) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    if (target != nullptr)
    {
        ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    }
    else
    {
        ai->perception.set_attention_mode(ATTENTION_IDLE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    }
    return despawn_on_foot_common_enter(np, last_action != this);
}

// 0x0069EF20 (:781)
ai_results retreat_despawn_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human_set_combat_ready(np, true, 0);
    const human* target = human_combat_get_target(np);
    if (target != nullptr)
    {
        ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        ai_combat_action_parameters params;
        params.init_npc(np);
        if (!params.init_target(true))
        {
            return AI_RESULT_FAILURE;
        }
        ai_combat_fire_at_target(params, false);
    }
    return despawn_on_foot_common_do_action(np, HMS_RUN);
}

// 0x0062ED50 (:808)
int retreat_unarmed_despawn_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const ai_perception& perception = np->get_ai()->perception;
    if (np->current_team != HUMAN_TEAM_EDF &&
        (human_combat_get_target(np) != nullptr || perception.get_belief(AI_BELIEF_ENEMY_POSITION) != nullptr || perception.get_belief(AI_BELIEF_ENEMY_EXISTS) != nullptr))
    {
        return 3000;
    }
    return -1;
}

// 0x0065D380 (:832)
ai_results retreat_unarmed_despawn_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (human_combat_get_target(np) == nullptr && ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION) == nullptr && ai->perception.get_belief(AI_BELIEF_ENEMY_EXISTS) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    ai->perception.set_attention_mode(ATTENTION_IDLE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    return despawn_on_foot_common_enter(np, last_action != this);
}

// 0x0069EFA0 (:853)
ai_results retreat_unarmed_despawn_action::do_action(npc* np) const
{
    human_set_combat_ready(np, true, 0);
    if (human_combat_get_target(np) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    return despawn_on_foot_common_do_action(np, HMS_RUN);
}

// 0x0069EFD0 (:870)
int flee_despawn_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return ai_get_enemy_flee_belief(np) != nullptr || ai_get_flee_to_safety_belief(np) != nullptr ? 7000 : -1;
}

// 0x0065D4E0 (:883)
ai_results flee_despawn_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        human_set_combat_ready(np, false, 0);
        np->get_ai()->perception.set_attention_mode(ATTENTION_IDLE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_OBLIVIOUS);
    }
    return despawn_on_foot_common_enter(np, last_action != this);
}

// 0x0069F000 (:896)
ai_results flee_despawn_action::do_action(npc* np) const
{
    return despawn_on_foot_common_do_action(np, HMS_SPRINT);
}

// 0x0069F020 (:905)
int drive_despawn_road_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    vehicle* vp = get_despawn_vehicle(np, AI_VEHICLE_SEAT_TYPE_DRIVER);
    if (vp == nullptr || object_road_get_closest_road(vp->pos, false, 60.0f, false, false) == nullptr)
    {
        return -1;
    }
    const vehicle* current = human_in_vehicle(np);
    if (current != nullptr)
    {
        if (current == vp)
        {
            if (ai_vehicle_get_seat_type(np, current, AI_VEHICLE_SEAT_TYPE_UNDEFINED) != AI_VEHICLE_SEAT_TYPE_DRIVER)
            {
                return -1;
            }
        }
        else
        {
            desired_conditions |= ai_plan_conditions(COND_REACH_VEHICLE) | ai_plan_conditions(COND_IN_VEHICLE);
        }
    }
    my_data.use_target = vp;
    return 1000;
}

// 0x005E9060 (:937)
void drive_despawn_road_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    if (setup_data.use_target != nullptr)
    {
        ai->desired_vehicle = setup_data.use_target->get_handle();
        ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_DRIVER;
    }
}

// Inlined in both drive_despawn enter functions.
static void set_despawn_drive_speed(ai_system* ai, vehicle* vp)
{
    if (vp->is_ambient())
    {
        const float speed = ai->desired_vehicle_speed;
        ai_vehicle_override_speed(vp, speed <= 0.0f ? 9.0f : speed);
    }
    else if (vp->vai.vai_flags.is_courier != 0)
    {
        ai_vehicle_override_speed(vp, 22.5f);
        ai->planner.lock_action(true);
    }
    else
    {
        ai_vehicle_override_speed(vp, ai->desired_vehicle_speed);
    }
}

// 0x0067AEC0 (:945)
ai_results drive_despawn_road_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || !human_is_driving(np))
    {
        return AI_RESULT_FAILURE;
    }
    man_seat_turret(np, vp);
    vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
    set_despawn_drive_speed(ai, vp);
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    object_path_road* road = object_road_get_closest_road(vp->pos, false, 60.0f, false, false);
    if (road != nullptr && ai_vehicle_pick_flee_destination_road(vp, road, ai->desired_destination, player_get_local_player()->pos))
    {
        return AI_RESULT_SUCCESS;
    }
    return AI_RESULT_FAILURE;
}

// Inlined in both drive_despawn do_action functions: waits out a blockage of an ambient vehicle. True while it waits.
static bool ambient_wait_do_frame(const npc* np, vehicle* vp)
{
    if (np->npc_flags.ambient_spawn != 0 && vp->vai.ambient_wait_timer.valid())
    {
        if (!vp->vai.ambient_wait_timer.elapsed())
        {
            vehicle_set_controls_when_abandoned(vp);
            return true;
        }
        vp->vai.ambient_wait_timer.invalidate();
        vp->vai.stuck.stuck_threshold_time.invalidate();
        vp->vai.stuck.extended_stuck_time.invalidate();
    }
    return false;
}

// 0x006B6660 (:997)
ai_results drive_despawn_road_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || !human_is_driving(np) || !ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER))
    {
        return AI_RESULT_FAILURE;
    }
    if (ambient_wait_do_frame(np, vp))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    const player* pl = player_get_local_player();
    if (pl == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai_vehicle_use_turret_if_available(np, vp, false, false, nullptr, ZERO_VECTOR);
    ai->do_despawn(false);
    // From the asm: the decompile shifts these one to the left. 5 m is the arrival distance.
    const ai_results result = ai_vehicle_travel_to_a_point_do_frame(np, vp, ai->desired_destination, 0xFFFFFFFFFFFFFFFFull, true, 5.0f, true, false, false, true);
    if (result == AI_RESULT_FAILURE)
    {
        return AI_RESULT_FAILURE;
    }
    if (result != AI_RESULT_SUCCESS)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    object_path_road* road = object_road_get_closest_road(vp->pos, false, 60.0f, false, false);
    if (road == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    return ai_vehicle_pick_flee_destination_road(vp, road, ai->desired_destination, pl->pos) ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x0069F150 (:1074)
int drive_despawn_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    vehicle* vp = get_despawn_vehicle(np, AI_VEHICLE_SEAT_TYPE_DRIVER);
    if (vp == nullptr)
    {
        return -1;
    }
    const vehicle* current = human_in_vehicle(np);
    if (current != nullptr)
    {
        if (current == vp)
        {
            if (ai_vehicle_get_seat_type(np, current, AI_VEHICLE_SEAT_TYPE_UNDEFINED) != AI_VEHICLE_SEAT_TYPE_DRIVER)
            {
                return -1;
            }
        }
        else
        {
            desired_conditions |= ai_plan_conditions(COND_REACH_VEHICLE) | ai_plan_conditions(COND_IN_VEHICLE);
        }
    }
    my_data.use_target = vp;
    return 1000;
}

// 0x005E9090 (:1098)
void drive_despawn_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    if (setup_data.use_target != nullptr)
    {
        ai->desired_vehicle = setup_data.use_target->get_handle();
        ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_DRIVER;
    }
}

// 0x0062EDA0 (:1106)
ai_results drive_despawn_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || !human_is_driving(np))
    {
        return AI_RESULT_FAILURE;
    }
    man_seat_turret(np, vp);
    vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
    set_despawn_drive_speed(ai, vp);
    if (last_action != this)
    {
        ai->action_timer.set(0);
        ai->desired_destination = vp->pos;
    }
    return AI_RESULT_SUCCESS;
}

// 0x006B67E0 (:1149)
ai_results drive_despawn_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || !human_is_driving(np) || !ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER))
    {
        return AI_RESULT_FAILURE;
    }
    if (ambient_wait_do_frame(np, vp))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    const player* pl = player_get_local_player();
    if (pl == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai_vehicle_use_turret_if_available(np, vp, false, false, nullptr, ZERO_VECTOR);
    ai->do_despawn(false);
    // From the asm: the decompile shifts these one to the left. 3 m is the arrival distance.
    const ai_results result = ai_vehicle_travel_to_a_point_do_frame(np, vp, ai->desired_destination, 0xFFFFFFFFFFFFFFFFull, false, 3.0f, true, false, false, true);
    if (result == AI_RESULT_FAILURE)
    {
        return AI_RESULT_FAILURE;
    }
    if (result != AI_RESULT_SUCCESS && !ai->action_timer.elapsed())
    {
        return AI_RESULT_IN_PROGRESS;
    }
    ai->action_timer.set_rand(2000, 3000);
    vector away;
    away.y = np->pos.y - pl->pos.y;
    away.z = np->pos.z - pl->pos.z;
    away.x = np->pos.x - pl->pos.x;
    away.flatten_mag_and_normalize_safe();
    vector dir;
    vector end_pos;
    ai->desired_destination = combat_maneuver_get_retreat_dir(dir, end_pos, vp, away, 10.0f, 30.0f) == AI_RESULT_SUCCESS ? end_pos : vp->pos;
    return AI_RESULT_IN_PROGRESS;
}

// 0x0069F260 (:1233)
int passenger_despawn_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    vehicle* vp = get_despawn_vehicle(np, AI_VEHICLE_SEAT_TYPE_PASSENGER);
    if (vp == nullptr)
    {
        return -1;
    }
    if (human_in_vehicle(np) != nullptr && np->vehicle_handle != vp->get_handle())
    {
        desired_conditions |= ai_plan_conditions(COND_REACH_VEHICLE) | ai_plan_conditions(COND_IN_VEHICLE);
    }
    my_data.use_target = vp;
    return 2000;
}

// 0x005E90C0 (:1252)
void passenger_despawn_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    if (setup_data.use_target != nullptr)
    {
        ai->desired_vehicle = setup_data.use_target->get_handle();
        ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_PASSENGER;
    }
}

// 0x0062EED0 (:1260)
ai_results passenger_despawn_action::enter(npc* np, const ai_action*) const
{
    const vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    man_seat_turret(np, vp);
    return AI_RESULT_SUCCESS;
}

// 0x006B6A30 (:1272)
ai_results passenger_despawn_action::do_action(npc* np) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai_vehicle_use_turret_if_available(np, vp, false, false, nullptr, ZERO_VECTOR);
    return AI_RESULT_IN_PROGRESS;
}

// The singletons, in rfg.exe's construction order: they fill action_pool, and that order breaks ties in the planner.
despawn_flyer_action despawn_flyer_action::the_instance; // 0x01D72314

// 0x0062CD20 (.h:15)
despawn_flyer_action::despawn_flyer_action()
    : ai_action("despawn flyer", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({70}))
{
    flags.has_enter = 1;
}

ai_action* despawn_flyer_action::get_instance()
{
    return &the_instance;
}

// 0x005E7F90
despawn_flyer_action::~despawn_flyer_action()
{
}

// 0x0062CD80 (.h:15)
int despawn_flyer_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

despawn_at_object_action despawn_at_object_action::the_instance; // 0x01D75604

// 0x0062CA40 (.h:14)
despawn_at_object_action::despawn_at_object_action()
    : ai_action("despawn at object", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({70}))
{
    flags.has_enter = 1;
}

ai_action* despawn_at_object_action::get_instance()
{
    return &the_instance;
}

// 0x005E7F80
despawn_at_object_action::~despawn_at_object_action()
{
}

// 0x0062CCE0 (.h:14)
int despawn_at_object_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

building_despawn_action building_despawn_action::the_instance; // 0x01D754DC

// 0x0062D0A0 (.h:17)
building_despawn_action::building_despawn_action()
    : ai_action("building despawn", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({70}))
{
    flags.has_enter = 1;
}

ai_action* building_despawn_action::get_instance()
{
    return &the_instance;
}

// 0x005E7FB0
building_despawn_action::~building_despawn_action()
{
}

// 0x0062D340 (.h:17)
int building_despawn_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

despawn_ped_network_action despawn_ped_network_action::the_instance; // 0x01DDB6F0

// 0x0062CDC0 (.h:16)
despawn_ped_network_action::despawn_ped_network_action()
    : ai_action("despawn ped network", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({70}))
{
    flags.has_enter = 1;
}

ai_action* despawn_ped_network_action::get_instance()
{
    return &the_instance;
}

// 0x005E7FA0
despawn_ped_network_action::~despawn_ped_network_action()
{
}

// 0x0062D060 (.h:16)
int despawn_ped_network_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

default_despawn_action default_despawn_action::the_instance; // 0x01DA6110

// 0x0062D380 (.h:18)
default_despawn_action::default_despawn_action()
    : ai_action("default despawn", ai_plan_conditions_from_bits({9, 14, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({70}))
{
    flags.has_enter = 1;
}

ai_action* default_despawn_action::get_instance()
{
    return &the_instance;
}

// 0x005E7FC0
default_despawn_action::~default_despawn_action()
{
}

// 0x0062D680 (.h:18)
int default_despawn_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

retreat_despawn_action retreat_despawn_action::the_instance; // 0x01DCA534

// 0x0062D6C0 (.h:19)
retreat_despawn_action::retreat_despawn_action()
    : ai_action("retreat despawn", ai_plan_conditions_from_bits({0, 2, 9, 14, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({70}))
{
    flags.has_enter = 1;
}

ai_action* retreat_despawn_action::get_instance()
{
    return &the_instance;
}

// 0x005E7FD0
retreat_despawn_action::~retreat_despawn_action()
{
}

// 0x0062DA70 (.h:19)
int retreat_despawn_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

retreat_unarmed_despawn_action retreat_unarmed_despawn_action::the_instance; // 0x01D71FDC

// 0x0062DAB0 (.h:20)
retreat_unarmed_despawn_action::retreat_unarmed_despawn_action()
    : ai_action("retreat unarmed despawn", ai_plan_conditions_from_bits({9, 14, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({70}))
{
    flags.has_enter = 1;
}

ai_action* retreat_unarmed_despawn_action::get_instance()
{
    return &the_instance;
}

// 0x005E7FE0
retreat_unarmed_despawn_action::~retreat_unarmed_despawn_action()
{
}

// 0x0062DDB0 (.h:20)
int retreat_unarmed_despawn_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

flee_despawn_action flee_despawn_action::the_instance; // 0x01D755D0

// 0x0062DDF0 (.h:21)
flee_despawn_action::flee_despawn_action()
    : ai_action("flee despawn", ai_plan_conditions_from_bits({9, 14, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({70}))
{
    flags.has_enter = 1;
}

ai_action* flee_despawn_action::get_instance()
{
    return &the_instance;
}

// 0x005E7FF0
flee_despawn_action::~flee_despawn_action()
{
}

// 0x0062E0F0 (.h:21)
int flee_despawn_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

drive_despawn_road_action drive_despawn_road_action::the_instance; // 0x01D71F38

// 0x0062E130 (.h:24)
drive_despawn_road_action::drive_despawn_road_action()
    : ai_action("drive road despawn", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({70}))
{
    flags.has_enter = 1;
}

ai_action* drive_despawn_road_action::get_instance()
{
    return &the_instance;
}

// 0x005E8000
drive_despawn_road_action::~drive_despawn_road_action()
{
}

// 0x0062E190 (.h:24)
int drive_despawn_road_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

drive_despawn_action drive_despawn_action::the_instance; // 0x01D68234

// 0x0062E1C0 (.h:25)
drive_despawn_action::drive_despawn_action()
    : ai_action("drive despawn", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({70}))
{
    flags.has_enter = 1;
}

ai_action* drive_despawn_action::get_instance()
{
    return &the_instance;
}

// 0x005E8010
drive_despawn_action::~drive_despawn_action()
{
}

// 0x0062E220 (.h:25)
int drive_despawn_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

passenger_despawn_action passenger_despawn_action::the_instance; // 0x01DDBA38

// 0x0062E250 (.h:26)
passenger_despawn_action::passenger_despawn_action()
    : ai_action("passenger despawn", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({70}))
{
    flags.has_enter = 1;
}

ai_action* passenger_despawn_action::get_instance()
{
    return &the_instance;
}

// 0x005E8020
passenger_despawn_action::~passenger_despawn_action()
{
}

// 0x0062E2B0 (.h:26)
int passenger_despawn_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

