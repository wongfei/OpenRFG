// Original: volition/rfg/code/ai/threaten_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/threaten_actions.h"

#include "volition/rfg/code/ai/ai_building_logic.h"
#include "volition/rfg/code/ai/ai_defense_coordination.h"
#include "volition/rfg/code/ai/ai_defense_coordination_internal.h"
#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/alerts.h"
#include "volition/rfg/code/ai/ambient_edf_action_manager.h"
#include "volition/rfg/code/ai/belief_iterator.h"
#include "volition/rfg/code/ai/belief_iterator.inl"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/civilian/civilian_restricted_area_actions.h"
#include "volition/rfg/code/ai/civilian/pedestrian_actions.h"
#include "volition/rfg/code/ai/combat/combat_melee_actions.h"
#include "volition/rfg/code/ai/follow_logic.h"
#include "volition/rfg/code/ai/formation.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_internal_shared.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_guard_node.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_patrol.h"
#include "volition/rfg/code/objects/object_restricted_area.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"

#include <float.h>
#include <math.h>

// Position 32 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG32")

extern float Edf_notice_player_with_obvious_weapon_dist; // percept.cpp, 0x01D65988
extern timestamp Taunt_enemy_timestamps[4]; // goals.cpp

static const float SIGHTED_ENEMY_RELEVANCE = 1.0f; // as in percept.cpp
static const float PATROL_THREATEN_CAPSULE_RADIUS_SQ = 9.0f; // 0x01642FBC
static const float MAX_THREATEN_FROM_TURRET_DIST_SQ = 144.0f; // 0x01642FB4
static const float MAX_THREATEN_DEFAULT_DIST_SQ = 25.0f; // 0x01642FB8
static const float MAX_OPPRESS_DIST_SQ = 100.0f; // 10 m
// 0x01DCA214, from the dynamic initializer at 0x01067FF0: the cosine of 0x012C95F8, which is 5 * 0.017453f in float, widened.
static const float FACING_TARGET_DOT_THREATEN_ACTIONS = (float)cos((double)(5.0f * 0.017453f));
static const float EXECUTION_FORWARD_OFFSET = 1.1958f; // from the execute animation
static const float EXECUTION_RIGHT_OFFSET = -0.0704f;
static const float CHECK_ID_FORWARD_OFFSET = 1.09f;
static const float INTERACTION_POSITION_DIST_SQ = 0.25f;
static int execution_chance = 50; // 0x01647FFC

// The desired target when it is an npc: the check every oppression action inlines.
static human* get_desired_civilian(const ai_system* ai)
{
    if (ai->desired_target == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(ai->desired_target);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return nullptr;
    }
    return static_cast<human*>(objp);
}

static human* get_attention_human(const ai_system* ai)
{
    object* objp = World.object_from_handle(ai->perception.get_attention_handle());
    return objp != nullptr && objp->get_obj_type() == OT_HUMAN ? static_cast<human*>(objp) : nullptr;
}

// Not in rfg.exe, where these calls are direct: the one friend this file and ambient_edf_action.cpp need for the private note_possible_enemy_location.
void threaten_note_enemy(ai_perception& perception, const human* enemy, bool react)
{
    perception.note_possible_enemy_location(enemy, SIGHTED_ENEMY_RELEVANCE, react);
}

// 0x005F3400 (:216). LTCG: np ESI, target and desired_pos on the stack. Walks the last half metre, then snaps into place.
static ai_results align_to_interaction_position(npc* np, npc* target, vector desired_pos)
{
    timestamp& timer = np->get_ai()->action_timer;
    if (!timer.valid())
    {
        timer.set(2000);
    }
    vector to_np(np->pos.x - target->pos.x, np->pos.y - target->pos.y, np->pos.z - target->pos.z);
    vector dir;
    dir = to_np.copy_flatten_normalize_safe();
    if (timer.elapsed())
    {
        np->move_speed = 0.0f;
        np->steering_heading_offset = 0.0f;
        if (0.010000001f < vec_xz_dist_squared(np->pos, desired_pos))
        {
            const vector facing(-dir.x, -dir.y, -dir.z);
            matrix orient;
            orient.make_quick(facing);
            np->update_pos_and_orient(desired_pos, orient, true);
        }
        return AI_RESULT_SUCCESS;
    }
    vector move(desired_pos.x - np->pos.x, 0.0f, desired_pos.z - np->pos.z);
    const float dist = move.flatten_mag_and_normalize_safe();
    if (dist < 0.1f)
    {
        timer.set(0);
        np->move_speed = 0.0f;
        np->steering_vector = vector(-dir.x, -dir.y, -dir.z);
        np->steering_heading_offset = 0.0f;
        return AI_RESULT_IN_PROGRESS;
    }
    if (dist < 0.5f)
    {
        np->move_speed = 1.0f;
        np->steering_vector = move;
        const vector& fvec = np->orient.fvec;
        if (0.0f < fvec.y * dir.y + fvec.x * dir.x + fvec.z * dir.z)
        {
            np->steering_heading_offset = fl_atan2_safe(-fvec.x, -fvec.z) - fl_atan2_safe(move.x, move.z);
        }
        else
        {
            np->steering_heading_offset = fl_atan2_safe(fvec.x, fvec.z) - fl_atan2_safe(move.x, move.z);
        }
        return AI_RESULT_IN_PROGRESS;
    }
    np->move_speed = 2.5f;
    np->steering_vector = move;
    np->steering_heading_offset = 0.0f;
    return AI_RESULT_IN_PROGRESS;
}

// 0x005F37B0 (:294). LTCG: first EDI, second ESI; the rest on the stack. A decompile shows these slots one to two to the left.
static void play_synced_animation(human* first, human* second, vector offset, int first_anim_action, int second_anim_action, vanim_aliases first_anim_alias, vanim_aliases second_anim_alias, bool set_alignment, unsigned int flags)
{
    vanim_play_action(first->char_inst->vp, index_alias(first_anim_action, first_anim_alias), flags, 1.0f, 1.0f);
    vanim_play_action(second->char_inst->vp, index_alias(second_anim_action, second_anim_alias), flags, 1.0f, 1.0f);
    if (set_alignment)
    {
        vector dir(second->pos.x - first->pos.x, second->pos.y - first->pos.y, second->pos.z - first->pos.z);
        dir.flatten_normalize();
        matrix orient;
        orient.make_quick(dir);
        vanim_set_action_starting_orient(first->char_inst->vp, first->pos, first->orient, first->pos, orient, first_anim_alias, 0.2f);
        const vector back(-dir.x, -dir.y, -dir.z);
        orient.make_quick(back);
        vanim_set_action_starting_orient(second->char_inst->vp, second->pos, second->orient, second->pos, orient, second_anim_alias, 0.2f);
    }
    first->block_movement_anim = first_anim_action;
    first->block_steering_vector.zero();
    first->block_steering_heading_offset = 0.0f;
    first->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
    second->block_movement_anim = second_anim_action;
    second->block_steering_vector.zero();
    second->block_steering_heading_offset = 0.0f;
    second->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
    vanim_attach_actions(first, second, first_anim_alias, second_anim_alias, offset, 0.0f);
}

// 0x006BF4A0 (:328). LTCG: np ESI, voice_line EDI, target on the stack.
static void threaten_go_hostile(npc* np, const human* target, voice_lines voice_line)
{
    threaten_note_enemy(np->get_ai()->perception, target, true);
    if (voice_line != VOICE_LINE_NONE)
    {
        np->play_voice_line(voice_line, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    }
    if (object_is_player(target) && gsm_get_alert_level() == ALERT_LEVEL_GREEN)
    {
        gsm_raise_the_alert_level_in_one_second_if_not_dead(np->get_handle());
    }
    alert_shout_enemy alert(np, target);
    npc_sound_info::queue(alert);
}

// 0x00689030 (:354). LTCG: np EAX, target and ignore_proximity on the stack.
static bool should_become_hostile(const npc* np, const human* target, bool ignore_proximity)
{
    const ai_system* ai = np->get_ai();
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_UNFRIENDLY, target->get_handle());
    if (belief == nullptr || (belief->flags & BF_UNCONFIRMED) != 0 || (belief->flags & BF_IN_RESTRICTED_AREA) == 0)
    {
        return false;
    }
    if (target->get_obj_type() == OT_HUMAN && target->get_sub_type() == OT_SUB_HUMAN_PLAYER && static_cast<const player*>(target)->pflags.jetpack_started)
    {
        return true;
    }
    if (human_combat_playing_combat_move(target))
    {
        return true;
    }
    if (!ignore_proximity && vec_dist_squared(np->pos, target->pos) < 6.25f)
    {
        return true;
    }
    if (human_in_vehicle(target) != nullptr)
    {
        return true;
    }
    belief_obj_iterator<rfg_mover> it;
    it.init(ai, AI_BELIEF_BUILDING);
    while (it)
    {
        rfg_mover* building = it;
        if (building->restricted_area != 0xFFFFFFFF)
        {
            const object* area = World.object_from_handle(building->restricted_area);
            if (area != nullptr && area->get_obj_type() == OT_RESTRICTED_AREA && !static_cast<const object_restricted_area*>(area)->is_disabled() && ai_building_is_in_building(target->pos, building))
            {
                return true;
            }
        }
        ++it;
    }
    return restricted_area_in_any_region(target->pos, false);
}

