// Original: volition/rfg/code/ai/civilian/ambient_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/civilian/ambient_actions.h"

#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/self_preservation_goals.h"
#include "volition/rfg/code/ai/vehicle/vehicle_ai_actions.h"
#include "volition/rfg/code/gameplay/activities/activity_house_arrest.h"
#include "volition/rfg/code/gameplay/activities/activity_raid_attack.h"
#include "volition/rfg/code/gameplay/air_bombing.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_move.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_internal_shared.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/humans/human_voice/voice_line_unlocking.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/rfg/code/spawn/ambient_prop_info.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_flyer.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"

#include <float.h>
#include <math.h>

// Position 2 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG02")

static timestamp Greet_npc_timeout; // 0x016424D4
static timestamp Greet_player_timeout; // 0x016424D8
static timestamp Mutter_player_timeout; // 0x016424DC
static timestamp Ambient_vlu_timeout; // 0x016424E0
static const int NUM_AMBIENT_LIST_ENTRIES = 8;
static unsigned int Npc_greet_list[NUM_AMBIENT_LIST_ENTRIES] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF }; // 0x01641550
static unsigned int Player_greet_list[NUM_AMBIENT_LIST_ENTRIES] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF }; // 0x01641570
static unsigned int Npc_mutter_list[NUM_AMBIENT_LIST_ENTRIES] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF }; // 0x01641590
// 0x01D687BC, from a dynamic initializer: the cosine of 0x012CF3B8, which holds 60 * 0.017453f in float, widened.
static const float PLAYER_GREET_DOT = (float)cos((double)(60.0f * 0.017453f));

static bool on_list(const unsigned int* list, unsigned int handle)
{
    for (int i = 0; i < NUM_AMBIENT_LIST_ENTRIES; i++)
    {
        if (list[i] == handle)
        {
            return true;
        }
    }
    return false;
}

// 0x0060F8E0 (:175). Not while a scripted intro is talking near the player.
static bool should_play_ambient_lines()
{
    const player* local = player_get_local_player();
    if (local == nullptr)
    {
        return true;
    }
    const unsigned int informant = house_arrest_get_informant_npc();
    const object* objp = informant != 0xFFFFFFFF ? World.object_from_handle(informant) : nullptr;
    if (objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_NPC && vec_xz_dist_squared(local->pos, objp->pos) < 400.0f)
    {
        return false;
    }
    const npc* intro = activity_raid_attack_get_intro_npc();
    return intro == nullptr || 400.0f <= vec_xz_dist_squared(local->pos, intro->pos);
}

// 0x0060F960 (:202). LTCG: np EAX, aip ECX.
static bool should_despawn_stuck_npc(const npc* np, const ai_system* aip)
{
    if (np->npc_flags.mission_related || np->hflags.activity_raid_required || np->hflags.activity_raid_optional || np->hflags.activity_ha_hostage)
    {
        return false;
    }
    return aip->get_squad() == nullptr;
}

// 0x00651F90 (:221). A vehicle moving over 2 m/s within 5 m, with a driver the npc is friendly with.
static vehicle* find_near_miss_vehicles(const npc* np)
{
    const vector near_min(np->pos.x - 5.0f, np->pos.y - 5.0f, np->pos.z - 5.0f);
    const vector near_max(np->pos.x + 5.0f, np->pos.y + 5.0f, np->pos.z + 5.0f);
    const int num_vehicles = World.get_num_objects<vehicle>();
    for (int i = 0; i < num_vehicles; i++)
    {
        vehicle* vp = World.get_object<vehicle>(i);
        const human* driver = vp != nullptr ? vehicle_passenger_in_seat(vp, VSI_DRIVER) : nullptr;
        if (driver == nullptr || human_teams_get_disposition(np, driver) <= TEAM_DISPOSITION_NONE)
        {
            continue;
        }
        vector vp_min;
        vector vp_max;
        vp->get_bbox(vp_min, vp_max);
        if (ix_box_box_aligned(near_min, near_max, vp_min, vp_max) && 2.0f < vp->forward_velocity)
        {
            return vp;
        }
    }
    return nullptr;
}

// 0x005E0460 (:260)
static animation_action pick_greet_animation()
{
    return (vrand() & 1) != 0 ? ANIMATION_ACTION_GREET_DISMISS_LEFT : ANIMATION_ACTION_GREET_DISMISS_RIGHT;
}

// 0x00652170 (:272)
static const npc* find_nearest_ambient_npc(const npc* np)
{
    const npc* best = nullptr;
    float best_dist_sq = FLT_MAX;
    const int num_npcs = World.get_num_objects<npc>();
    for (int i = 0; i < num_npcs; i++)
    {
        const npc* other = World.get_object<npc>(i);
        if (other->get_manager() != NPC_MANAGER_AMBIENT || other == np || other->vehicle_handle != 0xFFFFFFFF || human_teams_get_disposition(np, other) <= TEAM_DISPOSITION_NONE)
        {
            continue;
        }
        const float dist_sq = vec_dist_squared(np->pos, other->pos);
        if (dist_sq < best_dist_sq)
        {
            best = other;
            best_dist_sq = dist_sq;
        }
    }
    return best;
}

// 0x00652230 (:297). The nearest ambient npc within 10 m, when the two face each other.
static const npc* find_greet_npc(const npc* np)
{
    if (Greet_npc_timeout.valid() && !Greet_npc_timeout.elapsed())
    {
        return nullptr;
    }
    const npc* other = find_nearest_ambient_npc(np);
    if (other == nullptr || 100.0f < vec_dist_squared(other->pos, np->pos))
    {
        return nullptr;
    }
    vector dir(other->pos.x - np->pos.x, other->pos.y - np->pos.y, other->pos.z - np->pos.z);
    dir.normalize_safe();
    if (np->orient.fvec.y * dir.y + np->orient.fvec.x * dir.x + np->orient.fvec.z * dir.z <= PLAYER_GREET_DOT)
    {
        return nullptr;
    }
    return other->orient.fvec.y * -dir.y + other->orient.fvec.x * -dir.x + other->orient.fvec.z * -dir.z > PLAYER_GREET_DOT ? other : nullptr;
}

