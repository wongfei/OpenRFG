// Original: volition/rfg/code/ai/reaction_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt. Reaction reasons are bit numbers as well (ai_reaction_info::reason).

#include "volition/rfg/code/ai/reaction_actions.h"

#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/alerts.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/self_preservation_actions.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_raid.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"

// Position 27 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG27")

extern timestamp Taunt_enemy_timestamps[4]; // goals.cpp

// Reaction reasons of the ai_system (numbers from rfg.exe; their names are not recovered, these say what reads them).
static const unsigned int REACT_REASON_POINT_OUT_OR_BATTLE_CRY = 0x1;
static const unsigned int REACT_REASON_ENEMY = 0x43; // the mask react_to_enemy_action asks for
static const unsigned int REACT_REASON_DAMAGE = 0x2;
static const unsigned int REACT_REASON_WATCH = 0x4;
static const unsigned int REACT_REASON_WATCH_ON_TURRET = 0x6;
static const unsigned int REACT_REASON_COWER = 0x8;
static const unsigned int REACT_REASON_PROJECTILE = 0x10;
static const unsigned int REACT_REASON_DROVE_OFF_CIVILIAN = 0x20;
static const unsigned int REACT_REASON_TAUNT = 0x80;
static const unsigned int REACT_REASON_KILLED_ENEMY = 0x100;
static const unsigned int REACT_REASON_COMMANDEERED = 0x200;
static const unsigned int REACT_REASON_BOMB = 0x400;
static const unsigned int REACT_REASON_SALUTE = 0x800;

static const int ANIM_STATE_CATCH_BREATH = 5; // numeric index in rfg.exe, name unknown
static const int ACKNOWLEDGED_ENEMY_MS = 15000;

// 0x005F07A0 (:134)
int catch_breath_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (human_in_vehicle(np) != nullptr)
    {
        return -1;
    }
    if (np->current_team != HUMAN_TEAM_CIVILIAN && np->current_team != HUMAN_TEAM_GUERILLA && inv_item_get_best_weapon(np, false) != nullptr)
    {
        return -1;
    }
    const ai_system* ai = np->get_ai();
    return ai->exhaustion_timer.valid() && !ai->exhaustion_timer.elapsed() ? 5600 : -1;
}

// 0x005F0810 (:160)
int raid_reaction_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    return np->npc_flags.raid_cheer == 0 && np->npc_flags.raid_watch == 0 ? -1 : 2200;
}

// 0x005F0830 (:173)
int taunt_reaction_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    return human_in_vehicle(np) == nullptr && np->get_ai()->should_react(REACT_REASON_TAUNT) ? 700 : -1;
}

// 0x005F0870 (:192)
int salute_reaction_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    return human_in_vehicle(np) == nullptr && np->get_ai()->should_react(REACT_REASON_SALUTE) ? 325 : -1;
}

// 0x00640740 (:208)
bool salute_reaction_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_system* ai = np->get_ai();
    if (human_in_vehicle(np) != nullptr || !ai->should_react(REACT_REASON_SALUTE) || ai->reaction_info.target_handle == 0xFFFFFFFF)
    {
        return false;
    }
    object* objp = World.object_from_handle(ai->reaction_info.target_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC || objp->is_killed())
    {
        return false;
    }
    goal_setup_data.selected_target = objp;
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_OBJECT;
    return true;
}

// 0x006A8150 (:231)
void salute_reaction_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (goal_setup_data.selected_target != nullptr)
    {
        goal_setup_data.set_attention(np, nullptr);
    }
}

// 0x005F08C0 (:240)
int react_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const ai_system* ai = np->get_ai();
    return !ai->reaction_info.time_left.elapsed() && ai->reaction_info.reason != 0 ? 50000 : -1;
}

// 0x005F0900 (:250)
bool react_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_system* ai = np->get_ai();
    if (ai->reaction_info.time_left.elapsed() || ai->reaction_info.reason == 0)
    {
        return false;
    }
    goal_setup_data.selected_target = World.object_from_handle(ai->reaction_info.target_handle);
    goal_setup_data.attention_belief_type = ai->reaction_info.belief_type;
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_NONE;
    return goal_setup_data.attention_belief_type != AI_BELIEF_UNDEFINED || goal_setup_data.selected_target != nullptr;
}

// 0x006A8170 (:273)
void react_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    goal_setup_data.set_attention(np, nullptr);
}

// 0x005F0960 (:282)
int catch_breath_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->cur_stance != HST_PRONE ? 100 : -1;
}

// 0x006407D0 (:292)
ai_results catch_breath_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (this == last_action)
    {
        return AI_RESULT_SUCCESS;
    }
    ai->perception.set_attention_mode(ATTENTION_LOOK_AT_BELIEF, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_OBLIVIOUS);
    ai->stop_moving();
    vector dir = -np->orient.fvec;
    const ai_belief* belief = ai->perception.get_attention_belief();
    if (belief != nullptr)
    {
        dir = belief->pos - np->pos;
        dir.flatten_normalize_safe();
    }
    human_override_default_animation_state(np, true);
    if (np->orient.rvec * dir <= 0.0f)
    {
        np->char_inst->set_state(index_alias(ANIM_STATE_CATCH_BREATH, (vanim_aliases)-1), index_alias(ANIMATION_ACTION_STAND_SHUFFLE_LEFT, ANIM_ALIAS_TURN_LEFT), 0.3f, false);
    }
    else
    {
        np->char_inst->set_state(index_alias(ANIM_STATE_CATCH_BREATH, (vanim_aliases)-1), index_alias(ANIMATION_ACTION_STAND_SHUFFLE_RIGHT, ANIM_ALIAS_TURN_RIGHT), 0.3f, false);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F0980 (:334)
ai_results catch_breath_action::do_action(npc* np) const
{
    return np->get_ai()->exhaustion_timer.elapsed() ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

// 0x005F09B0 (:341)
ai_results catch_breath_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        human_override_default_animation_state(np, false);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F09D0 (:353)
int catch_breath_prone_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->cur_stance != HST_PRONE ? -1 : 100;
}

// 0x006409D0 (:363)
ai_results catch_breath_prone_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (this != last_action)
    {
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_BELIEF, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_OBLIVIOUS);
        ai->stop_moving();
        ai->planner.lock_action(true);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F09F0 (:382)
ai_results catch_breath_prone_action::do_action(npc* np) const
{
    return np->get_ai()->exhaustion_timer.elapsed() ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

// 0x005F0A20 (:392)
int react_to_enemy_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if (activity_is_active(ACTIVITY_MODE_RAID_DEFEND) || activity_is_active(ACTIVITY_MODE_RAID_ATTACK) || human_in_vehicle(np) != nullptr || human_combat_get_manned_turret(np) != nullptr)
    {
        return -1;
    }
    return np->get_ai()->should_react(REACT_REASON_ENEMY) ? 2000 : -1;
}

// 0x00640A60 (:414)
ai_results react_to_enemy_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (this != last_action)
    {
        ai->stop_moving();
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        ai->action_timer.set_rand(1000, 1600);
        ai->action_substate = 0;
        ai->planner.lock_action(true);
    }
    return AI_RESULT_SUCCESS;
}

