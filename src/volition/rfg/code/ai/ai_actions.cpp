// Original: volition/rfg/code/ai/ai_actions.cpp

#include "volition/rfg/code/ai/ai_actions.h"

#include "volition/rfg/code/ai/ai_planning_data.h"

#include "volition/rfg/code/ai/ai_script_actions.h"
#include "volition/rfg/code/ai/civilian/civilian_restricted_area_actions.h"
#include "volition/rfg/code/ai/civilian/ambient_actions.h"
#include "volition/rfg/code/ai/civilian/ambient_edf_action.h"
#include "volition/rfg/code/ai/civilian/guerrilla_actions.h"
#include "volition/rfg/code/ai/civilian/pedestrian_actions.h"
#include "volition/rfg/code/ai/combat/combat_actions.h"
#include "volition/rfg/code/ai/custom_actions.h"
#include "volition/rfg/code/ai/combat/combat_cover_actions.h"
#include "volition/rfg/code/ai/combat/combat_draw_reload_actions.h"
#include "volition/rfg/code/ai/combat/combat_maneuver_actions.h"
#include "volition/rfg/code/ai/combat/combat_melee_actions.h"
#include "volition/rfg/code/ai/combat/combat_thrown_actions.h"
#include "volition/rfg/code/ai/combat/combat_turret_actions.h"
#include "volition/rfg/code/ai/combat/acquire_visual_actions.h"
#include "volition/rfg/code/ai/combat/defensive_combat_actions.h"
#include "volition/rfg/code/ai/combat/dodge_actions.h"
#include "volition/rfg/code/ai/combat/flush_out_actions.h"
#include "volition/rfg/code/ai/despawn_actions.h"
#include "volition/rfg/code/ai/escape_actions.h"
#include "volition/rfg/code/ai/follow_actions.h"
#include "volition/rfg/code/ai/guard_actions.h"
#include "volition/rfg/code/ai/misc_actions.h"
#include "volition/rfg/code/ai/patrol_actions.h"
#include "volition/rfg/code/ai/investigation_actions.h"
#include "volition/rfg/code/ai/reaction_actions.h"
#include "volition/rfg/code/ai/search_actions.h"
#include "volition/rfg/code/ai/self_preservation_actions.h"
#include "volition/rfg/code/ai/reinforcement_actions.h"
#include "volition/rfg/code/ai/threaten_actions.h"
#include "volition/rfg/code/ai/vehicle/vehicle_ai_actions.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

int ai_action::num_action_types; // 0x01D67DB0
ai_action* ai_action::action_pool[320]; // 0x01D65220

// Not in rfg.exe, which links every object: an action file nothing else refers to would drop out of the rfg_ai library, and its
// singletons out of action_pool, without a link error. One action of each file; add a file when it lands.
extern ai_action* (* const Ai_action_files[])();
ai_action* (* const Ai_action_files[])() = {
    move_to_location_action::get_instance,
    do_action_node_action::get_instance,
    amb_edf_drive_by_action::get_instance,
    beat_civilian_action::get_instance,
    terrorize_action::get_instance,
    flee_restricted_region_action::get_instance,
    look_for_trouble_on_foot_action::get_instance,
    pedestrian_wander_action::get_instance,
    attack_action::get_instance,
    draw_firearm_action::get_instance,
    go_to_cover_action::get_instance,
    fall_back_action::get_instance,
    melee_action::get_instance,
    attack_grenade_direct_action::get_instance,
    go_to_turret_action::get_instance,
    defensive_combat_cover_action::get_instance,
    dodge_sideways_anim_action::get_instance,
    hold_position_action::get_instance,
    go_to_believed_location_action::get_instance,
    react_to_enemy_action::get_instance,
    search_building_action::get_instance,
    look_at_disturbance_action::get_instance,
    dive_action::get_instance,
    default_despawn_action::get_instance,
    default_escape_player_action::get_instance,
    follow_leader_action::get_instance,
    default_guard_action::get_instance,
    idle_action::get_instance,
    default_patrol_action::get_instance,
    default_reinforcement_action::get_instance,
    goto_vehicle_action::get_instance,
};

// 0x005DB7E0 (:23)
ai_action::ai_action(const char* _action_name, ai_plan_conditions pre, ai_plan_conditions post)
    : pre_conditions(pre), post_conditions(post), action_name(_action_name)
{
    flags.is_production_rule = 0;
    flags.has_enter = 0;
    flags.has_cleanup = 0;
    // 0x140 = MAX_ACTION_TYPES
    if (num_action_types == 320)
    {
        keen::debug::nativePrintArgs("Too many action types! Increase MAX_ACTION_TYPES");
        return;
    }
    action_pool[num_action_types] = this;
    num_action_types++;
}

// 0x005DB6E0 (.h:38)
ai_action::~ai_action()
{
}

// 0x005DB6F0 (.h:105): no cost means the planner never picks it.
int ai_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

// 0x005DB700 (.h:106)
int ai_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

// 0x005DB720 (.h:109)
void ai_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x005DB910 (:61)
ai_results ai_action::enter(npc*, const ai_action*) const
{
    keen::debug::nativePrintArgs("This function should never be called.");
    return AI_RESULT_SUCCESS;
}

// 0x005DB930 (:67)
ai_results ai_action::cleanup(npc*, const ai_action*) const
{
    keen::debug::nativePrintArgs("This function should never be called.");
    return AI_RESULT_SUCCESS;
}

// 0x005DB730 (.h:127)
ai_plan_conditions ai_action::get_pre_conditions() const
{
    return pre_conditions;
}

// 0x005DB750 (.h:128)
ai_plan_conditions ai_action::get_post_conditions() const
{
    return post_conditions;
}

// 0x005DB770 (.h:135)
bool ai_action::include_in_plan() const
{
    return !flags.is_production_rule;
}

// 0x005DB780 (.h:136)
bool ai_action::has_enter_function() const
{
    return flags.has_enter != 0;
}

// 0x005DB790 (.h:137)
bool ai_action::has_cleanup_function() const
{
    return flags.has_cleanup != 0;
}

// 0x005DB7A0 (.h:139)
const ai_action* const* ai_action::get_action_pool()
{
    return action_pool;
}

// 0x005DB7B0 (.h:140)
int ai_action::get_action_pool_size()
{
    return num_action_types;
}

// 0x005DB860 (:50)
ai_production_rule::ai_production_rule(const char* _action_name, ai_plan_conditions pre, ai_plan_conditions post)
    : ai_action(_action_name, pre, post)
{
    flags.is_production_rule = 1;
}

// 0x005DB900
ai_production_rule::~ai_production_rule()
{
}

// 0x005DB8D0 (.h:174)
int ai_production_rule::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 0;
}

// 0x005DB8E0 (.h:177)
int ai_production_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

// 0x005DB8F0 (.h:179)
ai_results ai_production_rule::do_action(npc*) const
{
    return AI_RESULT_FAILURE;
}
