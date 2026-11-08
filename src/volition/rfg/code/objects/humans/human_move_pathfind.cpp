// Original: volition/rfg/code/objects/humans/human_move_pathfind.cpp
//
// Following a path: walking from node to node with corners cut where the next node is in sight, or steering (on a
// mover). A walk stands in front of a busy ladder and mounts a free one; a steer also jumps where a node says so.

#include "volition/rfg/code/objects/humans/human_move.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/ladder.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/pathfinding/nav_cell_detour.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/matrix.h"

#include <math.h>

static const float TWO_PI = 6.2831855f;

// 0x00A0B440 (:46). LTCG: pf ESI.
static void reset_pf_timers(human_path_data* pf)
{
    pf->shortcut_time.set(-1);
    pf->dest_in_hull_time.set(-1);
    pf->detour_hull_check_timer.set(-1);
}

// The ladder right in front of the human, if any.
static ladder* find_ladder_ahead(const human* hp)
{
    const vector ahead(hp->pos.x + hp->orient.fvec.x, hp->pos.y + hp->orient.fvec.y, hp->pos.z + hp->orient.fvec.z);
    unsigned int handle;
    if (World.collect_objects<ladder>(ahead, 1.0f, &handle, 1, nullptr, nullptr) == 0)
    {
        return nullptr;
    }
    return handle != 0xFFFFFFFF ? World.object_from_handle<ladder>(handle) : nullptr;
}

// 0x00A0B480 (:63). LTCG: lp EDI. The next node must be nearer the ladder's far end than the human is to that node.
static bool human_should_mount_ladder(human* hp, ladder* lp)
{
    const path_node* node = hp->pf.path;
    const path_node* next = node->next;
    if (next == node || next == nullptr)
    {
        return false;
    }
    const float dist_to_next = fabsf(next->pos.y - hp->pos.y);
    vector local_min;
    vector local_max;
    lp->get_local_bbox(local_min, local_max);
    const float end = hp->pf.flags.climbing_up ? local_max.y : local_min.y;
    return fabsf(next->pos.y - (lp->pos.y + end)) < dist_to_next;
}

// 0x00A0B550 (:147). LTCG: hp ESI. Every 150 ms: the next node being in sight makes the current one unnecessary.
static bool human_move_visibility_process(human* hp, bool* no_start_nav_cell)
{
    if (no_start_nav_cell != nullptr)
    {
        *no_start_nav_cell = false;
    }
    if (hp->hflags.on_mover || !hp->pf.shortcut_time.elapsed())
    {
        return false;
    }
    hp->pf.shortcut_time.set(150);
    path_node* node = hp->pf.path;
    if (node == nullptr || node->next == node || node->next == nullptr)
    {
        return false;
    }
    const path_node* next = node->next;
    const compound_hull* hull = nav_cell_detour_get_compound_hull(node->compound_hull_handle);
    if (hull != nullptr && nav_cell_detour_is_point_in_compound_hull(hp->pos, hull))
    {
        return false;
    }
    if ((next->nav_cell_handle != 0xFFFFFFFFFFFFFFFFull || next->navpoint_handle == 0xFFFFFFFF) && bb_pf_is_point_visible(hp->pos, next->pos, hp->pf.last_visited_nav_cell_handle, next->nav_cell_handle, false, no_start_nav_cell))
    {
        hp->pf.last_cmp_hull = node->compound_hull_handle;
        rfg_pf_advance_to_next_path_node(&hp->pf.path, hp->pf.path);
        hp->pf.flags.smoothing = 0;
        if (hp->pf.path != nullptr && !hp->hflags.disable_all_path_smoothing)
        {
            hp->pf.flags.smoothing = rfg_pf_calculate_smoothing_info(hp);
        }
        return true;
    }
    return false;
}

