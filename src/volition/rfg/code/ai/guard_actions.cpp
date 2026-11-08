// Original: volition/rfg/code/ai/guard_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/guard_actions.h"

#include "volition/rfg/code/ai/ai_building_logic.h"
#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/belief_iterator.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/ai/guard_selection.h"
#include "volition/rfg/code/ai/search_actions.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/geom/character.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_move.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_guard_node.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/rand.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>

// Position 23 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG23")

static const float MAX_GUARD_INTEREST_FOV_DOT = (float)cos(0.785385012626648); // 0x01DCA854: 45 degrees
static float DEFAULT_GUARD_NODE_DEFENSIVE_BUFFER = 20.0f; // 0x012C832C
static float NEAR_VEHICLE_DEST_DIST_SQRD = 100.0f; // 0x01642EDC, .data, never written
static float MAX_EXAMINE_VEHICLE_DIST_SQ = 100.0f; // 0x01642EE0, .data, never written
static float AI_VEHICLE_AT_CHECKPOINT_DIST_SQ = 25.0f; // 0x0164255C, .data, never written

// 0x012C8C30
static const animation_action Guard_anim_list[5] = {(animation_action)342, (animation_action)343, (animation_action)344, (animation_action)829, (animation_action)830};

static const unsigned int COND_REACH_RAIL_POS = 50;
static const signed char NO_GUARD_DIRECTION = -0x80;

// nav_type values rfg.exe tests here by number.
static const signed char NAVPOINT_TYPE_BUILDING = 10;
static const signed char NAVPOINT_TYPE_TRANSITION_IN = 11;
static const signed char NAVPOINT_TYPE_INDOOR = 14;

// 0x00638700 (:163). LTCG: np ESI.
static bool should_guard_combat_ready(const npc* np)
{
    if (np == nullptr)
    {
        return false;
    }
    if (np->current_team == HUMAN_TEAM_MARAUDER || activity_is_active(ACTIVITY_MODE_RAID_DEFEND) || activity_is_active(ACTIVITY_MODE_RAID_ATTACK))
    {
        return true;
    }
    return np->get_ai()->perception.get_belief(AI_BELIEF_ENEMY_EXISTS) != nullptr;
}

// 0x005EA750 (:188)
static void set_max_watch_period(ai_system* aip)
{
    aip->guard_node_watch_period.set_rand(10000, 15000);
}

// 0x0067EC10 (:201): in a building, the navpoint nearest to np: an indoor one with a neighbour below, or a door.
static const navpoint* get_best_indoor_navpoint_to_face(const npc* np)
{
    rfg_mover* building = nullptr;
    if (!np->get_ai()->perception.is_in_closest_building(np, &building, 0.0f, 1.25f))
    {
        return nullptr;
    }
    float best_dist = FLT_MAX;
    const navpoint* best = nullptr;
    const rfg_mover* root = building;
    if (building != nullptr && building->flags.is_ai_subpiece != 0)
    {
        root = get_root_building_internal(building);
    }
    // Original: root is read without a null check.
    const bool composite = root->flags.is_ai_composite != 0;
    farray<rfg_mover*, 200> movers;
    movers.add(const_cast<rfg_mover*>(root));
    for (int i = 0; i < movers.size(); i++)
    {
        const object* first = movers[i]->child_ptr;
        if (first == nullptr)
        {
            continue;
        }
        const object* child = first;
        do
        {
            if (child->get_obj_type() == OT_NAVPOINT)
            {
                const navpoint* nav = static_cast<const navpoint*>(child);
                if (nav->type == NAVPOINT_TYPE_INDOOR)
                {
                    for (int j = 0; j < nav->num_links; j++)
                    {
                        const navpoint* link = nav->links[j];
                        if (link->type != NAVPOINT_TYPE_INDOOR)
                        {
                            continue;
                        }
                        if (nav->pos.y < np->pos.y || link->pos.y < np->pos.y)
                        {
                            vector middle;
                            middle.x = (nav->pos.x + link->pos.x) * 0.5f;
                            middle.y = (nav->pos.y + link->pos.y) * 0.5f;
                            middle.z = (nav->pos.z + link->pos.z) * 0.5f;
                            vector closest;
                            ix_closest_point_on_line_segment(closest, np->pos, nav->pos, middle);
                            const float dist = vec_xz_dist(closest, np->pos) - nav->radius;
                            if (dist < best_dist)
                            {
                                best = vec_xz_dist_squared(nav->pos, np->pos) <= 25.0f ? link : nav;
                                best_dist = dist;
                            }
                        }
                        break;
                    }
                }
                else if (nav->type == NAVPOINT_TYPE_TRANSITION_IN)
                {
                    const float dist = vec_xz_dist(nav->pos, np->pos) - nav->radius;
                    if (dist < best_dist)
                    {
                        best_dist = dist;
                        best = nav;
                    }
                }
            }
            else if (composite && child->get_sub_type() == OT_SUB_MOVER_RFG)
            {
                if (movers.size() >= movers.capacity())
                {
                    return nullptr;
                }
                movers.add(const_cast<rfg_mover*>(static_cast<const rfg_mover*>(child)));
            }
            child = child->child_next;
        } while (child != first);
    }
    return best;
}

// 0x0067EF60 (:298). LTCG: np ESI, result in EDI.
static vector get_default_guard_facing(const npc* np, const object* objp)
{
    const navpoint* nav = get_best_indoor_navpoint_to_face(np);
    if (nav != nullptr)
    {
        return nav->pos;
    }
    // Away from the object guarded, or straight ahead when on top of it; 0.5 m up.
    vector dir;
    dir.x = np->pos.x - objp->pos.x;
    dir.y = 0.0f;
    dir.z = np->pos.z - objp->pos.z;
    if (dir.is_null())
    {
        dir = np->orient.fvec;
    }
    vector facing;
    facing.x = dir.x + np->pos.x + 0.0f;
    facing.y = np->pos.y + dir.y + 0.5f;
    facing.z = np->pos.z + dir.z + 0.0f;
    return facing;
}

// 0x005EA770 (:332). LTCG: guard_pos EAX, objp EDI.
static bool pick_guard_point_outside(vector& guard_pos, const npc*, const object* objp)
{
    if (objp == nullptr)
    {
        return false;
    }
    vector bmin;
    vector bmax;
    objp->get_local_bbox(bmin, bmax);
    vector center;
    center.x = (bmin.x + bmax.x) * 0.5f;
    center.y = (bmin.y + bmax.y) * 0.5f;
    center.z = (bmin.z + bmax.z) * 0.5f;
    float radius;
    if (objp->get_obj_type() == OT_BOUNDING_BOX)
    {
        const float half_x = bmax.x - center.x;
        const float half_z = bmax.z - center.z;
        radius = (half_x - half_z >= 0.0f ? half_z : half_x) * 0.8f;
    }
    else
    {
        radius = vec_xz_dist(center, bmax);
    }
    // On a circle round the object, from the FPU code: fl_rand() * 2 pi.
    const float angle = fl_rand() * 6.2831854820251465f;
    guard_pos.x = (float)cos(angle) * radius + objp->pos.x + center.x;
    guard_pos.y = radius * 0.0f + objp->pos.y;
    guard_pos.z = (float)sin(angle) * radius + objp->pos.z + center.z;
    vector on_mesh;
    nav_cell* cell;
    bool in_cell;
    if (!bb_pf_find_closest_point_on_nav_mesh_fast(on_mesh, cell, guard_pos, nullptr, &in_cell, false, 5.0f))
    {
        return false;
    }
    if (!in_cell)
    {
        nav_cell_get_centroid(&on_mesh, cell);
    }
    guard_pos = on_mesh;
    return havok_terrain_height(guard_pos.y, guard_pos.x, guard_pos.z, false, nullptr);
}