// 0x0068F320 (:341)
void play_ambient_voice_lines(npc* np, object_action_node* anp)
{
    if (!anp->allow_ambient_voice_lines() || np->hflags.do_not_play_ambient_or_greet_lines || !anp->voice_timeout.elapsed())
    {
        return;
    }
    anp->voice_timeout.set(int_rand_range(300, 2000));
    if (!should_play_ambient_lines())
    {
        return;
    }
    ai_system* ai = np->get_ai();
    if (!Ambient_vlu_timeout.valid() || Ambient_vlu_timeout.elapsed())
    {
        Ambient_vlu_timeout.set_rand(5000, 15000);
        const voice_line_handle line = voice_line_unlocking_choose(VLUG_AMB_MINER);
        if (line != INVALID_VOICE_LINE_HANDLE)
        {
            ai->desired_voice_line = VOICE_LINE_NONE;
            if (np->play_voice_line(line, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_DIALOGUE_NORMAL, false, false, true, true) == PLAY_VOICE_SUCCESS)
            {
                return;
            }
        }
    }
    const bool mutter_blocked = Mutter_player_timeout.valid() && !Mutter_player_timeout.elapsed();
    const bool greet_blocked = Greet_player_timeout.valid() && !Greet_player_timeout.elapsed();
    if (mutter_blocked && greet_blocked)
    {
        return;
    }
    player* local = player_get_local_player();
    if (local == nullptr)
    {
        return;
    }
    const float dist_sq = vec_dist_squared(np->pos, local->pos);
    if (225.0f < dist_sq)
    {
        return;
    }
    bool greet = false;
    if (dist_sq < 25.0f)
    {
        vector to_player(local->pos.x - np->pos.x, local->pos.y - np->pos.y, local->pos.z - np->pos.z);
        to_player.normalize();
        greet = 0.35f < to_player * np->orient.fvec && !greet_blocked;
    }
    if (!greet && mutter_blocked)
    {
        return;
    }
    const ai_morale_levels morale = ai->get_district_morale_level();
    if (greet)
    {
        Greet_player_timeout.set(8000);
        ai->perception.pay_attention(local, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        if (np->current_team == HUMAN_TEAM_MARAUDER)
        {
            ai->desired_voice_line = VOICE_LINE_AMBIENT_THREATEN;
        }
        else
        {
            const bool famous = ai_system_consider_player_famous();
            if (morale == AI_MORALE_LEVEL_LOW)
            {
                ai->desired_voice_line = famous ? VOICE_LINE_AMBIENT_PLAYER_GREET_LOW_MORALE_FAMOUS : VOICE_LINE_AMBIENT_PLAYER_GREET_LOW_MORALE;
            }
            else if (morale == AI_MORALE_LEVEL_MEDIUM)
            {
                ai->desired_voice_line = famous ? VOICE_LINE_AMBIENT_PLAYER_GREET_MEDIUM_MORALE_FAMOUS : VOICE_LINE_AMBIENT_PLAYER_GREET_MEDIUM_MORALE;
            }
            else if (morale == AI_MORALE_LEVEL_HIGH)
            {
                ai->desired_voice_line = famous ? VOICE_LINE_AMBIENT_PLAYER_GREET_HIGH_MORALE_FAMOUS : VOICE_LINE_AMBIENT_PLAYER_GREET_HIGH_MORALE;
            }
            else
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                return;
            }
        }
    }
    else
    {
        if (morale == AI_MORALE_LEVEL_LOW)
        {
            ai->desired_voice_line = VOICE_LINE_AMBIENT_TALK_TO_SELF_LOW_MORALE;
        }
        else if (morale == AI_MORALE_LEVEL_MEDIUM)
        {
            ai->desired_voice_line = VOICE_LINE_AMBIENT_TALK_TO_SELF_MEDIUM_MORALE;
        }
        else if (morale == AI_MORALE_LEVEL_HIGH)
        {
            ai->desired_voice_line = VOICE_LINE_AMBIENT_TALK_TO_SELF_HIGH_MORALE;
        }
        else
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            return;
        }
        Mutter_player_timeout.set(8000);
    }
    np->play_voice_line(ai->desired_voice_line, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
}

// 0x005E0480 (:481). LTCG: anp ESI.
static const object* get_watch_target(const object_action_node* anp)
{
    return anp != nullptr && anp->look_at_parent() ? World.object_from_handle(anp->get_parent()) : nullptr;
}

// 0x005E04B0 (:496)
static bool on_mutter_list(const npc* np)
{
    return on_list(Npc_mutter_list, np->get_handle());
}

// 0x005E04D0 (:510)
static bool on_player_greet_list(const npc* np)
{
    return on_list(Player_greet_list, np->get_handle());
}

// 0x005E04F0 (:524)
static bool on_npc_greet_list(const npc* np)
{
    return on_list(Npc_greet_list, np->get_handle());
}

// 0x005E0510 (:540)
static void add_player_greet_list(const npc* np)
{
    Player_greet_list[int_rand_range(0, 7)] = np->get_handle();
}

// 0x005E0530 (:551)
static void add_npc_greet_list(const npc* np)
{
    Npc_greet_list[int_rand_range(0, 7)] = np->get_handle();
}

// 0x005E0550 (:562)
static void add_mutter_list(const npc* np)
{
    Npc_mutter_list[int_rand_range(0, 7)] = np->get_handle();
}

// 0x005E0570 (:576)
static bool on_ambient_action_list(const npc* np)
{
    return on_npc_greet_list(np) || on_player_greet_list(np) || on_mutter_list(np);
}

// Panicked npcs run when unsticking. Inlined at the top of both functions below.
static human_movement_states unstick_movement_state(const npc* np)
{
    const ai_perception& perception = np->get_ai()->perception;
    const bool panicked = perception.get_first_belief(AI_BELIEF_ENEMY_POSITION) != nullptr || perception.get_first_belief(AI_BELIEF_ENEMY_EXISTS) != nullptr || air_bomb_is_running() || (ALERT_LEVEL_GREEN < gsm_get_alert_level() && mission_get_mode() == MISSION_MODE_SNIPER) || ALERT_LEVEL_ORANGE < gsm_get_alert_level() || ai_get_enemy_flee_belief(np) != nullptr;
    return panicked ? HMS_RUN : HMS_WALK;
}

// 0x0068F6F0 (:582). LTCG: np ESI.
static bool unstick_human_move_to_point_regardless(npc* np, vector& pt, nav_cell* dest_nc, bool force_direct_walk, bool allow_direct_walk)
{
    const human_movement_states m_state = unstick_movement_state(np);
    np->hflags.use_bigsteps = 1;
    if (!force_direct_walk)
    {
        const uint64_t hint = dest_nc != nullptr ? nav_cell_get_handle(dest_nc) : 0xFFFFFFFFFFFFFFFFull;
        if (human_pathfind_to(np, pt, HS_STANDARD, m_state, hint, false))
        {
            return true;
        }
    }
    np->hflags.last_pf_failed = 1;
    if (allow_direct_walk)
    {
        human_move_force_direct_walk(np, pt, m_state, dest_nc);
        return true;
    }
    return false;
}

// 0x0068F7C0 (:601)
static bool unstick_human_pathfind_to_last_dest(npc* np)
{
    const human_movement_states m_state = unstick_movement_state(np);
    navpoint_set_treat_all_as_enabled(true);
    const bool found = human_pathfind_to(np, np->pf.eventual_goal_pos, HS_STANDARD, m_state, 0xFFFFFFFFFFFFFFFFull, false);
    navpoint_set_treat_all_as_enabled(false);
    np->hflags.last_pf_failed = 1;
    if (found && np->pf.path != nullptr && np->pf.path->navpoint_handle == 0xFFFFFFFF)
    {
        return false;
    }
    return found;
}

// 0x0060F990 (:632)
int goto_action_node_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (goal_setup_data.selected_action_node_set.size() < 1)
    {
        return -1;
    }
    return np->at_entry_point(goal_setup_data.selected_action_node_set[0]) ? 0 : 2000;
}

// 0x005E05C0 (:647)
ai_results goto_action_node_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    object_action_node* node = np->get_desired_action_node();
    if (node == nullptr || node->disabled_by != 0)
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    if (node->get_entry_point(np, ai->desired_destination) && node->get_entry_rotation(np, ai->desired_orient))
    {
        ai->action_timer.invalidate();
        ai->desired_vehicle_speed = 0.0f;
        return AI_RESULT_SUCCESS;
    }
    return AI_RESULT_FAILURE;
}

