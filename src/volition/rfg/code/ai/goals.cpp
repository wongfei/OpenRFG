// Original: volition/rfg/code/ai/goals.cpp
//
// rfg.exe passes conditions by bit number and the names are not in rfg.pdb, so working conditions are tested by number
// here too (plan_conditions.cpp lists the working set).

#include "volition/rfg/code/ai/goals.h"

#include "volition/rfg/code/ai/ai_building_logic.h"
#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/ambient_edf_action_manager.h"
#include "volition/rfg/code/ai/ambient_edf_action_manager_internal.h"
#include "volition/rfg/code/ai/follow_actions.h"
#include "volition/rfg/code/ai/reinforcement_actions.h"
#include "volition/rfg/code/ai/ai_defense_coordination.h"
#include "volition/rfg/code/ai/ambient_edf_action_manager_enum.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/activity_continuous_attack.h"
#include "volition/rfg/code/gameplay/activities/activity_courier_chase.h"
#include "volition/rfg/code/gameplay/activities/vrail_system.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/vehicle/vehicle_flyer.h"
#include "volition/rfg/code/objects/object_patrol.h"
#include "volition/rfg/code/objects/object_convoy.h"
#include "volition/rfg/code/ai/belief_iterator.h"
#include "volition/rfg/code/ai/vehicle/vehicle_ai_actions.h"
#include "volition/rfg/code/gameplay/activities/activity_courier_chase_internal.h"
#include "volition/rfg/code/gameplay/activities/raid_common.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/combat/combat_actions.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/combat/combat_cover.h"
#include "volition/rfg/code/ai/combat/combat_cover_actions.h"
#include "volition/rfg/code/ai/combat/combat_maneuver.h"
#include "volition/rfg/code/ai/combat/defensive_combat_actions.h"
#include "volition/rfg/code/ai/guard_actions.h"
#include "volition/rfg/code/ai/threaten_actions.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_house_arrest.h"
#include "volition/rfg/code/gameplay/activities/activity_raid.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/objects/object_restricted_area.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction_utilities.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"
#include "volition/rfg/code/gamewide/tweak_table.h"

#include <math.h>
#include <string.h>

// goal_pool fills in construction order and rank_goals breaks ties by it: see the singletons at the end of this file and
// self_preservation_goals.cpp's, which follow in rfg.exe's .CRT$XC table. A numbered section keeps that across translation
// units, which link order would not.
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG22")

int ai_goal::num_goal_types; // 0x01D67DC8
ai_goal* ai_goal::goal_pool[128]; // 0x01D65740

static bool Disable_threaten_behavior; // 0x01D67DCC
static bool Combat_maneuver_do_player_line_of_fire_checks = true; // 0x016413E2, .data 1, never written
static float Advance_flanking_spread = 5.0f; // 0x016415D8, .data, then the tweak table
static tweak_table_item Tweak_table_item_Advance_flanking_spread("Advance_flanking_spread", &Advance_flanking_spread); // 0x01D6866C
static const float RETREAT_DEFENSIVE_PERIMETER_BUFFER = 1.0f; // 0x012C89C4
static const float RETREAT_DEFENSIVE_PERIMETER_TURRET_BUFFER = 5.0f; // 0x012C89C8

// 0x01642E80: damage taken, shot at and bullet impact beliefs of any handle in the last 5 s (0x1388).
static const ai_belief_specification MELEE_SAFE_POS_BELIEF_SPECIFICATIONS[3] = {
    ai_belief_specification(AI_BELIEF_DAMAGE_TAKEN, 0xFFFFFFFF, 5000, BELIEF_REQUIREMENTS_NONE),
    ai_belief_specification(AI_BELIEF_SHOT_AT, 0xFFFFFFFF, 5000, BELIEF_REQUIREMENTS_NONE),
    ai_belief_specification(AI_BELIEF_BULLET_IMPACT, 0xFFFFFFFF, 5000, BELIEF_REQUIREMENTS_NONE),
};

// Working conditions (plan_conditions.cpp).
static const unsigned int COND_LOADED = 0;
static const unsigned int COND_HOLDING_FIREARM = 2;
static const unsigned int COND_HOLDING_MELEE = 3;
static const unsigned int COND_TARGET_IN_LOS = 23;

// 0x00944EE0
static bool fl_is_equal_eps(const float f1, const float f2, float epsilon)
{
    return fabsf(f1 - f2) < epsilon;
}

static bool has_condition(const ai_plan_conditions& conditions, unsigned int bit)
{
    return conditions & ai_plan_conditions(bit);
}

static gfm_orders* get_ai_squad_orders(const ai_system* ai)
{
    object_squad* squad = ai->get_squad();
    return squad != nullptr ? squad->get_squad_orders() : nullptr;
}

// 0x0065ED40 (:323). LTCG: np EAX, target on the stack.
defensive_combat_check_results do_defensive_combat_check(const npc* np, const human* target)
{
    ai_system* ai = const_cast<npc*>(np)->get_ai();
    gfm_orders* orders = get_ai_squad_orders(ai);
    if (orders == nullptr || (orders->get_orders_type() != GFM_ORDERS_TYPE_DEFEND && orders->get_orders_type() != GFM_ORDERS_TYPE_ROADBLOCK))
    {
        return DEFENSIVE_COMBAT_NOT_ALLOWED;
    }
    const object* defended = orders->get_target_object();
    if (defended == nullptr)
    {
        return DEFENSIVE_COMBAT_NOT_ALLOWED;
    }

    if (defended->get_obj_type() == OT_MOVER && defended->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        const weapon* firearm = ai_combat_get_action_firearm(np);
        if (firearm != nullptr && firearm->w_info->flags.can_scope)
        {
            return ai_building_is_in_building(target->pos, static_cast<const rfg_mover*>(defended)) ? DEFENSIVE_COMBAT_ALLOWED : DEFENSIVE_COMBAT_PREFERRED;
        }
    }
    else if (defended->get_obj_type() != OT_BOUNDING_BOX)
    {
        return DEFENSIVE_COMBAT_NOT_ALLOWED;
    }

    // outside_defensive_perimeter inlined twice; it answers true for a position of exactly zero.
    if (ai->outside_defensive_perimeter(np->pos, false, nullptr))
    {
        return DEFENSIVE_COMBAT_ALLOWED;
    }
    return ai->outside_defensive_perimeter(target->pos, false, nullptr) ? DEFENSIVE_COMBAT_PREFERRED : DEFENSIVE_COMBAT_ALLOWED;
}

// 0x0065F590 (:383): the pursued human's own squad decides, not the pursuer's.
int ai_get_pursuit_time(const npc*, const human* hp)
{
    if (hp != nullptr && hp->get_obj_type() == OT_HUMAN)
    {
        if (hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            return 12000;
        }
        if (hp->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            const player* protected_player = nullptr;
            const object_squad* squad = static_cast<const npc*>(hp)->get_ai()->get_squad();
            if (squad != nullptr)
            {
                protected_player = squad->get_player();
                if (protected_player == nullptr)
                {
                    gfm_orders* orders = squad->get_squad_orders();
                    if (orders != nullptr && orders->get_orders_type() == GFM_ORDERS_TYPE_DEFEND)
                    {
                        const object* defended = orders->get_target_object();
                        // get_target_object<player>
                        protected_player = (defended != nullptr && object_is_player(defended)) ? static_cast<const player*>(defended) : nullptr;
                    }
                }
            }
            return protected_player != nullptr ? 12000 : 8000;
        }
    }
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
    return 8000;
}

// 0x005E9EE0 (:404)
void ai_disable_threaten_behavior()
{
    Disable_threaten_behavior = true;
}

// 0x005E9EF0 (:410)
void ai_enable_threaten_behavior()
{
    Disable_threaten_behavior = false;
}

// 0x005E9F00 (:423)
ai_goal::ai_goal(ai_plan_conditions _post_conditions, const char* _goal_name)
    : post_conditions(_post_conditions), goal_name(_goal_name)
{
    // 0x80 = MAX_GOAL_TYPES
    if (num_goal_types == 128)
    {
        keen::debug::nativePrintArgs("Too many goal types! Increase MAX_GOAL_TYPES!");
        return;
    }
    goal_pool[num_goal_types] = this;
    num_goal_types++;
}

// 0x005DCD50 (.h:115)
ai_goal::~ai_goal()
{
}

// 0x005DCD60 (.h:117)
ai_plan_conditions ai_goal::get_conditions() const
{
    return post_conditions;
}

// 0x005FB860 (.h:118)
bool ai_goal::conditions_satisfied(ai_plan_conditions conditions) const
{
    return !(post_conditions & ~conditions);
}

// 0x005FB8F0 (.h:119)
bool ai_goal::pursues_condition(const ai_plan_conditions& conditions) const
{
    return post_conditions & conditions;
}

// 0x005DCD80 (.h:120)
const char* ai_goal::get_name() const
{
    return goal_name;
}

// 0x005DCD90 (.h:123)
bool ai_goal::store_setup_data(ai_goal_setup_data&, const npc*) const
{
    return true;
}

// 0x005DCDA0 (.h:124)
void ai_goal::commit_setup_data(npc*, const ai_goal_setup_data&) const
{
}

// 0x005E9F60 (:448)
ai_goal* ai_goal::get_goal_by_name(const char* name)
{
    for (int i = 0; i < num_goal_types; i++)
    {
        if (strcmp(goal_pool[i]->goal_name, name) == 0)
        {
            return goal_pool[i];
        }
    }
    return nullptr;
}

// 0x0065F650 (:468)
int wait_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    gfm_orders* orders = get_ai_squad_orders(np->get_ai());
    return (orders != nullptr && orders->get_orders_type() == GFM_ORDERS_TYPE_WAIT) ? 160000 : -1;
}

// 0x005E9FC0 (:486)
int idle_goal::get_fitness(const npc*, ai_plan_conditions) const
{
    return 1;
}

// 0x005E9FD0 (:497)
int idle_despawn_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (np->h_info->flags.vip || np->npc_flags.disallow_idle_despawn)
    {
        return -1;
    }
    if (np->current_team == HUMAN_TEAM_MARAUDER && npc_marauders_should_retreat())
    {
        return 100000;
    }
    if (np->npc_flags.allow_idle_despawn)
    {
        return np->get_lifetime() < 5000 ? -1 : 20;
    }
    return -1;
}

// 0x005EA040 (:534)
bool idle_despawn_goal::store_setup_data(ai_goal_setup_data&, const npc*) const
{
    return true;
}

// 0x005EA050 (:539)
void idle_despawn_goal::commit_setup_data(npc* np, const ai_goal_setup_data&) const
{
    np->npc_flags.converted_guerrilla = 0;
    np->start_active_despawn(NPC_DESPAWN_TYPE_LOW, nullptr);
}

// 0x006369F0 (:555)
int despawn_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const ai_system* ai = np->get_ai();
    if (!np->should_despawn())
    {
        return -1;
    }
    const vehicle* vp = human_in_vehicle(np);
    if ((vp == nullptr || !vp->m_flags.m_despawn_after_fade) && !np->npc_flags.despawn_after_fade
        && !ai->can_despawn(nullptr) && !np->npc_flags.high_priority_despawn)
    {
        return np->npc_flags.ambient_spawn ? 20 : 3250;
    }
    return 100000;
}

// 0x0067BCA0 (:580)
int kill_enemy_goal::get_fitness(const npc* np, ai_plan_conditions working_conditions) const
{
    if (human_team_is_non_combatant(np))
    {
        return -1;
    }
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed() || alm_grace_is_active(np, target) || !has_condition(working_conditions, COND_TARGET_IN_LOS))
    {
        return -1;
    }
    return np->get_ai()->perception.can_engage_target_reason(target) == AI_ENGAGE_ALLOWED ? 2500 : -1;
}

// 0x005EA080 (:612)
bool kill_enemy_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return false;
    }
    goal_setup_data.selected_target = target;
    return true;
}

// 0x006A0E50 (:627)
void kill_enemy_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (goal_setup_data.selected_target != nullptr)
    {
        np->get_ai()->perception.pay_attention(goal_setup_data.selected_target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_USE_CURRENT_MODE, ATTENTION_STEERING_USE_CURRENT_MODE, ATTENTION_AWARENESS_NORMAL);
    }
    ai_combat_post_attack_events(np, goal_setup_data.selected_target);
}

// 0x0067BD90 (:640)
int melee_goal::get_fitness(const npc* np, ai_plan_conditions working_conditions) const
{
    const timestamp& melee_time = np->combat.move_to_melee_timestamp;
    if ((melee_time.valid() && !melee_time.elapsed()) || human_team_is_non_combatant(np) || human_in_vehicle(np) != nullptr
        || (human_in_vehicle_transition(np, nullptr, VI_INTERACTION_ENTER) && (np->cur_state == HS_PLAYER_PATHING || np->cur_state == HS_VEHICLE_ENTERING)))
    {
        return -1;
    }
    if (!has_condition(working_conditions, COND_TARGET_IN_LOS))
    {
        return -1;
    }
    const inventory_item* item = inv_item_get_best_weapon(np, false);
    if (item == nullptr)
    {
        return -1;
    }
    const weapon* best_weapon = World.object_from_handle<weapon>(item->weapon_handle);
    if (best_weapon == nullptr)
    {
        return -1;
    }

    const ai_perception& perception = np->get_ai()->perception;
    const human* target = human_combat_get_target(np);
    if (human_is_dead(target) || human_in_vehicle(target) != nullptr || perception.can_engage_target_reason(target) != AI_ENGAGE_ALLOWED || alm_grace_is_active(np, target))
    {
        return -1;
    }
    const npc* target_npc = World.object_cast<npc>(const_cast<human*>(target));
    if (target_npc != nullptr && target_npc->get_ai()->oppression_action == AI_OPPRESSION_SHOOT_THIS_GUY)
    {
        return -1;
    }

    bool melee_only = has_condition(working_conditions, COND_HOLDING_MELEE);
    if (!melee_only && !has_condition(working_conditions, COND_HOLDING_FIREARM) && best_weapon->w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE)
    {
        melee_only = true;
    }

    const float dist_squared = vec_dist_squared(np->pos, target->pos);
    const int fitness = dist_squared <= 5.0f ? 3500 : 3200;

    const turret* manned = human_combat_get_manned_turret(np);
    if (manned != nullptr)
    {
        vector head;
        human_get_head_pos(target, head);
        if (manned->can_hit_target(head))
        {
            const weapon_info* turret_weapon = manned->t_info->w_info;
            if (!turret_weapon->is_explosive())
            {
                return -1;
            }
            const float radius = turret_weapon->m_explosion_info->m_secondary_radius;
            if (radius * radius <= dist_squared)
            {
                return -1;
            }
        }
    }

    if (melee_only)
    {
        return fitness;
    }

    const ai_combat_should_charge_results charge = ai_combat_should_charge_target(np);
    if (charge != AI_COMBAT_SHOULD_NOT_CHARGE && best_weapon->get_range_category(sqrtf(dist_squared)) < 2)
    {
        return charge == AI_COMBAT_MUST_CHARGE ? 51500 : fitness;
    }

    // A loaded firearm with a quarter of its magazine left only melees closer in.
    float range = 5.0f;
    if (has_condition(working_conditions, COND_LOADED) && has_condition(working_conditions, COND_HOLDING_FIREARM))
    {
        const weapon* held = human_weapons_get_weapon(np);
        if (0.25 <= static_cast<double>(held->get_rounds_in_magazine()) / static_cast<double>(held->w_info->magazine_size))
        {
            range = 3.0f;
        }
    }
    if (np->current_team == HUMAN_TEAM_MARAUDER)
    {
        const float engagement = best_weapon->w_info->min_engagement_dist * 1.2f;
        if (range - engagement < 0.0f)
        {
            range = engagement;
        }
    }
    if (human_teams_are_allies(np, HUMAN_TEAM_EDF) && !human_combat_is_manning_turret(np) && human_weapons_get_weapon(target) == nullptr && range - 7.0f < 0.0f)
    {
        range = 7.0f;
    }
    return perception.has_clear_line_of_travel(target, range, 1.2f) ? fitness : -1;
}

// 0x005EA0C0 (:793)
bool melee_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    human* target = human_combat_get_target(np);
    goal_setup_data.selected_target = target;
    if (target == nullptr)
    {
        return false;
    }
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_OBJECT;
    return true;
}

// 0x006A0E90 (:804)
void melee_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    goal_setup_data.set_attention(np, nullptr);
    ai_combat_post_attack_events(np, goal_setup_data.selected_target);
}

// 0x0067C250 (:815)
int melee_vehicle_goal::get_fitness(const npc* np, ai_plan_conditions working_conditions) const
{
    if (np->current_team == HUMAN_TEAM_MARAUDER)
    {
        const inventory_item* grenade = inv_item_get_weapon_from_class(np, WEAPON_CLASS_THROWN_GRENADE);
        if (grenade != nullptr && grenade->weapon_handle != 0xFFFFFFFF && World.object_from_handle<weapon>(grenade->weapon_handle) != nullptr)
        {
            return -1;
        }
    }
    if (human_team_is_non_combatant(np) || human_in_vehicle(np) != nullptr || human_combat_get_manned_turret(np) != nullptr
        || (human_in_vehicle_transition(np, nullptr, VI_INTERACTION_ENTER) && (np->cur_state == HS_PLAYER_PATHING || np->cur_state == HS_VEHICLE_ENTERING))
        || !has_condition(working_conditions, COND_TARGET_IN_LOS))
    {
        return -1;
    }
    if (!has_condition(working_conditions, COND_HOLDING_MELEE))
    {
        const inventory_item* melee = inv_item_get_best_melee(np);
        if (melee == nullptr || World.object_from_handle<weapon>(melee->weapon_handle) == nullptr)
        {
            return -1;
        }
    }
    const human* target = human_combat_get_target(np);
    if (human_is_dead(target) || np->get_ai()->perception.can_engage_target_reason(target) != AI_ENGAGE_ALLOWED || alm_grace_is_active(np, target))
    {
        return -1;
    }
    const vehicle* vp = human_in_vehicle(target);
    return (vp != nullptr && !vehicle_is_on_fire(vp)) ? 3510 : -1;
}