// 0x005EAA60 (:377): building navpoints more than 5 m from the npc.
static bool interior_guard_point_collect_callback(object* objp, unsigned int, const void* data)
{
    return static_cast<const navpoint*>(objp)->type == NAVPOINT_TYPE_BUILDING && vec_dist_squared(static_cast<const npc*>(data)->pos, objp->pos) > 25.0f;
}

// 0x0067F0C0 (:395). LTCG: guard_pos EDI, np EDX, objp EAX.
static bool pick_guard_point_inside(vector& guard_pos, const npc* np, const object* objp, const object* bb_obj)
{
    if (objp == nullptr)
    {
        return false;
    }
    farray<const navpoint*, 50> navpoints;
    world_collect_descendants<navpoint>(objp, navpoints, interior_guard_point_collect_callback, np);
    vector bmin;
    vector bmax;
    if (bb_obj != nullptr)
    {
        bb_obj->get_local_bbox(bmin, bmax);
    }
    while (navpoints.size() >= 1)
    {
        const int i = int_rand_range(0, navpoints.size() - 1);
        guard_pos = navpoints[i]->pos;
        if (bb_obj == nullptr || ix_point_in_box_oriented(guard_pos, bb_obj->pos, bb_obj->orient, bmin, bmax))
        {
            return true;
        }
        navpoints.remove_unsorted(i);
    }
    return false;
}

// 0x005EAAA0 (:489). LTCG: hp EAX. hp within 30 m and, unless ignored, within 45 degrees ahead of np.
static bool in_attention_area(const npc* np, const human* hp, bool ignore_direction)
{
    if (vec_dist_squared(hp->pos, np->pos) > 900.0f)
    {
        return false;
    }
    if (ignore_direction)
    {
        return true;
    }
    vector dir;
    dir.x = hp->pos.x - np->pos.x;
    dir.y = hp->pos.y - np->pos.y;
    dir.z = hp->pos.z - np->pos.z;
    dir.flatten_normalize_safe();
    return MAX_GUARD_INTEREST_FOV_DOT <= np->orient.fvec.y * dir.y + np->orient.fvec.x * dir.x + np->orient.fvec.z * dir.z;
}

// 0x0067F1A0 (:514). LTCG: aip EAX. A player among the unfriendly within 30 m, or else the first of them.
static human* select_attention_target_civilian(const npc* np, const ai_system* aip)
{
    human* first = nullptr;
    belief_obj_iterator<human> it;
    it.init(aip, AI_BELIEF_UNFRIENDLY);
    for (; it; ++it)
    {
        human* hp = it;
        if (vec_dist_squared(hp->pos, np->pos) <= 900.0f)
        {
            if (object_is_player(hp))
            {
                return hp;
            }
            if (first == nullptr)
            {
                first = hp;
            }
        }
    }
    return first;
}

// 0x00660330 (:539): the nearest ally moving (over 1 m/s) in view within 30 m.
static human* select_attention_target_ally(const npc* np, const ai_system*)
{
    human* best = nullptr;
    float best_dist_sq = FLT_MAX;
    const int count = World.get_num_objects<human>();
    for (int i = 0; i < count; i++)
    {
        human* hp = World.get_object<human>(i);
        if (human_teams_get_disposition(np, hp) != TEAM_DISPOSITION_ALLIED || hp == np || vec_dist_squared(hp->pos, np->pos) > 900.0f)
        {
            continue;
        }
        vector dir;
        dir.x = hp->pos.x - np->pos.x;
        dir.y = hp->pos.y - np->pos.y;
        dir.z = hp->pos.z - np->pos.z;
        dir.flatten_normalize_safe();
        if (MAX_GUARD_INTEREST_FOV_DOT <= np->orient.fvec.y * dir.y + dir.x * np->orient.fvec.x + np->orient.fvec.z * dir.z &&
            hp->velocity.x * hp->velocity.x + hp->velocity.y * hp->velocity.y + hp->velocity.z * hp->velocity.z > 1.0f)
        {
            const float dist_sq = vec_dist_squared(np->pos, hp->pos);
            if (dist_sq < best_dist_sq)
            {
                best = hp;
                best_dist_sq = dist_sq;
            }
        }
    }
    return best;
}

// 0x0067F260 (:574). LTCG: np EDI, aip ESI.
static human* should_watch_target_civilian(const npc* np, const ai_system* aip)
{
    human* current = World.object_from_handle<human>(aip->get_target_handle());
    if (current != nullptr && !current->is_killed() && vec_dist_squared(current->pos, np->pos) <= 900.0f)
    {
        // (2 < disposition) - 1 & current: hostile dispositions drop it.
        return (int)human_teams_get_disposition(current, np) > 2 ? nullptr : current;
    }
    return select_attention_target_civilian(np, aip);
}

// 0x00660560 (:592). LTCG: aip EAX, np EDI.
static human* should_watch_target_ally(const npc* np, const ai_system* aip)
{
    human* current = World.object_from_handle<human>(aip->get_target_handle());
    if (current != nullptr && !current->is_killed() && human_teams_get_disposition(current, np) != TEAM_DISPOSITION_ALLIED)
    {
        if (in_attention_area(np, current, false) && current->velocity.x * current->velocity.x + current->velocity.y * current->velocity.y + current->velocity.z * current->velocity.z >= 1.0f)
        {
            return current;
        }
        return nullptr;
    }
    return select_attention_target_ally(np, aip);
}

// Attention away from any object, then a look at a point; rfg.exe inlines this in three functions below.
static void look_at_point(npc* np, ai_system* aip, const vector& target)
{
    aip->perception.stop_paying_attention(false);
    aip->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    human_set_look_at_target(np, target, false, 0.02f);
}

// 0x00660620 (:645). LTCG: aip ESI, np EDI.
static bool is_turning_towards_facing(npc* np, ai_system* aip, const object_guard_node* gnp, bool change_attention_mode)
{
    static const float SAME_DIRECTION_DOT = (float)cos(0.2617949843406677); // 15 degrees
    if (gnp == nullptr)
    {
        return true;
    }
    const signed char index = aip->get_guard_node() != nullptr ? aip->get_guard_direction_index() : NO_GUARD_DIRECTION;
    const vector facing = gnp->get_facing_from_index(index);
    if (np->orient.fvec.y * facing.y + np->orient.fvec.x * facing.x + np->orient.fvec.z * facing.z >= SAME_DIRECTION_DOT)
    {
        return false;
    }
    vector target;
    target.x = np->pos.x + facing.x * 10.0f;
    target.y = np->pos.y + facing.y * 10.0f;
    target.z = np->pos.z + facing.z * 10.0f;
    human_move_turn_to_target(np, target);
    if (change_attention_mode)
    {
        target.y += 0.5f;
        look_at_point(np, aip, target);
    }
    return true;
}

