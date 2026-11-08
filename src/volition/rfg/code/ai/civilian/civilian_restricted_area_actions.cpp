// Original: volition/rfg/code/ai/civilian/civilian_restricted_area_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/civilian/civilian_restricted_area_actions.h"

#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/belief_iterator.h"
#include "volition/rfg/code/ai/belief_iterator.inl"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/self_preservation_actions.h"
#include "volition/rfg/code/gameplay/activities/morale_effect_manager.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_internal_shared.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_restricted_area.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/pathfinding/nav_heap.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"

#include <float.h>
#include <math.h>

// Position 4 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG04")

extern nav_heap Nav_heap; // nav_heap.cpp

// 0x01DCA3B4, from the dynamic initializer at 0x01062050: the cosine of 0x012C95F8, which is 5 * 0.017453f in float, widened.
static const float FACING_TARGET_DOT = (float)cos((double)(5.0f * 0.017453f));
// 0x01D75544, from the dynamic initializer at 0x01061FC0, the postconditions of threaten_civilian_goal and oppress_civilian_goal.
static const ai_plan_conditions THREATEN_CIVILIAN_GOAL_CONDITIONS = ai_plan_conditions_from_bits({104, 106});

// 0x005E13C0 (:36)
bool point_satisfies_search(float (*heuristic_func)(const vector&, void*), const vector& pos, void* heuristic_data)
{
    return heuristic_func(pos, heuristic_data) <= 0.0f;
}

// 0x00654F80 (:109). LTCG: start_pos EAX, the rest on the stack. A* over the nav mesh for the nearest point the heuristic answers 0 or less for.
static bool search_for_flee_destination_pos(vector& destination_pos, const vector& start_pos, nav_cell* start_cell, float (*heuristic_func)(const vector&, void*), void* heuristic_data)
{
    Nav_heap.insert_cell(start_cell, 0.0f, heuristic_func(start_pos, heuristic_data), -1, start_pos);
    for (nav_cell* cell = Nav_heap.remove_best_cell(); cell != nullptr; cell = Nav_heap.remove_best_cell())
    {
        nav_heap_cell_data* data = Nav_heap.get_cell_data(cell);
        if (data->arrival_edge != -1 && data->est_cost_to_goal <= 0.0f)
        {
            destination_pos = data->arrival_pos;
            return true;
        }
        for (int edge = 0; edge < 3; edge++)
        {
            if (edge == data->arrival_edge)
            {
                continue;
            }
            nav_cell* next = cell->adjacent_cells[edge].to_pointer();
            if (next == nullptr || (next->flags & 8) != 0) // impassable
            {
                continue;
            }
            nav_heap_cell_data* next_data = Nav_heap.get_cell_data(next);
            if (next_data != nullptr && !next_data->is_open)
            {
                continue;
            }
            vector centroid;
            nav_cell_get_centroid(&centroid, next);
            const float cost = vec_dist(data->arrival_pos, centroid) + data->arrival_cost;
            int back_edge = 0;
            while (back_edge < 3 && next->adjacent_cells[back_edge].to_pointer() != cell)
            {
                back_edge++;
            }
            if (back_edge == 3)
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                back_edge = -1;
            }
            if (next_data == nullptr)
            {
                Nav_heap.insert_cell(next, cost, heuristic_func(centroid, heuristic_data), (char)back_edge, centroid);
            }
            else if (cost < next_data->arrival_cost)
            {
                next_data->arrival_cost = cost;
                next_data->arrival_edge = (char)back_edge;
                Nav_heap.adjust_cell(next_data->heap_index);
            }
        }
    }
    return false;
}

// 0x00674D40 (:220). LTCG: np ESI.
static bool find_flee_destination_pos(vector& destination_pos, const npc* np, float (*heuristic_func)(const vector&, void*), void* heuristic_data)
{
    if (np == nullptr || heuristic_func == nullptr)
    {
        return false;
    }
    nav_cell* cell;
    navpoint* point;
    if (!np->get_ai()->perception.get_nearest_navpoint_or_navcell(cell, point, nullptr, nullptr) || cell == nullptr)
    {
        return false;
    }
    Nav_heap.start_session();
    const bool found = search_for_flee_destination_pos(destination_pos, np->pos, cell, heuristic_func, heuristic_data);
    Nav_heap.end_session();
    return found;
}

