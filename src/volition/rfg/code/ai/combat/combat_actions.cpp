// Original: volition/rfg/code/ai/combat/combat_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/combat/combat_actions.h"

#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/combat/combat_maneuver.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/rand.h"

// Position 8 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG08")

// Set from the tweak table, like Civilian_avoid_player_thrown_weapon_dist in hazard_detection.cpp.
static int Ai_voice_morale_boost_chance; // 0x01D65974
static tweak_table_item Tweak_table_item_Ai_voice_morale_boost_chance("Ai_voice_morale_boost_chance", &Ai_voice_morale_boost_chance);
static int Ai_voice_team_specific_chance; // 0x01D67750
static tweak_table_item Tweak_table_item_Ai_voice_team_specific_chance("Ai_voice_team_specific_chance", &Ai_voice_team_specific_chance);

static bool Combat_maneuver_do_player_line_of_fire_checks = true; // 0x016413E2, .data 1, never written (also in goals.cpp)

// 0x006572B0 (:108). LTCG: np EDI. Steps aside when the muzzle, not the eyes, has the line of fire blocked.
bool attack_get_sidestep_adjustment(vector& move_dir, const npc* np, const weapon* wp)
{
    const ai_system* ai = np->get_ai();
    if (ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) == nullptr)
    {
        return false;
    }
    const timestamp& decent_shot = ai->perception.attention.last_decent_shot_time;
    const int since_decent_shot = decent_shot.valid() ? decent_shot.time_since() : -1;
    if (np->npc_flags.maneuver_required == 0 && since_decent_shot >= 0 && since_decent_shot <= 1000)
    {
        return false;
    }
    vector eye_pos;
    human_get_eye_pos_orient(np, eye_pos, nullptr);
    vector muzzle_pos;
    wp->get_muzzle_transform(muzzle_pos, nullptr);
    vector offset;
    offset.x = muzzle_pos.x - eye_pos.x;
    offset.y = muzzle_pos.y - eye_pos.y;
    offset.z = muzzle_pos.z - eye_pos.z;
    vec_change_to_local_coords(offset, ZERO_VECTOR, np->orient);
    if (offset.x == 0.0f)
    {
        return false;
    }
    if (offset.x >= 0.0f)
    {
        move_dir.x = -np->orient.rvec.x;
        move_dir.y = -np->orient.rvec.y;
        move_dir.z = -np->orient.rvec.z;
    }
    else
    {
        move_dir = np->orient.rvec;
    }
    return true;
}

// 0x00693680 (:149)
static bool attack_adjust_position(const ai_combat_action_parameters& params)
{
    npc* np = params.np;
    ai_system* ai = np->get_ai();
    const human_movement_states m_state = np->cur_movement_state;
    vector move_dir;
    if (!attack_get_sidestep_adjustment(move_dir, np, params.weapon_inst))
    {
        if (np->current_team == HUMAN_TEAM_MARAUDER || !combat_maneuver_get_spacing_move_dir(move_dir, np, params.target) || move_dir.flatten_mag_and_normalize_safe() < 2.0f)
        {
            return false;
        }
    }
    np->npc_flags.maneuver_required = 0;
    if (m_state != HMS_IDLE && np->steering_vector * move_dir < 0.25881904f) // cos 75 degrees
    {
        return false;
    }
    const float dist = human_get_move_speed_info(np, HMS_WALK)->average_speed * 1.5f;
    vector dest;
    dest.x = np->pos.x + move_dir.x * dist;
    dest.y = np->pos.y + move_dir.y * dist;
    dest.z = np->pos.z + move_dir.z * dist;
    vector new_pos;
    if (combat_maneuver_is_path_obstructed(np, dest, &new_pos) || new_pos == ZERO_VECTOR || ai->outside_defensive_perimeter(new_pos, false, nullptr))
    {
        return false;
    }
    if (!ai->perception.firing_los_clear(params.target, HST_CURRENT, &new_pos))
    {
        return false;
    }
    ai->set_direct_movement_fast(move_dir, HMS_WALK, HST_CURRENT, -1.0f);
    ai->action_pos = new_pos;
    return true;
}