// 0x00660860 (:683). LTCG: aip ESI, np EDI. Glances somewhere ahead every 3 to 5 s.
static void do_guard_idle_action_look(npc* np, ai_system* aip)
{
    static const float GUARD_LOOK_DIRECTION_DIST_Z = 10.0f;
    static const float GUARD_LOOK_DIRECTION_DIST_X = GUARD_LOOK_DIRECTION_DIST_Z * 0.70710677f;
    static const float GUARD_LOOK_DIRECTION_DIST_Y = GUARD_LOOK_DIRECTION_DIST_X * 0.01f;
    if (aip->action_timer.valid() && !aip->action_timer.elapsed())
    {
        return;
    }
    const float side = GUARD_LOOK_DIRECTION_DIST_X * fl_rand();
    const float up = GUARD_LOOK_DIRECTION_DIST_Y * fl_rand();
    const unsigned int signs = vrand();
    const float side_sign = (signs & 1) == 0 ? -1.0f : 1.0f;
    const float up_sign = (signs & 2) == 0 ? -1.0f : 1.0f;
    vector target;
    target.x = np->pos.x + np->orient.fvec.x * GUARD_LOOK_DIRECTION_DIST_Z + np->orient.rvec.x * side_sign * side + np->orient.uvec.x * up_sign * up;
    target.y = np->pos.y + np->orient.fvec.y * GUARD_LOOK_DIRECTION_DIST_Z + np->orient.rvec.y * side_sign * side + np->orient.uvec.y * up_sign * up + 0.5f;
    target.z = np->pos.z + np->orient.fvec.z * GUARD_LOOK_DIRECTION_DIST_Z + np->orient.rvec.z * side_sign * side + np->orient.uvec.z * up_sign * up;
    look_at_point(np, aip, target);
    aip->action_timer.set_rand(3000, 5000);
}

// 0x00660CD0 (:750). LTCG: aip ESI, np EDI. Sweeps the head 45 degrees to each side in 16 s; true when done.
static bool do_guard_idle_action_scan(npc* np, ai_system* aip)
{
    static const float MAX_FOV_RANGE = 1.57077f;
    if (aip->action_timer.valid())
    {
        if (aip->action_timer.elapsed())
        {
            return true;
        }
    }
    else
    {
        aip->action_timer.set(16000);
    }
    // From the FPU code: a triangle wave over the 16 s, 0x1F40 is the half.
    const int ramp = 8000 - abs(8000 - aip->action_timer.time_until());
    const float angle = 0.785385012626648f - ((float)(ramp / 8000.0) * MAX_FOV_RANGE);
    vector dir;
    dir.x = -(float)sin(angle);
    dir.y = 0.0f;
    dir.z = (float)cos(angle);
    dir.unrotate(np->orient);
    vector target;
    target.x = np->pos.x + dir.x * 10.0f;
    target.y = np->pos.y + dir.y * 10.0f + 0.5f;
    target.z = np->pos.z + dir.z * 10.0f;
    look_at_point(np, aip, target);
    return false;
}

// 0x005EABD0 (:805). LTCG: anim_index ESI, np EDI.
static int do_guard_idle_animation(npc* np, ai_system* aip, animation_action anim_index)
{
    np->char_inst->play_action(anim_index, ANIM_ALIAS_NONE, 1.0f, 0);
    aip->desired_anim_action = anim_index;
    return (int)(vanim_get_anim_length(np->char_inst->vp, anim_index) * 1000.0); // from the FPU code: s to ms
}

// 0x005EAC20 (:818). LTCG: np EDI. True when the animation has played out.
static bool do_guard_idle_action_animate(npc* np, ai_system* aip)
{
    if (aip->action_timer.valid())
    {
        return aip->action_timer.elapsed();
    }
    aip->action_timer.set(do_guard_idle_animation(np, aip, Guard_anim_list[vrand() % 5]));
    return false;
}

// 0x005EACC0 (:840). LTCG: aip ESI, np EDI.
static void do_guard_idle_anim_cleanup(npc* np, ai_system* aip)
{
    if (np->char_inst->is_playing_action(aip->desired_anim_action))
    {
        np->char_inst->stop_action(aip->desired_anim_action, 10, false);
    }
}

// 0x006A24B0 (:848). LTCG: np EAX, aip EDI.
static bool do_watch_common(npc* np, ai_system* aip, const object_guard_node* gnp)
{
    const human* target = should_watch_target_civilian(np, aip);
    if (target == nullptr)
    {
        if (is_turning_towards_facing(np, aip, gnp, true))
        {
            return true;
        }
        target = should_watch_target_ally(np, aip);
        if (target == nullptr)
        {
            return false;
        }
    }
    aip->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    return true;
}

// 0x00638740 (:870). LTCG: np EAX, setup_data ECX.
static void guard_at_node_commit_common(npc* np, const ai_action_setup_data& setup_data)
{
    ai_system* ai = np->get_ai();
    object* target = setup_data.use_target;
    if (target == nullptr || target->get_obj_type() != OT_GUARD_NODE)
    {
        return;
    }
    object_guard_node* gnp = static_cast<object_guard_node*>(target);
    if (gnp->can_reserve(np->get_handle()))
    {
        const signed char index = ai->get_guard_direction_index();
        ai->reserve_guard_node(gnp, index == NO_GUARD_DIRECTION ? 0 : index);
    }
}

// 0x006A2520 (:893). LTCG: np EAX.
static ai_results guard_at_node_enter_common(npc* np, bool same_action)
{
    ai_system* ai = np->get_ai();
    if (ai->get_guard_node() != nullptr)
    {
        if (ai->is_at_guard_post())
        {
            if (!same_action)
            {
                ai->action_timer.invalidate();
            }
            human_set_combat_ready(np, should_guard_combat_ready(np), 5000);
            return AI_RESULT_SUCCESS;
        }
        ai->release_guard_pos();
    }
    return AI_RESULT_FAILURE;
}

// 0x006A25B0 (:921). LTCG: np EAX.
static object_guard_node* guard_at_node_do_action_common(npc* np)
{
    ai_system* ai = np->get_ai();
    object_guard_node* gnp = ai->get_guard_node();
    if (gnp == nullptr)
    {
        return nullptr;
    }
    if (ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION) == nullptr && ai->is_at_guard_post() && ai->reserve_guard_node(gnp, NO_GUARD_DIRECTION))
    {
        return gnp;
    }
    ai->release_guard_pos();
    return nullptr;
}