// 0x005E13F0 (:246)
bool do_get_cost_tests(const npc* np, float (*heuristic_func)(const vector&, void*), void* heuristic_data)
{
    return 0.0f < heuristic_func(np->pos, heuristic_data);
}

// 0x00674DC0 (:253)
ai_results flee_enter_common(npc* np, bool first_frame, float (*heuristic_func)(const vector&, void*), void* heuristic_data)
{
    vector& dest = np->get_ai()->desired_destination;
    if (!first_frame && 1.0f <= vec_dist_squared(dest, np->pos) && heuristic_func(dest, heuristic_data) <= 0.0f)
    {
        return AI_RESULT_SUCCESS;
    }
    return find_flee_destination_pos(dest, np, heuristic_func, heuristic_data) ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x00690C00 (:268)
ai_results flee_do_action_common(npc* np, float (*heuristic_func)(const vector&, void*), void* heuristic_data)
{
    ai_system* ai = np->get_ai();
    if (heuristic_func(np->pos, heuristic_data) <= 0.0f)
    {
        return AI_RESULT_SUCCESS;
    }
    if (ai->is_pathing_to(ai->desired_destination, 0.25f, 1.0f))
    {
        if (np->movement_mode == HMM_PATHFINDING)
        {
            np->cur_movement_state = HMS_RUN;
        }
        return AI_RESULT_IN_PROGRESS;
    }
    const ai_pathfind_results result = ai->pathfind_to(ai->desired_destination, HMS_RUN, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false);
    return result != AI_PF_SUCCESS && AI_PF_RAGDOLLED < result ? AI_RESULT_FAILURE : AI_RESULT_IN_PROGRESS;
}

// 0x005E1430 (:310)
static float flee_restricted_region_heuristic(const vector& pos, void*)
{
    return restricted_area_in_any_region(pos, true) ? 1.0f : 0.0f;
}

// 0x005E1470 (:316)
int flee_restricted_region_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return restricted_area_in_any_region(np->pos, true) ? 3000 : -1;
}

// 0x00674E30 (:321)
ai_results flee_restricted_region_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        np->play_voice_line(VOICE_LINE_PATROL_REQUEST_MOVE_CIVILIAN_SUBMISSIVE_RESPONSE, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    }
    return flee_enter_common(np, last_action != this, flee_restricted_region_heuristic, nullptr);
}

// 0x00690CA0 (:330)
ai_results flee_restricted_region_action::do_action(npc* np) const
{
    return flee_do_action_common(np, flee_restricted_region_heuristic, nullptr);
}

// 0x006135E0 (:345). How far inside 25 m of the nearest unfriendly the point is.
static float flee_unfriendly_npcs_heuristic(const vector& pos, void* heuristic_data)
{
    const farray<vector, 15>& locations = *static_cast<const farray<vector, 15>*>(heuristic_data);
    float closest = FLT_MAX;
    for (int i = 0; i < locations.size(); i++)
    {
        const float dist_sq = vec_dist_squared(pos, locations[i]);
        if (dist_sq - closest < 0.0f)
        {
            closest = dist_sq;
        }
    }
    const float inside = 25.0f - sqrtf(closest);
    return 0.0f - inside < 0.0f ? inside : 0.0f;
}

// 0x00674EC0 (:368). LTCG: loc_list EDI, np EAX.
static void build_unfriendly_location_list(farray<vector, 15>& loc_list, const npc* np)
{
    belief_obj_iterator<human> it;
    it.init(np->get_ai(), AI_BELIEF_UNFRIENDLY);
    while (it)
    {
        const human* hp = it;
        if (loc_list.full())
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
        else
        {
            loc_list.add(hp->pos);
        }
        ++it;
    }
}

// 0x00674F90 (:379)
int flee_threatening_npc_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    farray<vector, 15> locations;
    build_unfriendly_location_list(locations, np);
    return flee_unfriendly_npcs_heuristic(np->pos, &locations) <= 0.0f ? -1 : 5000;
}