// 0x0068F870 (:686)
ai_results goto_action_node_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    ai->perception.maybe_look_away_from_corpse();
    object_action_node* node = np->get_desired_action_node();
    if (node == nullptr || node->disabled_by != 0 || !node->prop_requirement_met(np))
    {
        np->release_all_action_nodes();
        return AI_RESULT_FAILURE;
    }
    if (node->at_entry_point(np))
    {
        ai->stop_moving();
        np->inertial_motion.zero();
        return AI_RESULT_SUCCESS;
    }
    vector& entry_point = ai->desired_destination;
    if (!node->get_entry_point(np, entry_point))
    {
        node->set_disabled_by(ANDR_ENTRY_POINT);
        np->release_all_action_nodes();
        return AI_RESULT_FAILURE;
    }
    if (ai->is_pathing_to(entry_point, 0.25f, 1.0f))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (1.0f < vec_dist_squared(np->pos, entry_point))
    {
        const ai_pathfind_results result = ai->pathfind_to(entry_point, node->node_flags.run_to ? HMS_RUN : HMS_WALK, HST_STAND, AI_PATHFIND_PRIORITY_LOW, 0xFFFFFFFFFFFFFFFFull, false);
        if (ai_pf_to_result(result) == AI_RESULT_FAILURE)
        {
            node->set_disabled_by(ANDR_PATHFINDING);
            np->release_all_action_nodes();
            return AI_RESULT_FAILURE;
        }
        return AI_RESULT_IN_PROGRESS;
    }
    // The last metre is walked by hand for up to 2 s, then the npc is put there.
    if (!ai->action_timer.valid())
    {
        ai->action_timer.set(2000);
    }
    if (ai->action_timer.elapsed())
    {
        np->hflags.override_steering_heading_offset = 0;
        np->move_speed = 0.0f;
        np->steering_heading_offset = 0.0f;
        if (0.010000001f < vec_xz_dist_squared(np->pos, entry_point))
        {
            np->update_pos_and_orient(entry_point, np->orient, true);
        }
    }
    else
    {
        vector move(entry_point.x - np->pos.x, 0.0f, entry_point.z - np->pos.z);
        const float dist = move.mag_and_normalize_safe();
        if (0.5f <= dist)
        {
            np->hflags.override_steering_heading_offset = 0;
            np->move_speed = 2.5f;
            np->steering_vector = move;
            np->steering_heading_offset = 0.0f;
            return AI_RESULT_IN_PROGRESS;
        }
        if (0.1f <= dist)
        {
            np->move_speed = 1.0f;
            np->steering_vector = move;
            const float move_heading = fl_atan2_safe(np->steering_vector.x, np->steering_vector.z);
            const float desired_heading = ai->desired_orient.extract_heading();
            np->hflags.override_steering_heading_offset = 1;
            np->steering_heading_offset = desired_heading - move_heading;
            return AI_RESULT_IN_PROGRESS;
        }
        ai->action_timer.set(0);
        np->move_speed = 0.0f;
        np->steering_vector = ai->desired_orient.fvec;
        np->hflags.override_steering_heading_offset = 0;
        np->steering_heading_offset = 0.0f;
    }
    ai->stop_moving();
    np->steering_vector = ai->desired_orient.fvec;
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E0640 (:834)
ai_results goto_action_node_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->hflags.override_steering_heading_offset = 0;
    }
    return AI_RESULT_SUCCESS;
}

static object_action_node* get_selected_action_node(const ai_goal_setup_data& goal_setup_data)
{
    if (goal_setup_data.selected_action_node_set.size() < 1 || goal_setup_data.selected_action_node_set[0] == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(goal_setup_data.selected_action_node_set[0]);
    return objp != nullptr && objp->get_obj_type() == OT_ACTION_NODE ? static_cast<object_action_node*>(objp) : nullptr;
}

// 0x0060F9D0 (:849)
int do_action_node_action::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    const object_action_node* node = get_selected_action_node(goal_setup_data);
    if (node == nullptr)
    {
        return -1;
    }
    if (!np->has_entered_action_node(node))
    {
        desired_conditions |= ai_plan_conditions_from_bits({134, 135});
    }
    const ai_plan_conditions& working = goal_setup_data.initial_working_conditions;
    if (node->weapon_required())
    {
        if (!(ai_plan_conditions(2) & working))
        {
            desired_conditions |= ai_plan_conditions_from_bits({0, 2});
        }
    }
    else if (node->melee_weapon_required())
    {
        if (!(ai_plan_conditions(3) & working))
        {
            desired_conditions |= ai_plan_conditions(3);
        }
    }
    else if (!!((ai_plan_conditions(3) | ai_plan_conditions(2)) & working))
    {
        desired_conditions |= ai_plan_conditions(7);
    }
    return 2000;
}

// 0x005E0660 (:892)
void do_action_node_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x006523E0 (:897). rfg.exe inlines the whole of set_attention_mode at the end, the mode argument being the current one.
ai_results do_action_node_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    object_action_node* node = np->get_current_action_node(false);
    np->hflags.external_force_applied = 0;
    if (node == nullptr || node->disabled_by != 0)
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        ai->action_timer.set(250);
        ai->action_substate = node->has_ambient_actions(np) ? 1 : 0;
        ai->animation_timer.set_rand(1000, 6000);
    }
    ai->action_state = 0;
    ai->perception.set_attention_mode(ai->perception.get_attention_mode(), ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    return AI_RESULT_SUCCESS;
}

// 0x0068FC10 (:931)
ai_results do_action_node_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    object_action_node* node = np->get_current_action_node(false);
    if (node == nullptr || node->disabled_by != 0)
    {
        return AI_RESULT_FAILURE;
    }
    if (node->cooldown_period.valid() && !node->cooldown_period.elapsed())
    {
        return AI_RESULT_SUCCESS;
    }
    if (node->weapon_required())
    {
        human_set_combat_ready(np, true, 0);
        ai->perception.set_attention_mode(ATTENTION_AIM_STRAIGHT_AHEAD, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
        weapon* wp = human_weapons_get_weapon(np);
        if (wp == nullptr)
        {
            return AI_RESULT_FAILURE;
        }
        if (wp->get_rounds_in_magazine() < 1)
        {
            wp->set_rounds_in_magazine(1);
        }
        if (np->char_inst->is_playing_action(ANIMATION_ACTION_STAND_FIRE) && ai->action_state == 0)
        {
            ai->flags.fire_weapon_this_frame = true;
            ai->action_state = 1;
        }
        else
        {
            ai->set_desired_burst_size(0);
        }
    }
    play_ambient_voice_lines(np, node);
    const action_node_definition* definition = node->base_node_info;
    if (definition->prop_creation_time == PROP_CREATION_TIME_POST_ENTRY_ANIM && np->amb_prop_info == nullptr)
    {
        const ambient_prop_info* prop = node->get_ambient_prop_info();
        if (prop == nullptr || !amb_prop_create(prop, np))
        {
            return AI_RESULT_FAILURE;
        }
    }
    if (np->amb_prop_info != nullptr && definition->prop_removal_time == PROP_REMOVAL_TIME_ANIMATION_ACTION && node->drop_action_trigger_elapsed(np))
    {
        amb_prop_detach_props(np, true);
    }
    if (definition->prop_creation_time == PROP_CREATION_TIME_ANIMATION_ACTION && np->amb_prop_info == nullptr && node->pickup_action_trigger_elapsed(np) && definition->prop_removal_time == PROP_REMOVAL_TIME_ANIMATION_ACTION && !node->drop_action_trigger_elapsed(np))
    {
        const ambient_prop_info* prop = node->get_ambient_prop_info();
        if (prop == nullptr || !amb_prop_create(prop, np))
        {
            return AI_RESULT_FAILURE;
        }
    }
    if (np->hflags.external_force_applied)
    {
        return AI_RESULT_FAILURE;
    }
    if (!node->has_entered_node(np))
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return AI_RESULT_FAILURE;
    }
    if (!ai->action_timer.elapsed() || !np->completed_action_node(node))
    {
        if (ai->action_substate != 0 && !node->doing_action(np) && node->action_cooldown_timer.elapsed() && fl_rand_chance(0.5f))
        {
            node->play_ambient_action(np);
            ai->action_state = 0;
        }
        return ai->action_timer.valid() ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
    }
    if (!np->h_info->flags.old_coot && !node->doing_action(np))
    {
        node->set_cooldown_time();
        np->action_node_data.exit_quick = false;
        return AI_RESULT_SUCCESS;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x0060FD50 (:1065)
ai_results do_action_node_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    ai_system* ai = np->get_ai();
    if (np->npc_flags.ambient_spawn && ai->get_squad_handle() == 0xFFFFFFFF)
    {
        np->start_active_despawn(NPC_DESPAWN_TYPE_LOW, nullptr);
    }
    if (np->char_inst->rdd.is_loaded())
    {
        np->char_inst->stop_action(ANIM_ALIAS_ACTION_NODE_ACTION, 5, false);
    }
    object_action_node* node = np->get_current_action_node(true);
    if (node != nullptr)
    {
        // A next action that needs the node (condition 134) keeps the reservation.
        if (next_action == nullptr || !(next_action->get_pre_conditions() & ai_plan_conditions(134)))
        {
            np->release_action_node(node);
        }
        if (node->weapon_required())
        {
            ai->set_desired_burst_size(0);
        }
        if (np->amb_prop_info != nullptr && (node->base_node_info->prop_removal_time == PROP_REMOVAL_TIME_NODE_COMPLETE || node->base_node_info->prop_removal_time == PROP_REMOVAL_TIME_ANIMATION_ACTION))
        {
            amb_prop_detach_props(np, true);
        }
    }
    np->npc_flags.used_action_node = 1;
    return AI_RESULT_SUCCESS;
}