// 0x005EACF0 (:952). LTCG: np ECX.
static ai_results guard_at_node_cleanup_common(npc* np, bool same_action)
{
    const animation_action anim = np->get_ai()->desired_anim_action;
    if (!same_action && anim != ANIMATION_ACTION_NONE)
    {
        np->char_inst->stop_action(anim, 3, false);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005EAD20 (:975)
int go_to_guard_post_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 1000;
}

// 0x006A2630 (:982)
ai_results go_to_guard_post_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    vector guard_pos;
    if (!ai->get_guard_pos(guard_pos))
    {
        return AI_RESULT_FAILURE;
    }
    object_guard_node* gnp = ai->get_guard_node();
    if (guard_pos == ZERO_VECTOR || ai->outside_defensive_perimeter(guard_pos, false, nullptr))
    {
        return AI_RESULT_FAILURE;
    }
    if (gnp == nullptr)
    {
        ai->reserve_guard_pos(guard_pos);
    }
    else if (!ai->reserve_guard_node(gnp, NO_GUARD_DIRECTION))
    {
        return AI_RESULT_FAILURE;
    }
    if (should_guard_combat_ready(np))
    {
        human_set_combat_ready(np, true, 0);
    }
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }

    const ai_goal* goal = ai->planner.get_current_goal();
    int m_state = ai->flags.run_to_guard_post + HMS_WALK;
    if (goal == acquire_target_goal::get_instance())
    {
        const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, np->combat.target_handle);
        if (belief == nullptr)
        {
            return AI_RESULT_FAILURE;
        }
        m_state = HMS_RUN;
        ai->perception.pay_attention(belief->handle, belief->get_type(), ATTENTION_ACQUIRE_VISUAL, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
        human_set_combat_ready(np, true, 0);
    }
    else if (goal == watch_area_goal::get_instance() || goal == defensive_combat_goal::get_instance())
    {
        m_state = HMS_RUN;
    }
    else
    {
        const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION);
        bool ready = true;
        if (belief == nullptr)
        {
            ai->perception.stop_paying_attention(false);
            ai->perception.set_attention_mode(ATTENTION_IDLE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
            ready = should_guard_combat_ready(np);
        }
        else
        {
            ai->perception.pay_attention(belief->handle, belief->get_type(), ATTENTION_AIM_AT_BELIEF, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        }
        human_set_combat_ready(np, ready, 5000);
        // Runs after a player when farther than 1.3 times the half diagonal of what it defends (1.69 = 1.3 squared).
        const object* defended = ai->is_in_player_squad(true) != nullptr ? ai->get_defend_object() : nullptr;
        if (defended != nullptr)
        {
            vector bmin;
            vector bmax;
            defended->get_local_bbox(bmin, bmax);
            if (vec_dist_squared(defended->pos, np->pos) > vec_dist_squared(bmin, bmax) * 0.5f * 1.6899998f)
            {
                m_state = HMS_RUN;
            }
        }
    }
    ai->action_substate = m_state;
    return AI_RESULT_SUCCESS;
}

// 0x006A2B40 (:1081)
ai_results go_to_guard_post_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    vector guard_pos;
    if (!ai->get_guard_pos(guard_pos))
    {
        return AI_RESULT_FAILURE;
    }
    object_guard_node* gnp = ai->get_guard_node();
    if (gnp != nullptr && (gnp->is_disabled()))
    {
        ai->release_guard_pos();
        return AI_RESULT_FAILURE;
    }
    if (guard_pos == ZERO_VECTOR || ai->outside_defensive_perimeter(guard_pos, false, nullptr))
    {
        return AI_RESULT_FAILURE;
    }
    if (!(gnp == nullptr ? ai->reserve_guard_pos(guard_pos) : ai->reserve_guard_node(gnp, NO_GUARD_DIRECTION)))
    {
        return AI_RESULT_FAILURE;
    }
    np->combat.combat_maneuver_timestamp.set(3000);
    if (should_guard_combat_ready(np))
    {
        human_set_combat_ready(np, true, 0);
    }
    if (ai->perception.get_attention_mode() == ATTENTION_AIM_AT_BELIEF && !human_is_combat_ready(np))
    {
        ai->perception.stop_paying_attention(false);
        ai->perception.set_attention_mode(ATTENTION_IDLE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    }

    const ai_goal* goal = ai->planner.get_current_goal();
    if (goal == watch_area_goal::get_instance())
    {
        human_set_combat_ready(np, true, 0);
    }
    else if (goal == defensive_combat_goal::get_instance())
    {
        ai_combat_action_parameters params;
        params.init_npc(np);
        if (!params.init_target(true))
        {
            return AI_RESULT_FAILURE;
        }
        if (ai->perception.get_attention_mode() != ATTENTION_AIM_AT_OBJECT)
        {
            ai->perception.set_attention_mode(ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        }
        human_set_combat_ready(np, true, 0);
        if (ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr && ai->perception.can_engage_target_reason(params.target) == AI_ENGAGE_ALLOWED)
        {
            ai_combat_fire_at_target(params, true);
        }
    }
    else if (goal == acquire_target_goal::get_instance())
    {
        human_set_combat_ready(np, true, 0);
        if (ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr)
        {
            return AI_RESULT_SUCCESS;
        }
    }

    if (ai->is_pathing_to(guard_pos, 0.25f, 1.0f))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (ai->is_at_guard_post())
    {
        return AI_RESULT_SUCCESS;
    }
    if (guard_pos == ZERO_VECTOR || ai->outside_defensive_perimeter(guard_pos, false, &DEFAULT_GUARD_NODE_DEFENSIVE_BUFFER))
    {
        // Out of reach for good: nobody else needs to try this node of what the squad defends.
        const object* defended = gnp != nullptr ? ai->get_defend_object() : nullptr;
        if (defended != nullptr && defended->get_handle() == gnp->get_parent())
        {
            gnp->guard_node_flags.disabled = 1;
        }
    }
    else if (ai_pf_to_result(ai->pathfind_to(guard_pos, (human_movement_states)ai->action_substate, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false)) != AI_RESULT_FAILURE)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    ai->release_guard_pos();
    return AI_RESULT_FAILURE;
}

// 0x005EAD30 (:1205)
int roadblock_guard_rule::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 100000;
}

// 0x00638790 (:1213)
int roadblock_vehicle_rule::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const object_squad* squad = np->get_ai()->get_squad();
    if (squad == nullptr)
    {
        return -1;
    }
    const vehicle* vp = squad->get_vehicle();
    if (vp == nullptr || vp->is_killed())
    {
        return -1;
    }
    return np->vehicle_handle != vp->get_handle() ? 10 : -1; // (-x & 0xB) - 1
}

// 0x005EAD40 (:1243)
int default_guard_rule::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 5000;
}

// 0x0067F2E0 (:1248)
int default_guard_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    const object* defended = goal_setup_data.defend_object;
    if (defended == nullptr)
    {
        return -1;
    }
    if (ai->has_valid_guard_post())
    {
        if (!ai->get_guard_pos(my_data.pos))
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            return -1;
        }
        return 1000;
    }
    if (defended->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        // Only some marauders (16 in 100, EDX <= 0xF) go inside the building.
        if (np->current_team == HUMAN_TEAM_MARAUDER && vrand() % 100 <= 15 && pick_guard_point_inside(my_data.pos, np, defended, nullptr))
        {
            return 1000;
        }
    }
    else if (defended->get_obj_type() == OT_BOUNDING_BOX)
    {
        const object* parent = World.object_from_handle(defended->get_parent());
        if (parent != nullptr && pick_guard_point_inside(my_data.pos, np, parent, defended))
        {
            return 1000;
        }
    }
    return pick_guard_point_outside(my_data.pos, np, defended) ? 1000 : -1;
}

// 0x005EAD50 (:1301)
void default_guard_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    np->get_ai()->reserve_guard_pos(setup_data.pos);
}