// 0x00640B20 (:433)
ai_results react_to_enemy_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (ai->action_substate == 0 && ai->action_timer.time_until() < 701 && ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr) // ms
    {
        ai->action_substate = 1;
        human_set_combat_ready(np, true, 700);
        ai_combat_action_parameters params;
        params.init_npc(np);
        if (params.init_target(true) && params.weapon_inst != nullptr && ai_combat_fire_check_range(params))
        {
            ai->perception.set_attention_mode(ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        }
    }
    return ai->action_timer.elapsed() ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

// 0x005F0A90 (:452)
ai_results react_to_enemy_action::cleanup(npc* np, const ai_action*) const
{
    np->get_ai()->reaction_info.reset();
    return AI_RESULT_SUCCESS;
}

// 0x005F0AE0 (:462)
int surprised_by_projectile_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const ai_system* ai = np->get_ai();
    if (human_in_vehicle(np) != nullptr || !ai->should_react(REACT_REASON_PROJECTILE))
    {
        return -1;
    }
    if (human_combat_cover_get_node(np) == nullptr || human_combat_cover_is_using_node(np))
    {
        return 2500;
    }
    // On the way to cover: only when the run has only just begun.
    if (ai->planner.get_current_action() != flee_to_defensive_cover_action::get_instance() || ai->planner.get_time_in_current_action() > 250)
    {
        return -1;
    }
    return 2500;
}

// 0x00640C50 (:489)
ai_results surprised_by_projectile_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (this != last_action)
    {
        if (ai_combat_should_charge_target(np) == AI_COMBAT_SHOULD_NOT_CHARGE || np->cur_movement_state != HMS_RUN)
        {
            ai->stop_moving();
        }
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_BELIEF, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        ai->planner.lock_action(true);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F0B70 (:515)
ai_results surprised_by_projectile_action::do_action(npc* np) const
{
    if (np->combat.combat_ready_timestamp.valid() && !np->combat.combat_ready_timestamp.elapsed())
    {
        human_set_combat_ready(np, true, 0);
    }
    return np->get_ai()->should_react(REACT_REASON_PROJECTILE) ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x005F0BF0 (:527)
ai_results surprised_by_projectile_action::cleanup(npc* np, const ai_action*) const
{
    np->get_ai()->reaction_info.reset();
    return AI_RESULT_SUCCESS;
}

// 0x00683B90 (:537)
int react_killed_enemy_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const ai_system* ai = np->get_ai();
    if (np->current_team != HUMAN_TEAM_MARAUDER || human_is_driving(np) || !ai->should_react(REACT_REASON_KILLED_ENEMY) || !np->voice->can_play_line(VOICE_LINE_BATTLE_CRY, 0, nullptr))
    {
        return -1;
    }
    const ai_belief* ally = ai->perception.get_first_belief(AI_BELIEF_CLOSEST_ALLY);
    if (ally == nullptr)
    {
        return -1;
    }
    const human* hp = World.object_from_handle<human>(ally->handle);
    if (human_is_dead(hp))
    {
        return -1;
    }
    vector head_pos;
    human_get_head_pos(hp, head_pos);
    return ai->perception.has_los_to_position(head_pos, false) ? 500 : -1;
}

// 0x00683C50 (:583)
ai_results react_killed_enemy_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (this != last_action)
    {
        const object* target = ai->perception.get_attention_target();
        if (target == nullptr || target->get_obj_type() != OT_HUMAN)
        {
            return AI_RESULT_FAILURE;
        }
        ai->planner.lock_action(true);
        ai->stop_moving();
        ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
        if (np->get_current_voice_line() != VOICE_LINE_BATTLE_CRY)
        {
            np->play_voice_line(VOICE_LINE_BATTLE_CRY, match_goal_callback, const_cast<ai_goal*>(ai->planner.get_current_goal()), AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        }
        if (human_in_vehicle(np) == nullptr && human_combat_get_manned_turret(np) == nullptr)
        {
            np->char_inst->play_action(ANIMATION_ACTION_BATTLE_CRY, ANIM_ALIAS_NONE, 1.0f, 0);
        }
    }
    human_set_combat_ready(np, true, 0);
    return AI_RESULT_SUCCESS;
}

// 0x005F0C40 (:617)
ai_results react_killed_enemy_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human_set_combat_ready(np, true, 0);
    const ai_belief* damage = ai->perception.get_most_recent_belief(AI_BELIEF_DAMAGE_TAKEN, BELIEF_REQUIREMENTS_NONE, OT_UNDEFINED);
    if (damage != nullptr && damage->update_time.time_since() < ai->planner.get_time_in_current_action())
    {
        np->char_inst->stop_action(ANIMATION_ACTION_BATTLE_CRY, 10, true); // hit while crying
    }
    if (np->char_inst->is_playing_action(ANIMATION_ACTION_BATTLE_CRY) || np->get_current_voice_line() == VOICE_LINE_BATTLE_CRY)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    ai->reaction_info.reset();
    return AI_RESULT_SUCCESS;
}