// 0x005EA0F0 (:885)
bool melee_vehicle_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed() || human_in_vehicle(target) == nullptr)
    {
        return false;
    }
    goal_setup_data.selected_target = target;
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_OBJECT;
    return true;
}

// 0x006A0EC0 (:897)
void melee_vehicle_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    goal_setup_data.set_attention(np, nullptr);
    ai_combat_post_attack_events(np, goal_setup_data.selected_target);
}

// 0x0067C410 (:908)
int melee_safe_position_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (np->current_team == HUMAN_TEAM_MARAUDER || human_team_is_non_combatant(np) || human_in_vehicle_interaction(np, nullptr))
    {
        return -1;
    }
    const inventory_item* item = inv_item_get_best_weapon(np, false);
    if (item == nullptr || item->weapon_handle == 0xFFFFFFFF)
    {
        return -1;
    }
    const weapon* best_weapon = World.object_from_handle<weapon>(item->weapon_handle);
    if (best_weapon == nullptr || best_weapon->w_info->ammo_type != WEAPON_AMMO_TYPE_MELEE)
    {
        return -1;
    }

    const ai_system* ai = np->get_ai();
    const human* target = human_combat_get_target(np);
    if (target == nullptr || human_is_dead(target) || human_in_vehicle(target) != nullptr
        || ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, target) == nullptr
        || ai->perception.can_engage_target_reason(target) != AI_ENGAGE_ALLOWED || alm_grace_is_active(np, target))
    {
        return -1;
    }

    if (!human_combat_allowed_to_damage_target(np, target))
    {
        return 3520;
    }
    if (ai->planner.get_current_goal() == this && human_combat_cover_is_using_node(np))
    {
        const cover_node* node = human_combat_cover_get_node(np);
        if (node != nullptr && !node->is_disabled())
        {
            return 3520;
        }
    }

    ai_belief* beliefs[20];
    const int count = ai->perception.collect_all_matching_beliefs(beliefs, 20, MELEE_SAFE_POS_BELIEF_SPECIFICATIONS, 3, 0xFFFFFFFF);
    for (int i = 0; i < count; i++)
    {
        const human* other = World.object_from_handle<human>(beliefs[i]->handle);
        if (!human_is_dead(other) && human_teams_are_enemies(np, other))
        {
            return 3520;
        }
    }
    return -1;
}

// 0x0067C5E0 (:980)
bool melee_safe_position_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    if (human_in_vehicle_interaction(np, nullptr))
    {
        return false;
    }
    human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return false;
    }
    if (human_combat_allowed_to_damage_target(np, target) && np->get_ai()->perception.has_clear_line_of_travel(target, 5.0f, 1.2f))
    {
        return false;
    }
    goal_setup_data.selected_target = target;
    goal_setup_data.attention_mode = ATTENTION_USE_CURRENT_MODE;
    return true;
}

// 0x006A0EF0 (:1006)
void melee_safe_position_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    goal_setup_data.set_attention(np, nullptr);
    ai_combat_post_attack_events(np, goal_setup_data.selected_target);
}

static bool is_going_to_cover(const npc* np)
{
    const ai_action* act = np->get_ai()->planner.get_current_action();
    return act != nullptr && (act == go_to_cover_action::get_instance() || act == defensive_combat_cover_action::get_instance());
}

// 0x0067C680 (:1017)
int flush_out_target_goal::get_fitness(const npc* np, ai_plan_conditions working_conditions) const
{
    if (human_team_is_non_combatant(np) || is_going_to_cover(np) || activity_house_arrest_is_human_hostage(np))
    {
        return -1;
    }
    const human* target = human_combat_get_target(np);
    if (target == nullptr || alm_grace_is_active(np, target) || has_condition(working_conditions, COND_TARGET_IN_LOS))
    {
        return -1;
    }
    const ai_system* ai = np->get_ai();
    const ai_goal* current = ai->planner.get_current_goal();
    if (current == acquire_target_goal::get_instance())
    {
        return -1;
    }
    if (current != this && fl_rand() < 0.25f)
    {
        return -1;
    }
    if (ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, target) == nullptr || ai->perception.can_engage_target_reason(target) != AI_ENGAGE_ALLOWED)
    {
        return -1;
    }
    return do_defensive_combat_check(np, target) == DEFENSIVE_COMBAT_PREFERRED ? 2750 : 1850;
}

// 0x0065F6A0 (:1095)
bool flush_out_target_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_perception& perception = np->get_ai()->perception;
    human* target = human_combat_get_target(np);
    if (target == nullptr || perception.get_belief(AI_BELIEF_ENEMY_POSITION, target->get_handle()) == nullptr || perception.enemy_in_los(target->get_handle()))
    {
        return false;
    }
    goal_setup_data.attention_belief_obj_type = OT_HUMAN;
    goal_setup_data.attention_belief_type = AI_BELIEF_ENEMY_POSITION;
    goal_setup_data.attention_mode = ATTENTION_USE_CURRENT_MODE;
    goal_setup_data.selected_target = target;
    goal_setup_data.attention_steering_mode = static_cast<attention_steering_modes>(ATTENTION_STEERING_TURN_TO_TARGET + (human_combat_cover_is_using_node(np) ? 1 : 0));
    return true;
}

// 0x006A0F20 (:1123)
void flush_out_target_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    goal_setup_data.set_attention(np, nullptr);
}

// 0x0067C7C0 (:1131)
int acquire_target_goal::get_fitness(const npc* np, ai_plan_conditions working_conditions) const
{
    if (human_team_is_non_combatant(np))
    {
        return -1;
    }
    if (Combat_maneuver_do_player_line_of_fire_checks)
    {
        const player* local = player_get_local_player();
        const human* line_target = human_combat_get_target(np);
        if (local != nullptr && line_target != nullptr && human_teams_get_disposition(local, np) == TEAM_DISPOSITION_ALLIED
            && combat_maneuver_path_crosses_player_line_of_fire(np, line_target->pos, nullptr))
        {
            return -1;
        }
    }
    if (is_going_to_cover(np))
    {
        return -1;
    }
    const ai_perception& perception = np->get_ai()->perception;
    const human* target = human_combat_get_target(np);
    if (target == nullptr || perception.get_belief(AI_BELIEF_ENEMY_POSITION, target) == nullptr || alm_grace_is_active(np, target)
        || has_condition(working_conditions, COND_TARGET_IN_LOS) || perception.can_engage_target_reason(target) != AI_ENGAGE_ALLOWED)
    {
        return -1;
    }
    return 1700;
}

// 0x00636A80 (:1172)
bool acquire_target_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    human* target = human_combat_get_target(np);
    if (target == nullptr || np->get_ai()->perception.get_belief(AI_BELIEF_ENEMY_POSITION, target->get_handle()) == nullptr)
    {
        return false;
    }
    goal_setup_data.selected_target = target;
    goal_setup_data.attention_belief_obj_type = OT_HUMAN;
    goal_setup_data.attention_belief_type = AI_BELIEF_ENEMY_POSITION;
    goal_setup_data.attention_mode = ATTENTION_ACQUIRE_VISUAL;
    return true;
}

// 0x006A0F40 (:1189)
void acquire_target_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    goal_setup_data.set_attention(np, nullptr);
}

// 0x00636AF0 (:1200)
int retreat_defensive_perimeter_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    ai_system* ai = const_cast<npc*>(np)->get_ai();
    gfm_orders* orders = get_ai_squad_orders(ai);
    if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_DEFEND)
    {
        return -1;
    }
    const object* defended = orders->get_target_object();
    if (defended != nullptr && defended->get_obj_type() == OT_HUMAN)
    {
        return -1;
    }

    const float* buffer_dist = nullptr;
    if (ai->planner.get_current_goal() != this)
    {
        buffer_dist = human_combat_get_manned_turret(np) != nullptr ? &RETREAT_DEFENSIVE_PERIMETER_TURRET_BUFFER : &RETREAT_DEFENSIVE_PERIMETER_BUFFER;
    }

    // outside_defensive_perimeter is inlined four times here.
    if (!ai->outside_defensive_perimeter(np->pos, false, buffer_dist))
    {
        return -1;
    }
    if (ai->planner.get_current_goal() != this && np->movement_mode == HMM_PATHFINDING
        && !ai->outside_defensive_perimeter(np->pf.eventual_goal_pos, false, buffer_dist))
    {
        return -1;
    }

    int fitness = ai->perception.on_alert() ? 3100 : 400;
    if ((np->hflags.activity_raid_required || np->hflags.activity_raid_optional)
        && (activity_is_active(ACTIVITY_MODE_RAID_ATTACK) || activity_is_active(ACTIVITY_MODE_RAID_DEFEND))
        && !human_is_driving(np) && activity_raid_is_in_raid_region(np->pos))
    {
        fitness = 2050;
    }

    if (!ai->outside_defensive_perimeter(orders->get_location(), false, nullptr))
    {
        return fitness;
    }
    if (defended == nullptr)
    {
        return -1;
    }
    return ai->outside_defensive_perimeter(defended->pos, false, nullptr) ? -1 : fitness;
}

// 0x00637260 (:1269)
bool retreat_defensive_perimeter_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    ai_system* ai = const_cast<npc*>(np)->get_ai();
    gfm_orders* orders = get_ai_squad_orders(ai);
    if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_DEFEND)
    {
        return false;
    }
    const object* defended = orders->get_target_object();
    if (defended == nullptr || defended->get_obj_type() == OT_HUMAN)
    {
        return false;
    }
    const float* buffer_dist = nullptr;
    if (ai->planner.get_current_goal() != this)
    {
        buffer_dist = human_combat_is_manning_turret(np) ? &RETREAT_DEFENSIVE_PERIMETER_TURRET_BUFFER : &RETREAT_DEFENSIVE_PERIMETER_BUFFER;
    }
    if (!ai->outside_defensive_perimeter(np->pos, false, buffer_dist))
    {
        return false;
    }
    goal_setup_data.defend_object = defended;
    goal_setup_data.min_drive_dist = 30.0f;
    return true;
}

// 0x005EA140 (:1314)
void retreat_defensive_perimeter_goal::commit_setup_data(npc*, const ai_goal_setup_data&) const
{
}

// 0x00637570 (:1322)
object* get_gawk_target(const npc* np)
{
    const ai_system* ai = np->get_ai();
    const ai_belief* head = ai->perception.get_first_belief(AI_BELIEF_VEHICLE);
    for (const ai_belief* belief = head; belief != nullptr;)
    {
        // Seen within 0x259 ms, flags 0x380 and not 0x40; a flyer, or a vehicle with 0x400.
        if (belief->update_time.time_since() < 601 && (belief->flags & 0x40) == 0 && (belief->flags & 0x380) != 0 && belief->handle != 0xFFFFFFFF)
        {
            object* objp = World.object_from_handle(belief->handle);
            if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && !objp->is_killed()
                && (objp->get_sub_type() == OT_SUB_VEHICLE_FLYER || (belief->flags & 0x400) != 0)
                && !BELIEF_REQUIREMENTS_HEAVY_VEHICLE_THREAT.test(belief->flags))
            {
                // Farther (MAX_EXPANDED_GAWK_DIST_SQ) once gawking.
                const float max_dist_squared = ai->planner.get_current_goal() == gawk_goal::get_instance() ? 625.0f : 400.0f;
                if (vec_dist_squared(objp->pos, np->pos) <= max_dist_squared)
                {
                    return objp;
                }
            }
        }
        belief = belief->next;
        if (belief == head)
        {
            break;
        }
    }
    const ai_belief* destruction = ai->perception.get_first_belief(AI_BELIEF_LARGE_DESTRUCTION);
    if (destruction == nullptr || destruction->handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(destruction->handle);
    return (objp != nullptr && objp->get_obj_type() == OT_MOVER) ? objp : nullptr;
}

// 0x006376C0 (:1372)
int gawk_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const timestamp_pair& timer = np->get_ai()->gawk_timer;
    if (timer.next.valid() && !timer.next.elapsed() && timer.last.valid() && timer.last.elapsed())
    {
        return -1;
    }
    const vehicle* vp = human_in_vehicle(np);
    if (vp != nullptr && (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER || vehicle_is_tank(vp) || vp->get_sub_type() == OT_SUB_VEHICLE_FLYER))
    {
        return -1;
    }
    return get_gawk_target(np) != nullptr ? 150 : -1;
}

// 0x00637760 (:1400)
bool gawk_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    object* target = get_gawk_target(np);
    if (target == nullptr)
    {
        return false;
    }
    goal_setup_data.selected_target = target;
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_OBJECT;
    return true;
}

// 0x006A0F60 (:1413)
void gawk_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    ai_system* ai = np->get_ai();
    goal_setup_data.set_attention(np, nullptr);
    if (ai->gawk_timer.next.valid() && !ai->gawk_timer.next.elapsed())
    {
        return;
    }
    ai->gawk_timer.next.set_rand(30000, 45000);
    ai->gawk_timer.last.set_rand(5000, 15000);
}

// 0x005EA150 (:1426)
int look_for_trouble_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (np->current_team == HUMAN_TEAM_GUERILLA)
    {
        const mission_info* info = mission_info_get(mission_get_current_mission());
        if (info != nullptr && info->mis_mode == MISSION_MODE_FMC)
        {
            return 200;
        }
    }
    return -1;
}

// 0x005EA190 (:1446)
int respond_to_honk::get_fitness(const npc* np, ai_plan_conditions) const
{
    return np->npc_flags.honked_at ? 110 : -1;
}

// 0x0067C900 (:1464)
int defensive_combat_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (human_team_is_non_combatant(np))
    {
        return -1;
    }
    const ai_perception& perception = np->get_ai()->perception;
    const human* target = human_combat_get_target(np);
    if (target != nullptr && perception.get_belief(AI_BELIEF_ENEMY_POSITION, target->get_handle()) != nullptr)
    {
        if (alm_grace_is_active(np, target))
        {
            return -1;
        }
        const ai_engage_target_results reason = perception.can_engage_target_reason(target);
        if (reason != AI_ENGAGE_ALLOWED && reason != AI_ENGAGE_NOT_ALLOWED_TOO_MANY)
        {
            return -1;
        }
        if (activity_house_arrest_is_human_hostage(np))
        {
            return 2700;
        }
        const defensive_combat_check_results check = do_defensive_combat_check(np, target);
        if (check == DEFENSIVE_COMBAT_PREFERRED)
        {
            return 2700;
        }
        return check == DEFENSIVE_COMBAT_ALLOWED ? 1600 : -1;
    }
    return activity_raid_human_has_raid_flags(np) ? 2650 : -1;
}

// 0x0065F730 (:1509)
bool defensive_combat_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_system* ai = np->get_ai();
    human* target = human_combat_get_target(np);
    vector prepare_pos = ZERO_VECTOR;
    const bool prepare = activity_raid_npc_should_prepare(np);
    if (prepare)
    {
        // The raid position is the dummy child of the squad's spawn node.
        const object_squad* squad = ai->get_squad();
        if (squad == nullptr || squad->spawn_node_handle == 0xFFFFFFFF)
        {
            return false;
        }
        const object* node = World.object_from_handle(squad->spawn_node_handle);
        if (node == nullptr || node->get_obj_type() != OT_SQUAD_SPAWN_NODE)
        {
            return false;
        }
        const object* dummy = node->child_ptr;
        while (dummy != nullptr && dummy->get_obj_type() != OT_DUMMY)
        {
            dummy = dummy->child_next;
            if (dummy == node->child_ptr)
            {
                return false;
            }
        }
        if (dummy == nullptr)
        {
            return false;
        }
        prepare_pos = dummy->pos;
    }

    if ((target == nullptr || ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, target->get_handle()) == nullptr) && !activity_raid_npc_should_prepare(np))
    {
        return false;
    }
    gfm_orders* orders = get_ai_squad_orders(ai);
    if (orders == nullptr || (orders->get_orders_type() != GFM_ORDERS_TYPE_DEFEND && orders->get_orders_type() != GFM_ORDERS_TYPE_ROADBLOCK))
    {
        return false;
    }
    const object* defended = orders->get_target_object();
    if (defended == nullptr)
    {
        return false;
    }
    goal_setup_data.defend_object = defended;
    goal_setup_data.attention_belief_obj_type = OT_HUMAN;
    goal_setup_data.attention_belief_type = AI_BELIEF_ENEMY_POSITION;
    const bool first_contact = ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr;
    goal_setup_data.position = prepare_pos;
    goal_setup_data.selected_target = target;
    goal_setup_data.attention_steering_mode = ATTENTION_STEERING_TURN_TO_TARGET;
    // Without first contact it acquires a visual: 5 + 4 in rfg.exe.
    goal_setup_data.attention_mode = first_contact ? ATTENTION_AIM_AT_OBJECT : ATTENTION_ACQUIRE_VISUAL;
    return true;
}

// 0x006A0FD0 (:1575)
void defensive_combat_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (activity_raid_npc_should_prepare(np))
    {
        np->get_ai()->desired_target_pos = goal_setup_data.position;
    }
    else
    {
        goal_setup_data.set_attention(np, nullptr);
    }
    ai_combat_post_attack_events(np, goal_setup_data.selected_target);
}

// 0x0067C9D0 (:1599)
int combat_support_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (human_team_is_non_combatant(np))
    {
        return -1;
    }
    const vehicle* vp = human_in_vehicle(np);
    if (vp != nullptr && human_is_driving(np) && ai_vehicle_has_manned_turret(vp))
    {
        return -1;
    }
    const ai_perception& perception = np->get_ai()->perception;
    const human* target = human_combat_get_target(np);
    if (target == nullptr || perception.get_belief(AI_BELIEF_ENEMY_POSITION, target->get_handle()) == nullptr || alm_grace_is_active(np, target))
    {
        return -1;
    }
    return perception.can_engage_target_reason(target) == AI_ENGAGE_NOT_ALLOWED_TOO_MANY ? 1300 : -1;
}