// 0x006A3310 (:1314)
ai_results default_guard_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    gfm_orders* orders = squad->get_squad_orders();
    if (orders == nullptr || (orders->get_orders_type() != GFM_ORDERS_TYPE_DEFEND && orders->get_orders_type() != GFM_ORDERS_TYPE_ROADBLOCK))
    {
        return AI_RESULT_FAILURE;
    }
    const object* target = orders->get_target_object();
    if (target == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (!ai->is_at_guard_post())
    {
        ai->release_guard_pos();
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, should_guard_combat_ready(np), 0);
    if (last_action != this)
    {
        ai->perception.set_attention_mode(ATTENTION_IDLE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        ai->action_timer.set(15000);
        ai->desired_goal_pos = get_default_guard_facing(np, target);
        ai->action_substate = 0;
        return AI_RESULT_SUCCESS;
    }
    // Replanned while guarding: an idle animation, once, half the time.
    if (should_guard_combat_ready(np) || ai->action_substate != 0 || !int_rand_chance(50))
    {
        return AI_RESULT_SUCCESS;
    }
    ai->action_substate = 1;
    ai->action_timer.invalidate();
    ai->perception.set_attention_mode(ATTENTION_IDLE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    if (do_guard_idle_action_animate(np, ai))
    {
        ai->desired_anim_action = ANIMATION_ACTION_NONE;
    }
    ai->action_timer.set(ai->action_timer.time_until() + 7500); // 0x1D4C
    return AI_RESULT_SUCCESS;
}

// 0x006A3610 (:1366)
ai_results default_guard_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    gfm_orders* orders = squad->get_squad_orders();
    if (orders == nullptr || (orders->get_orders_type() != GFM_ORDERS_TYPE_DEFEND && orders->get_orders_type() != GFM_ORDERS_TYPE_ROADBLOCK) || orders->get_target_object() == nullptr || !ai->is_at_guard_post())
    {
        return AI_RESULT_FAILURE;
    }
    if (np->current_team != HUMAN_TEAM_MARAUDER)
    {
        ai->stop_moving();
        vector guard_pos;
        if (!ai->get_guard_pos(guard_pos) || !ai->reserve_guard_pos(guard_pos))
        {
            return AI_RESULT_FAILURE;
        }
        human_set_combat_ready(np, should_guard_combat_ready(np), 0);
        human_move_turn_to_target(np, ai->desired_goal_pos);
        if (!ai->action_timer.elapsed())
        {
            return AI_RESULT_IN_PROGRESS;
        }
    }
    ai->release_guard_pos();
    return AI_RESULT_SUCCESS;
}

// 0x005EADA0 (:1414)
ai_results default_guard_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        do_guard_idle_anim_cleanup(np, np->get_ai());
    }
    return AI_RESULT_SUCCESS;
}

// 0x00638810 (:1433)
static bool guard_can_use_turret(const npc* np, const turret* tp)
{
    const ai_system* ai = np->get_ai();
    if (tp == nullptr || !tp->is_functional(false) || tp->turret_flags.vehicle_turret || tp->pos == ZERO_VECTOR || ai->outside_defensive_perimeter(tp->pos, false, nullptr))
    {
        return false;
    }
    const human* op = human_combat_turret_get_operator(tp);
    if (np != op)
    {
        if (op != nullptr || (tp->disable_for_npcs_time.valid() && !tp->disable_for_npcs_time.elapsed()))
        {
            return false;
        }
        if (np->combat.reserved_turret != tp->get_handle())
        {
            return human_combat_turret_get_reserving_human(tp) == nullptr;
        }
    }
    return true;
}

// 0x00660ED0 (:1467)
int guard_from_turret_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    turret* reserved = human_combat_get_reserved_turret(np);
    if (guard_can_use_turret(np, reserved))
    {
        my_data.use_target = reserved;
        return 300;
    }
    turret* best = nullptr;
    float best_dist_sq = FLT_MAX;
    const int count = World.get_num_objects<turret>();
    for (int i = 0; i < count; i++)
    {
        turret* tp = World.get_object<turret>(i);
        if (guard_can_use_turret(np, tp))
        {
            const float dist_sq = vec_dist_squared(np->pos, tp->pos);
            if (dist_sq < best_dist_sq)
            {
                best_dist_sq = dist_sq;
                best = tp;
            }
        }
    }
    if (best == nullptr)
    {
        return -1;
    }
    my_data.use_target = best;
    return 300;
}

// 0x00638AA0 (:1502)
void guard_from_turret_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    object* target = setup_data.use_target;
    if (target != nullptr && target->get_obj_type() == OT_TURRET)
    {
        human_combat_reserve_turret(np, static_cast<turret*>(target));
    }
}

// 0x00660FC0 (:1509)
ai_results guard_from_turret_action::enter(npc* np, const ai_action*) const
{
    np->get_ai()->perception.set_attention_mode(ATTENTION_IDLE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    return AI_RESULT_SUCCESS;
}

// 0x00638AD0 (:1517)
ai_results guard_from_turret_action::do_action(npc* np) const
{
    const ai_system* ai = np->get_ai();
    turret* tp = human_combat_get_manned_turret(np);
    const object_squad* squad = tp != nullptr ? ai->get_squad() : nullptr;
    if (squad == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    gfm_orders* orders = squad->get_squad_orders();
    if (orders == nullptr || (orders->get_orders_type() != GFM_ORDERS_TYPE_DEFEND && orders->get_orders_type() != GFM_ORDERS_TYPE_ROADBLOCK) || orders->get_target_object() == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    vector aim;
    aim.x = tp->orient.uvec.x + tp->pos.x + tp->orient.fvec.x * 5.0f;
    aim.y = tp->orient.uvec.y + tp->pos.y + tp->orient.fvec.y * 5.0f;
    aim.z = tp->orient.uvec.z + tp->pos.z + tp->orient.fvec.z * 5.0f;
    tp->aim_turret(aim);
    return AI_RESULT_IN_PROGRESS;
}

// 0x0067F3E0 (:1543)
int guard_at_post_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const player* pl = player_get_local_player();
    if (pl == nullptr)
    {
        return -1;
    }
    if (np->current_team == HUMAN_TEAM_MARAUDER && human_teams_get_disposition(pl, HUMAN_TEAM_MARAUDER) != TEAM_DISPOSITION_ALLIED)
    {
        return -1;
    }
    const ai_system* ai = np->get_ai();
    if (ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION) != nullptr)
    {
        return -1;
    }
    object_guard_node* gnp = nullptr;
    if (ai->has_valid_guard_post())
    {
        gnp = ai->get_guard_node();
    }
    if (gnp == nullptr)
    {
        const weapon* firearm = ai_combat_get_action_firearm(np);
        gnp = firearm != nullptr && firearm->w_info->flags.can_scope ? guard_selection_choose_sniper_node(np, nullptr) : guard_selection_choose_node(np, GN_SELECT_CLOSEST_AVAILABLE);
        if (gnp == nullptr)
        {
            return -1;
        }
    }
    if (gnp->is_disabled())
    {
        return -1;
    }
    my_data.use_target = gnp;
    return 500;
}

// 0x00638C30 (:1584)
void guard_at_post_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    guard_at_node_commit_common(np, setup_data);
}

// 0x006A3710 (:1589)
ai_results guard_at_post_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this && human_teams_get_disposition(np, HUMAN_TEAM_EDF) != TEAM_DISPOSITION_ALLIED)
    {
        np->get_ai()->perception.set_attention_mode(ATTENTION_IDLE, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    }
    return guard_at_node_enter_common(np, last_action == this);
}