// 0x00643770 (:415). LTCG: np ESI.
static void threaten_action_set_attention_mode(npc* np)
{
    ai_system* ai = np->get_ai();
    if (human_combat_get_manned_turret(np) != nullptr)
    {
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        return;
    }
    human_set_combat_ready(np, true, 0);
    ai->perception.set_attention_mode(ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
}

// 0x006BF530 (:437). LTCG: np EAX, can_acknowledge and ignore_proximity on the stack. False when the target is gone or the npc went hostile.
static bool threaten_action_common(npc* np, bool can_acknowledge, bool ignore_proximity)
{
    ai_system* ai = np->get_ai();
    human* target = get_attention_human(ai);
    if (target == nullptr)
    {
        return false;
    }
    if (should_become_hostile(np, target, ignore_proximity))
    {
        threaten_go_hostile(np, target, VOICE_LINE_RESTRICTED_TARGET_HOSTILE);
        return false;
    }
    if (ai->perception.get_attention_mode() == ATTENTION_AIM_AT_OBJECT)
    {
        human_set_combat_ready(np, true, 0);
    }
    turret* tp = human_combat_get_manned_turret(np);
    if (tp != nullptr)
    {
        vector aim_pos;
        if (ai->perception.get_effective_target_pos(aim_pos, target, nullptr, nullptr, false))
        {
            tp->aim_turret(aim_pos);
        }
    }
    object_guard_node* gnp = ai->get_guard_node();
    if (gnp != nullptr && vec_dist_squared(np->pos, gnp->pos) < 4.0f)
    {
        ai->reserve_guard_node(gnp, -128);
    }
    if (can_acknowledge && ai_defense_get_threaten_stage(ai->desired_defense_assignment_handle, nullptr) == AI_DEFENSE_THREATEN_NONE && !target->has_been_acknowledged())
    {
        np->play_voice_line(np->npc_flags.activity_ha ? VOICE_LINE_HA_EDF_SPOT_PLAYER : VOICE_LINE_RESTRICTED_SPOT, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        alert_shout_unfriendly unfriendly(np, target);
        npc_sound_info::queue(unfriendly);
        alert_shout_threaten threaten(np, target);
        npc_sound_info::queue(threaten);
    }
    return true;
}

static ai_results threaten_action_common_result(npc* np, bool can_acknowledge, bool ignore_proximity)
{
    return threaten_action_common(np, can_acknowledge, ignore_proximity) ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

static bool threaten_stage_ignores_proximity(const ai_system* ai)
{
    return AI_DEFENSE_THREATEN_WARNING_SHOT < ai_defense_get_threaten_stage(ai->desired_defense_assignment_handle, nullptr);
}

// 0x0066E590 (:515). LTCG: np EAX, dist_sq on the stack.
static bool done_with_goto(const npc* np, float dist_sq)
{
    if (ai_defense_get_assignment(np->get_ai()->desired_defense_assignment_handle) == AI_DEFENSE_ASSIGNMENT_ACCOMPANY && dist_sq <= 64.0f)
    {
        return true;
    }
    return dist_sq <= 16.0f || !restricted_area_in_any_region(np->pos, false);
}

// 0x005F39C0 (:535). LTCG: target EDX, dir ECX, np on the stack.
static bool target_is_in_way_of_patrol(const npc* np, const human* target, const vector& dir)
{
    if (1.5f < fabsf(np->pos.y - target->pos.y))
    {
        return false;
    }
    const vector start(np->pos.x + dir.x * 0.0f, np->pos.y + dir.y * 0.0f, np->pos.z + dir.z * 0.0f);
    const vector end(np->pos.x + dir.x * 4.0f, np->pos.y + dir.y * 4.0f, np->pos.z + dir.z * 4.0f);
    vector closest;
    ix_closest_point_on_line_segment(closest, target->pos, start, end);
    return vec_xz_dist_squared(closest, target->pos) <= PATROL_THREATEN_CAPSULE_RADIUS_SQ;
}

// 0x006437C0 (:546)
static bool threaten_target_is_valid_internal(const npc* np, const object_squad* squad, const human* target, bool on_turret, gfm_orders_types orders_type, const vector& patrol_dir)
{
    const ai_system* ai = np->get_ai();
    if (100.0f <= vec_xz_dist_squared(np->pos, target->pos))
    {
        return false;
    }
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_UNFRIENDLY, target->get_handle());
    if (belief == nullptr || (belief->flags & BF_UNCONFIRMED) != 0)
    {
        return false;
    }
    if (target->is_killed() || human_in_vehicle(target) != nullptr)
    {
        return false;
    }
    if (target->get_obj_type() == OT_HUMAN && target->get_sub_type() == OT_SUB_HUMAN_NPC && static_cast<const npc*>(target)->npc_flags.civilian_has_been_oppressed)
    {
        return false;
    }
    if (on_turret)
    {
        return vec_dist_squared(np->pos, target->pos) <= MAX_THREATEN_FROM_TURRET_DIST_SQ;
    }
    if (orders_type == GFM_ORDERS_TYPE_PATROL)
    {
        if (target_is_in_way_of_patrol(np, target, patrol_dir))
        {
            return true;
        }
        for (int i = 0; i < squad->member_array.size(); i++)
        {
            const unsigned int handle = squad->member_array[i];
            if (handle == np->get_handle())
            {
                continue;
            }
            const npc* member = World.object_from_handle<npc>(handle);
            if (member == nullptr)
            {
                continue;
            }
            const vector to_target(target->pos.x - member->pos.x, target->pos.y - member->pos.y, target->pos.z - member->pos.z);
            if (0.0f < to_target * patrol_dir && target_is_in_way_of_patrol(member, target, patrol_dir))
            {
                return true;
            }
        }
        return false;
    }
    if (orders_type == GFM_ORDERS_TYPE_DEFEND && ai->outside_defensive_perimeter(target->pos, false, nullptr))
    {
        return false;
    }
    return vec_dist_squared(np->pos, target->pos) <= MAX_THREATEN_DEFAULT_DIST_SQ;
}

// 0x00643BB0 (:623). LTCG: np EAX, dir on the stack.
static bool get_next_patrol_dir(vector& dir, const npc* np)
{
    const object_squad* squad = np->get_ai()->get_squad();
    if (squad == nullptr)
    {
        return false;
    }
    const object_patrol* patrol = squad->get_patrol();
    if (patrol == nullptr)
    {
        return false;
    }
    const npc* leader = squad->get_leader();
    if (leader == nullptr || leader->is_killed())
    {
        return false;
    }
    const patrol_node_handle node = patrol->get_next_patrol_point_handle(np->pos);
    vector point;
    if (!patrol->get_node_pos(node, point))
    {
        return false;
    }
    dir = vector(point.x - leader->pos.x, point.y - leader->pos.y, point.z - leader->pos.z);
    if (dir.flatten_mag_and_normalize_safe() < 1.0f)
    {
        dir = vector(point.x - np->pos.x, point.y - np->pos.y, point.z - np->pos.z);
        dir.flatten_normalize_safe();
    }
    return true;
}

// 0x005F3B30 (:672)
void ai_taunt_enemy_reset_delay(const npc* np)
{
    if (np != nullptr && np->current_team != HUMAN_TEAM_NONE)
    {
        Taunt_enemy_timestamps[np->current_team].set_rand(7000, 12000);
    }
}

// 0x005F3B60 (:686)
bool ai_can_taunt_enemy(const npc* np)
{
    if (np == nullptr || np->current_team == HUMAN_TEAM_NONE)
    {
        return false;
    }
    const timestamp& next = Taunt_enemy_timestamps[np->current_team];
    return !next.valid() || next.elapsed();
}

// 0x00643D50 (:694)
bool ai_threaten_target_is_valid(const npc* np, const human* target)
{
    if (np == nullptr || human_teams_get_disposition(np, HUMAN_TEAM_EDF) != TEAM_DISPOSITION_ALLIED)
    {
        return false;
    }
    if (object_is_player(target) && human_weapons_is_obvious_weapon_equipped(target))
    {
        const float notice_dist = Edf_notice_player_with_obvious_weapon_dist + 2.0f;
        if (vec_dist_squared(np->pos, target->pos) < notice_dist * notice_dist)
        {
            return true;
        }
    }
    const bool on_turret = human_combat_get_manned_turret(np) != nullptr;
    const object_squad* squad = np->get_ai()->get_squad();
    if (squad == nullptr)
    {
        return false;
    }
    const gfm_orders_types orders_type = squad->get_squad_orders_type();
    vector patrol_dir; // only read for a patrol, which fills it (original)
    if (orders_type == GFM_ORDERS_TYPE_PATROL)
    {
        if (!get_next_patrol_dir(patrol_dir, np))
        {
            return false;
        }
    }
    else if (orders_type != GFM_ORDERS_TYPE_DEFEND && !on_turret)
    {
        return false;
    }
    return threaten_target_is_valid_internal(np, squad, target, on_turret, orders_type, patrol_dir);
}

// 0x00689170 (:736)
human* ai_threaten_pick_target(const npc* np)
{
    if (np == nullptr || human_teams_get_disposition(np, HUMAN_TEAM_EDF) != TEAM_DISPOSITION_ALLIED)
    {
        return nullptr;
    }
    const ai_system* ai = np->get_ai();
    const bool on_turret = human_combat_get_manned_turret(np) != nullptr;
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr)
    {
        return nullptr;
    }
    const gfm_orders_types orders_type = squad->get_squad_orders_type();
    vector patrol_dir;
    if (orders_type == GFM_ORDERS_TYPE_PATROL)
    {
        if (!get_next_patrol_dir(patrol_dir, np))
        {
            return nullptr;
        }
    }
    else if (orders_type != GFM_ORDERS_TYPE_DEFEND && !on_turret)
    {
        return nullptr;
    }
    if (ai->planner.get_current_goal() == threaten_civilian_goal::get_instance())
    {
        human* target = get_attention_human(ai);
        if (target != nullptr && !target->is_killed() && threaten_target_is_valid_internal(np, squad, target, on_turret, orders_type, patrol_dir))
        {
            return target;
        }
    }
    belief_obj_iterator<human> it;
    it.init(ai, AI_BELIEF_UNFRIENDLY);
    while (it)
    {
        human* candidate = it;
        if (threaten_target_is_valid_internal(np, squad, candidate, on_turret, orders_type, patrol_dir))
        {
            return candidate;
        }
        ++it;
    }
    return nullptr;
}

// 0x00643E80 (:787)
human* ai_threaten_pick_watch_target(const npc* np, npc** ally)
{
    if (ally != nullptr)
    {
        *ally = nullptr;
    }
    const object_squad* squad = np->get_ai()->get_squad();
    if (squad == nullptr || squad->get_squad_orders_type() != GFM_ORDERS_TYPE_PATROL)
    {
        return nullptr;
    }
    npc* best_ally = nullptr;
    human* best_target = nullptr;
    float best_dist_sq = FLT_MAX;
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        const unsigned int handle = squad->member_array[i];
        object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC || objp == np)
        {
            continue;
        }
        npc* member = static_cast<npc*>(objp);
        const ai_system* member_ai = member->get_ai();
        const ai_goal* goal = member_ai->planner.get_current_goal();
        if (goal != threaten_civilian_goal::get_instance() && goal != oppress_civilian_goal::get_instance())
        {
            continue;
        }
        human* target = get_attention_human(member_ai);
        if (target == nullptr || target->is_killed())
        {
            continue;
        }
        const float dist_sq = vec_dist_squared(target->pos, np->pos);
        if (dist_sq < best_dist_sq)
        {
            best_dist_sq = dist_sq;
            best_target = target;
            best_ally = member;
        }
    }
    if (best_target != nullptr && ally != nullptr)
    {
        *ally = best_ally;
    }
    return best_target;
}

// 0x00644000 (:852)
human* ai_taunt_unfriendly_pick_target(const npc* np)
{
    if (np == nullptr || human_teams_get_disposition(np, HUMAN_TEAM_EDF) != TEAM_DISPOSITION_ALLIED)
    {
        return nullptr;
    }
    const ai_system* ai = np->get_ai();
    const player* local = player_get_local_player();
    if (local == nullptr || human_combat_get_manned_turret(np) != nullptr)
    {
        return nullptr;
    }
    const gfm_orders_types orders_type = ai->get_orders_type();
    if (orders_type != GFM_ORDERS_TYPE_PATROL && orders_type != GFM_ORDERS_TYPE_DEFEND)
    {
        return nullptr;
    }
    for (const ai_belief* belief = ai->perception.get_first_belief(AI_BELIEF_UNFRIENDLY); belief != nullptr; belief = ai->perception.get_next_belief(belief))
    {
        if ((belief->flags & BF_UNCONFIRMED) != 0)
        {
            continue;
        }
        human* hp = World.object_from_handle<human>(belief->handle);
        if (hp == nullptr || hp->is_killed() || human_in_vehicle(hp) != nullptr || human_is_ragdolled(hp) || hp->cur_stance == HST_PRONE || hp->has_been_acknowledged())
        {
            continue;
        }
        if (vec_dist_squared(np->pos, hp->pos) <= 324.0f && vec_dist_squared(local->pos, hp->pos) <= 1600.0f)
        {
            return hp;
        }
    }
    return nullptr;
}

// 0x00644150 (:914)
human* ai_taunt_enemy_pick_target(const npc* np)
{
    const ai_system* ai = np->get_ai();
    const player* local = player_get_local_player();
    if (local == nullptr || human_combat_get_manned_turret(np) != nullptr)
    {
        return nullptr;
    }
    for (const ai_belief* belief = ai->perception.get_first_belief(AI_BELIEF_FIRST_CONTACT); belief != nullptr; belief = ai->perception.get_next_belief(belief))
    {
        object* objp = belief->handle != 0xFFFFFFFF ? World.object_from_handle(belief->handle) : nullptr;
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN)
        {
            continue;
        }
        human* hp = static_cast<human*>(objp);
        if (hp->is_killed() || human_in_vehicle(hp) != nullptr || human_is_ragdolled(hp) || hp->cur_stance == HST_PRONE)
        {
            continue;
        }
        if (vec_dist_squared(np->pos, hp->pos) <= 900.0f && vec_dist_squared(local->pos, hp->pos) <= 1600.0f)
        {
            return hp;
        }
    }
    return nullptr;
}

// 0x006892F0 (:961)
int confront_civilian_kill_action::get_cost(const npc*, const ai_goal_setup_data& goal_setup_data) const
{
    const object* target = goal_setup_data.selected_target;
    if (goal_setup_data.defense_assignment == AI_DEFENSE_ASSIGNMENT_THREATEN && target != nullptr && target->get_obj_type() == OT_HUMAN)
    {
        const ai_defense_target* info = get_target_info(goal_setup_data.defense_assignment_handle, static_cast<const human*>(target));
        if (info != nullptr && info->flags.acknowledged && AI_DEFENSE_THREATEN_WARNING_SHOT < info->threaten_stage)
        {
            return 100000;
        }
    }
    return -1;
}