// 0x00A0B670 (:212). A temporary node at the human's position, from which detours around new hulls are added.
static void human_move_check_detour_hulls(human* hp)
{
    if (hp == nullptr || hp->pf.path == nullptr)
    {
        return;
    }
    const nav_cell* cell = nav_cell_from_handle(hp->pf.last_visited_nav_cell_handle);
    vector pos = hp->pos;
    if (cell == nullptr || !nav_cell_is_point_in_cell_column(cell, pos))
    {
        uint64_t handle;
        if (bb_pf_get_nav_cell_from_hint(&pos, hp->pf.last_visited_nav_cell_handle, hp->pos, false, &handle, true, false, NAV_CELL_TRAVERSAL_ENABLED) == nullptr)
        {
            return;
        }
        hp->pf.last_visited_nav_cell_handle = handle;
    }
    hp->pf.last_visited_nav_cell_update_timer.set_rand(500, 1000);
    path_node* node = bb_pf_get_free_path_node(pos, hp->pf.last_visited_nav_cell_handle, 0xFFFFFFFF);
    if (node == nullptr)
    {
        return;
    }
    path_node*& path = hp->pf.path;
    if (path == nullptr)
    {
        node->next = node;
        node->prev = node;
    }
    else
    {
        path->prev->next = node;
        node->prev = path->prev;
        path->prev = node;
        node->next = path;
    }
    path = node;
    // Magic number: 2, the flag of a stuck human. Its name is not recovered.
    if (!nav_cell_detour_add_detour_points_to_path(hp, hp->hflags.stuck ? 2 : 0, 15.0f))
    {
        bb_pf_free_path_node(&hp->pf.path, hp->pf.path);
    }
}

// 0x00A0B7D0 (:259). Every 200 ms: the node lies in a hull other than its own, and the human is within 2 m of that hull.
static bool human_move_is_dest_in_hull(human* hp)
{
    if (!hp->pf.dest_in_hull_time.elapsed())
    {
        return false;
    }
    hp->pf.dest_in_hull_time.set(200);
    const path_node* node = hp->pf.path;
    if (node == nullptr)
    {
        return false;
    }
    vector adjusted_pos = ZERO_VECTOR;
    const nav_cell* cell = bb_pf_get_nav_cell_from_hint(&adjusted_pos, node->nav_cell_handle, node->pos, false, nullptr, false, false, NAV_CELL_TRAVERSAL_ENABLED);
    if (cell == nullptr)
    {
        return false;
    }
    const compound_hull* hull = nav_cell_detour_is_point_in_compound_hull(node->pos, cell);
    if (hull == nullptr || nav_cell_detour_get_compound_hull(node->compound_hull_handle) == hull)
    {
        return false;
    }
    vector2 closest;
    int closest_edge;
    nav_cell_detour_get_closest_point_on_compound_hull(&closest, &closest_edge, hp->pos, hull);
    const vector closest_pos(closest.x, hp->pos.y, closest.y);
    return vec_xz_dist_squared(hp->pos, closest_pos) <= 4.0f;
}

// The node is too close to a repulsor to stop at.
static bool node_in_repulsor(const path_node* node, float distance_left, const compound_repulsor* repulsors, int num_repulsors)
{
    for (int i = 0; i < num_repulsors; i++)
    {
        const float radius = repulsors[i].radius;
        if (vec_xz_dist_squared(repulsors[i].pos, node->pos) < radius * radius && distance_left < radius + 1.0f)
        {
            return true;
        }
    }
    return false;
}

// The ladder part of both process functions. Answers true when the caller must return, with the answer in result.
static bool process_ladder_node(human* hp, const path_node* node, bool& result)
{
    ladder* lp = find_ladder_ahead(hp);
    if (lp == nullptr)
    {
        return false;
    }
    const path_node* next = node->next != hp->pf.path ? node->next : nullptr;
    const bool climbing_up = next == nullptr || node->pos.y <= next->pos.y;
    if (!lp->is_enabled() && climbing_up)
    {
        result = false;
        return true;
    }
    if (0 < lp->get_use_count())
    {
        hp->steering_vector = ZERO_VECTOR;
        result = true;
        return true;
    }
    hp->pf.flags.climbing_up = climbing_up;
    if (human_should_mount_ladder(hp, lp))
    {
        human_mount_ladder(hp, lp);
        if (hp->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            static_cast<npc*>(hp)->get_ai()->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        }
    }
    return false;
}