// 0x00693A80 (:224)
human_stances get_attack_stance(const ai_combat_action_parameters& params)
{
    const npc* np = params.np;
    const ai_system* ai = np->get_ai();
    if (np->current_team == HUMAN_TEAM_MARAUDER)
    {
        return HST_STAND;
    }
    if (np->cur_stance == HST_PRONE)
    {
        return HST_CROUCH;
    }
    if ((ai->perception.is_in_closest_building(np, nullptr, 0.0f, 1.25f) && human_in_flyer(params.target)) || !ai->perception.firing_los_clear(params.target, HST_CROUCH, nullptr))
    {
        return HST_STAND;
    }
    const bool in_ally_fire_elapsed = ai->perception.in_ally_line_of_fire_time.elapsed();
    const bool ally_in_fire_elapsed = ai->perception.ally_in_line_of_fire_time.elapsed();
    if (in_ally_fire_elapsed)
    {
        if (!ally_in_fire_elapsed)
        {
            return HST_STAND;
        }
    }
    else if (ally_in_fire_elapsed)
    {
        return HST_CROUCH;
    }
    const weapon_info* info = human_weapons_get_weapon_info(np);
    if (info == nullptr)
    {
        return HST_STAND;
    }
    float near_dist = (info->max_engagement_dist - info->min_engagement_dist) * 0.4f + info->min_engagement_dist;
    float far_dist = (info->max_engagement_dist - info->min_engagement_dist) * 0.6f + info->min_engagement_dist;
    if (np->has_personal_shield())
    {
        near_dist *= 0.6666667f;
        far_dist *= 0.6666667f;
    }
    if (params.dist_to_target < near_dist)
    {
        return HST_STAND;
    }
    return params.dist_to_target <= far_dist ? np->cur_stance : HST_CROUCH;
}

// 0x00617250 (:289)
void combat_position_adjust_enter(npc* np)
{
    ai_system* ai = np->get_ai();
    int delay = int_rand_range(1000, 1500);
    if (np->combat.combat_maneuver_timestamp.valid())
    {
        const int remaining = np->combat.combat_maneuver_timestamp.time_until();
        if (delay <= remaining)
        {
            delay = remaining;
        }
    }
    ai->action_timer.set(delay);
    ai->stop_moving();
    ai->action_pos = np->pos;
}

// 0x00693C40 (:312)
void combat_position_adjust_do_frame(const ai_combat_action_parameters& params, bool allow_stance_changes)
{
    npc* np = params.np;
    ai_system* ai = np->get_ai();
    if (allow_stance_changes && ai->stance_change_timer.elapsed())
    {
        human_set_stance(np, get_attack_stance(params));
        ai->stance_change_timer.set_rand(3000, 5000);
    }
    if (ai->action_timer.elapsed())
    {
        if (!attack_adjust_position(params))
        {
            ai->stop_moving();
            ai->action_pos = np->pos;
        }
        ai->action_timer.set_rand(1000, 1500);
    }
    if (np->cur_movement_state != HMS_IDLE)
    {
        vector to_pos;
        to_pos.x = ai->action_pos.x - np->pos.x;
        to_pos.y = ai->action_pos.y - np->pos.y;
        to_pos.z = ai->action_pos.z - np->pos.z;
        if (np->steering_vector * to_pos < 0.0f) // past it
        {
            ai->stop_moving();
            return;
        }
        np->combat.combat_maneuver_timestamp.set(3000);
    }
}

// 0x00657440 (:359)
int attack_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const human* target = human_combat_get_target(np);
    const weapon* wp = ai_combat_get_action_firearm(np);
    if (target == nullptr || wp == nullptr)
    {
        return -1;
    }
    if (wp->get_range_category(vec_dist(np->pos, target->pos)) == WEAPON_OUT_OF_RANGE && np->combat.no_path_to_target_timestamp.elapsed())
    {
        return -1;
    }
    return 500;
}