// 0x006BF710 (:981)
ai_results confront_civilian_kill_action::do_action(npc* np) const
{
    human* enemy = get_attention_human(np->get_ai());
    if (enemy == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    threaten_go_hostile(np, enemy, VOICE_LINE_NONE);
    return AI_RESULT_SUCCESS;
}

// 0x005F3BA0 (:997)
int confront_civilian_guard_action::get_cost(const npc*, const ai_goal_setup_data& goal_setup_data) const
{
    return goal_setup_data.defense_assignment < AI_DEFENSE_ASSIGNMENT_THREATEN ? 20000 : -1;
}

// 0x006BF7C0 (:1006)
ai_results confront_civilian_guard_action::enter(npc* np, const ai_action* last_action) const
{
    threaten_action_set_attention_mode(np);
    if (last_action != this)
    {
        np->get_ai()->stop_moving();
    }
    return threaten_action_common(np, true, false) ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x006BF830 (:1022)
ai_results confront_civilian_guard_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (AI_DEFENSE_ASSIGNMENT_GUARD < ai_defense_get_assignment(ai->desired_defense_assignment_handle))
    {
        return AI_RESULT_SUCCESS;
    }
    const object* objp = World.object_from_handle(ai->perception.get_attention_handle());
    if (objp != nullptr && objp->get_obj_type() == OT_HUMAN)
    {
        np->steering_vector = vector(objp->pos.x - np->pos.x, objp->pos.y - np->pos.y, objp->pos.z - np->pos.z);
    }
    return threaten_action_common_result(np, true, false);
}

// 0x00689340 (:1045)
int confront_civilian_wait_action::get_cost(const npc*, const ai_goal_setup_data& goal_setup_data) const
{
    const object* target = goal_setup_data.selected_target;
    const human* hp = target != nullptr && target->get_obj_type() == OT_HUMAN ? static_cast<const human*>(target) : nullptr;
    const ai_defense_target* info = get_target_info(goal_setup_data.defense_assignment_handle, hp);
    return info != nullptr && info->flags.acknowledged && info->stage_timer.elapsed() ? 0 : 100;
}

// 0x006BF910 (:1056)
ai_results confront_civilian_wait_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    threaten_action_set_attention_mode(np);
    if (last_action != this)
    {
        ai->stop_moving();
        if (human_combat_get_manned_turret(np) == nullptr && !ai->planner.plan_contains(confront_civilian_kill_action::get_instance()) && !ai->planner.plan_contains(melee_action::get_instance()))
        {
            human_override_default_animation_state(np, true);
            np->char_inst->set_state(index_alias(ANIMATION_STATE_GUARD_AIM, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
        }
    }
    return threaten_action_common(np, false, threaten_stage_ignores_proximity(ai)) ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x006BFA70 (:1082)
ai_results confront_civilian_wait_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const ai_defense_target* info = get_target_info(ai->desired_defense_assignment_handle, nullptr);
    if (info != nullptr && info->flags.acknowledged && info->stage_timer.elapsed())
    {
        return AI_RESULT_SUCCESS;
    }
    return threaten_action_common_result(np, false, threaten_stage_ignores_proximity(ai));
}

// 0x005F3BC0 (:1097)
ai_results confront_civilian_wait_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        human_override_default_animation_state(np, false);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F3BE0 (:1109)
int confront_civilian_accompany_action::get_cost(const npc*, const ai_goal_setup_data& goal_setup_data) const
{
    return goal_setup_data.defense_assignment == AI_DEFENSE_ASSIGNMENT_ACCOMPANY ? 10000 : -1;
}

// 0x006BFB90 (:1118)
ai_results confront_civilian_accompany_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    threaten_action_set_attention_mode(np);
    if (last_action != this)
    {
        npc* threatener = ai_defense_get_threatening_npc(ai->desired_defense_assignment_handle);
        if (threatener == nullptr || threatener == np)
        {
            return AI_RESULT_FAILURE;
        }
        if (ai->follow_in_formation(threatener, AI_FORMATION_FLOCK) == 0xFFFFFFFF)
        {
            ai_defense_report_no_path(ai->desired_defense_assignment_handle);
            return AI_RESULT_FAILURE;
        }
        ai->action_timer.invalidate();
    }
    return threaten_action_common(np, false, threaten_stage_ignores_proximity(ai)) ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x006BFCB0 (:1151)
ai_results confront_civilian_accompany_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const object* objp = World.object_from_handle(ai->perception.get_attention_handle());
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN)
    {
        return AI_RESULT_FAILURE;
    }
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_UNFRIENDLY, objp->get_handle());
    if (belief == nullptr || (belief->flags & BF_UNCONFIRMED) != 0)
    {
        return AI_RESULT_FAILURE;
    }
    const npc* threatener = ai_defense_get_threatening_npc(ai->desired_defense_assignment_handle);
    if (threatener == nullptr || threatener == np)
    {
        return AI_RESULT_FAILURE;
    }
    const unsigned int formation = ai->get_formation_handle();
    if (formation == 0xFFFFFFFF || !ai_formation_is_member(formation, np->get_handle()) || !ai_formation_is_leader(threatener->get_handle(), formation))
    {
        return AI_RESULT_FAILURE;
    }
    if (ai_do_follow_frame(np, formation, HST_STAND))
    {
        ai->action_timer.invalidate();
    }
    else if (ai->action_timer.valid())
    {
        if (ai->action_timer.elapsed())
        {
            ai_defense_report_no_path(ai->desired_defense_assignment_handle);
            return AI_RESULT_FAILURE;
        }
    }
    else
    {
        ai->stop_moving();
        ai->action_timer.set(2000);
    }
    return threaten_action_common_result(np, false, threaten_stage_ignores_proximity(ai));
}

// 0x006BFE10 (:1211)
ai_results confront_civilian_accompany_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->get_ai()->leave_formation();
    }
    return AI_RESULT_SUCCESS;
}

// 0x006BB1C0 (:1229)
int confront_civilian_first_moving_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (human_combat_get_manned_turret(np) != nullptr)
    {
        return -1;
    }
    if (goal_setup_data.defense_assignment == AI_DEFENSE_ASSIGNMENT_THREATEN && !ai_defense_is_anchored(np, goal_setup_data.defense_assignment_handle) && ai_defense_get_threaten_stage(goal_setup_data.defense_assignment_handle, nullptr) == AI_DEFENSE_THREATEN_FIRST_WARNING)
    {
        return 3500;
    }
    return -1;
}

// 0x006BFE30 (:1251)
ai_results confront_civilian_first_moving_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    threaten_action_set_attention_mode(np);
    if (last_action != this)
    {
        ai->stop_moving();
        ai->action_substate = 0;
    }
    return threaten_action_common(np, false, false) ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x006BFEB0 (:1272)
