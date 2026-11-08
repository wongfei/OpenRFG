// Original: volition/rfg/code/ai/combat/combat_draw_reload_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/combat/combat_draw_reload_actions.h"

#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/combat/combat_maneuver_actions.h"
#include "volition/rfg/code/ai/combat/combat_melee_actions.h"
#include "volition/rfg/code/ai/combat/combat_turret_actions.h"
#include "volition/rfg/code/ai/vehicle/vehicle_ai_actions.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/util/varray.h"

#include <float.h>
#include <math.h>

// Position 10 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG10")

static const float MARAUDER_VAULT_TARGET_DIR_COS = (float)cos(0.785398006439209); // 0x01DA03EC, 45 degrees

// 0x005E4220 (:67). LTCG: np EDI.
static void maybe_play_join_player_voice_line(npc* np)
{
    if (np->npc_flags.converted_guerrilla == 0)
    {
        return;
    }
    const player* pl = player_get_local_player();
    if (pl != nullptr && human_teams_get_disposition(pl, np) == TEAM_DISPOSITION_ALLIED && vec_dist_squared(np->pos, pl->pos) < 400.0f)
    {
        np->play_voice_line(VOICE_LINE_COMBAT_ALLY_JOIN, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    }
}

// 0x00623D10 (:88)
int draw_firearm_action::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node* next_action) const
{
    static const ai_plan_conditions EXCLUDE_CONDITIONS = ai_plan_conditions_from_bits({0, 1});
    if ((np->h_info->flags.guerrilla_tech && !mission_is_active()) || np->npc_flags.prevent_weapon_draw != 0)
    {
        return -1;
    }
    if (desired_conditions & EXCLUDE_CONDITIONS)
    {
        return -1;
    }
    if (inv_item_get_best_firearm(np) == nullptr || ai_plan_contains_action(next_action, get_out_of_vehicle_action::get_instance()) || ai_plan_contains_action(next_action, release_turret_action::get_instance()))
    {
        return -1;
    }
    return 200;
}