// 0x00690CC0 (:386)
ai_results flee_threatening_npc_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        ai->action_timer.set_rand(500, 800);
        ai->action_substate = 0;
    }
    bool first_frame = false;
    if (ai->action_substate == 0)
    {
        if (!ai->action_timer.elapsed())
        {
            return AI_RESULT_IN_PROGRESS;
        }
        const ai_belief* belief = ai->perception.get_first_belief(AI_BELIEF_UNFRIENDLY);
        if (belief != nullptr)
        {
            ai->perception.pay_attention(belief->handle, belief->get_type(), ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
        }
        np->play_voice_line(ai->persona_flags.always_flee_edf ? VOICE_LINE_CIVILIAN_SPOT_EDF_AND_LEAVE : VOICE_LINE_PATROL_REQUEST_MOVE_CIVILIAN_SUBMISSIVE_RESPONSE, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        ai->action_substate = 1;
        first_frame = true;
    }
    farray<vector, 15> locations;
    build_unfriendly_location_list(locations, np);
    return flee_enter_common(np, first_frame, flee_unfriendly_npcs_heuristic, &locations);
}

// 0x00690E30 (:421)
ai_results flee_threatening_npc_action::do_action(npc* np) const
{
    farray<vector, 15> locations;
    build_unfriendly_location_list(locations, np);
    return flee_do_action_common(np, flee_unfriendly_npcs_heuristic, &locations);
}

// The first unfriendly npc that is threatening a civilian from the ground, or null. Inlined in both get_cost functions.
static npc* find_threatening_npc(const ai_system* ai)
{
    belief_obj_iterator<npc> it;
    it.init(ai, AI_BELIEF_UNFRIENDLY);
    while (it)
    {
        npc* hp = it;
        const ai_goal* goal = hp->get_ai()->planner.get_current_goal();
        if (goal != nullptr && goal->pursues_condition(THREATEN_CIVILIAN_GOAL_CONDITIONS) && human_combat_get_manned_turret(hp) == nullptr)
        {
            return hp;
        }
        ++it;
    }
    return nullptr;
}

// 0x00675030 (:431)
int confront_threatening_npc_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    if (!ai->persona_flags.stand_up_to_edf || AI_OPPRESSION_CHECK_ID < ai->oppression_action)
    {
        return -1;
    }
    npc* threatener = find_threatening_npc(ai);
    if (threatener == nullptr)
    {
        return -1;
    }
    my_data.use_target = threatener;
    return 4000;
}

// 0x00690EB0 (:467)
void confront_threatening_npc_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    if (setup_data.use_target != nullptr)
    {
        np->get_ai()->perception.pay_attention(setup_data.use_target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    }
}

// 0x005E14A0 (:473)
ai_results confront_threatening_npc_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        ai_system* ai = np->get_ai();
        ai->action_timer.set_rand(500, 800);
        ai->action_substate = 0;
    }
    return AI_RESULT_SUCCESS;
}

static const object* get_attention_npc(const ai_system* ai)
{
    const object* objp = World.object_from_handle(ai->perception.get_attention_handle());
    return objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_NPC ? objp : nullptr;
}