// 0x0060FEA0 (:1119)
int enter_action_node_action::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc*, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    const object_action_node* node = get_selected_action_node(goal_setup_data);
    if (node == nullptr)
    {
        return -1;
    }
    if (node->get_ambient_prop_info() == nullptr)
    {
        desired_conditions |= ai_plan_conditions(137);
    }
    const ai_plan_conditions& working = goal_setup_data.initial_working_conditions;
    const bool has_firearm_condition = !!(ai_plan_conditions(2) & working);
    if (node->weapon_required())
    {
        if (!has_firearm_condition)
        {
            desired_conditions |= ai_plan_conditions_from_bits({0, 2});
        }
    }
    else if (node->melee_weapon_required())
    {
        if (!(ai_plan_conditions(3) & working))
        {
            desired_conditions |= ai_plan_conditions(3);
        }
    }
    else if (has_firearm_condition)
    {
        desired_conditions |= ai_plan_conditions(7);
    }
    return 2000;
}

// 0x005E0670 (:1165)
void enter_action_node_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x0068FEE0 (:1171)
ai_results enter_action_node_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    ai_system* ai = np->get_ai();
    object_action_node* node = np->get_desired_action_node();
    ai->action_timer.set(250);
    np->scripted_action_node_handle = 0xFFFFFFFF;
    if (node == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const item* held = amb_prop_human_find(np);
    if (held != nullptr)
    {
        const ambient_prop_info* prop = node->get_ambient_prop_info();
        if (prop == nullptr || held->info != prop->prop_item_info)
        {
            amb_prop_detach_props(np, true);
        }
    }
    play_ambient_voice_lines(np, node);
    ai->planner.lock_action(true);
    ai->stop_moving();
    const object* watch = get_watch_target(node);
    if (watch != nullptr)
    {
        ai->perception.pay_attention(watch->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    }
    else
    {
        ai->perception.set_attention_mode(ATTENTION_IDLE, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    }
    return node->enter_node(np) ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x00610140 (:1232)
ai_results enter_action_node_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    object_action_node* node = np->get_desired_action_node();
    if (node == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (node->disabled_by != 0)
    {
        const unsigned int handle = np->get_current_action_node_handle();
        object* current = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
        if (current != nullptr && current->get_obj_type() == OT_ACTION_NODE)
        {
            static_cast<object_action_node*>(current)->emergency_exit(np);
        }
        return AI_RESULT_FAILURE;
    }
    node->disable_node_collision(np);
    const action_node_prop_creation_time creation_time = node->base_node_info->prop_creation_time;
    if (creation_time == PROP_CREATION_TIME_PRE_ENTRY_ANIM)
    {
        if (np->amb_prop_info == nullptr)
        {
            item* pickup = node->get_pickup_object();
            if (pickup == nullptr)
            {
                return AI_RESULT_FAILURE;
            }
            if (node->pickup_trigger_elapsed(np) && !amb_prop_mount(np, node->get_ambient_prop_info(), pickup))
            {
                return AI_RESULT_FAILURE;
            }
        }
    }
    else if (creation_time == PROP_CREATION_TIME_ENTRY_ANIM && np->amb_prop_info == nullptr && node->pickup_trigger_elapsed(np))
    {
        const ambient_prop_info* prop = node->get_ambient_prop_info();
        if (prop == nullptr || !amb_prop_create(prop, np))
        {
            return AI_RESULT_FAILURE;
        }
    }
    if (!ai->action_timer.elapsed())
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (!action_node_has_entered_node(np, node))
    {
        return 9999 < ai->action_timer.time_since() ? AI_RESULT_FAILURE : AI_RESULT_IN_PROGRESS;
    }
    return action_node_is_entering(np) ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x00610290 (:1313)
ai_results enter_action_node_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->get_ai()->planner.lock_action(false);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E0680 (:1332)
int exit_action_node_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->get_current_action_node_handle() != 0xFFFFFFFF ? 2000 : 0;
}

// 0x006102C0 (:1342)
ai_results exit_action_node_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    object_action_node* node = np->get_current_action_node(false);
    if (node == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    np->hflags.saved_pushes_debris_scripted = np->hflags.pushes_debris_scripted;
    np->hflags.pushes_debris_scripted = 1;
    node->exit_node(np);
    ai_system* ai = np->get_ai();
    ai->desired_target = node->get_handle();
    ai->planner.lock_action(true);
    ai->action_substate = node->base_node_info->prop_removal_time == PROP_REMOVAL_TIME_EXIT_ANIMATION ? 1 : 0;
    return AI_RESULT_SUCCESS;
}

static object_action_node* get_desired_target_node(const ai_system* ai)
{
    object* objp = ai->desired_target != 0xFFFFFFFF ? World.object_from_handle(ai->desired_target) : nullptr;
    return objp != nullptr && objp->get_obj_type() == OT_ACTION_NODE ? static_cast<object_action_node*>(objp) : nullptr;
}

// 0x00610350 (:1377)
ai_results exit_action_node_action::do_action(npc* np) const
{
    object_action_node* node = get_desired_target_node(np->get_ai());
    if (node != nullptr)
    {
        if (np->amb_prop_info != nullptr && node->has_drop_prop_trigger() && node->drop_trigger_elapsed(np))
        {
            amb_prop_detach_props(np, node->base_node_info->prop_removal_time == PROP_REMOVAL_TIME_EXIT_ANIMATION);
        }
        if (action_node_is_exiting(np))
        {
            return AI_RESULT_IN_PROGRESS;
        }
        np->hflags.pushes_debris_scripted = np->hflags.saved_pushes_debris_scripted;
    }
    return AI_RESULT_SUCCESS;
}

// 0x00610400 (:1411)
ai_results exit_action_node_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        ai_system* ai = np->get_ai();
        ai->planner.lock_action(false);
        if (action_node_is_exiting(np) && np->char_inst->rdd.is_loaded())
        {
            np->char_inst->stop_action(ANIM_ALIAS_TRANSITION_FROM_ACTION_NODE, 5, false);
        }
        object_action_node* node = get_desired_target_node(ai);
        if (node != nullptr)
        {
            node->destroy_child_relations();
            if (ai->action_substate != 0 && np->amb_prop_info != nullptr)
            {
                amb_prop_detach_props(np, true);
            }
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E06A0 (:1452)
int unstick_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 1;
}

// 0x006104A0 (:1457)
ai_results unstick_action::enter(npc* np, const ai_action* last_action) const
{
    const object* vp = np->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(np->vehicle_handle) : nullptr;
    if ((vp != nullptr && vp->get_obj_type() == OT_VEHICLE) || human_is_driving(np) || !np->hflags.extended_stuck)
    {
        return AI_RESULT_FAILURE;
    }
    if (np->hflags.stuck)
    {
        human_set_movement_mode(np, HMM_NONE, HMSM_NONE);
        human_set_state(np, HS_STANDARD, HMS_IDLE, -1.0f);
        np->hflags.stuck = 0;
    }
    if (last_action != this)
    {
        ai_system* ai = np->get_ai();
        ai->action_timer.set(0);
        ai->action_state = 0;
        ai->action_substate = 0;
    }
    return AI_RESULT_SUCCESS;
}

// 0x006B5500 (:1497). Each try escalates: pathfind to the last goal, walk to the mesh, to the cell's centre, to random points and at last straight there; after four, a stuck npc offscreen despawns or jumps.
ai_results unstick_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (np->current_team != HUMAN_TEAM_CIVILIAN)
    {
        ai_combat_fire_at_target_if_available(np, false, false);
    }
    if (vehicle_interaction_ragdoll_fall_in_seat(np, nullptr))
    {
        np->hflags.last_pf_failed = 0;
        np->hflags.extended_stuck = 0;
        return AI_RESULT_SUCCESS;
    }
    if (!np->hflags.extended_stuck)
    {
        if (ai->action_state == 0)
        {
            ai->action_timer.set(0);
            ai->action_state = 1;
        }
        else if (3000 < ai->action_timer.time_since())
        {
            np->hflags.last_pf_failed = 0;
            return AI_RESULT_SUCCESS;
        }
        return AI_RESULT_IN_PROGRESS;
    }
    if (ai->action_state != 0)
    {
        ai->action_state = 0;
        ai->action_timer.set_rand(1000, 2500);
    }
    if (np->hflags.stuck)
    {
        human_set_movement_mode(np, HMM_NONE, HMSM_NONE);
        human_set_state(np, HS_STANDARD, HMS_IDLE, -1.0f);
        np->hflags.stuck = 0;
        if (!ai->action_timer.valid())
        {
            ai->action_timer.set_rand(1000, 2500);
        }
    }
    if (3 < ai->action_substate && np->hflags.extended_stuck)
    {
        np->npc_flags.despawn_npc = 1;
        const bool can_despawn = should_despawn_stuck_npc(np, ai);
        const bool offscreen = np->is_offscreen();
        if (offscreen && can_despawn)
        {
            ai->do_despawn(true);
            np->hflags.last_pf_failed = 0;
            return AI_RESULT_SUCCESS;
        }
        np->npc_flags.despawn_npc = 0;
        if (offscreen)
        {
            vector try_pos(np->pos.x + fl_rand_range(-20.0f, 20.0f), np->pos.y + 0.0f, np->pos.z + fl_rand_range(-20.0f, 20.0f));
            if (ai->is_in_player_squad(true) != nullptr)
            {
                const player* local = player_get_local_player();
                try_pos = vector(local->pos.x - local->orient.fvec.x * 5.0f, local->pos.y - local->orient.fvec.y * 5.0f, local->pos.z - local->orient.fvec.z * 5.0f);
            }
            vector mesh_pos;
            nav_cell* cell;
            if (rfg_pf_find_closest_point_on_nav_mesh_using_collect(try_pos, false, mesh_pos, cell, 50.0f))
            {
                const vector cull_pos(mesh_pos.x, mesh_pos.y + 0.85f, mesh_pos.z);
                if (game_render_cull_sphere(cull_pos, 1.25f))
                {
                    matrix orient = IDENTITY_MATRIX;
                    human_teleport(np, mesh_pos, orient, 10.0f, true);
                    np->hflags.last_pf_failed = 0;
                    return AI_RESULT_SUCCESS;
                }
            }
        }
    }
    timestamp& timer = ai->action_timer;
    if (!timer.valid())
    {
        if (np->movement_mode == HMM_PATHFINDING)
        {
            timer.set(3500);
        }
        else
        {
            timer.set_rand(1000, 2500);
        }
        return AI_RESULT_IN_PROGRESS;
    }
    if (!timer.elapsed())
    {
        return AI_RESULT_IN_PROGRESS;
    }
    nav_cell* last_cell = nav_cell_from_handle(np->pf.last_visited_nav_cell_handle);
    const bool in_last_cell = last_cell != nullptr && nav_cell_is_point_in_cell_column(last_cell, np->pos);
    const int attempt = ai->action_substate + (in_last_cell ? 3 : 0);
    bool moved = false;
    if (attempt == 0)
    {
        moved = unstick_human_pathfind_to_last_dest(np);
    }
    else if (attempt == 1)
    {
        vector mesh_pos;
        nav_cell* cell = nullptr;
        if (rfg_pf_find_closest_point_on_nav_mesh_using_collect(np->pos, false, mesh_pos, cell, 20.0f))
        {
            unstick_human_move_to_point_regardless(np, mesh_pos, cell, false, true);
            moved = true;
        }
    }
    else if (attempt == 2)
    {
        vector v0;
        vector v1;
        vector v2;
        if (last_cell != nullptr && nav_cell_get_all_verts(last_cell, v0, v1, v2))
        {
            vector centre((v1.x + v0.x + v2.x) / 3.0f, (v2.y + v0.y + v1.y) / 3.0f, (v2.z + v0.z + v1.z) / 3.0f);
            unstick_human_move_to_point_regardless(np, centre, last_cell, false, true);
            moved = true;
        }
    }
    else
    {
        vector try_pos(np->pos.x + fl_rand_range(-20.0f, 20.0f), np->pos.y + 0.0f, np->pos.z + fl_rand_range(-20.0f, 20.0f));
        nav_cell* cell = nullptr;
        if (attempt < 6)
        {
            rfg_pf_find_closest_point_on_nav_mesh_using_collect(try_pos, false, try_pos, cell, 50.0f);
        }
        moved = unstick_human_move_to_point_regardless(np, try_pos, cell, 5 < attempt, 4 < attempt);
    }
    ai->action_substate++;
    if (moved)
    {
        timer.invalidate();
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00652940 (:1682)
int yell_at_vehicle_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if (np->vehicle_handle != 0xFFFFFFFF)
    {
        return -1;
    }
    if (np->get_ai()->planner.get_current_action() == this)
    {
        return 60;
    }
    if (Yell_vehicle_timeout.valid() && !Yell_vehicle_timeout.elapsed())
    {
        return -1;
    }
    if (np->is_offscreen() || !should_play_ambient_lines())
    {
        return -1;
    }
    return find_near_miss_vehicles(np) != nullptr ? 120 : -1;
}

// 0x006900A0 (:1723)
ai_results yell_at_vehicle_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    const vehicle* vp = find_near_miss_vehicles(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    Yell_vehicle_timeout.set(5000);
    ai_system* ai = np->get_ai();
    ai->action_obj_handle = vp->get_handle();
    ai->desired_anim_action = ANIMATION_ACTION_REACT_ANGRY_A;
    ai->desired_voice_line = human_teams_get_disposition(np, vp->team_of_most_recent_driver) < TEAM_DISPOSITION_NONE ? VOICE_LINE_VEHICLE_DODGE_UNFRIENDLY : VOICE_LINE_VEHICLE_DODGE_FRIENDLY;
    ai->stop_moving();
    ai->perception.pay_attention(vp->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    np->play_voice_line(ai->desired_voice_line, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    np->char_inst->play_action(ai->desired_anim_action, ANIM_ALIAS_NONE, 1.0f, 0);
    return AI_RESULT_SUCCESS;
}

// 0x005E06B0 (:1754)
ai_results yell_at_vehicle_action::do_action(npc* np) const
{
    Yell_vehicle_timeout.set(5000);
    const ai_system* ai = np->get_ai();
    return np->get_current_voice_line() == ai->desired_voice_line || np->char_inst->is_playing_action(ai->desired_anim_action) ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x00610560 (:1772)
ai_results yell_at_vehicle_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        const ai_system* ai = np->get_ai();
        if (np->char_inst->is_playing_action(ai->desired_anim_action) && np->char_inst->rdd.is_loaded())
        {
            np->char_inst->stop_action(ai->desired_anim_action, 5, false);
        }
        if (np->get_current_voice_line() == ai->desired_voice_line)
        {
            np->stop_voice(VOICE_LINE_PRIORITY_HIGH);
        }
    }
    return AI_RESULT_SUCCESS;
}

// The checks mutter_player_action and say_ambient_line_action share; next is the action's own timeout.
static int ambient_line_cost(const npc* np, const ai_action* action, const timestamp& next, int cost)
{
    const ai_system* ai = np->get_ai();
    if (ALERT_LEVEL_GREEN < gsm_get_alert_level() || kiosk_get_active_kiosk() != nullptr || np->vehicle_handle != 0xFFFFFFFF || np->amb_prop_info != nullptr || (np->current_team == HUMAN_TEAM_MARAUDER && ai->persona_flags.follow_player))
    {
        return -1;
    }
    if (ai->planner.get_current_action() == action)
    {
        return 60;
    }
    if (np->get_current_voice_line_handle() != INVALID_VOICE_LINE_HANDLE || (next.valid() && !next.elapsed()))
    {
        return -1;
    }
    const player* local = player_get_local_player();
    if (!human_teams_are_friendly(local, np->h_info->default_team) || !should_play_ambient_lines() || on_ambient_action_list(np) || 100.0f < vec_dist_squared(local->pos, np->pos))
    {
        return -1;
    }
    return cost;
}

// 0x006105D0 (:1797)
int mutter_player_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return ambient_line_cost(np, this, Mutter_player_timeout, 130);
}

// 0x005E0710 (:1872)
ai_results mutter_player_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    if (np->get_current_voice_line_handle() != INVALID_VOICE_LINE_HANDLE || (Mutter_player_timeout.valid() && !Mutter_player_timeout.elapsed()))
    {
        return AI_RESULT_FAILURE;
    }
    Mutter_player_timeout.set(8000);
    add_mutter_list(np);
    ai_system* ai = np->get_ai();
    const ai_morale_levels morale = ai->get_district_morale_level();
    if (morale == AI_MORALE_LEVEL_LOW)
    {
        ai->desired_voice_line = VOICE_LINE_AMBIENT_TALK_TO_SELF_LOW_MORALE;
    }
    else if (morale == AI_MORALE_LEVEL_MEDIUM)
    {
        ai->desired_voice_line = VOICE_LINE_AMBIENT_TALK_TO_SELF_MEDIUM_MORALE;
    }
    else if (morale == AI_MORALE_LEVEL_HIGH)
    {
        ai->desired_voice_line = VOICE_LINE_AMBIENT_TALK_TO_SELF_HIGH_MORALE;
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return AI_RESULT_FAILURE;
    }
    np->play_voice_line(ai->desired_voice_line, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    return AI_RESULT_SUCCESS;
}

// 0x005E07F0 (:1916)
ai_results mutter_player_action::do_action(npc* np) const
{
    if (np->get_current_voice_line() != np->get_ai()->desired_voice_line)
    {
        return AI_RESULT_SUCCESS;
    }
    Mutter_player_timeout.set(8000);
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E0830 (:1930)
ai_results mutter_player_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this && np->get_current_voice_line() == np->get_ai()->desired_voice_line)
    {
        np->stop_voice(VOICE_LINE_PRIORITY_HIGH);
    }
    return AI_RESULT_SUCCESS;
}

// 0x006106D0 (:1947)
int say_ambient_line_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return ambient_line_cost(np, this, Ambient_vlu_timeout, 120);
}

// 0x005E0870 (:2022)
ai_results say_ambient_line_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    if (np->get_current_voice_line_handle() != INVALID_VOICE_LINE_HANDLE || (Ambient_vlu_timeout.valid() && !Ambient_vlu_timeout.elapsed()))
    {
        return AI_RESULT_FAILURE;
    }
    Ambient_vlu_timeout.set_rand(5000, 15000);
    const voice_line_handle line = voice_line_unlocking_choose(VLUG_AMB_MINER);
    ai_system* ai = np->get_ai();
    ai->action_obj_handle = 0xFFFFFFFF;
    if (line != INVALID_VOICE_LINE_HANDLE && np->play_voice_line(line, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_DIALOGUE_NORMAL, false, false, true, true) == PLAY_VOICE_SUCCESS)
    {
        ai->action_obj_handle = line;
        return AI_RESULT_SUCCESS;
    }
    return AI_RESULT_FAILURE;
}

