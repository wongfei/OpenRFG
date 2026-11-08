// Original: volition/rfg/code/ai/combat/combat_thrown_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/combat/combat_thrown_actions.h"

#include "volition/rfg/code/ai/ai_building_logic.h"
#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/belief_iterator.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/combat/ai_thrown_weapon_logic.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/time/timestamp.h"

// Position 13 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG13")

static bool Ai_use_grenades = true; // 0x016413E3, the low byte read from rfg.exe; nothing writes it
static timestamp Global_ai_grenade_cooldown_timer; // 0x0164264C
static timestamp Global_ai_bomb_cooldown_timer; // 0x01642650
static unsigned int Global_ai_bomb_reservation_handle; // 0x01D65970

// 0x005E6610 (:130). LTCG: np EDI.
static bool can_plant_bomb(const npc* np)
{
    if (np->combat.throw_grenade_timestamp.valid() && !np->combat.throw_grenade_timestamp.elapsed())
    {
        return false;
    }
    if (Global_ai_bomb_reservation_handle != np->get_handle() && Global_ai_bomb_cooldown_timer.valid() && !Global_ai_bomb_cooldown_timer.elapsed())
    {
        return false;
    }
    return true;
}

// 0x005E6660 (:154). LTCG: np EAX, setting_bomb on the stack.
static bool reserve_plant_bomb(npc* np, bool setting_bomb)
{
    if (!can_plant_bomb(np))
    {
        return false;
    }
    Global_ai_bomb_reservation_handle = np->get_handle();
    const int cooldown = int_rand_range(10000, 20000); // ms
    Global_ai_bomb_cooldown_timer.set(cooldown);
    if (setting_bomb)
    {
        Global_ai_grenade_cooldown_timer.set(cooldown);
        np->combat.throw_grenade_timestamp.set_rand(15000, 30000);
    }
    return true;
}

// 0x00626B70 (:180). LTCG: next_action EAX.
static weapon* get_weapon_from_plan(const ai_planner_search_node* next_action)
{
    if (next_action == nullptr)
    {
        return nullptr;
    }
    object* objp = next_action->action_data.use_target;
    // Original bug kept: the search never moves to the next node (0x00626B80 jumps to itself), so a next action that has
    // no weapon hangs here. goto_enemy_to_place_bomb_action's only next action is place_bomb_action, which sets one.
    while (objp == nullptr || objp->get_obj_type() != OT_ITEM || objp->get_sub_type() != OT_SUB_ITEM_WEAPON)
    {
    }
    return static_cast<weapon*>(objp);
}

// 0x00626BA0 (:203). LTCG: np EAX, target ECX, weapon_type on the stack.
static weapon* get_weapon_for_offhand_attack(const npc* np, const human* target, weapon_class_type weapon_type)
{
    if (!Ai_use_grenades || np->npc_flags.disable_grenades != 0 || target == nullptr || target->is_killed())
    {
        return nullptr;
    }
    if (weapon_type == WEAPON_CLASS_SINGULARITY_BOMB)
    {
        if (!can_plant_bomb(np))
        {
            return nullptr;
        }
    }
    else
    {
        if (np->combat.throw_grenade_timestamp.valid() && !np->combat.throw_grenade_timestamp.elapsed())
        {
            return nullptr;
        }
        if (Global_ai_grenade_cooldown_timer.valid() && !Global_ai_grenade_cooldown_timer.elapsed())
        {
            return nullptr;
        }
    }
    const inventory_item* item = inv_item_get_weapon_from_class(np, weapon_type);
    return item != nullptr ? World.object_from_handle<weapon>(item->weapon_handle) : nullptr;
}