// 0x006551C0 (:491)
ai_results confront_threatening_npc_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const object* target = get_attention_npc(ai);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (ai->action_substate == 0)
    {
        if (!ai->action_timer.elapsed())
        {
            return AI_RESULT_IN_PROGRESS;
        }
        ai->stop_moving();
        vector dir(target->pos.x - np->pos.x, target->pos.y - np->pos.y, target->pos.z - np->pos.z);
        dir.flatten_normalize();
        np->steering_vector = dir;
        if (FACING_TARGET_DOT <= np->orient.fvec.y * dir.y + np->orient.fvec.x * dir.x + np->orient.fvec.z * dir.z)
        {
            ai->action_substate = 1;
        }
        return AI_RESULT_IN_PROGRESS;
    }
    if (ai->action_substate == 1)
    {
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        np->char_inst->play_action(50 < vrand() % 100 ? ANIMATION_ACTION_REACT_ANGRY_C : ANIMATION_ACTION_REACT_ANGRY_B, ANIM_ALIAS_TAUNT, 1.0f, 0);
        vector to_target(target->pos.x - np->pos.x, target->pos.y - np->pos.y, target->pos.z - np->pos.z);
        const vector dir = to_target.copy_flatten_normalize();
        matrix orient;
        orient.make_quick(dir);
        np->char_inst->set_action_dest_orient(np->orient, orient, ANIM_ALIAS_TAUNT, 0.5f);
        np->play_voice_line(VOICE_LINE_PATROL_REQUEST_MOVE_CIVILIAN_DEFIANT_RESPONSE, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        ai->action_substate = 2;
        return AI_RESULT_IN_PROGRESS;
    }
    if (ai->action_substate != 2)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (np->get_current_voice_line_handle() == INVALID_VOICE_LINE_HANDLE)
    {
        np->char_inst->stop_action(ANIM_ALIAS_TAUNT, 10, false);
    }
    if (np->char_inst->is_playing_action(ANIM_ALIAS_TAUNT))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    // rfg.exe inlines the whole of set_attention_mode here, the mode argument being the current one (0x00655370 on).
    const attention_look_at_modes mode = ai->perception.get_attention_mode();
    ai->perception.set_attention_mode(mode, mode == ATTENTION_SEARCH || mode == ATTENTION_ACQUIRE_VISUAL ? ATTENTION_STEERING_PLANT_FEET : ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    ai->action_substate = 3;
    return AI_RESULT_IN_PROGRESS;
}

// 0x006136B0 (:544)
ai_results confront_threatening_npc_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        if (np->char_inst->rdd.is_loaded())
        {
            np->char_inst->stop_action(ANIM_ALIAS_TAUNT, 5, true);
        }
        const ai_belief* belief = np->get_ai()->perception.get_first_belief(AI_BELIEF_UNFRIENDLY);
        if (belief != nullptr && belief->handle != 0xFFFFFFFF)
        {
            const object* enemy = World.object_from_handle(belief->handle);
            if (enemy != nullptr && enemy->get_obj_type() == OT_HUMAN && morale_maybe_convert_civilian_spontaneously(np, static_cast<const human*>(enemy), 0.5f))
            {
                morale_maybe_convert_civilians_spontaneously_around_event(np->pos, 30.0f, static_cast<const human*>(enemy), 0.5f);
            }
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x00675120 (:573)
int cower_from_threatening_npc_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    if (!ai->persona_flags.cower_from_edf || AI_OPPRESSION_CHECK_ID < ai->oppression_action)
    {
        return -1;
    }
    npc* threatener = find_threatening_npc(ai);
    if (threatener == nullptr)
    {
        return -1;
    }
    my_data.use_target = threatener;
    return 4010;
}

// 0x00690EE0 (:609)
void cower_from_threatening_npc_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    if (setup_data.use_target != nullptr)
    {
        np->get_ai()->perception.pay_attention(setup_data.use_target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    }
}

// 0x005E14E0 (:615)
ai_results cower_from_threatening_npc_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        ai_system* ai = np->get_ai();
        ai->action_timer.set_rand(500, 800);
        ai->action_substate = 0;
    }
    return AI_RESULT_SUCCESS;
}