// 0x00637790 (:1633)
bool combat_support_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    human* target = human_combat_get_target(np);
    if (target == nullptr || np->get_ai()->perception.get_belief(AI_BELIEF_ENEMY_POSITION, target->get_handle()) == nullptr)
    {
        return false;
    }
    goal_setup_data.selected_target = target;
    goal_setup_data.attention_mode = ATTENTION_AIM_AT_OBJECT;
    return true;
}

// 0x006A1040 (:1648)
void combat_support_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    goal_setup_data.set_attention(np, nullptr);
}

// 0x006377F0 (:1656)
int adjust_cover_stance_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (human_team_is_non_combatant(np))
    {
        return -1;
    }
    const ai_perception& perception = np->get_ai()->perception;
    const cover_node* node = human_combat_cover_get_node(np);
    if (node == nullptr || !human_combat_cover_is_using_node(np))
    {
        return -1;
    }
    const human* target = human_combat_get_target(np);
    if (target == nullptr || alm_grace_is_active(np, target) || perception.get_belief(AI_BELIEF_ENEMY_POSITION, target) == nullptr)
    {
        return -1;
    }
    if (human_combat_cover_is_stepped_out(np) && perception.get_attention_mode() == ATTENTION_AIM_AT_OBJECT && perception.can_hit_target())
    {
        return -1;
    }
    return cover_get_new_stance(np, node, target->pos) != cover_stance_from_state(np) ? 2800 : -1;
}

// 0x006378D0 (:1705)
bool adjust_cover_stance_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_perception& perception = np->get_ai()->perception;
    const cover_node* node = human_combat_cover_get_node(np);
    if (node == nullptr || !human_combat_cover_is_using_node(np))
    {
        return false;
    }
    const human* target = human_combat_get_target(np);
    if (target == nullptr || perception.get_belief(AI_BELIEF_ENEMY_POSITION, target) == nullptr)
    {
        return false;
    }
    if (human_combat_cover_is_stepped_out(np) && perception.get_attention_mode() == ATTENTION_AIM_AT_OBJECT && perception.can_hit_target())
    {
        return false;
    }
    const cover_node_stances current = cover_stance_from_state(np);
    const cover_node_stances wanted = cover_get_new_stance(np, node, target->pos);
    if (wanted == current)
    {
        return false;
    }
    goal_setup_data.cover_stance = wanted;
    return true;
}

// 0x005EA1B0 (:1746)
void adjust_cover_stance_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    np->get_ai()->desired_cover_stance = goal_setup_data.cover_stance;
}

// 0x005EA1D0 (:1755)
int reload_weapon_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (human_team_is_non_combatant(np))
    {
        return -1;
    }
    const weapon* wp = human_weapons_get_weapon(np);
    if (wp == nullptr || human_combat_get_reserved_turret(np) != nullptr)
    {
        return -1;
    }
    const float fraction = static_cast<float>(wp->get_rounds_in_magazine()) / static_cast<float>(wp->w_info->magazine_size);
    if (fraction <= 0.2f)
    {
        return 3700;
    }
    return fraction <= 0.5f ? 1500 : -1;
}

// 0x0065F920 (:1798)
int dodge_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const ai_system* ai = np->get_ai();
    const timestamp& maneuver = np->combat.combat_maneuver_timestamp;
    if (maneuver.valid() && !maneuver.elapsed())
    {
        if (ai->planner.get_current_action() != attack_action::get_instance() || ai->planner.get_time_in_current_action() < 1000)
        {
            return -1;
        }
    }
    if (np->cur_stance == HST_PRONE || human_in_vehicle(np) != nullptr || human_is_flinching(np) || human_combat_get_manned_turret(np) != nullptr)
    {
        return -1;
    }
    const ai_belief* contact = ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle);
    if (contact == nullptr || contact->update_time.time_since() < 2000)
    {
        return -1;
    }
    const ai_belief_specification specifications[2] = {
        ai_belief_specification(AI_BELIEF_SHOT_AT, 0xFFFFFFFF, 1000, BELIEF_REQUIREMENTS_NONE),
        ai_belief_specification(AI_BELIEF_BULLET_IMPACT, np->combat.target_handle, 500, BELIEF_REQUIREMENTS_NONE),
    };
    return ai->perception.find_matching_belief(specifications, 2, 0xFFFFFFFF) != nullptr ? 3400 : -1;
}

// 0x0065FA80 (:1843)
bool dodge_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    if (np->cur_stance == HST_PRONE || human_in_vehicle(np) != nullptr || human_is_flinching(np) || human_combat_get_manned_turret(np) != nullptr)
    {
        return false;
    }
    const ai_belief_specification specifications[2] = {
        ai_belief_specification(AI_BELIEF_SHOT_AT, 0xFFFFFFFF, 1000, BELIEF_REQUIREMENTS_NONE),
        ai_belief_specification(AI_BELIEF_BULLET_IMPACT, np->combat.target_handle, 500, BELIEF_REQUIREMENTS_NONE),
    };
    ai_belief* belief = nullptr;
    if (np->get_ai()->perception.collect_all_matching_beliefs(&belief, 1, specifications, 2, 0xFFFFFFFF) < 1 || belief == nullptr)
    {
        return false;
    }
    human* attacker = World.object_from_handle<human>(belief->handle);
    if (attacker == nullptr)
    {
        return false;
    }
    goal_setup_data.position = belief->pos;
    goal_setup_data.selected_target = attacker;
    goal_setup_data.attention_belief_type = belief->get_type();
    return true;
}

// 0x005EA280 (:1874)
void dodge_goal::commit_setup_data(npc*, const ai_goal_setup_data&) const
{
}

// 0x0067CA80 (:1895)
bool maneuver_goal_get_range_common(weapon_range_categories& range_cat, ai_combat_action_parameters& params, ai_plan_conditions working_conditions, bool allow_from_cover, bool skip_los_and_engage_checks)
{
    // Holding a melee weapon or having no weapon at all (data[0] & 0x48).
    if (has_condition(working_conditions, COND_HOLDING_MELEE) || has_condition(working_conditions, 6))
    {
        return false;
    }
    if (!allow_from_cover && has_condition(working_conditions, 60))
    {
        return false;
    }

    if (params.weapon_inst == nullptr || !params.weapon_inst->w_info->is_firearm())
    {
        const inventory_item* item = inv_item_get_best_firearm(params.np);
        if (item == nullptr)
        {
            return false;
        }
        params.set_weapon(item->weapon_handle != 0xFFFFFFFF ? World.object_from_handle<weapon>(item->weapon_handle) : nullptr);
    }
    if (params.weapon_inst == nullptr || !params.weapon_inst->w_info->is_firearm() || human_combat_get_manned_turret(params.np) != nullptr)
    {
        return false;
    }

    const vehicle* vp = World.object_from_handle<vehicle>(params.np->vehicle_handle);
    if (vp != nullptr)
    {
        const human* gunner = vehicle_get_human_in_gunner_seat(vp);
        if (gunner != nullptr && params.np != gunner)
        {
            return false;
        }
    }
    if (!params.init_target(false) || human_is_dead(params.target))
    {
        return false;
    }
    if (!skip_los_and_engage_checks)
    {
        const ai_perception& perception = params.np->get_ai()->perception;
        if (!perception.enemy_in_los(params.target->get_handle()) || perception.can_engage_target_reason(params.target) != AI_ENGAGE_ALLOWED)
        {
            return false;
        }
    }
    range_cat = params.weapon_inst->get_range_category(params.dist_to_target);
    return true;
}

// 0x0067CD00 (:1960)
int fall_back_goal::get_fitness(const npc* np, ai_plan_conditions working_conditions) const
{
    if (np->current_team == HUMAN_TEAM_MARAUDER || human_team_is_non_combatant(np))
    {
        return -1;
    }
    const ai_system* ai = np->get_ai();
    bool sniper = false;
    const weapon* equipped = np->get_equipped_weapon();
    if (equipped == nullptr || equipped->w_info == nullptr || equipped->w_info->weapon_anim_group != ANIMATION_GROUP_SNIPER)
    {
        if (ai->persona_flags.heavy_vehicle_no_fall_back)
        {
            return -1;
        }
    }
    else
    {
        sniper = true;
        if (ai->perception.get_first_belief(AI_BELIEF_CLOSEST_ALLY) == nullptr)
        {
            return -1;
        }
    }

    // Bit 13: in a building.
    const float max_dist = has_condition(working_conditions, 13) ? 10.0f : 25.0f;
    ai_combat_action_parameters params;
    params.init_npc(const_cast<npc*>(np));
    weapon_range_categories range_cat;
    if (maneuver_goal_get_range_common(range_cat, params, working_conditions, true, true)
        && (sniper || ai_perception::human_is_in_heavy_vehicle(params.target)) && static_cast<int>(range_cat) < 3 && params.dist_to_target < max_dist)
    {
        return human_combat_cover_is_using_node(np) ? 3600 : 3800;
    }
    return -1;
}

// 0x006A1060 (:2029)
int advance_goal::get_fitness(const npc* np, ai_plan_conditions working_conditions) const
{
    if (human_team_is_non_combatant(np) || activity_house_arrest_is_human_hostage(np))
    {
        return -1;
    }
    ai_combat_action_parameters params;
    params.init_npc(const_cast<npc*>(np));
    if (Combat_maneuver_do_player_line_of_fire_checks)
    {
        const player* local = player_get_local_player();
        const human* line_target = human_combat_get_target(np);
        if (local != nullptr && line_target != nullptr && human_teams_are_allies(local, np)
            && combat_maneuver_path_crosses_player_line_of_fire(np, line_target->pos, nullptr))
        {
            return -1;
        }
    }
    const ai_system* ai = params.np->get_ai();
    if (ai->planner.get_current_goal() != this && ai->not_using_cover_timer.valid() && ai->not_using_cover_timer.time_since() < 3000)
    {
        return -1;
    }
    weapon_range_categories range_cat;
    if (!maneuver_goal_get_range_common(range_cat, params, working_conditions, true, false))
    {
        return -1;
    }
    const int category = static_cast<int>(range_cat);

    if (params.target->vehicle_handle != 0xFFFFFFFF)
    {
        const object* objp = World.object_from_handle(params.target->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            const vehicle* vp = static_cast<const vehicle*>(objp);
            if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
            {
                if (category < 3)
                {
                    return -1;
                }
                const float max_range = params.weapon_inst->w_info->max_range;
                if (vec_xz_dist_squared(np->pos, vp->pos) <= max_range * max_range)
                {
                    return -1;
                }
            }
            if (ai_perception::vehicle_is_heavy(vp))
            {
                const float safe_dist = ai->perception.is_in_closest_building(np, nullptr, 0.0f, 1.25f) ? 13.0f : 30.0f;
                if (category < 3 || params.dist_to_target < safe_dist)
                {
                    return -1;
                }
            }
        }
    }

    if (ai->perception.enemy_is_being_attacked_from_turret(params.target) && category <= 2)
    {
        return -1;
    }
    const ai_combat_should_charge_results charge = ai_combat_should_charge_target(params);
    if (np->current_team == HUMAN_TEAM_MARAUDER || charge != AI_COMBAT_SHOULD_NOT_CHARGE)
    {
        return (category > 1 || charge == AI_COMBAT_MUST_CHARGE) ? 2600 : -1;
    }
    if (category <= 1)
    {
        return -1;
    }
    const timestamp& maneuver = np->combat.combat_maneuver_timestamp;
    if (category < 3 && maneuver.valid() && !maneuver.elapsed())
    {
        return -1;
    }
    return 2600;
}

// 0x0067CED0 (:2139)
bool advance_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    npc* mutable_np = const_cast<npc*>(np);
    ai_system* ai = mutable_np->get_ai();
    human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed() || alm_grace_is_active(np, target))
    {
        return false;
    }
    goal_setup_data.selected_target = target;
    goal_setup_data.attention_mode = ATTENTION_USE_CURRENT_MODE;

    const weapon* wp = human_weapons_get_weapon(np);
    if (wp == nullptr || !wp->w_info->is_firearm())
    {
        const inventory_item* item = inv_item_get_best_firearm(np);
        wp = item != nullptr ? World.object_from_handle<weapon>(item->weapon_handle) : nullptr;
        if (wp == nullptr || !wp->w_info->is_firearm())
        {
            return false;
        }
    }
    goal_setup_data.flags.force_pursuit = true;
    goal_setup_data.selected_weapon = wp;

    gfm_orders* orders = ai->get_orders();
    if (orders != nullptr)
    {
        const object* defended = orders->get_target_object();
        if (target == defended)
        {
            return true;
        }
        const vector orders_pos = defended != nullptr ? defended->pos : orders->get_orders_pos();
        if (vec_dist_squared(target->pos, orders_pos) < 100.0f
            || (orders->get_orders_type() == GFM_ORDERS_TYPE_DEFEND && !ai->outside_defensive_perimeter(target->pos, true, nullptr)))
        {
            return true;
        }
    }

    if (np->current_team != HUMAN_TEAM_MARAUDER)
    {
        return true;
    }

    // Marauders can drop a pursuit once it has run past its time and the target is beyond their notice distance.
    const unsigned int handle = target->get_handle();
    const ai_belief_specification specifications[4] = {
        ai_belief_specification(AI_BELIEF_DAMAGE_TAKEN, handle, 3000, BELIEF_REQUIREMENTS_NONE),
        ai_belief_specification(AI_BELIEF_BULLET_IMPACT, handle, 3000, BELIEF_REQUIREMENTS_NONE),
        ai_belief_specification(AI_BELIEF_COMBAT_SOUND, handle, 3000, BELIEF_REQUIREMENTS_NONE),
        ai_belief_specification(AI_BELIEF_EXPLOSION, handle, 3000, BELIEF_REQUIREMENTS_NONE),
    };
    const bool recently_attacked = ai->perception.find_matching_belief(specifications, 4, 0xFFFFFFFF) != nullptr;

    const timestamp& last_pursuit = np->combat.last_pursuit_timestamp;
    if (!last_pursuit.valid() || last_pursuit.time_since() >= 30000 || vec_xz_dist_squared(np->pos, target->pos) <= 100.0f)
    {
        return true;
    }
    vector velocity;
    if (ai->perception.target_tracking_get_aggregate_velocity(velocity, target, 1.5f))
    {
        vector direction;
        vec_normalized_dir(direction, np->pos, target->pos);
        const float along = velocity.y * direction.y + direction.x * velocity.x + velocity.z * direction.z;
        if (along > 2.0f || (along > -2.0f && recently_attacked))
        {
            return true;
        }
    }
    goal_setup_data.flags.force_pursuit = false;
    if (np->combat.pursuit_timestamp.time_since() <= ai_get_pursuit_time(np, target))
    {
        return true;
    }
    const float notice_dist = ai->perception.get_max_notice_dist(target);
    if (notice_dist * notice_dist < vec_dist_squared(np->pos, target->pos))
    {
        ai->perception.deprecate_all_enemy_beliefs(handle, nullptr);
        mutable_np->combat.target_handle = 0xFFFFFFFF;
        mutable_np->combat.targets_building_handle = 0xFFFFFFFF;
        ai->perception.enter_idle_attention();
    }
    return false;
}

// 0x006A12C0 (:2285)
void advance_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    const ai_system* ai = np->get_ai();
    goal_setup_data.set_attention(np, nullptr);
    const timestamp& last_pursuit = np->combat.last_pursuit_timestamp;
    if (!last_pursuit.valid() || last_pursuit.time_since() >= 30000 || goal_setup_data.flags.force_pursuit)
    {
        np->combat.pursuit_timestamp.set(0);
    }
    np->combat.last_pursuit_timestamp.set(0);
    const ai_goal* last_goal = ai->planner.get_last_goal();
    if (last_goal != this && last_goal != reload_weapon_goal::get_instance())
    {
        np->combat.flank_delta = fl_rand_range(-Advance_flanking_spread, Advance_flanking_spread);
    }
}

// 0x005EA290 (:2316)
int custom_civ_action_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    return np->npc_flags.amb_watch_player ? 120000 : -1;
}

// 0x0065FB90 (:2331)
int custom_orders_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    gfm_orders* orders = get_ai_squad_orders(np->get_ai());
    return (orders != nullptr && orders->get_orders_type() == GFM_ORDERS_VEHICLE_DROP_OFF) ? 120000 : -1;
}

static bool in_current_ambient_edf_squad(const npc* np)
{
    const unsigned int squad = np->get_ai()->get_squad_handle();
    return squad == Current_ambient_edf_info.ambient_squad_handle || squad == Current_ambient_edf_info.supporting_squad_handle;
}

// 0x005EA2B0 (:2357)
int ambient_edf_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (Current_ambient_edf_info.ambient_squad_handle == 0xFFFFFFFF || !in_current_ambient_edf_squad(np))
    {
        return -1;
    }
    if (Current_ambient_edf_info.supporting_squad_handle != 0xFFFFFFFF && np->get_ai()->get_squad_handle() == Current_ambient_edf_info.supporting_squad_handle)
    {
        return 98000;
    }
    return Current_ambient_edf_info.action_type == AMBIENT_EDF_ACTION_TYPE_FLYER ? 98000 : 60;
}

// 0x00637980 (:2377)
bool ambient_edf_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    gfm_orders* orders = get_ai_squad_orders(np->get_ai());
    if (orders == nullptr)
    {
        return false;
    }
    const bool defending = orders->get_orders_type() == GFM_ORDERS_TYPE_DEFEND || orders->get_orders_type() == GFM_ORDERS_TYPE_ROADBLOCK;
    goal_setup_data.defend_object = defending ? orders->get_target_object() : nullptr;
    goal_setup_data.position = orders->get_location();
    goal_setup_data.min_drive_dist = 20.0f;
    return true;
}

// 0x005EA320 (:2393)
void ambient_edf_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_goal_pos = goal_setup_data.position;
    ai->desired_vehicle_dist_to_goal_pos = 20.0f;
    if (Current_ambient_edf_info.ambient_squad_handle != 0xFFFFFFFF && in_current_ambient_edf_squad(np) && Current_ambient_edf_info.action_type == AMBIENT_EDF_ACTION_TYPE_FLYER)
    {
        ai->desired_vehicle_speed = 0.0f;
        return;
    }
    ai->desired_vehicle_speed = 9.0f; // AMBIENT_VEHICLE_SPEED
}