// 0x00626C40 (:252)
static bool use_grenade_get_cost_common(const npc* np, const ai_goal_setup_data& goal_setup_data)
{
    const human* hp = human_combat_get_target(np);
    const weapon* wp = get_weapon_for_offhand_attack(np, hp, WEAPON_CLASS_THROWN_GRENADE);
    if (wp == nullptr || hp->current_team == HUMAN_TEAM_CIVILIAN)
    {
        return false;
    }
    vector target_pos = hp->pos;
    const vehicle* vp = nullptr;
    if (human_in_vehicle(hp) != nullptr && hp->vehicle_handle != 0xFFFFFFFF)
    {
        const object* objp = World.object_from_handle(hp->vehicle_handle);
        vp = objp != nullptr && objp->get_obj_type() == OT_VEHICLE ? static_cast<const vehicle*>(objp) : nullptr;
    }
    if (vp != nullptr)
    {
        vector velocity;
        vehicle_get_velocity(vp, velocity);
        target_pos = vp->pos + velocity * ((float)wp->w_info->projectile_info.fuse_time / 1000.0f);
    }
    else if (goal_setup_data.goal == acquire_target_goal::get_instance())
    {
        const ai_belief* belief = np->get_ai()->perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(hp));
        if (belief == nullptr)
        {
            return false;
        }
        target_pos = belief->pos;
    }
    const vector to_target = target_pos - np->pos;
    if (to_target.y > 5.0f) // m
    {
        return false;
    }
    const float xz_dist_sq = to_target.x * to_target.x + to_target.z * to_target.z;
    const weapon_info* w_info = wp->w_info;
    const float blast_radius = w_info->m_ai_explosion_info != nullptr ? w_info->m_ai_explosion_info->m_knockdown_radius : w_info->m_explosion_info->m_knockdown_radius;
    const float max_dist = w_info->projectile_info.max_throw_dist;
    return blast_radius * blast_radius <= xz_dist_sq && xz_dist_sq <= max_dist * max_dist && (human_combat_cover_is_using_node(np) || 0.0f <= to_target * np->orient.fvec);
}

