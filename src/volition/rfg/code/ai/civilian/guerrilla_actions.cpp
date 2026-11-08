// Original: volition/rfg/code/ai/civilian/guerrilla_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/civilian/guerrilla_actions.h"

#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/civilian/civilian_common.h"
#include "volition/rfg/code/ai/follow_logic.h"
#include "volition/rfg/code/ai/formation.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/spawn/ambient_spawn_manager.h"
#include "volition/rfg/code/vehicle/vehicle_audio.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/vlib/code/math/bbox.h"

// Position 5 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG05")

// 0x005E16F0 (:60)
int look_for_trouble_on_foot_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    bbox box;
    box.min = vector(np->pos.x - 30.0f, np->pos.y - 30.0f, np->pos.z - 30.0f);
    box.max = vector(np->pos.x + 30.0f, np->pos.y + 30.0f, np->pos.z + 30.0f);
    return ped_node_near_box(box) ? 2000 : -1;
}

// 0x00655DF0 (:69)
ai_results look_for_trouble_on_foot_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    human_set_combat_ready(np, false, 0);
    if (last_action != this)
    {
        ai->perception.set_attention_mode(ATTENTION_SEARCH, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
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

// 0x00690F10 (:89)
ai_results look_for_trouble_on_foot_action::do_action(npc* np) const
{
    return civilian_ped_do_action_common(np, HMS_WALK, PED_NODE_SELECT_MODE_NORMAL) ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x00655FC0 (:100). Only npcs at a lower address are followed, and the lowest in range wins, so a group settles on one leader.
int look_for_trouble_follow_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const unsigned int formation = np->get_ai()->get_formation_handle();
    const object* current_leader = formation != 0xFFFFFFFF ? ai_formation_get_leader(formation) : nullptr;
    npc* best = nullptr;
    const int num_npcs = World.get_num_objects<npc>();
    for (int i = 0; i < num_npcs; i++)
    {
        npc* other = World.get_object<npc>(i);
        if (other >= np || human_teams_get_disposition(np, other) != TEAM_DISPOSITION_ALLIED || other->is_killed() || other->get_ai()->planner.get_current_action() != look_for_trouble_on_foot_action::get_instance())
        {
            continue;
        }
        const float max_dist_sq = other == current_leader ? 225.0f : 56.25f;
        if (vec_dist_squared(np->pos, other->pos) <= max_dist_sq && (best == nullptr || other < best))
        {
            best = other;
        }
    }
    if (best == nullptr)
    {
        return -1;
    }
    my_data.use_target = best;
    return 1900;
}

// 0x005E17C0 (:148)
void look_for_trouble_follow_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    if (setup_data.use_target != nullptr)
    {
        np->get_ai()->desired_follow_target = setup_data.use_target->get_handle();
    }
}

// 0x006BBF90 (:155)
ai_results look_for_trouble_follow_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    human_set_combat_ready(np, false, 0);
    ai->perception.set_attention_mode(ATTENTION_SEARCH, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    const object* target = ai->desired_follow_target != 0xFFFFFFFF ? World.object_from_handle(ai->desired_follow_target) : nullptr;
    if (target == nullptr || target->get_obj_type() != OT_HUMAN || target->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return AI_RESULT_FAILURE;
    }
    return ai->follow_in_formation(target, AI_FORMATION_FLOCK) != 0xFFFFFFFF ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x006B5C20 (:177)
ai_results look_for_trouble_follow_action::do_action(npc* np) const
{
    const unsigned int formation = np->get_ai()->get_formation_handle();
    if (formation == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    const object* leader = ai_formation_get_leader(formation);
    if (leader == nullptr || leader->get_obj_type() != OT_HUMAN || leader->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return AI_RESULT_FAILURE;
    }
    if (static_cast<const npc*>(leader)->get_ai()->planner.get_current_action() != look_for_trouble_on_foot_action::get_instance())
    {
        return AI_RESULT_FAILURE;
    }
    return ai_do_follow_frame(np, formation, HST_STAND) ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x006BC140 (:205)
ai_results look_for_trouble_follow_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->get_ai()->leave_formation();
    }
    return AI_RESULT_SUCCESS;
}

// 0x00675210 (:220)
int look_for_trouble_in_vehicle_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED) != AI_VEHICLE_SEAT_TYPE_DRIVER || !ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER))
    {
        return -1;
    }
    return object_road_get_closest_road(vp->pos, np->npc_flags.can_use_special_roads, 100.0f, false, false) != nullptr ? 1000 : -1;
}

// 0x00656130 (:247)
ai_results look_for_trouble_in_vehicle_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        ai->guard_node_watch_period.invalidate();
        ai->desired_destination_object = 0xFFFFFFFF;
        if (!civilian_pick_destination_on_current_road(np))
        {
            return AI_RESULT_FAILURE;
        }
        vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
        if (!vp->vai.vai_flags.m_jumping)
        {
            vehicle_ai_clear_speed_override(&vp->vai);
        }
        ai->flags.played_stuck_line = false;
    }
    return AI_RESULT_SUCCESS;
}

// 0x00690F30 (:280)
ai_results look_for_trouble_in_vehicle_action::do_action(npc* np) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || !human_is_driving(np) || !ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER))
    {
        return AI_RESULT_FAILURE;
    }
    return civilian_drive_do_action_common(np, vp, false) ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x005E17F0 (:293)
ai_results look_for_trouble_in_vehicle_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this && vehicle_get_honked_horn(np->vehicle_handle))
    {
        automobile_release_horn(np->vehicle_handle);
    }
    return AI_RESULT_SUCCESS;
}

look_for_trouble_on_foot_action look_for_trouble_on_foot_action::the_instance; // 0x01D6812C

// 0x006138F0 (.h:13)
look_for_trouble_on_foot_action::look_for_trouble_on_foot_action()
    : ai_action("look for trouble on foot", ai_plan_conditions_from_bits({4, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({139}))
{
    flags.has_enter = 1;
}

ai_action* look_for_trouble_on_foot_action::get_instance()
{
    return &the_instance;
}

// 0x005E16C0
look_for_trouble_on_foot_action::~look_for_trouble_on_foot_action()
{
}

// 0x00613BF0 (.h:13)
int look_for_trouble_on_foot_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

look_for_trouble_follow_action look_for_trouble_follow_action::the_instance; // 0x01D9F9C8

// 0x00613C30 (.h:14)
look_for_trouble_follow_action::look_for_trouble_follow_action()
    : ai_action("look for trouble follow", ai_plan_conditions_from_bits({4, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({139}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* look_for_trouble_follow_action::get_instance()
{
    return &the_instance;
}

// 0x005E16D0
look_for_trouble_follow_action::~look_for_trouble_follow_action()
{
}

// 0x00613F30 (.h:14)
int look_for_trouble_follow_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

look_for_trouble_in_vehicle_action look_for_trouble_in_vehicle_action::the_instance; // 0x01D75728

// 0x00613F60 (.h:17)
look_for_trouble_in_vehicle_action::look_for_trouble_in_vehicle_action()
    : ai_action("look for trouble in vehicle", ai_plan_conditions_from_bits({9, 49, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({139}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* look_for_trouble_in_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005E16E0
look_for_trouble_in_vehicle_action::~look_for_trouble_in_vehicle_action()
{
}

// 0x00614200 (.h:17)
int look_for_trouble_in_vehicle_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

