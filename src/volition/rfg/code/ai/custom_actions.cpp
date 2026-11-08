// Original: volition/rfg/code/ai/custom_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/custom_actions.h"

#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/belief_iterator.h"
#include "volition/rfg/code/ai/belief_iterator.inl"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/despawn_actions.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_house_arrest.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_internal_shared.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/humans/human_voice/voice_line_unlocking.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_guard_node.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/vehicle/vehicle_audio.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"

#include <float.h>
#include <math.h>

// Position 18 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG18")

extern gfm_orders_manager Orders_manager; // gameplay.cpp

float Sniper_prefer_enemy_xz_dist; // 0x01D6719C
float Sniper_prefer_enemy_y_dist; // 0x01D65968
int Sniper_attack_player_chance; // 0x01D65950
static tweak_table_item Tweak_table_item_Sniper_prefer_enemy_xz_dist("Sniper_prefer_enemy_xz_dist", &Sniper_prefer_enemy_xz_dist);
static tweak_table_item Tweak_table_item_Sniper_prefer_enemy_y_dist("Sniper_prefer_enemy_y_dist", &Sniper_prefer_enemy_y_dist);
static tweak_table_item Tweak_table_item_Sniper_attack_player_chance("Sniper_attack_player_chance", &Sniper_attack_player_chance);

static const float NEAR_TARGET_DIST_SQ = 0.48999998f; // 0.7 m
static const char* const SABOTEUR_BOMB_NAME = "singularity_EBS";

// 0x0065C900 (:128)
bool is_too_close_to_npc(unsigned int npc_handle, const vector& pos)
{
    const int num_npcs = World.get_num_objects<npc>();
    for (int i = 0; i < num_npcs; i++)
    {
        const npc* objp = World.get_object<npc>(i);
        if (npc_handle != objp->get_handle() && vec_xz_dist_squared(pos, objp->pos) < 2.25f)
        {
            return true;
        }
    }
    return false;
}

// 0x0065C990 (:148). The centre of the nearest passable nav cell within 10 m of the point that no other npc stands on.
bool move_with_human_teleport_destination(unsigned int npc_handle, const vector& desired_teleport_pos, const float MAX_DIST_SQRD, vector& teleport_pos)
{
    vector ground = desired_teleport_pos;
    float terrain_height;
    if (havok_terrain_height(terrain_height, desired_teleport_pos.x, desired_teleport_pos.z, false, nullptr))
    {
        ground.y = terrain_height;
    }
    nav_cell* cells[80];
    const int num_cells = havok_collect_nav_cells(ground, 10.0f, cells, 80, false);
    if (num_cells < 0)
    {
        return false;
    }
    const nav_cell* best = nullptr;
    float best_dist_sq = MAX_DIST_SQRD;
    vector best_pos;
    best_pos.zero();
    for (int i = 0; i < num_cells; i++)
    {
        const nav_cell* cell = cells[i];
        if ((cell->flags & 8) != 0) // impassable
        {
            continue;
        }
        vector centroid;
        nav_cell_get_centroid(&centroid, cell);
        const float dist_sq = vec_dist_squared(ground, centroid);
        if (dist_sq <= best_dist_sq && !is_too_close_to_npc(npc_handle, centroid))
        {
            best = cell;
            best_dist_sq = dist_sq;
            best_pos = centroid;
        }
    }
    if (best == nullptr)
    {
        return false;
    }
    teleport_pos = best_pos;
    return true;
}

// 0x005E8040 (:212). start_pos is dropped by LTCG: rfg.exe searches from the local player.
static bool find_drop_off_position_cell(vector& drop_off_pos)
{
    vector closest;
    nav_cell* cell;
    const bool found = bb_pf_find_closest_point_on_nav_mesh_slow(player_get_local_player()->pos, closest, cell);
    if (found)
    {
        nav_cell_get_centroid(&drop_off_pos, cell);
    }
    return found;
}