// 0x0069A4B0 (:325)
ai_results attack_grenade_enter_common(npc* np, bool try_to_lead_target, vector* throw_vel_output)
{
    ai_system* ai = np->get_ai();
    if (Global_ai_grenade_cooldown_timer.valid() && !Global_ai_grenade_cooldown_timer.elapsed())
    {
        return AI_RESULT_FAILURE;
    }
    const human* hp = human_combat_get_target(np);
    if (hp == nullptr || hp->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    const inventory_item* item = inv_item_get_weapon_from_class(np, WEAPON_CLASS_THROWN_GRENADE);
    weapon* wp = item != nullptr ? World.object_from_handle<weapon>(item->weapon_handle) : nullptr;
    if (wp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(hp));
    if (belief == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const bool in_cover = human_combat_cover_is_using_node(np);
    if (in_cover)
    {
        ai->perception.pay_attention(static_cast<const object*>(hp), ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    }
    ai_throw_situations situation;
    const vehicle* vp = human_in_vehicle(hp) != nullptr ? World.object_from_handle<vehicle>(hp->vehicle_handle) : nullptr;
    if (vp != nullptr)
    {
        if (!in_cover)
        {
            ai->perception.pay_attention(static_cast<const object*>(vp), ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
        }
        situation = AI_THROW_SITUATION_VEHICLE;
    }
    else if (ai->planner.get_current_goal() == acquire_target_goal::get_instance())
    {
        if (!in_cover)
        {
            ai->perception.pay_attention(belief, ATTENTION_LOOK_AT_BELIEF, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
        }
        situation = AI_THROW_SITUATION_ACQUIRE_VISUAL;
    }
    else
    {
        if (!in_cover)
        {
            ai->perception.pay_attention(static_cast<const object*>(hp), ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
        }
        situation = AI_THROW_SITUATION_NORMAL;
    }
    const bool marauder_at_vehicle = np->current_team == HUMAN_TEAM_MARAUDER && human_in_vehicle(hp) != nullptr;
    const bool gutter = wp->w_info->weapon_anim_group == ANIMATION_GROUP_GUTTER;
    vector throw_velocity;
    ai_throw_types throw_type;
    if (!ai_thrown_weapon_calculate_throw(throw_velocity, np, wp, hp, situation, try_to_lead_target, &throw_type))
    {
        if (try_to_lead_target)
        {
            return AI_RESULT_IN_PROGRESS; // the next frame tries again without leading the target
        }
        if (!marauder_at_vehicle)
        {
            Global_ai_grenade_cooldown_timer.set_rand(10000, 20000);
        }
        else if (gutter)
        {
            Global_ai_grenade_cooldown_timer.set_rand(500, 1000);
        }
        else
        {
            Global_ai_grenade_cooldown_timer.set_rand(2000, 3000);
        }
        return AI_RESULT_FAILURE;
    }
    if (throw_vel_output != nullptr)
    {
        *throw_vel_output = throw_velocity;
    }
    if (!marauder_at_vehicle)
    {
        np->combat.throw_grenade_timestamp.set_rand(15000, 30000);
        Global_ai_grenade_cooldown_timer.set_rand(10000, 20000);
    }
    else if (gutter)
    {
        np->combat.throw_grenade_timestamp.set_rand(1000, 2000);
        Global_ai_grenade_cooldown_timer.set_rand(500, 1000);
    }
    else
    {
        np->combat.throw_grenade_timestamp.set_rand(3000, 4000);
        Global_ai_grenade_cooldown_timer.set_rand(2000, 3000);
    }
    human_set_combat_ready(np, false, 0);
    ai->stop_moving();
    human_weapons_start_throw(np, wp, &throw_velocity, throw_type == AI_THROW_TYPE_ROLL);
    ai->planner.lock_action(true);
    return AI_RESULT_SUCCESS;
}

// 0x005E6700 (:441)
ai_results attack_grenade_do_action_common(npc* np)
{
    return np->char_inst->is_playing_action(ANIM_ALIAS_THROW) ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x00626F20 (:466)
int attack_grenade_direct_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (human_combat_get_manned_turret(np) != nullptr || human_combat_cover_is_using_node(np))
    {
        return -1;
    }
    return use_grenade_get_cost_common(np, goal_setup_data) ? 50 : -1;
}

// 0x0069A7D0 (:496)
ai_results attack_grenade_direct_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        ai->action_substate = 1;
    }
    vector throw_velocity;
    const ai_results result = attack_grenade_enter_common(np, ai->action_substate != 0, &throw_velocity);
    ai->action_substate = 0;
    if (result == AI_RESULT_SUCCESS)
    {
        throw_velocity.flatten_normalize();
        ai->desired_orient.fvec = throw_velocity;
    }
    return result;
}

// 0x005E6720 (:523)
ai_results attack_grenade_direct_action::do_action(npc* np) const
{
    np->steering_vector = np->get_ai()->desired_orient.fvec;
    return attack_grenade_do_action_common(np);
}

// 0x00626F70 (:545)
int attack_grenade_cover_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (!human_combat_cover_is_stepped_out(np))
    {
        return -1;
    }
    return use_grenade_get_cost_common(np, goal_setup_data) ? 50 : -1;
}

// 0x0069A850 (:563)
ai_results attack_grenade_cover_action::enter(npc* np, const ai_action*) const
{
    np->steering_vector = ZERO_VECTOR;
    return attack_grenade_enter_common(np, false, nullptr);
}

// 0x005E6770 (:571)
ai_results attack_grenade_cover_action::do_action(npc* np) const
{
    return attack_grenade_do_action_common(np);
}

// 0x00678840 (:579)
int place_bomb_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    const human* target = human_combat_get_target(np);
    if (!Ai_use_grenades || np->npc_flags.disable_grenades != 0 || target == nullptr || target->is_killed() || !can_plant_bomb(np))
    {
        return -1;
    }
    const inventory_item* item = inv_item_get_weapon_from_class(np, WEAPON_CLASS_SINGULARITY_BOMB);
    weapon* wp = item != nullptr ? World.object_from_handle<weapon>(item->weapon_handle) : nullptr;
    if (wp == nullptr)
    {
        return -1;
    }
    const explosion_info* explosion = wp->w_info->m_ai_explosion_info != nullptr ? wp->w_info->m_ai_explosion_info : wp->w_info->m_explosion_info;
    if (explosion == nullptr)
    {
        return -1;
    }
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(target));
    if (belief == nullptr)
    {
        return -1;
    }
    belief_obj_iterator<rfg_mover> buildings;
    buildings.init(ai, AI_BELIEF_BUILDING);
    for (; buildings; ++buildings)
    {
        float dist_sq;
        const rfg_mover* building = buildings;
        if (human_teams_are_friendly(np, building->team) && ai_building_find_dist_squared(dist_sq, belief->pos, building, explosion->m_secondary_radius))
        {
            return -1;
        }
    }
    const vehicle* vp = human_in_vehicle(np);
    if (vp != nullptr && vehicle_passenger_in_seat(vp, VSI_DRIVER) != nullptr)
    {
        return -1;
    }
    my_data.use_target = wp;
    return 25;
}

// 0x005E67A0 (:627)
void place_bomb_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    np->get_ai()->desired_weapon = setup_data.use_target->get_handle();
    reserve_plant_bomb(np, false);
}