// 0x006A3830 (:1606)
ai_results guard_at_post_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr || squad->get_squad_orders() == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    object_guard_node* gnp = guard_at_node_do_action_common(np);
    if (gnp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const weapon* firearm = ai_combat_get_action_firearm(np);
    const bool sniper = firearm != nullptr && firearm->w_info->flags.can_scope;
    if (!sniper && gnp->get_group_id() != 0 && ai->get_time_in_guard_node_group() > 150000) // 2.5 minutes in a group
    {
        object_guard_node* next = guard_selection_choose_node(np, GN_SELECT_DIFFERENT_GROUP);
        ai->reset_time_in_guard_node_group();
        if (next != nullptr)
        {
            return ai->reserve_guard_node(next, 0) ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
        }
    }
    if (ai->get_time_at_guard_node() > 20000)
    {
        object_guard_node* next = sniper ? guard_selection_choose_sniper_node(np, gnp) : guard_selection_choose_node(np, GN_SELECT_NEW_NODE_SAME_GROUP);
        ai->reset_time_at_guard_node();
        if (next != nullptr)
        {
            return ai->reserve_guard_node(next, 0) ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
        }
    }

    const human_team_dispositions disposition = human_teams_get_disposition(np, HUMAN_TEAM_EDF);
    if (disposition == TEAM_DISPOSITION_ALLIED)
    {
        if (!ai->guard_node_watch_period.valid() || ai->guard_node_watch_period.elapsed())
        {
            set_max_watch_period(ai);
        }
        else if (do_watch_common(np, ai, gnp))
        {
            return AI_RESULT_IN_PROGRESS;
        }
    }
    else if (is_turning_towards_facing(np, ai, gnp, false))
    {
        return AI_RESULT_IN_PROGRESS;
    }

    const signed char min_index = gnp->get_min_facing_index();
    const signed char max_index = gnp->get_max_facing_index();
    if (max_index > min_index && ai->get_time_at_guard_facing() > 15000)
    {
        signed char index = ai->get_guard_direction_index();
        if (index == min_index || (index != max_index && !int_rand_chance(50)))
        {
            index++;
        }
        else
        {
            index--;
        }
        return ai->reserve_guard_node(gnp, index) ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
    }
    if (disposition == TEAM_DISPOSITION_ALLIED)
    {
        do_guard_idle_action_look(np, ai);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005EADF0 (:1718)
ai_results guard_at_post_action::cleanup(npc* np, const ai_action* next_action) const
{
    return guard_at_node_cleanup_common(np, next_action == this);
}

// Shared by the scan and animate costs: EDF and friends on a quiet watch at a guard node.
static object_guard_node* can_idle_at_post(const npc* np)
{
    if (np->current_team == HUMAN_TEAM_MARAUDER || activity_is_active(ACTIVITY_MODE_RAID_DEFEND) || activity_is_active(ACTIVITY_MODE_RAID_ATTACK))
    {
        return nullptr;
    }
    const ai_system* ai = np->get_ai();
    if (ai->perception.get_belief(AI_BELIEF_ENEMY_EXISTS) != nullptr)
    {
        return nullptr;
    }
    object_guard_node* gnp = ai->get_guard_node();
    return ai->has_valid_guard_post() ? gnp : nullptr;
}

// 0x0067F490 (:1726)
int guard_at_post_scan_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    object_guard_node* gnp = can_idle_at_post(np);
    if (gnp == nullptr)
    {
        return -1;
    }
    my_data.use_target = gnp;
    const ai_action* current = np->get_ai()->planner.get_current_action();
    if (current == this)
    {
        return 300;
    }
    if (current != guard_at_post_animate_action::get_instance() && fl_rand() <= 0.1f)
    {
        return 300;
    }
    return -1;
}

// 0x00638C90 (:1770)
void guard_at_post_scan_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    guard_at_node_commit_common(np, setup_data);
}

// 0x006A3A60 (:1775)
ai_results guard_at_post_scan_action::enter(npc* np, const ai_action* last_action) const
{
    return guard_at_node_enter_common(np, last_action == this);
}

// 0x006A3A80 (:1780)
ai_results guard_at_post_scan_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const object_guard_node* gnp = guard_at_node_do_action_common(np);
    if (gnp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (ai->guard_node_watch_period.valid() && !ai->guard_node_watch_period.elapsed())
    {
        if (do_watch_common(np, ai, gnp))
        {
            return AI_RESULT_IN_PROGRESS;
        }
    }
    else
    {
        set_max_watch_period(ai);
    }
    return do_guard_idle_action_scan(np, ai) ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

// 0x005EAE30 (:1809)
ai_results guard_at_post_scan_action::cleanup(npc* np, const ai_action* next_action) const
{
    return guard_at_node_cleanup_common(np, next_action == this);
}

// 0x0067F540 (:1817)
int guard_at_post_animate_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    object_guard_node* gnp = can_idle_at_post(np);
    if (gnp == nullptr)
    {
        return -1;
    }
    my_data.use_target = gnp;
    const ai_action* current = np->get_ai()->planner.get_current_action();
    if (current == this)
    {
        return 200;
    }
    if (current != guard_at_post_scan_action::get_instance() && fl_rand() <= 0.1f)
    {
        return 200;
    }
    return -1;
}

// 0x00638CF0 (:1858)
void guard_at_post_animate_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    guard_at_node_commit_common(np, setup_data);
}

// 0x006A3B10 (:1864)
ai_results guard_at_post_animate_action::enter(npc* np, const ai_action* last_action) const
{
    return guard_at_node_enter_common(np, last_action == this);
}

// 0x006A3B30 (:1869)
ai_results guard_at_post_animate_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (guard_at_node_do_action_common(np) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (do_guard_idle_action_animate(np, ai))
    {
        ai->desired_anim_action = ANIMATION_ACTION_NONE;
        return AI_RESULT_SUCCESS;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005EAE70 (:1890)
ai_results guard_at_post_animate_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        do_guard_idle_anim_cleanup(np, np->get_ai());
    }
    return guard_at_node_cleanup_common(np, next_action == this);
}

// 0x006A3B80 (:1904)
int guard_from_vehicle_action::get_cost(ai_action_setup_data&, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const object_squad* squad = np->get_ai()->get_squad();
    if (squad == nullptr || !squad->stay_in_vehicle())
    {
        return -1;
    }
    const vehicle* vp = squad->get_vehicle();
    if (vp == nullptr)
    {
        return -1;
    }
    if (!(ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER) && ai_vehicle_is_seat_available(vp, np, VSI_DRIVER)) && !ai_vehicle_is_usable_and_available(vp, np, AI_VEHICLE_SEAT_TYPE_PASSENGER))
    {
        return -1;
    }
    const gfm_orders* orders = squad->get_squad_orders();
    if (orders == nullptr)
    {
        return -1;
    }
    const vector orders_pos = orders->get_orders_pos();
    if (vec_dist_squared(vp->pos, orders_pos) > NEAR_VEHICLE_DEST_DIST_SQRD)
    {
        desired_conditions |= ai_plan_conditions(COND_REACH_RAIL_POS);
    }
    return 100;
}

