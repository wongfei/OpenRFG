// Original: volition/rfg/code/ai/combat/combat_maneuver.cpp

#include "volition/rfg/code/ai/combat/combat_maneuver.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/belief_iterator.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_autoaim.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/pathfinding/nav_cell_detour.h"
#include "volition/rfg/code/pathfinding/nav_heap.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <math.h>

extern bool Navpoint_treat_all_as_enabled; // object_navpoint.cpp
extern nav_heap Nav_heap; // nav_heap.cpp

// 0x01D9F74C, cos(75 degrees) set by a dynamic initializer: the radians are 1.3089749813079834 at 0x012CF510.
static const float COS_MAX_RETREAT_ANGLE = cosf(1.3089749813079834f);
// 0x012C8680: the test directions of get_retreat_dir_internal, as parts of 75 degrees either side of the desired one.
static const float Retreat_test_angle_proportions[3] = {0.16666667f, 0.5f, 1.0f};
// 0x01642630 and 0x0164263C, .data: the box ahead of a chasing vehicle that a fleeing vehicle's retreat avoids.
static vector RETREAT_VEHICLE_AVOID_ENEMY_BMIN(-15.0f, -100.0f, 0.0f);
static vector RETREAT_VEHICLE_AVOID_ENEMY_BMAX(15.0f, 100.0f, 75.0f);

// 0x005E4470 (:188): a is on the maneuver, b on the line of fire. Past the end of the maneuver counts from its end.
static bool get_closest_point_on_maneuver_to_line_of_fire(ix_collision& a, ix_collision& b, const vector& start_pos, const vector& end_pos, const vector& muzzle_pos, const vector& furthest_pos)
{
    if (!ix_closest_line_line(a, b, start_pos, end_pos, muzzle_pos, furthest_pos) || a.fraction < 0.1f || b.fraction <= 0.0f || 1.0f < b.fraction)
    {
        return false;
    }
    if (1.0f < a.fraction)
    {
        a.fraction = 1.0f;
        a.hit_point = end_pos;
        b.fraction = ix_closest_point_on_line_segment(b.hit_point, a.hit_point, muzzle_pos, furthest_pos);
        const float length = vec_dist(muzzle_pos, furthest_pos);
        if (0.0f < length)
        {
            b.fraction = b.fraction / length;
        }
        else
        {
            b.fraction = 1.0f;
        }
    }
    return true;
}

// 0x006770F0 (:220): within 0.9 m of a manned turret's line of fire, or inside twice its spread. Up to 1.25 times the weapon's range.
static bool move_crosses_turret_fire(const npc* np, const vector& start_pos, const vector& end_pos)
{
    if (np == nullptr)
    {
        return false;
    }
    belief_obj_iterator<turret> it;
    it.init(np->get_ai(), AI_BELIEF_TURRET);
    for (; it; ++it)
    {
        turret* tp = it;
        if (!tp->is_functional(false))
        {
            continue;
        }
        const weapon* wp = tp->get_weapon_inst();
        if (wp == nullptr || wp->w_info == nullptr || human_combat_turret_get_operator(tp) == nullptr)
        {
            continue;
        }
        vector muzzle_pos;
        matrix muzzle_orient;
        wp->get_muzzle_transform(muzzle_pos, &muzzle_orient);
        const float max_spread = wp->w_info->max_spread;
        const vector furthest = muzzle_pos + muzzle_orient.fvec * (wp->w_info->max_range * 1.25f);
        ix_collision a;
        ix_collision b;
        if (get_closest_point_on_maneuver_to_line_of_fire(a, b, start_pos, end_pos, muzzle_pos, furthest))
        {
            // 0.81 is a double in rfg.exe, read as 0.809999942779541.
            if ((double)vec_dist_squared(a.hit_point, b.hit_point) < 0.809999942779541)
            {
                return true;
            }
            vector to_maneuver = a.hit_point - muzzle_pos;
            to_maneuver.normalize_safe();
            if (acosf(to_maneuver * muzzle_orient.fvec) <= max_spread * 2.0f)
            {
                return true;
            }
        }
    }
    return false;
}