// 0x0069A880 (:634)
ai_results place_bomb_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(target));
    if (belief == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    weapon* wp = ai->desired_weapon != 0xFFFFFFFF ? World.object_from_handle<weapon>(ai->desired_weapon) : nullptr;
    if (wp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, false, 0);
    ai->stop_moving();
    human_set_stance(np, HST_STAND);
    ai->perception.pay_attention(belief, ATTENTION_LOOK_AT_BELIEF, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    human_weapons_start_throw(np, wp, &ZERO_VECTOR, false);
    ai->planner.lock_action(true);
    reserve_plant_bomb(np, true);
    return AI_RESULT_SUCCESS;
}

// 0x005E6830 (:673)
ai_results place_bomb_action::do_action(npc* np) const
{
    return attack_grenade_do_action_common(np);
}

// 0x005E6860 (:678)
ai_results place_bomb_action::cleanup(npc*, const ai_action*) const
{
    return AI_RESULT_SUCCESS;
}

// 0x00626FB0 (:688)
int goto_enemy_to_place_bomb_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node* next_action) const
{
    const human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return -1;
    }
    const ai_belief* belief = np->get_ai()->perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(target));
    if (belief == nullptr || next_action == nullptr)
    {
        return -1;
    }
    const weapon_info* w_info = get_weapon_from_plan(next_action)->w_info;
    const explosion_info* explosion = w_info->m_ai_explosion_info != nullptr ? w_info->m_ai_explosion_info : w_info->m_explosion_info;
    if (explosion == nullptr)
    {
        return -1;
    }
    const float reach = explosion->m_knockdown_radius * 0.8f;
    const float dist_sq = vec_dist_squared(belief->pos, np->pos);
    if (dist_sq <= reach * reach)
    {
        return 0;
    }
    return dist_sq <= 400.0f ? 25 : -1; // 20 m
}

// 0x005E6870 (:721)
void goto_enemy_to_place_bomb_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x0069A980 (:727)
ai_results goto_enemy_to_place_bomb_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        const human* target = human_combat_get_target(np);
        const ai_belief* belief = target != nullptr ? ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(target)) : nullptr;
        if (belief == nullptr)
        {
            return AI_RESULT_FAILURE;
        }
        ai->perception.pay_attention(belief, ATTENTION_LOOK_AT_BELIEF, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    }
    reserve_plant_bomb(np, false);
    human_set_combat_ready(np, true, 0);
    return AI_RESULT_SUCCESS;
}

// 0x006B6270 (:755)
ai_results goto_enemy_to_place_bomb_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(target));
    if (belief == nullptr || ai->desired_weapon == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    const weapon* wp = World.object_from_handle<weapon>(ai->desired_weapon);
    if (wp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const explosion_info* explosion = wp->w_info->m_ai_explosion_info != nullptr ? wp->w_info->m_ai_explosion_info : wp->w_info->m_explosion_info;
    if (explosion == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    reserve_plant_bomb(np, false);
    const float reach = explosion->m_knockdown_radius * 0.8f;
    const float dist_sq = vec_dist_squared(belief->pos, np->pos);
    if (dist_sq <= reach * reach)
    {
        return AI_RESULT_SUCCESS;
    }
    if (dist_sq > 400.0f) // 20 m
    {
        return AI_RESULT_FAILURE;
    }
    if (!ai->is_pathing_to(belief->pos, 0.25f, 1.0f) && ai->can_pathfind() && ai_pf_to_result(ai->pathfind_to(target, HMS_RUN, HST_CROUCH, AI_PATHFIND_PRIORITY_HIGH, &belief->pos)) == AI_RESULT_FAILURE)
    {
        ai->planner.avoid_action(this, 15000); // ms
        return AI_RESULT_FAILURE;
    }
    return AI_RESULT_IN_PROGRESS;
}

// True when another enemy position is within 5 m of belief.
static bool another_enemy_near_belief(const ai_perception& perception, const ai_belief* belief)
{
    for (const ai_belief* other = perception.get_first_belief(AI_BELIEF_ENEMY_POSITION); other != nullptr; other = perception.get_next_belief(other))
    {
        if (other != belief && vec_dist_squared(other->pos, belief->pos) < 25.0f)
        {
            return true;
        }
    }
    return false;
}

// 0x006789D0 (:809)
int grenade_kills_enemy_rule::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const ai_perception& perception = np->get_ai()->perception;
    if (np->npc_flags.disable_grenades != 0)
    {
        return -1;
    }
    const human* hp = human_combat_get_target(np);
    if (hp == nullptr)
    {
        return -1;
    }
    const ai_belief* belief = perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(hp));
    if (belief == nullptr)
    {
        return -1;
    }
    if (human_in_vehicle(hp) != nullptr && hp->vehicle_handle != 0xFFFFFFFF)
    {
        const object* objp = World.object_from_handle(hp->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            return objp->get_sub_type() == OT_SUB_VEHICLE_FLYER ? -1 : 10;
        }
    }
    vector velocity;
    if (perception.target_tracking_get_aggregate_velocity(velocity, hp, 1.5f))
    {
        vector dir;
        vec_normalized_dir(dir, hp->pos, np->pos);
        dir.flatten_normalize_safe();
        if (3.0f < velocity * dir) // m/s, running away
        {
            return 10;
        }
    }
    return another_enemy_near_belief(perception, belief) ? 10 : -1;
}