// 0x00693DB0 (:382)
ai_results attack_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    if (last_action != this)
    {
        ai->action_timer.set(0);
        np->combat.shoot_enemy_timestamp.invalidate();
        np->combat.hold_trigger_timestamp.invalidate();
        combat_position_adjust_enter(np);

        voice_lines line = VOICE_LINE_NONE;
        if (ai->perception.get_belief(AI_BELIEF_CLOSEST_ALLY) != nullptr && !((unsigned int)Ai_voice_morale_boost_chance < vrand() % 100))
        {
            line = VOICE_LINE_COMBAT_BOOST_TEAM_MORALE;
        }
        else
        {
            const vehicle* vp = World.object_from_handle<vehicle>(target->vehicle_handle);
            if (vp != nullptr)
            {
                if (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER)
                {
                    line = VOICE_LINE_COMBAT_TAUNT_ENEMY_WALKER;
                }
                else if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO)
                {
                    // Bool * 2 + the line: _ENEMY_VEHICLE, or _ENEMY_VEHICLE_HEAVY two lines on.
                    line = (voice_lines)((ai_perception::vehicle_is_heavy(vp) ? 2 : 0) + VOICE_LINE_COMBAT_TAUNT_ENEMY_VEHICLE);
                }
            }
            else if ((unsigned int)Ai_voice_team_specific_chance < vrand() % 100)
            {
                if (human_teams_get_disposition(target, HUMAN_TEAM_EDF) == TEAM_DISPOSITION_ALLIED)
                {
                    line = VOICE_LINE_COMBAT_TAUNT_EDF;
                }
                else
                {
                    line = (voice_lines)((human_teams_are_allies(target, HUMAN_TEAM_MARAUDER) ? 1 : 0) + VOICE_LINE_COMBAT_TAUNT_GUERILLA);
                }
            }
            else
            {
                line = VOICE_LINE_COMBAT_TAUNT_ENEMY;
            }
        }
        if (line != VOICE_LINE_NONE)
        {
            np->play_voice_line(line, match_goal_callback, const_cast<ai_goal*>(ai->planner.get_current_goal()), AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        }
    }
    if (Combat_maneuver_do_player_line_of_fire_checks && ai->perception.is_in_friendly_player_line_of_fire())
    {
        human_set_stance(np, HST_CROUCH);
    }
    return AI_RESULT_SUCCESS;
}

// 0x006B6100 (:447)
ai_results attack_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    ai_combat_action_parameters params;
    params.init_npc(np);
    const weapon* wp = params.weapon_inst;
    if (wp == nullptr || (wp->rounds_in_magazine < 1 && (wp->rounds_in_reserve < 1 || wp->w_info->ammo_type != WEAPON_AMMO_TYPE_THROWN) && !wp->has_unlimited_magazine_ammo()))
    {
        return AI_RESULT_FAILURE;
    }
    if (!wp->w_info->is_firearm() || !params.init_target(true) || (wp->get_range_category(params.dist_to_target) == WEAPON_OUT_OF_RANGE && np->combat.no_path_to_target_timestamp.elapsed()))
    {
        return AI_RESULT_FAILURE;
    }
    // Within 3 m once the timer ran out: time for melee.
    if (ai->action_timer.elapsed() && params.dist_to_target < 3.0f)
    {
        ai->action_timer.set(500);
        np->combat.move_to_melee_timestamp.invalidate();
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    if (ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) == nullptr && !ai->perception.can_hit_target())
    {
        return AI_RESULT_FAILURE;
    }
    ai_combat_fire_at_target(params, true);
    combat_position_adjust_do_frame(params, true);
    return AI_RESULT_IN_PROGRESS;
}

// 0x006172C0 (:489)
ai_results attack_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this && np->char_inst->rdd.is_loaded() && np->char_inst->is_playing_action(ANIM_ALIAS_FIRE))
    {
        np->char_inst->stop_action(ANIM_ALIAS_FIRE, 5, false);
    }
    return AI_RESULT_SUCCESS;
}

// The singletons, in rfg.exe's construction order: they fill action_pool, and that order breaks ties in the planner.
attack_action attack_action::the_instance; // 0x01D7253C

// 0x00616EC0 (.h:18)
attack_action::attack_action()
    : ai_action("attack", ai_plan_conditions_from_bits({0, 2, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({78}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* attack_action::get_instance()
{
    return &the_instance;
}

// 0x005E2AD0
attack_action::~attack_action()
{
}

// 0x00617210 (.h:18)
int attack_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