// 0x00658BE0 (:292): the player the squad follows or defends has a firearm up, and fires within 1.5 m of the move.
static bool move_crosses_player_fire(const npc* np, vector start_pos, vector end_pos)
{
    const unsigned int squad_handle = np->get_ai()->get_squad_handle();
    if (squad_handle == 0xFFFFFFFF)
    {
        return false;
    }
    object_squad* squad = static_cast<object_squad*>(World.object_from_handle(squad_handle));
    if (squad == nullptr || squad->get_obj_type() != OT_SQUAD)
    {
        return false;
    }
    player* pl = squad->get_player();
    if (pl == nullptr)
    {
        const gfm_orders* orders = squad->get_squad_orders();
        if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_DEFEND)
        {
            return false;
        }
        pl = World.object_cast<player>(orders->get_target_object());
        if (pl == nullptr)
        {
            return false;
        }
    }
    if (!pl->combat.combat_ready_timestamp.valid() || pl->combat.combat_ready_timestamp.elapsed())
    {
        return false;
    }
    const weapon* wp = human_weapons_get_weapon(pl);
    if (wp == nullptr || !wp->w_info->is_firearm())
    {
        return false;
    }
    vector muzzle_pos;
    wp->get_muzzle_transform(muzzle_pos, nullptr);
    vector aim_dir;
    vec_normalized_dir(aim_dir, pl->aim_pos, muzzle_pos);
    const vector furthest = muzzle_pos + aim_dir * wp->w_info->max_engagement_dist;
    // 0.95: about chest height; the end point is not raised as well.
    start_pos.y += 0.95f;
    float s;
    float t;
    vector closest_a;
    vector closest_b;
    ix_closest_linesegment_linesegment(s, t, closest_a, closest_b, start_pos, end_pos, muzzle_pos, furthest);
    return 0.0f < s && 0.0f < t && vec_dist_squared(closest_a, closest_b) <= 2.25f;
}

// 0x005E4560 (:357): towards whichever end of the link is nearer the desired direction.
static bool get_retreat_dir_on_nav_link(vector& dir, vector& end_pos, const npc* np, const vector& desired_dir, const navpoint* npp1, const navpoint* npp2, const navpoint** end_npp)
{
    vector to_first = npp1->pos - np->pos;
    vector to_second = npp2->pos - np->pos;
    to_first.flatten_normalize();
    to_second.flatten_normalize();
    if (desired_dir * to_first <= desired_dir * to_second)
    {
        dir = to_second;
        end_pos = npp2->pos;
        if (end_npp != nullptr)
        {
            *end_npp = npp2;
        }
    }
    else
    {
        dir = to_first;
        end_pos = npp1->pos;
        if (end_npp != nullptr)
        {
            *end_npp = npp1;
        }
    }
    return true;
}

// 0x005E4730 (:403)
static bool get_retreat_dir_to_nav_link(vector& dir, vector& end_pos, const npc* np, const vector& closest_pos)
{
    dir = closest_pos - np->pos;
    dir.flatten_normalize();
    end_pos = closest_pos;
    return true;
}

// 0x005E47C0 (:430): the enabled link nearest the desired direction, within 75 degrees.
static bool get_retreat_dir_from_navpoint(vector& dir, vector& end_pos, const npc* np, const vector& desired_dir, const navpoint* npp, const navpoint** end_npp)
{
    if (end_npp != nullptr)
    {
        *end_npp = nullptr;
    }
    float best_dot = COS_MAX_RETREAT_ANGLE;
    vector best_dir = ZERO_VECTOR;
    const navpoint* best = nullptr;
    for (int i = 0; i < npp->num_links; i++)
    {
        const navpoint* link = npp->links[i];
        if ((!Navpoint_treat_all_as_enabled && npp->link_disabled_by[i] != 0) || link == nullptr || (!Navpoint_treat_all_as_enabled && link->heap_data.disabled_by != 0))
        {
            continue;
        }
        vector to_link = link->pos - np->pos;
        to_link.flatten_normalize_safe();
        const float dot = desired_dir.y * to_link.y + desired_dir.x * to_link.x + desired_dir.z * to_link.z;
        if (best_dot < dot)
        {
            best_dir = to_link;
            best = link;
            best_dot = dot;
        }
    }
    if (best == nullptr)
    {
        return false;
    }
    if (end_npp != nullptr)
    {
        *end_npp = best;
    }
    end_pos = best->pos;
    dir = best_dir;
    return true;
}