// 0x00683D40 (:642)
int battle_cry_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    if (np->current_team != HUMAN_TEAM_MARAUDER || human_is_driving(np) || !ai->should_react(REACT_REASON_POINT_OUT_OR_BATTLE_CRY))
    {
        return -1;
    }
    const ai_belief* ally = ai->perception.get_first_belief(AI_BELIEF_CLOSEST_ALLY);
    if (ally == nullptr || ally->handle == 0xFFFFFFFF)
    {
        return -1;
    }
    const object* ally_obj = World.object_from_handle(ally->handle);
    if (ally_obj == nullptr || ally_obj->get_obj_type() != OT_HUMAN || ally_obj->is_killed())
    {
        return -1;
    }
    object* selected = goal_setup_data.selected_target;
    if (selected == nullptr || selected->get_obj_type() != OT_HUMAN || static_cast<const human*>(selected)->has_been_acknowledged() || vec_dist_squared(np->pos, selected->pos) < 49.0f) // 7 m
    {
        return -1;
    }
    my_data.attack_target = selected;
    vector head_pos;
    human_get_head_pos(static_cast<const human*>(ally_obj), head_pos);
    return ai->perception.has_los_to_position(head_pos, false) ? 900 : -1;
}

// 0x00640CF0 (:693)
void battle_cry_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    static_cast<human*>(setup_data.attack_target)->acknowledge_human(ACKNOWLEDGED_ENEMY_MS);
    if (np != nullptr && np->current_team != HUMAN_TEAM_NONE)
    {
        Taunt_enemy_timestamps[np->current_team].set_rand(7000, 12000);
    }
}

// 0x006A8190 (:701)
ai_results battle_cry_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (this != last_action)
    {
        const object* target = ai->perception.get_attention_target();
        if (target == nullptr || target->get_obj_type() != OT_HUMAN)
        {
            return AI_RESULT_FAILURE;
        }
        ai->perception.set_attention_mode(ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
        ai->planner.lock_action(true);
        ai->stop_moving();
        if (np->get_current_voice_line() != VOICE_LINE_BATTLE_CRY)
        {
            np->play_voice_line(VOICE_LINE_BATTLE_CRY, match_goal_callback, const_cast<ai_goal*>(ai->planner.get_current_goal()), AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
            npc_sound_info::queue<alert_shout_enemy>(alert_shout_enemy(np, static_cast<const human*>(target)));
        }
        if (human_in_vehicle(np) == nullptr && human_combat_get_manned_turret(np) == nullptr)
        {
            matrix dest_orient;
            dest_orient.make_quick((target->pos - np->pos).copy_flatten_normalize_safe());
            np->char_inst->play_action(ANIMATION_ACTION_BATTLE_CRY, ANIM_ALIAS_NONE, 1.0f, 0x4040);
            np->char_inst->set_action_starting_orient(np->pos, np->orient, np->pos, dest_orient, ANIMATION_ACTION_BATTLE_CRY, -1.0f);
        }
    }
    human_set_combat_ready(np, true, 0);
    return AI_RESULT_SUCCESS;
}

// 0x00640D40 (:741)
ai_results battle_cry_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human_set_combat_ready(np, true, 0);
    object* target = ai->perception.get_attention_target();
    if (target != nullptr && target->get_obj_type() == OT_HUMAN)
    {
        static_cast<human*>(target)->acknowledge_human(ACKNOWLEDGED_ENEMY_MS);
    }
    if (np->char_inst->is_playing_action(ANIMATION_ACTION_BATTLE_CRY) || np->get_current_voice_line() == VOICE_LINE_BATTLE_CRY)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    ai->reaction_info.reset();
    return AI_RESULT_SUCCESS;
}

// 0x00683E50 (:766)
int point_out_enemy_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    if (np->current_team == HUMAN_TEAM_MARAUDER || human_in_vehicle(np) != nullptr || human_combat_get_manned_turret(np) != nullptr || np->npc_flags.roadblock_guard != 0 || !ai->should_react(REACT_REASON_POINT_OUT_OR_BATTLE_CRY))
    {
        return -1;
    }
    const ai_belief* ally = ai->perception.get_first_belief(AI_BELIEF_CLOSEST_ALLY);
    if (ally == nullptr)
    {
        return -1;
    }
    const human* ally_hp = World.object_from_handle<human>(ally->handle);
    if (ally_hp == nullptr || human_is_dead(ally_hp))
    {
        return -1;
    }
    object* selected = goal_setup_data.selected_target;
    if (selected == nullptr || selected->get_obj_type() != OT_HUMAN || static_cast<const human*>(selected)->has_been_acknowledged() || vec_dist_squared(np->pos, selected->pos) < 49.0f) // 7 m
    {
        return -1;
    }
    my_data.attack_target = selected;
    vector head_pos;
    human_get_head_pos(ally_hp, head_pos);
    return ai->perception.has_los_to_position(head_pos, false) ? 1000 : -1;
}

// 0x005F0D20 (:822)
void point_out_enemy_action::commit_setup_data(npc*, const ai_action_setup_data& setup_data) const
{
    static_cast<human*>(setup_data.attack_target)->acknowledge_human(ACKNOWLEDGED_ENEMY_MS);
}