// 0x00AC85E0 (:312). LTCG: hp ECX.
static bool human_move_walk_process(human* hp, vector& dest, vector& dir, float distance_left)
{
    human_path_data& pf = hp->pf;
    if (hp->hflags.is_on_ladder)
    {
        if (pf.path == nullptr)
        {
            return false;
        }
        ladder_process_human(hp, pf.flags.climbing_up ? 1.0f : -1.0f);
        return true;
    }
    // A ladder within the next three nodes that is taken makes the human wait.
    const path_node* look = pf.path;
    for (int i = 0; i < 3 && look != nullptr; i++)
    {
        if (look->flags.ladder)
        {
            ladder* lp = find_ladder_ahead(hp);
            if (lp != nullptr && 0 < lp->get_use_count())
            {
                hp->steering_vector = ZERO_VECTOR;
                hp->move_speed = -1.0f;
                hp->cur_state = HS_STANDARD;
                hp->cur_movement_state = HMS_IDLE;
                return true;
            }
            break;
        }
        look = look->next;
        if (look == pf.path)
        {
            break;
        }
    }
    if (hp->char_inst->is_playing_action(ANIM_ALIAS_TURNING) && !vanim_is_action_ending_this_frame(hp->char_inst->vp, ANIM_ALIAS_TURNING))
    {
        return true;
    }
    float arrive_dist = hp->actual_velocity_mag * 0.08f;
    if (arrive_dist - 0.3f < 0.0f)
    {
        arrive_dist = 0.3f;
    }
    bool no_start_nav_cell = false;
    const bool shortcut = human_move_visibility_process(hp, &no_start_nav_cell);
    if (shortcut || pf.flags.smoothing)
    {
        if (pf.flags.smoothing)
        {
            rfg_pf_get_smoothing_position(hp, dest);
        }
        else
        {
            dest = pf.path->pos;
        }
        dir = vector(dest.x - hp->pos.x, 0.0f, dest.z - hp->pos.z);
        distance_left = dir.mag_and_normalize_safe();
    }
    if (pf.path == nullptr)
    {
        return false;
    }
    if (pf.detour_hull_check_timer.elapsed())
    {
        human_move_check_detour_hulls(hp);
        pf.detour_hull_check_timer.set_rand(150, 300);
    }
    int num_repulsors = 0;
    compound_repulsor repulsors[8];
    hp->turnto_target = bb_pf_calculate_turn_to_target(hp, dest, num_repulsors, repulsors, !no_start_nav_cell);
    const bool turned = human_move_turn_to_target(hp, hp->turnto_target);
    if (pf.path == nullptr)
    {
        return false;
    }
    if (!turned && hp->actual_velocity_mag < 0.01f)
    {
        return true;
    }
    if (vec_xz_dist_squared(hp->turnto_target, dest) <= 0.001f || vec_xz_dist_squared(hp->turnto_target, hp->pos) <= 0.001f)
    {
        hp->steering_vector = dir;
    }
    else
    {
        vector steer(hp->turnto_target.x - hp->pos.x, 0.0f, hp->turnto_target.z - hp->pos.z);
        steer.normalize();
        hp->steering_vector = steer;
    }
    if (pf.flags.smoothing)
    {
        // How far around the circle from where the curve started; the curve ends at its point or its length.
        float travelled = 0.0f;
        if (0.04f < vec_xz_dist_squared(pf.smooth_start_pos, hp->pos))
        {
            vector radial(hp->pos.x - pf.center_of_circle.x, 0.0f, hp->pos.z - pf.center_of_circle.z);
            float cos_angle = (radial.x * (pf.smooth_start_pos.x - pf.center_of_circle.x) + 0.0f + radial.z * (pf.smooth_start_pos.z - pf.center_of_circle.z)) / (pf.movement_radius * pf.movement_radius);
            if (cos_angle - -1.0f < 0.0f)
            {
                cos_angle = -1.0f;
            }
            if (0.0f <= cos_angle - 1.0f)
            {
                cos_angle = 1.0f;
            }
            radial.normalize_safe();
            float angle = fl_acos(cos_angle);
            if (pf.smooth_start_fvec.y * radial.y + pf.smooth_start_fvec.x * radial.x + pf.smooth_start_fvec.z * radial.z < 0.0f)
            {
                angle = TWO_PI - angle;
            }
            travelled = angle * pf.movement_radius;
        }
        if (vec_xz_dist_squared(hp->pos, pf.smoothPoint) < 0.01f || pf.curve_distance < travelled)
        {
            pf.flags.smoothing = 0;
        }
        return true;
    }
    bool advanced = false;
    while (true)
    {
        path_node* node = pf.path;
        bool in_repulsor = pf.affected_by_repulsors && distance_left < 5.0f && node_in_repulsor(node, distance_left, repulsors, num_repulsors);
        if (human_move_is_dest_in_hull(hp))
        {
            in_repulsor = true;
        }
        if ((node->normal.y * dir.y + dir.x * node->normal.x + node->normal.z * dir.z <= 0.0f || arrive_dist < distance_left) && 0.1f < distance_left && (!in_repulsor || node == pf.path->prev))
        {
            const uint64_t handle = pf.path->nav_cell_handle;
            if (pf.path->distance_left + distance_left <= 0.0f)
            {
                bb_pf_free_path(&pf.path);
                return handle == 0xFFFFFFFFFFFFFFFFull && human_pathfind_to(hp, pf.eventual_goal_pos, HS_CURRENT, HMS_CURRENT, 0xFFFFFFFFFFFFFFFFull, false);
            }
            if (advanced && !hp->hflags.disable_all_path_smoothing)
            {
                pf.flags.smoothing = rfg_pf_calculate_smoothing_info(hp);
            }
            return true;
        }
        if (node->flags.ladder)
        {
            bool result;
            if (process_ladder_node(hp, node, result))
            {
                return result;
            }
        }
        hp->hflags.allow_steep_slopes = node->flags.stairs ? 1 : 0;
        const uint64_t handle = node->nav_cell_handle;
        pf.last_cmp_hull = node->compound_hull_handle;
        rfg_pf_advance_to_next_path_node(&pf.path, node);
        advanced = true;
        if (pf.path == nullptr)
        {
            if (in_repulsor)
            {
                hp->hflags.last_dest_in_repulsor = 1;
            }
            return handle == 0xFFFFFFFFFFFFFFFFull && human_pathfind_to(hp, pf.eventual_goal_pos, HS_CURRENT, HMS_CURRENT, 0xFFFFFFFFFFFFFFFFull, false);
        }
        dir = vector(pf.path->pos.x - hp->pos.x, 0.0f, pf.path->pos.z - hp->pos.z);
        distance_left = dir.mag_and_normalize_safe();
    }
}