// 0x006559A0 (:635)
ai_results cower_from_threatening_npc_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const object* target = get_attention_npc(ai);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (!ai->action_timer.elapsed())
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (ai->action_substate == 0)
    {
        ai->stop_moving();
        vector dir(target->pos.x - np->pos.x, target->pos.y - np->pos.y, target->pos.z - np->pos.z);
        dir.flatten_normalize();
        np->steering_vector = dir;
        if (FACING_TARGET_DOT <= np->orient.fvec.y * dir.y + np->orient.fvec.x * dir.x + np->orient.fvec.z * dir.z)
        {
            ai->action_substate = 1;
        }
    }
    else if (ai->action_substate == 1)
    {
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        ai->stop_moving();
        human_override_default_animation_state(np, true);
        np->char_inst->set_state(index_alias(ANIMATION_STATE_AMB_COWER_STAND, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
        ai->action_substate = 2;
        np->play_voice_line(VOICE_LINE_CIVILIAN_COWER_EDF, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00655BD0 (:672)
ai_results cower_from_threatening_npc_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this && next_action != get_on_knees_action::get_instance())
    {
        ai_maybe_play_cower_to_stand_animation(np, next_action);
        human_override_default_animation_state(np, false);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E1520 (:685)
int get_on_knees_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return human_team_is_non_combatant(np) && np->get_ai()->oppression_action == AI_OPPRESSION_GET_ON_KNEES ? 10 : -1;
}

// 0x00613760 (:696)
ai_results get_on_knees_action::enter(npc* np, const ai_action* last_action) const
{
    if (np->current_team != HUMAN_TEAM_CIVILIAN)
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        ai_system* ai = np->get_ai();
        ai->action_state = 0;
        ai->action_timer.set_rand(1000, 1500);
        ai->stop_moving();
        human_set_stance(np, HST_STAND);
        ai->planner.lock_action(true);
    }
    return AI_RESULT_SUCCESS;
}

// The chance is a local static in each of the two actions below: complying makes the next civilian likelier to run, running less likely.
static void raise_run_away_chance(int& chance)
{
    chance += 10;
    if (99 < chance)
    {
        chance = 100;
    }
}

static void civilian_makes_a_run_for_it(npc* np, int& chance)
{
    np->get_ai()->oppression_action = AI_OPPRESSION_SHOOT_THIS_GUY;
    np->play_voice_line(VOICE_LINE_PANIC, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    np->hit_points = 100.0f;
    chance = chance - 10 < 1 ? 0 : chance - 10;
}

// 0x00655C00 (:725)
ai_results get_on_knees_action::do_action(npc* np) const
{
    static int RUN_AWAY_CHANCE = 50; // 0x01647FD8
    ai_system* ai = np->get_ai();
    if (np->current_team != HUMAN_TEAM_CIVILIAN)
    {
        return AI_RESULT_FAILURE;
    }
    if (ai->oppression_action != AI_OPPRESSION_GET_ON_KNEES)
    {
        return AI_RESULT_SUCCESS;
    }
    if (ai->action_state != 0)
    {
        return ai->action_timer.time_since() < 20001 ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
    }
    if (!ai->action_timer.elapsed())
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if ((unsigned int)RUN_AWAY_CHANCE < vrand() % 100)
    {
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        human_override_default_animation_state(np, true);
        np->char_inst->set_state(index_alias(ANIMATION_STATE_AMB_COWER_CROUCH, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
        np->char_inst->play_action(AMIMATION_ACTION_COWER_TURN, ANIM_ALIAS_NONE, 1.0f, 0x4040);
        raise_run_away_chance(RUN_AWAY_CHANCE);
        ai->action_state = 1;
        return AI_RESULT_IN_PROGRESS;
    }
    civilian_makes_a_run_for_it(np, RUN_AWAY_CHANCE);
    return AI_RESULT_SUCCESS;
}

// 0x00655D90 (:778)
ai_results get_on_knees_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        ai_system* ai = np->get_ai();
        if (ai->oppression_action != AI_OPPRESSION_SHOOT_THIS_GUY)
        {
            ai->oppression_action = AI_OPPRESSION_NONE;
        }
        if (np->char_inst->get_animation_state() == ANIMATION_STATE_AMB_COWER_CROUCH)
        {
            ai_maybe_play_cower_to_stand_animation(np, next_action);
            human_override_default_animation_state(np, false);
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E1560 (:800)
int civilian_show_id_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return human_team_is_non_combatant(np) && np->get_ai()->oppression_action == AI_OPPRESSION_SHOW_ID ? 10 : -1;
}

// 0x005E15A0 (:811)
ai_results civilian_show_id_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        ai_system* ai = np->get_ai();
        ai->stop_moving();
        human_set_stance(np, HST_STAND);
        ai->action_timer.set_rand(2000, 3000);
    }
    return AI_RESULT_SUCCESS;
}

// 0x006137D0 (:827)
ai_results civilian_show_id_action::do_action(npc* np) const
{
    static int RUN_AWAY_CHANCE = 50; // 0x01645408
    ai_system* ai = np->get_ai();
    if (human_is_flinching(np))
    {
        return AI_RESULT_SUCCESS;
    }
    if (np->char_inst->is_playing_action(ANIM_ALIAS_SCRIPTED))
    {
        ai->action_timer.invalidate();
    }
    if (ai->action_timer.valid() && ai->action_timer.elapsed())
    {
        ai->action_timer.invalidate();
        if (vrand() % 100 <= (unsigned int)RUN_AWAY_CHANCE)
        {
            civilian_makes_a_run_for_it(np, RUN_AWAY_CHANCE);
            return AI_RESULT_SUCCESS;
        }
        raise_run_away_chance(RUN_AWAY_CHANCE);
    }
    if (ai->oppression_action != AI_OPPRESSION_SHOW_ID && !np->char_inst->is_playing_action(ANIM_ALIAS_SCRIPTED))
    {
        return AI_RESULT_SUCCESS;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E15F0 (:871)
ai_results civilian_show_id_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        ai_system* ai = np->get_ai();
        if (ai->oppression_action != AI_OPPRESSION_SHOOT_THIS_GUY)
        {
            ai->oppression_action = AI_OPPRESSION_NONE;
        }
        if (np->char_inst->is_playing_action(ANIM_ALIAS_SCRIPTED))
        {
            np->char_inst->stop_action(ANIM_ALIAS_SCRIPTED, 5, false);
        }
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

flee_restricted_region_action flee_restricted_region_action::the_instance; // 0x01DC9754

// 0x00612400 (.h:7)
flee_restricted_region_action::flee_restricted_region_action()
    : ai_action("flee restricted region", ai_plan_conditions_from_bits({9, 14, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({107}))
{
    flags.has_enter = 1;
}

ai_action* flee_restricted_region_action::get_instance()
{
    return &the_instance;
}

// 0x005E1330
flee_restricted_region_action::~flee_restricted_region_action()
{
}

// 0x00612700 (.h:7)
int flee_restricted_region_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

flee_threatening_npc_action flee_threatening_npc_action::the_instance; // 0x01D74F70

// 0x00612740 (.h:8)
flee_threatening_npc_action::flee_threatening_npc_action()
    : ai_action("flee threatening npc", ai_plan_conditions_from_bits({9, 14, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({107}))
{
    flags.has_enter = 1;
}

ai_action* flee_threatening_npc_action::get_instance()
{
    return &the_instance;
}

// 0x005E1340
flee_threatening_npc_action::~flee_threatening_npc_action()
{
}

// 0x00612A40 (.h:8)
int flee_threatening_npc_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

confront_threatening_npc_action confront_threatening_npc_action::the_instance; // 0x01DDBA04

// 0x00612A80 (.h:9)
confront_threatening_npc_action::confront_threatening_npc_action()
    : ai_action("confront threatening npc", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({107}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* confront_threatening_npc_action::get_instance()
{
    return &the_instance;
}

// 0x005E1360
confront_threatening_npc_action::~confront_threatening_npc_action()
{
}

// 0x00612D20 (.h:9)
int confront_threatening_npc_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

cower_from_threatening_npc_action cower_from_threatening_npc_action::the_instance; // 0x01DCA2AC

// 0x00612D50 (.h:10)
cower_from_threatening_npc_action::cower_from_threatening_npc_action()
    : ai_action("cower from threatening npc", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({107}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* cower_from_threatening_npc_action::get_instance()
{
    return &the_instance;
}

// 0x005E1370
cower_from_threatening_npc_action::~cower_from_threatening_npc_action()
{
}

// 0x00612FF0 (.h:10)
int cower_from_threatening_npc_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

get_on_knees_action get_on_knees_action::the_instance; // 0x01D72628

// 0x00613020 (.h:11)
get_on_knees_action::get_on_knees_action()
    : ai_action("get on knees", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({107}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* get_on_knees_action::get_instance()
{
    return &the_instance;
}

// 0x005E1390
get_on_knees_action::~get_on_knees_action()
{
}

// 0x006132C0 (.h:11)
int get_on_knees_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

civilian_show_id_action civilian_show_id_action::the_instance; // 0x01DC9CA8

// 0x00613300 (.h:12)
civilian_show_id_action::civilian_show_id_action()
    : ai_action("show ID", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({107}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* civilian_show_id_action::get_instance()
{
    return &the_instance;
}

// 0x005E13B0
civilian_show_id_action::~civilian_show_id_action()
{
}

// 0x006135A0 (.h:12)
int civilian_show_id_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