// 0x005E49B0 (:498): how far (squared) the nav mesh goes that way. The line starts a metre along it when that is on a cell.
static float get_retreat_dir_dist_sq(const nav_cell* ncp, const vector& start_point, const vector& desired_flee_dir, float max_dist, nav_cell** end_nav_cell)
{
    nav_cell_longest_los_input input;
    nav_cell_longest_los_output output;
    input.start_pos = start_point;
    input.delta_2d.x = desired_flee_dir.x * max_dist;
    input.delta_2d.y = desired_flee_dir.z * max_dist;
    vector ahead = start_point + desired_flee_dir;
    input.start_nav_cell = const_cast<nav_cell*>(ncp);
    nav_cell* ahead_cell = bb_pf_get_nav_cell_from_hint(&ahead, const_cast<nav_cell*>(ncp), ahead, false, nullptr, false, false, NAV_CELL_TRAVERSAL_ENABLED);
    if (ahead_cell != nullptr)
    {
        input.start_pos = ahead;
        input.start_nav_cell = ahead_cell;
    }
    input.flags.use_disabled_nav_cells = 0;
    input.flags.ignore_detour_hulls = 0;
    input.flags.check_start_in_detour_hull = 1;
    nav_cell_find_longest_line_of_sight_point(output, input);
    if (end_nav_cell != nullptr)
    {
        *end_nav_cell = output.end_nav_cell;
    }
    if (output.result == NAV_CELL_LONGEST_LOS_RESULT_REACHED_DESTINATION)
    {
        return max_dist * max_dist;
    }
    return vec_xz_dist_squared(start_point, output.end_pos);
}

// 0x005E4B70 (:582): the desired direction and three either side, with 5% of 75 degrees of noise, the furthest over min_dist wins. Original
// quirk: rfg.exe weighs a direction by 1 - proportion * 0 (the 0 at 0x012B80F8), reading one past its 3 proportions for the last, so all weigh 1.
static bool get_retreat_dir_internal(vector& dir, vector& end_pos, const nav_cell* start_nav_cell, const vector& start_point, const vector& desired_dir, float min_dist, float max_dist, uint64_t* end_nav_cell)
{
    if (end_nav_cell != nullptr)
    {
        *end_nav_cell = 0xFFFFFFFFFFFFFFFFull;
    }
    vector directions[7];
    directions[0] = desired_dir;
    for (int i = 0; i < 3; i++)
    {
        // 75 degrees, in radians with the constant 0.017453 of rfg.exe.
        const float angle = (fl_rand_range(-0.05f, 0.05f) + Retreat_test_angle_proportions[i]) * 75.0f * 0.017453f;
        directions[i * 2 + 1] = desired_dir.copy_rotate_around_y_axis(angle);
        directions[i * 2 + 2] = desired_dir.copy_rotate_around_y_axis(-angle);
    }
    const float min_dist_sqrd = min_dist * min_dist;
    int best = -1;
    float best_dist_sqrd = -1.0f;
    nav_cell* best_cell = nullptr;
    for (int i = 0; i < 7; i++)
    {
        nav_cell* cell;
        const float dist_sqrd = get_retreat_dir_dist_sq(start_nav_cell, start_point, directions[i], max_dist, &cell);
        if (min_dist_sqrd <= dist_sqrd && (best == -1 || best_dist_sqrd < dist_sqrd))
        {
            best_cell = cell;
            best = i;
            best_dist_sqrd = dist_sqrd;
        }
    }
    if (best == -1)
    {
        return false;
    }
    dir = directions[best];
    // A quarter of a metre short.
    end_pos = start_point + dir * (sqrtf(best_dist_sqrd) - 0.25f);
    if (end_nav_cell != nullptr)
    {
        if (best_cell == nullptr)
        {
            return false;
        }
        *end_nav_cell = nav_cell_get_handle(best_cell);
        end_pos = nav_cell_force_point_to_cell(best_cell, end_pos);
    }
    return true;
}