// 0x006A83A0 (:829)
ai_results point_out_enemy_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (this != last_action)
    {
        const object* objp = ai->perception.get_attention_target();
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN)
        {
            return AI_RESULT_FAILURE;
        }
        const human* hp = static_cast<const human*>(objp);
        ai->perception.pay_attention(hp->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
        ai->planner.lock_action(true);
        ai->stop_moving();
        if (!np->is_playing_spot_line())
        {
            voice_lines line = VOICE_LINE_NONE;
            const vehicle* vp = human_in_vehicle(hp);
            if (vp != nullptr)
            {
                if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO)
                {
                    line = VOICE_LINE_COMBAT_SPOT_ENEMY_VEHICLE;
                }
            }
            else if (human_teams_get_disposition(hp, HUMAN_TEAM_EDF) == TEAM_DISPOSITION_ALLIED)
            {
                line = VOICE_LINE_COMBAT_SPOT_EDF_TO_ALLIES;
            }
            else if (human_teams_are_allies(hp, HUMAN_TEAM_MARAUDER))
            {
                line = VOICE_LINE_COMBAT_SPOT_MARAUDER_TO_ALLIES;
            }
            if (line != VOICE_LINE_NONE)
            {
                np->play_voice_line(line, match_goal_callback, const_cast<ai_goal*>(ai->planner.get_current_goal()), AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
            }
            npc_sound_info::queue<alert_shout_enemy>(alert_shout_enemy(np, hp));
        }
        vector dir;
        vec_normalized_dir(dir, hp->pos, np->pos);
        animation_action anim_index;
        if (np->orient.fvec * dir > 0.70710677f) // within 45 degrees ahead
        {
            np->steering_vector = hp->pos - np->pos;
            np->steering_vector.flatten_normalize_safe();
            anim_index = ANIMATION_ACTION_POINT_AHEAD;
        }
        else
        {
            anim_index = np->orient.rvec * dir <= 0.0f ? ANIMATION_ACTION_POINT_LEFT : ANIMATION_ACTION_POINT_RIGHT;
        }
        np->char_inst->play_action(anim_index, ANIM_ALIAS_POINT, 1.0f, 0x4020);
    }
    human_set_combat_ready(np, true, 0);
    return AI_RESULT_SUCCESS;
}

// 0x00640DF0 (:904)
ai_results point_out_enemy_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human_set_combat_ready(np, true, 0);
    object* target = ai->perception.get_attention_target();
    if (target == nullptr || target->get_obj_type() != OT_HUMAN)
    {
        return AI_RESULT_FAILURE;
    }
    static_cast<human*>(target)->acknowledge_human(ACKNOWLEDGED_ENEMY_MS);
    if (!np->char_inst->is_playing_action(ANIM_ALIAS_POINT))
    {
        ai->reaction_info.reset();
        return AI_RESULT_SUCCESS;
    }
    const ai_belief* damage = ai->perception.get_first_belief(AI_BELIEF_DAMAGE_TAKEN);
    if (damage == nullptr)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    // Hit while pointing: stop and react to the damage instead.
    np->stop_voice(VOICE_LINE_PRIORITY_HIGH);
    np->char_inst->stop_action(ANIM_ALIAS_POINT, 5, false);
    ai->end_reaction(REACT_REASON_POINT_OUT_OR_BATTLE_CRY);
    ai->reaction_info.belief_type = damage->get_type();
    ai->reaction_info.target_handle = damage->handle;
    ai->reaction_info.reason = REACT_REASON_DAMAGE;
    ai->reaction_info.time_left.set(int_rand_range(2000, 3000));
    return AI_RESULT_FAILURE;
}

// 0x005F0D40 (:945)
int drove_off_civilian_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return human_in_vehicle(np) == nullptr && np->get_ai()->should_react(REACT_REASON_DROVE_OFF_CIVILIAN) ? 1500 : -1;
}

// 0x00683F70 (:962)
ai_results drove_off_civilian_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (this == last_action)
    {
        return AI_RESULT_SUCCESS;
    }
    ai->stop_moving();
    const object* target = ai->perception.get_attention_target();
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    np->play_voice_line(VOICE_LINE_RESTRICTED_TAUNT, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    if (human_combat_get_manned_turret(np) == nullptr)
    {
        ai->action_timer.invalidate();
        ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        np->char_inst->play_action(ANIMATION_ACTION_POINT_AHEAD, ANIM_ALIAS_POINT, 1.0f, 0);
    }
    else
    {
        ai->action_timer.set(2000); // ms
        ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    }
    ai->planner.lock_action(true);
    return AI_RESULT_SUCCESS;
}

// 0x005F0D80 (:1000)
ai_results drove_off_civilian_action::do_action(npc* np) const
{
    const timestamp& timer = np->get_ai()->action_timer;
    if (timer.valid())
    {
        return timer.elapsed() ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
    }
    return np->char_inst->is_playing_action(ANIM_ALIAS_POINT) ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x005F0DE0 (:1015)
ai_results drove_off_civilian_action::cleanup(npc* np, const ai_action*) const
{
    np->get_ai()->end_reaction(REACT_REASON_DROVE_OFF_CIVILIAN);
    return AI_RESULT_SUCCESS;
}

// 0x005F0E30 (:1025)
int react_watch_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (human_in_vehicle(np) != nullptr)
    {
        return -1;
    }
    if (goal_setup_data.goal == react_goal::get_instance() && !np->get_ai()->should_react(human_combat_get_manned_turret(np) != nullptr ? REACT_REASON_WATCH_ON_TURRET : REACT_REASON_WATCH))
    {
        return -1;
    }
    return 3000;
}

// 0x00640F40 (:1047)
ai_results react_watch_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (this == last_action)
    {
        return AI_RESULT_SUCCESS;
    }
    ai->stop_moving();
    const object* target = ai->perception.get_attention_target();
    const ai_belief* belief = ai->perception.get_attention_belief();
    attention_look_at_modes mode = ATTENTION_LOOK_AT_BELIEF;
    if (target == nullptr)
    {
        if (belief == nullptr)
        {
            return AI_RESULT_FAILURE;
        }
    }
    else if (belief == nullptr || (belief->get_type() != AI_BELIEF_FIRST_CONTACT && (belief->get_type() < AI_BELIEF_COMBAT_SOUND || belief->get_type() > AI_BELIEF_EXPLOSION)))
    {
        mode = ATTENTION_LOOK_AT_OBJECT;
    }
    ai->perception.set_attention_mode(mode, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    int wait;
    if (human_combat_get_manned_turret(np) != nullptr)
    {
        wait = int_rand_range(800, 1200);
    }
    else if (human_team_is_non_combatant(np->current_team))
    {
        wait = int_rand_range(600, 1000);
    }
    else
    {
        wait = int_rand_range(2000, 4000);
    }
    ai->action_timer.set(wait);
    ai->planner.lock_action(true);
    return AI_RESULT_SUCCESS;
}