// 0x00637A60 (:2412): the player if it qualifies, otherwise the first human with flag 0x10.
human* select_confront_target(const npc* np)
{
    const ai_belief* head = np->get_ai()->perception.get_first_belief(AI_BELIEF_UNFRIENDLY);
    human* found = nullptr;
    for (const ai_belief* belief = head; belief != nullptr;)
    {
        if ((belief->flags & 0x10) != 0 && belief->handle != 0xFFFFFFFF)
        {
            object* objp = World.object_from_handle(belief->handle);
            if (objp != nullptr && objp->get_obj_type() == OT_HUMAN)
            {
                if (object_is_player(objp))
                {
                    return static_cast<human*>(objp);
                }
                if (found == nullptr)
                {
                    found = static_cast<human*>(objp);
                }
            }
        }
        belief = belief->next;
        if (belief == head)
        {
            break;
        }
    }
    return found;
}

static bool in_raid(const npc* np)
{
    return np->hflags.activity_raid_required || np->hflags.activity_raid_optional;
}

// 0x00637AE0 (:2439)
int confront_civilian_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (Disable_threaten_behavior || human_teams_get_disposition(np, HUMAN_TEAM_EDF) != TEAM_DISPOSITION_ALLIED || !human_weapons_is_firearm_equipped(np) || in_raid(np))
    {
        return -1;
    }
    return select_confront_target(np) != nullptr ? 920 : -1;
}

// 0x006B7420 (:2461)
bool confront_civilian_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    human* target = select_confront_target(np);
    if (target == nullptr)
    {
        return false;
    }
    const unsigned int assignment = ai_defense_request_assignment(np, target, np->get_ai()->desired_defense_assignment_handle);
    goal_setup_data.defense_assignment_handle = assignment;
    goal_setup_data.defense_assignment = ai_defense_get_assignment(assignment);
    goal_setup_data.selected_target = target;
    goal_setup_data.attention_mode = ATTENTION_USE_CURRENT_MODE;
    return true;
}

// 0x006A1370 (:2484)
void confront_civilian_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    goal_setup_data.set_attention(np, nullptr);
    np->get_ai()->desired_defense_assignment_handle = goal_setup_data.defense_assignment_handle;
}

// 0x0065FBE0 (:2495)
int threaten_watch_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (human_teams_get_disposition(np, HUMAN_TEAM_EDF) != TEAM_DISPOSITION_ALLIED || human_in_vehicle(np) != nullptr || human_combat_get_manned_turret(np) != nullptr || in_raid(np))
    {
        return -1;
    }
    return ai_threaten_pick_watch_target(np, nullptr) != nullptr ? 910 : -1;
}

// 0x0065FC40 (:2516)
bool threaten_watch_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    npc* leader = nullptr;
    human* target = ai_threaten_pick_watch_target(np, &leader);
    if (target == nullptr || leader == nullptr)
    {
        return false;
    }
    goal_setup_data.selected_target = target;
    goal_setup_data.selected_leader = leader;
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_OBJECT;
    return true;
}

// 0x006A13A0 (:2530)
void threaten_watch_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (goal_setup_data.selected_target != nullptr && goal_setup_data.selected_leader != nullptr)
    {
        ai_system* ai = np->get_ai();
        ai->desired_follow_target = goal_setup_data.selected_leader->get_handle();
        ai->desired_target = goal_setup_data.selected_target->get_handle();
        goal_setup_data.set_attention(np, nullptr);
    }
}

// 0x006A13E0 (:2544)
int threaten_civilian_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (Disable_threaten_behavior || human_teams_get_disposition(np, HUMAN_TEAM_EDF) != TEAM_DISPOSITION_ALLIED || human_in_vehicle(np) != nullptr || in_raid(np))
    {
        return -1;
    }
    return ai_threaten_pick_target(np) != nullptr ? 900 : -1;
}

// 0x006A1440 (:2568)
bool threaten_civilian_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    human* target = ai_threaten_pick_target(np);
    if (target == nullptr)
    {
        return false;
    }
    goal_setup_data.selected_target = target;
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_OBJECT;
    return true;
}

// 0x006A1470 (:2588)
void threaten_civilian_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (goal_setup_data.selected_target != nullptr)
    {
        goal_setup_data.set_attention(np, nullptr);
    }
}

static timestamp Taunt_civilian_timestamp; // 0x01642ED4
static timestamp Taunt_player_timestamp; // 0x01642ED8
timestamp Taunt_enemy_timestamps[4]; // 0x01DCA268, by team (battle_cry_action sets it too)

// 0x0065FC90 (:2597)
int taunt_civilian_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if ((Taunt_civilian_timestamp.valid() && !Taunt_civilian_timestamp.elapsed())
        || np->get_ai()->planner.get_current_action() == guard_examine_vehicle_action::get_instance())
    {
        return -1;
    }
    const player* local = player_get_local_player();
    if (local == nullptr || vec_dist_squared(np->pos, local->pos) > 1600.0f || human_teams_get_disposition(np, HUMAN_TEAM_EDF) != TEAM_DISPOSITION_ALLIED || human_in_vehicle(np) != nullptr)
    {
        return -1;
    }
    return ai_taunt_unfriendly_pick_target(np) != nullptr ? 710 : -1;
}

// 0x0065FD40 (:2626)
bool taunt_civilian_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    if ((Taunt_civilian_timestamp.valid() && !Taunt_civilian_timestamp.elapsed())
        || np->get_ai()->planner.get_current_action() == guard_examine_vehicle_action::get_instance()
        || restricted_area_in_any_region(np->pos, true))
    {
        return false;
    }
    human* target = ai_taunt_unfriendly_pick_target(np);
    if (target == nullptr)
    {
        return false;
    }
    goal_setup_data.selected_target = target;
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_OBJECT;
    return true;
}

// 0x006A1490 (:2656)
void taunt_civilian_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (goal_setup_data.selected_target != nullptr)
    {
        goal_setup_data.set_attention(np, nullptr);
        Taunt_civilian_timestamp.set_rand(30000, 60000);
    }
}

// 0x0065FDC0 (:2666)
int taunt_enemy_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (np->current_team != HUMAN_TEAM_MARAUDER)
    {
        return -1;
    }
    const timestamp& next = Taunt_enemy_timestamps[HUMAN_TEAM_MARAUDER];
    if (next.valid() && !next.elapsed())
    {
        return -1;
    }
    return ai_taunt_enemy_pick_target(np) != nullptr ? 3300 : -1;
}

// 0x0065FE10 (:2685)
bool taunt_enemy_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    if (np == nullptr || np->current_team == HUMAN_TEAM_NONE)
    {
        return false;
    }
    const timestamp& next = Taunt_enemy_timestamps[np->current_team];
    if (next.valid() && !next.elapsed())
    {
        return false;
    }
    human* target = ai_taunt_enemy_pick_target(np);
    if (target == nullptr)
    {
        return false;
    }
    goal_setup_data.selected_target = target;
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_OBJECT;
    return true;
}

// 0x006A14D0 (:2702)
void taunt_enemy_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (goal_setup_data.selected_target != nullptr)
    {
        goal_setup_data.set_attention(np, nullptr);
        if (np != nullptr && np->current_team != HUMAN_TEAM_NONE)
        {
            Taunt_enemy_timestamps[np->current_team].set_rand(7000, 12000);
        }
    }
}

// 0x005EA3A0 (:2712)
int taunt_player_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (np->current_team != HUMAN_TEAM_MARAUDER || human_in_vehicle(np) != nullptr || !np->get_ai()->persona_flags.follow_player)
    {
        return -1;
    }
    const player* local = player_get_local_player();
    if (local == nullptr || local->is_killed() || vec_dist_squared(np->pos, local->pos) > 400.0f)
    {
        return -1;
    }
    return (Taunt_player_timestamp.valid() && !Taunt_player_timestamp.elapsed()) ? -1 : 720;
}

// 0x006A1510 (:2744)
bool taunt_player_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    player* local = player_get_local_player();
    if (local == nullptr || local->is_killed() || vec_dist_squared(np->pos, local->pos) > 400.0f)
    {
        return false;
    }
    if (Taunt_player_timestamp.valid() && !Taunt_player_timestamp.elapsed())
    {
        return false;
    }
    const vector above(UP_VECTOR.x + np->pos.x, np->pos.y + UP_VECTOR.y, np->pos.z + UP_VECTOR.z);
    if (game_render_cull_sphere(above, 1.0f))
    {
        return false;
    }
    vector head;
    human_get_head_pos(local, head);
    if (!np->get_ai()->perception.has_los_to_position(head, false))
    {
        return false;
    }
    goal_setup_data.selected_target = local;
    goal_setup_data.attention_mode = ATTENTION_AIM_AT_OBJECT;
    return true;
}

// 0x006A1640 (:2779)
void taunt_player_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (goal_setup_data.selected_target != nullptr)
    {
        Taunt_player_timestamp.set_rand(10000, 15000);
        goal_setup_data.set_attention(np, nullptr);
    }
}

// 0x00637B40 (:2789)
int oppress_civilian_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const ai_system* ai = np->get_ai();
    if (Disable_threaten_behavior || np->current_team != HUMAN_TEAM_EDF)
    {
        return -1;
    }
    const ai_belief* beatdown = ai->perception.get_first_belief(AI_BELIEF_BEATDOWN_TARGET);
    if (beatdown != nullptr)
    {
        return human_is_dead(beatdown->handle) ? -1 : 930;
    }
    if (ai->planner.get_last_goal() == this && ai->oppression_action != AI_OPPRESSION_NONE)
    {
        // get_attention_target<human> (0x005F8BE0), inlined.
        const object* attended = World.object_from_handle(ai->get_target_handle());
        const human* target = (attended != nullptr && attended->get_obj_type() == OT_HUMAN) ? static_cast<const human*>(attended) : nullptr;
        if (target != nullptr && !human_is_dead(target))
        {
            return 930;
        }
    }
    return -1;
}

// 0x0067D2A0 (:2824)
bool oppress_civilian_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    ai_system* ai = const_cast<npc*>(np)->get_ai();
    ai_belief* belief = ai->perception.get_first_belief(AI_BELIEF_BEATDOWN_TARGET);
    human* target = nullptr;
    if (belief != nullptr && belief->handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(belief->handle);
        if (objp != nullptr && objp->get_obj_type() == OT_HUMAN)
        {
            target = static_cast<human*>(objp);
        }
    }

    const bool continuing = ai->planner.get_last_goal() == this && ai->oppression_action != AI_OPPRESSION_BEATDOWN;
    if (target == nullptr)
    {
        if (!continuing || ai->desired_target == 0xFFFFFFFF)
        {
            return false;
        }
        object* objp = World.object_from_handle(ai->desired_target);
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC)
        {
            return false;
        }
        target = static_cast<human*>(objp);
    }
    if (!continuing && !ai_threaten_target_is_valid(np, target))
    {
        ai->perception.deprecate_belief(belief);
        return false;
    }

    goal_setup_data.selected_target = target;
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_OBJECT;
    if (ai->planner.get_last_goal() == this)
    {
        goal_setup_data.oppression_action = ai->oppression_action;
        return true;
    }

    goal_setup_data.oppression_action = AI_OPPRESSION_BEATDOWN;
    if (target->get_obj_type() != OT_HUMAN || target->get_sub_type() != OT_SUB_HUMAN_NPC || (np->hflags.riot_shield && np->shield_handle != 0xFFFFFFFF))
    {
        return true;
    }
    const weapon* equipped = np->get_equipped_weapon();
    if (equipped != nullptr)
    {
        const animation_group group = equipped->w_info->weapon_anim_group;
        if (group == ANIMATION_GROUP_REVOLVER)
        {
            goal_setup_data.oppression_action = static_cast<ai_oppression_actions>(int_rand_range(1, 2));
        }
        else if (group == ANIMATION_GROUP_AK || group == ANIMATION_GROUP_ENFORCER)
        {
            goal_setup_data.oppression_action = static_cast<ai_oppression_actions>(int_rand_range(1, 3));
        }
    }
    if (goal_setup_data.oppression_action != AI_OPPRESSION_BEATDOWN)
    {
        ai->perception.cancel_beatdown(target);
    }
    return true;
}

// 0x006A1680 (:2902)
void oppress_civilian_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (goal_setup_data.selected_target == nullptr)
    {
        return;
    }
    ai_system* ai = np->get_ai();
    goal_setup_data.set_attention(np, nullptr);
    ai->oppression_action = goal_setup_data.oppression_action;
    ai->desired_target = goal_setup_data.selected_target->get_handle();
    const human* target = goal_setup_data.selected_target->get_obj_type() == OT_HUMAN ? static_cast<const human*>(goal_setup_data.selected_target) : nullptr;
    if (!human_weapons_is_obvious_weapon_equipped(target))
    {
        ai->flags.less_aggressive_melee = true;
    }
}

static ai_belief* get_heavy_vehicle_threat_belief(const npc* np)
{
    const ai_belief_specification specification(AI_BELIEF_VEHICLE, 0xFFFFFFFF, -1, BELIEF_REQUIREMENTS_HEAVY_VEHICLE_THREAT);
    ai_belief* belief = nullptr;
    return np->get_ai()->perception.collect_all_matching_beliefs(&belief, 1, &specification, 1, 0xFFFFFFFF) < 1 ? nullptr : belief;
}

// 0x0065FE70 (:2918)
int flee_heavy_vehicle_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (!np->npc_flags.ambient_spawn)
    {
        return -1;
    }
    return get_heavy_vehicle_threat_belief(np) != nullptr ? 5920 : -1;
}

// 0x0065FF00 (:2945)
bool flee_heavy_vehicle_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_belief* belief = get_heavy_vehicle_threat_belief(np);
    if (belief == nullptr)
    {
        return false;
    }
    goal_setup_data.attention_belief_type = AI_BELIEF_VEHICLE;
    goal_setup_data.selected_target = World.object_from_handle(belief->handle);
    return true;
}

// 0x006A16E0 (:2964)
void flee_heavy_vehicle_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    const ai_belief* belief = goal_setup_data.get_attention_belief(np);
    if (belief != nullptr)
    {
        np->get_ai()->perception.pay_attention(belief->handle, belief->get_type(), ATTENTION_LOOK_AT_BELIEF, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    }
}

static const float FLEE_UNFRIENDLIES_EXPANDED_DIST_SQ = 625.0f; // 0x01642EC8

// 0x00637BC0 (:2982)
int flee_restricted_area_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const ai_system* ai = np->get_ai();
    if (!human_team_is_non_combatant(np) || human_in_vehicle(np) != nullptr || np->npc_flags.civilian_has_been_oppressed)
    {
        return -1;
    }
    if (ai->oppression_action >= AI_OPPRESSION_CIVILIAN_ACTIONS)
    {
        return 5800;
    }
    const bool acknowledged_elapsed = np->acknowledged_time.elapsed();
    if (!acknowledged_elapsed && restricted_area_in_any_region(np->pos, true))
    {
        return 5800;
    }
    if (!acknowledged_elapsed || (ai->persona_flags.always_flee_edf && !np->npc_flags.ambient_spawn))
    {
        const ai_belief* unfriendly = ai->perception.get_first_belief(AI_BELIEF_UNFRIENDLY);
        if (unfriendly != nullptr)
        {
            const human* hp = World.object_from_handle<human>(unfriendly->handle);
            float max_dist_squared = ai->planner.get_current_goal() == this ? FLEE_UNFRIENDLIES_EXPANDED_DIST_SQ : 400.0f;
            if (gsm_get_alert_level() > 0)
            {
                max_dist_squared *= 4.0f;
            }
            if (hp != nullptr && vec_dist_squared(hp->pos, np->pos) <= max_dist_squared)
            {
                return 5800;
            }
        }
    }
    return -1;
}

// 0x00637D10 (:3038). LTCG: node_list ESI.
int find_nearest_action_node(const npc* np, farray<object_action_node const*, 608>& node_list)
{
    if (node_list.size() <= 1)
    {
        return 0;
    }
    int nearest = 0;
    float nearest_dist = vec_dist_squared(np->pos, node_list[0]->pos);
    for (int i = 1; i < node_list.size(); i++)
    {
        const float dist = vec_dist_squared(np->pos, node_list[i]->pos);
        if (dist < nearest_dist)
        {
            nearest_dist = dist;
            nearest = i;
        }
    }
    return nearest;
}

// 0x00637DC0 (:3071)
int return_to_orders_pos_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const ai_system* ai = np->get_ai();
    if (ai->get_squad() == nullptr || np->should_despawn())
    {
        return -1;
    }
    gfm_orders* orders = get_ai_squad_orders(ai);
    if (orders == nullptr)
    {
        return -1;
    }
    return vec_dist_squared(np->pos, orders->get_orders_pos()) >= 100.0f ? 40 : -1;
}

// 0x0067D420 (:3110)
int use_action_node_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (np->npc_flags.ambient_spawn && np->npc_flags.used_action_node)
    {
        return -1;
    }
    if (human_team_is_non_combatant(np) && gsm_get_alert_level() > 1)
    {
        return -1;
    }
    if (np->should_despawn() || np->char_inst->vp->base_anim_set_variant != ANIM_SET_VARIANT_NONE || !np->get_ai()->allowed_to_use_action_node())
    {
        return -1;
    }
    if (np->scripted_action_node_handle != 0xFFFFFFFF)
    {
        return 120000;
    }
    return np->hflags.only_use_action_nodes ? 55000 : 100;
}