// 0x006A3CB0 (:1951)
void guard_from_vehicle_action::commit_setup_data(npc* np, const ai_action_setup_data&) const
{
    ai_system* ai = np->get_ai();
    ai->desired_vehicle = 0xFFFFFFFF;
    const object_squad* squad = ai->get_squad();
    const vehicle* vp = squad != nullptr ? squad->get_vehicle() : nullptr;
    if (vp == nullptr)
    {
        return;
    }
    if (ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER) && ai_vehicle_is_seat_available(vp, np, VSI_DRIVER))
    {
        ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_DRIVER;
    }
    else if (ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_PASSENGER) && passenger_seat_available(vp, np))
    {
        ai->desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_PASSENGER;
    }
    else
    {
        return;
    }
    const gfm_orders* orders = squad->get_squad_orders();
    if (orders != nullptr)
    {
        ai->desired_vehicle = vp->get_handle();
        ai->desired_goal_pos = orders->get_orders_pos();
        ai->desired_vehicle_dist_to_goal_pos = 15.0f;
    }
}

// 0x005EAEF0 (:1990)
ai_results guard_from_vehicle_action::do_action(npc* np) const
{
    const vehicle* vp = human_in_vehicle(np);
    if (vp != nullptr && vehicle_passenger_in_seat(vp, VSI_DRIVER) != nullptr)
    {
        vehicle_set_controls_when_abandoned(vp);
    }
    return AI_RESULT_IN_PROGRESS;
}

// The driver of a vehicle at a checkpoint, when it is an npc this guard does not trust.
static bool unfriendly_npc_driver(const vehicle* vp, const npc* np, human** driver_out)
{
    human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
    if (driver == nullptr || driver->get_obj_type() != OT_HUMAN || driver->get_sub_type() != OT_SUB_HUMAN_NPC || !human_teams_are_unfriendly(driver, np))
    {
        return false;
    }
    if (driver_out != nullptr)
    {
        *driver_out = driver;
    }
    return true;
}

// 0x0067F5F0 (:2002)
int guard_examine_vehicle_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    if (human_teams_get_disposition(np, HUMAN_TEAM_EDF) != TEAM_DISPOSITION_ALLIED)
    {
        return -1;
    }
    const object_squad* squad = ai->get_squad();
    if (squad == nullptr)
    {
        return -1;
    }
    gfm_orders* orders = squad->get_squad_orders();
    if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_DEFEND || human_in_vehicle_interaction(np, nullptr))
    {
        return -1;
    }
    if (ai->planner.get_current_action() == this)
    {
        vehicle* current = World.object_from_handle<vehicle>(ai->desired_vehicle);
        if (current != nullptr && unfriendly_npc_driver(current, np, nullptr))
        {
            my_data.use_target = current;
            return 150;
        }
    }
    vehicle* best = nullptr;
    float best_dist_sq = MAX_EXAMINE_VEHICLE_DIST_SQ;
    belief_obj_iterator<vehicle> it;
    it.init(ai, AI_BELIEF_VEHICLE);
    for (; it; ++it)
    {
        vehicle* vp = it;
        const object* checkpoint = World.object_from_handle<navpoint>(vp->vai.checkpoint_handle);
        human* driver = nullptr;
        if (checkpoint == nullptr || vec_xz_dist_squared(vp->pos, checkpoint->pos) > AI_VEHICLE_AT_CHECKPOINT_DIST_SQ || !unfriendly_npc_driver(vp, np, &driver))
        {
            continue;
        }
        // Each driver once, except the one already being looked at.
        if (driver->has_been_acknowledged() && (ai->planner.get_current_action() != this || ai->desired_vehicle != vp->get_handle()))
        {
            continue;
        }
        vector local_pos = np->pos;
        vec_change_to_local_coords(local_pos, vp->pos, vp->orient);
        vector bmin;
        vector bmax;
        vp->get_local_bbox(bmin, bmax);
        const float dist_sq = bbox_find_distance_squared(bmin, bmax, local_pos);
        if (dist_sq < best_dist_sq)
        {
            best_dist_sq = dist_sq;
            best = vp;
        }
    }
    if (best == nullptr)
    {
        return -1;
    }
    my_data.use_target = best;
    return 150;
}

// 0x006A3DB0 (:2083)
void guard_examine_vehicle_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    const object* target = setup_data.use_target;
    if (target == nullptr || target->get_obj_type() != OT_VEHICLE)
    {
        return;
    }
    ai->desired_vehicle = target->get_handle();
    human* driver = vehicle_passenger_in_seat(static_cast<const vehicle*>(target), VSI_DRIVER);
    if (driver != nullptr)
    {
        ai->perception.pay_attention(driver->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        driver->acknowledged_time.set(15000);
    }
}

// 0x006610C0 (:2101)
ai_results guard_examine_vehicle_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        ai->action_substate = 0;
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        ai->action_timer.set(8000);
    }
    return AI_RESULT_SUCCESS;
}

