// Original: volition/rfg/code/ai/ai_planning_data.h
// Implementation: volition/rfg/code/ai/ai_planning_data.cpp
//
// Planner setup data: the per-goal and per-action data a goal or action stores while the plan is searched
// (store_setup_data) and commits to the npc when the plan is adopted, and queries on a plan's actions.
#pragma once

#include <stdint.h>
#include "volition/rfg/code/ai/ai_defense_coordination.h"
#include "volition/rfg/code/ai/ai_types.h"
#include "volition/rfg/code/ai/hazard_detection.h"
#include "volition/rfg/code/ai/percept.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/objects/object_cover_node_enums.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/varray.h"

class ai_action;
struct ai_belief;
class ai_goal;
struct ai_planner_search_node;
class cover_node;
struct human_melee_combat_move_data;
class navpoint;
class npc;
class object;
class object_convoy;
class object_patrol;
class rfg_mover;
class turret;
class weapon;

// size 0xE8
struct ai_goal_setup_data
{
    // size 0x1
    struct goal_setup_flags
    {
        bool trapped : 1; // 0x00:0
        bool cover_direct_to_stepout : 1; // 0x00:1
        bool force_pursuit : 1; // 0x00:2
    };

    ai_goal_setup_data(const ai_goal* goal_); // 0x0064D5F0 (:75)
    void get_hazard_physics_info(const npc* np, const object* obj); // 0x006739E0 (.h:111)
    bool is_pursuing_condition(const ai_plan_conditions& conditions) const; // 0x005FE530 (:82)
    const ai_belief* get_attention_belief(const npc* np) const; // 0x005FE540 (:87)
    void set_attention(npc* np, ai_belief** belief_ptr) const; // 0x0068EDA0 (:106)
    ~ai_goal_setup_data(); // 0x0066A090

    const ai_goal* goal; // 0x00
    cover_node* selected_cover_node; // 0x04
    turret* selected_turret; // 0x08
    object* selected_target; // 0x0C
    object* selected_leader; // 0x10
    navpoint* selected_navpoint; // 0x14
    rfg_mover* selected_building; // 0x18
    const object_patrol* selected_patrol; // 0x1C
    const object_convoy* selected_convoy; // 0x20
    farray<unsigned int,4> selected_action_node_set; // 0x24
    const weapon* selected_weapon; // 0x44
    const object* defend_object; // 0x48
    object* use_target; // 0x4C
    ai_belief_types attention_belief_type; // 0x50
    object_type attention_belief_obj_type; // 0x54
    attention_look_at_modes attention_mode; // 0x58
    attention_steering_modes attention_steering_mode; // 0x5C
    attention_awareness_levels attention_awareness_level; // 0x60
    vector position; // 0x64
    vector direction; // 0x70
    float min_drive_dist; // 0x7C
    ai_oppression_actions oppression_action; // 0x80
    hazard_physics_info haz_info; // 0x84
    ai_plan_conditions initial_working_conditions; // 0xBC
    unsigned int search_handle; // 0xD0
    unsigned int defense_assignment_handle; // 0xD4
    ai_defense_assignments defense_assignment; // 0xD8
    cover_node_stances cover_stance; // 0xDC
    cover_node_firing_positions cover_firing_position; // 0xE0
    goal_setup_flags flags; // 0xE4
};

// size 0x40
struct ai_action_setup_data
{
    // size 0x1
    struct action_setup_flags
    {
        bool cover_direct_to_stepout : 1; // 0x00:0
        bool use_road : 1; // 0x00:1
        bool recalculate : 1; // 0x00:2
    };

    ai_action_setup_data(); // 0x005DD490 (:152)

    vector pos; // 0x00
    float distance; // 0x0C
    object* attack_target; // 0x10
    object* use_target; // 0x14
    object* defend_target; // 0x18
    rfg_mover* building_target; // 0x1C
    uint64_t nav_cell_hint; // 0x20
    const human_melee_combat_move_data* melee_move; // 0x28
    animation_action action_anim; // 0x2C
    cover_node_stances cover_stance; // 0x30
    cover_node_firing_positions cover_firing_position; // 0x34
    action_setup_flags flags; // 0x38
};

bool ai_plan_contains_action(const ai_planner_search_node* plan, const ai_action* act); // 0x005DB6B0 (.h:195)
bool ai_plan_contains_action_precondition(const ai_planner_search_node* plan, const ai_plan_conditions& cond); // 0x005FE3F0 (:16)
bool ai_plan_contains_action_postcondition(const ai_planner_search_node* plan, const ai_plan_conditions& cond); // 0x005FE490 (:27)