// 0x0067D4B0 (:3156)
bool use_action_node_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    farray<object_action_node const*, 608> all_nodes;
    farray<object_action_node const*, 608> normal_nodes;
    farray<object_action_node const*, 608> priority_nodes;

    if (np->scripted_action_node_handle != 0xFFFFFFFF)
    {
        object_action_node* scripted = static_cast<object_action_node*>(World.object_from_handle(np->scripted_action_node_handle));
        if (scripted == nullptr || scripted->get_obj_type() != OT_ACTION_NODE)
        {
            return false;
        }
        scripted->cooldown_period.set(0);
        farray<unsigned int, 4> node_set;
        const_cast<npc*>(np)->get_ai()->planner.request_replan();
        if (action_node_build_set(np, scripted, node_set))
        {
            action_node_reserve_node(const_cast<npc*>(np), node_set);
        }
    }

    farray<unsigned int, 4>& action_node_set = goal_setup_data.selected_action_node_set;
    np->get_reserved_nodes(action_node_set);
    if (action_node_set.size() >= 1)
    {
        const object_action_node* desired = np->get_desired_action_node();
        if (desired != nullptr && desired->disabled_by == 0)
        {
            return true;
        }
        const_cast<npc*>(np)->release_all_action_nodes();
        return false;
    }

    if (!action_node_collect_nodes(np, 30.0f, all_nodes, np->npc_flags.ambient_spawn != 0, np->get_ai()->get_patrol_object() != nullptr))
    {
        return false;
    }
    action_node_set.remove_all();
    if (all_nodes.size() <= 0)
    {
        return false;
    }
    for (int i = 0; i < all_nodes.size(); i++)
    {
        const object_action_node* node = all_nodes[i];
        (node->node_flags.high_priority ? priority_nodes : normal_nodes).add(node);
    }

    while (priority_nodes.size() > 0)
    {
        const int nearest = find_nearest_action_node(np, priority_nodes);
        const object_action_node* node = priority_nodes[nearest];
        priority_nodes.remove_unsorted(nearest);
        if (action_node_build_set(np, node, action_node_set))
        {
            return true;
        }
    }
    if (normal_nodes.size() < 1)
    {
        return false;
    }
    // A fresh npc (0x1388 ms or younger) takes the nearest, an older one any of them.
    if (np->get_lifetime() > 5000)
    {
        return action_node_build_set(np, normal_nodes[int_rand_range(0, normal_nodes.size() - 1)], action_node_set);
    }
    while (normal_nodes.size() > 0)
    {
        const int nearest = find_nearest_action_node(np, normal_nodes);
        const object_action_node* node = normal_nodes[nearest];
        normal_nodes.remove_unsorted(nearest);
        if (action_node_build_set(np, node, action_node_set))
        {
            return true;
        }
    }
    return false;
}

// 0x005EA440 (:3274)
void use_action_node_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    action_node_reserve_node(np->get_handle(), goal_setup_data.selected_action_node_set);
}

// 0x00637E80 (:3286)
int roadblock_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const ai_system* ai = np->get_ai();
    gfm_orders* orders = get_ai_squad_orders(ai);
    if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_ROADBLOCK)
    {
        return -1;
    }
    // The whole squad has to be up and alive.
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr || squad->get_squad_def() == nullptr || squad->get_squad_def()->total_npc_count != squad->member_array.size())
    {
        return -1;
    }
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        const unsigned int handle = squad->member_array[i];
        const object* member = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
        if (member == nullptr || member->get_obj_type() != OT_HUMAN || member->get_sub_type() != OT_SUB_HUMAN_NPC || member->is_killed())
        {
            return -1;
        }
    }
    if (orders->get_target_object() == nullptr)
    {
        return -1;
    }
    if (np->vehicle_handle != 0xFFFFFFFF && vehicle_passenger_in_seat(np->vehicle_handle, VSI_DRIVER) == np)
    {
        return 96000;
    }
    return 350;
}

// 0x00637FA0 (:3343)
bool roadblock_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    gfm_orders* orders = get_ai_squad_orders(np->get_ai());
    if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_ROADBLOCK)
    {
        return false;
    }
    const object* defended = orders->get_target_object();
    if (defended == nullptr)
    {
        return false;
    }
    goal_setup_data.defend_object = defended;
    return true;
}

// 0x005EA460 (:3368)
void roadblock_goal::commit_setup_data(npc*, const ai_goal_setup_data&) const
{
}

static const object* get_defend_or_roadblock_target(const npc* np)
{
    gfm_orders* orders = get_ai_squad_orders(np->get_ai());
    if (orders == nullptr || (orders->get_orders_type() != GFM_ORDERS_TYPE_DEFEND && orders->get_orders_type() != GFM_ORDERS_TYPE_ROADBLOCK))
    {
        return nullptr;
    }
    return orders->get_target_object();
}

// 0x00638000 (:3379)
int guard_object_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const object* defended = get_defend_or_roadblock_target(np);
    return (defended != nullptr && defended->get_obj_type() != OT_HUMAN) ? 300 : -1;
}

// 0x00638070 (:3392)
bool guard_object_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const object* defended = get_defend_or_roadblock_target(np);
    if (defended == nullptr || defended->get_obj_type() == OT_HUMAN)
    {
        return false;
    }
    goal_setup_data.defend_object = defended;
    return true;
}

// 0x005EA470 (:3406)
void guard_object_goal::commit_setup_data(npc*, const ai_goal_setup_data&) const
{
}

static const float MAX_DEFEND_HUMAN_DIST_SQ = 49.0f; // 0x016421C0
// Dynamic initializers in rfg.exe: 49 * 1.2f^2 (double 0x012CF678) and 49 * 25 (0x012C95E8).
static const float MIN_MOVE_WITH_HUMAN_TETHER_DIST_SQ = static_cast<float>(MAX_DEFEND_HUMAN_DIST_SQ * 1.440000057220459); // 0x01DA3EE4
static const float MAX_WAIT_FOR_BODYGUARDS_DIST_SQ = MAX_DEFEND_HUMAN_DIST_SQ * 25.0f; // 0x01DCA568

// 0x005EA480 (:3416). LTCG: np EAX.
float get_tether_distance_sqrd(const npc* np)
{
    // MAX_PED_FOLLOW_PLAYER_DIST_SQ for marauders.
    return np->current_team == HUMAN_TEAM_MARAUDER ? 1600.0f : MIN_MOVE_WITH_HUMAN_TETHER_DIST_SQ;
}

// 0x006A1720 (:3429)
int move_with_human_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const object* defended = get_defend_or_roadblock_target(np);
    if (defended == nullptr || defended->get_obj_type() != OT_HUMAN)
    {
        return -1;
    }
    const human* hp = static_cast<const human*>(defended);
    if (object_is_player(hp))
    {
        if (human_in_vehicle(hp) != nullptr || human_in_vehicle_transition(hp, nullptr, VI_INTERACTION_ENTER))
        {
            return 4400;
        }
        return vec_dist_squared(hp->pos, np->pos) > get_tether_distance_sqrd(np) ? 4400 : 600;
    }

    const float dist_squared = vec_dist_squared(hp->pos, np->pos);
    if (dist_squared <= MAX_WAIT_FOR_BODYGUARDS_DIST_SQ)
    {
        if (human_in_vehicle(hp) != nullptr || human_in_vehicle_transition(hp, nullptr, VI_INTERACTION_ENTER))
        {
            if (hp->vehicle_handle != 0xFFFFFFFF)
            {
                const object* objp = World.object_from_handle(hp->vehicle_handle);
                if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && ai_vehicle_is_usable_and_available(static_cast<const vehicle*>(objp), np, AI_VEHICLE_SEAT_TYPE_GUNNER))
                {
                    return 4400;
                }
            }
        }
        else if (dist_squared > MIN_MOVE_WITH_HUMAN_TETHER_DIST_SQ)
        {
            return 4400;
        }
    }
    return 600;
}

// 0x006380E0 (:3485)
bool move_with_human_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const object* defended = get_defend_or_roadblock_target(np);
    if (defended == nullptr || defended->get_obj_type() != OT_HUMAN)
    {
        return false;
    }
    goal_setup_data.defend_object = defended;
    goal_setup_data.min_drive_dist = 20.0f;
    return true;
}

// 0x005EA4A0 (:3503)
void move_with_human_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    np->get_ai()->desired_follow_target = goal_setup_data.defend_object->get_handle();
}

// 0x0067D7C0 (:3512)
int patrol_path_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const ai_system* ai = np->get_ai();
    const object_patrol* patrol = ai->get_patrol_object();
    if (patrol == nullptr)
    {
        return -1;
    }
    const object_squad* squad = ai->get_squad();
    if (patrol->patrol_flags.override_patrol && (squad == nullptr || squad->get_leader() == np))
    {
        return 90210;
    }
    return activity_cc_is_courier(np) ? 70000 : 250;
}

// 0x0067D850 (:3540)
bool patrol_path_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    goal_setup_data.selected_patrol = np->get_ai()->get_patrol_object();
    return goal_setup_data.selected_patrol != nullptr;
}

// 0x0067D870 (:3556)
void patrol_path_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_destination_object = goal_setup_data.selected_patrol->get_handle();
    gfm_orders* orders = get_ai_squad_orders(ai);
    if (orders != nullptr && orders->get_orders_type() == GFM_ORDERS_TYPE_COURIER)
    {
        object_squad* squad = ai->get_squad();
        activity_courier_chase_data* courier = squad != nullptr ? squad->get_courier_data() : nullptr;
        if (courier != nullptr)
        {
            ai->desired_vehicle_speed = courier->get_move_speed();
        }
    }
}

// 0x00638160 (:3575)
int wait_for_squad_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const object_squad* squad = np->get_ai()->get_squad();
    if (squad == nullptr || squad->get_leader() != np || human_in_vehicle(np) != nullptr)
    {
        return -1;
    }
    return squad->followers_near_leader() ? -1 : 500;
}

// 0x0067D900 (:3608)
int follow_leader_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const ai_system* ai = np->get_ai();
    const object* defended = get_defend_or_roadblock_target(np);
    if (defended != nullptr && defended->get_obj_type() == OT_HUMAN)
    {
        return -1;
    }
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr)
    {
        return -1;
    }
    const gfm_orders_types orders_type = squad->get_squad_orders_type();
    if (orders_type == GFM_ORDERS_TYPE_AMBIENT_PED || orders_type == GFM_ORDERS_TYPE_AMBIENT_ACTION || orders_type == GFM_ORDERS_TYPE_ATTACK)
    {
        return -1;
    }
    if (orders_type == GFM_ORDERS_TYPE_DEFEND && ai->is_in_player_squad(true) != nullptr)
    {
        return -1;
    }
    const npc* leader = squad->get_leader();
    if (np == leader)
    {
        return -1;
    }

    if (orders_type == GFM_ORDERS_TYPE_CONVOY)
    {
        const object_convoy* convoy = ai->get_convoy_object();
        if (convoy != nullptr)
        {
            if (!convoy->flags.attacked)
            {
                return 4500;
            }
            const convoy_squad_roles role = convoy->get_squad_role(squad->get_handle());
            if (role != CONVOY_SQUAD_ROLE_ON_FOOT_ESCORT)
            {
                return role != CONVOY_SQUAD_ROLE_BREAK_AND_ATTACK ? 4500 : 1650;
            }
        }
    }

    bool patrolling = false;
    if (orders_type == GFM_ORDERS_TYPE_PATROL)
    {
        patrolling = true;
        const object_patrol* patrol = ai->get_patrol_object();
        if (patrol != nullptr && patrol->patrol_flags.override_patrol)
        {
            return 90210;
        }
    }

    const human* target = human_combat_get_target(np);
    if (target != nullptr && ai->perception.can_engage_target_reason(target) == AI_ENGAGE_ALLOWED)
    {
        return -1;
    }
    if (patrolling || amb_edf_action_participant(np))
    {
        return 610;
    }
    if (leader == nullptr || leader->get_ai() == nullptr || leader->get_ai()->near_orders_location(0.0f))
    {
        return -1;
    }
    return ai->near_orders_location(0.0f) ? -1 : 610;
}

// 0x0067DAF0 (:3715)
int reinforcement_goal::get_fitness(const npc* np, ai_plan_conditions working_conditions) const
{
    const ai_system* ai = np->get_ai();
    const vehicle* vp;
    if (activity_raid_go_to_raid(np) && activity_raid_has_raid_orders(np))
    {
        return human_in_vehicle(np) != nullptr ? 8000 : 4200;
    }
    gfm_orders* orders = get_ai_squad_orders(ai);
    if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_ATTACK || ai->is_done_reinforcing())
    {
        return -1;
    }
    vector dest = orders->get_location();
    const vector orders_pos = orders->get_location();
    find_reinforcement_destination(dest, np, ai, orders_pos);
    const float dist_squared = vec_dist_squared(dest, np->pos);

    vp = human_in_vehicle(np);
    bool far_away = 3600.0f <= dist_squared;
    if (vp != nullptr && vehicle_is_tank(vp) && has_condition(working_conditions, COND_TARGET_IN_LOS))
    {
        if (14400.0f <= dist_squared)
        {
            return 8000;
        }
        far_away = false;
    }
    if (far_away)
    {
        return vp != nullptr ? 8000 : 4200;
    }

    // Recent contact or combat sound, within 0x4E20 ms.
    const ai_belief* contact = ai->perception.get_first_belief(AI_BELIEF_LAST_CONTACT);
    if (contact != nullptr && contact->update_time.time_since() <= 20000)
    {
        return 1450;
    }
    const ai_belief* sound = ai->perception.get_first_belief(AI_BELIEF_COMBAT_SOUND);
    if (sound != nullptr && sound->update_time.time_since() <= 20000)
    {
        return 1450;
    }
    return 1800;
}

// 0x006381D0 (:3780)
bool reinforcement_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    gfm_orders* orders = get_ai_squad_orders(np->get_ai());
    if (orders == nullptr)
    {
        return false;
    }
    goal_setup_data.defend_object = get_defend_or_roadblock_target(np);
    if (goal_setup_data.defend_object == nullptr)
    {
        goal_setup_data.position = orders->get_location();
    }
    goal_setup_data.min_drive_dist = 20.0f;
    return true;
}

// 0x0067DD60 (:3802)
void reinforcement_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_goal_pos = goal_setup_data.position;
    ai->desired_vehicle_dist_to_goal_pos = 10.0f;
    gfm_orders* orders = get_ai_squad_orders(ai);
    if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_ATTACK)
    {
        return;
    }
    ai->desired_vehicle_speed = 20.0f;
    const human* target = human_combat_get_target(np);
    if (target != nullptr && target->vehicle_handle != 0xFFFFFFFF)
    {
        const object* objp = World.object_from_handle(target->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            float speed;
            vehicle_get_speed(static_cast<const vehicle*>(objp), speed);
            ai->desired_vehicle_speed = speed + 20.0f;
        }
    }
}

static const float CHASE_TARGET_FAR_DIST_SQRD = 3600.0f; // 0x01642ECC
static const float CHASE_TARGET_NEAR_DIST_SQRD = 1600.0f; // 0x01642ED0

// 0x006A18A0 (:3827)
int chase_target_goal::get_fitness(const npc* np, ai_plan_conditions working_conditions) const
{
    const ai_system* ai = np->get_ai();
    if (np->npc_flags.ambient_edf_flyer || human_team_is_non_combatant(np) || activity_house_arrest_is_human_hostage(np))
    {
        return -1;
    }
    const vehicle* own_vehicle = human_in_vehicle(np);
    if (human_teams_get_disposition(np, HUMAN_TEAM_GUERILLA) == TEAM_DISPOSITION_HOSTILE && activity_pursuit_npc(np)
        && own_vehicle != nullptr && !vehicle_is_flipped(own_vehicle))
    {
        if (is_player_my_target_or_is_my_target_with_player_in_stuck_vehicle(np))
        {
            return -1;
        }
        if (activity_is_active(ACTIVITY_MODE_CONTINUOUS_ATTACK))
        {
            if (World.object_from_handle(activity_cont_attack_get_target()) == nullptr)
            {
                return -1;
            }
            if (!human_is_driving(np) && human_combat_is_manning_turret(np))
            {
                return 2000;
            }
        }
        return 90100;
    }

    if (!has_condition(working_conditions, COND_TARGET_IN_LOS))
    {
        return -1;
    }
    if (ai->has_orders_of_type(GFM_ORDERS_TYPE_DEFEND))
    {
        if (own_vehicle == nullptr)
        {
            return -1;
        }
        if (vehicle_is_tank(own_vehicle))
        {
            const vector to_destination(ai->desired_destination.x - np->pos.x, ai->desired_destination.y - np->pos.y, ai->desired_destination.z - np->pos.z);
            if (to_destination.mag_squared() > 2500.0f)
            {
                return -1;
            }
        }
        else if (own_vehicle->get_sub_type() != OT_SUB_VEHICLE_FLYER)
        {
            return -1;
        }
    }

    const human* target = human_combat_get_target(np);
    if (target == nullptr || alm_grace_is_active(np, target))
    {
        return -1;
    }
    const vehicle* target_vehicle = World.object_from_handle<vehicle>(target->vehicle_handle);
    const vector target_pos = target_vehicle != nullptr ? target_vehicle->pos : target->pos;
    const float dist_squared = vec_dist_squared(target_pos, np->pos);

    const vehicle* vp = own_vehicle;
    if (vp == nullptr)
    {
        vp = ai_vehicle_locate_tank(np);
    }
    else if (human_is_driving(np))
    {
        if (is_player_my_target_or_is_my_target_with_player_in_stuck_vehicle(np) && !vehicle_is_tank(vp))
        {
            return -1;
        }
        if (target_vehicle != nullptr && target_vehicle->get_sub_type() != OT_SUB_VEHICLE_WALKER)
        {
            float speed;
            vehicle_get_speed(target_vehicle, speed);
            if (!fl_is_equal_eps(speed, 0.0f, 0.01f))
            {
                return 3900;
            }
        }
        const float near_dist = (target_vehicle != nullptr && target_vehicle->get_sub_type() == OT_SUB_VEHICLE_WALKER) ? 400.0f : CHASE_TARGET_NEAR_DIST_SQRD;
        if (dist_squared < near_dist)
        {
            const turret* driver_turret = vehicle_get_seat_turret(vp, VSI_DRIVER);
            if (ai_vehicle_has_manned_turret(vp))
            {
                return 3900;
            }
            return (driver_turret == nullptr || !driver_turret->turret_flags.locked_turret) ? 2000 : 3900;
        }
        return 3900;
    }

    const float start_dist = ai->planner.get_current_goal() == this ? CHASE_TARGET_NEAR_DIST_SQRD : CHASE_TARGET_FAR_DIST_SQRD;
    if (dist_squared < start_dist)
    {
        return (vp != nullptr && vehicle_is_tank(vp)) ? 3900 : 2000;
    }
    return 3900;
}