// 0x005F0EA0 (:1111)
ai_results react_watch_action::do_action(npc* np) const
{
    return np->get_ai()->action_timer.elapsed() ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

// 0x005F0ED0 (:1118)
ai_results react_watch_action::cleanup(npc* np, const ai_action*) const
{
    np->get_ai()->reaction_info.reset();
    return AI_RESULT_SUCCESS;
}

// 0x005F0F20 (:1128)
int react_cower_rule::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return human_in_vehicle(np) == nullptr && np->get_ai()->should_react(REACT_REASON_COWER) ? 10000 : -1;
}

// 0x005F0F60 (:1148)
int react_bomb_rule::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->get_ai()->should_react(REACT_REASON_BOMB) ? 1000 : -1;
}

// 0x005F0FA0 (:1163)
int return_taunt_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->get_ai()->persona_flags.stand_up_to_edf ? 2000 : -1;
}

// 0x005F0FD0 (:1180)
ai_results return_taunt_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        ai->action_timer.set_rand(500, 800);
        ai->action_substate = 0;
    }
    return AI_RESULT_SUCCESS;
}

// 0x00684050 (:1192)
ai_results return_taunt_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const object* taunter = ai->reaction_info.target_handle != 0xFFFFFFFF ? World.object_from_handle(ai->reaction_info.target_handle) : nullptr;
    if (taunter == nullptr || taunter->get_obj_type() != OT_HUMAN || taunter->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    if (ai->action_substate != 0)
    {
        return np->char_inst->is_playing_action(ANIM_ALIAS_TAUNT) ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
    }
    if (ai->action_timer.elapsed())
    {
        ai->stop_moving();
        ai->perception.pay_attention(taunter->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
        np->char_inst->play_action((animation_action)(ANIMATION_ACTION_REACT_ANGRY_A + (vrand() % 100 > 50 ? 3 : 0)), ANIM_ALIAS_TAUNT, 1.0f, 0);
        matrix dest_orient;
        dest_orient.make_quick((taunter->pos - np->pos).copy_flatten_normalize());
        np->char_inst->set_action_dest_orient(np->orient, dest_orient, ANIM_ALIAS_TAUNT, 0.25f);
        np->play_voice_line(VOICE_LINE_PATROL_TAUNT_CIVILIAN_DEFIANT_RESPONSE, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        ai->planner.lock_action(true);
        ai->action_substate = 1;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00641050 (:1231)
ai_results return_taunt_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->get_ai()->end_reaction(REACT_REASON_TAUNT);
        if (np->char_inst->rdd.is_loaded())
        {
            np->char_inst->stop_action(ANIM_ALIAS_TAUNT, 5, true);
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F1010 (:1249)
int react_to_taunt_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 2100;
}

// 0x005F1020 (:1254)
ai_results react_to_taunt_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        ai->action_timer.set_rand(500, 800);
        ai->action_substate = 0;
    }
    return AI_RESULT_SUCCESS;
}

// 0x00684220 (:1266)
ai_results react_to_taunt_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const object* taunter = ai->reaction_info.target_handle != 0xFFFFFFFF ? World.object_from_handle(ai->reaction_info.target_handle) : nullptr;
    if (taunter == nullptr || taunter->get_obj_type() != OT_HUMAN || taunter->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    if (ai->action_substate != 0)
    {
        return np->char_inst->is_playing_action(ANIMATION_ACTION_REACT_SURPRISED) ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
    }
    if (ai->action_timer.elapsed())
    {
        ai->stop_moving();
        ai->perception.pay_attention(taunter->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
        np->char_inst->play_action(ANIMATION_ACTION_REACT_SURPRISED, ANIM_ALIAS_NONE, 1.0f, 0);
        matrix dest_orient;
        dest_orient.make_quick((taunter->pos - np->pos).copy_flatten_normalize());
        np->char_inst->set_action_dest_orient(np->orient, dest_orient, ANIMATION_ACTION_REACT_SURPRISED, 0.25f);
        np->play_voice_line(VOICE_LINE_FLINCH, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        ai->planner.lock_action(true);
        ai->action_substate = 1;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005F1060 (:1304)
ai_results react_to_taunt_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->get_ai()->end_reaction(REACT_REASON_TAUNT);
        np->char_inst->stop_action(ANIMATION_ACTION_REACT_SURPRISED, 5, true);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F10D0 (:1319)
int flee_from_taunt_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return vrand() % 100 > 50 ? -1 : 2050;
}

// 0x006410C0 (:1328)
ai_results flee_from_taunt_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const object* taunter = ai->reaction_info.target_handle != 0xFFFFFFFF ? World.object_from_handle(ai->reaction_info.target_handle) : nullptr;
    if (taunter != nullptr && taunter->get_obj_type() == OT_HUMAN && !taunter->is_killed())
    {
        np->acknowledged_time.set(ACKNOWLEDGED_ENEMY_MS); // the npc's own, not the taunter's
    }
    ai->end_reaction(REACT_REASON_TAUNT);
    return AI_RESULT_SUCCESS;
}

// 0x005F1100 (:1343)
int react_vehicle_commandeered::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->get_ai()->should_react(REACT_REASON_COMMANDEERED) ? 100 : -1;
}

// 0x0066A0A0 (:1353)
ai_results react_vehicle_commandeered::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    const object* objp = ai->reaction_info.target_handle != 0xFFFFFFFF ? World.object_from_handle(ai->reaction_info.target_handle) : nullptr;
    const object* vehicle_obj = nullptr;
    if (objp != nullptr && objp->get_obj_type() == OT_HUMAN && static_cast<const human*>(objp)->vehicle_handle != 0xFFFFFFFF)
    {
        vehicle_obj = World.object_from_handle(static_cast<const human*>(objp)->vehicle_handle);
    }
    if (vehicle_obj == nullptr || vehicle_obj->get_obj_type() != OT_VEHICLE)
    {
        ai->end_reaction(REACT_REASON_COMMANDEERED);
        return AI_RESULT_FAILURE;
    }
    // Back away from both the vehicle and whoever took it.
    const vector to_vehicle = (vehicle_obj->pos - np->pos).copy_flatten_normalize_safe();
    const vector to_human = (objp->pos - np->pos).copy_flatten_normalize_safe();
    ai->perception.set_attention_mode(ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    const vector away = -(to_human + to_vehicle);
    ai->set_direct_movement_fast(away.copy_flatten_normalize_safe(), HMS_WALK, HST_STAND, -1.0f);
    ai->action_timer.set(2000); // ms
    ai->planner.lock_action(true);
    ai->pedestrian_info.paused = true;
    ai->pedestrian_info.pause_timer.set(0);
    ai->pedestrian_info.animation_cooldown.set(10000); // ms
    return AI_RESULT_SUCCESS;
}

// 0x005F1140 (:1385)
ai_results react_vehicle_commandeered::do_action(npc* np) const
{
    return np->get_ai()->action_timer.elapsed() ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

// 0x005F1170 (:1395)
ai_results react_vehicle_commandeered::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->get_ai()->end_reaction(REACT_REASON_COMMANDEERED);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F11C0 (:1472)
int watch_raid_end_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if (np->npc_flags.raid_cheer != 0)
    {
        return -1;
    }
    return activity_raid_get_raid_object() != nullptr ? 200 : 0;
}

// 0x006843D0 (:1489)
ai_results watch_raid_end_action::enter(npc* np, const ai_action* last_action) const
{
    if (this == last_action)
    {
        return AI_RESULT_SUCCESS;
    }
    ai_system* ai = np->get_ai();
    const object* raid_object = activity_raid_get_raid_object();
    if (raid_object == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai->perception.pay_attention(raid_object->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    ai->stop_moving();
    return AI_RESULT_SUCCESS;
}

// 0x005F11F0 (:1513)
ai_results watch_raid_end_action::do_action(npc* np) const
{
    return np->npc_flags.raid_cheer != 0 ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

// 0x005F1210 (:1526)
int celebrate_raid_end_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->npc_flags.raid_cheer != 0 ? 200 : -1;
}

// 0x00684420 (:1535)
ai_results celebrate_raid_end_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (this == last_action)
    {
        return AI_RESULT_SUCCESS;
    }
    ai->action_timer.set_rand(250, 3000);
    ai->stop_moving();
    const object* raid_object = activity_raid_get_raid_object();
    if (raid_object != nullptr)
    {
        ai->perception.pay_attention(raid_object->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    }
    if (np->npc_flags.raid_cheer == 0)
    {
        ai->desired_voice_line = VOICE_LINE_NONE;
    }
    else if (np->current_team == HUMAN_TEAM_MARAUDER || vrand() % 100 <= 40)
    {
        ai->desired_voice_line = VOICE_LINE_CHEER;
    }
    else
    {
        ai->desired_voice_line = (voice_lines)(VOICE_LINE_CHEER - 97); // the line of that number, name not checked
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F1230 (:1567)
ai_results celebrate_raid_end_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (ai->action_timer.valid())
    {
        if (ai->action_timer.elapsed())
        {
            if (ai->desired_voice_line != VOICE_LINE_NONE && np->get_current_voice_line_handle() == INVALID_VOICE_LINE_HANDLE)
            {
                np->play_voice_line(ai->desired_voice_line, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
            }
            np->char_inst->play_action(ANIMATION_ACTION_CHEER_A, ANIM_ALIAS_NONE, 1.0f, 0);
            ai->action_timer.invalidate();
        }
    }
    else if (!np->char_inst->is_playing_action(ANIMATION_ACTION_CHEER_A))
    {
        if (np->npc_flags.raid_cheer == 0)
        {
            return AI_RESULT_SUCCESS;
        }
        ai->action_timer.set(0);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00641150 (:1598)
ai_results celebrate_raid_end_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (this != next_action && np->char_inst->rdd.is_loaded())
    {
        np->char_inst->stop_action(ANIMATION_ACTION_CHEER_A, 5, false);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F1300 (:1617)
int approach_raid_area_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if (this != np->get_ai()->planner.get_current_action())
    {
        const float radius = activity_raid_get_demolition_radius();
        const object* raid_object = activity_raid_get_raid_object();
        if (raid_object == nullptr)
        {
            return -1;
        }
        if (vec_xz_dist_squared(raid_object->pos, np->pos) < (radius + 10.0f) * (radius + 10.0f)) // 10 m beyond the radius
        {
            return 0;
        }
    }
    return 200;
}

// 0x005F1380 (:1643)
ai_results approach_raid_area_action::enter(npc* np, const ai_action* last_action) const
{
    if (this == last_action)
    {
        return AI_RESULT_SUCCESS;
    }
    ai_system* ai = np->get_ai();
    if (!activity_raid_get_exit_point(np->pos, ai->desired_destination))
    {
        return AI_RESULT_FAILURE;
    }
    const float dist = fl_rand_range(0.0f, 10.0f) + activity_raid_get_demolition_radius() + 10.0f;
    ai->desired_vehicle_dist_to_goal_pos = dist * dist;
    return AI_RESULT_SUCCESS;
}

// 0x006A8620 (:1669)
ai_results approach_raid_area_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const object* raid_object = activity_raid_get_raid_object();
    if (raid_object != nullptr && vec_xz_dist_squared(raid_object->pos, np->pos) < ai->desired_vehicle_dist_to_goal_pos)
    {
        return AI_RESULT_SUCCESS;
    }
    if (vec_xz_dist_squared(ai->desired_destination, np->pos) < 1.0f) // 1 m
    {
        return AI_RESULT_SUCCESS;
    }
    const human_movement_states m_state = vec_dist_squared(ai->desired_destination, np->pos) <= 10.0f ? HMS_WALK : HMS_RUN;
    return ai->pathfind_to(ai->desired_destination, m_state, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false) > AI_PF_RAGDOLLED ? AI_RESULT_FAILURE : AI_RESULT_IN_PROGRESS;
}

// catch_breath_prone_action has no initializer in the .CRT$XC table and no singleton, so rfg.exe never constructs it.
catch_breath_goal catch_breath_goal::the_instance; // 0x01DC9AFC

// 0x0063A1F0 (.h:14)
catch_breath_goal::catch_breath_goal()
    : ai_goal(ai_plan_conditions(68), "catch breath")
{
}

ai_goal* catch_breath_goal::get_instance()
{
    return &the_instance;
}

// 0x005EBAD0
catch_breath_goal::~catch_breath_goal()
{
}

react_goal react_goal::the_instance; // 0x01DC9A04

// 0x0063A290 (.h:15)
react_goal::react_goal()
    : ai_goal(ai_plan_conditions(69), "react")
{
}

ai_goal* react_goal::get_instance()
{
    return &the_instance;
}

// 0x005EBAF0
react_goal::~react_goal()
{
}

raid_reaction_goal raid_reaction_goal::the_instance; // 0x01D68828

// 0x0063A330 (.h:16)
raid_reaction_goal::raid_reaction_goal()
    : ai_goal(ai_plan_conditions(71), "raid reaction")
{
}

ai_goal* raid_reaction_goal::get_instance()
{
    return &the_instance;
}

// 0x005EBB00
raid_reaction_goal::~raid_reaction_goal()
{
}

taunt_reaction_goal taunt_reaction_goal::the_instance; // 0x01D68878

// 0x0063A3D0 (.h:17)
taunt_reaction_goal::taunt_reaction_goal()
    : ai_goal(ai_plan_conditions(72), "react to taunt")
{
}

ai_goal* taunt_reaction_goal::get_instance()
{
    return &the_instance;
}

// 0x005EBB10
taunt_reaction_goal::~taunt_reaction_goal()
{
}

salute_reaction_goal salute_reaction_goal::the_instance; // 0x01DC986C

// 0x0063A470 (.h:18)
salute_reaction_goal::salute_reaction_goal()
    : ai_goal(ai_plan_conditions(73), "react to being saluted")
{
}

ai_goal* salute_reaction_goal::get_instance()
{
    return &the_instance;
}

// 0x005EBB20
salute_reaction_goal::~salute_reaction_goal()
{
}

catch_breath_action catch_breath_action::the_instance; // 0x01DDB838

// 0x0063A510 (.h:21)
catch_breath_action::catch_breath_action()
    : ai_action("catch breath", ai_plan_conditions_from_bits({47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({68}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* catch_breath_action::get_instance()
{
    return &the_instance;
}

// 0x005EBB30
catch_breath_action::~catch_breath_action()
{
}

// 0x0063A740 (.h:21)
int catch_breath_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

battle_cry_action battle_cry_action::the_instance; // 0x01D74E30

// 0x0063A780 (.h:25)
battle_cry_action::battle_cry_action()
    : ai_action("battle cry", ai_plan_conditions_from_bits({9, 58, 136, 153}), ai_plan_conditions_from_bits({69}))
{
    flags.has_enter = 1;
}

ai_action* battle_cry_action::get_instance()
{
    return &the_instance;
}

// 0x005EBB40
battle_cry_action::~battle_cry_action()
{
}

// 0x0063A8F0 (.h:25)
int battle_cry_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

point_out_enemy_action point_out_enemy_action::the_instance; // 0x01DCA728

// 0x0063A920 (.h:26)
point_out_enemy_action::point_out_enemy_action()
    : ai_action("point out enemy", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 153}), ai_plan_conditions_from_bits({69}))
{
    flags.has_enter = 1;
}

ai_action* point_out_enemy_action::get_instance()
{
    return &the_instance;
}

// 0x005EBB50
point_out_enemy_action::~point_out_enemy_action()
{
}

// 0x0063AB60 (.h:26)
int point_out_enemy_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

react_to_enemy_action react_to_enemy_action::the_instance; // 0x01DCA160

// 0x0063AB90 (.h:27)
react_to_enemy_action::react_to_enemy_action()
    : ai_action("react to enemy", ai_plan_conditions_from_bits({9, 58}), ai_plan_conditions_from_bits({69}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* react_to_enemy_action::get_instance()
{
    return &the_instance;
}

// 0x005EBB60
react_to_enemy_action::~react_to_enemy_action()
{
}

// 0x0063AC40 (.h:27)
int react_to_enemy_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

react_watch_action react_watch_action::the_instance; // 0x01D72134

// 0x0063AD10 (.h:31)
react_watch_action::react_watch_action()
    : ai_action("react - watch", ai_plan_conditions_from_bits({9}), ai_plan_conditions_from_bits({69, 73}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* react_watch_action::get_instance()
{
    return &the_instance;
}

// 0x005EBB80
react_watch_action::~react_watch_action()
{
}

// 0x0063ADD0 (.h:31)
int react_watch_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

surprised_by_projectile_action surprised_by_projectile_action::the_instance; // 0x01DCA3EC

// 0x0063AC80 (.h:28)
surprised_by_projectile_action::surprised_by_projectile_action()
    : ai_action("surprised by projectile", ai_plan_conditions(), ai_plan_conditions_from_bits({69}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* surprised_by_projectile_action::get_instance()
{
    return &the_instance;
}

// 0x005EBB70
surprised_by_projectile_action::~surprised_by_projectile_action()
{
}

// 0x0063ACD0 (.h:28)
int surprised_by_projectile_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

drove_off_civilian_action drove_off_civilian_action::the_instance; // 0x01DCA0EC

// 0x0063AEB0 (.h:37)
drove_off_civilian_action::drove_off_civilian_action()
    : ai_action("drove off civilian", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 153}), ai_plan_conditions_from_bits({69}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* drove_off_civilian_action::get_instance()
{
    return &the_instance;
}

// 0x005EBBA0
drove_off_civilian_action::~drove_off_civilian_action()
{
}

// 0x0063B0F0 (.h:37)
int drove_off_civilian_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

react_vehicle_commandeered react_vehicle_commandeered::the_instance; // 0x01DCA278

// 0x0063AE10 (.h:34)
react_vehicle_commandeered::react_vehicle_commandeered()
    : ai_action("react - commandeered", ai_plan_conditions_from_bits({47}), ai_plan_conditions_from_bits({69}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* react_vehicle_commandeered::get_instance()
{
    return &the_instance;
}

// 0x005EBB90
react_vehicle_commandeered::~react_vehicle_commandeered()
{
}

// 0x0063AE70 (.h:34)
int react_vehicle_commandeered::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

react_cower_rule react_cower_rule::the_instance; // 0x01D71EB4

// 0x0063B130 (.h:40)
react_cower_rule::react_cower_rule()
    : ai_production_rule("cower = react", ai_plan_conditions_from_bits({124}), ai_plan_conditions_from_bits({69}))
{
}

ai_action* react_cower_rule::get_instance()
{
    return &the_instance;
}

// 0x005EBBB0
react_cower_rule::~react_cower_rule()
{
}

// 0x0063B1B0 (.h:40)
int react_cower_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

react_bomb_rule react_bomb_rule::the_instance; // 0x01DCA498

// 0x0063B1E0 (.h:41)
react_bomb_rule::react_bomb_rule()
    : ai_production_rule("react bomb", ai_plan_conditions_from_bits({123}), ai_plan_conditions_from_bits({69}))
{
}

ai_action* react_bomb_rule::get_instance()
{
    return &the_instance;
}

// 0x005EBBC0
react_bomb_rule::~react_bomb_rule()
{
}

// 0x0063B260 (.h:41)
int react_bomb_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

return_taunt_action return_taunt_action::the_instance; // 0x01D6AB20

// 0x0063B290 (.h:44)
return_taunt_action::return_taunt_action()
    : ai_action("return taunt", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({72}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* return_taunt_action::get_instance()
{
    return &the_instance;
}

// 0x005EBBD0
return_taunt_action::~return_taunt_action()
{
}

// 0x0063B530 (.h:44)
int return_taunt_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

react_to_taunt_action react_to_taunt_action::the_instance; // 0x01D7219C

// 0x0063B570 (.h:45)
react_to_taunt_action::react_to_taunt_action()
    : ai_action("react to taunt", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({72}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* react_to_taunt_action::get_instance()
{
    return &the_instance;
}

// 0x005EBBE0
react_to_taunt_action::~react_to_taunt_action()
{
}

// 0x0063B810 (.h:45)
int react_to_taunt_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

flee_from_taunt_action flee_from_taunt_action::the_instance; // 0x01DDACF0

// 0x0063B850 (.h:46)
flee_from_taunt_action::flee_from_taunt_action()
    : ai_action("flee from taunt", ai_plan_conditions(), ai_plan_conditions_from_bits({72}))
{
}

ai_action* flee_from_taunt_action::get_instance()
{
    return &the_instance;
}

// 0x005EBBF0
flee_from_taunt_action::~flee_from_taunt_action()
{
}

// 0x0063B8A0 (.h:46)
int flee_from_taunt_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

watch_raid_end_action watch_raid_end_action::the_instance; // 0x01D687C0

// 0x0063BBC0 (.h:52)
watch_raid_end_action::watch_raid_end_action()
    : ai_action("watch raid end", ai_plan_conditions_from_bits({9, 47, 58, 74, 136, 137, 145, 153}), ai_plan_conditions_from_bits({71}))
{
    flags.has_enter = 1;
}

ai_action* watch_raid_end_action::get_instance()
{
    return &the_instance;
}

// 0x005EBC10
watch_raid_end_action::~watch_raid_end_action()
{
}

// 0x0063BEC0 (.h:52)
int watch_raid_end_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

celebrate_raid_end_action celebrate_raid_end_action::the_instance; // 0x01D68374

// 0x0063BF00 (.h:53)
celebrate_raid_end_action::celebrate_raid_end_action()
    : ai_action("celebrate raid end", ai_plan_conditions_from_bits({9, 47, 58, 74, 136, 137, 145, 153}), ai_plan_conditions_from_bits({71}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* celebrate_raid_end_action::get_instance()
{
    return &the_instance;
}

// 0x005EBC20
celebrate_raid_end_action::~celebrate_raid_end_action()
{
}

// 0x0063C200 (.h:53)
int celebrate_raid_end_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

approach_raid_area_action approach_raid_area_action::the_instance; // 0x01DCA1AC

// 0x0063C240 (.h:54)
approach_raid_area_action::approach_raid_area_action()
    : ai_action("approach raid area", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({74}))
{
    flags.has_enter = 1;
}

ai_action* approach_raid_area_action::get_instance()
{
    return &the_instance;
}

// 0x005EBC30
approach_raid_area_action::~approach_raid_area_action()
{
}

// 0x0063C4E0 (.h:54)
int approach_raid_area_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

react_killed_enemy_action react_killed_enemy_action::the_instance; // 0x01DC95F0

// 0x0063B8E0 (.h:49)
react_killed_enemy_action::react_killed_enemy_action()
    : ai_action("react - killed enemy", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({69}))
{
    flags.has_enter = 1;
}

ai_action* react_killed_enemy_action::get_instance()
{
    return &the_instance;
}

// 0x005EBC00
react_killed_enemy_action::~react_killed_enemy_action()
{
}

// 0x0063BB80 (.h:49)
int react_killed_enemy_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