// 0x005E4FC0 (:678): lower is better: away from the enemy, 100 for going over 75 degrees behind the vehicle, 40 per cell vertex in the box
// ahead of a chasing vehicle.
static float get_vehicle_heuristic_cost(const nav_cell* ncp, const vector& pos, const vehicle* my_vehicle, const vector& flee_from_pos, const vehicle* flee_from_vehicle, bool do_evade)
{
    float cost = -vec_dist(pos, flee_from_pos);
    vector to_pos = pos - my_vehicle->pos;
    to_pos.flatten_normalize_safe();
    const vector behind = (-my_vehicle->orient.fvec).copy_flatten_normalize_safe();
    if (0.25881904f <= to_pos * behind)
    {
        cost += 100.0f;
    }
    if (flee_from_vehicle != nullptr && do_evade)
    {
        const vector enemy_forward = flee_from_vehicle->orient.fvec.copy_flatten_normalize_safe();
        matrix enemy_orient;
        enemy_orient.make(enemy_forward);
        for (int i = 0; i < 3; i++)
        {
            const vector vertex = *nav_cell_get_vertex(ncp, i, nullptr);
            if (ix_point_in_box_oriented(vertex, flee_from_vehicle->pos, enemy_orient, RETREAT_VEHICLE_AVOID_ENEMY_BMIN, RETREAT_VEHICLE_AVOID_ENEMY_BMAX))
            {
                cost += 40.0f;
            }
        }
    }
    return cost;
}

// 0x00658E20 (:722): an A* over the vehicle cells, to the first cell max_travel_dist from the start or else the cheapest seen. Half of the
// time (51 of 100) it evades the box in front of a chasing vehicle.
static bool do_retreat_vehicle_astar_search(vector& end_pos, const vehicle* vp, nav_cell* start_ncp, const vector& start_pos, const vector& flee_from_pos, const vehicle* flee_from_vehicle, float max_travel_dist, bool light_structures)
{
    const bool do_evade = vrand() % 100 < 51;
    Nav_heap.insert_cell(start_ncp, 0.0f, 10000.0f, -1, start_pos);
    nav_cell* best_cell = nullptr;
    float best_cost = 0.0f;
    while (0 < Nav_heap.num_nav_cells)
    {
        nav_cell* cell = Nav_heap.remove_best_cell();
        nav_heap_cell_data* data = Nav_heap.get_cell_data(cell);
        const float arrival_cost = data->arrival_cost;
        const char arrival_edge = data->arrival_edge;
        const float est_cost = data->est_cost_to_goal;
        if (max_travel_dist * max_travel_dist <= vec_dist_squared(start_pos, data->arrival_pos))
        {
            end_pos = data->arrival_pos;
            return true;
        }
        if (est_cost < best_cost)
        {
            best_cost = est_cost;
            best_cell = cell;
        }
        for (int edge = 0; edge < 3; edge++)
        {
            if (edge == arrival_edge)
            {
                continue;
            }
            nav_cell* next = cell->adjacent_cells[edge].to_pointer();
            if (next == nullptr || ((next->flags & NAV_CELL_FLAG_DISABLED) != 0 && !(light_structures && (next->flags & NAV_CELL_FLAG_LIGHT) != 0)))
            {
                continue;
            }
            char next_edge = -1;
            for (int back = 0; back < 3; back++)
            {
                if (next->adjacent_cells[back].to_pointer() == cell)
                {
                    next_edge = (char)back;
                    break;
                }
            }
            nav_heap_cell_data* next_data = Nav_heap.get_cell_data(next);
            if (next_data != nullptr && !next_data->is_open)
            {
                continue;
            }
            vector centroid;
            nav_cell_get_centroid(&centroid, next);
            const float cost = vec_dist(data->arrival_pos, centroid) + arrival_cost;
            if (next_data == nullptr)
            {
                Nav_heap.insert_cell(next, cost, get_vehicle_heuristic_cost(next, centroid, vp, flee_from_pos, flee_from_vehicle, do_evade), next_edge, centroid);
            }
            else if (cost < next_data->arrival_cost)
            {
                next_data->arrival_edge = next_edge;
                next_data->arrival_cost = cost;
                // Original quirk: re-sorts the heap at the cell just removed, not at the one that changed.
                Nav_heap.adjust_cell(data->heap_index);
            }
        }
    }
    if (best_cell != nullptr)
    {
        nav_cell_get_centroid(&end_pos, best_cell);
        return true;
    }
    return false;
}

