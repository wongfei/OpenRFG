// Original: volition/rfg/code/ai/ai_script_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/ai_script_actions.h"

#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/rfg/code/vehicle_ai/vehicle_navmesh_common.h"
#include "volition/rfg/code/vehicle_ai/vehicle_navmesh_path_create.h"
#include "volition/rfg/code/weapons/weapons.h"

// Position 1 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG01")

// 0x005DD790 (:31)
int move_to_location_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 5;
}

// 0x006B4F60 (:36)
ai_results move_to_location_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    const object* location = World.object_from_handle(np->scripting_info.location_object);
    if (location == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const ai_pathfind_results result = ai->pathfind_to(location->pos, np->scripting_info.flags.run ? HMS_RUN : HMS_WALK, HST_STAND, AI_PATHFIND_PRIORITY_HIGH, 0xFFFFFFFFFFFFFFFFull, false);
    if (result == AI_PF_SUCCESS)
    {
        return AI_RESULT_SUCCESS;
    }
    return result > AI_PF_RAGDOLLED ? AI_RESULT_FAILURE : AI_RESULT_IN_PROGRESS;
}

// A navpoint's radius, or half a metre for any other location; inlined in do_action and cleanup.
static float location_radius(const object* location)
{
    return location->get_obj_type() == OT_NAVPOINT ? static_cast<const navpoint*>(location)->radius : 0.5f;
}

// 0x006B4FD0 (:56)
ai_results move_to_location_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (np->scripting_info.flags.fire_at_target)
    {
        ai_combat_action_parameters params;
        params.init_npc(np);
        if (np->combat.combat_ready_timestamp.valid() && !np->combat.combat_ready_timestamp.elapsed() && ai->perception.get_attention_mode() == ATTENTION_AIM_AT_OBJECT &&
            params.init_target(false) && params.weapon_inst != nullptr && params.weapon_inst->is_loaded() && params.weapon_inst->w_info->is_firearm() &&
            params.weapon_inst->get_range_category(params.dist_to_target) != WEAPON_OUT_OF_RANGE)
        {
            human_set_combat_ready(np, true, 0);
            if (ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr && ai->perception.can_hit_target())
            {
                ai_combat_fire_at_target(params, true);
            }
        }
    }
    const object* location = World.object_from_handle(np->scripting_info.location_object);
    if (location == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const float radius = location_radius(location);
    if (vec_dist_squared(location->pos, np->pos) < radius * radius)
    {
        return AI_RESULT_SUCCESS;
    }
    if (!ai->is_pathing_to(location->pos, 0.25f, 1.0f))
    {
        const ai_pathfind_results result = ai->pathfind_to(location->pos, np->scripting_info.flags.run ? HMS_RUN : HMS_WALK, HST_CURRENT, AI_PATHFIND_PRIORITY_HIGH, 0xFFFFFFFFFFFFFFFFull, false);
        if (result != AI_PF_SUCCESS && result > AI_PF_RAGDOLLED)
        {
            return AI_RESULT_FAILURE;
        }
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005DD7B0 (:116)
ai_results move_to_location_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        const object* location = World.object_from_handle(np->scripting_info.location_object);
        if (location == nullptr)
        {
            return AI_RESULT_FAILURE;
        }
        const float radius = location_radius(location);
        if (vec_dist_squared(location->pos, np->pos) < radius * radius)
        {
            np->npc_flags.scripted_move = 0;
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x005DD840 (:144)
int drive_to_location_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return vehicle_passenger_in_or_entering_seat(np->vehicle_handle, VSI_DRIVER) != np ? -1 : 5; // (-x & 0xFFFFFFFA) + 5
}

// 0x005FEB50 (:155)
ai_results drive_to_location_action::enter(npc* np, const ai_action* last_action) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
    if (np->vehicle_handle == vp->get_handle() && np->npc_flags.no_turret_attacks == 0)
    {
        turret* tp = vehicle_get_seat_turret(vp, np->vehicle_seat_idx);
        if (tp != nullptr && !tp->turret_flags.locked_turret)
        {
            human_combat_man_turret(np, tp);
        }
    }
    if (last_action != this)
    {
        vp->vai.navmesh_path_update.set(0);
    }
    return AI_RESULT_SUCCESS;
}

// 0x006B51B0 (:174)
ai_results drive_to_location_action::do_action(npc* np) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai_vehicle_use_turret_if_available(np, vp, false, false, nullptr, ZERO_VECTOR);
    if (!human_is_driving(np))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    const object* location = World.object_from_handle(np->scripting_info.location_object);
    if (location == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (vec_dist_squared(location->pos, vp->pos) < 1.0f)
    {
        vehicle_set_controls_when_abandoned(vp);
        return AI_RESULT_SUCCESS;
    }
    if (!np->scripting_info.flags.drive_directly)
    {
        // From the asm: the decompile shifts these one to the left. 3 m is the arrival distance.
        return ai_vehicle_travel_to_a_point_do_frame(np, vp, location->pos, 0xFFFFFFFFFFFFFFFFull, false, 3.0f, false, false, false, true) != AI_RESULT_FAILURE ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
    }
    if (vp->vai.route_info.num_route_segments != 0 && !vp->vai.navmesh_path_update.elapsed())
    {
        if (!vehicle_ai_process_stuck(vp, true))
        {
            vehicle_navmesh_common_follow_path(vp);
        }
        return AI_RESULT_IN_PROGRESS;
    }
    // Straight there, off road, re-made every half second to second.
    vehicle_path_info path_info;
    path_info.flags.off_road = 0;
    path_info.flags.on_bridge = 0;
    const pf_calc_point points[2] = {pf_calc_point(vp->pos, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF), pf_calc_point(location->pos, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF)};
    navmesh_lane_spline_node* path = vehicle_generate_direct_path_between_points(2, points, path_info, vp);
    vehicle_ai_free_all_paths(&vp->vai);
    vehicle_navmesh_common_init_path_follow(&vp->vai, path);
    vp->vai.current_path_info = path_info;
    vp->vai.navmesh_path_update.set_rand(500, 1000);
    vp->vai.vai_flags.m_goal_stop_override = 0;
    return AI_RESULT_IN_PROGRESS;
}

// 0x005DD870 (:246)
ai_results drive_to_location_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->npc_flags.scripted_drive = 0;
    }
    return AI_RESULT_SUCCESS;
}

// The singletons, in rfg.exe's construction order: they fill action_pool, and that order breaks ties in the planner.
move_to_location_action move_to_location_action::the_instance; // 0x01DC96B4

// 0x005FE840 (.h:56)
move_to_location_action::move_to_location_action()
    : ai_action("move to location", ai_plan_conditions_from_bits({47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({154}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* move_to_location_action::get_instance()
{
    return &the_instance;
}

// 0x005DD770
move_to_location_action::~move_to_location_action()
{
}

// 0x005FEA70 (.h:56)
int move_to_location_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

drive_to_location_action drive_to_location_action::the_instance; // 0x01DCA380

// 0x005FEAB0 (.h:57)
drive_to_location_action::drive_to_location_action()
    : ai_action("drive to location", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({154}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* drive_to_location_action::get_instance()
{
    return &the_instance;
}

// 0x005DD780
drive_to_location_action::~drive_to_location_action()
{
}

// 0x005FEB10 (.h:57)
int drive_to_location_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