// 0x005E4280 (:128)
void draw_firearm_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x006589E0 (:134)
ai_results draw_firearm_action::enter(npc* np, const ai_action* prev_action) const
{
    ai_system* ai = np->get_ai();
    ai->planner.lock_action(true);
    ai->perception.set_attention_mode(ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    human_set_combat_ready(np, false, 0);
    if (prev_action != this)
    {
        maybe_play_join_player_voice_line(np);
        npc_maybe_set_male_guerrilla_anim_set(np);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E4290 (:153)
ai_results draw_firearm_action::do_action(npc* np) const
{
    inventory_item* item = inv_item_get_best_firearm(np);
    if (item == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (np->desired_equipped_inv_item != item)
    {
        human_equip_inventory_item(np, item);
        return AI_RESULT_IN_PROGRESS;
    }
    return np->get_equipped_inv_item() != item ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x005E42E0 (:179)
int draw_melee_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node* next_action) const
{
    if ((np->h_info->flags.guerrilla_tech && !mission_is_active()) || inv_item_get_best_melee(np) == nullptr)
    {
        return -1;
    }
    if (ai_plan_contains_action(next_action, get_out_of_vehicle_action::get_instance()) || ai_plan_contains_action(next_action, release_turret_action::get_instance()))
    {
        return -1;
    }
    return 5000;
}

// 0x005E4360 (:202)
void draw_melee_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x00658A90 (:208)
ai_results draw_melee_action::enter(npc* np, const ai_action* prev_action) const
{
    ai_system* ai = np->get_ai();
    ai->planner.lock_action(true);
    if (ai->perception.get_attention_mode() == ATTENTION_AIM_AT_OBJECT)
    {
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    }
    human_set_combat_ready(np, false, 0);
    if (prev_action != this)
    {
        maybe_play_join_player_voice_line(np);
        npc_maybe_set_male_guerrilla_anim_set(np);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E4370 (:233)
ai_results draw_melee_action::do_action(npc* np) const
{
    inventory_item* item = inv_item_get_best_melee(np);
    if (item == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (np->desired_equipped_inv_item != item)
    {
        human_equip_inventory_item(np, item);
        return AI_RESULT_IN_PROGRESS;
    }
    return np->get_equipped_inv_item() != item ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x005E43C0 (:259)
int holster_weapon_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 50;
}

// 0x00658B40 (:264)
ai_results holster_weapon_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    ai->planner.lock_action(true);
    if (ai->perception.get_attention_mode() == ATTENTION_AIM_AT_OBJECT)
    {
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    }
    human_set_combat_ready(np, false, 0);
    return AI_RESULT_SUCCESS;
}

// 0x005E43D0 (:281)
ai_results holster_weapon_action::do_action(npc* np) const
{
    if (np->desired_equipped_inv_item != nullptr)
    {
        human_unequip_inventory_item(np);
        return AI_RESULT_IN_PROGRESS;
    }
    return np->get_equipped_inv_item() == nullptr && !np->char_inst->is_playing_action(ANIM_ALIAS_EQUIP) ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

// 0x005E4420 (:302)
int drop_weapon_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 100000;
}

// 0x005E4430 (:307)
ai_results drop_weapon_action::do_action(npc* np) const
{
    human_weapons_immediately_holster_weapon(np);
    human_weapons_drop_equipped_weapon(np);
    return AI_RESULT_SUCCESS;
}

// 0x005E4450 (:318)
int reload_weapon_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 4000;
}

// One attack the weapon can do at this distance goes on the list; the nearest one that is not in reach yet is remembered.
static void consider_melee_move(float dist, const human_melee_combat_move_data* move, base_array<const human_melee_combat_move_data*>& in_reach, float& best_min, float& best_max, const human_melee_combat_move_data*& best)
{
    if (move == nullptr || dist > move->ai_max_dist)
    {
        return;
    }
    if (dist < move->ai_min_dist)
    {
        if (move->ai_min_dist < best_min || (move->ai_min_dist == best_min && move->ai_max_dist < best_max))
        {
            best_min = move->ai_min_dist;
            best_max = move->ai_max_dist;
            best = move;
        }
    }
    else
    {
        in_reach.add(move);
    }
}

// Inlined twice below: a melee attack worth stopping for, within 1.2 m of height. Marauders with a gutter also vault at a
// target running away from them on the mesh.
static const human_melee_combat_move_data* pick_melee_move_at_target(npc* np, const ai_combat_action_parameters& params)
{
    const weapon_info* info = human_weapons_get_weapon_info(np);
    if (info == nullptr)
    {
        return nullptr;
    }
    const human* target = params.target;
    const human* human_target = nullptr;
    float dist;
    if (target == nullptr || target->get_obj_type() != OT_HUMAN)
    {
        vector local_pos = np->pos;
        // Original: target is used without a null check on this path.
        vec_change_to_local_coords(local_pos, target->pos, target->orient);
        vector bmin;
        vector bmax;
        target->get_local_bbox(bmin, bmax);
        float height = local_pos.y - bmax.y;
        if (height - (bmin.y - local_pos.y) < 0.0f)
        {
            height = bmin.y - local_pos.y;
        }
        if (height > 1.2f)
        {
            return nullptr;
        }
        dist = sqrtf(bbox_find_xz_distance_squared(bmin, bmax, local_pos));
    }
    else
    {
        if (fabsf(target->pos.y - np->pos.y) > 1.2f)
        {
            return nullptr;
        }
        dist = vec_xz_dist(np->pos, target->pos);
        human_target = target;
    }

    farray<const human_melee_combat_move_data*, 10> in_reach;
    float best_min = FLT_MAX;
    float best_max = FLT_MAX;
    const human_melee_combat_move_data* best = nullptr;
    if (info->standing_primary_melee_attack != INVALID_MELEE_COMBAT_MOVE_ID)
    {
        consider_melee_move(dist, human_combat_move_data_from_index(np, info->standing_primary_melee_attack), in_reach, best_min, best_max, best);
    }
    if (info->standing_secondary_melee_attack != INVALID_MELEE_COMBAT_MOVE_ID)
    {
        consider_melee_move(dist, human_combat_move_data_from_index(np, info->standing_secondary_melee_attack), in_reach, best_min, best_max, best);
    }
    if (info->standing_tertiary_melee_attack != INVALID_MELEE_COMBAT_MOVE_ID)
    {
        consider_melee_move(dist, human_combat_move_data_from_index(np, info->standing_tertiary_melee_attack), in_reach, best_min, best_max, best);
    }
    if (np->current_team == HUMAN_TEAM_MARAUDER && info->weapon_class == WEAPON_CLASS_GUTTER)
    {
        static const human_melee_combat_move_id vault_attack_move_id = human_combat_lookup_combat_move("vault_attack");
        if (vault_attack_move_id != INVALID_MELEE_COMBAT_MOVE_ID && human_target != nullptr)
        {
            const ai_system* ai = np->get_ai();
            const nav_cell* ncp = nav_cell_from_handle(np->pf.last_visited_nav_cell_handle);
            float height;
            if (ncp != nullptr && (ncp->flags & NAV_CELL_FLAG_DISABLED) == 0 && nav_cell_is_point_in_cell_column(ncp, np->pos) && havok_terrain_height(height, np->pos.x, np->pos.z, false, nullptr) && np->pos.y <= height + 1.25f &&
                ai->perception.is_on_navmesh(human_target, nullptr, 1.25f) && human_target->actual_velocity_mag > 2.0f)
            {
                vector target_dir = human_target->velocity;
                target_dir.flatten_normalize_safe();
                vector my_dir = np->orient.fvec;
                my_dir.flatten_normalize();
                if (MARAUDER_VAULT_TARGET_DIR_COS < my_dir * target_dir)
                {
                    consider_melee_move(dist, human_combat_move_data_from_index(np, vault_attack_move_id), in_reach, best_min, best_max, best);
                }
            }
        }
    }
    if (in_reach.size() == 1)
    {
        return in_reach[0];
    }
    if (in_reach.size() != 0)
    {
        return in_reach[int_rand_range(0, in_reach.size() - 1)];
    }
    return best;
}

// 0x00694F70 (:324)
ai_results reload_weapon_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    ai_combat_action_parameters params;
    params.init_npc(np);
    if (params.weapon_inst == nullptr || !params.weapon_inst->w_info->is_firearm())
    {
        return AI_RESULT_FAILURE;
    }
    ai->planner.lock_action(true);
    params.weapon_inst->cancel_reload();

    bool stop = true;
    if (np->current_team == HUMAN_TEAM_MARAUDER)
    {
        // Marauders running at their target reload on the move, unless a melee attack is at hand.
        if (params.init_target(true) && pick_melee_move_at_target(np, params) == nullptr)
        {
            if ((int)np->cur_movement_state < 1)
            {
                stop = false;
            }
            else
            {
                const vector target_dir = params.target_dir.copy_flatten_normalize_safe();
                if (np->steering_vector * target_dir >= 0.8660254f) // cos 30 degrees
                {
                    stop = false;
                }
            }
        }
    }
    else if (last_action == advance_action::get_instance() || last_action == partial_advance_action::get_instance())
    {
        stop = false;
    }
    if (stop)
    {
        ai->stop_moving();
        const human* target = human_combat_get_target(np);
        if (target != nullptr)
        {
            vector to_target;
            to_target.x = target->pos.x - np->pos.x;
            to_target.y = target->pos.y - np->pos.y;
            to_target.z = target->pos.z - np->pos.z;
            np->steering_vector = to_target.copy_flatten_normalize_safe();
        }
        ai->perception.set_attention_mode(ATTENTION_USE_CURRENT_MODE, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    }
    if (ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION) != nullptr)
    {
        human_set_combat_ready(np, true, 0);
    }
    return AI_RESULT_SUCCESS;
}

// 0x00695C80 (:384)
ai_results reload_weapon_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION) != nullptr)
    {
        human_set_combat_ready(np, true, 0);
    }
    if (np->current_team == HUMAN_TEAM_MARAUDER && np->cur_movement_state != HMS_IDLE)
    {
        ai_combat_action_parameters params;
        params.init_npc(np);
        if (params.init_target(true) && pick_melee_move_at_target(np, params) != nullptr)
        {
            ai->stop_moving();
            ai->perception.set_attention_mode(ATTENTION_USE_CURRENT_MODE, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
        }
    }
    return ai_combat_do_reload(np) ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

// 0x005E4460 (:412)
ai_results reload_weapon_action::cleanup(npc*, const ai_action*) const
{
    return AI_RESULT_SUCCESS;
}

// The singletons, in rfg.exe's construction order: they fill action_pool, and that order breaks ties in the planner.
draw_firearm_action draw_firearm_action::the_instance; // 0x01DA60DC

// 0x0061FEA0 (.h:15)
draw_firearm_action::draw_firearm_action()
    : ai_action("draw firearm", ai_plan_conditions_from_bits({9, 47, 137, 145}), ai_plan_conditions_from_bits({2}))
{
    flags.has_enter = 1;
}

ai_action* draw_firearm_action::get_instance()
{
    return &the_instance;
}

// 0x005E3ED0
draw_firearm_action::~draw_firearm_action()
{
}

// 0x00620000 (.h:15)
int draw_firearm_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

draw_melee_action draw_melee_action::the_instance; // 0x01DC9B34

// 0x00620030 (.h:16)
draw_melee_action::draw_melee_action()
    : ai_action("draw melee weapon", ai_plan_conditions_from_bits({9, 47, 136, 137, 145, 153}), ai_plan_conditions_from_bits({3}))
{
    flags.has_enter = 1;
}

ai_action* draw_melee_action::get_instance()
{
    return &the_instance;
}

// 0x005E3EE0
draw_melee_action::~draw_melee_action()
{
}

// 0x00620260 (.h:16)
int draw_melee_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

holster_weapon_action holster_weapon_action::the_instance; // 0x01DDB088

// 0x00620290 (.h:17)
holster_weapon_action::holster_weapon_action()
    : ai_action("holster weapon", ai_plan_conditions(), ai_plan_conditions_from_bits({7}))
{
    flags.has_enter = 1;
}

ai_action* holster_weapon_action::get_instance()
{
    return &the_instance;
}

// 0x005E3EF0
holster_weapon_action::~holster_weapon_action()
{
}

// 0x006202E0 (.h:17)
int holster_weapon_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

drop_weapon_action drop_weapon_action::the_instance; // 0x01D68714

// 0x00620320 (.h:18)
drop_weapon_action::drop_weapon_action()
    : ai_action("drop weapon", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({6}))
{
}

ai_action* drop_weapon_action::get_instance()
{
    return &the_instance;
}

// 0x005E3F00
drop_weapon_action::~drop_weapon_action()
{
}

// 0x006205C0 (.h:18)
int drop_weapon_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

reload_weapon_action reload_weapon_action::the_instance; // 0x01DCA218

// 0x00620600 (.h:20)
reload_weapon_action::reload_weapon_action()
    : ai_action("reload weapon", ai_plan_conditions_from_bits({2, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({0, 1}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* reload_weapon_action::get_instance()
{
    return &the_instance;
}

// 0x005E3F10
reload_weapon_action::~reload_weapon_action()
{
}

// 0x00620940 (.h:20)
int reload_weapon_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

firearm_effective_rule firearm_effective_rule::the_instance; // 0x01D7505C

// 0x00620980 (.h:24)
firearm_effective_rule::firearm_effective_rule()
    : ai_production_rule("firearm effective rule", ai_plan_conditions_from_bits({0, 2}), ai_plan_conditions_from_bits({4}))
{
}

ai_action* firearm_effective_rule::get_instance()
{
    return &the_instance;
}

// 0x005E3F20
firearm_effective_rule::~firearm_effective_rule()
{
}

melee_effective_rule melee_effective_rule::the_instance; // 0x01DC9FE8

// 0x00620A80 (.h:25)
melee_effective_rule::melee_effective_rule()
    : ai_production_rule("melee effective rule", ai_plan_conditions_from_bits({3}), ai_plan_conditions_from_bits({4}))
{
}

ai_action* melee_effective_rule::get_instance()
{
    return &the_instance;
}

// 0x005E3F30
melee_effective_rule::~melee_effective_rule()
{
}