// 0x00623EA0 (:944): living allies on the same level, within 1.5 m in y.
static bool combat_spacing_collect_allies_cb(object* hit_objp, unsigned int, const void* cb_data)
{
    if (hit_objp == nullptr || hit_objp->get_obj_type() != OT_HUMAN || hit_objp == cb_data || hit_objp->is_killed())
    {
        return false;
    }
    const human* self = static_cast<const human*>(cb_data);
    if (human_teams_get_disposition(self, static_cast<const human*>(hit_objp)) != TEAM_DISPOSITION_ALLIED)
    {
        return false;
    }
    return fabsf(self->pos.y - hit_objp->pos.y) < 1.5f;
}

// 0x005E5C50: empty (MOV EAX,ECX; RET) and with no callers in rfg.exe, where it is inlined away. The members stay unset:
// get_ally_target_info fills the ones its caller reads.
ally_target_info::ally_target_info()
{
}

// 0x00623F20 (:959)
static bool get_ally_target_info(ally_target_info& info, const human* hp)
{
    info.ally = hp;
    const player* pl = hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? static_cast<const player*>(hp) : nullptr;
    const npc* np = hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC ? static_cast<const npc*>(hp) : nullptr;
    if (pl != nullptr)
    {
        info.target = pl->aim_pos;
        info.has_target = true;
        return true;
    }
    if (np == nullptr)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        return false;
    }
    const human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        info.has_target = false;
        return true;
    }
    info.target = target->pos;
    info.has_target = true;
    return true;
}

// 0x005E51B0 (:1001)
static vector get_target_repulsor_force(const vector& target_dir, float target_dist, float force_dist)
{
    return target_dir * -((force_dist - target_dist) * 2.5f);
}

// One line's sideways push on pos: delta is pos minus the ally, the line runs from the ally along line_dir for line_length.
static vector line_repulsor_force(const vector& delta, const vector& line_dir, float along, float line_length, float force_dist)
{
    if (along < 0.0f || line_length < along)
    {
        return ZERO_VECTOR;
    }
    const float side = delta.x * line_dir.z + 0.0f + -line_dir.x * delta.z;
    const float sign = side < 0.0f ? -1.0f : 1.0f;
    const float room = force_dist - fabsf(side);
    const float strength = 0.0f - room < 0.0f ? room : 0.0f;
    return vector(sign * line_dir.z * strength, sign * 0.0f * strength, -line_dir.x * sign * strength);
}

// 0x005E5220 (:1015): away from the ally, off the line from the ally to the npc's target, and off the line of the ally's own fire.
static vector get_ally_repulsor_force(const ally_target_info& info, const npc* np, const human* target, float force_dist)
{
    vector to_target = target->pos - np->pos;
    const float target_dist = to_target.flatten_mag_and_normalize_safe();
    vector delta(np->pos.x - info.ally->pos.x, 0.0f, np->pos.z - info.ally->pos.z);
    vector from_ally = delta;
    const float ally_dist = from_ally.mag_and_normalize_safe();
    float push = force_dist - ally_dist;
    if (0.0f <= 0.0f - push)
    {
        push = 0.0f;
    }
    vector force = from_ally * (push * 2.0f);
    force += line_repulsor_force(delta, to_target, -(delta.x * to_target.x + to_target.y * 0.0f + delta.z * to_target.z), target_dist, force_dist);
    if (info.has_target)
    {
        vector ally_aim = info.target - info.ally->pos;
        const float aim_dist = ally_aim.flatten_mag_and_normalize_safe();
        force += line_repulsor_force(delta, ally_aim, delta.x * ally_aim.x + ally_aim.y * 0.0f + delta.z * ally_aim.z, aim_dist, force_dist);
    }
    return force;
}

// 0x005E5830 (:1104)
static bool obstruction_callback(object* hit_objp, void* cb_data)
{
    return hit_objp != cb_data && hit_objp->get_obj_type() != OT_ZONE;
}

// 0x005E5850 (:1144)
static bool evaluate_dodge_filter_callback(object* hit_objp, void* filter_params)
{
    return hit_objp != filter_params;
}