// 0x005E0910 (:2059)
ai_results say_ambient_line_action::do_action(npc* np) const
{
    if ((unsigned int)np->get_current_voice_line_handle() != np->get_ai()->action_obj_handle)
    {
        return AI_RESULT_SUCCESS;
    }
    Ambient_vlu_timeout.set_rand(5000, 15000);
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E0950 (:2073)
ai_results say_ambient_line_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this && (unsigned int)np->get_current_voice_line_handle() == np->get_ai()->action_obj_handle)
    {
        np->stop_voice(VOICE_LINE_PRIORITY_HIGH);
    }
    return AI_RESULT_SUCCESS;
}

// 0x006529D0 (:2092)
int greet_npc_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if (np->hflags.do_not_play_ambient_or_greet_lines || np->vehicle_handle != 0xFFFFFFFF || np->amb_prop_info != nullptr || 2500.0f < vec_dist_squared(player_get_local_player()->pos, np->pos) || np->is_offscreen() || !should_play_ambient_lines())
    {
        return -1;
    }
    const ai_pedestrian_info& info = np->get_ai()->pedestrian_info;
    const npc* other = !info.npc_greeting_timeout.elapsed() ? World.object_from_handle<npc>(info.greeting_npc_handle) : find_greet_npc(np);
    return other != nullptr ? 100 : -1;
}