// 0x0067DE40 (:3966)
bool chase_target_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_system* ai = np->get_ai();
    if (human_teams_get_disposition(np, HUMAN_TEAM_GUERILLA) == TEAM_DISPOSITION_HOSTILE && activity_pursuit_npc(np) && np->vehicle_handle != 0xFFFFFFFF)
    {
        const object* objp = World.object_from_handle(np->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && !vehicle_is_flipped(static_cast<const vehicle*>(objp)))
        {
            goal_setup_data.selected_target = activity_is_active(ACTIVITY_MODE_CONTINUOUS_ATTACK)
                ? World.object_from_handle(activity_cont_attack_get_target())
                : static_cast<object*>(player_get_local_player());
            return true;
        }
    }

    human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return false;
    }
    gfm_orders* orders = get_ai_squad_orders(ai);
    if (orders != nullptr && orders->get_orders_type() == GFM_ORDERS_TYPE_DEFEND)
    {
        const vehicle* own_vehicle = human_in_vehicle(np);
        if (own_vehicle == nullptr)
        {
            return false;
        }
        if (vehicle_is_tank(own_vehicle))
        {
            const float dx = ai->desired_destination.x - np->pos.x;
            const float dy = ai->desired_destination.y - np->pos.y;
            const float dz = ai->desired_destination.z - np->pos.z;
            if (dx * dx + dy * dy + dz * dz > 2500.0f)
            {
                return false;
            }
        }
        else if (own_vehicle->get_sub_type() != OT_SUB_VEHICLE_FLYER)
        {
            return false;
        }
    }

    vector target_pos;
    vector target_velocity;
    const object* target_vehicle = target->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(target->vehicle_handle) : nullptr;
    if (target_vehicle != nullptr && target_vehicle->get_obj_type() == OT_VEHICLE)
    {
        target_pos = target_vehicle->pos;
        vehicle_get_velocity(static_cast<const vehicle*>(target_vehicle), target_velocity);
    }
    else
    {
        target_pos = target->pos;
        target_velocity = target->actual_velocity;
    }

    vector direction(target_pos.x - np->pos.x, target_pos.y - np->pos.y, target_pos.z - np->pos.z);
    const float distance = direction.mag_and_normalize_safe();
    if (human_in_vehicle(np) == nullptr)
    {
        // On foot it does not chase a nearby target whose velocity points back at it.
        const float max_dist = ai->planner.get_current_goal() == this ? 40.0f : 60.0f;
        if (distance < max_dist && target_velocity.y * direction.y + target_velocity.x * direction.x + target_velocity.z * direction.z < -1.0f)
        {
            return false;
        }
    }
    goal_setup_data.selected_target = target;
    return true;
}

// 0x006A1CE0 (:4038)
void chase_target_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (goal_setup_data.selected_target != nullptr)
    {
        ai_system* ai = np->get_ai();
        ai->desired_follow_target = goal_setup_data.selected_target->get_handle();
        ai->perception.pay_attention(goal_setup_data.selected_target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        ai_combat_post_attack_events(np, goal_setup_data.selected_target);
    }
}

// 0x0065FFA0 (:4056)
int accompany_chase_target_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (human_team_is_non_combatant(np) || human_in_vehicle(np) == nullptr)
    {
        return -1;
    }
    const human* driver = vehicle_passenger_in_seat(np->vehicle_handle, VSI_DRIVER);
    if (driver == nullptr || driver->get_obj_type() != OT_HUMAN || driver->get_sub_type() != OT_SUB_HUMAN_NPC || driver == np)
    {
        return -1;
    }
    static const ai_plan_conditions PURSUIT_GOAL_CONDITIONS = ai_plan_conditions(51) | ai_plan_conditions(COND_TARGET_IN_LOS); // 0x01DDBBD0
    const ai_goal* goal = static_cast<const npc*>(driver)->get_ai()->planner.get_current_goal();
    return (goal != nullptr && goal->pursues_condition(PURSUIT_GOAL_CONDITIONS)) ? 2900 : -1;
}

// 0x005EA4C0 (:4085)
int drive_on_rail_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    return (vrail_any_ready() && vrail_is_driver(np)) ? 90200 : -1;
}

// 0x006A1D30 (:4102)
bool drive_on_rail_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_system* ai = np->get_ai();
    const object_patrol* patrol = ai->get_patrol_object();
    if (patrol == nullptr)
    {
        return false;
    }
    goal_setup_data.selected_patrol = patrol;
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        const object_squad* squad = ai->get_squad();
        vp = squad != nullptr ? squad->get_vehicle() : nullptr;
        if (vp == nullptr)
        {
            return false;
        }
    }
    goal_setup_data.use_target = vp;
    if (!ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER) || !ai_vehicle_is_seat_available(vp, np, VSI_DRIVER))
    {
        return false;
    }
    switch (vrail_get_patrol_state())
    {
    case VRAIL_PATROL_STATE_WAIT:
        goal_setup_data.position = patrol->pos;
        return true;
    case VRAIL_PATROL_STATE_PATROL:
        goal_setup_data.position = np->pos;
        return true;
    default:
        return vrail_drop_point(goal_setup_data.position);
    }
}

// 0x006382B0 (:4149)
void drive_on_rail_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_destination_object = goal_setup_data.selected_patrol->get_handle();
    ai->desired_vehicle = goal_setup_data.use_target->get_handle();
    ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_DRIVER;
    object_squad* squad = ai->get_squad();
    if (squad != nullptr && !squad->current_node_handle.is_valid())
    {
        squad->current_node_handle = goal_setup_data.selected_patrol->root_node_handle;
    }
    ai->desired_vehicle_speed = 0.0f;
    ai->desired_goal_pos = goal_setup_data.position;
    ai->desired_vehicle_dist_to_goal_pos = 10.0f;
}

// 0x00638370 (:4180)
int smash_building_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    gfm_orders* orders = get_ai_squad_orders(np->get_ai());
    if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_SMASH)
    {
        return -1;
    }
    return orders->any_valid_building() ? 20000 : -1;
}

// 0x006383D0 (:4196)
bool smash_building_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    gfm_orders* orders = get_ai_squad_orders(np->get_ai());
    if (orders == nullptr)
    {
        return false;
    }
    rfg_mover* building = orders->get_best_building(np);
    if (building == nullptr || building->get_obj_type() != OT_MOVER || building->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return false;
    }
    goal_setup_data.selected_building = building;
    return true;
}

// 0x005EA4F0 (:4223)
void smash_building_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    ai_system* ai = np->get_ai();
    if (goal_setup_data.selected_building->get_handle() != ai->desired_destination_object)
    {
        np->npc_flags.turning_around = 0;
    }
    ai->desired_destination_object = goal_setup_data.selected_building->get_handle();
}

// 0x006A1E20 (:4237)
int man_turret_in_vehicle_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (human_team_is_non_combatant(np) || (np->hflags.riot_shield && np->shield_handle != 0xFFFFFFFF))
    {
        return -1;
    }
    const ai_system* ai = np->get_ai();
    const vehicle* vp = ai_vehicle_locate_for_man_turret_in_vehicle(np);
    if (vp == nullptr)
    {
        return -1;
    }
    const bool enemy_known = ai->perception.get_first_belief(AI_BELIEF_ENEMY_POSITION) != nullptr;
    const player* squad_player = ai->is_in_player_squad(true);
    if (squad_player != nullptr && !enemy_known && human_in_vehicle(squad_player) == nullptr)
    {
        return -1;
    }
    const human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
    if (driver == nullptr)
    {
        return enemy_known ? 3000 : 1350;
    }
    if (driver->get_obj_type() == OT_HUMAN && driver->get_sub_type() == OT_SUB_HUMAN_PLAYER && squad_player == nullptr)
    {
        return -1;
    }
    return 4800;
}

// 0x006A1EF0 (:4304)
bool man_turret_in_vehicle_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    vehicle* vp = ai_vehicle_locate_for_man_turret_in_vehicle(np);
    if (vp == nullptr)
    {
        return false;
    }
    goal_setup_data.selected_leader = vehicle_passenger_in_seat(vp, VSI_DRIVER);
    goal_setup_data.use_target = vp;
    return true;
}

// 0x005EA530 (:4319)
void man_turret_in_vehicle_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (goal_setup_data.selected_leader != nullptr)
    {
        np->get_ai()->desired_follow_target = goal_setup_data.selected_leader->get_handle();
    }
}

// 0x005EA560 (:4332). LTCG: passenger EAX, driver EDX.
const bool npc_wants_to_be_in_my_car(const npc* passenger, const npc* driver)
{
    const ai_system* ai = passenger->get_ai();
    const ai_action* act = ai->planner.get_current_action();
    if (driver->vehicle_handle == ai->desired_vehicle && (act == goto_vehicle_action::get_instance() || act == enter_vehicle_action::get_instance()))
    {
        return true;
    }
    return act == follow_leader_action::get_instance() && ai->desired_follow_target == driver->get_handle();
}

// 0x00638440 (:4356)
int wait_passenger_clear_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if ((np->npc_flags.passenger_wait || np->npc_flags.player_exit_wait) && np->vehicle_handle != 0xFFFFFFFF)
    {
        const object* objp = World.object_from_handle(np->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            return vehicle_passenger_in_seat(static_cast<const vehicle*>(objp), VSI_DRIVER) == np ? 101000 : -1;
        }
    }
    return -1;
}

static bool passenger_is_coming(const vehicle* vp)
{
    if (vehicle_passenger_in_entering_or_reserving_seat(vp, VSI_DRIVER) != nullptr)
    {
        return true;
    }
    // The rail patrol's vehicle waits for its squad's first member too.
    unsigned int patrol_vehicle;
    unsigned int patrol_squad;
    if (vrail_get_patrol_vehicle(patrol_vehicle) && patrol_vehicle == vp->get_handle() && vrail_get_patrol_squad(patrol_squad))
    {
        const object_squad* squad = World.object_from_handle<object_squad>(patrol_squad);
        return squad != nullptr && World.object_from_handle<human>(squad->member_array[0]) != nullptr;
    }
    return false;
}

// 0x006A1F30 (:4379)
int wait_for_passengers_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const ai_system* ai = np->get_ai();
    if (!human_is_driving(np))
    {
        return -1;
    }
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr || (!human_team_is_non_combatant(np) && vehicle_interaction_passengers_stuck(vp)))
    {
        return -1;
    }
    // Original: no null check of the local player.
    const player* local = player_get_local_player();
    if (local->vehicle_handle != 0xFFFFFFFF && vec_dist_squared(local->pos, np->pos) > 1600.0f)
    {
        return -1;
    }

    bool drivable = vehicle_is_usable(nullptr, nullptr, vp, false, false) && vp->get_sub_type() != OT_SUB_VEHICLE_WALKER
        && (!vp->m_flags.player_may_capture || (unsigned int)vp->info->m_abandon_hitpoints < vehicle_get_hit_points(vp)) && vp->no_npc_entry_timer.elapsed();
    if (drivable && vp->get_sub_type() != OT_SUB_VEHICLE_FLYER && vp->orient.uvec.y < 0.5f)
    {
        float speed;
        vehicle_get_speed(vp, speed);
        drivable = 1.0f <= speed;
    }
    if (drivable)
    {
        const bool burning = vp->m_flags.m_on_fire ? !vp->m_flags.m_invulnerable : (vp->m_flags.m_engine_smoking && vp->get_sub_type() != OT_SUB_VEHICLE_FLYER);
        if (!burning && passenger_is_coming(vp) && passenger_seat_available(vp, nullptr))
        {
            const object* defended = ai->get_defend_object();
            const int num_npcs = World.get_num_objects<npc>();
            if (defended == nullptr || defended->get_obj_type() != OT_HUMAN)
            {
                for (int i = 0; i < num_npcs; i++)
                {
                    const npc* other = World.get_object<npc>(i);
                    if (other != np && human_teams_get_disposition(np, other) == TEAM_DISPOSITION_ALLIED && human_in_vehicle(other) == nullptr
                        && (human_in_vehicle_transition(other, vp, VI_INTERACTION_NONE)
                            || (vec_dist_squared(np->pos, other->pos) <= 900.0f && npc_wants_to_be_in_my_car(other, np))))
                    {
                        return np->npc_flags.bureaucrat ? 6000 : 4600;
                    }
                }
                return -1;
            }
            for (int i = 0; i < num_npcs; i++)
            {
                const npc* other = World.get_object<npc>(i);
                if (other == np || human_in_vehicle(other) != nullptr)
                {
                    continue;
                }
                const ai_system* other_ai = other->get_ai();
                if (other_ai->get_defend_object() == defended && vec_dist_squared(np->pos, other->pos) <= 900.0f
                    && other_ai->planner.get_current_goal() == move_with_human_goal::get_instance() && other_ai->planner.plan_contains(enter_vehicle_action::get_instance()))
                {
                    return 4600;
                }
            }
            return -1;
        }
    }

    // Eight interaction slots.
    for (int i = 0; i < 8; i++)
    {
        const vi_human_manager& slot = vp->vii.humans[i];
        if (slot.handle != 0xFFFFFFFF && (slot.interaction == VI_INTERACTION_RESERVE || slot.interaction == VI_INTERACTION_ENTER))
        {
            return 4600;
        }
    }
    return -1;
}

// 0x00660080 (:4520)
int drop_off_passengers_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (!human_is_driving(np))
    {
        return -1;
    }
    const vehicle* vp = human_in_vehicle(np);
    return (vp != nullptr && ai_vehicle_should_stop_to_let_passenger_out(vp)) ? 4700 : -1;
}

// 0x005EA5B0 (:4543)
int tied_up_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    return np->npc_flags.tied_up ? 90300 : -1;
}

// The convoy of a convoy squad's leader, if the leader's squad leads the convoy (lead) or not (!lead).
static object_convoy* get_led_convoy(const npc* np, bool lead)
{
    const ai_system* ai = np->get_ai();
    gfm_orders* orders = get_ai_squad_orders(ai);
    if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_CONVOY)
    {
        return nullptr;
    }
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr || squad->get_leader() != np)
    {
        return nullptr;
    }
    object_convoy* convoy = ai->get_convoy_object();
    if (convoy == nullptr || (convoy->get_lead_squad_handle() == squad->get_handle()) != lead)
    {
        return nullptr;
    }
    return convoy;
}

// 0x0067E190 (:4555)
int convoy_lead_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const object_convoy* convoy = get_led_convoy(np, true);
    if (convoy == nullptr)
    {
        return -1;
    }
    if (!convoy->flags.attacked)
    {
        return 4500;
    }
    return convoy->get_squad_role(np->get_ai()->get_squad_handle()) != CONVOY_SQUAD_ROLE_BREAK_AND_ATTACK ? 4500 : 1650;
}

// 0x0067E270 (:4592)
bool convoy_lead_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const object_convoy* convoy = get_led_convoy(np, true);
    if (convoy == nullptr)
    {
        return false;
    }
    goal_setup_data.defend_object = convoy;
    goal_setup_data.selected_convoy = convoy;
    return true;
}

// 0x005EA5D0 (:4619)
void convoy_lead_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_destination_object = goal_setup_data.selected_convoy->get_handle();
    ai->desired_vehicle_speed = goal_setup_data.selected_convoy->get_move_speed();
}

// 0x0067E320 (:4633)
int convoy_follow_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const object_convoy* convoy = get_led_convoy(np, false);
    if (convoy == nullptr)
    {
        return -1;
    }
    if (!convoy->flags.attacked)
    {
        return 4500;
    }
    const unsigned int squad_handle = np->get_ai()->get_squad_handle();
    const convoy_squad_roles role = convoy->get_squad_role(squad_handle);
    if (role == CONVOY_SQUAD_ROLE_BREAK_AND_ATTACK || !convoy->in_convoy_formation(squad_handle))
    {
        return 1650;
    }
    return (role == CONVOY_SQUAD_ROLE_TARGET || role == CONVOY_SQUAD_ROLE_DEFENDER) ? 4500 : -1;
}

static object_squad* get_convoy_on_foot_follow_squad(const npc* np)
{
    const ai_system* ai = np->get_ai();
    gfm_orders* orders = get_ai_squad_orders(ai);
    if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_CONVOY)
    {
        return nullptr;
    }
    const object_convoy* convoy = ai->get_convoy_object();
    if (convoy == nullptr || convoy->get_squad_role(ai->get_squad_handle()) != CONVOY_SQUAD_ROLE_ON_FOOT_ESCORT)
    {
        return nullptr;
    }
    return convoy->get_on_foot_follow_squad(ai->get_squad_handle());
}

// 0x0067E410 (:4686)
int convoy_escort_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const object_squad* squad = get_convoy_on_foot_follow_squad(np);
    if (squad == nullptr)
    {
        return -1;
    }
    if (squad->simulated())
    {
        return squad->veh_spawn_group != nullptr ? 1400 : -1;
    }
    return squad->get_vehicle() != nullptr ? 1400 : -1;
}

// 0x0067E4D0 (:4727)
bool convoy_escort_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const object_squad* squad = get_convoy_on_foot_follow_squad(np);
    if (squad == nullptr)
    {
        return false;
    }
    if (squad->simulated())
    {
        goal_setup_data.defend_object = squad;
        return true;
    }
    const vehicle* vp = squad->get_vehicle();
    if (vp == nullptr)
    {
        return false;
    }
    goal_setup_data.defend_object = vp;
    return true;
}

// 0x005EA600 (:4770)
void convoy_escort_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (goal_setup_data.defend_object != nullptr)
    {
        np->get_ai()->desired_follow_target = goal_setup_data.defend_object->get_handle();
    }
}