// 0x00677360 (:1149)
bool combat_maneuver_evaluate_dodge_endpoint(const npc* np, const vector& dodge_end, vector* adjusted_point)
{
    if (move_crosses_turret_fire(np, np->pos, dodge_end) || move_crosses_player_fire(np, np->pos, dodge_end))
    {
        return false;
    }
    // A 0.5 m sphere along the way at 0.8 m high, and solid ground under the end.
    raycast_input input;
    raycast_output output;
    input.start = vector(np->pos.x, np->pos.y + 0.8f, np->pos.z);
    input.end = vector(dodge_end.x, dodge_end.y + 0.8f, dodge_end.z);
    input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    input.m_flags = 0;
    input.m_filter_info = COLLIDE_NPC;
    input.filter_callback = evaluate_dodge_filter_callback;
    input.filter_params = const_cast<npc*>(np);
    input.radius = 0.5f;
    if (havok_spherecast(input, output))
    {
        return false;
    }
    input.start = input.end;
    input.end = vector(dodge_end.x, dodge_end.y - 1.0f, dodge_end.z);
    if (!havok_raycast(input, output))
    {
        return false;
    }
    if (adjusted_point != nullptr)
    {
        *adjusted_point = output.m_hits[0].hit_pos;
    }
    return true;
}

// 0x005E5860 (:1255)
bool combat_maneuver_get_indoor_retreat_dir(vector& dir, vector& end_pos, const npc* np, const vector& desired_dir, unsigned int* end_navpoint_handle)
{
    if (end_navpoint_handle != nullptr)
    {
        *end_navpoint_handle = 0xFFFFFFFF;
    }
    navpoint* other = nullptr;
    // 10 to 21: the indoor navpoint types.
    const navpoint* npp = rfg_pf_find_closest_navpoint_to_pos(np->pos, other, 10, 21);
    if (npp == nullptr)
    {
        return false;
    }
    const navpoint* end_npp = nullptr;
    bool result;
    if (other == nullptr || vec_xz_dist(np->pos, npp->pos) - npp->radius <= 0.5f)
    {
        result = get_retreat_dir_from_navpoint(dir, end_pos, np, desired_dir, npp, &end_npp);
    }
    else
    {
        vector closest;
        ix_closest_point_on_line_segment(closest, np->pos, npp->pos, other->pos);
        const float radius = 0.0f <= npp->radius - other->radius ? other->radius : npp->radius;
        if (0.5f < vec_xz_dist(closest, np->pos) - radius)
        {
            // Off the link: on to it first.
            get_retreat_dir_to_nav_link(dir, end_pos, np, closest);
            if (end_navpoint_handle != nullptr)
            {
                *end_navpoint_handle = npp->get_handle();
            }
            return true;
        }
        result = get_retreat_dir_on_nav_link(dir, end_pos, np, desired_dir, npp, other, &end_npp);
    }
    if (end_npp != nullptr && end_navpoint_handle != nullptr)
    {
        *end_navpoint_handle = end_npp->get_handle();
    }
    return result;
}