// 0x00690180 (:2150)
ai_results greet_npc_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    ai_system* ai = np->get_ai();
    const npc* other;
    if (ai->pedestrian_info.npc_greeting_timeout.elapsed())
    {
        other = find_greet_npc(np);
        ai->action_substate = 1;
    }
    else
    {
        const unsigned int handle = ai->pedestrian_info.greeting_npc_handle;
        const object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
        other = objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_NPC ? static_cast<const npc*>(objp) : nullptr;
        ai->action_substate = 0;
    }
    if (other == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    add_npc_greet_list(np);
    Greet_npc_timeout.set(8000);
    ai->action_obj_handle = other->get_handle();
    ai->perception.pay_attention(other->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    ai->desired_anim_action = pick_greet_animation();
    np->char_inst->play_action(ai->desired_anim_action, ANIM_ALIAS_NONE, 1.0f, 0);
    return AI_RESULT_SUCCESS;
}

// 0x006107D0 (:2197)
ai_results greet_npc_action::do_action(npc* np) const
{
    const ai_system* ai = np->get_ai();
    Greet_npc_timeout.set(8000);
    if (np->char_inst->is_playing_action(ai->desired_anim_action))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (0 < ai->action_substate && ai->action_obj_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(ai->action_obj_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            ai_pedestrian_info& info = static_cast<npc*>(objp)->get_ai()->pedestrian_info;
            info.greeting_npc_handle = np->get_handle();
            info.npc_greeting_timeout.set(1500);
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x00610870 (:2222)
ai_results greet_npc_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        const ai_system* ai = np->get_ai();
        if (np->char_inst->is_playing_action(ai->desired_anim_action) && np->char_inst->rdd.is_loaded())
        {
            np->char_inst->stop_action(ai->desired_anim_action, 5, false);
        }
        if (np->get_current_voice_line() == ai->desired_voice_line)
        {
            np->stop_voice(VOICE_LINE_PRIORITY_HIGH);
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x00690290 (:2247)
int greet_player_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const ai_system* ai = np->get_ai();
    if (np->hflags.do_not_play_ambient_or_greet_lines || ALERT_LEVEL_GREEN < gsm_get_alert_level() || np->vehicle_handle != 0xFFFFFFFF || np->amb_prop_info != nullptr || (np->current_team == HUMAN_TEAM_MARAUDER && ai->persona_flags.follow_player))
    {
        return -1;
    }
    if (Greet_player_timeout.valid() && !Greet_player_timeout.elapsed())
    {
        return -1;
    }
    const player* local = player_get_local_player();
    if (!human_teams_are_friendly(local, np->h_info->default_team) || on_ambient_action_list(np) || 100.0f < vec_dist_squared(local->pos, np->pos))
    {
        return -1;
    }
    vector to_player(local->pos.x - np->pos.x, local->pos.y - np->pos.y, local->pos.z - np->pos.z);
    to_player.normalize_safe();
    if (np->orient.fvec * to_player < PLAYER_GREET_DOT || np->is_offscreen() || !should_play_ambient_lines())
    {
        return -1;
    }
    return ai->perception.has_los_to_position(local->pos, false) ? 110 : -1;
}

// 0x00690440 (:2328)
ai_results greet_player_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    const player* local = player_get_local_player();
    if (local == nullptr || (Greet_player_timeout.valid() && !Greet_player_timeout.elapsed()))
    {
        return AI_RESULT_FAILURE;
    }
    ai_system* ai = np->get_ai();
    Greet_player_timeout.set(8000);
    add_player_greet_list(np);
    ai->perception.pay_attention(local->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    ai->desired_anim_action = pick_greet_animation();
    np->char_inst->play_action(ai->desired_anim_action, ANIM_ALIAS_NONE, 1.0f, 0);
    const ai_morale_levels morale = ai->get_district_morale_level();
    const bool famous = ai_system_consider_player_famous();
    if (morale == AI_MORALE_LEVEL_LOW)
    {
        ai->desired_voice_line = famous ? VOICE_LINE_AMBIENT_PLAYER_GREET_LOW_MORALE_FAMOUS : VOICE_LINE_AMBIENT_PLAYER_GREET_LOW_MORALE;
    }
    else if (morale == AI_MORALE_LEVEL_MEDIUM)
    {
        ai->desired_voice_line = famous ? VOICE_LINE_AMBIENT_PLAYER_GREET_MEDIUM_MORALE_FAMOUS : VOICE_LINE_AMBIENT_PLAYER_GREET_MEDIUM_MORALE;
    }
    else if (morale == AI_MORALE_LEVEL_HIGH)
    {
        ai->desired_voice_line = famous ? VOICE_LINE_AMBIENT_PLAYER_GREET_HIGH_MORALE_FAMOUS : VOICE_LINE_AMBIENT_PLAYER_GREET_HIGH_MORALE;
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return AI_RESULT_FAILURE;
    }
    np->play_voice_line(ai->desired_voice_line, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    return AI_RESULT_SUCCESS;
}

// 0x006108E0 (:2380)
ai_results greet_player_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    Greet_player_timeout.set(8000);
    if (np->char_inst->is_playing_action(ai->desired_anim_action))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    ai->planner.lock_action(false);
    return AI_RESULT_SUCCESS;
}

// 0x00610940 (:2395)
ai_results greet_player_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        ai_system* ai = np->get_ai();
        if (np->char_inst->is_playing_action(ai->desired_anim_action) && np->char_inst->rdd.is_loaded())
        {
            np->char_inst->stop_action(ai->desired_anim_action, 5, false);
        }
        ai->planner.lock_action(false);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E0990 (:2417)
int gawk_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return human_in_vehicle(np) != nullptr ? -1 : 1000;
}

// 0x005E09B0 (:2426)
ai_results gawk_action::do_action(npc* np) const
{
    human_set_stance(np, HST_STAND);
    np->get_ai()->stop_moving();
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E09E0 (:2436)
int gawk_vehicle_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return human_in_vehicle(np) != nullptr ? 1000 : -1;
}

// 0x006109B0 (:2445)
ai_results gawk_vehicle_action::do_action(npc* np) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const int sub_type = vp->get_sub_type();
    if (sub_type == OT_SUB_VEHICLE_AUTO)
    {
        automobile_set_controls_when_abandoned(vp->get_handle());
    }
    else if (sub_type == OT_SUB_VEHICLE_FLYER)
    {
        flyer_set_controls_when_abandoned(static_cast<flyer*>(vp));
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E0A00 (:2463)
int drop_ambient_prop_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 10;
}

// 0x00652A80 (:2468)
ai_results drop_ambient_prop_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    ai_system* ai = np->get_ai();
    if (np->amb_prop_info == nullptr)
    {
        ai->action_timer.set(0);
        ai->desired_anim_action = ANIMATION_ACTION_NONE;
        return AI_RESULT_SUCCESS;
    }
    ai->action_timer.set(250);
    const animation_action anim = np->amb_prop_info->prop_exit_anim;
    ai->desired_anim_action = anim;
    if (anim != ANIMATION_ACTION_NONE)
    {
        np->char_inst->play_action(anim, ANIM_ALIAS_NONE, 1.0f, 0);
    }
    ai->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    ai->planner.lock_action(true);
    return AI_RESULT_SUCCESS;
}

// 0x005E0A10 (:2503)
ai_results drop_ambient_prop_action::do_action(npc* np) const
{
    const ai_system* ai = np->get_ai();
    const animation_action anim = ai->desired_anim_action;
    if (anim != ANIMATION_ACTION_NONE && np->amb_prop_info != nullptr && -1 < np->amb_prop_info->exit_anim_trigger_id && np->char_inst->is_playing_action(anim) && vanim_has_trigger_elapsed(np->char_inst->vp, np->amb_prop_info->exit_anim_trigger_id, anim, -1))
    {
        amb_prop_detach_props(np, false);
    }
    if (!ai->action_timer.elapsed())
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (anim != ANIMATION_ACTION_NONE && np->char_inst->is_playing_action(anim))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (np->amb_prop_info != nullptr)
    {
        amb_prop_detach_props(np, false);
    }
    return AI_RESULT_SUCCESS;
}

// 0x00610A20 (:2540)
ai_results drop_ambient_prop_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        ai_system* ai = np->get_ai();
        ai->planner.lock_action(false);
        if (np->amb_prop_info != nullptr)
        {
            amb_prop_detach_props(np, false);
        }
        const animation_action anim = ai->desired_anim_action;
        if (anim != ANIMATION_ACTION_NONE && np->char_inst->is_playing_action(anim) && np->char_inst->rdd.is_loaded())
        {
            np->char_inst->stop_action(anim, 5, false);
        }
    }
    return AI_RESULT_SUCCESS;
}