ai_results confront_civilian_first_moving_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human* target = get_attention_human(ai);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_UNFRIENDLY, target->get_handle());
    const bool still_unfriendly = belief != nullptr && (belief->flags & BF_UNCONFIRMED) == 0;
    const float dist_sq = vec_dist_squared(np->pos, target->pos);
    if (still_unfriendly && ai->action_substate == 0 && dist_sq < 225.0f && !ai_defense_should_wait_to_threaten(ai->desired_defense_assignment_handle, target))
    {
        if (!ai_defense_report_threaten_start(ai->desired_defense_assignment_handle))
        {
            return AI_RESULT_FAILURE;
        }
        np->play_voice_line(VOICE_LINE_RESTRICTED_FIRST_WARNING, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        if (!human_combat_is_manning_turret(np))
        {
            np->char_inst->play_action(ANIMATION_ACTION_GUARD_WEAPON_MOTION, ANIM_ALIAS_NONE, 1.0f, 0);
        }
        if (np->get_current_voice_line() == VOICE_LINE_RESTRICTED_FIRST_WARNING)
        {
            ai->action_timer.invalidate();
        }
        else
        {
            ai->action_timer.set(3000);
        }
        ai->action_substate = 1;
    }
    if (ai->action_substate == 1 && (!ai->action_timer.valid() || ai->action_timer.elapsed()) && np->get_current_voice_line_handle() == INVALID_VOICE_LINE_HANDLE)
    {
        ai_defense_report_threaten_done(ai->desired_defense_assignment_handle);
        return AI_RESULT_SUCCESS;
    }
    const human_movement_states m_state = !still_unfriendly || 256.0f <= dist_sq ? HMS_RUN : HMS_WALK;
    if (done_with_goto(np, dist_sq))
    {
        ai->stop_moving();
        if (!still_unfriendly && ai->action_substate == 0)
        {
            ai_defense_report_threaten_failed(ai->desired_defense_assignment_handle);
            return AI_RESULT_FAILURE;
        }
    }
    else if (!ai->is_pathing_to(target->pos, 0.25f, 1.0f))
    {
        const ai_pathfind_results result = ai->pathfind_to(target, m_state, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, nullptr);
        if (result != AI_PF_SUCCESS && AI_PF_RAGDOLLED < result && ai->action_substate == 0)
        {
            ai_defense_report_no_path(ai->desired_defense_assignment_handle);
            if (!still_unfriendly)
            {
                ai_defense_report_threaten_failed(ai->desired_defense_assignment_handle);
                return AI_RESULT_FAILURE;
            }
        }
    }
    if (np->movement_mode == HMM_PATHFINDING)
    {
        human_override_default_animation_state(np, false);
        np->cur_movement_state = m_state;
    }
    else
    {
        human_override_default_animation_state(np, true);
        np->char_inst->set_state(index_alias(ANIMATION_STATE_GUARD_AIM, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
    }
    return threaten_action_common_result(np, false, false);
}

// 0x00689390 (:1375)
ai_results confront_civilian_first_moving_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        human_override_default_animation_state(np, false);
        const unsigned int def_handle = np->get_ai()->desired_defense_assignment_handle;
        if (ai_defense_get_threaten_stage(def_handle, nullptr) == AI_DEFENSE_THREATEN_FIRST_WARNING)
        {
            ai_defense_report_threaten_failed(def_handle);
            return AI_RESULT_FAILURE;
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x00689430 (:1400)
int confront_civilian_first_warning_action::get_cost(const npc*, const ai_goal_setup_data& goal_setup_data) const
{
    return goal_setup_data.defense_assignment == AI_DEFENSE_ASSIGNMENT_THREATEN && ai_defense_get_threaten_stage(goal_setup_data.defense_assignment_handle, nullptr) == AI_DEFENSE_THREATEN_FIRST_WARNING ? 4000 : -1;
}

// 0x006C01F0 (:1414)
ai_results confront_civilian_first_warning_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    threaten_action_set_attention_mode(np);
    if (last_action != this)
    {
        if (!ai_defense_report_threaten_start(ai->desired_defense_assignment_handle))
        {
            return AI_RESULT_FAILURE;
        }
        ai->stop_moving();
        np->play_voice_line(VOICE_LINE_RESTRICTED_FIRST_WARNING, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        if (human_combat_get_manned_turret(np) == nullptr)
        {
            human_override_default_animation_state(np, true);
            np->char_inst->set_state(index_alias(ANIMATION_STATE_GUARD_AIM, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
            np->char_inst->play_action(ANIMATION_ACTION_GUARD_WEAPON_MOTION, ANIM_ALIAS_NONE, 1.0f, 0);
            ai->planner.lock_action(true);
        }
        if (np->get_current_voice_line() == VOICE_LINE_RESTRICTED_FIRST_WARNING)
        {
            ai->action_timer.invalidate();
        }
        else
        {
            ai->action_timer.set(3000);
        }
    }
    return threaten_action_common(np, false, false) ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x006C0330 (:1454)
ai_results confront_civilian_first_warning_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if ((!ai->action_timer.valid() || ai->action_timer.elapsed()) && np->get_current_voice_line_handle() == INVALID_VOICE_LINE_HANDLE)
    {
        ai_defense_report_threaten_done(ai->desired_defense_assignment_handle);
        return AI_RESULT_SUCCESS;
    }
    const object* objp = World.object_from_handle(ai->perception.get_attention_handle());
    if (objp != nullptr && objp->get_obj_type() == OT_HUMAN)
    {
        np->steering_vector = vector(objp->pos.x - np->pos.x, objp->pos.y - np->pos.y, objp->pos.z - np->pos.z);
    }
    return threaten_action_common_result(np, false, false);
}

// 0x006894C0 (:1475)
ai_results confront_civilian_first_warning_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        human_override_default_animation_state(np, false);
        const unsigned int def_handle = np->get_ai()->desired_defense_assignment_handle;
        if (ai_defense_get_threaten_stage(def_handle, nullptr) == AI_DEFENSE_THREATEN_FIRST_WARNING)
        {
            ai_defense_report_threaten_failed(def_handle);
            return AI_RESULT_FAILURE;
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x00689560 (:1598)
int confront_civilian_warning_shot_action::get_cost(const npc*, const ai_goal_setup_data& goal_setup_data) const
{
    return goal_setup_data.defense_assignment == AI_DEFENSE_ASSIGNMENT_THREATEN && ai_defense_get_threaten_stage(goal_setup_data.defense_assignment_handle, nullptr) == AI_DEFENSE_THREATEN_WARNING_SHOT ? 6000 : -1;
}

// 0x006C0440 (:1612)
ai_results confront_civilian_warning_shot_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    threaten_action_set_attention_mode(np);
    if (last_action != this)
    {
        if (!ai_defense_report_threaten_start(ai->desired_defense_assignment_handle))
        {
            return AI_RESULT_FAILURE;
        }
        ai->stop_moving();
        if (human_combat_get_manned_turret(np) == nullptr)
        {
            ai->action_substate = 0;
            np->char_inst->play_action(ANIMATION_ACTION_GUARD_WARNING_SHOT, ANIM_ALIAS_NONE, 1.0f, 0);
        }
        else
        {
            ai->action_substate = 2;
            np->play_voice_line(VOICE_LINE_RESTRICTED_WARNING_SHOT, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
            if (np->get_current_voice_line() == VOICE_LINE_RESTRICTED_WARNING_SHOT)
            {
                ai->action_timer.invalidate();
            }
            else
            {
                ai->action_timer.set(3000);
            }
        }
        ai->planner.lock_action(true);
    }
    return threaten_action_common(np, false, false) ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x006C0560 (:1660)
ai_results confront_civilian_warning_shot_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (ai->action_substate == 0)
    {
        if (vanim_has_trigger_just_elapsed(np->char_inst->vp, Trigger_warning_shot, ANIMATION_ACTION_GUARD_WARNING_SHOT, -1))
        {
            ai->action_substate = 1;
        }
        else if (!np->char_inst->is_playing_action(ANIMATION_ACTION_GUARD_WARNING_SHOT))
        {
            ai->action_substate = 2;
        }
    }
    if (ai->action_substate == 1)
    {
        weapon* wp = human_weapons_get_weapon(np);
        if (wp == nullptr)
        {
            return AI_RESULT_FAILURE;
        }
        if (wp->get_rounds_in_magazine() < 1)
        {
            wp->set_rounds_in_magazine(1);
        }
        if (wp->get_num_rounds_fired() < 1)
        {
            ai->flags.fire_weapon_this_frame = true;
        }
        else
        {
            ai->action_substate = 2;
            ai->set_desired_burst_size(0);
            np->play_voice_line(VOICE_LINE_RESTRICTED_WARNING_SHOT, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
            if (np->get_current_voice_line() == VOICE_LINE_RESTRICTED_WARNING_SHOT)
            {
                ai->action_timer.invalidate();
            }
            else
            {
                ai->action_timer.set(3000);
            }
        }
    }
    if (ai->action_substate == 2 && (!ai->action_timer.valid() || ai->action_timer.elapsed()) && np->get_current_voice_line_handle() == INVALID_VOICE_LINE_HANDLE)
    {
        ai_defense_report_threaten_done(ai->desired_defense_assignment_handle);
        return AI_RESULT_SUCCESS;
    }
    return threaten_action_common_result(np, false, false);
}

// 0x006895F0 (:1714)
ai_results confront_civilian_warning_shot_action::cleanup(npc* np, const ai_action* next_action) const
{
    const unsigned int def_handle = np->get_ai()->desired_defense_assignment_handle;
    if (next_action != this && ai_defense_get_threaten_stage(def_handle, nullptr) == AI_DEFENSE_THREATEN_WARNING_SHOT)
    {
        ai_defense_report_threaten_failed(def_handle);
        return AI_RESULT_FAILURE;
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F3C00 (:1736)
int confront_civilian_dont_goto_rule::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 3000;
}

// 0x006BB280 (:1741)
int confront_civilian_goto_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (npc_is_anchored(np))
    {
        return -1;
    }
    const unsigned int def_handle = goal_setup_data.defense_assignment_handle;
    const ai_defense_defender* defender = Ai_defense_object_manager.defender_from_handle(def_handle);
    if (defender != nullptr && defender->flags.no_path)
    {
        return -1;
    }
    const ai_defense_target* info = get_target_info(def_handle, nullptr);
    const object* target = goal_setup_data.selected_target;
    if (info == nullptr || !info->flags.acknowledged || AI_DEFENSE_THREATEN_DASH_AND_BASH <= info->threaten_stage || target == nullptr || target->get_obj_type() != OT_HUMAN)
    {
        return -1;
    }
    if (done_with_goto(np, vec_dist_squared(np->pos, target->pos)) && np->get_ai()->perception.unfriendly_in_los(target->get_handle()))
    {
        return 0;
    }
    return 1000;
}

// 0x006C06F0 (:1770)
ai_results confront_civilian_goto_action::enter(npc* np, const ai_action* last_action) const
{
    threaten_action_set_attention_mode(np);
    if (last_action != this)
    {
        np->get_ai()->stop_moving();
    }
    return threaten_action_common(np, false, false) ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x006C0760 (:1788)
ai_results confront_civilian_goto_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human* target = get_attention_human(ai);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_UNFRIENDLY, target->get_handle());
    const bool still_unfriendly = belief != nullptr && (belief->flags & BF_UNCONFIRMED) == 0;
    const float dist_sq = vec_dist_squared(np->pos, target->pos);
    if (done_with_goto(np, dist_sq))
    {
        if (still_unfriendly)
        {
            return AI_RESULT_SUCCESS;
        }
    }
    else
    {
        const ai_defense_threaten_stages stage = ai_defense_get_threaten_stage(ai->desired_defense_assignment_handle, target);
        const human_movement_states m_state = still_unfriendly && dist_sq < 256.0f && stage < AI_DEFENSE_THREATEN_WARNING_SHOT ? HMS_WALK : HMS_RUN;
        bool pathing = ai->is_pathing_to(target->pos, 0.25f, 1.0f);
        if (!pathing)
        {
            const ai_pathfind_results result = ai->pathfind_to(target, m_state, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, nullptr);
            pathing = result == AI_PF_SUCCESS || result <= AI_PF_RAGDOLLED;
        }
        if (pathing)
        {
            if (np->movement_mode == HMM_PATHFINDING)
            {
                np->cur_movement_state = m_state;
            }
            return threaten_action_common_result(np, false, false);
        }
        ai_defense_report_no_path(ai->desired_defense_assignment_handle);
        if (ai->perception.unfriendly_in_los(target->get_handle()))
        {
            return AI_RESULT_SUCCESS;
        }
    }
    ai_defense_report_threaten_failed(ai->desired_defense_assignment_handle);
    return AI_RESULT_FAILURE;
}

// 0x005F3C10 (:1857)
int threaten_civilian_from_turret_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return human_combat_get_manned_turret(np) != nullptr ? 1000 : -1;
}

// 0x006C0950 (:1868)
ai_results threaten_civilian_from_turret_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human* target = get_attention_human(ai);
    if (target == nullptr || target->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    turret* tp = human_combat_get_manned_turret(np);
    if (tp == nullptr || !ai_threaten_target_is_valid(np, target))
    {
        return AI_RESULT_FAILURE;
    }
    vector aim_pos;
    if (!ai->perception.get_effective_target_pos(aim_pos, target, nullptr, nullptr, false))
    {
        return AI_RESULT_FAILURE;
    }
    tp->aim_turret(aim_pos);
    const float dist_sq = vec_xz_dist_squared(tp->pos, target->pos);
    if (dist_sq < 9.0f)
    {
        threaten_go_hostile(np, target, VOICE_LINE_NONE);
        return AI_RESULT_SUCCESS;
    }
    if (!target->has_been_acknowledged() && dist_sq < 49.0f)
    {
        np->play_voice_line(np->npc_flags.activity_ha ? VOICE_LINE_HA_EDF_SPOT_PLAYER : VOICE_LINE_RESTRICTED_SPOT, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        alert_shout_threaten alert(np, target);
        npc_sound_info::queue(alert);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005F3C30 (:1912)
int threaten_civilian_default_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 2000;
}

// 0x005F3C40 (:1917)
ai_results threaten_civilian_default_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        ai_system* ai = np->get_ai();
        ai->action_timer.invalidate();
        ai->action_substate = 0;
        ai->stop_moving();
    }
    return AI_RESULT_SUCCESS;
}

// 0x00689680 (:1935)
ai_results threaten_civilian_default_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human* target = get_attention_human(ai);
    if (target == nullptr || target->is_killed() || !ai_threaten_target_is_valid(np, target))
    {
        return AI_RESULT_FAILURE;
    }
    if (!target->has_been_acknowledged())
    {
        if (ai->action_substate == 0)
        {
            ai->stop_moving();
            vector dir(target->pos.x - np->pos.x, target->pos.y - np->pos.y, target->pos.z - np->pos.z);
            dir.flatten_normalize();
            np->steering_vector = dir;
            if (FACING_TARGET_DOT_THREATEN_ACTIONS <= np->orient.fvec.y * dir.y + np->orient.fvec.x * dir.x + np->orient.fvec.z * dir.z)
            {
                ai->action_substate = 1;
            }
        }
        else if (ai->action_substate == 1)
        {
            ai->perception.set_attention_mode(ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
            human_set_stance(np, HST_STAND);
            np->char_inst->play_action(ANIMATION_ACTION_MOTION_TO_MOVE, ANIM_ALIAS_NONE, 1.0f, 0);
            np->play_voice_line(VOICE_LINE_PATROL_REQUEST_MOVE, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
            np->block_steering_vector = np->steering_vector;
            np->block_movement_anim = ANIMATION_ACTION_MOTION_TO_MOVE;
            np->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
            alert_shout_threaten alert(np, target);
            npc_sound_info::queue(alert);
        }
    }
    if (target->has_been_acknowledged() && !ai->action_timer.valid())
    {
        ai->action_timer.set(target->get_sub_type() == OT_SUB_HUMAN_NPC ? 3000 : 5000);
    }
    if (ai->action_timer.valid() && ai->action_timer.elapsed())
    {
        ai->perception.start_beatdown(target);
        return AI_RESULT_SUCCESS;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005F3C80 (:1990)
int threaten_civilian_watch_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 2000;
}

// 0x00689970 (:1995)
ai_results threaten_civilian_watch_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        ai->stop_moving();
        human_set_stance(np, HST_STAND);
    }
    object* target = ai->desired_target != 0xFFFFFFFF ? World.object_from_handle(ai->desired_target) : nullptr;
    if (target != nullptr && target->get_obj_type() != OT_HUMAN)
    {
        target = nullptr;
    }
    npc* ally = nullptr;
    if (target != nullptr)
    {
        ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        const object_squad* squad = ai->get_squad();
        if (squad != nullptr)
        {
            for (int i = 0; i < squad->member_array.size(); i++)
            {
                const unsigned int handle = squad->member_array[i];
                object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
                if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC || objp == np)
                {
                    continue;
                }
                npc* member = static_cast<npc*>(objp);
                if (member->get_ai()->oppression_action != AI_OPPRESSION_NONE || member->get_ai()->planner.get_current_action() == threaten_civilian_default_action::get_instance())
                {
                    ally = member;
                    break;
                }
            }
        }
    }
    vector& dest = ai->desired_destination;
    dest = ZERO_VECTOR;
    ai->desired_goal_nav_cell_hint_handle = 0xFFFFFFFFFFFFFFFFull;
    if (ally == nullptr || target == nullptr)
    {
        return AI_RESULT_SUCCESS;
    }
    // Keeps the npc out of the line from the target to its ally. The radius comes from how far beyond the ends of the line the npc is.
    vector line(ally->pos.x - target->pos.x, ally->pos.y - target->pos.y, ally->pos.z - target->pos.z);
    const float np_dx = np->pos.x - target->pos.x;
    const float np_dz = np->pos.z - target->pos.z;
    const float length = line.flatten_mag_and_normalize();
    const float proj = np_dx * line.x + line.y * 0.0f + np_dz * line.z;
    float outside = -proj;
    if (-proj - (proj - length) < 0.0f)
    {
        outside = proj - length;
    }
    const float min_radius = proj <= 0.0f ? 0.0f : 2.0f;
    const float radius = fl_parameter(outside, min_radius, 4.0f, 4.0f, min_radius, true);
    if (radius <= 0.0f)
    {
        return AI_RESULT_SUCCESS;
    }
    const vector closest(target->pos.x + line.x * proj, target->pos.y + line.y * proj, target->pos.z + line.z * proj);
    if (radius * radius <= vec_xz_dist_squared(np->pos, closest))
    {
        return AI_RESULT_SUCCESS;
    }
    const vector side(line.y * UP_VECTOR.z - line.z * UP_VECTOR.y, line.z * UP_VECTOR.x - line.x * UP_VECTOR.z, line.x * UP_VECTOR.y - line.y * UP_VECTOR.x);
    const vector plus(closest.x + side.x * radius, closest.y + side.y * radius, closest.z + side.z * radius);
    const vector minus(closest.x - side.x * radius, closest.y - side.y * radius, closest.z - side.z * radius);
    const bool on_minus_side = (np->pos.x - closest.x) * side.x + (np->pos.y - closest.y) * side.y + (np->pos.z - closest.z) * side.z <= 0.0f;
    nav_cell* cell = nullptr;
    dest = on_minus_side ? minus : plus;
    if (!bb_pf_find_closest_point_on_nav_mesh_fast(dest, cell, dest, nullptr, nullptr, false, 2.0f))
    {
        dest = on_minus_side ? plus : minus;
        if (!bb_pf_find_closest_point_on_nav_mesh_fast(dest, cell, dest, nullptr, nullptr, false, 2.0f))
        {
            dest = ZERO_VECTOR;
        }
    }
    if ((dest.x != ZERO_VECTOR.x || dest.y != ZERO_VECTOR.y || dest.z != ZERO_VECTOR.z) && cell != nullptr)
    {
        ai->desired_goal_nav_cell_hint_handle = nav_cell_get_handle(cell);
    }
    return AI_RESULT_SUCCESS;
}

// 0x00644250 (:2099)
ai_results threaten_civilian_watch_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    vector& dest = ai->desired_destination;
    if (dest.x == ZERO_VECTOR.x && dest.y == ZERO_VECTOR.y && dest.z == ZERO_VECTOR.z)
    {
        ai->stop_moving();
    }
    else if (vec_xz_dist_squared(dest, np->pos) <= 0.25f)
    {
        dest = ZERO_VECTOR;
    }
    else
    {
        // Pushes at 0x006442E4: no navpoint, walking, standing, smoothing disabled.
        ai->set_direct_movement_to_pos(dest, ai->desired_goal_nav_cell_hint_handle, HMS_WALK, HST_STAND, true);
    }
    human_set_combat_ready(np, true, int_rand_range(0, 500));
    return AI_RESULT_IN_PROGRESS;
}

// 0x005F3C90 (:2122)
int taunt_player_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    // Original quirk kept: no null check of the local player.
    if (player_get_local_player()->current_team == np->current_team)
    {
        return -1;
    }
    if (np->get_ai()->planner.get_current_action() == pedestrian_follow_player_action::get_instance() && np->movement_mode == HMM_PATHFINDING)
    {
        my_data.action_anim = ANIMATION_ACTION_WALKING_TAUNT;
        return 1000;
    }
    my_data.action_anim = 50 < vrand() % 100 ? ANIMATION_ACTION_TAUNT_ALT : ANIMATION_ACTION_TAUNT;
    return 1000;
}

// 0x005F3D20 (:2141)
void taunt_player_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    np->get_ai()->desired_anim_action = setup_data.action_anim;
}

// 0x00644340 (:2147)
ai_results taunt_player_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const player* local = player_get_local_player();
    if (local == nullptr || local->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        const animation_action anim = ai->desired_anim_action;
        ai->perception.set_attention_mode(ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
        np->char_inst->play_action(anim, ANIM_ALIAS_TAUNT, 1.0f, 0);
        vector facing;
        if (anim == ANIMATION_ACTION_WALKING_TAUNT && np->movement_mode == HMM_PATHFINDING)
        {
            facing = np->steering_vector;
        }
        else
        {
            const vector to_player(local->pos.x - np->pos.x, local->pos.y - np->pos.y, local->pos.z - np->pos.z);
            facing = to_player.copy_flatten_normalize_safe();
        }
        matrix orient;
        orient.make_quick(facing);
        np->char_inst->set_action_starting_orient(np->pos, np->orient, np->pos, orient, ANIM_ALIAS_TAUNT, -1.0f);
        ai->planner.lock_action(true);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F3D40 (:2177)
ai_results taunt_player_action::do_action(npc* np) const
{
    return np->char_inst->is_playing_action(ANIM_ALIAS_TAUNT) ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x00644510 (:2186)
ai_results taunt_player_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this && np->char_inst->rdd.is_loaded())
    {
        np->char_inst->stop_action(ANIM_ALIAS_TAUNT, 10, true);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F3D70 (:2200)
int taunt_civilian_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const human_voice_persona* voice = np->voice;
    return voice != nullptr && voice->can_play_line(VOICE_LINE_PATROL_TAUNT_CIVILIAN, 0, nullptr) ? 2000 : -1;
}

// 0x0066E5F0 (:2210)
ai_results taunt_civilian_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    object* target = World.object_from_handle(ai->perception.get_attention_handle());
    if (target == nullptr || target->get_obj_type() != OT_HUMAN || target->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        np->play_voice_line(VOICE_LINE_PATROL_TAUNT_CIVILIAN, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        if (np->get_current_voice_line() != VOICE_LINE_PATROL_TAUNT_CIVILIAN)
        {
            return AI_RESULT_FAILURE;
        }
        ai->perception.set_attention_mode(ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        np->char_inst->play_action(ANIMATION_ACTION_TAUNT, ANIM_ALIAS_TAUNT, 1.0f, 0);
        if (target->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            static_cast<npc*>(target)->get_ai()->perception.register_taunt(np);
        }
        ai->planner.lock_action(true);
        ai->action_timer.invalidate();
    }
    return AI_RESULT_SUCCESS;
}

// 0x00644540 (:2241)
ai_results taunt_civilian_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const object* target = World.object_from_handle(ai->perception.get_attention_handle());
    if (target == nullptr || target->get_obj_type() != OT_HUMAN || target->is_killed() || ai->perception.get_first_belief(AI_BELIEF_ENEMY_EXISTS) != nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (ai->action_timer.valid() && ai->action_timer.elapsed())
    {
        return AI_RESULT_SUCCESS;
    }
    if (np->get_current_voice_line() != VOICE_LINE_PATROL_TAUNT_CIVILIAN && !ai->action_timer.valid())
    {
        ai->action_timer.set(400);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00644600 (:2268)
ai_results taunt_civilian_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        if (np->get_current_voice_line() == VOICE_LINE_PATROL_TAUNT_CIVILIAN)
        {
            np->stop_voice(VOICE_LINE_PRIORITY_HIGH);
        }
        if (np->char_inst->rdd.is_loaded())
        {
            np->char_inst->stop_action(ANIM_ALIAS_TAUNT, 10, true);
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F3DA0 (:2287)
int taunt_enemy_running_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if (np->cur_movement_state != HMS_RUN)
    {
        return -1;
    }
    const human* target = human_combat_get_target(np);
    return target != nullptr && 25.0f <= vec_dist_squared(np->pos, target->pos) ? 1000 : -1;
}

// 0x00644650 (:2302)
ai_results taunt_enemy_running_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    human* target = get_attention_human(ai);
    if (target == nullptr || target->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        np->play_voice_line(human_in_vehicle(target) == nullptr ? VOICE_LINE_COMBAT_TAUNT_ENEMY : VOICE_LINE_COMBAT_TAUNT_ENEMY_VEHICLE, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        ai->perception.set_attention_mode(ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        np->char_inst->play_action(ANIMATION_ACTION_COMBAT_RUN_CHEER, ANIM_ALIAS_TAUNT, 1.0f, 0);
        ai->planner.lock_action(true);
        ai->action_timer.invalidate();
    }
    return AI_RESULT_SUCCESS;
}

// 0x00644790 (:2329)
ai_results taunt_enemy_running_action::do_action(npc* np) const
{
    const object* target = World.object_from_handle(np->get_ai()->perception.get_attention_handle());
    if (target == nullptr || target->get_obj_type() != OT_HUMAN || target->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    return np->char_inst->is_playing_action(ANIM_ALIAS_TAUNT) ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x00644800 (:2356)
ai_results taunt_enemy_running_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this && np->char_inst->rdd.is_loaded())
    {
        np->char_inst->stop_action(ANIM_ALIAS_TAUNT, 5, true);
    }
    return AI_RESULT_SUCCESS;
}

// 0x006BB3E0 (:2375)
int confront_civilian_beatdown_rule::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    const object* target = goal_setup_data.selected_target;
    if (target == nullptr || target->get_obj_type() != OT_HUMAN || target->is_killed() || ai_defense_is_anchored(np, np->get_ai()->desired_defense_assignment_handle) || goal_setup_data.defense_assignment != AI_DEFENSE_ASSIGNMENT_THREATEN)
    {
        return -1;
    }
    const float dist_sq = vec_dist_squared(np->pos, target->pos);
    const unsigned int def_handle = goal_setup_data.defense_assignment_handle;
    const ai_defense_threaten_stages stage = ai_defense_get_threaten_stage(def_handle, nullptr);
    if (stage == AI_DEFENSE_THREATEN_KILL)
    {
        if (9.0f < dist_sq || !ai_defense_should_wait_to_threaten(def_handle, nullptr))
        {
            return -1;
        }
    }
    else if (stage != AI_DEFENSE_THREATEN_DASH_AND_BASH)
    {
        return -1;
    }
    if (225.0f < dist_sq)
    {
        return -1;
    }
    return restricted_area_in_any_region(target->pos, true) ? 10 : -1;
}

// 0x005F3DF0 (:2426). LTCG: target EDI, np on the stack.
static const human_melee_combat_move_data* pick_beat_civilian_combat_move(const npc* np, const human* target)
{
    const weapon* wp = np->get_equipped_weapon();
    if (wp == nullptr)
    {
        return nullptr;
    }
    const char* name;
    if (human_is_ragdolled(target) || target->cur_stance == HST_PRONE)
    {
        name = wp->w_info->weapon_anim_group == ANIMATION_GROUP_REVOLVER || vrand() % 100 < 51 ? "ar_kick" : "ar_stomp";
    }
    else
    {
        name = object_is_player(target) || wp->w_info->weapon_anim_group == ANIMATION_GROUP_REVOLVER || int_rand_chance(50) ? "ar_melee_beat" : "ar_melee_shove";
    }
    const human_melee_combat_move_id move_id = human_combat_lookup_combat_move(name);
    return move_id != INVALID_MELEE_COMBAT_MOVE_ID ? human_combat_move_data_from_index(np, move_id) : nullptr;
}

// 0x00644830 (:2449)
int beat_civilian_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (goal_setup_data.oppression_action != AI_OPPRESSION_BEATDOWN)
    {
        return -1;
    }
    const object* target = goal_setup_data.selected_target;
    if (target == nullptr || target->get_obj_type() != OT_HUMAN || target->is_killed() || human_in_vehicle(static_cast<const human*>(target)) != nullptr)
    {
        return -1;
    }
    my_data.melee_move = pick_beat_civilian_combat_move(np, static_cast<const human*>(target));
    return my_data.melee_move != nullptr ? 500 : -1;
}

// 0x005F3EA0 (:2469)
void beat_civilian_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    np->get_ai()->desired_melee_move = setup_data.melee_move;
}

// 0x0068A1B0 (:2475)
ai_results beat_civilian_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    ai->stop_moving();
    human_set_stance(np, HST_STAND);
    human* target = get_attention_human(ai);
    if (target == nullptr || target->is_killed() || human_in_vehicle(target) != nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    const vector to_target(target->pos.x - np->pos.x, 0.0f, target->pos.z - np->pos.z);
    human_combat_play_combat_move(np, ai->desired_melee_move, false, true);
    matrix orient;
    orient.make(to_target);
    np->char_inst->set_action_starting_orient(np->pos, np->orient, np->pos, orient, ANIM_ALIAS_COMBAT_MOVE, -1.0f);
    if (human_is_ragdolled(target) || target->cur_stance == HST_PRONE)
    {
        np->play_voice_line(VOICE_LINE_PATROL_STOMP_CIVILIAN, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    }
    if (ai->desired_melee_move->flags.npc_knockback)
    {
        np->combat.melee_attack_interval_timer.set_rand(2500, 3000);
    }
    else
    {
        np->combat.melee_attack_interval_timer.set_rand(1000, 1500);
    }
    np->combat.move_to_melee_timestamp.invalidate();
    ai->planner.lock_action(true);
    return AI_RESULT_SUCCESS;
}

// 0x006448A0 (:2521)
ai_results beat_civilian_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human* target = get_attention_human(ai);
    if (target == nullptr || human_in_vehicle(target) != nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (np->char_inst->is_playing_action(ANIM_ALIAS_COMBAT_MOVE))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    const weapon* wp = human_weapons_get_weapon(np);
    if (wp != nullptr && wp->char_inst != nullptr && wp->char_inst->is_playing_action(ANIM_ALIAS_COMBAT_MOVE))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (object_is_player(target) || (!human_is_ragdolled(target) && target->cur_stance != HST_PRONE))
    {
        return AI_RESULT_SUCCESS;
    }
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr)
    {
        return AI_RESULT_SUCCESS;
    }
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        const unsigned int handle = squad->member_array[i];
        object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC || objp == np)
        {
            continue;
        }
        npc* member = static_cast<npc*>(objp);
        if (member->get_ai()->perception.get_belief(AI_BELIEF_UNFRIENDLY, target) == nullptr)
        {
            continue;
        }
        const play_voice_status status = member->play_voice_line(VOICE_LINE_PATROL_BEAT_CIVILIAN_EDF_RESPONSE, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        if (status == PLAY_VOICE_SUCCESS || status == PLAY_VOICE_FAILURE_CHANCE)
        {
            break;
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F3EC0 (:2571). LTCG: target ECX, the result in EAX. np is dropped by LTCG.
static vector get_execution_position(const human* target)
{
    const vector& fvec = target->orient.fvec;
    const vector& rvec = target->orient.rvec;
    return vector(target->pos.x - fvec.x * EXECUTION_FORWARD_OFFSET + rvec.x * EXECUTION_RIGHT_OFFSET, target->pos.y - fvec.y * EXECUTION_FORWARD_OFFSET + rvec.y * EXECUTION_RIGHT_OFFSET, target->pos.z - fvec.z * EXECUTION_FORWARD_OFFSET + rvec.z * EXECUTION_RIGHT_OFFSET);
}

// 0x006449F0 (:2586)
int execute_civilian_action::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (goal_setup_data.oppression_action != AI_OPPRESSION_EXECUTE)
    {
        return -1;
    }
    const object* target = goal_setup_data.selected_target;
    if (target == nullptr || target->get_obj_type() != OT_HUMAN || target->is_killed() || human_in_vehicle(static_cast<const human*>(target)) != nullptr)
    {
        return -1;
    }
    if (MAX_OPPRESS_DIST_SQ <= vec_xz_dist_squared(np->pos, target->pos))
    {
        return -1;
    }
    if (INTERACTION_POSITION_DIST_SQ <= vec_xz_dist_squared(np->pos, get_execution_position(static_cast<const human*>(target))))
    {
        desired_conditions |= ai_plan_conditions(28);
    }
    return 50;
}

// 0x0068A350 (:2612)
ai_results execute_civilian_action::enter(npc* np, const ai_action* prev_action) const
{
    ai_system* ai = np->get_ai();
    human* target = get_desired_civilian(ai);
    if (target == nullptr || target->is_killed() || human_in_vehicle(target) != nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai->stop_moving();
    human_set_stance(np, HST_STAND);
    const vector to_target(target->pos.x - np->pos.x, target->pos.y - np->pos.y, target->pos.z - np->pos.z);
    vector dir;
    dir = to_target.copy_flatten_normalize_safe();
    np->steering_vector = dir;
    ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    if (prev_action != this)
    {
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        human_override_default_animation_state(np, true);
        np->char_inst->set_state(index_alias(ANIMATION_STATE_AMB_EXECUTE_STAND, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
        ai->action_state = 0;
        ai->action_timer.set(1500);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F3FD0 (:2650)
void execute_civilian_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x006C0A90 (:2655)
ai_results execute_civilian_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human* target = get_desired_civilian(ai);
    if (target == nullptr || human_in_vehicle(target) != nullptr || MAX_OPPRESS_DIST_SQ <= vec_xz_dist_squared(np->pos, target->pos))
    {
        return AI_RESULT_FAILURE;
    }
    if (!target->is_killed() && target->char_inst->get_animation_state() != ANIMATION_STATE_AMB_COWER_CROUCH)
    {
        threaten_note_enemy(ai->perception, target, true);
        return AI_RESULT_FAILURE;
    }
    if (ai->action_state == 0)
    {
        np->play_voice_line(VOICE_LINE_EXECUTE, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        ai->action_state = 1;
    }
    if (!ai->action_timer.elapsed() || np->get_current_voice_line_handle() != INVALID_VOICE_LINE_HANDLE)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (ai->action_state == 1)
    {
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        np->char_inst->play_action(ANIMATION_ACTION_EXECUTE_FIRE, ANIM_ALIAS_SCRIPTED, 1.0f, 0);
        weapon* wp = human_weapons_get_weapon(np);
        if (wp == nullptr)
        {
            return AI_RESULT_FAILURE;
        }
        if (wp->get_rounds_in_magazine() < 1)
        {
            wp->set_rounds_in_magazine(1);
        }
        ai->flags.fire_weapon_this_frame = true;
        ai->action_state = 2;
        human_ragdoll_on_impact(target, 10.0f, 0.1f, false);
        const vector from_target(np->pos.x - target->pos.x, np->pos.y - target->pos.y, np->pos.z - target->pos.z);
        const animation_action death = from_target * target->orient.fvec <= 0.0f ? ANIMATION_ACTION_COWER_DEATH : ANIMATION_ACTION_CROUCH_DEATH_HEAD_FORWARD;
        target->char_inst->play_action(death, ANIM_ALIAS_DEATH, 1.0f, 0);
        human_damage(target, np, 100000.0f, DBT_BULLET, nullptr, nullptr, true, false);
        ai->planner.lock_action(true);
    }
    else if (ai->action_state == 2)
    {
        if (!np->char_inst->is_playing_action(ANIMATION_ACTION_EXECUTE_FIRE))
        {
            ai->action_timer.set_rand(3000, 5000);
            ai->action_state = 3;
            return AI_RESULT_IN_PROGRESS;
        }
        ai->set_desired_burst_size(0);
        human_override_default_animation_state(np, false);
    }
    else if (ai->action_state == 3 && ai->action_timer.elapsed())
    {
        return AI_RESULT_SUCCESS;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00644C10 (:2765)
ai_results execute_civilian_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        ai_system* ai = np->get_ai();
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        human_override_default_animation_state(np, false);
        human* target = get_desired_civilian(ai);
        if (target != nullptr)
        {
            static_cast<npc*>(target)->get_ai()->oppression_action = AI_OPPRESSION_NONE;
        }
        ai->oppression_action = AI_OPPRESSION_NONE;
    }
    return AI_RESULT_SUCCESS;
}

// 0x00644CA0 (:2787)
int move_to_execution_position_action::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (goal_setup_data.oppression_action != AI_OPPRESSION_EXECUTE)
    {
        return -1;
    }
    const object* target = goal_setup_data.selected_target;
    if (target == nullptr || target->get_obj_type() != OT_HUMAN)
    {
        return -1;
    }
    const human* hp = static_cast<const human*>(target);
    if (human_in_vehicle(hp) != nullptr || MAX_OPPRESS_DIST_SQ <= vec_xz_dist_squared(np->pos, hp->pos))
    {
        return -1;
    }
    if (hp->char_inst->get_animation_state() != ANIMATION_STATE_AMB_COWER_CROUCH || hp->char_inst->is_playing_action(AMIMATION_ACTION_COWER_TURN))
    {
        desired_conditions |= ai_plan_conditions(29);
    }
    return 100;
}

// 0x005F3FE0 (:2814)
void move_to_execution_position_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x0068A530 (:2819)
ai_results move_to_execution_position_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        ai_system* ai = np->get_ai();
        ai->action_timer.invalidate();
        human_set_stance(np, HST_STAND);
        const human* target = get_desired_civilian(ai);
        if (target != nullptr)
        {
            ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x006C0E60 (:2837)
ai_results move_to_execution_position_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human* target = get_desired_civilian(ai);
    if (target == nullptr || human_in_vehicle(target) != nullptr || MAX_OPPRESS_DIST_SQ <= vec_xz_dist_squared(np->pos, target->pos))
    {
        return AI_RESULT_FAILURE;
    }
    if (target->char_inst->get_animation_state() != ANIMATION_STATE_AMB_COWER_CROUCH)
    {
        threaten_note_enemy(ai->perception, target, true);
        return AI_RESULT_FAILURE;
    }
    const vector pos = get_execution_position(target);
    if (vec_xz_dist_squared(np->pos, pos) < INTERACTION_POSITION_DIST_SQ)
    {
        return align_to_interaction_position(np, static_cast<npc*>(target), pos);
    }
    const ai_pathfind_results result = ai->pathfind_to(pos, HMS_WALK, HST_CURRENT, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false);
    return ai_pf_to_result(result) != AI_RESULT_FAILURE ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x00644DB0 (:2877)
ai_results move_to_execution_position_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this && next_action != execute_civilian_action::get_instance())
    {
        ai_system* ai = np->get_ai();
        human* target = get_desired_civilian(ai);
        if (target != nullptr)
        {
            static_cast<npc*>(target)->get_ai()->oppression_action = AI_OPPRESSION_NONE;
        }
        ai->oppression_action = AI_OPPRESSION_NONE;
    }
    return AI_RESULT_SUCCESS;
}

// 0x00644E20 (:2898)
int order_civilian_to_knees_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (goal_setup_data.oppression_action != AI_OPPRESSION_EXECUTE)
    {
        return -1;
    }
    const object* target = goal_setup_data.selected_target;
    if (target == nullptr || target->get_obj_type() != OT_HUMAN || target->get_sub_type() != OT_SUB_HUMAN_NPC || target->is_killed())
    {
        return -1;
    }
    const human* hp = static_cast<const human*>(target);
    if (human_in_vehicle(hp) != nullptr || MAX_OPPRESS_DIST_SQ <= vec_xz_dist_squared(np->pos, hp->pos))
    {
        return -1;
    }
    return hp->char_inst->get_animation_state() == ANIMATION_STATE_AMB_COWER_CROUCH && !hp->char_inst->is_playing_action(AMIMATION_ACTION_COWER_TURN) ? -1 : 10;
}

// 0x0068A5A0 (:2924)
ai_results order_civilian_to_knees_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const human* target = get_desired_civilian(ai);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        ai->action_state = 0;
        ai->action_timer.set(4000);
        ai->stop_moving();
        human_set_stance(np, HST_STAND);
        ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    }
    const vector to_target(target->pos.x - np->pos.x, target->pos.y - np->pos.y, target->pos.z - np->pos.z);
    vector dir;
    dir = to_target.copy_flatten_normalize_safe();
    np->steering_vector = dir;
    return AI_RESULT_SUCCESS;
}

// True while a civilian argues back, which the two demands wait for.
static bool civilian_is_confronting(const human* target)
{
    if (static_cast<const npc*>(target)->get_ai()->planner.get_current_action() != confront_threatening_npc_action::get_instance())
    {
        return false;
    }
    return target->char_inst->is_playing_action(ANIMATION_ACTION_REACT_ANGRY_B) || target->char_inst->is_playing_action(ANIMATION_ACTION_REACT_ANGRY_C);
}

// 0x006C1090 (:2955)
ai_results order_civilian_to_knees_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human* target = get_desired_civilian(ai);
    if (target == nullptr || target->is_killed() || human_in_vehicle(target) != nullptr || MAX_OPPRESS_DIST_SQ <= vec_xz_dist_squared(np->pos, target->pos))
    {
        return AI_RESULT_FAILURE;
    }
    if (civilian_is_confronting(target))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    ai_system* target_ai = static_cast<npc*>(target)->get_ai();
    if (ai->action_state == 0)
    {
        np->char_inst->play_action(ANIMATION_ACTION_GUARD_POINT, ANIM_ALIAS_NONE, 1.0f, 0);
        np->play_voice_line(VOICE_LINE_EXECUTE_SETUP, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        target_ai->oppression_action = AI_OPPRESSION_GET_ON_KNEES;
        ai->action_state = 1;
        return AI_RESULT_IN_PROGRESS;
    }
    if (np->get_current_voice_line_handle() == INVALID_VOICE_LINE_HANDLE)
    {
        np->char_inst->stop_action(ANIMATION_ACTION_GUARD_POINT, 10, false);
    }
    const ai_oppression_actions state = target_ai->oppression_action;
    if (state == AI_OPPRESSION_GET_ON_KNEES || (state == AI_OPPRESSION_SHOOT_THIS_GUY && target->cur_movement_state == HMS_IDLE))
    {
        if (ai->action_timer.elapsed() && target->char_inst->get_animation_state() == ANIMATION_STATE_AMB_COWER_CROUCH && !target->char_inst->is_playing_action(AMIMATION_ACTION_COWER_TURN))
        {
            return AI_RESULT_SUCCESS;
        }
        return AI_RESULT_IN_PROGRESS;
    }
    threaten_note_enemy(ai->perception, target, false);
    return AI_RESULT_FAILURE;
}

// Clears both oppression states, unless the civilian has been marked to be shot.
static void release_civilian_from_order(ai_system* ai)
{
    human* target = get_desired_civilian(ai);
    if (target != nullptr)
    {
        ai_system* target_ai = static_cast<npc*>(target)->get_ai();
        if (target_ai->oppression_action != AI_OPPRESSION_SHOOT_THIS_GUY)
        {
            target_ai->oppression_action = AI_OPPRESSION_NONE;
        }
    }
    ai->oppression_action = AI_OPPRESSION_NONE;
}

// 0x00644EC0 (:3013)
ai_results order_civilian_to_knees_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this && next_action != move_to_execution_position_action::get_instance() && next_action != execute_civilian_action::get_instance())
    {
        release_civilian_from_order(np->get_ai());
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F3FF0 (:3036). LTCG: target ECX, the result in EAX.
static vector get_check_id_position(const human* target)
{
    const vector& fvec = target->orient.fvec;
    return vector(target->pos.x + fvec.x * CHECK_ID_FORWARD_OFFSET, target->pos.y + fvec.y * CHECK_ID_FORWARD_OFFSET, target->pos.z + fvec.z * CHECK_ID_FORWARD_OFFSET);
}

// 0x005F4080 (:3041)
static vector get_check_id_offset()
{
    return vector(FORWARD_VECTOR.x * CHECK_ID_FORWARD_OFFSET, FORWARD_VECTOR.y * CHECK_ID_FORWARD_OFFSET, FORWARD_VECTOR.z * CHECK_ID_FORWARD_OFFSET);
}

// 0x00644F40 (:3046)
int demand_civilian_id_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (goal_setup_data.oppression_action != AI_OPPRESSION_CHECK_ID)
    {
        return -1;
    }
    const object* target = goal_setup_data.selected_target;
    if (target == nullptr || target->get_obj_type() != OT_HUMAN || target->get_sub_type() != OT_SUB_HUMAN_NPC || target->is_killed())
    {
        return -1;
    }
    const npc* civilian = static_cast<const npc*>(target);
    if (human_in_vehicle(civilian) != nullptr || MAX_OPPRESS_DIST_SQ <= vec_xz_dist_squared(np->pos, civilian->pos))
    {
        return -1;
    }
    return civilian->get_ai()->planner.get_current_action() != civilian_show_id_action::get_instance() ? 10 : -1;
}

// 0x0068A6C0 (:3073)
ai_results demand_civilian_id_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const human* target = get_desired_civilian(ai);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        ai->action_state = 0;
        ai->action_timer.invalidate();
        ai->action_substate = 0;
        ai->stop_moving();
        human_set_stance(np, HST_STAND);
        ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    }
    const vector to_target(target->pos.x - np->pos.x, target->pos.y - np->pos.y, target->pos.z - np->pos.z);
    vector dir;
    dir = to_target.copy_flatten_normalize_safe();
    np->steering_vector = dir;
    return AI_RESULT_SUCCESS;
}

// 0x006C1280 (:3110)
ai_results demand_civilian_id_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human* target = get_desired_civilian(ai);
    if (target == nullptr || target->is_killed() || human_in_vehicle(target) != nullptr || MAX_OPPRESS_DIST_SQ <= vec_xz_dist_squared(np->pos, target->pos))
    {
        return AI_RESULT_FAILURE;
    }
    if (civilian_is_confronting(target))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (ai->action_timer.valid())
    {
        if (ai->action_timer.elapsed() && ai->action_substate == 0)
        {
            const vector from_target(np->pos.x - target->pos.x, np->pos.y - target->pos.y, np->pos.z - target->pos.z);
            vector dir;
            dir = from_target.copy_flatten_normalize_safe();
            target->steering_vector = dir;
            ai->action_substate = 1;
        }
    }
    else
    {
        ai->action_timer.set_rand(1000, 1500);
    }
    ai_system* target_ai = static_cast<npc*>(target)->get_ai();
    if (ai->action_state == 0)
    {
        np->char_inst->play_action(ANIMATION_ACTION_GUARD_POINT, ANIM_ALIAS_NONE, 1.0f, 0);
        np->play_voice_line(VOICE_LINE_DEMAND_CIV_ID, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        target_ai->oppression_action = AI_OPPRESSION_SHOW_ID;
        ai->action_state = 1;
        return AI_RESULT_IN_PROGRESS;
    }
    if (np->get_current_voice_line_handle() == INVALID_VOICE_LINE_HANDLE)
    {
        np->char_inst->stop_action(ANIMATION_ACTION_GUARD_POINT, 10, false);
    }
    if (!np->char_inst->is_playing_action(ANIMATION_ACTION_GUARD_POINT))
    {
        return AI_RESULT_SUCCESS;
    }
    const ai_oppression_actions state = target_ai->oppression_action;
    if (state != AI_OPPRESSION_SHOW_ID && (state != AI_OPPRESSION_SHOOT_THIS_GUY || target->cur_movement_state != HMS_IDLE))
    {
        threaten_note_enemy(ai->perception, target, true);
        return AI_RESULT_FAILURE;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00644FE0 (:3178)
ai_results demand_civilian_id_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this && next_action != move_to_check_civilian_id_action::get_instance() && next_action != check_civilian_id_action::get_instance())
    {
        release_civilian_from_order(np->get_ai());
    }
    return AI_RESULT_SUCCESS;
}

// 0x00645060 (:3201)
int move_to_check_civilian_id_action::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc*, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (goal_setup_data.oppression_action != AI_OPPRESSION_CHECK_ID)
    {
        return -1;
    }
    const object* target = goal_setup_data.selected_target;
    if (target == nullptr || target->get_obj_type() != OT_HUMAN || target->get_sub_type() != OT_SUB_HUMAN_NPC || human_in_vehicle(static_cast<const human*>(target)) != nullptr)
    {
        return -1;
    }
    if (static_cast<const npc*>(target)->get_ai()->oppression_action != AI_OPPRESSION_SHOW_ID)
    {
        desired_conditions |= ai_plan_conditions(29);
    }
    return 100;
}

// 0x005F40D0 (:3222)
void move_to_check_civilian_id_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x0068A7F0 (:3227)
ai_results move_to_check_civilian_id_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        ai->action_timer.invalidate();
        human_set_stance(np, HST_STAND);
        const human* target = get_desired_civilian(ai);
        if (target != nullptr)
        {
            ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        }
    }
    human* target = get_desired_civilian(ai);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    static_cast<npc*>(target)->get_ai()->perception.pay_attention(np->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    const vector to_target(target->pos.x - np->pos.x, target->pos.y - np->pos.y, target->pos.z - np->pos.z);
    vector dir;
    dir = to_target.copy_flatten_normalize_safe();
    target->steering_vector = vector(-dir.x, -dir.y, -dir.z);
    return AI_RESULT_SUCCESS;
}