// 0x005E8080 (:232). LTCG: drop_off_pos ESI.
static bool find_drop_off_position_player(vector& drop_off_pos)
{
    const nav_cell* cell = bb_pf_get_nav_cell(player_get_local_player()->pos, true, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
    if (cell != nullptr && (cell->flags & 8) == 0)
    {
        nav_cell_get_centroid(&drop_off_pos, cell);
        return true;
    }
    return find_drop_off_position_cell(drop_off_pos);
}

// 0x005E80F0 (:260). LTCG: np ESI.
static void return_to_action_node_state(npc* np)
{
    object_action_node* node = np->get_current_action_node(false);
    if (node == nullptr)
    {
        return;
    }
    const animation_state state = node->get_animation_state(np->get_action_node_index(node));
    if (state == ANIMATION_STATE_NONE)
    {
        human_override_default_animation_state(np, false);
        return;
    }
    human_override_default_animation_state(np, true);
    np->char_inst->set_state(index_alias(state, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
}

// 0x0065CAE0 (:279). LTCG: np ESI.
static bool reserve_action_node(npc* np)
{
    const object_action_node* root = np->get_current_action_node(false);
    if (root == nullptr)
    {
        return false;
    }
    farray<unsigned int, 4> node_set;
    return action_node_build_set(np, root, node_set) && action_node_reserve_node(np, node_set);
}

// 0x0069CAC0 (:294). LTCG: aip EDI.
static ai_results activity_intro_move(npc* np, ai_system* aip)
{
    if (np == nullptr || aip == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const object* target = World.object_from_handle(aip->custom_behavior_target);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const ai_pathfind_results path = aip->pathfind_to(target->pos, HMS_WALK, HST_CURRENT, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false);
    const ai_results result = path == AI_PF_SUCCESS ? AI_RESULT_SUCCESS : (AI_PF_RAGDOLLED < path ? AI_RESULT_FAILURE : AI_RESULT_IN_PROGRESS);
    if (!aip->action_timer.elapsed() && result != AI_RESULT_SUCCESS)
    {
        return result;
    }
    aip->action_state = 1;
    aip->stop_moving();
    aip->action_timer.set(0);
    vector to_target(target->pos.x - np->pos.x, 0.0f, target->pos.z - np->pos.z);
    to_target.normalize_safe();
    aip->desired_evade_dir = to_target.is_null() ? FORWARD_VECTOR : to_target;
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E8160 (:339). LTCG: np EDI, aip ESI. NEAR_ORIENT_DOT is a local static of the cosine of 0x012C95F8, 5 * 0.017453f in float, widened.
static ai_results activity_intro_turn(npc* np, ai_system* aip)
{
    static const float NEAR_ORIENT_DOT = (float)cos((double)(5.0f * 0.017453f));
    if (np == nullptr || aip == nullptr || World.object_from_handle(aip->custom_behavior_target) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const vector& dir = aip->desired_evade_dir;
    np->steering_vector = dir;
    if (NEAR_ORIENT_DOT <= dir.y * np->orient.fvec.y + np->orient.fvec.x * dir.x + dir.z * np->orient.fvec.z)
    {
        aip->action_state = 2;
        np->char_inst->play_action(aip->desired_anim_action, ANIM_ALIAS_NONE, 1.0f, 0);
        if (np->play_voice_line((voice_line_handle)aip->custom_behavior_value, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_DIALOGUE_HIGH, true, true, true, true) != PLAY_VOICE_SUCCESS)
        {
            return AI_RESULT_FAILURE;
        }
        aip->action_timer.set(10000);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E8290 (:386). LTCG: np EDI, aip ESI.
static ai_results activity_intro_wait(npc* np, ai_system* aip)
{
    if (np == nullptr || aip == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (aip->action_timer.elapsed())
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        aip->action_state = 3;
    }
    if ((voice_line_handle)aip->custom_behavior_value == np->get_current_voice_line_handle())
    {
        aip->action_state = 3;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E82F0 (:409). LTCG: np EDI, aip ESI.
static ai_results activity_intro_talk(npc* np, ai_system* aip)
{
    if (np == nullptr || aip == nullptr || World.object_from_handle(aip->custom_behavior_target) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    return (voice_line_handle)aip->custom_behavior_value == np->get_current_voice_line_handle() ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x0065CB60 (:446). LTCG: np EAX, first_frame and delay_time on the stack.
static ai_results old_coot_enter_common(npc* np, bool first_frame, int delay_time)
{
    ai_system* ai = np->get_ai();
    if (!reserve_action_node(np))
    {
        return AI_RESULT_FAILURE;
    }
    if (first_frame)
    {
        ai->perception.set_attention_mode(ATTENTION_IDLE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        human_override_default_animation_state(np, true);
        const int state = int_rand_range(973, 975); // the three rambling stands
        np->char_inst->set_state(index_alias(state, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
        ai->action_timer.set(delay_time);
        ai->action_substate = -1;
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E8320 (:473). LTCG: np EDI; group_name is dropped by LTCG.
static ai_results old_coot_do_action_common(npc* np)
{
    ai_system* ai = np->get_ai();
    if (ai->action_substate == -1)
    {
        if (!ai->action_timer.elapsed())
        {
            return AI_RESULT_IN_PROGRESS;
        }
        const voice_line_handle line = voice_line_unlocking_choose(VLUG_OLD_COOT);
        if (line == INVALID_VOICE_LINE_HANDLE)
        {
            return AI_RESULT_FAILURE;
        }
        ai->action_substate = line;
        np->play_voice_line(line, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_DIALOGUE_NORMAL, false, false, true, true);
        return AI_RESULT_IN_PROGRESS;
    }
    if (np->get_current_voice_line_handle() == (voice_line_handle)ai->action_substate)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    ai->end_custom_behavior();
    return AI_RESULT_SUCCESS;
}

// 0x005E83B0 (:505). LTCG: np on the stack, last_frame in a register.
ai_results old_coot_cleanup_common(npc* np, bool last_frame)
{
    ai_system* ai = np->get_ai();
    if (last_frame)
    {
        return_to_action_node_state(np);
        ai->end_custom_behavior();
        if (ai->action_substate != -1 && (voice_line_handle)ai->action_substate == np->get_current_voice_line_handle())
        {
            np->stop_voice(VOICE_LINE_PRIORITY_HIGH);
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x0069CC40 (:530)
static bool tied_up_callback(const human* hp, void*)
{
    if (hp == nullptr || hp->get_obj_type() != OT_HUMAN || hp->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return false;
    }
    const ai_system* ai = static_cast<const npc*>(hp)->get_ai();
    if (ai->planner.get_current_goal() != tied_up_goal::get_instance())
    {
        return false;
    }
    const player* local = player_get_local_player();
    if (local != nullptr && vec_dist_squared(hp->pos, local->pos) < 225.0f)
    {
        vector head;
        human_get_head_pos(local, head);
        if (ai->perception.has_los_to_position(head, false))
        {
            return false;
        }
    }
    activity_house_arrest_play_voice_line(hp->pos, HUMAN_TEAM_EDF, VOICE_LINE_HA_EDF_HOSTAGE_RESPONSE);
    return true;
}

// 0x0062E2E0 (:568). LTCG: np EAX, count ESI.
static weapon* saboteur_get_bomb(const npc* np, int* count)
{
    const inventory_item* item = human_has_item(np, SABOTEUR_BOMB_NAME);
    if (item == nullptr)
    {
        return nullptr;
    }
    if (count != nullptr)
    {
        *count = item->count;
    }
    if (item->weapon_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(item->weapon_handle);
    return objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON ? static_cast<weapon*>(objp) : nullptr;
}

// 0x005E8400 (:589)
int custom_animation_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const ai_scripting_behaviors behavior = np->get_ai()->custom_behavior;
    return behavior == AI_SB_PLAY_ANIMATION || behavior == AI_SB_PLAY_ANIMATION_TO_PLAYER ? 60100 : -1;
}

// 0x005E8430 (:602)
int custom_behavior_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const ai_scripting_behaviors behavior = np->get_ai()->custom_behavior;
    if (AI_SB_NONE < behavior && behavior < NUM_AI_SCRIPTING_BEHAVIORS)
    {
        return behavior == AI_SB_PLANT_BOMB ? 3350 : 60000;
    }
    return -1;
}

// What rfg.exe hands the collect callback in place of the npc; see sniper_find_new_guard_node.
struct sniper_guard_node_collect_data
{
    vector pos;
    unsigned int excluded_handle;
};

// 0x005E8460 (:622)
bool sniper_find_new_guard_node_collect_cb(object* obj, unsigned int, const void* cb_data)
{
    const sniper_guard_node_collect_data* data = static_cast<const sniper_guard_node_collect_data*>(cb_data);
    if (1.0f < fabsf(obj->pos.y - data->pos.y) || vec_dist_squared(obj->pos, data->pos) < 4.0f)
    {
        return false;
    }
    return !static_cast<object_guard_node*>(obj)->is_occupied(data->excluded_handle);
}

// 0x0065CCC0 (:655). LTCG: new_guard_node EDI; np and spawn_nodep on the stack.
// Original bug kept: the callback casts its data to the npc, and rfg.exe passes the address of a stack local instead (LEA at
// 0x0065CD16), so it reads that frame. The offsets of the npc's position land on the local bounding box maximum of the
// building, so nodes are taken within 1 m of that height and over 2 m from it, a world position compared with a local one.
// The offset of the handle lands on the caller's slot for the result, still 0 (0x0065CE24), so no node counts as the npc's own.
// np is otherwise unused.
static bool sniper_find_new_guard_node(object** new_guard_node, const npc*, const object* spawn_nodep)
{
    if (spawn_nodep == nullptr)
    {
        return false;
    }
    const object* building = World.object_from_handle(spawn_nodep->get_parent());
    if (building == nullptr)
    {
        return false;
    }
    vector bmin;
    sniper_guard_node_collect_data data;
    building->get_local_bbox(bmin, data.pos);
    data.excluded_handle = 0;
    unsigned int nodes[10];
    const int num_nodes = World.collect_objects<object_guard_node>(bmin, data.pos, building->pos, building->orient, nodes, 10, sniper_find_new_guard_node_collect_cb, &data);
    if (num_nodes == 0)
    {
        return false;
    }
    *new_guard_node = World.object_from_handle(nodes[int_rand_range(0, num_nodes - 1)]);
    return true;
}

// 0x0067AB80 (:681)
int terrorize_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const object_squad* squad = np->get_ai()->get_squad();
    return squad != nullptr && squad->get_squad_orders_type() == GFM_ORDERS_TYPE_TERRORIZE ? 59950 : -1;
}

// 0x0065CD90 (:691)
bool terrorize_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_system* ai = np->get_ai();
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr)
    {
        return false;
    }
    gfm_orders* orders = squad->get_squad_orders();
    if (orders == nullptr)
    {
        return false;
    }
    object* target = orders->get_target_object();
    if (target == nullptr)
    {
        return false;
    }
    const unsigned int player_handle = player_get_local_player()->get_handle();
    if (ai->perception.get_belief(AI_BELIEF_SHOT_AT, player_handle) != nullptr && !ai->perception.enemy_in_los(player_handle))
    {
        object* new_node = nullptr;
        if (sniper_find_new_guard_node(&new_node, np, target))
        {
            goal_setup_data.selected_target = new_node;
            return true;
        }
    }
    goal_setup_data.selected_target = target;
    return true;
}

// 0x005E8530 (:721)
void terrorize_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    np->get_ai()->desired_target = goal_setup_data.selected_target->get_handle();
}

// 0x005E8550 (:731)
int custom_watch_player_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return 225.0f < vec_dist_squared(player_get_local_player()->pos, np->pos) ? -1 : 100;
}

// 0x0069CCF0 (:748)
ai_results custom_watch_player_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        ai_system* ai = np->get_ai();
        const player* local = player_get_local_player();
        ai->stop_moving();
        if (local != nullptr)
        {
            ai->perception.pay_attention(local->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x0069CD40 (:768)
ai_results custom_watch_player_action::do_action(npc* np) const
{
    static const float TURN_DOT = (float)cos((double)(45.0f * 0.017453f)); // 0x012CEB58, widened from float
    ai_system* ai = np->get_ai();
    const player* local = player_get_local_player();
    ai->stop_moving();
    if (local != nullptr)
    {
        ai->perception.pay_attention(local->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    }
    // Original quirk kept: the position is read after the null test above, unguarded.
    vector dir(local->pos.x - np->pos.x, 0.0f, local->pos.z - np->pos.z);
    dir.normalize_safe();
    if (!dir.is_null() && np->orient.fvec.y * dir.y + np->orient.fvec.x * dir.x + np->orient.fvec.z * dir.z < TURN_DOT)
    {
        np->steering_vector = dir;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E8590 (:804)
int custom_animation_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 100;
}

// 0x0069CEC0 (:809)
ai_results custom_animation_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const bool to_player = ai->custom_behavior == AI_SB_PLAY_ANIMATION_TO_PLAYER;
    const player* local = player_get_local_player();
    if (last_action != this)
    {
        ai->stop_moving();
        ai->desired_anim_action = (animation_action)ai->custom_behavior_value;
        if (to_player && local != nullptr)
        {
            ai->perception.pay_attention(local->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        }
    }
    if (to_player && local != nullptr)
    {
        np->steering_vector = vector(local->pos.x - np->pos.x, local->pos.y - np->pos.y, local->pos.z - np->pos.z);
        np->steering_vector.flatten_normalize();
        if (np->steering_vector.y * np->orient.fvec.y + np->orient.fvec.x * np->steering_vector.x + np->steering_vector.z * np->orient.fvec.z < 0.8660254f) // cos 30
        {
            return AI_RESULT_IN_PROGRESS;
        }
    }
    np->char_inst->play_action(ai->desired_anim_action, ANIM_ALIAS_SCRIPTED, 1.0f, 0x40);
    ai->planner.lock_action(true);
    return AI_RESULT_SUCCESS;
}

// 0x005E85A0 (:843)
ai_results custom_animation_action::do_action(npc* np) const
{
    if (np->char_inst->is_playing_action(ANIM_ALIAS_SCRIPTED))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    ai_system* ai = np->get_ai();
    if ((ai->custom_behavior == AI_SB_PLAY_ANIMATION || ai->custom_behavior == AI_SB_PLAY_ANIMATION_TO_PLAYER) && ai->custom_behavior_value == ai->desired_anim_action)
    {
        ai->end_custom_behavior();
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E8600 (:860)
ai_results custom_animation_action::cleanup(npc*, const ai_action*) const
{
    return AI_RESULT_SUCCESS;
}

// 0x005E8610 (:868)
int custom_trapped_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const ai_system* ai = np->get_ai();
    return (ai->custom_behavior == AI_SB_TRAPPED_IN_BUILDING || ai->custom_behavior == AI_SB_TRAPPED_IN_JAIL) && ai->custom_behavior_value == 0 ? 100 : -1;
}

// 0x005E8640 (:878)
ai_results custom_trapped_action::enter(npc* np, const ai_action*) const
{
    human_override_default_animation_state(np, true);
    human_set_stance(np, HST_STAND);
    np->char_inst->set_state(index_alias(ANIMATION_STATE_COWER_STAND, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
    return AI_RESULT_SUCCESS;
}

// 0x005E8690 (:889)
ai_results custom_trapped_action::do_action(npc*) const
{
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E86A0 (:896)
ai_results custom_trapped_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        human_override_default_animation_state(np, false);
        const ai_system* ai = np->get_ai();
        if ((ai->custom_behavior == AI_SB_TRAPPED_IN_BUILDING || ai->custom_behavior == AI_SB_TRAPPED_IN_JAIL) && ai->custom_behavior_value == 0)
        {
            human_damage(np, nullptr, np->hit_points, DBT_NONE, nullptr, nullptr, true, false);
            return AI_RESULT_FAILURE;
        }
        np->play_voice_line(VOICE_LINE_PANIC, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, true, true);
    }
    return AI_RESULT_SUCCESS;
}

// 0x0067ABD0 (:922)
int custom_rescued_action::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    if ((ai->custom_behavior != AI_SB_TRAPPED_IN_BUILDING && ai->custom_behavior != AI_SB_TRAPPED_IN_JAIL) || ai->custom_behavior_value != 1)
    {
        return -1;
    }
    if (ai->perception.is_in_closest_building(np, nullptr, 0.0f, 1.25f))
    {
        desired_conditions |= ai_plan_conditions(121);
    }
    return 100;
}

// 0x005E8720 (:936)
void custom_rescued_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x0062E330 (:942)
ai_results custom_rescued_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        ai_system* ai = np->get_ai();
        ai->action_timer.set(0);
        ai->desired_destination = np->pos;
        ai->flags.cannot_retreat = false;
        ai->action_substate = 0;
    }
    return AI_RESULT_SUCCESS;
}

// 0x006B6640 (:947)
ai_results custom_rescued_action::do_action(npc* np) const
{
    return despawn_on_foot_common_do_action(np, HMS_SPRINT);
}

// 0x005E8730 (:955)
int custom_activity_intro_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->get_ai()->custom_behavior == AI_SB_ACTIVITY_INTRO && np->voice != nullptr && np->voice->is_loaded() ? 100 : -1;
}

// 0x0069D070 (:976)
ai_results custom_activity_intro_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    ai_system* ai = np->get_ai();
    const object* target = World.object_from_handle(ai->custom_behavior_target);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    ai->desired_anim_action = (animation_action)ai->custom_action_id;
    ai->action_state = 0;
    ai->action_timer.set(10000);
    return AI_RESULT_SUCCESS;
}

// 0x0069D0E0 (:1009)
ai_results custom_activity_intro_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    switch (ai->action_state)
    {
    case 0:
        return activity_intro_move(np, ai);
    case 1:
        return activity_intro_turn(np, ai);
    case 2:
        return activity_intro_wait(np, ai);
    case 3:
        return activity_intro_talk(np, ai);
    }
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    return AI_RESULT_FAILURE;
}

// 0x0062E380 (:1032)
ai_results custom_activity_intro_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        ai_system* ai = np->get_ai();
        ai->perception.stop_paying_attention(false);
        if (np->char_inst->is_playing_action(ai->desired_anim_action))
        {
            np->char_inst->stop_action(ai->desired_anim_action, 9, true);
        }
        if (ai->custom_behavior == AI_SB_ACTIVITY_INTRO)
        {
            ai->end_custom_behavior();
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E8770 (:1058)
int custom_turn_animate_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->get_ai()->custom_behavior == AI_SB_TURN_ANIMATE ? 100 : -1;
}

// The flattened direction to an object in desired_destination, or forward at no distance.
static void set_direction_to_object(ai_system* ai, const npc* np, const object* objp)
{
    vector& dir = ai->desired_destination;
    dir = vector(objp->pos.x - np->pos.x, objp->pos.y - np->pos.y, objp->pos.z - np->pos.z);
    dir.y = 0.0f;
    dir.normalize_safe();
    if (dir.is_null())
    {
        dir = FORWARD_VECTOR;
    }
}

// 0x0069D160 (:1068)
ai_results custom_turn_animate_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    ai_system* ai = np->get_ai();
    const object* target = World.object_from_handle(ai->custom_behavior_target);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    set_direction_to_object(ai, np, target);
    ai->stop_moving();
    ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    ai->desired_anim_action = (animation_action)ai->custom_behavior_value;
    ai->action_state = 0;
    ai->action_timer.invalidate();
    np->steering_vector = ai->desired_destination;
    return AI_RESULT_SUCCESS;
}

// 0x005E8790 (:1108)
ai_results custom_turn_animate_action::do_action(npc* np) const
{
    static const float NEAR_ORIENT_DOT = (float)cos((double)(5.0f * 0.017453f)); // 0x012C95F8, widened from float
    ai_system* ai = np->get_ai();
    switch (ai->action_state)
    {
    case 0:
    {
        const vector& dir = ai->desired_destination;
        np->steering_vector = dir;
        if (dir.y * np->orient.fvec.y + np->orient.fvec.x * dir.x + dir.z * np->orient.fvec.z < NEAR_ORIENT_DOT)
        {
            return AI_RESULT_IN_PROGRESS;
        }
        ai->action_state = 1;
        np->char_inst->play_action(ai->desired_anim_action, ANIM_ALIAS_NONE, 1.0f, 0);
        ai->action_timer.set(500);
        return AI_RESULT_IN_PROGRESS;
    }
    case 1:
        if (ai->action_timer.elapsed() && !np->char_inst->is_playing_action(ai->desired_anim_action))
        {
            ai->action_state = 2;
            ai->action_timer.set(3000);
        }
        return AI_RESULT_IN_PROGRESS;
    case 2:
        return ai->action_timer.elapsed() ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
    }
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    return AI_RESULT_FAILURE;
}

// 0x0062E400 (:1176)
ai_results custom_turn_animate_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        ai_system* ai = np->get_ai();
        ai->perception.stop_paying_attention(false);
        if (np->char_inst->is_playing_action(ai->desired_anim_action))
        {
            np->char_inst->stop_action(ai->desired_anim_action, 3, true);
        }
        if (ai->custom_behavior == AI_SB_TURN_ANIMATE)
        {
            ai->end_custom_behavior();
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E8930 (:1203)
int custom_follow_wounded::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->get_ai()->custom_behavior == AI_SB_FOLLOW_WOUNDED ? 100 : -1;
}

// 0x005E8950 (:1213)
ai_results custom_follow_wounded::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        ai_system* ai = np->get_ai();
        ai->desired_target_pos.zero();
        ai->action_timer.set(0);
    }
    return AI_RESULT_SUCCESS;
}

// 0x0069D2C0 (:1230). Original quirk kept: rfg.exe's null test is of a member's address (LEA ECX,[EAX + 0xDDC] at 0x0069D2D4), which is never null, so a null local player crashes on the next line. A follower offscreen beyond 25 m jumps behind the camera.
ai_results custom_follow_wounded::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const player* local = player_get_local_player();
    const float dist_sq = vec_dist_squared(np->pos, local->pos);
    if (dist_sq < 16.0f)
    {
        ai->stop_moving();
        return AI_RESULT_IN_PROGRESS;
    }
    const bool offscreen = np->is_offscreen();
    if (625.0f < dist_sq && offscreen)
    {
        const vector camera_dir = Rfg_camera.real_orient.fvec.copy_flatten_normalize_safe();
        const vector behind(Rfg_camera.real_pos.x - camera_dir.x * 5.0f, Rfg_camera.real_pos.y - camera_dir.y * 5.0f, Rfg_camera.real_pos.z - camera_dir.z * 5.0f);
        vector teleport_pos = behind;
        if (move_with_human_teleport_destination(np->get_handle(), behind, 25.0f, teleport_pos))
        {
            teleport_pos.y += 0.5f;
            if (game_render_cull_sphere(teleport_pos, 1.25f))
            {
                matrix orient;
                orient.make_uvec(camera_dir, UP_VECTOR);
                human_teleport(np, teleport_pos, orient, 6.0f, true);
                return AI_RESULT_IN_PROGRESS;
            }
        }
    }
    np->move_speed = offscreen ? 0.0f : 1.8f;
    const ai_pathfind_results result = ai->pathfind_to(local->pos, HMS_WALK, HST_STAND, AI_PATHFIND_PRIORITY_LOW, 0xFFFFFFFFFFFFFFFFull, false);
    return result != AI_PF_SUCCESS && AI_PF_RAGDOLLED < result ? AI_RESULT_FAILURE : AI_RESULT_IN_PROGRESS;
}

// 0x005E8990 (:1304)
int custom_look_at_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->get_ai()->custom_behavior == AI_SB_LOOK_AT_OBJECT ? 100 : -1;
}

// 0x0069D500 (:1314)
ai_results custom_look_at_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const object* target = World.object_from_handle(ai->custom_behavior_target);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    set_direction_to_object(ai, np, target);
    ai->stop_moving();
    np->steering_vector = ai->desired_destination;
    if (last_action != this)
    {
        human_set_stance(np, HST_STAND);
        ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ai->custom_behavior_value != 0 ? ATTENTION_AIM_AT_OBJECT : ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E89B0 (:1356)
ai_results custom_look_at_action::do_action(npc* np) const
{
    return World.object_from_handle(np->get_ai()->custom_behavior_target) != nullptr ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x0062E480 (:1372)
ai_results custom_look_at_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->get_ai()->perception.stop_paying_attention(false);
    }
    return AI_RESULT_SUCCESS;
}

static object* get_npc_spawn_node(const npc* np)
{
    if (np->spawn_node_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(np->spawn_node_handle);
    return objp != nullptr && objp->get_obj_type() == OT_NPC_SPAWN_NODE ? objp : nullptr;
}

// 0x0062E4C0 (:1391). LTCG: np ESI.
static void end_marauder_lookout(npc* np)
{
    np->get_ai()->end_custom_behavior();
    np->start_active_despawn(NPC_DESPAWN_TYPE_HIGH, get_npc_spawn_node(np));
}

// 0x005E89E0 (:1403)
int marauder_lookout_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->get_ai()->custom_behavior == AI_SB_MARAUDER_LOOKOUT ? 100 : -1;
}

// 0x0069D670 (:1413)
ai_results marauder_lookout_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        ai->planner.lock_action(true);
        ai->action_substate = 0;
    }
    const player* local = player_get_local_player();
    if (local != nullptr)
    {
        ai->perception.pay_attention(local->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        return AI_RESULT_SUCCESS;
    }
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    end_marauder_lookout(np);
    return AI_RESULT_FAILURE;
}

// 0x0069D730 (:1434)
ai_results marauder_lookout_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    object* destination = ai->custom_behavior_target != 0xFFFFFFFF ? World.object_from_handle(ai->custom_behavior_target) : nullptr;
    if (destination != nullptr && destination->get_obj_type() != OT_NAVPOINT)
    {
        destination = nullptr;
    }
    const player* local = player_get_local_player();
    const object* spawn_node = get_npc_spawn_node(np);
    if (destination == nullptr || local == nullptr || spawn_node == nullptr)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        end_marauder_lookout(np);
        return AI_RESULT_FAILURE;
    }
    const double spot_dist = (double)ai->custom_behavior_value;
    if ((double)vec_dist_squared(np->pos, local->pos) < spot_dist * spot_dist)
    {
        end_marauder_lookout(np);
        return AI_RESULT_SUCCESS;
    }
    const vector to_spawn(spawn_node->pos.x - destination->pos.x, spawn_node->pos.y - destination->pos.y, spawn_node->pos.z - destination->pos.z);
    const vector spawn_dir = to_spawn.copy_flatten_normalize_safe();
    const vector to_player(local->pos.x - np->pos.x, local->pos.y - np->pos.y, local->pos.z - np->pos.z);
    const vector player_dir = to_player.copy_flatten_normalize_safe();
    if (spawn_dir.y * player_dir.y + player_dir.x * spawn_dir.x + spawn_dir.z * player_dir.z <= -0.5f)
    {
        static const ai_belief_specification BELIEF_REQS[6] = {
            ai_belief_specification(AI_BELIEF_BULLET_IMPACT, 0xFFFFFFFF, -1, BELIEF_REQUIREMENTS_NONE),
            ai_belief_specification(AI_BELIEF_COMBAT_SOUND, 0xFFFFFFFF, -1, BELIEF_REQUIREMENTS_NONE),
            ai_belief_specification(AI_BELIEF_SHOT_AT, 0xFFFFFFFF, -1, BELIEF_REQUIREMENTS_NONE),
            ai_belief_specification(AI_BELIEF_DAMAGE_TAKEN, 0xFFFFFFFF, -1, BELIEF_REQUIREMENTS_NONE),
            ai_belief_specification(AI_BELIEF_PROJECTILE, 0xFFFFFFFF, -1, BELIEF_REQUIREMENTS_NONE),
            ai_belief_specification(AI_BELIEF_EXPLOSION, 0xFFFFFFFF, -1, BELIEF_REQUIREMENTS_NONE),
        };
        ai_belief* found;
        if (ai->perception.collect_all_matching_beliefs(&found, 1, BELIEF_REQS, 6, 0xFFFFFFFF) == 0)
        {
            if (1.0f <= vec_dist_squared(np->pos, destination->pos))
            {
                if (!ai->is_pathing_to(destination->pos, 0.25f, 1.0f) && ai_pf_to_result(ai->pathfind_to(destination->pos, HMS_RUN, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false)) == AI_RESULT_FAILURE)
                {
                    end_marauder_lookout(np);
                    return AI_RESULT_FAILURE;
                }
            }
            else if (ai->action_substate == 0)
            {
                ai->stop_moving();
                np->char_inst->play_action(ANIMATION_ACTION_BATTLE_CRY, ANIM_ALIAS_NONE, 1.0f, 0);
                ai->action_substate = 1;
            }
            return AI_RESULT_IN_PROGRESS;
        }
    }
    end_marauder_lookout(np);
    return AI_RESULT_SUCCESS;
}

// 0x0062E510 (:1504)
int saboteur_goto_action::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node* next_action) const
{
    const unsigned int handle = np->get_ai()->custom_behavior_target;
    const object* target = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
    if (target != nullptr && target->get_obj_type() == OT_NAVPOINT && vec_dist_squared(np->pos, target->pos) <= NEAR_TARGET_DIST_SQ)
    {
        desired_conditions &= ~ai_plan_conditions(4);
        return 0;
    }
    return ai_plan_contains_action_postcondition(next_action, ai_plan_conditions(7)) ? 100 : -1;
}

// 0x005E8A00 (:1522)
void saboteur_goto_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x0069DB80 (:1528)
ai_results saboteur_goto_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (saboteur_get_bomb(np, nullptr) != nullptr)
    {
        ai_combat_fire_at_target_if_available(np, false, false);
        const object* target = ai->custom_behavior_target != 0xFFFFFFFF ? World.object_from_handle(ai->custom_behavior_target) : nullptr;
        if (target != nullptr && target->get_obj_type() == OT_NAVPOINT)
        {
            if (vec_dist_squared(np->pos, target->pos) <= NEAR_TARGET_DIST_SQ)
            {
                return AI_RESULT_SUCCESS;
            }
            if (ai->is_pathing_to(target->pos, 0.25f, 1.0f))
            {
                return AI_RESULT_IN_PROGRESS;
            }
            const ai_pathfind_results result = ai->pathfind_to(target->pos, HMS_RUN, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false);
            if (result == AI_PF_SUCCESS || result <= AI_PF_RAGDOLLED)
            {
                return AI_RESULT_IN_PROGRESS;
            }
        }
    }
    ai->end_custom_behavior();
    return AI_RESULT_FAILURE;
}

// 0x0062E610 (:1575). LTCG: np ESI. Once the planted bomb has left the npc's hands the behavior ends, and the plan unlocks when the throw allows movement.
static void maybe_end_saboteur_behavior(npc* np)
{
    ai_system* ai = np->get_ai();
    if (np->npc_flags.remove_icon_on_death || ai->custom_behavior == AI_SB_PLANT_BOMB)
    {
        const unsigned int handle = ai->desired_weapon;
        object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
        if (objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON && !np->combat.thrown_weapon_released)
        {
            const inventory_item* item = inv_item_get_item_from_weapon(np, static_cast<weapon*>(objp));
            if (item != nullptr && ai->action_substate <= item->count)
            {
                return;
            }
        }
        ai->end_custom_behavior();
        Cartographer.remove_icon(np->get_handle());
        np->npc_flags.remove_icon_on_death = 0;
    }
    if (np->char_inst->is_playing_action(ANIM_ALIAS_THROW) && !vanim_has_trigger_elapsed(np->char_inst->vp, Trigger_release_movement, ANIM_ALIAS_THROW, -1))
    {
        return;
    }
    ai->planner.lock_action(false);
}

// 0x0062E6E0 (:1616)
int saboteur_bomb_action::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    if (ai->custom_behavior != AI_SB_PLANT_BOMB)
    {
        return -1;
    }
    const object* target = ai->custom_behavior_target != 0xFFFFFFFF ? World.object_from_handle(ai->custom_behavior_target) : nullptr;
    if (target != nullptr && target->get_obj_type() == OT_NAVPOINT && vec_dist_squared(np->pos, target->pos) <= NEAR_TARGET_DIST_SQ)
    {
        return 100;
    }
    desired_conditions |= ai_plan_conditions(7);
    return 100;
}

// 0x005E8A10 (:1632)
void saboteur_bomb_action::commit_setup_data(npc*, const ai_action_setup_data&) const
{
}

// 0x0065CE70 (:1638)
ai_results saboteur_bomb_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    ai_system* ai = np->get_ai();
    weapon* bomb = saboteur_get_bomb(np, &ai->action_substate);
    if (bomb == nullptr)
    {
        ai->end_custom_behavior();
        return AI_RESULT_FAILURE;
    }
    ai->desired_weapon = bomb->get_handle();
    human_set_combat_ready(np, false, 0);
    ai->stop_moving();
    human_set_stance(np, HST_STAND);
    ai->perception.set_attention_mode(ATTENTION_IDLE, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    human_weapons_start_throw(np, bomb, &ZERO_VECTOR, false);
    ai->planner.lock_action(true);
    return AI_RESULT_SUCCESS;
}

// 0x0062E7A0 (:1669)
ai_results saboteur_bomb_action::do_action(npc* np) const
{
    maybe_end_saboteur_behavior(np);
    return np->char_inst->is_playing_action(ANIM_ALIAS_THROW) ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x0062E7D0 (:1681)
ai_results saboteur_bomb_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        maybe_end_saboteur_behavior(np);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E8A20 (:1693)
int terrorize_goto_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    const object* target = goal_setup_data.selected_target;
    if (target == nullptr)
    {
        return -1;
    }
    return vec_dist_squared(np->pos, target->pos) <= NEAR_TARGET_DIST_SQ ? 0 : 100;
}

// 0x005E8A60 (:1706)
ai_results terrorize_goto_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        np->get_ai()->action_timer.set(20000);
    }
    return AI_RESULT_SUCCESS;
}

// 0x0069DC90 (:1720)
ai_results terrorize_goto_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    ai_combat_fire_at_target_if_available(np, false, false);
    const object* target = World.object_from_handle(ai->desired_target);
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    object_squad* squad = ai->get_squad();
    const float dist_sq = vec_dist_squared(np->pos, target->pos);
    if (squad == nullptr || squad->get_leader() != np)
    {
        if (dist_sq <= 9.0f)
        {
            return AI_RESULT_SUCCESS;
        }
    }
    else if (dist_sq <= NEAR_TARGET_DIST_SQ)
    {
        return AI_RESULT_SUCCESS;
    }
    const human* combat_target = human_combat_get_target(np);
    if (combat_target != nullptr && fabsf(np->pos.y - combat_target->pos.y) < Sniper_prefer_enemy_y_dist && vec_xz_dist_squared(np->pos, combat_target->pos) < Sniper_prefer_enemy_xz_dist * Sniper_prefer_enemy_xz_dist)
    {
        ai->planner.avoid_action(terrorize_action::get_instance(), 30000);
        return AI_RESULT_FAILURE;
    }
    if (ai->action_timer.elapsed())
    {
        const player* local = player_get_local_player();
        if (squad != nullptr && local != nullptr)
        {
            gfm_orders_creation_params params;
            params.ofp.reset();
            params.reset();
            params.team = squad->get_squad_team();
            params.object_target = local->get_handle();
            params.location_target = local->pos;
            params.orders_type = GFM_ORDERS_TYPE_ATTACK;
            squad->assign_orders(Orders_manager.create_new_orders(params));
        }
        return AI_RESULT_FAILURE;
    }
    if (!ai->is_pathing_to(target->pos, 0.25f, 1.0f) && ai_pf_to_result(ai->pathfind_to(target->pos, HMS_RUN, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false)) == AI_RESULT_FAILURE)
    {
        const unsigned int parent = np->get_parent();
        if (ai->desired_target == parent)
        {
            ai->planner.avoid_action(this, 5000);
            return AI_RESULT_FAILURE;
        }
        ai->desired_target = parent;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x0067AC90 (:1811)
static human* sniper_get_best_target(npc* np)
{
    const ai_system* ai = np->get_ai();
    object* attention = World.object_from_handle(ai->perception.get_attention_handle());
    const human* current = attention != nullptr && attention->get_obj_type() == OT_HUMAN && !attention->is_killed() ? static_cast<human*>(attention) : nullptr;
    human* best = nullptr;
    float best_dist = FLT_MAX;
    human* combat_target = human_combat_get_target(np);
    if (combat_target != nullptr && ai->perception.enemy_in_los(combat_target->get_handle()) && ai->perception.can_engage_target_reason(combat_target) == AI_ENGAGE_ALLOWED)
    {
        if (ai->perception.get_belief(AI_BELIEF_SHOULD_ENGAGE_ENEMY, combat_target) != nullptr)
        {
            return combat_target;
        }
        best_dist = vec_dist(combat_target->pos, np->pos);
        best = combat_target;
        if (combat_target == current)
        {
            best_dist -= 5.0f;
        }
    }
    belief_obj_iterator<human> it;
    it.init(ai, AI_BELIEF_UNFRIENDLY);
    while (it)
    {
        if ((it.get_belief()->flags & BF_UNCONFIRMED) == 0)
        {
            human* hp = it;
            float dist = vec_dist(np->pos, hp->pos);
            if (hp == current)
            {
                dist -= 5.0f;
            }
            if (dist < best_dist)
            {
                best_dist = dist;
                best = hp;
            }
        }
        ++it;
    }
    return best;
}

// 0x0062E7F0 (:1866)
int terrorize_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    const object* target = goal_setup_data.selected_target;
    if (target == nullptr)
    {
        return -1;
    }
    const ai_perception& perception = np->get_ai()->perception;
    for (const ai_belief* belief = perception.get_first_belief(AI_BELIEF_ENEMY_POSITION); belief != nullptr; belief = perception.get_next_belief(belief))
    {
        if (fabsf(target->pos.y - belief->pos.y) < Sniper_prefer_enemy_y_dist && vec_xz_dist_squared(target->pos, belief->pos) < Sniper_prefer_enemy_xz_dist * Sniper_prefer_enemy_xz_dist)
        {
            return -1;
        }
    }
    return 100;
}

// 0x005E8A90 (:1889)
ai_results terrorize_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        ai_system* ai = np->get_ai();
        human_set_combat_ready(np, true, 0);
        ai->stop_moving();
        human_set_stance(np, HST_STAND);
        ai->action_timer.set(0);
        ai->action_substate = 0;
        ai->flags.use_alternate_target = vrand() % 100 <= (unsigned int)Sniper_attack_player_chance;
    }
    return AI_RESULT_SUCCESS;
}

