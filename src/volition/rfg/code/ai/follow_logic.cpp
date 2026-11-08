// Original: volition/rfg/code/ai/follow_logic.cpp
//
// Keeps a follower in its formation slot. Rigid formations walk straight to the slot; the others match the slot's direction
// and speed while near it, and blend towards it otherwise.

#include "volition/rfg/code/ai/follow_logic.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/formation.h"
#include "volition/rfg/code/ai/formation_internal.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_convoy.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <float.h>
#include <math.h>

static float MIN_DIST; // 0x01DDBBC8, .bss, never written
static float MAX_DIST = 10.0f; // 0x01647FDC, .data, never written
static float MIN_SPEED; // 0x01DDBBCC, .bss, never written
static float MAX_SPEED = 10.0f; // 0x01647FE0, .data, never written
static float MAX_DELTA_SPEED_PER_SECOND = 6.0f; // 0x0164275C, .data, never written
static float BBOX_FUDGE_FACTOR = 1.5f; // 0x01647FE4, .data, never written

static const float COS_75_DEGREES = 0.25881904f;

// 0x005E93F0 (:29)
follow_parameters::follow_parameters()
{
    desired_speed = 0.0f;
    cur_speed = 0.0f;
}

static float dot(const vector& a, const vector& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// The share of the slot's direction per follow mode, in both direct movement functions.
static float follow_mode_blend_limit(ai_formation_follow_modes mode)
{
    switch (mode)
    {
    case AI_FFM_FOLLOW_THE_LEADER:
    case AI_FFM_FOLLOW_THE_LEADER_MUST_PATH:
        return 0.7f;
    case AI_FFM_FLOCK:
        return 0.5f;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        return 0.3f;
    case AI_FFM_NORMAL:
        return 0.3f;
    }
}

// 0x0065DCB0 (:282). LTCG: np ESI.
static bool follow_get_direct_movement(follow_parameters& params, const npc* np, const follow_information* follow_info)
{
    const ai_system* ai = np->get_ai();
    const bool moving = np->cur_movement_state != HMS_IDLE && !np->steering_vector.is_null();
    vector cur_dir = moving ? np->steering_vector : np->orient.fvec;
    cur_dir.flatten_normalize_safe();
    if (moving)
    {
        if (np->move_speed <= 0.0f)
        {
            const human_move_speed_info* info = human_get_move_speed_info(np, np->cur_movement_state);
            if (info == nullptr)
            {
                return false;
            }
            params.cur_speed = info->average_speed;
        }
        else
        {
            params.cur_speed = np->move_speed;
        }
    }

    vector to_slot;
    to_slot.x = follow_info->formation_pos.x - np->pos.x;
    to_slot.y = follow_info->formation_pos.y - np->pos.y;
    to_slot.z = follow_info->formation_pos.z - np->pos.z;
    const float dist = to_slot.flatten_mag_and_normalize_safe();
    const bool at_slot = fabsf(dist) < 0.15f;
    const vector slot_dir = follow_info->formation_dir.copy_flatten_normalize_safe();
    const float slot_speed = follow_info->formation_speed;

    if (ai_formation_is_rigid(ai->get_formation_handle()))
    {
        params.desired_dir = to_slot;
        if (np->cur_movement_state == HMS_IDLE && dist <= 1.5f)
        {
            return true;
        }
        params.desired_speed = fl_parameter(dist, MIN_DIST, MAX_DIST, MIN_SPEED, MAX_SPEED, true);
        return true;
    }

    bool stop = false;
    if (slot_speed > 1.0f)
    {
        if (at_slot)
        {
            if (dot(slot_dir, cur_dir) < COS_75_DEGREES)
            {
                stop = true;
            }
            else
            {
                params.desired_dir = slot_dir;
                params.desired_speed = 0.0f - (slot_speed - 0.2f) < 0.0f ? slot_speed - 0.2f : 0.0f;
                goto check_too_slow;
            }
        }
        else
        {
            stop = dot(slot_dir, to_slot) < -COS_75_DEGREES && dot(slot_dir, cur_dir) < 0.5f;
        }
    }
    else
    {
        stop = at_slot;
    }
    if (stop)
    {
        params.desired_speed = 0.0f;
        params.desired_dir = cur_dir;
        return true;
    }

    {
        float along = dot(slot_dir, to_slot);
        if (object_is_player(follow_info->my_leader))
        {
            along += 1.0f; // keeps up: a player's followers don't slow down for a slot behind them
        }
        const float speed = (along * 0.5f * dist + slot_speed) - 0.2f;
        params.desired_speed = 0.0f <= 0.0f - speed ? 0.0f : speed;
        const float dist_share = fl_parameter(dist, 0.15f, 0.5f, 0.0f, follow_mode_blend_limit(follow_info->follow_mode), true);
        const float along_share = fl_parameter(along * 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, true);
        vec_interp_const_norm(params.desired_dir, slot_dir, to_slot, along_share * dist_share);
    }

check_too_slow:
    if ((ai->stop_following_timer.valid() && params.desired_speed < 0.8f) || params.desired_speed < 0.5f)
    {
        params.desired_speed = 0.0f;
        params.desired_dir = cur_dir;
    }
    return true;
}

// 0x0065E2A0 (:543). LTCG: np EAX.
static void follow_set_direct_movement(npc* np, const follow_parameters& params, human_stances stance)
{
    ai_system* ai = np->get_ai();
    const human_move_speed_info (&speeds)[4] = human_get_move_speed_block(np);
    if (np->movement_mode != HMM_NONE)
    {
        human_set_movement_mode(np, HMM_NONE, HMSM_NONE);
    }

    // The speed stays inside what the movement states can do and changes at most 6 m/s per second, and the movement
    // state follows it.
    const float accelerated = params.cur_speed + MAX_DELTA_SPEED_PER_SECOND * Frametime;
    float highest = speeds[3].max_speed - 0.05f;
    if (0.0f <= highest - accelerated)
    {
        highest = accelerated;
    }
    const float walking = speeds[0].max_speed + 0.05f;
    if (highest - walking < 0.0f)
    {
        highest = walking;
    }
    float decelerated = params.cur_speed - MAX_DELTA_SPEED_PER_SECOND * Frametime;
    if (decelerated - 0.0f < 0.0f)
    {
        decelerated = 0.0f;
    }
    float speed = speeds[3].min_speed - 0.05f;
    if (decelerated - speed < 0.0f)
    {
        speed = decelerated;
    }
    if (0.0f <= params.desired_speed - speed)
    {
        speed = params.desired_speed;
    }
    if (0.0f <= speed - highest)
    {
        speed = highest;
    }

    int m_state = np->cur_movement_state;
    while (speeds[m_state].max_speed < speed)
    {
        m_state++;
    }
    while (speed < speeds[m_state].min_speed)
    {
        m_state--;
    }

    if (m_state == HMS_IDLE)
    {
        ai->stop_moving();
        if (!ai_formation_is_rigid(ai->get_formation_handle()))
        {
            ai->stop_following_timer.set_rand(1000, 1500);
            np->pf.dest_check_timer.invalidate();
            return;
        }
    }
    else
    {
        ai->set_direct_movement_fast(params.desired_dir, (human_movement_states)m_state, stance, speed);
        if (np->pf.dest_check_timer.valid())
        {
            return;
        }
    }
    np->pf.dest_check_timer.set(0);
}

// 0x005E9400 (:649). LTCG: params EDI, vp ESI.
static bool vehicle_follow_get_direct_movement(follow_parameters& params, const vehicle* vp, const follow_information* follow_info)
{
    const vector forward = vp->orient.fvec.copy_flatten_normalize();
    vector velocity;
    vehicle_get_velocity(vp, velocity);
    params.cur_speed = dot(velocity, forward);

    vector to_slot;
    to_slot.x = follow_info->formation_pos.x - vp->pos.x;
    to_slot.y = follow_info->formation_pos.y - vp->pos.y;
    to_slot.z = follow_info->formation_pos.z - vp->pos.z;
    const float dist = to_slot.flatten_mag_and_normalize_safe();
    const bool at_slot = fabsf(dist) < 0.15f;
    const vector slot_dir = follow_info->formation_dir.copy_flatten_normalize_safe();
    const float slot_speed = follow_info->formation_speed;
    const float facing = dot(slot_dir, forward);
    const float along = dot(slot_dir, to_slot);

    bool stop = false;
    if (slot_speed <= 1.0f)
    {
        if (at_slot)
        {
            if (facing < 0.8660254f || follow_info->follow_mode == AI_FFM_FOLLOW_THE_LEADER || follow_info->follow_mode == AI_FFM_FOLLOW_THE_LEADER_MUST_PATH)
            {
                stop = true;
            }
            else
            {
                params.desired_dir = slot_dir;
                params.desired_speed = 0.0f - (slot_speed - 0.4f) < 0.0f ? slot_speed - 0.4f : 0.0f;
                return true;
            }
        }
    }
    else if (at_slot)
    {
        if (facing >= COS_75_DEGREES)
        {
            params.desired_dir.x = follow_info->my_leader->pos.x - vp->pos.x;
            params.desired_dir.y = follow_info->my_leader->pos.y - vp->pos.y;
            params.desired_dir.z = follow_info->my_leader->pos.z - vp->pos.z;
            params.desired_dir.normalize_safe();
            const float speed = slot_speed - 0.4f;
            params.desired_speed = 0.0f <= 0.0f - speed ? 0.0f : speed;
            return true;
        }
        stop = true;
    }
    else
    {
        stop = along < -COS_75_DEGREES && facing < 0.5f;
    }
    if (stop)
    {
        params.desired_speed = 0.0f;
        params.desired_dir = forward;
        return true;
    }

    const float speed = (along * dist * 0.5f + slot_speed) - 0.4f;
    params.desired_speed = 0.0f <= 0.0f - speed ? 0.0f : speed;
    const float dist_share = fl_parameter(dist, 0.15f, 0.5f, 0.0f, follow_mode_blend_limit(follow_info->follow_mode), true);
    const float along_share = fl_parameter(along, -1.0f, 0.0f, 0.0f, 1.0f, true);
    vec_interp_const_norm(params.desired_dir, slot_dir, to_slot, along_share * dist_share);
    return true;
}

// 0x006A0100 (:968)
bool ai_do_follow_frame(npc* np, unsigned int formation_handle, human_stances stance)
{
    ai_system* ai = np->get_ai();
    if (np->movement_mode == HMM_FALL || np->movement_mode == HMM_JUMP || np->movement_mode == HMM_KNOCKDOWN)
    {
        return true;
    }
    const follow_information* info = ai_formation_get_follower_position(formation_handle, np);
    if (info == nullptr)
    {
        return false;
    }
    if (info->follow_mode == AI_FFM_INVALID)
    {
        return true;
    }
    const object* leader = info->my_leader;
    if ((leader->get_obj_type() == OT_HUMAN && static_cast<const human*>(leader)->hflags.is_on_ladder != 0) || info->position_invalid)
    {
        ai->stop_moving();
        return true;
    }

    const object* formation_leader = ai_formation_get_leader(formation_handle);
    const vehicle* leader_vehicle = formation_leader != nullptr && formation_leader->get_obj_type() == OT_VEHICLE ? static_cast<const vehicle*>(formation_leader) : nullptr;
    const bool follows_player = object_is_player(formation_leader) || (leader_vehicle != nullptr && object_is_player(vehicle_passenger_in_seat(leader_vehicle, VSI_DRIVER)));
    const ai_pathfind_priorities priority = (ai_pathfind_priorities)(AI_PATHFIND_PRIORITY_NORMAL + (follows_player ? 1 : 0));

    const vector& slot = info->formation_pos;
    const float dist_sq = vec_dist_squared(slot, np->pos);
    bool path_to_slot = dist_sq > 25.0f;
    if (!path_to_slot && leader->get_obj_type() == OT_VEHICLE)
    {
        // Under the leading vehicle, which direct movement cannot get out of.
        vector bmin;
        vector bmax;
        leader->get_local_bbox(bmin, bmax);
        bmin.x -= BBOX_FUDGE_FACTOR;
        bmin.z -= BBOX_FUDGE_FACTOR;
        bmin.y = -FLT_MAX;
        bmax.x += BBOX_FUDGE_FACTOR;
        bmax.z += BBOX_FUDGE_FACTOR;
        bmax.y = FLT_MAX;
        vector local_pos = np->pos;
        vec_change_to_local_coords(local_pos, leader->pos, leader->orient);
        path_to_slot = ix_point_in_box(local_pos, bmin, bmax);
    }
    if (path_to_slot)
    {
        human_movement_states m_state = HMS_RUN;
        float move_speed = -1.0f;
        if (ai->get_convoy_object() != nullptr || follows_player)
        {
            move_speed = 8.0f;
            m_state = HMS_SPRINT;
        }
        const ai_results result = ai_pf_to_result(ai->pathfind_to(slot, m_state, stance, priority, 0xFFFFFFFFFFFFFFFFull, false));
        if (result == AI_RESULT_FAILURE)
        {
            return false;
        }
        if (result == AI_RESULT_SUCCESS)
        {
            np->move_speed = move_speed;
        }
        return true;
    }

    if (info->follow_mode == AI_FFM_FOLLOW_THE_LEADER_MUST_PATH)
    {
        if (dist_sq > 1.0f)
        {
            ai->pathfind_to(slot, dist_sq < 6.25f ? HMS_WALK : HMS_RUN, stance, priority, 0xFFFFFFFFFFFFFFFFull, false);
        }
        else
        {
            ai->stop_moving();
        }
        return true;
    }
    if (np->cur_movement_state == HMS_IDLE && ai->stop_following_timer.valid() && !ai->stop_following_timer.elapsed())
    {
        return true;
    }

    if (leader->get_obj_type() == OT_HUMAN)
    {
        const float height_diff = fabsf(leader->pos.y - np->pos.y);
        if (height_diff > 0.75f)
        {
            // Not at the leader's level: stay put unless both stand on the mesh and it is a small step.
            const nav_cell* ncp = nav_cell_from_handle(np->pf.last_visited_nav_cell_handle);
            float height;
            if (ncp == nullptr || (ncp->flags & NAV_CELL_FLAG_DISABLED) != 0 || !nav_cell_is_point_in_cell_column(ncp, np->pos) ||
                !havok_terrain_height(height, np->pos.x, np->pos.z, false, nullptr) || height + 1.25f < np->pos.y ||
                !ai->perception.is_on_navmesh(static_cast<const human*>(leader), nullptr, 1.25f) || height_diff > 2.0f)
            {
                ai->stop_moving();
                return true;
            }
        }
    }

    follow_parameters params;
    if (!follow_get_direct_movement(params, np, info))
    {
        return false;
    }
    nav_cell* cell = nullptr;
    if (ai->perception.is_on_navmesh(np, &cell, 1.25f))
    {
        vector ahead;
        ahead.x = params.desired_dir.x * 0.5f + np->pos.x;
        ahead.y = np->pos.y + params.desired_dir.y * 0.5f;
        ahead.z = np->pos.z + params.desired_dir.z * 0.5f;
        vector end_pos;
        const nav_cell* end_cell;
        int edge;
        if (!nav_cell_find_longest_line_of_sight_no_detour(end_pos, end_cell, np->pos, cell, ahead, false, &edge))
        {
            // Slide along the edge in the way, or stand when it is straight across.
            vector edge_dir;
            nav_cell_get_edge_dir(&edge_dir, end_cell, edge);
            const float along_edge = dot(edge_dir, params.desired_dir);
            if (fabsf(along_edge) < 0.1f)
            {
                ai->stop_moving();
                return true;
            }
            const float sign = fl_sign(along_edge);
            params.desired_dir.x = sign * edge_dir.x;
            params.desired_dir.y = edge_dir.y * sign;
            params.desired_dir.z = sign * edge_dir.z;
            params.desired_dir.flatten_normalize();
        }
    }

    if (np->cur_movement_state == HMS_IDLE)
    {
        if (params.desired_speed == 0.0f)
        {
            ai->stop_following_timer.invalidate();
        }
    }
    else if (params.desired_speed == 0.0f)
    {
        // Keeps moving for a moment before it stops.
        if (ai->stop_following_timer.valid())
        {
            if (!ai->stop_following_timer.elapsed())
            {
                params.desired_speed = params.cur_speed;
            }
        }
        else
        {
            ai->stop_following_timer.set_rand(350, 600);
            params.desired_speed = params.cur_speed;
        }
    }
    else
    {
        ai->stop_following_timer.invalidate();
    }
    follow_set_direct_movement(np, params, stance);
    return true;
}

// 0x006A07E0 (:1177)
bool ai_vehicle_do_follow_frame(npc* np, vehicle* vp, unsigned int formation_handle)
{
    ai_system* ai = np->get_ai();
    if (vehicle_passenger_in_seat(vp, VSI_DRIVER) != np)
    {
        if (vehicle_passenger_entering_seat(vp, VSI_DRIVER) == np)
        {
            return true;
        }
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        return false;
    }

    // In a convoy the squad holds the slot, otherwise the vehicle does.
    const object* follower = vp;
    object_convoy* convoy = ai->get_convoy_object();
    if (convoy != nullptr && convoy->in_convoy_formation(ai->get_squad_handle()))
    {
        follower = World.object_from_handle<object_squad>(ai->get_squad_handle());
    }
    if (follower == nullptr || formation_handle == 0xFFFFFFFF)
    {
        return false;
    }
    const follow_information* info = ai_formation_get_follower_position(formation_handle, follower);
    if (info == nullptr || info->follow_mode == AI_FFM_INVALID)
    {
        return false;
    }

    if (ai->is_in_player_squad(true) != nullptr)
    {
        const float dist = vec_dist(vp->pos, info->formation_pos);
        const bool far_behind = dist > 10.0f;
        vector to_slot;
        to_slot.x = info->formation_pos.x - vp->pos.x;
        to_slot.y = info->formation_pos.y - vp->pos.y;
        to_slot.z = info->formation_pos.z - vp->pos.z;
        float speed = info->formation_speed + dot(info->formation_dir, to_slot) * 0.5f;
        const float catch_up = (dist - 10.0f) * 0.5f;
        if (speed - catch_up < 0.0f)
        {
            speed = catch_up;
        }
        if (speed - 0.0f < 0.0f)
        {
            speed = 0.0f;
        }
        ai_vehicle_override_speed(vp, speed);
        // From the asm: the decompile merges this call with the one below. 4 m is the arrival distance.
        return ai_vehicle_travel_to_a_point_do_frame(np, vp, info->formation_pos, 0xFFFFFFFFFFFFFFFFull, false, 4.0f, far_behind, false, false, true) != AI_RESULT_FAILURE;
    }

    const float dist_sq = vec_dist_squared(info->formation_pos, vp->pos);
    vp->vai.vai_flags.m_follow_pathfinding = 1;
    follow_parameters params;
    vehicle_follow_get_direct_movement(params, vp, info);
    float speed = params.desired_speed;
    convoy = ai->get_convoy_object();
    if (dist_sq > 156.25f && convoy != nullptr) // 12.5 m behind: faster than the convoy
    {
        speed = convoy->get_move_speed() + 5.0f;
    }
    ai_vehicle_override_speed(vp, speed);
    if (speed <= 1.0f)
    {
        vehicle_set_controls_when_abandoned(vp);
        return true;
    }
    return ai_vehicle_travel_to_a_point_do_frame(np, vp, info->ideal_formation_pos, 0xFFFFFFFFFFFFFFFFull, convoy != nullptr, 5.0f, true, false, false, true) != AI_RESULT_FAILURE;
}