do_action_node_action do_action_node_action::the_instance; // 0x01D752C4

// 0x0060D870 (.h:13)
do_action_node_action::do_action_node_action()
    : ai_action("do action node", ai_plan_conditions_from_bits({9, 47, 58, 134, 145, 153}), ai_plan_conditions_from_bits({133}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* do_action_node_action::get_instance()
{
    return &the_instance;
}

// 0x005E03A0
do_action_node_action::~do_action_node_action()
{
}

// 0x0060DAA0 (.h:13)
int do_action_node_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

goto_action_node_action goto_action_node_action::the_instance; // 0x01DC9F38

// 0x0060D5F0 (.h:12)
goto_action_node_action::goto_action_node_action()
    : ai_action("goto action node", ai_plan_conditions_from_bits({9, 47, 58, 136, 145, 153}), ai_plan_conditions_from_bits({135}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* goto_action_node_action::get_instance()
{
    return &the_instance;
}

// 0x005E0390
goto_action_node_action::~goto_action_node_action()
{
}

// 0x0060D830 (.h:12)
int goto_action_node_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

enter_action_node_action enter_action_node_action::the_instance; // 0x01D9F92C

// 0x0060DAD0 (.h:16)
enter_action_node_action::enter_action_node_action()
    : ai_action("enter action node", ai_plan_conditions_from_bits({9, 47, 58, 135, 145, 153}), ai_plan_conditions_from_bits({134}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* enter_action_node_action::get_instance()
{
    return &the_instance;
}

// 0x005E03B0
enter_action_node_action::~enter_action_node_action()
{
}

// 0x0060DD10 (.h:16)
int enter_action_node_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

exit_action_node_action exit_action_node_action::the_instance; // 0x01DC9BA8

// 0x0060DD40 (.h:17)
exit_action_node_action::exit_action_node_action()
    : ai_action("exit action node", ai_plan_conditions_from_bits({9, 47, 58, 145, 153}), ai_plan_conditions_from_bits({136}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* exit_action_node_action::get_instance()
{
    return &the_instance;
}

// 0x005E03C0
exit_action_node_action::~exit_action_node_action()
{
}

// 0x0060DF00 (.h:17)
int exit_action_node_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

greet_npc_action greet_npc_action::the_instance; // 0x01DDB8F0

// 0x0060DF40 (.h:20)
greet_npc_action::greet_npc_action()
    : ai_action("greet npc action", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({128, 130}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* greet_npc_action::get_instance()
{
    return &the_instance;
}

// 0x005E03D0
greet_npc_action::~greet_npc_action()
{
}

// 0x0060E210 (.h:20)
int greet_npc_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

greet_player_action greet_player_action::the_instance; // 0x01DDACA0

// 0x0060E250 (.h:21)
greet_player_action::greet_player_action()
    : ai_action("greet player action", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({128, 130}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* greet_player_action::get_instance()
{
    return &the_instance;
}

// 0x005E03E0
greet_player_action::~greet_player_action()
{
}

// 0x0060E520 (.h:21)
int greet_player_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

mutter_player_action mutter_player_action::the_instance; // 0x01D682D0

// 0x0060E560 (.h:22)
mutter_player_action::mutter_player_action()
    : ai_action("mutter player action", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({128, 130}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* mutter_player_action::get_instance()
{
    return &the_instance;
}

// 0x005E03F0
mutter_player_action::~mutter_player_action()
{
}

// 0x0060E830 (.h:22)
int mutter_player_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

say_ambient_line_action say_ambient_line_action::the_instance; // 0x01D68844

// 0x0060E870 (.h:23)
say_ambient_line_action::say_ambient_line_action()
    : ai_action("say ambient line action", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({128, 130}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* say_ambient_line_action::get_instance()
{
    return &the_instance;
}

// 0x005E0400
say_ambient_line_action::~say_ambient_line_action()
{
}

// 0x0060EB40 (.h:23)
int say_ambient_line_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

unstick_action unstick_action::the_instance; // 0x01DC9A7C

// 0x0060EB80 (.h:26)
unstick_action::unstick_action()
    : ai_action("unstick", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({11}))
{
    flags.has_enter = 1;
}

ai_action* unstick_action::get_instance()
{
    return &the_instance;
}

// 0x005E0410
unstick_action::~unstick_action()
{
}

// 0x0060EE20 (.h:26)
int unstick_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

yell_at_vehicle_action yell_at_vehicle_action::the_instance; // 0x01DDB10C

// 0x0060EE60 (.h:29)
yell_at_vehicle_action::yell_at_vehicle_action()
    : ai_action("yell at vehicle action", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({128, 130}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* yell_at_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005E0420
yell_at_vehicle_action::~yell_at_vehicle_action()
{
}

// 0x0060F130 (.h:29)
int yell_at_vehicle_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

gawk_action gawk_action::the_instance; // 0x01DA06A8

// 0x0060F170 (.h:32)
gawk_action::gawk_action()
    : ai_action("gawk", ai_plan_conditions_from_bits({9, 47, 58, 136, 145, 153}), ai_plan_conditions_from_bits({138}))
{
}

ai_action* gawk_action::get_instance()
{
    return &the_instance;
}

// 0x005E0430
gawk_action::~gawk_action()
{
}

// 0x0060F3A0 (.h:32)
int gawk_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

gawk_vehicle_action gawk_vehicle_action::the_instance; // 0x01D751A4

// 0x0060F3E0 (.h:33)
gawk_vehicle_action::gawk_vehicle_action()
    : ai_action("gawk in vehicle", ai_plan_conditions_from_bits({9, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({138}))
{
}

ai_action* gawk_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005E0440
gawk_vehicle_action::~gawk_vehicle_action()
{
}

// 0x0060F620 (.h:33)
int gawk_vehicle_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

drop_ambient_prop_action drop_ambient_prop_action::the_instance; // 0x01DA0368

// 0x0060F660 (.h:36)
drop_ambient_prop_action::drop_ambient_prop_action()
    : ai_action("drop ambient prop", ai_plan_conditions_from_bits({9, 47, 58, 136, 145, 153}), ai_plan_conditions_from_bits({137}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* drop_ambient_prop_action::get_instance()
{
    return &the_instance;
}

// 0x005E0450
drop_ambient_prop_action::~drop_ambient_prop_action()
{
}

// 0x0060F8A0 (.h:36)
int drop_ambient_prop_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