// 0x0067E580 (:4780)
int wait_for_bodyguards_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (!np->npc_flags.bureaucrat)
    {
        return -1;
    }
    return ai_get_wait_for_bodyguards_result(np) == AI_RESULT_IN_PROGRESS ? 6000 : -1;
}

// 0x0067E5B0 (:4798)
int examine_corpse_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const ai_system* ai = np->get_ai();
    // Friendly or better with EDF, and not on alert.
    const human_team_dispositions disposition = human_teams_get_disposition(np, HUMAN_TEAM_EDF);
    if ((disposition >= TEAM_DISPOSITION_FRIENDLY && gsm_get_alert_level() >= 1) || activity_on_alert(np) || disposition < TEAM_DISPOSITION_FRIENDLY
        || player_find_shortest_player_dist_sqrd(np->pos) > 40000.0f)
    {
        return -1;
    }

    if (ai->reserved_corpse_handle != 0xFFFFFFFF)
    {
        const object* objp = World.object_from_handle(ai->reserved_corpse_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_HUMAN)
        {
            const human* corpse = static_cast<const human*>(objp);
            if (!corpse->hflags.death_reported && corpse->is_killed() && human_teams_are_friendly(np, corpse))
            {
                return 1150;
            }
        }
    }

    belief_obj_iterator<human> corpses;
    corpses.init(ai, AI_BELIEF_CORPSE);
    for (; corpses; ++corpses)
    {
        const human* corpse = corpses;
        if (corpse->is_killed() && !corpse->hflags.death_reported && human_teams_get_disposition(np, corpse) >= TEAM_DISPOSITION_FRIENDLY)
        {
            return 1150;
        }
    }
    return -1;
}

// 0x0067E730 (:4849)
bool examine_corpse_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_system* ai = np->get_ai();
    if (ai->reserved_corpse_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(ai->reserved_corpse_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_HUMAN)
        {
            const human* corpse = static_cast<const human*>(objp);
            if (!corpse->hflags.death_reported && corpse->is_killed() && human_teams_get_disposition(np, corpse) >= TEAM_DISPOSITION_FRIENDLY)
            {
                goal_setup_data.selected_target = objp;
                return true;
            }
        }
    }

    human* nearest = nullptr;
    float nearest_dist = 3.4028235e+38f; // FLT_MAX
    belief_obj_iterator<human> corpses;
    corpses.init(ai, AI_BELIEF_CORPSE);
    for (; corpses; ++corpses)
    {
        human* corpse = corpses;
        if (!corpse->hflags.death_reported && human_teams_get_disposition(np, corpse) >= TEAM_DISPOSITION_FRIENDLY)
        {
            const float dist = vec_dist_squared(np->pos, corpse->pos);
            if (dist < nearest_dist)
            {
                nearest = corpse;
                nearest_dist = dist;
            }
        }
    }
    if (nearest == nullptr)
    {
        return false;
    }
    goal_setup_data.selected_target = nearest;
    return true;
}

// 0x005EA630 (:4899)
void examine_corpse_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (goal_setup_data.selected_target != nullptr)
    {
        np->get_ai()->reserved_corpse_handle = goal_setup_data.selected_target->get_handle();
    }
}

// 0x006384A0 (:4913)
int move_out_of_way_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const timestamp& timer = np->avoidance_request_timer;
    if (np->get_ai()->planner.get_current_goal() == this)
    {
        return timer.valid() ? 3450 : -1;
    }
    if (!timer.valid() || timer.elapsed())
    {
        return -1;
    }
    if (np->avoidance_request_human == 0xFFFFFFFF)
    {
        return 3450;
    }
    const object* objp = World.object_from_handle(np->avoidance_request_human);
    if (objp != nullptr && objp->get_obj_type() == OT_HUMAN && !objp->is_killed())
    {
        if (vec_dist_squared(np->pos, objp->pos) <= 4.0f || static_cast<const human*>(objp)->hflags.is_on_ladder)
        {
            return 3450;
        }
    }
    return -1;
}

// 0x005EA660 (:4949)
int get_out_of_vehicle_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (human_in_vehicle(np) == nullptr)
    {
        return -1;
    }
    return np->npc_flags.force_out_of_vehicle ? 150000 : -1;
}

// 0x006A22E0 (:4965)
int salute_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (human_teams_get_disposition(np, HUMAN_TEAM_EDF) != TEAM_DISPOSITION_ALLIED)
    {
        return -1;
    }
    const ai_system* ai = np->get_ai();
    if (!ai->salute_timer.elapsed() || human_combat_get_manned_turret(np) != nullptr)
    {
        return -1;
    }
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr || squad->get_leader() != np)
    {
        return -1;
    }
    gfm_orders* orders = squad->get_squad_orders();
    if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_DEFEND)
    {
        return -1;
    }
    const object* defended = orders->get_target_object();
    if (defended == nullptr || (defended->get_sub_type() != OT_SUB_MOVER_RFG && defended->get_obj_type() != OT_BOUNDING_BOX))
    {
        return -1;
    }
    return ai->is_at_guard_post() ? 325 : -1;
}

// 0x006600C0 (:5009)
bool salute_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const object_squad* own_squad = np->get_ai()->get_squad();
    if (own_squad == nullptr || own_squad->get_leader() != np)
    {
        return false;
    }
    gfm_orders* orders = own_squad->get_squad_orders();
    if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_DEFEND)
    {
        return false;
    }
    // A patrol's leader within 20 m and within 45 degrees of facing.
    const int num_squads = World.get_num_objects<object_squad>();
    for (int i = 0; i < num_squads; i++)
    {
        const object_squad* squad = World.get_object<object_squad>(i);
        if (squad->get_squad_orders_type() != GFM_ORDERS_TYPE_PATROL)
        {
            continue;
        }
        npc* leader = squad->get_leader();
        if (leader == nullptr || leader->is_killed() || vec_dist_squared(np->pos, leader->pos) > 400.0f)
        {
            continue;
        }
        const vector to_leader(leader->pos.x - np->pos.x, leader->pos.y - np->pos.y, leader->pos.z - np->pos.z);
        const vector direction = to_leader.copy_normalize_safe();
        if (0.70710677f <= np->orient.fvec.y * direction.y + np->orient.fvec.x * direction.x + np->orient.fvec.z * direction.z)
        {
            goal_setup_data.selected_target = leader;
            goal_setup_data.attention_mode = ATTENTION_AIM_AT_OBJECT;
            goal_setup_data.attention_steering_mode = ATTENTION_STEERING_TURN_TO_TARGET;
            return true;
        }
    }
    return false;
}

// 0x006A23A0 (:5054)
void salute_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    object* target = goal_setup_data.selected_target;
    if (target == nullptr || target->get_obj_type() != OT_HUMAN || target->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return;
    }
    ai_system* target_ai = static_cast<npc*>(target)->get_ai();
    if (np != nullptr)
    {
        target_ai->reaction_info.belief_type = AI_BELIEF_UNDEFINED;
        target_ai->reaction_info.target_handle = np->get_handle();
        target_ai->reaction_info.reason = 0x800;
        target_ai->reaction_info.time_left.set(int_rand_range(2000, 3000));
    }
    target_ai->planner.request_replan();
    goal_setup_data.set_attention(np, nullptr);
    np->get_ai()->salute_timer.set(15000);
}

// 0x005EA6A0 (:5072)
int move_to_location_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    return np->npc_flags.scripted_move == 1 ? 120000 : -1;
}

// 0x005EA6D0 (:5083)
int drive_to_location_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (human_in_vehicle(np) == nullptr)
    {
        return -1;
    }
    return np->npc_flags.scripted_drive == 1 ? 115000 : -1;
}

// 0x00638560 (:5098)
int loiter_flyer_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const ai_perception& perception = np->get_ai()->perception;
    if (np->vehicle_handle != 0xFFFFFFFF)
    {
        const object* objp = World.object_from_handle(np->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && objp->get_sub_type() == OT_SUB_VEHICLE_FLYER && perception.get_first_belief(AI_BELIEF_ENEMY_EXISTS) != nullptr)
        {
            return perception.get_first_belief(AI_BELIEF_ENEMY_POSITION) != nullptr ? -1 : 1100;
        }
    }
    return -1;
}

static const flyer* as_flyer(const object* objp)
{
    return (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && objp->get_sub_type() == OT_SUB_VEHICLE_FLYER) ? static_cast<const flyer*>(objp) : nullptr;
}

static vehicle* find_nearest_free_flyer(const npc* np)
{
    vehicle* nearest = nullptr;
    float nearest_dist = 3.4028235e+38f; // FLT_MAX
    const int num_flyers = World.get_num_objects<flyer>();
    for (int i = 0; i < num_flyers; i++)
    {
        vehicle* vp = World.get_object<flyer>(i);
        if (ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER) && vp != nullptr && ai_vehicle_is_seat_available(vp, np, VSI_DRIVER) && !vehicle_is_reserved(vp))
        {
            const float dist = vec_dist_squared(vp->pos, np->pos);
            if (dist < nearest_dist)
            {
                nearest_dist = dist;
                nearest = vp;
            }
        }
    }
    return nearest;
}

// 0x0067E8A0 (:5123)
int man_flyer_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (!np->npc_flags.man_flyer)
    {
        return -1;
    }
    const flyer* in_flyer = as_flyer(np->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(np->vehicle_handle) : nullptr);
    if (in_flyer != nullptr)
    {
        return in_flyer->flyer_flags.on_ground ? 95000 : -1;
    }
    if (as_flyer(np->reserved_vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(np->reserved_vehicle_handle) : nullptr) != nullptr)
    {
        return 95000;
    }
    const object_squad* squad = np->get_ai()->get_squad();
    if (squad != nullptr && squad->squad_vehicle != 0xFFFFFFFF)
    {
        const flyer* squad_flyer = as_flyer(World.object_from_handle(squad->squad_vehicle));
        if (squad_flyer != nullptr && !squad_flyer->m_flags.m_corpse && !squad_flyer->m_flags.m_on_fire && !vehicle_is_reserved(squad_flyer))
        {
            return 95000;
        }
    }
    return find_nearest_free_flyer(np) != nullptr ? 95000 : -1;
}

// 0x0067EA70 (:5185)
bool man_flyer_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    object* objp = np->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(np->vehicle_handle) : nullptr;
    if (as_flyer(objp) == nullptr)
    {
        objp = np->reserved_vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(np->reserved_vehicle_handle) : nullptr;
    }
    if (as_flyer(objp) == nullptr)
    {
        const object_squad* squad = np->get_ai()->get_squad();
        objp = (squad != nullptr && squad->squad_vehicle != 0xFFFFFFFF) ? World.object_from_handle(squad->squad_vehicle) : nullptr;
    }
    if (as_flyer(objp) == nullptr)
    {
        objp = find_nearest_free_flyer(np);
        if (objp == nullptr)
        {
            return false;
        }
    }
    goal_setup_data.use_target = objp;
    return true;
}

// 0x005EA710 (:5231)
void man_flyer_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    ai_system* ai = np->get_ai();
    vehicle_reserve_vehicle(static_cast<vehicle*>(goal_setup_data.use_target), np);
    ai->desired_vehicle = goal_setup_data.use_target->get_handle();
    ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_DRIVER;
}

// 0x006385C0 (:5244)
int fodder_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    gfm_orders* orders = get_ai_squad_orders(np->get_ai());
    if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_FODDER)
    {
        return -1;
    }
    return human_is_driving(np) ? 95000 : -1;
}

// 0x006602C0 (:5274)
int get_back_on_navmesh_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (!human_is_driving(np) || np->vehicle_handle == 0xFFFFFFFF)
    {
        return -1;
    }
    const object* objp = World.object_from_handle(np->vehicle_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
    {
        return -1;
    }
    const vehicle* vp = static_cast<const vehicle*>(objp);
    if (!vp->vai.vai_flags.fallen_off_navmesh || (ai_vehicle_should_stop_to_let_passenger_out(vp) && !vehicle_interaction_passengers_stuck(vp)))
    {
        return -1;
    }
    return 140000;
}

// 0x00638620 (:5302)
int recover_from_stuck_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (human_is_driving(np))
    {
        return -1;
    }
    if (np->vehicle_handle != 0xFFFFFFFF)
    {
        const object* objp = World.object_from_handle(np->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            return -1;
        }
    }
    return np->hflags.extended_stuck ? 130000 : -1;
}

// 0x00638680 (:5323)
int pull_off_road_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (!human_is_driving(np) || np->vehicle_handle == 0xFFFFFFFF)
    {
        return -1;
    }
    const object* objp = World.object_from_handle(np->vehicle_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
    {
        return -1;
    }
    const vehicle* vp = static_cast<const vehicle*>(objp);
    if (vp->is_ambient() && vp->vai.vai_flags.pull_off && vp->vai.pull_off_timer.valid() && !vp->vai.pull_off_timer.elapsed())
    {
        return 6001;
    }
    return -1;
}

// The singletons, in rfg.exe's construction order: it fills goal_pool, and that order breaks ties in rank_goals.
idle_goal idle_goal::the_instance; // 0x01DDAC50

// 0x005FB980
idle_goal::idle_goal()
    : ai_goal(ai_plan_conditions(67), "idle")
{
}

// 0x005DCDE0
idle_goal::~idle_goal()
{
}

idle_despawn_goal idle_despawn_goal::the_instance; // 0x01DC9788

// 0x005FBAC0
idle_despawn_goal::idle_despawn_goal()
    : ai_goal(ai_plan_conditions(70), "idle despawn")
{
}

ai_goal* idle_despawn_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCE00
idle_despawn_goal::~idle_despawn_goal()
{
}

despawn_goal despawn_goal::the_instance; // 0x01D755A8

// 0x005FBA20
despawn_goal::despawn_goal()
    : ai_goal(ai_plan_conditions(70), "despawn")
{
}

ai_goal* despawn_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCDF0
despawn_goal::~despawn_goal()
{
}

wait_goal wait_goal::the_instance; // 0x01D7566C

// 0x005FBB60
wait_goal::wait_goal()
    : ai_goal(ai_plan_conditions(76), "wait")
{
}

ai_goal* wait_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCE10
wait_goal::~wait_goal()
{
}

kill_enemy_goal kill_enemy_goal::the_instance; // 0x01DDB9B4

// 0x005FBC00
kill_enemy_goal::kill_enemy_goal()
    : ai_goal(ai_plan_conditions(78), "kill enemy")
{
}

ai_goal* kill_enemy_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCE20
kill_enemy_goal::~kill_enemy_goal()
{
}

melee_goal melee_goal::the_instance; // 0x01DCA364

// 0x005FBCA0
melee_goal::melee_goal()
    : ai_goal(ai_plan_conditions(79), "melee")
{
}

ai_goal* melee_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCE40
melee_goal::~melee_goal()
{
}

melee_vehicle_goal melee_vehicle_goal::the_instance; // 0x01D75414

// 0x005FBD40
melee_vehicle_goal::melee_vehicle_goal()
    : ai_goal(ai_plan_conditions(80), "melee vehicle")
{
}

ai_goal* melee_vehicle_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCE50
melee_vehicle_goal::~melee_vehicle_goal()
{
}

melee_safe_position_goal melee_safe_position_goal::the_instance; // 0x01DC952C

// 0x005FBDE0
melee_safe_position_goal::melee_safe_position_goal()
    : ai_goal(ai_plan_conditions(81), "melee safe position")
{
}

ai_goal* melee_safe_position_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCE70
melee_safe_position_goal::~melee_safe_position_goal()
{
}

adjust_cover_stance_goal adjust_cover_stance_goal::the_instance; // 0x01DA60C0

// 0x005FC380
adjust_cover_stance_goal::adjust_cover_stance_goal()
    : ai_goal(ai_plan_conditions(63), "adjust cover stance")
{
}

ai_goal* adjust_cover_stance_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCF70
adjust_cover_stance_goal::~adjust_cover_stance_goal()
{
}

flush_out_target_goal flush_out_target_goal::the_instance; // 0x01DCA6B0

// 0x005FC100
flush_out_target_goal::flush_out_target_goal()
    : ai_goal(ai_plan_conditions(21), "flush out target")
{
}

ai_goal* flush_out_target_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCF00
flush_out_target_goal::~flush_out_target_goal()
{
}

acquire_target_goal acquire_target_goal::the_instance; // 0x01DDACD4

// 0x005FC1A0
acquire_target_goal::acquire_target_goal()
    : ai_goal(ai_plan_conditions(23), "acquire target")
{
}

ai_goal* acquire_target_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCF20
acquire_target_goal::~acquire_target_goal()
{
}

defensive_combat_goal defensive_combat_goal::the_instance; // 0x01D74F04

// 0x005FC240
defensive_combat_goal::defensive_combat_goal()
    : ai_goal(ai_plan_conditions(24), "defensive combat")
{
}

ai_goal* defensive_combat_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCF40
defensive_combat_goal::~defensive_combat_goal()
{
}

combat_support_goal combat_support_goal::the_instance; // 0x01DA0270

// 0x005FC2E0
combat_support_goal::combat_support_goal()
    : ai_goal(ai_plan_conditions(88), "combat support")
{
}

ai_goal* combat_support_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCF60
combat_support_goal::~combat_support_goal()
{
}

reload_weapon_goal reload_weapon_goal::the_instance; // 0x01D7516C

// 0x005FBE80
reload_weapon_goal::reload_weapon_goal()
    : ai_goal(ai_plan_conditions(1), "reload weapon")
{
}

ai_goal* reload_weapon_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCE90
reload_weapon_goal::~reload_weapon_goal()
{
}

dodge_goal dodge_goal::the_instance; // 0x01DC99CC

// 0x005FBF20
dodge_goal::dodge_goal()
    : ai_goal(ai_plan_conditions(82), "dodge")
{
}

ai_goal* dodge_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCEB0
dodge_goal::~dodge_goal()
{
}

advance_goal advance_goal::the_instance; // 0x01D67E44

// 0x005FBFC0
advance_goal::advance_goal()
    : ai_goal(ai_plan_conditions(83), "advance")
{
}