// 0x006270B0 (:875)
int bomb_kills_enemy_rule::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const ai_perception& perception = np->get_ai()->perception;
    if (np->npc_flags.disable_grenades != 0)
    {
        return -1;
    }
    const human* hp = human_combat_get_target(np);
    if (hp == nullptr)
    {
        return -1;
    }
    const ai_belief* belief = perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(hp));
    if (belief == nullptr)
    {
        return -1;
    }
    const vehicle* vp = human_in_vehicle(np);
    if (vp != nullptr && vehicle_passenger_in_seat(vp, VSI_DRIVER) != nullptr)
    {
        return -1;
    }
    return another_enemy_near_belief(perception, belief) ? 10 : -1;
}

attack_grenade_direct_action attack_grenade_direct_action::the_instance; // 0x01DC9F04

// 0x00626050 (.h:15)
attack_grenade_direct_action::attack_grenade_direct_action()
    : ai_action("attack grenade direct", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({84, 85}))
{
    flags.has_enter = 1;
}

ai_action* attack_grenade_direct_action::get_instance()
{
    return &the_instance;
}

// 0x005E65B0
attack_grenade_direct_action::~attack_grenade_direct_action()
{
}

// 0x00626330 (.h:15)
int attack_grenade_direct_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

attack_grenade_cover_action attack_grenade_cover_action::the_instance; // 0x01DA3E7C

// 0x00626370 (.h:16)
attack_grenade_cover_action::attack_grenade_cover_action()
    : ai_action("attack grenade from cover", ai_plan_conditions_from_bits({61}), ai_plan_conditions_from_bits({85}))
{
    flags.has_enter = 1;
}

ai_action* attack_grenade_cover_action::get_instance()
{
    return &the_instance;
}

// 0x005E65C0
attack_grenade_cover_action::~attack_grenade_cover_action()
{
}

// 0x006263D0 (.h:16)
int attack_grenade_cover_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

place_bomb_action place_bomb_action::the_instance; // 0x01D75574

// 0x00626410 (.h:18)
place_bomb_action::place_bomb_action()
    : ai_action("place bomb", ai_plan_conditions_from_bits({9, 47, 58, 87, 136, 137, 145, 153}), ai_plan_conditions_from_bits({86}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* place_bomb_action::get_instance()
{
    return &the_instance;
}

// 0x005E65D0
place_bomb_action::~place_bomb_action()
{
}

// 0x00626710 (.h:18)
int place_bomb_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

goto_enemy_to_place_bomb_action goto_enemy_to_place_bomb_action::the_instance; // 0x01DDB940

// 0x00626740 (.h:19)
goto_enemy_to_place_bomb_action::goto_enemy_to_place_bomb_action()
    : ai_action("place bomb goto", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({87}))
{
    flags.has_enter = 1;
}

ai_action* goto_enemy_to_place_bomb_action::get_instance()
{
    return &the_instance;
}

// 0x005E65E0
goto_enemy_to_place_bomb_action::~goto_enemy_to_place_bomb_action()
{
}

// 0x006269E0 (.h:19)
int goto_enemy_to_place_bomb_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

grenade_kills_enemy_rule grenade_kills_enemy_rule::the_instance; // 0x01DA6040

// 0x00626A10 (.h:21)
grenade_kills_enemy_rule::grenade_kills_enemy_rule()
    : ai_production_rule("grenade kills enemy rule", ai_plan_conditions_from_bits({85}), ai_plan_conditions_from_bits({78}))
{
}

ai_action* grenade_kills_enemy_rule::get_instance()
{
    return &the_instance;
}

// 0x005E65F0
grenade_kills_enemy_rule::~grenade_kills_enemy_rule()
{
}

// 0x00626A90 (.h:21)
int grenade_kills_enemy_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

bomb_kills_enemy_rule bomb_kills_enemy_rule::the_instance; // 0x01D71FA8

// 0x00626AC0 (.h:22)
bomb_kills_enemy_rule::bomb_kills_enemy_rule()
    : ai_production_rule("bomb kills enemy rule", ai_plan_conditions_from_bits({86}), ai_plan_conditions_from_bits({78}))
{
}

ai_action* bomb_kills_enemy_rule::get_instance()
{
    return &the_instance;
}

// 0x005E6600
bomb_kills_enemy_rule::~bomb_kills_enemy_rule()
{
}

// 0x00626B40 (.h:22)
int bomb_kills_enemy_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