// 0x00623FC0 (:1367): IN_PROGRESS is no way out in that direction, tried by the caller's next attempt.
ai_results combat_maneuver_get_retreat_dir(vector& dir, vector& end_pos, const npc* np, const vector& desired_dir, float min_dist, float max_dist, uint64_t* end_nav_cell)
{
    vector start = np->pos;
    nav_cell* cell = nullptr;
    if (!np->get_ai()->perception.is_on_navmesh(np, &cell, 1.25f) && bb_pf_find_closest_point_on_nav_mesh(np->pos, start, cell, false))
    {
        // Over 0.65 m off the mesh: no. Otherwise 0.1 m past the edge.
        vector to_mesh = start - np->pos;
        if (0.65f < to_mesh.mag_and_normalize_safe())
        {
            return AI_RESULT_FAILURE;
        }
        start = start + to_mesh * 0.1f;
    }
    if (cell == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    start = nav_cell_force_point_to_cell(cell, start);
    nav_cell_detour_remove_convex_hull_for_request(np->nav_cell_detour_request_handle);
    return get_retreat_dir_internal(dir, end_pos, cell, start, desired_dir, min_dist, max_dist, end_nav_cell) ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

// 0x00696830 (:1422): attempt 0 searches; later attempts try directions out to 40 and 75 degrees either side straight along min_dist.
ai_results combat_maneuver_get_retreat_dir_unified(vector& dir, vector& end_pos, const npc* np, const vector& desired_dir, float min_dist, float max_dist, int attempt, uint64_t* end_nav_cell)
{
    if (attempt == 0)
    {
        return combat_maneuver_get_retreat_dir(dir, end_pos, np, desired_dir, min_dist, max_dist, end_nav_cell);
    }
    // The radians at 0x012CEF80: 40 degrees. The constants below are cos and sin of 75 degrees.
    static const float COS_40 = cosf(0.6981199979782104f); // 0x01DE443C, set on the first call in rfg.exe
    static const float SIN_40 = sinf(0.6981199979782104f);
    const vector side = desired_dir ^ UP_VECTOR;
    switch (attempt)
    {
    case 1:
        dir = desired_dir;
        break;
    case 2:
        dir = desired_dir * COS_40 + side * SIN_40;
        break;
    case 3:
        dir = desired_dir * COS_40 - side * SIN_40;
        break;
    case 4:
        dir = side * 0.9659258f + desired_dir * 0.25881904f;
        break;
    case 5:
        dir = desired_dir * 0.25881904f - side * 0.9659258f;
        break;
    default:
        return AI_RESULT_FAILURE;
    }
    end_pos = np->pos + dir * min_dist;
    if (combat_maneuver_is_path_obstructed(np, end_pos, nullptr))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    return bb_pf_get_nav_cell(end_pos, false, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr) != nullptr ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

// 0x005E5A60 (:1484)
ai_results combat_maneuver_get_retreat_dir(vector& dir, vector& end_pos, const vehicle* vp, const vector& desired_dir, float min_dist, float max_dist)
{
    vector start = vp->pos;
    nav_cell* cell = bb_pf_get_nav_cell(start, true, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
    if (cell == nullptr)
    {
        bb_pf_find_closest_point_on_nav_mesh(vp->pos, start, cell, true);
        if (cell == nullptr)
        {
            return AI_RESULT_FAILURE;
        }
    }
    start = nav_cell_force_point_to_cell(cell, start);
    return get_retreat_dir_internal(dir, end_pos, cell, start, desired_dir, min_dist, max_dist, nullptr) ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

// 0x00659160 (:1526), with get_vehicle_retreat_pos_internal (0x00659110, :929) inlined as in rfg.exe.
bool combat_maneuver_get_vehicle_retreat_pos(vector& end_pos, const vehicle* vp, const human* enemy, float min_travel_dist, float max_travel_dist, bool light_structures)
{
    vector start = vp->pos;
    nav_cell* cell = bb_pf_get_nav_cell(start, true, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
    if (cell == nullptr)
    {
        if (bb_pf_find_closest_point_on_nav_mesh(vp->pos, start, cell, true))
        {
            vector to_mesh = start - vp->pos;
            to_mesh.flatten_normalize_safe();
            start = start + to_mesh * 0.1f;
        }
        if (cell == nullptr)
        {
            return false;
        }
    }
    start = nav_cell_force_point_to_cell(cell, start);
    const vehicle* enemy_vehicle = nullptr;
    if (enemy->vehicle_handle != 0xFFFFFFFF)
    {
        const object* objp = World.object_from_handle(enemy->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            enemy_vehicle = static_cast<const vehicle*>(objp);
        }
    }
    const vector flee_from = enemy_vehicle != nullptr ? enemy_vehicle->pos : enemy->pos;
    Nav_heap.start_session();
    bool found = do_retreat_vehicle_astar_search(end_pos, vp, cell, start, flee_from, enemy_vehicle, max_travel_dist, light_structures);
    Nav_heap.end_session();
    if (found && vec_dist_squared(end_pos, start) < min_travel_dist * min_travel_dist)
    {
        found = false;
    }
    return found;
}

// 0x00677510 (:1578): out of the target's face (3.25 m, indoors 2.25 m), away from allies within 4 m (indoors 2.5 m) who fired in 8 s, off their lines.
bool combat_maneuver_get_spacing_move_dir(vector& force, const npc* np, const human* target)
{
    if (target == nullptr)
    {
        return false;
    }
    force = ZERO_VECTOR;
    const bool indoors = np->get_ai()->perception.is_in_closest_building(np, nullptr, 0.0f, 1.25f);
    const float target_space = indoors ? 2.25f : 3.25f;
    vector to_target = target->pos - np->pos;
    bool pushed = false;
    const float target_dist = to_target.flatten_mag_and_normalize_safe();
    if (target_dist < target_space)
    {
        force += get_target_repulsor_force(to_target, target_dist, target_space);
        pushed = true;
    }
    unsigned int allies[10];
    const int num_allies = World.collect_objects<human>(np->pos, 10.0f, allies, 10, combat_spacing_collect_allies_cb, np);
    if (num_allies == 0 && !pushed)
    {
        return false;
    }
    ally_target_info infos[10];
    int num_infos = 0;
    for (int i = 0; i < num_allies; i++)
    {
        const human* ally = World.object_from_handle<human>(allies[i]);
        const weapon* wp = human_weapons_get_weapon(ally);
        if (wp != nullptr && wp->time_since_fired() < 8001 && get_ally_target_info(infos[num_infos], ally))
        {
            num_infos++;
        }
    }
    if (num_infos == 0 && !pushed)
    {
        return false;
    }
    const float ally_space = indoors ? 2.5f : 4.0f;
    for (int i = 0; i < num_infos; i++)
    {
        force += get_ally_repulsor_force(infos[i], np, target, ally_space);
    }
    return true;
}

// 0x006778B0 (:1653): true is obstructed. A slope over 1 in 2 at the end is obstructed; new_pos is the ground there.
bool combat_maneuver_is_path_obstructed(const npc* np, const vector& dest, vector* new_pos)
{
    raycast_input input;
    raycast_output output;
    input.start = vector(np->pos.x, np->pos.y + 0.8f, np->pos.z);
    input.end = vector(dest.x, dest.y + 0.8f, dest.z);
    input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    input.m_flags = 0;
    input.radius = 0.5f;
    input.m_filter_info = COLLIDE_PHYSICAL_OBJECT;
    input.filter_callback = obstruction_callback;
    input.filter_params = const_cast<npc*>(np);
    if (havok_spherecast(input, output))
    {
        return true;
    }
    float ground;
    if (!havok_terrain_height(ground, dest.x, dest.z, false, nullptr))
    {
        return true;
    }
    const float half_dist = vec_xz_dist(np->pos, dest) * 0.5f;
    const float own_y = np->pos.y;
    if (half_dist < ground - own_y)
    {
        return true;
    }
    if (own_y - ground <= half_dist)
    {
        if (new_pos != nullptr)
        {
            *new_pos = vector(dest.x, ground, dest.z);
        }
        return false;
    }
    // Dropping away faster than that: is there something to land on?
    input.start = vector(dest.x, dest.y + 0.8f, dest.z);
    input.end = vector(dest.x, 0.5f - half_dist + dest.y, dest.z);
    input.filter_callback = nullptr;
    if (!havok_spherecast(input, output))
    {
        return true;
    }
    if (new_pos != nullptr)
    {
        *new_pos = output.m_hits[0].hit_pos;
    }
    return false;
}

// 0x005E5C60 (:1764): in 2D, across the 100 m along the player's aim. Not a player with a melee weapon.
bool combat_maneuver_path_crosses_player_line_of_fire(const npc* np, const vector& dest, float* intersection_dist_sqr)
{
    const player* pl = player_get_local_player();
    if (pl == nullptr)
    {
        return false;
    }
    if (intersection_dist_sqr != nullptr)
    {
        *intersection_dist_sqr = -1.0f;
    }
    const weapon* wp = pl->get_equipped_weapon();
    if (wp != nullptr && wp->w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE)
    {
        return false;
    }
    const vector2 move_start(np->pos.x, np->pos.z);
    const vector2 move_end(dest.x, dest.z);
    const vector2 fire_start(pl->pos.x, pl->pos.z);
    const vector aim = player_autoaim_get_aim_vector();
    const vector2 fire_end(pl->pos.x + aim.x * 100.0f, pl->pos.z + aim.z * 100.0f);
    vector2 ix;
    if (!ix2d_linesegment_linesegment(move_start, move_end, fire_start, fire_end, &ix))
    {
        return false;
    }
    if (intersection_dist_sqr != nullptr)
    {
        *intersection_dist_sqr = vec2_dist_squared(fire_start, ix);
    }
    return true;
}