ai_goal* advance_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCED0
advance_goal::~advance_goal()
{
}

fall_back_goal fall_back_goal::the_instance; // 0x01DC9704

// 0x005FC060
fall_back_goal::fall_back_goal()
    : ai_goal(ai_plan_conditions(84), "fall back")
{
}

ai_goal* fall_back_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCEE0
fall_back_goal::~fall_back_goal()
{
}

guard_object_goal guard_object_goal::the_instance; // 0x01DDB06C

// 0x005FC560
guard_object_goal::guard_object_goal()
    : ai_goal(ai_plan_conditions(31), "guard object")
{
}

ai_goal* guard_object_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCFA0
guard_object_goal::~guard_object_goal()
{
}

roadblock_goal roadblock_goal::the_instance; // 0x01DDADE0

// 0x005FC600
roadblock_goal::roadblock_goal()
    : ai_goal(ai_plan_conditions(32), "roadblock")
{
}

ai_goal* roadblock_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCFB0
roadblock_goal::~roadblock_goal()
{
}

use_action_node_goal use_action_node_goal::the_instance; // 0x01642D10

// 0x005FCB00
use_action_node_goal::use_action_node_goal()
    : ai_goal(ai_plan_conditions(133), "use action node")
{
}

ai_goal* use_action_node_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD070
use_action_node_goal::~use_action_node_goal()
{
}

move_with_human_goal move_with_human_goal::the_instance; // 0x01DCA314

// 0x005FC740
move_with_human_goal::move_with_human_goal()
    : ai_goal(ai_plan_conditions(33), "move with human")
{
}

ai_goal* move_with_human_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCFE0
move_with_human_goal::~move_with_human_goal()
{
}

return_to_orders_pos_goal return_to_orders_pos_goal::the_instance; // 0x01D725D8

// 0x005FCBA0
return_to_orders_pos_goal::return_to_orders_pos_goal()
    : ai_goal(ai_plan_conditions(43), "return to orders pos")
{
}

ai_goal* return_to_orders_pos_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD080
return_to_orders_pos_goal::~return_to_orders_pos_goal()
{
}

confront_civilian_goal confront_civilian_goal::the_instance; // 0x01D9F83C

// 0x005FCD80
confront_civilian_goal::confront_civilian_goal()
    : ai_goal(ai_plan_conditions(101), "confront civilian")
{
}

// 0x005DD0C0
confront_civilian_goal::~confront_civilian_goal()
{
}

threaten_civilian_goal threaten_civilian_goal::the_instance; // 0x01D68570

// 0x005FCE20
threaten_civilian_goal::threaten_civilian_goal()
    : ai_goal(ai_plan_conditions(104), "threaten civilian")
{
}

// 0x005DD0E0
threaten_civilian_goal::~threaten_civilian_goal()
{
}

threaten_watch_goal threaten_watch_goal::the_instance; // 0x01D74F20

// 0x005FCEC0
threaten_watch_goal::threaten_watch_goal()
    : ai_goal(ai_plan_conditions(105), "threaten watch")
{
}

// 0x005DD100
threaten_watch_goal::~threaten_watch_goal()
{
}

oppress_civilian_goal oppress_civilian_goal::the_instance; // 0x01D9F960

// 0x005FCF60
oppress_civilian_goal::oppress_civilian_goal()
    : ai_goal(ai_plan_conditions(106), "oppress civilian")
{
}

ai_goal* oppress_civilian_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD120
oppress_civilian_goal::~oppress_civilian_goal()
{
}

taunt_civilian_goal taunt_civilian_goal::the_instance; // 0x01D67F48

// 0x005FD000
taunt_civilian_goal::taunt_civilian_goal()
    : ai_goal(ai_plan_conditions(111), "taunt civilian")
{
}

ai_goal* taunt_civilian_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD130
taunt_civilian_goal::~taunt_civilian_goal()
{
}

taunt_player_goal taunt_player_goal::the_instance; // 0x01D75040

// 0x005FD0A0
taunt_player_goal::taunt_player_goal()
    : ai_goal(ai_plan_conditions(110), "taunt player")
{
}

ai_goal* taunt_player_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD140
taunt_player_goal::~taunt_player_goal()
{
}

taunt_enemy_goal taunt_enemy_goal::the_instance; // 0x01D74EE8

// 0x005FD140
taunt_enemy_goal::taunt_enemy_goal()
    : ai_goal(ai_plan_conditions(112), "taunt enemy")
{
}

ai_goal* taunt_enemy_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD150
taunt_enemy_goal::~taunt_enemy_goal()
{
}

flee_restricted_area_goal flee_restricted_area_goal::the_instance; // 0x01DCA454

// 0x005FC9C0
flee_restricted_area_goal::flee_restricted_area_goal()
    : ai_goal(ai_plan_conditions(107), "flee restricted area")
{
}

// 0x005DD040
flee_restricted_area_goal::~flee_restricted_area_goal()
{
}

flee_heavy_vehicle_goal flee_heavy_vehicle_goal::the_instance; // 0x01DCA47C

// 0x005FCA60
flee_heavy_vehicle_goal::flee_heavy_vehicle_goal()
    : ai_goal(ai_plan_conditions(108), "flee heavy vehicle")
{
}

// 0x005DD060
flee_heavy_vehicle_goal::~flee_heavy_vehicle_goal()
{
}

patrol_path_goal patrol_path_goal::the_instance; // 0x01D7207C

// 0x005FC6A0
patrol_path_goal::patrol_path_goal()
    : ai_goal(ai_plan_conditions(38), "patrol path")
{
}

ai_goal* patrol_path_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCFC0
patrol_path_goal::~patrol_path_goal()
{
}

follow_leader_goal follow_leader_goal::the_instance; // 0x01D74ECC

// 0x005FC7E0
follow_leader_goal::follow_leader_goal()
    : ai_goal(ai_plan_conditions(39), "follow leader")
{
}

ai_goal* follow_leader_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCFF0
follow_leader_goal::~follow_leader_goal()
{
}

wait_for_squad_goal wait_for_squad_goal::the_instance; // 0x01DC99E8

// 0x005FC880
wait_for_squad_goal::wait_for_squad_goal()
    : ai_goal(ai_plan_conditions(40), "wait for squad")
{
}

ai_goal* wait_for_squad_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD000
wait_for_squad_goal::~wait_for_squad_goal()
{
}

reinforcement_goal reinforcement_goal::the_instance; // 0x01DDB924

// 0x005FC920
reinforcement_goal::reinforcement_goal()
    : ai_goal(ai_plan_conditions(41), "reinforcement")
{
}

ai_goal* reinforcement_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD020
reinforcement_goal::~reinforcement_goal()
{
}

respond_to_honk respond_to_honk::the_instance; // 0x01642D2C

// 0x005FCCE0
respond_to_honk::respond_to_honk()
    : ai_goal(ai_plan_conditions(140), "respond to honk")
{
}

ai_goal* respond_to_honk::get_instance()
{
    return &the_instance;
}

// 0x005DD0A0
respond_to_honk::~respond_to_honk()
{
}

chase_target_goal chase_target_goal::the_instance; // 0x01D9F620

// 0x005FD320
chase_target_goal::chase_target_goal()
    : ai_goal(ai_plan_conditions(51), "chase target")
{
}

ai_goal* chase_target_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD190
chase_target_goal::~chase_target_goal()
{
}

accompany_chase_target_goal accompany_chase_target_goal::the_instance; // 0x01DC9B18

// 0x005FD3C0
accompany_chase_target_goal::accompany_chase_target_goal()
    : ai_goal(ai_plan_conditions(52), "accompany chase")
{
}

ai_goal* accompany_chase_target_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD1A0
accompany_chase_target_goal::~accompany_chase_target_goal()
{
}

smash_building_goal smash_building_goal::the_instance; // 0x01DA0550

// 0x005FD1E0
smash_building_goal::smash_building_goal()
    : ai_goal(ai_plan_conditions(53), "smash building")
{
}

ai_goal* smash_building_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD160
smash_building_goal::~smash_building_goal()
{
}

man_flyer_goal man_flyer_goal::the_instance; // 0x01DDAD58

// 0x005FD780
man_flyer_goal::man_flyer_goal()
    : ai_goal(ai_plan_conditions(55), "man flyer")
{
}

// 0x005DD220
man_flyer_goal::~man_flyer_goal()
{
}

loiter_flyer_goal loiter_flyer_goal::the_instance; // 0x01DC9BDC

// 0x005FD6E0
loiter_flyer_goal::loiter_flyer_goal()
    : ai_goal(ai_plan_conditions(23), "loiter flyer")
{
}

ai_goal* loiter_flyer_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD200
loiter_flyer_goal::~loiter_flyer_goal()
{
}

drive_on_rail_goal drive_on_rail_goal::the_instance; // 0x01642D48

// 0x005FD280
drive_on_rail_goal::drive_on_rail_goal()
    : ai_goal(ai_plan_conditions(141), "drive on rail")
{
}

ai_goal* drive_on_rail_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD170
drive_on_rail_goal::~drive_on_rail_goal()
{
}

man_turret_in_vehicle_goal man_turret_in_vehicle_goal::the_instance; // 0x01DDB0F0

// 0x005FD460
man_turret_in_vehicle_goal::man_turret_in_vehicle_goal()
    : ai_goal(ai_plan_conditions(46), "man turret in vehicle")
{
}

ai_goal* man_turret_in_vehicle_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD1B0
man_turret_in_vehicle_goal::~man_turret_in_vehicle_goal()
{
}

wait_for_passengers_goal wait_for_passengers_goal::the_instance; // 0x01DC97B0

// 0x005FD500
wait_for_passengers_goal::wait_for_passengers_goal()
    : ai_goal(ai_plan_conditions(44), "wait for passengers")
{
}

ai_goal* wait_for_passengers_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD1D0
wait_for_passengers_goal::~wait_for_passengers_goal()
{
}

drop_off_passengers_goal drop_off_passengers_goal::the_instance; // 0x01D68304

// 0x005FD640
drop_off_passengers_goal::drop_off_passengers_goal()
    : ai_goal(ai_plan_conditions(45), "drop off passengers")
{
}

ai_goal* drop_off_passengers_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD1F0
drop_off_passengers_goal::~drop_off_passengers_goal()
{
}

wait_passenger_clear_goal wait_passenger_clear_goal::the_instance; // 0x01D75188

// 0x005FD5A0
wait_passenger_clear_goal::wait_passenger_clear_goal()
    : ai_goal(ai_plan_conditions(57), "wait passenger clear")
{
}

ai_goal* wait_passenger_clear_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD1E0
wait_passenger_clear_goal::~wait_passenger_clear_goal()
{
}

tied_up_goal tied_up_goal::the_instance; // 0x01642D64

// 0x005FD960
tied_up_goal::tied_up_goal()
    : ai_goal(ai_plan_conditions(146), "tied up")
{
}

ai_goal* tied_up_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD260
tied_up_goal::~tied_up_goal()
{
}

convoy_lead_goal convoy_lead_goal::the_instance; // 0x01642D80

// 0x005FDA00
convoy_lead_goal::convoy_lead_goal()
    : ai_goal(ai_plan_conditions(147), "convoy lead")
{
}

ai_goal* convoy_lead_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD270
convoy_lead_goal::~convoy_lead_goal()
{
}

convoy_follow_goal convoy_follow_goal::the_instance; // 0x01642D9C

// 0x005FDAA0
convoy_follow_goal::convoy_follow_goal()
    : ai_goal(ai_plan_conditions(148), "convoy follow")
{
}

ai_goal* convoy_follow_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD280
convoy_follow_goal::~convoy_follow_goal()
{
}

convoy_escort_goal convoy_escort_goal::the_instance; // 0x01642DB8

// 0x005FDB40
convoy_escort_goal::convoy_escort_goal()
    : ai_goal(ai_plan_conditions(149), "convoy escort")
{
}

ai_goal* convoy_escort_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD290
convoy_escort_goal::~convoy_escort_goal()
{
}

wait_for_bodyguards_goal wait_for_bodyguards_goal::the_instance; // 0x01642DD4

// 0x005FDBE0
wait_for_bodyguards_goal::wait_for_bodyguards_goal()
    : ai_goal(ai_plan_conditions(150), "wait for bodyguards")
{
}

ai_goal* wait_for_bodyguards_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD2A0
wait_for_bodyguards_goal::~wait_for_bodyguards_goal()
{
}

examine_corpse_goal examine_corpse_goal::the_instance; // 0x01DA0744

// 0x005FDC80
examine_corpse_goal::examine_corpse_goal()
    : ai_goal(ai_plan_conditions(97), "examine corpse")
{
}

ai_goal* examine_corpse_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD2B0
examine_corpse_goal::~examine_corpse_goal()
{
}

move_out_of_way_goal move_out_of_way_goal::the_instance; // 0x01DC9800

// 0x005FDD20
move_out_of_way_goal::move_out_of_way_goal()
    : ai_goal(ai_plan_conditions(75), "move out of way")
{
}

ai_goal* move_out_of_way_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD2C0
move_out_of_way_goal::~move_out_of_way_goal()
{
}

retreat_defensive_perimeter_goal retreat_defensive_perimeter_goal::the_instance; // 0x01642DF0

// 0x005FC420
retreat_defensive_perimeter_goal::retreat_defensive_perimeter_goal()
    : ai_goal(ai_plan_conditions(152), "retreat def perimeter")
{
}

ai_goal* retreat_defensive_perimeter_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCF80
retreat_defensive_perimeter_goal::~retreat_defensive_perimeter_goal()
{
}

gawk_goal gawk_goal::the_instance; // 0x01642E0C

// 0x005FDDC0
gawk_goal::gawk_goal()
    : ai_goal(ai_plan_conditions(138), "gawk")
{
}

ai_goal* gawk_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD2E0
gawk_goal::~gawk_goal()
{
}

get_out_of_vehicle_goal get_out_of_vehicle_goal::the_instance; // 0x01D7524C

// 0x005FDE60
get_out_of_vehicle_goal::get_out_of_vehicle_goal()
    : ai_goal(ai_plan_conditions(47), "get out of vehicle")
{
}

ai_goal* get_out_of_vehicle_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD2F0
get_out_of_vehicle_goal::~get_out_of_vehicle_goal()
{
}

salute_goal salute_goal::the_instance; // 0x01DA0658

// 0x005FDF00
salute_goal::salute_goal()
    : ai_goal(ai_plan_conditions(77), "salute")
{
}

ai_goal* salute_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD300
salute_goal::~salute_goal()
{
}

look_for_trouble_goal look_for_trouble_goal::the_instance; // 0x01642E28

// 0x005FC4C0
look_for_trouble_goal::look_for_trouble_goal()
    : ai_goal(ai_plan_conditions(139), "look for trouble")
{
}

ai_goal* look_for_trouble_goal::get_instance()
{
    return &the_instance;
}

// 0x005DCF90
look_for_trouble_goal::~look_for_trouble_goal()
{
}

move_to_location_goal move_to_location_goal::the_instance; // 0x01642E44

// 0x005FE040
move_to_location_goal::move_to_location_goal()
    : ai_goal(ai_plan_conditions(154), "move to location")
{
}

ai_goal* move_to_location_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD320
move_to_location_goal::~move_to_location_goal()
{
}

drive_to_location_goal drive_to_location_goal::the_instance; // 0x01642E60

// 0x005FE0E0
drive_to_location_goal::drive_to_location_goal()
    : ai_goal(ai_plan_conditions(154), "drive to location")
{
}

ai_goal* drive_to_location_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD340
drive_to_location_goal::~drive_to_location_goal()
{
}

fodder_goal fodder_goal::the_instance; // 0x01D9F8DC

// 0x005FCC40
fodder_goal::fodder_goal()
    : ai_goal(ai_plan_conditions(42), "fodder")
{
}

ai_goal* fodder_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD090
fodder_goal::~fodder_goal()
{
}

get_back_on_navmesh_goal get_back_on_navmesh_goal::the_instance; // 0x01D68650

// 0x005FD820
get_back_on_navmesh_goal::get_back_on_navmesh_goal()
    : ai_goal(ai_plan_conditions(66), "get back on navmesh")
{
}

ai_goal* get_back_on_navmesh_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD230
get_back_on_navmesh_goal::~get_back_on_navmesh_goal()
{
}

recover_from_stuck_goal recover_from_stuck_goal::the_instance; // 0x01DDB010

// 0x005FDFA0
recover_from_stuck_goal::recover_from_stuck_goal()
    : ai_goal(ai_plan_conditions(11), "recover from stuck")
{
}

ai_goal* recover_from_stuck_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD310
recover_from_stuck_goal::~recover_from_stuck_goal()
{
}

pull_off_road_goal pull_off_road_goal::the_instance; // 0x01DA0534

// 0x005FD8C0
pull_off_road_goal::pull_off_road_goal()
    : ai_goal(ai_plan_conditions(56), "pull off road")
{
}

ai_goal* pull_off_road_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD240
pull_off_road_goal::~pull_off_road_goal()
{
}

ambient_edf_goal ambient_edf_goal::the_instance; // 0x01DC9D44

// 0x005FE2C0
ambient_edf_goal::ambient_edf_goal()
    : ai_goal(ai_plan_conditions(100), "ambient edf")
{
}

ai_goal* ambient_edf_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD370
ambient_edf_goal::~ambient_edf_goal()
{
}

custom_orders_goal custom_orders_goal::the_instance; // 0x01DDB8D4

// 0x005FE180
custom_orders_goal::custom_orders_goal()
    : ai_goal(ai_plan_conditions(113), "special orders")
{
}

ai_goal* custom_orders_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD350
custom_orders_goal::~custom_orders_goal()
{
}

custom_civ_action_goal custom_civ_action_goal::the_instance; // 0x01DC981C

// 0x005FE220
custom_civ_action_goal::custom_civ_action_goal()
    : ai_goal(ai_plan_conditions(127), "custom civ action")
{
}

ai_goal* custom_civ_action_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD360
custom_civ_action_goal::~custom_civ_action_goal()
{
}