// 0x0069DF30 (:1914). action_substate adds up the time on a target; past 7 s the choice of whom to shoot is made again.
ai_results terrorize_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const object* node = World.object_from_handle(ai->desired_target);
    if (node == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    if (ai->action_timer.elapsed())
    {
        if (7000 < ai->action_substate)
        {
            ai->action_substate = 0;
            ai->flags.use_alternate_target = vrand() % 100 <= (unsigned int)Sniper_attack_player_chance;
        }
        human* target = nullptr;
        if (ai->flags.use_alternate_target)
        {
            target = human_combat_get_target(np);
            if (target == nullptr || !ai->perception.enemy_in_los(target->get_handle()))
            {
                ai->flags.use_alternate_target = false;
                ai->action_substate = 0;
                target = sniper_get_best_target(np);
            }
        }
        else
        {
            target = sniper_get_best_target(np);
        }
        const int delay = int_rand_range(1500, 3000);
        ai->action_timer.set(delay);
        ai->action_substate += delay;
        if (target == nullptr)
        {
            ai->perception.set_attention_mode(ATTENTION_IDLE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
            np->steering_vector = node->orient.fvec.copy_flatten_normalize_safe();
        }
        else
        {
            ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
            np->steering_vector = ZERO_VECTOR;
        }
    }
    ai_combat_action_parameters params;
    params.init_npc(np);
    params.init_target(true);
    if (params.target != nullptr && ai->perception.get_attention_mode() == ATTENTION_AIM_AT_OBJECT)
    {
        if (ai->flags.use_alternate_target)
        {
            np->aim_override_dir = ZERO_VECTOR;
        }
        else
        {
            const weapon* wp = np->get_equipped_weapon();
            if (wp == nullptr)
            {
                return AI_RESULT_FAILURE;
            }
            object* attention = World.object_from_handle(ai->perception.get_attention_handle());
            if (attention == nullptr || attention->get_obj_type() != OT_HUMAN || object_is_player(attention))
            {
                np->aim_override_dir = ZERO_VECTOR;
            }
            else
            {
                vector head;
                human_get_head_pos(static_cast<human*>(attention), head);
                vector muzzle;
                wp->get_muzzle_transform(muzzle, nullptr);
                np->aim_override_dir = vector(head.x - muzzle.x, head.y - muzzle.y, head.z - muzzle.z);
                np->aim_override_dir.normalize_safe();
            }
        }
        ai_combat_fire_at_target(params, true);
    }
    const unsigned int player_handle = player_get_local_player()->get_handle();
    if (ai->perception.get_belief(AI_BELIEF_SHOT_AT, player_handle) != nullptr && !ai->perception.enemy_in_los(player_handle))
    {
        ai->planner.request_replan();
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E8B10 (:2021)
ai_results terrorize_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->aim_override_dir = ZERO_VECTOR;
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E8B40 (:2065)
int coot_ramble_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if (np->get_current_action_node_handle() == 0xFFFFFFFF)
    {
        return -1;
    }
    return np->get_ai()->custom_behavior == AI_SB_COOT_RAMBLE ? 100 : -1;
}

// 0x0065D010 (:2078). The 750 ms is pushed at 0x0065D014.
ai_results coot_ramble_action::enter(npc* np, const ai_action* last_action) const
{
    return old_coot_enter_common(np, last_action != this, 750);
}

// 0x005E8B70 (:2083)
ai_results coot_ramble_action::do_action(npc* np) const
{
    return old_coot_do_action_common(np);
}

// 0x005E8B80 (:2088)
ai_results coot_ramble_action::cleanup(npc* np, const ai_action* next_action) const
{
    return old_coot_cleanup_common(np, next_action != this);
}

// 0x005E8BD0 (:2096)
int vehicle_hostage_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return human_in_vehicle(np) != nullptr ? 90 : -1;
}

// 0x005E8BF0 (:2106)
ai_results vehicle_hostage_action::enter(npc* np, const ai_action*) const
{
    return np->cur_state == HS_VEHICLE_ENTERING ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x005E8C10 (:2126)
ai_results vehicle_hostage_action::do_action(npc*) const
{
    return AI_RESULT_IN_PROGRESS;
}

// 0x005E8C20 (:2132)
ai_results vehicle_hostage_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        if (np->npc_flags.freed_from_bonds)
        {
            np->npc_flags.tied_up = 0;
            np->npc_flags.freed_from_bonds = 0;
        }
        human_override_default_animation_state(np, false);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E8C50 (:2152)
int tied_up_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return human_in_vehicle(np) != nullptr ? -1 : 100;
}

// 0x0069E320 (:2162)
ai_results tied_up_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        ai->action_substate = 0;
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
        human_override_default_animation_state(np, true);
        np->char_inst->set_state(index_alias(ANIMATION_STATE_TIED_UP_KNEEL, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
    }
    if (ai->action_substate == 0 && np->current_team != HUMAN_TEAM_MARAUDER && 16.0f < vec_dist_squared(player_get_local_player()->pos, np->pos))
    {
        if (activity_is_active(ACTIVITY_MODE_HOUSE_ARREST))
        {
            if (np->get_current_voice_line() == VOICE_LINE_HA_HOSTAGE_HELP)
            {
                np->stop_voice(VOICE_LINE_PRIORITY_HIGH);
            }
            np->play_voice_line(VOICE_LINE_HA_HOSTAGE_HELP_PC, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        }
        else
        {
            np->play_voice_line(VOICE_LINE_HA_HOSTAGE_HELP, tied_up_callback, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x0062E8B0 (:2215)
ai_results tied_up_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (!np->npc_flags.tied_up)
    {
        return AI_RESULT_SUCCESS;
    }
    if (!np->npc_flags.freed_from_bonds)
    {
        if (vec_dist_squared(player_get_local_player()->pos, np->pos) < 900.0f)
        {
            np->hflags.invulnerable = 0; // AND [ESI + 0xB4], ~0x20 at 0x0062E8F0
        }
    }
    else if (ai->action_substate < 1)
    {
        ai->action_substate = 1;
        ai->planner.lock_action(true);
        human_override_default_animation_state(np, false);
        np->char_inst->play_action(ANIMATION_ACTION_KNEEL_TO_STAND_HANDS_TIED, ANIM_ALIAS_NONE, 1.0f, 0x4040);
        const voice_lines line = np->get_current_voice_line();
        if (line == VOICE_LINE_HA_HOSTAGE_HELP || line == VOICE_LINE_HA_HOSTAGE_HELP_PC)
        {
            np->stop_voice(VOICE_LINE_PRIORITY_HIGH);
        }
    }
    else if (!np->char_inst->is_playing_action(ANIMATION_ACTION_KNEEL_TO_STAND_HANDS_TIED))
    {
        np->npc_flags.tied_up = 0;
        np->npc_flags.freed_from_bonds = 0;
        return AI_RESULT_SUCCESS;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x0062E9A0 (:2268)
ai_results tied_up_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    if (np->char_inst->rdd.is_loaded())
    {
        human_override_default_animation_state(np, false);
        np->char_inst->stop_action(ANIMATION_ACTION_KNEEL_TO_STAND_HANDS_TIED, 3, true);
    }
    if (np->get_ai()->action_substate < 1)
    {
        human_damage(np, nullptr, np->hit_points, DBT_NONE, nullptr, nullptr, true, false);
    }
    np->npc_flags.tied_up = 0;
    np->npc_flags.freed_from_bonds = 0;
    const voice_lines line = np->get_current_voice_line();
    if (line == VOICE_LINE_HA_HOSTAGE_HELP || line == VOICE_LINE_HA_HOSTAGE_HELP_PC)
    {
        np->stop_voice(VOICE_LINE_PRIORITY_HIGH);
    }
    return AI_RESULT_SUCCESS;
}

static vehicle* get_human_vehicle(const human* hp)
{
    const unsigned int handle = hp->vehicle_handle;
    object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
    return objp != nullptr && objp->get_obj_type() == OT_VEHICLE && objp->get_sub_type() == OT_SUB_VEHICLE_AUTO ? static_cast<vehicle*>(objp) : nullptr;
}

// 0x0069E440 (:2314)
int drop_off_vehicle_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const object_squad* squad = np->get_ai()->get_squad();
    if (squad == nullptr || squad->get_squad_orders_type() != GFM_ORDERS_VEHICLE_DROP_OFF)
    {
        return -1;
    }
    const vehicle* vp = get_human_vehicle(np);
    return vp != nullptr && ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER) && ai_vehicle_is_seat_available(vp, np, VSI_DRIVER) ? 100 : -1;
}

// 0x0062EA60 (:2341)
ai_results drop_off_vehicle_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    ai_system* ai = np->get_ai();
    vehicle* vp = get_human_vehicle(np);
    if (vp == nullptr || !find_drop_off_position_player(ai->desired_destination))
    {
        return AI_RESULT_FAILURE;
    }
    vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
    ai->action_timer.invalidate();
    ai->action_state = 1;
    return AI_RESULT_SUCCESS;
}

// 0x0069E4E0 (:2369). Honks once on the way within 20 m of the player. NEAR_PLAYER_DIST_SQRD, a local static of 100, is never read.
ai_results drop_off_vehicle_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = get_human_vehicle(np);
    object_squad* squad = ai->get_squad();
    if (vp == nullptr || squad == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    player* local = player_get_local_player();
    if (ai->action_state != 0)
    {
        if (ai->action_timer.valid())
        {
            if (ai->action_timer.elapsed())
            {
                automobile_release_horn(np->vehicle_handle);
                ai->action_state = 0;
            }
        }
        else if (vec_dist_squared(np->pos, local->pos) < 400.0f)
        {
            automobile_honk_horn(np->vehicle_handle);
            ai->action_timer.set_rand(1000, 1000);
        }
    }
    vector drop_off_pos;
    if (find_drop_off_position_player(drop_off_pos))
    {
        ai->desired_destination = drop_off_pos;
    }
    // Pushes at 0x0069E625: no hint, no roads, 10 m (0x012C8500), smoothing on.
    const ai_results result = ai_vehicle_travel_to_a_point_do_frame(np, vp, ai->desired_destination, 0xFFFFFFFFFFFFFFFFull, false, 10.0f, false, false, false, true);
    if (result == AI_RESULT_FAILURE)
    {
        return AI_RESULT_FAILURE;
    }
    if (result != AI_RESULT_SUCCESS)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    np->play_voice_line(VOICE_LINE_AMBIENT_PLAYER_GREET_HIGH_MORALE, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_MEDIUM, true, false);
    local->last_vehicle_driven = np->vehicle_handle;
    np->hflags.disallow_vehicle_entry = 1;
    vehicle_set_controls_when_abandoned(vp);
    vi_exit_data exit_data;
    vehicle_interaction_request_exit(np, &exit_data);
    squad->request_despawn_all_members(NPC_DESPAWN_TYPE_LOW, nullptr, true);
    return AI_RESULT_SUCCESS;
}

// 0x005E8C70 (:2457)
int tech_upgrade_response::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->get_ai()->custom_behavior == AI_SB_TECH_RESPONSE ? 100 : -1;
}

// 0x0069E6D0 (:2467)
ai_results tech_upgrade_response::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        object_action_node* node = np->get_current_action_node(false);
        if (node != nullptr)
        {
            node->emergency_exit(np);
        }
        np->release_all_action_nodes();
        ai_system* ai = np->get_ai();
        ai->action_timer.set(1000);
        const int line = ai->custom_behavior_value;
        ai->action_substate = 0;
        ai->desired_voice_line = VOICE_LINE_NONE;
        if (-1 < line && line < 129) // the number of voice lines
        {
            ai->desired_voice_line = (voice_lines)line;
            ai->action_substate = 1;
        }
        ai->stop_moving();
        const player* local = player_get_local_player();
        if (local != nullptr)
        {
            ai->perception.pay_attention(local->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x005E8C90 (:2510)
ai_results tech_upgrade_response::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (ai->action_substate != 0)
    {
        np->play_voice_line(ai->desired_voice_line, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_HIGH, false, false);
        ai->action_substate = 0;
    }
    if (!ai->action_timer.elapsed())
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (ai->desired_voice_line != VOICE_LINE_NONE && ai->desired_voice_line == np->get_current_voice_line())
    {
        return AI_RESULT_IN_PROGRESS;
    }
    return np->char_inst->is_playing_action(ANIM_ALIAS_SCRIPTED) ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x0062EAF0 (:2541)
ai_results tech_upgrade_response::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        ai_system* ai = np->get_ai();
        if (ai->desired_voice_line != VOICE_LINE_NONE && ai->desired_voice_line == np->get_current_voice_line())
        {
            np->stop_voice(VOICE_LINE_PRIORITY_HIGH);
        }
        ai->custom_behavior = AI_SB_NONE;
        if (np->char_inst->is_playing_action(ANIM_ALIAS_SCRIPTED) && np->char_inst->rdd.is_loaded())
        {
            np->char_inst->stop_action(ANIM_ALIAS_SCRIPTED, 3, false);
        }
    }
    return AI_RESULT_SUCCESS;
}

custom_animation_goal custom_animation_goal::the_instance; // 0x016426F4

// 0x005FF8E0 (.h:16)
custom_animation_goal::custom_animation_goal()
    : ai_goal(ai_plan_conditions(155), "custom animation")
{
}

ai_goal* custom_animation_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD910
custom_animation_goal::~custom_animation_goal()
{
}

custom_behavior_goal custom_behavior_goal::the_instance; // 0x01642710

// 0x005FF980 (.h:17)
custom_behavior_goal::custom_behavior_goal()
    : ai_goal(ai_plan_conditions(156), "custom behavior")
{
}

ai_goal* custom_behavior_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD920
custom_behavior_goal::~custom_behavior_goal()
{
}

terrorize_goal terrorize_goal::the_instance; // 0x0164272C

// 0x005FFA20 (.h:18)
terrorize_goal::terrorize_goal()
    : ai_goal(ai_plan_conditions(158), "custom sniper")
{
}

ai_goal* terrorize_goal::get_instance()
{
    return &the_instance;
}

// 0x005DD930
terrorize_goal::~terrorize_goal()
{
}

custom_animation_action custom_animation_action::the_instance; // 0x01D67DD0

// 0x005FFAC0 (.h:21)
custom_animation_action::custom_animation_action()
    : ai_action("custom animation", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({155}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* custom_animation_action::get_instance()
{
    return &the_instance;
}

// 0x005DD940
custom_animation_action::~custom_animation_action()
{
}

// 0x005FFD60 (.h:21)
int custom_animation_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

custom_trapped_action custom_trapped_action::the_instance; // 0x01D723B0

// 0x005FFDA0 (.h:25)
custom_trapped_action::custom_trapped_action()
    : ai_action("custom: trapped", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({156}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* custom_trapped_action::get_instance()
{
    return &the_instance;
}

// 0x005DD950
custom_trapped_action::~custom_trapped_action()
{
}

// 0x00600040 (.h:25)
int custom_trapped_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

custom_rescued_action custom_rescued_action::the_instance; // 0x01DC9AB0

// 0x00600080 (.h:26)
custom_rescued_action::custom_rescued_action()
    : ai_action("custom: rescued", ai_plan_conditions_from_bits({9, 14, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({156}))
{
    flags.has_enter = 1;
}

// 0x005DD970
custom_rescued_action::~custom_rescued_action()
{
}

// 0x00600380 (.h:26)
int custom_rescued_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

marauder_lookout_action marauder_lookout_action::the_instance; // 0x01DDB764

// 0x00600F30 (.h:38)
marauder_lookout_action::marauder_lookout_action()
    : ai_action("custom: lookout", ai_plan_conditions_from_bits({4, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({156}))
{
    flags.has_enter = 1;
}

ai_action* marauder_lookout_action::get_instance()
{
    return &the_instance;
}

// 0x005DD9C0
marauder_lookout_action::~marauder_lookout_action()
{
}

// 0x00601230 (.h:38)
int marauder_lookout_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

custom_look_at_action custom_look_at_action::the_instance; // 0x01DDAF40

// 0x006003B0 (.h:29)
custom_look_at_action::custom_look_at_action()
    : ai_action("custom: look at", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({156}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* custom_look_at_action::get_instance()
{
    return &the_instance;
}

// 0x005DD980
custom_look_at_action::~custom_look_at_action()
{
}

// 0x00600650 (.h:29)
int custom_look_at_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

custom_turn_animate_action custom_turn_animate_action::the_instance; // 0x01D72674

// 0x00600690 (.h:30)
custom_turn_animate_action::custom_turn_animate_action()
    : ai_action("custom: turn animate", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({156}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* custom_turn_animate_action::get_instance()
{
    return &the_instance;
}

// 0x005DD990
custom_turn_animate_action::~custom_turn_animate_action()
{
}

// 0x00600930 (.h:30)
int custom_turn_animate_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

custom_activity_intro_action custom_activity_intro_action::the_instance; // 0x01DA0704

// 0x00600970 (.h:31)
custom_activity_intro_action::custom_activity_intro_action()
    : ai_action("custom: activity intro", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({156}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* custom_activity_intro_action::get_instance()
{
    return &the_instance;
}

// 0x005DD9A0
custom_activity_intro_action::~custom_activity_intro_action()
{
}

// 0x00600C10 (.h:31)
int custom_activity_intro_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

custom_follow_wounded custom_follow_wounded::the_instance; // 0x01D67E60

// 0x00600C50 (.h:34)
custom_follow_wounded::custom_follow_wounded()
    : ai_action("custom: follow wounded", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({156}))
{
    flags.has_enter = 1;
}

ai_action* custom_follow_wounded::get_instance()
{
    return &the_instance;
}

// 0x005DD9B0
custom_follow_wounded::~custom_follow_wounded()
{
}

// 0x00600EF0 (.h:34)
int custom_follow_wounded::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

saboteur_goto_action saboteur_goto_action::the_instance; // 0x01D75638

// 0x00601270 (.h:41)
saboteur_goto_action::saboteur_goto_action()
    : ai_action("saboteur goto", ai_plan_conditions_from_bits({4, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({157}))
{
}

ai_action* saboteur_goto_action::get_instance()
{
    return &the_instance;
}

// 0x005DD9D0
saboteur_goto_action::~saboteur_goto_action()
{
}

// 0x00601560 (.h:41)
int saboteur_goto_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

saboteur_bomb_action saboteur_bomb_action::the_instance; // 0x01D75304

// 0x00601590 (.h:42)
saboteur_bomb_action::saboteur_bomb_action()
    : ai_action("saboteur bomb", ai_plan_conditions_from_bits({7, 9, 47, 58, 136, 137, 145, 153, 157}), ai_plan_conditions_from_bits({156}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* saboteur_bomb_action::get_instance()
{
    return &the_instance;
}

// 0x005DD9E0
saboteur_bomb_action::~saboteur_bomb_action()
{
}

// 0x006018F0 (.h:42)
int saboteur_bomb_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

terrorize_goto_action terrorize_goto_action::the_instance; // 0x01D683A8

// 0x00601920 (.h:45)
terrorize_goto_action::terrorize_goto_action()
    : ai_action("terrorize goto", ai_plan_conditions_from_bits({0, 2, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({159}))
{
    flags.has_enter = 1;
}

ai_action* terrorize_goto_action::get_instance()
{
    return &the_instance;
}

// 0x005DD9F0
terrorize_goto_action::~terrorize_goto_action()
{
}

// 0x00601C70 (.h:45)
int terrorize_goto_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

terrorize_action terrorize_action::the_instance; // 0x01DDAD24

// 0x00601CB0 (.h:46)
terrorize_action::terrorize_action()
    : ai_action("terrorize", ai_plan_conditions_from_bits({0, 2, 9, 47, 58, 136, 137, 145, 153, 159}), ai_plan_conditions_from_bits({158}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

// 0x005DDA10
terrorize_action::~terrorize_action()
{
}

// 0x00602070 (.h:46)
int terrorize_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

coot_ramble_action coot_ramble_action::the_instance; // 0x01D75834

// 0x00602610 (.h:54)
coot_ramble_action::coot_ramble_action()
    : ai_action("coot ramble", ai_plan_conditions_from_bits({134}), ai_plan_conditions_from_bits({156}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* coot_ramble_action::get_instance()
{
    return &the_instance;
}

// 0x005DDA40
coot_ramble_action::~coot_ramble_action()
{
}

// 0x00602660 (.h:54)
int coot_ramble_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

tied_up_action tied_up_action::the_instance; // 0x01DDB688

// 0x006020B0 (.h:49)
tied_up_action::tied_up_action()
    : ai_action("tied up", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({146}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* tied_up_action::get_instance()
{
    return &the_instance;
}

// 0x005DDA20
tied_up_action::~tied_up_action()
{
}

// 0x00602350 (.h:49)
int tied_up_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

vehicle_hostage_action vehicle_hostage_action::the_instance; // 0x01DA3EB0

// 0x00602390 (.h:50)
vehicle_hostage_action::vehicle_hostage_action()
    : ai_action("vehicle hostage", ai_plan_conditions_from_bits({9, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({146}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* vehicle_hostage_action::get_instance()
{
    return &the_instance;
}

// 0x005DDA30
vehicle_hostage_action::~vehicle_hostage_action()
{
}

// 0x006025D0 (.h:50)
int vehicle_hostage_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

tech_upgrade_response tech_upgrade_response::the_instance; // 0x01DCA01C

// 0x006026A0 (.h:56)
tech_upgrade_response::tech_upgrade_response()
    : ai_action("tech upgrade response", ai_plan_conditions(), ai_plan_conditions_from_bits({156}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* tech_upgrade_response::get_instance()
{
    return &the_instance;
}

// 0x005DDA50
tech_upgrade_response::~tech_upgrade_response()
{
}

// 0x006026F0 (.h:56)
int tech_upgrade_response::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

drop_off_vehicle_action drop_off_vehicle_action::the_instance; // 0x01DA023C

// 0x00602730 (.h:59)
drop_off_vehicle_action::drop_off_vehicle_action()
    : ai_action("drop off vehicle", ai_plan_conditions_from_bits({9, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({113}))
{
    flags.has_enter = 1;
}

ai_action* drop_off_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005DDA60
drop_off_vehicle_action::~drop_off_vehicle_action()
{
}

// 0x00602970 (.h:59)
int drop_off_vehicle_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

custom_watch_player_action custom_watch_player_action::the_instance; // 0x01D68200

// 0x006029B0 (.h:62)
custom_watch_player_action::custom_watch_player_action()
    : ai_action("custom watch player", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({127}))
{
    flags.has_enter = 1;
}

ai_action* custom_watch_player_action::get_instance()
{
    return &the_instance;
}

// 0x005DDA70
custom_watch_player_action::~custom_watch_player_action()
{
}

// 0x00602C50 (.h:62)
int custom_watch_player_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

