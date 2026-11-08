// Original: volition/rfg/code/ai/ai_planning_data.cpp

#include "volition/rfg/code/ai/ai_planning_data.h"

#include "volition/rfg/code/ai/ai_actions.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/objects/humans/npc.h"

// 0x005DB6B0 (.h:195)
bool ai_plan_contains_action(const ai_planner_search_node* plan, const ai_action* act)
{
    for (; plan != nullptr; plan = plan->next_node)
    {
        if (plan->next_action == act)
        {
            return true;
        }
    }
    return false;
}

// 0x005FE3F0 (:16)
bool ai_plan_contains_action_precondition(const ai_planner_search_node* plan, const ai_plan_conditions& cond)
{
    for (; plan != nullptr && plan->next_action != nullptr; plan = plan->next_node)
    {
        if (plan->next_action->get_pre_conditions() & cond)
        {
            return true;
        }
    }
    return false;
}

// 0x005FE490 (:27)
bool ai_plan_contains_action_postcondition(const ai_planner_search_node* plan, const ai_plan_conditions& cond)
{
    for (; plan != nullptr && plan->next_action != nullptr; plan = plan->next_node)
    {
        if (plan->next_action->get_post_conditions() & cond)
        {
            return true;
        }
    }
    return false;
}

// 0x0064D5F0 (:75)
ai_goal_setup_data::ai_goal_setup_data(const ai_goal* goal_)
    : goal(goal_), selected_cover_node(nullptr), selected_turret(nullptr), selected_target(nullptr),
      selected_leader(nullptr), selected_navpoint(nullptr), selected_building(nullptr), selected_patrol(nullptr),
      selected_convoy(nullptr), selected_weapon(nullptr), defend_object(nullptr), use_target(nullptr),
      attention_belief_type(AI_BELIEF_UNDEFINED), attention_belief_obj_type(OT_UNDEFINED),
      attention_mode(ATTENTION_LOOK_AT_NONE), attention_steering_mode(ATTENTION_STEERING_USE_DEFAULT),
      attention_awareness_level(ATTENTION_AWARENESS_NORMAL), position(ZERO_VECTOR), direction(ZERO_VECTOR),
      min_drive_dist(-1.0f), search_handle(0xFFFFFFFF), defense_assignment_handle(0xFFFFFFFF),
      cover_stance(COVER_STANCE_UNDEFINED), cover_firing_position(COVER_FIRE_UNDEFINED)
{
    flags.trapped = false;
    flags.cover_direct_to_stepout = false;
    flags.force_pursuit = false;
    // rfg.exe's constructor does not set oppression_action, haz_info or defense_assignment either.
    selected_action_node_set.remove_all();
}

// 0x0066A090
ai_goal_setup_data::~ai_goal_setup_data()
{
}

// 0x006739E0 (.h:111)
void ai_goal_setup_data::get_hazard_physics_info(const npc* np, const object* obj)
{
    ::get_hazard_physics_info(haz_info, np, obj);
}

// 0x005FE530 (:82)
bool ai_goal_setup_data::is_pursuing_condition(const ai_plan_conditions& conditions) const
{
    return goal->pursues_condition(conditions);
}

// 0x005FE540 (:87)
const ai_belief* ai_goal_setup_data::get_attention_belief(const npc* np) const
{
    if (attention_belief_type == AI_BELIEF_UNDEFINED)
    {
        return nullptr;
    }
    const ai_perception& perception = np->get_ai()->perception;
    if (selected_target != nullptr)
    {
        const ai_belief* belief = perception.get_belief(attention_belief_type, selected_target->get_handle());
        if (belief != nullptr)
        {
            return belief;
        }
    }
    const ai_belief_requirements requirements(0, 0, 0);
    return perception.get_belief(attention_belief_type, requirements, attention_belief_obj_type);
}

// 0x0068EDA0 (:106)
void ai_goal_setup_data::set_attention(npc* np, ai_belief** belief_ptr) const
{
    ai_perception& perception = np->get_ai()->perception;
    if (attention_belief_type == AI_BELIEF_UNDEFINED)
    {
        if (selected_target != nullptr)
        {
            perception.pay_attention(selected_target->get_handle(), AI_BELIEF_UNDEFINED, attention_mode, attention_steering_mode, attention_awareness_level);
        }
        // Original: belief_ptr is left unset on this path.
        return;
    }

    ai_belief* belief = selected_target != nullptr ? perception.get_belief(attention_belief_type, selected_target->get_handle()) : nullptr;
    if (belief == nullptr)
    {
        const ai_belief_requirements requirements(0, 0, 0);
        belief = perception.get_belief(attention_belief_type, requirements, attention_belief_obj_type);
    }

    if (belief != nullptr)
    {
        perception.pay_attention(belief->handle, belief->get_type(), attention_mode, attention_steering_mode, attention_awareness_level);
    }
    else if (selected_target != nullptr)
    {
        perception.pay_attention(selected_target->get_handle(), AI_BELIEF_UNDEFINED, attention_mode, attention_steering_mode, attention_awareness_level);
    }

    if (belief_ptr != nullptr)
    {
        *belief_ptr = belief;
    }
}

// 0x005DD490 (:152)
ai_action_setup_data::ai_action_setup_data()
    : pos(ZERO_VECTOR), distance(0.0f), attack_target(nullptr), use_target(nullptr), defend_target(nullptr),
      building_target(nullptr), nav_cell_hint(0xFFFFFFFFFFFFFFFFull), melee_move(nullptr),
      action_anim(ANIMATION_ACTION_NONE), cover_stance(COVER_STANCE_UNDEFINED), cover_firing_position(COVER_FIRE_UNDEFINED)
{
    flags.cover_direct_to_stepout = false;
    flags.use_road = false;
    flags.recalculate = false;
}