// 0x006C1540 (:3259)
ai_results move_to_check_civilian_id_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human* target = get_desired_civilian(ai);
    if (target == nullptr || human_in_vehicle(target) != nullptr || MAX_OPPRESS_DIST_SQ <= vec_xz_dist_squared(np->pos, target->pos))
    {
        return AI_RESULT_FAILURE;
    }
    if (static_cast<npc*>(target)->get_ai()->oppression_action != AI_OPPRESSION_SHOW_ID)
    {
        threaten_note_enemy(ai->perception, target, true);
        return AI_RESULT_FAILURE;
    }
    const vector pos = get_check_id_position(target);
    if (vec_xz_dist_squared(np->pos, pos) < INTERACTION_POSITION_DIST_SQ)
    {
        return align_to_interaction_position(np, static_cast<npc*>(target), pos);
    }
    const ai_pathfind_results result = ai->pathfind_to(pos, HMS_WALK, HST_CURRENT, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false);
    return ai_pf_to_result(result) != AI_RESULT_FAILURE ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x00645130 (:3299)
ai_results move_to_check_civilian_id_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this && next_action != check_civilian_id_action::get_instance())
    {
        ai_system* ai = np->get_ai();
        human* target = get_desired_civilian(ai);
        if (target != nullptr)
        {
            static_cast<npc*>(target)->get_ai()->oppression_action = AI_OPPRESSION_NONE;
        }
        ai->oppression_action = AI_OPPRESSION_NONE;
    }
    return AI_RESULT_SUCCESS;
}