// 0x006A3E20 (:2121)
ai_results guard_examine_vehicle_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = World.object_from_handle<vehicle>(ai->desired_vehicle);
    if (vp == nullptr || vp->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
    if (driver == nullptr || driver->get_obj_type() != OT_HUMAN || driver->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return AI_RESULT_FAILURE;
    }
    driver->acknowledged_time.set(15000);
    if (ai->action_substate < 2)
    {
        if (vp->vai.checkpoint_handle == 0xFFFFFFFF)
        {
            return AI_RESULT_FAILURE;
        }
        // Holds the vehicle at the checkpoint for as long as the inspection takes.
        if (!vp->vai.checkpoint_timer.valid() || vp->vai.checkpoint_timer.time_until() < 1000)
        {
            vp->vai.checkpoint_timer.set(1000);
        }
    }
    if (ai->action_substate != 0)
    {
        if (!np->char_inst->is_playing_action(ANIMATION_ACTION_GUARD_INSPECT))
        {
            return AI_RESULT_SUCCESS;
        }
        if (ai->action_substate < 2 && vanim_has_trigger_just_elapsed(np->char_inst->vp, Trigger_impact_start, ANIMATION_ACTION_GUARD_INSPECT, -1))
        {
            vp->vai.checkpoint_timer.set(0);
            ai->action_substate = 2;
        }
        return AI_RESULT_IN_PROGRESS;
    }
    if (ai->action_timer.elapsed())
    {
        vp->vai.checkpoint_timer.set(0);
        ai->planner.avoid_action(this, 10000);
        return AI_RESULT_FAILURE;
    }

    // Goes to the side of the vehicle it stands on, 1.5 m out from the driver's door, and faces in.
    vector seat_pos;
    matrix seat_orient;
    vehicle_interaction_get_seat_pos_orient(vp, VSI_DRIVER, seat_pos, &seat_orient);
    vector seat_local = seat_pos;
    vec_change_to_local_coords(seat_local, vp->pos, vp->orient);
    vector bmin;
    vector bmax;
    vp->get_local_bbox(bmin, bmax);
    vector to_np;
    to_np.x = np->pos.x - vp->pos.x;
    to_np.y = np->pos.y - vp->pos.y;
    to_np.z = np->pos.z - vp->pos.z;
    float side;
    vector face;
    if (to_np * vp->orient.rvec <= 0.0f)
    {
        face = vp->orient.rvec;
        side = -((seat_local.x - bmin.x) + 1.5f);
    }
    else
    {
        side = (bmax.x - seat_local.x) + 1.5f;
        face.x = -vp->orient.rvec.x;
        face.y = -vp->orient.rvec.y;
        face.z = -vp->orient.rvec.z;
    }
    vector stand_pos;
    stand_pos.x = seat_pos.x + vp->orient.rvec.x * side;
    stand_pos.y = seat_pos.y + vp->orient.rvec.y * side;
    stand_pos.z = seat_pos.z + vp->orient.rvec.z * side;
    if (vec_xz_dist_squared(np->pos, stand_pos) < 0.25f)
    {
        ai->stop_moving();
        np->steering_vector = face.copy_flatten_normalize();
        np->char_inst->play_action(ANIMATION_ACTION_GUARD_INSPECT, ANIM_ALIAS_NONE, 1.0f, 0x20);
        ai->action_substate = 1;
    }
    else if (!ai->is_pathing_to(stand_pos, 0.25f, 1.0f))
    {
        ai->pathfind_to(stand_pos, HMS_WALK, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00638D50 (:2227)
ai_results guard_examine_vehicle_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this && np->char_inst->rdd.is_loaded())
    {
        np->char_inst->stop_action(ANIMATION_ACTION_GUARD_INSPECT, 5, false);
    }
    return AI_RESULT_SUCCESS;
}

// The singletons, in rfg.exe's construction order: they fill action_pool, and that order breaks ties in the planner.
go_to_guard_post_action go_to_guard_post_action::the_instance; // 0x01DDB8A0

// 0x006271A0 (.h:13)
go_to_guard_post_action::go_to_guard_post_action()
    : ai_action("go to guard post", ai_plan_conditions_from_bits({4, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({36}))
{
    flags.has_enter = 1;
}

ai_action* go_to_guard_post_action::get_instance()
{
    return &the_instance;
}

// 0x005E6880
go_to_guard_post_action::~go_to_guard_post_action()
{
}

// 0x006274A0 (.h:13)
int go_to_guard_post_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

guard_at_post_action guard_at_post_action::the_instance; // 0x01DC98FC

// 0x006278F0 (.h:22)
guard_at_post_action::guard_at_post_action()
    : ai_action("guard at post", ai_plan_conditions_from_bits({4, 9, 36, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({31}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* guard_at_post_action::get_instance()
{
    return &the_instance;
}

// 0x005E68B0
guard_at_post_action::~guard_at_post_action()
{
}

// 0x00627C40 (.h:22)
int guard_at_post_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

default_guard_action default_guard_action::the_instance; // 0x01D75474

// 0x006274E0 (.h:16)
default_guard_action::default_guard_action()
    : ai_action("default guard", ai_plan_conditions_from_bits({4, 9, 36, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({37}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* default_guard_action::get_instance()
{
    return &the_instance;
}

// 0x005E6890
default_guard_action::~default_guard_action()
{
}

// 0x00627830 (.h:16)
int default_guard_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

guard_from_turret_action guard_from_turret_action::the_instance; // 0x01DDB528

// 0x00627860 (.h:19)
guard_from_turret_action::guard_from_turret_action()
    : ai_action("guard from turret", ai_plan_conditions_from_bits({144}), ai_plan_conditions_from_bits({31}))
{
    flags.has_enter = 1;
}

ai_action* guard_from_turret_action::get_instance()
{
    return &the_instance;
}

// 0x005E68A0
guard_from_turret_action::~guard_from_turret_action()
{
}

// 0x006278C0 (.h:19)
int guard_from_turret_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

guard_at_post_scan_action guard_at_post_scan_action::the_instance; // 0x01DC9548

// 0x00627C70 (.h:23)
guard_at_post_scan_action::guard_at_post_scan_action()
    : ai_action("guard at post scan", ai_plan_conditions_from_bits({4, 9, 36, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({31}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* guard_at_post_scan_action::get_instance()
{
    return &the_instance;
}

// 0x005E68D0
guard_at_post_scan_action::~guard_at_post_scan_action()
{
}

// 0x00627FC0 (.h:23)
int guard_at_post_scan_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

guard_at_post_animate_action guard_at_post_animate_action::the_instance; // 0x01DDB654

// 0x00628000 (.h:24)
guard_at_post_animate_action::guard_at_post_animate_action()
    : ai_action("guard at post animate", ai_plan_conditions_from_bits({4, 9, 36, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({31}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* guard_at_post_animate_action::get_instance()
{
    return &the_instance;
}

// 0x005E68F0
guard_at_post_animate_action::~guard_at_post_animate_action()
{
}

// 0x00628350 (.h:24)
int guard_at_post_animate_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

guard_from_vehicle_action guard_from_vehicle_action::the_instance; // 0x01DDBA6C

// 0x00628380 (.h:27)
guard_from_vehicle_action::guard_from_vehicle_action()
    : ai_action("guard from vehicle", ai_plan_conditions_from_bits({9, 49, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({31}))
{
}

ai_action* guard_from_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005E6900
guard_from_vehicle_action::~guard_from_vehicle_action()
{
}

// 0x00628620 (.h:27)
int guard_from_vehicle_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

guard_examine_vehicle_action guard_examine_vehicle_action::the_instance; // 0x01DCA1E0

// 0x00628650 (.h:30)
guard_examine_vehicle_action::guard_examine_vehicle_action()
    : ai_action("guard examine vehicle", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({31}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* guard_examine_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005E6920
guard_examine_vehicle_action::~guard_examine_vehicle_action()
{
}

// 0x006288F0 (.h:30)
int guard_examine_vehicle_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

default_guard_rule default_guard_rule::the_instance; // 0x01DC9680

// 0x00628920 (.h:33)
default_guard_rule::default_guard_rule()
    : ai_production_rule("default guard rule", ai_plan_conditions_from_bits({37}), ai_plan_conditions_from_bits({31}))
{
}

ai_action* default_guard_rule::get_instance()
{
    return &the_instance;
}

// 0x005E6930
default_guard_rule::~default_guard_rule()
{
}

// 0x006289A0 (.h:33)
int default_guard_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

roadblock_vehicle_rule roadblock_vehicle_rule::the_instance; // 0x01D686E0

// 0x006289D0 (.h:36)
roadblock_vehicle_rule::roadblock_vehicle_rule()
    : ai_production_rule("roadblock vehicle rule", ai_plan_conditions_from_bits({49}), ai_plan_conditions_from_bits({32}))
{
}

ai_action* roadblock_vehicle_rule::get_instance()
{
    return &the_instance;
}

// 0x005E6940
roadblock_vehicle_rule::~roadblock_vehicle_rule()
{
}

// 0x00628A50 (.h:36)
int roadblock_vehicle_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

roadblock_guard_rule roadblock_guard_rule::the_instance; // 0x01D680C0

// 0x00628A80 (.h:37)
roadblock_guard_rule::roadblock_guard_rule()
    : ai_production_rule("roadblock guard rule", ai_plan_conditions_from_bits({31}), ai_plan_conditions_from_bits({32}))
{
}

ai_action* roadblock_guard_rule::get_instance()
{
    return &the_instance;
}

// 0x005E6950
roadblock_guard_rule::~roadblock_guard_rule()
{
}

// 0x00628B00 (.h:37)
int roadblock_guard_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