// 0x00AC9160 (:592). LTCG: hp ESI.
static bool human_move_steer_process(human* hp, vector& dest, vector& dir, float distance_left)
{
    human_path_data& pf = hp->pf;
    if (hp->hflags.is_on_ladder)
    {
        if (pf.path == nullptr)
        {
            return false;
        }
        ladder_process_human(hp, pf.flags.climbing_up ? 1.0f : -1.0f);
        return true;
    }
    if (hp->update_timer.elapsed() || !hp->update_timer.valid())
    {
        hp->update_timer.set_rand(450, 550);
    }
    if (pf.path == nullptr)
    {
        return hp->hflags.recalculate_at_node && human_pathfind_to(hp, pf.eventual_goal_pos, HS_CURRENT, HMS_CURRENT, 0xFFFFFFFFFFFFFFFFull, true);
    }
    int num_repulsors = 0;
    compound_repulsor repulsors[8];
    hp->turnto_target = bb_pf_calculate_turn_to_target(hp, dest, num_repulsors, repulsors, true);
    const bool turned = human_move_turn_to_target(hp, hp->turnto_target);
    if (pf.path == nullptr)
    {
        return false;
    }
    if (!turned && hp->actual_velocity_mag < 0.01f)
    {
        return true;
    }
    if (vec_xz_dist_squared(hp->turnto_target, dest) <= 0.001f || vec_xz_dist_squared(hp->turnto_target, hp->pos) <= 0.001f)
    {
        hp->steering_vector = dir;
    }
    else
    {
        vector steer(hp->turnto_target.x - hp->pos.x, 0.0f, hp->turnto_target.z - hp->pos.z);
        steer.normalize();
        hp->steering_vector = steer;
    }
    const float arrive_dist = hp->actual_velocity_mag * 0.08f;
    while (true)
    {
        path_node* node = pf.path;
        const bool in_repulsor = !node->flags.jumping && pf.affected_by_repulsors && distance_left < 5.0f && node_in_repulsor(node, distance_left, repulsors, num_repulsors);
        const bool at_goal = node->pos.x == pf.eventual_goal_pos.x && node->pos.y == pf.eventual_goal_pos.y && node->pos.z == pf.eventual_goal_pos.z;
        if ((node->normal.y * dir.y + node->normal.x * dir.x + node->normal.z * dir.z <= 0.0f || arrive_dist < distance_left) && 0.3f < distance_left && (!in_repulsor || at_goal))
        {
            if (0.0f < pf.path->distance_left + distance_left)
            {
                return true;
            }
            if (hp->hflags.recalculate_at_node)
            {
                if (pf.path->nav_cell_handle != 0xFFFFFFFFFFFFFFFFull)
                {
                    pf.last_visited_nav_cell_handle = pf.path->nav_cell_handle;
                }
                if (human_pathfind_to(hp, pf.eventual_goal_pos, HS_CURRENT, HMS_CURRENT, 0xFFFFFFFFFFFFFFFFull, true))
                {
                    return true;
                }
            }
            bb_pf_free_path(&pf.path);
            return false;
        }
        if (node->flags.jumping || node->flags.leap_over)
        {
            matrix start_orient(hp->orient);
            const path_node* next = node->next;
            if (next != pf.path && next != nullptr)
            {
                vector flat(next->pos.x - node->pos.x, 0.0f, next->pos.z - node->pos.z);
                flat.normalize_safe();
                start_orient.make_quick(flat);
                hp->steering_vector = flat;
            }
            human_jump(hp, node->flags.jumping, node->flags.leap_over, false);
            hp->char_inst->set_action_starting_orient(hp->pos, hp->orient, hp->pos, start_orient, ANIM_ALIAS_SPECIAL_JUMP, 0.3f);
        }
        if (node->flags.ladder)
        {
            bool result;
            if (process_ladder_node(hp, node, result))
            {
                return result;
            }
        }
        hp->hflags.allow_steep_slopes = node->flags.stairs ? 1 : 0;
        pf.last_cmp_hull = 0xFFFFFFFF;
        const uint64_t handle = node->nav_cell_handle;
        rfg_pf_advance_to_next_path_node(&pf.path, node);
        if (pf.path == nullptr)
        {
            if (in_repulsor)
            {
                hp->hflags.last_dest_in_repulsor = 1;
            }
            if (!hp->hflags.recalculate_at_node)
            {
                return false;
            }
            if (handle != 0xFFFFFFFFFFFFFFFFull)
            {
                pf.last_visited_nav_cell_handle = handle;
            }
            return human_pathfind_to(hp, pf.eventual_goal_pos, HS_CURRENT, HMS_CURRENT, 0xFFFFFFFFFFFFFFFFull, false);
        }
        dir = vector(pf.path->pos.x - hp->pos.x, 0.0f, pf.path->pos.z - hp->pos.z);
        distance_left = dir.mag_and_normalize_safe();
        dest = pf.path->pos;
    }
}

// 0x00AC9960 (:851)
bool human_move_pathfind(human* hp)
{
    human_path_data& pf = hp->pf;
    if (pf.path == nullptr)
    {
        if (hp->hflags.is_on_ladder)
        {
            human_dismount_ladder(hp);
        }
        return false;
    }
    if (hp->do_avoidance_pause_frame())
    {
        return true;
    }
    vector dest = pf.path->pos;
    vector dir(dest.x - hp->pos.x, 0.0f, dest.z - hp->pos.z);
    const float distance_left = dir.mag_and_normalize_safe();
    switch (pf.action)
    {
    case HPA_NORMAL:
        return human_move_walk_process(hp, dest, dir, distance_left);
    case HPA_STARTUP:
        hp->turnto_target = dest;
        pf.action = HPA_NORMAL;
        if (!hp->hflags.on_mover)
        {
            reset_pf_timers(&pf);
            return human_move_walk_process(hp, dest, dir, distance_left);
        }
        pf.action = HPA_STEER;
        return human_move_steer_process(hp, dest, dir, distance_left);
    case HPA_STEER:
        return human_move_steer_process(hp, dest, dir, distance_left);
    default:
        return true;
    }
}