// 0x006451A0 (:3321)
int check_civilian_id_action::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (goal_setup_data.oppression_action != AI_OPPRESSION_CHECK_ID)
    {
        return -1;
    }
    const object* target = goal_setup_data.selected_target;
    if (target == nullptr || target->get_obj_type() != OT_HUMAN || target->get_sub_type() != OT_SUB_HUMAN_NPC || target->is_killed())
    {
        return -1;
    }
    const human* hp = static_cast<const human*>(target);
    if (human_in_vehicle(hp) != nullptr)
    {
        return -1;
    }
    if (MAX_OPPRESS_DIST_SQ <= vec_xz_dist_squared(np->pos, hp->pos))
    {
        return 0;
    }
    if (INTERACTION_POSITION_DIST_SQ <= vec_xz_dist_squared(np->pos, get_check_id_position(hp)))
    {
        desired_conditions |= ai_plan_conditions(28);
    }
    return 50;
}

// 0x0068A980 (:3347)
ai_results check_civilian_id_action::enter(npc* np, const ai_action* prev_action) const
{
    ai_system* ai = np->get_ai();
    human* target = get_desired_civilian(ai);
    if (target == nullptr || target->is_killed() || human_in_vehicle(target) != nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (prev_action != this)
    {
        ai->stop_moving();
        human_set_stance(np, HST_STAND);
        static_cast<npc*>(target)->get_ai()->stop_moving();
        ai->action_state = 0;
        ai->action_substate = 0;
        ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    }
    ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    const vector to_target(target->pos.x - np->pos.x, target->pos.y - np->pos.y, target->pos.z - np->pos.z);
    vector dir;
    dir = to_target.copy_flatten_normalize_safe();
    np->steering_vector = dir;
    target->steering_vector = vector(-dir.x, -dir.y, -dir.z);
    return AI_RESULT_SUCCESS;
}

// 0x005F40E0 (:3392)
void check_civilian_id_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x006C16F0 (:3397)
ai_results check_civilian_id_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human* target = get_desired_civilian(ai);
    if (target == nullptr || target->is_killed() || human_in_vehicle(target) != nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    npc* civilian = static_cast<npc*>(target);
    const ai_oppression_actions state = civilian->get_ai()->oppression_action;
    if ((state != AI_OPPRESSION_SHOW_ID && (state != AI_OPPRESSION_SHOOT_THIS_GUY || target->cur_movement_state != HMS_IDLE)) || (np->char_inst->is_playing_action(ANIM_ALIAS_SCRIPTED) && !target->char_inst->is_playing_action(ANIM_ALIAS_SCRIPTED)))
    {
        ai->action_substate = 1;
        return AI_RESULT_FAILURE;
    }
    if (ai->action_substate == 1)
    {
        human_set_combat_ready(np, true, 0);
        ai->perception.set_attention_mode(ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    }
    if (ai->action_state == 0)
    {
        np->anim_prop_info = amb_edf_get_civ_id_item();
        target->anim_prop_info = amb_edf_get_civ_id_item();
        // Pushes as a decompile hides them: actions 666 and 657, alignment on, flags 0x10 (0x006C1A2C).
        play_synced_animation(target, np, get_check_id_offset(), ANIMATION_ACTION_GIVE_ID, ANIMATION_ACTION_CHECK_ID, ANIM_ALIAS_SCRIPTED, ANIM_ALIAS_SCRIPTED, true, 0x10);
        ai->action_state = 1;
    }
    else if (ai->action_state == 1)
    {
        if (!vanim_has_trigger_elapsed(np->char_inst->vp, amb_edf_get_civ_id_anim_trigger_id(), ANIM_ALIAS_SCRIPTED, -1) && np->char_inst->is_playing_action(ANIM_ALIAS_SCRIPTED))
        {
            return AI_RESULT_IN_PROGRESS;
        }
        if (!int_rand_chance(execution_chance))
        {
            play_synced_animation(target, np, get_check_id_offset(), ANIMATION_ACTION_GIVE_ID_APPROVED, ANIMATION_ACTION_CHECK_ID_APPROVED, ANIM_ALIAS_SCRIPTED, ANIM_ALIAS_SCRIPTED, false, 0x10);
            np->play_voice_line(VOICE_LINE_CIV_ID_CLEAN, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
            execution_chance += 10;
            if (99 < execution_chance)
            {
                execution_chance = 100;
            }
            ai->action_state = 2;
            return AI_RESULT_IN_PROGRESS;
        }
        play_synced_animation(target, np, get_check_id_offset(), ANIMATION_ACTION_GIVE_ID_DENIED, ANIMATION_ACTION_CHECK_ID_DENIED, ANIM_ALIAS_SCRIPTED, ANIM_ALIAS_SCRIPTED, false, 0x10);
        np->play_voice_line(VOICE_LINE_CIV_ID_UNCLEAN, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        ai->action_substate = 1;
        ai->action_state = 2;
        execution_chance = execution_chance - 30 < 1 ? 0 : execution_chance - 30;
        return AI_RESULT_IN_PROGRESS;
    }
    else if (ai->action_state == 2 && !np->char_inst->is_playing_action(ANIM_ALIAS_SCRIPTED))
    {
        if (ai->action_substate != 1)
        {
            civilian->npc_flags.civilian_has_been_oppressed = 1;
            return AI_RESULT_SUCCESS;
        }
        threaten_note_enemy(ai->perception, target, false);
        civilian->get_ai()->oppression_action = AI_OPPRESSION_SHOOT_THIS_GUY;
        target->hit_points = 100.0f;
        return AI_RESULT_SUCCESS;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x006C1AB0 (:3507)
ai_results check_civilian_id_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        ai_system* ai = np->get_ai();
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        human_override_default_animation_state(np, false);
        if (np->char_inst->is_playing_action(ANIM_ALIAS_SCRIPTED))
        {
            np->char_inst->stop_action(ANIM_ALIAS_SCRIPTED, 5, false);
        }
        human* target = get_desired_civilian(ai);
        if (target != nullptr)
        {
            ai_system* target_ai = static_cast<npc*>(target)->get_ai();
            if (ai->action_substate == 1)
            {
                threaten_note_enemy(ai->perception, target, true);
                target_ai->oppression_action = AI_OPPRESSION_SHOOT_THIS_GUY;
            }
            else
            {
                target_ai->oppression_action = AI_OPPRESSION_NONE;
            }
        }
        ai->oppression_action = AI_OPPRESSION_NONE;
        item* prop = human_get_prop(np, PROP_TYPE_CODE_DESIGNATED, nullptr);
        for (int i = 0; prop != nullptr && i < 5; i++)
        {
            np->remove_child(prop);
            prop->unset_owner(true);
            World.destroy_object(prop, false);
            prop = human_get_prop(np, PROP_TYPE_CODE_DESIGNATED, nullptr);
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F40F0 (:3550)
int threaten_civilian_warning_shot_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 10000;
}

// 0x00645340 (:3555)
ai_results threaten_civilian_warning_shot_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        ai->stop_moving();
        human_set_stance(np, HST_STAND);
        np->char_inst->play_action(ANIMATION_ACTION_GUARD_WARNING_SHOT, ANIM_ALIAS_NONE, 1.0f, 0);
        ai->action_substate = 0;
    }
    if (ai->perception.get_attention_mode() != ATTENTION_AIM_AT_OBJECT)
    {
        ai->perception.set_attention_mode(ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    }
    human_set_combat_ready(np, true, 0);
    return AI_RESULT_SUCCESS;
}

// 0x006C1C00 (:3579)
ai_results threaten_civilian_warning_shot_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    human* target = get_attention_human(ai);
    if (target == nullptr || target->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    ai_belief* belief = ai->perception.get_belief(AI_BELIEF_BEATDOWN_TARGET, target->get_handle());
    if (belief == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (ai->perception.get_attention_mode() != ATTENTION_AIM_AT_OBJECT)
    {
        ai->perception.set_attention_mode(ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    }
    human_set_combat_ready(np, true, 0);
    if (ai->action_substate == 0)
    {
        if (vanim_has_trigger_just_elapsed(np->char_inst->vp, Trigger_warning_shot, ANIMATION_ACTION_GUARD_WARNING_SHOT, -1))
        {
            ai->action_substate = 1;
        }
        else if (!np->char_inst->is_playing_action(ANIMATION_ACTION_GUARD_WARNING_SHOT))
        {
            ai->action_substate = 2;
            ai->action_timer.set(0);
        }
    }
    if (ai->action_substate == 1)
    {
        weapon* wp = human_weapons_get_weapon(np);
        if (wp == nullptr)
        {
            return AI_RESULT_FAILURE;
        }
        if (wp->get_rounds_in_magazine() < 1)
        {
            wp->set_rounds_in_magazine(1);
        }
        if (wp->get_num_rounds_fired() < 1)
        {
            ai->flags.fire_weapon_this_frame = true;
        }
        else
        {
            ai->action_substate = 2;
            ai->action_timer.set(0);
            ai->set_desired_burst_size(0);
            np->play_voice_line(VOICE_LINE_RESTRICTED_WARNING_SHOT, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        }
    }
    if (ai->action_substate == 2 && 8000 < belief->update_time.time_since() && 3000 < ai->action_timer.time_since())
    {
        threaten_go_hostile(np, target, VOICE_LINE_RESTRICTED_TARGET_HOSTILE);
        return AI_RESULT_SUCCESS;
    }
    return AI_RESULT_IN_PROGRESS;
}

confront_civilian_guard_action confront_civilian_guard_action::the_instance; // 0x01D72570

// 0x00619410 (.h:13)
confront_civilian_guard_action::confront_civilian_guard_action()
    : ai_action("confront guard", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 153}), ai_plan_conditions_from_bits({101}))
{
    flags.has_enter = 1;
}

ai_action* confront_civilian_guard_action::get_instance()
{
    return &the_instance;
}

// 0x005E2C20
confront_civilian_guard_action::~confront_civilian_guard_action()
{
}

// 0x00619650 (.h:13)
int confront_civilian_guard_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

confront_civilian_wait_action confront_civilian_wait_action::the_instance; // 0x01DDB4E8

// 0x00619690 (.h:14)
confront_civilian_wait_action::confront_civilian_wait_action()
    : ai_action("confront wait", ai_plan_conditions_from_bits({9, 47, 58, 103, 136, 137, 153}), ai_plan_conditions_from_bits({102}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* confront_civilian_wait_action::get_instance()
{
    return &the_instance;
}

// 0x005E2C30
confront_civilian_wait_action::~confront_civilian_wait_action()
{
}

// 0x00619920 (.h:14)
int confront_civilian_wait_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

confront_civilian_accompany_action confront_civilian_accompany_action::the_instance; // 0x01D685A8

// 0x00619960 (.h:15)
confront_civilian_accompany_action::confront_civilian_accompany_action()
    : ai_action("confront accompany", ai_plan_conditions_from_bits({103}), ai_plan_conditions_from_bits({101}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* confront_civilian_accompany_action::get_instance()
{
    return &the_instance;
}

// 0x005E2C40
confront_civilian_accompany_action::~confront_civilian_accompany_action()
{
}

// 0x006199C0 (.h:15)
int confront_civilian_accompany_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

confront_civilian_first_moving_action confront_civilian_first_moving_action::the_instance; // 0x01D750F8

// 0x00619A00 (.h:16)
confront_civilian_first_moving_action::confront_civilian_first_moving_action()
    : ai_action("confront 1st moving", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({101}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* confront_civilian_first_moving_action::get_instance()
{
    return &the_instance;
}

// 0x005E2C50
confront_civilian_first_moving_action::~confront_civilian_first_moving_action()
{
}

// 0x00619CA0 (.h:16)
int confront_civilian_first_moving_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

confront_civilian_first_warning_action confront_civilian_first_warning_action::the_instance; // 0x01D68340

// 0x00619CE0 (.h:17)
confront_civilian_first_warning_action::confront_civilian_first_warning_action()
    : ai_action("confront 1st warning", ai_plan_conditions_from_bits({9, 47, 58, 102, 136, 137, 153}), ai_plan_conditions_from_bits({101}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* confront_civilian_first_warning_action::get_instance()
{
    return &the_instance;
}

// 0x005E2C60
confront_civilian_first_warning_action::~confront_civilian_first_warning_action()
{
}

// 0x00619F70 (.h:17)
int confront_civilian_first_warning_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

confront_civilian_warning_shot_action confront_civilian_warning_shot_action::the_instance; // 0x01D68160

// 0x00619FB0 (.h:19)
confront_civilian_warning_shot_action::confront_civilian_warning_shot_action()
    : ai_action("confront 3rd warning", ai_plan_conditions_from_bits({9, 47, 58, 102, 136, 137, 153}), ai_plan_conditions_from_bits({101}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* confront_civilian_warning_shot_action::get_instance()
{
    return &the_instance;
}

// 0x005E2C70
confront_civilian_warning_shot_action::~confront_civilian_warning_shot_action()
{
}

// 0x0061A240 (.h:19)
int confront_civilian_warning_shot_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

confront_civilian_kill_action confront_civilian_kill_action::the_instance; // 0x01DCA814

// 0x0061A280 (.h:20)
confront_civilian_kill_action::confront_civilian_kill_action()
    : ai_action("confront kill", ai_plan_conditions_from_bits({9, 47, 58, 102, 136, 137, 153}), ai_plan_conditions_from_bits({101}))
{
}

ai_action* confront_civilian_kill_action::get_instance()
{
    return &the_instance;
}

// 0x005E2C90
confront_civilian_kill_action::~confront_civilian_kill_action()
{
}

// 0x0061A510 (.h:20)
int confront_civilian_kill_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

confront_civilian_goto_action confront_civilian_goto_action::the_instance; // 0x01D7575C

// 0x0061A550 (.h:21)
confront_civilian_goto_action::confront_civilian_goto_action()
    : ai_action("confront goto", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({103}))
{
    flags.has_enter = 1;
}

ai_action* confront_civilian_goto_action::get_instance()
{
    return &the_instance;
}

// 0x005E2CA0
confront_civilian_goto_action::~confront_civilian_goto_action()
{
}

// 0x0061A7F0 (.h:21)
int confront_civilian_goto_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

confront_civilian_dont_goto_rule confront_civilian_dont_goto_rule::the_instance; // 0x01DDB1F8

// 0x0061A830 (.h:22)
confront_civilian_dont_goto_rule::confront_civilian_dont_goto_rule()
    : ai_production_rule("confront don't goto", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({103}))
{
}

ai_action* confront_civilian_dont_goto_rule::get_instance()
{
    return &the_instance;
}

// 0x005E2CB0
confront_civilian_dont_goto_rule::~confront_civilian_dont_goto_rule()
{
}

// 0x0061AB10 (.h:22)
int confront_civilian_dont_goto_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

confront_civilian_beatdown_rule confront_civilian_beatdown_rule::the_instance; // 0x01D724D4

// 0x0061AB40 (.h:23)
confront_civilian_beatdown_rule::confront_civilian_beatdown_rule()
    : ai_production_rule("melee = beatdown", ai_plan_conditions_from_bits({79}), ai_plan_conditions_from_bits({101}))
{
}

ai_action* confront_civilian_beatdown_rule::get_instance()
{
    return &the_instance;
}

// 0x005E2CC0
confront_civilian_beatdown_rule::~confront_civilian_beatdown_rule()
{
}

// 0x0061ABC0 (.h:23)
int confront_civilian_beatdown_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

threaten_civilian_from_turret_action threaten_civilian_from_turret_action::the_instance; // 0x01DDB260

// 0x0061ABF0 (.h:26)
threaten_civilian_from_turret_action::threaten_civilian_from_turret_action()
    : ai_action("threaten from turret", ai_plan_conditions_from_bits({144}), ai_plan_conditions_from_bits({104}))
{
}

ai_action* threaten_civilian_from_turret_action::get_instance()
{
    return &the_instance;
}

// 0x005E2CD0
threaten_civilian_from_turret_action::~threaten_civilian_from_turret_action()
{
}

// 0x0061AC40 (.h:26)
int threaten_civilian_from_turret_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

threaten_civilian_default_action threaten_civilian_default_action::the_instance; // 0x01D6877C

// 0x0061AC80 (.h:27)
threaten_civilian_default_action::threaten_civilian_default_action()
    : ai_action("threaten default", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({104}))
{
    flags.has_enter = 1;
}

ai_action* threaten_civilian_default_action::get_instance()
{
    return &the_instance;
}

// 0x005E2CF0
threaten_civilian_default_action::~threaten_civilian_default_action()
{
}

// 0x0061AF20 (.h:27)
int threaten_civilian_default_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

threaten_civilian_watch_action threaten_civilian_watch_action::the_instance; // 0x01D75290

// 0x0061AF60 (.h:28)
threaten_civilian_watch_action::threaten_civilian_watch_action()
    : ai_action("threaten watch", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({105}))
{
    flags.has_enter = 1;
}

ai_action* threaten_civilian_watch_action::get_instance()
{
    return &the_instance;
}

// 0x005E2D00
threaten_civilian_watch_action::~threaten_civilian_watch_action()
{
}

// 0x0061B200 (.h:28)
int threaten_civilian_watch_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

beat_civilian_action beat_civilian_action::the_instance; // 0x01D7589C

// 0x0061B240 (.h:29)
beat_civilian_action::beat_civilian_action()
    : ai_action("beat civilian action", ai_plan_conditions_from_bits({9, 25, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({106}))
{
    flags.has_enter = 1;
}

ai_action* beat_civilian_action::get_instance()
{
    return &the_instance;
}

// 0x005E2D20
beat_civilian_action::~beat_civilian_action()
{
}

// 0x0061B540 (.h:29)
int beat_civilian_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

threaten_civilian_warning_shot_action threaten_civilian_warning_shot_action::the_instance; // 0x01D75868

// 0x0061B570 (.h:30)
threaten_civilian_warning_shot_action::threaten_civilian_warning_shot_action()
    : ai_action("threaten warning shot", ai_plan_conditions_from_bits({0, 2, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({106}))
{
    flags.has_enter = 1;
}

ai_action* threaten_civilian_warning_shot_action::get_instance()
{
    return &the_instance;
}

// 0x005E2D30
threaten_civilian_warning_shot_action::~threaten_civilian_warning_shot_action()
{
}

// 0x0061B8C0 (.h:30)
int threaten_civilian_warning_shot_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

order_civilian_to_knees_action order_civilian_to_knees_action::the_instance; // 0x01D75510

// 0x0061B900 (.h:33)
order_civilian_to_knees_action::order_civilian_to_knees_action()
    : ai_action("order civ to knees", ai_plan_conditions_from_bits({0, 2, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({29}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* order_civilian_to_knees_action::get_instance()
{
    return &the_instance;
}

// 0x005E2D40
order_civilian_to_knees_action::~order_civilian_to_knees_action()
{
}

// 0x0061BC50 (.h:33)
int order_civilian_to_knees_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

move_to_execution_position_action move_to_execution_position_action::the_instance; // 0x01D72278

// 0x0061BC90 (.h:34)
move_to_execution_position_action::move_to_execution_position_action()
    : ai_action("move to execution pos", ai_plan_conditions_from_bits({0, 2, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({28}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* move_to_execution_position_action::get_instance()
{
    return &the_instance;
}

// 0x005E2D60
move_to_execution_position_action::~move_to_execution_position_action()
{
}

// 0x0061BFE0 (.h:34)
int move_to_execution_position_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

execute_civilian_action execute_civilian_action::the_instance; // 0x01DDAFDC

// 0x0061C010 (.h:35)
execute_civilian_action::execute_civilian_action()
    : ai_action("execute civilian action", ai_plan_conditions_from_bits({0, 2, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({106}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* execute_civilian_action::get_instance()
{
    return &the_instance;
}

// 0x005E2D80
execute_civilian_action::~execute_civilian_action()
{
}

// 0x0061C360 (.h:35)
int execute_civilian_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

demand_civilian_id_action demand_civilian_id_action::the_instance; // 0x01DA3EE8

// 0x0061C390 (.h:38)
demand_civilian_id_action::demand_civilian_id_action()
    : ai_action("demand civ ID", ai_plan_conditions_from_bits({0, 2, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({29}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* demand_civilian_id_action::get_instance()
{
    return &the_instance;
}

// 0x005E2D90
demand_civilian_id_action::~demand_civilian_id_action()
{
}

// 0x0061C6E0 (.h:38)
int demand_civilian_id_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

move_to_check_civilian_id_action move_to_check_civilian_id_action::the_instance; // 0x01DA3E10

// 0x0061C720 (.h:39)
move_to_check_civilian_id_action::move_to_check_civilian_id_action()
    : ai_action("move to check civ ID", ai_plan_conditions_from_bits({0, 2, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({28}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* move_to_check_civilian_id_action::get_instance()
{
    return &the_instance;
}

// 0x005E2DB0
move_to_check_civilian_id_action::~move_to_check_civilian_id_action()
{
}

// 0x0061CA70 (.h:39)
int move_to_check_civilian_id_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

check_civilian_id_action check_civilian_id_action::the_instance; // 0x01DC9490

// 0x0061CAA0 (.h:40)
check_civilian_id_action::check_civilian_id_action()
    : ai_action("check civ ID", ai_plan_conditions_from_bits({0, 2, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({106}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* check_civilian_id_action::get_instance()
{
    return &the_instance;
}

// 0x005E2DD0
check_civilian_id_action::~check_civilian_id_action()
{
}

// 0x0061CDF0 (.h:40)
int check_civilian_id_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

taunt_civilian_action taunt_civilian_action::the_instance; // 0x01D758DC

// 0x0061D150 (.h:44)
taunt_civilian_action::taunt_civilian_action()
    : ai_action("taunt civilian", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({111}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* taunt_civilian_action::get_instance()
{
    return &the_instance;
}

// 0x005E2DF0
taunt_civilian_action::~taunt_civilian_action()
{
}

// 0x0061D3F0 (.h:44)
int taunt_civilian_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

taunt_player_action taunt_player_action::the_instance; // 0x01DDAF74

// 0x0061CE20 (.h:43)
taunt_player_action::taunt_player_action()
    : ai_action("taunt player", ai_plan_conditions_from_bits({4, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({110}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* taunt_player_action::get_instance()
{
    return &the_instance;
}

// 0x005E2DE0
taunt_player_action::~taunt_player_action()
{
}

// 0x0061D120 (.h:43)
int taunt_player_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

taunt_enemy_running_action taunt_enemy_running_action::the_instance; // 0x01DCA790

// 0x0061D430 (.h:45)
taunt_enemy_running_action::taunt_enemy_running_action()
    : ai_action("taunt enemy run", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({112}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* taunt_enemy_running_action::get_instance()
{
    return &the_instance;
}

// 0x005E2E00
taunt_enemy_running_action::~taunt_enemy_running_action()
{
}

// 0x0061D6D0 (.h:45)
int taunt_enemy_running_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

