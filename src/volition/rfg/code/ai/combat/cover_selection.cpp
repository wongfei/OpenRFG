// Original: volition/rfg/code/ai/combat/cover_selection.cpp

#include "volition/rfg/code/ai/combat/cover_selection.h"

#include "volition/rfg/code/ai/ai_rate_limiters.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/combat/cover_selection_internal.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/pathfinding/nav_heap.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"

#include <math.h>
#include <stdint.h>

extern bool Navpoint_treat_all_as_enabled; // object_navpoint.cpp

// The one search object: cover searches never overlap, and the node arrays it uses are global too.
static cover_search_object Cover_search_object; // 0x01642658

// Cosine of 80 degrees in single precision radians: a dynamic initializer in rfg.exe, zero in .data.
static const float NODE_IN_TARGET_DIRECTION_DOT = (float)cos((double)1.39624f); // 0x01DDB4B0

// The navpoint types that sit on the nav mesh, with nav_cell_ptr set; the names are not recovered.
static const signed char NAVPOINT_TYPE_ON_NAVMESH_A = 11;
static const signed char NAVPOINT_TYPE_ON_NAVMESH_B = 12;
// The types that can be used to reach a cover node on a mover (10 to 17); the names are not recovered.
static const signed char NAVPOINT_TYPE_MOVER_FIRST = 10;
static const signed char NAVPOINT_TYPE_MOVER_LAST = 17;

// 0x00629560 (:119)
static navpoint* find_closest_navpoint_to_cover_node(const cover_node* node, float* node_to_navpoint_dist)
{
    const unsigned int parent_handle = node->get_parent();
    if (parent_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    const object* parent = World.object_from_handle(parent_handle);
    if (parent == nullptr || parent->get_obj_type() != OT_MOVER || parent->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return nullptr;
    }
    navpoint* closest = nullptr;
    // Magic number: the distance reported when there is none.
    float closest_dist = -1.0f;
    object* child = parent->child_ptr;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_NAVPOINT)
        {
            navpoint* npp = (navpoint*)child;
            if (npp->type >= NAVPOINT_TYPE_MOVER_FIRST && npp->type <= NAVPOINT_TYPE_MOVER_LAST &&
                ((npp->type != NAVPOINT_TYPE_ON_NAVMESH_A && npp->type != NAVPOINT_TYPE_ON_NAVMESH_B) || npp->num_links != 1) &&
                fabsf(node->pos.y - npp->pos.y) <= 1.0f)
            {
                const float dist = vec_xz_dist(node->pos, npp->pos) - npp->radius;
                if (closest == nullptr || dist < closest_dist)
                {
                    closest = npp;
                    closest_dist = dist;
                }
            }
        }
        child = child->child_next;
        if (child == parent->child_ptr)
        {
            break;
        }
    }
    if (node_to_navpoint_dist != nullptr)
    {
        *node_to_navpoint_dist = closest_dist;
    }
    return closest;
}

// 0x005E6A10 (:176): movers other than the node's parent that clutter it. World only and no collision rfg movers do not.
static bool clutter_collect_cb(object* objp, unsigned int, const void* cb_data)
{
    if (objp->get_parent() == (unsigned int)(uintptr_t)cb_data)
    {
        return false;
    }
    if (objp->get_sub_type() == OT_SUB_MOVER_GENERAL)
    {
        return true;
    }
    if (objp->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return false;
    }
    const rfg_move_type move_type = ((rfg_mover*)objp)->get_move_type();
    return move_type != RFGM_FIXED && (move_type <= RFGM_ULTRA_LITE || move_type > RFGM_NO_COLLISION);
}

// 0x00629680 (:232)
float cover_search_object::get_travel_cost(const vector& start, const vector& end) const
{
    return get_travel_cost(start, end, vec_dist(start, end));
}

// 0x005E6A50 (:238): travel_dist, raised for a leg that heads towards the target and passes within avoid_dist of it.
float cover_search_object::get_travel_cost(const vector& start, const vector& end, float travel_dist) const
{
    // Magic number: shorter legs cost nothing.
    if (travel_dist < 0.001f)
    {
        return 0.0f;
    }
    float multiplier = 1.0f;
    const float one_over_dist = 1.0f / travel_dist;
    const vector travel_dir((end.x - start.x) * one_over_dist, (end.y - start.y) * one_over_dist, (end.z - start.z) * one_over_dist);
    vector target_dir;
    vec_normalized_dir(target_dir, params.avg_target_pos, start);
    const float dot = target_dir.y * travel_dir.y + travel_dir.x * target_dir.x + travel_dir.z * target_dir.z;
    if (0.0f <= dot)
    {
        vector closest;
        ix_closest_point_on_line_segment(closest, params.avg_target_pos, start, end);
        const float dist_sq = vec_dist_squared(closest, params.avg_target_pos);
        if (dist_sq < params.avoid_dist * params.avoid_dist)
        {
            multiplier = (float)pow((double)params.range_penalty, (double)((1.0f - (float)sqrt((double)dist_sq) / params.avoid_dist) * dot));
        }
    }
    return multiplier * travel_dist;
}

// 0x0065A6F0 (:289)
bool cover_search_object::get_candidate_cover_nodes()
{
    Num_cover_node_candidates = 0;
    const float max_travel_sq = params.max_travel_cost * params.max_travel_cost;
    // Magic number: 150 handles on the stack.
    unsigned int handles[150];
    const int num_handles = World.collect_objects<cover_node>(params.np->pos, params.max_collect_dist, handles, 150, nullptr, nullptr);
    vector to_target_dir(params.avg_target_pos.x - params.np->pos.x, params.avg_target_pos.y - params.np->pos.y, params.avg_target_pos.z - params.np->pos.z);
    const float to_target_dist = to_target_dir.mag_and_normalize();
    const float to_target_dist_sq = to_target_dist * to_target_dist;
    for (int i = 0; i < num_handles; ++i)
    {
        cover_node* node = handles[i] != 0xFFFFFFFF ? (cover_node*)World.object_from_handle(handles[i]) : nullptr;
        if (node == nullptr || node->get_obj_type() != OT_COVER_NODE)
        {
            continue;
        }
        if (node->obj_flags.streamed == 0 || node->is_disabled() || node == params.ignore_node)
        {
            continue;
        }
        const float to_x = params.avg_target_pos.x - node->pos.x;
        const float to_y = params.avg_target_pos.y - node->pos.y;
        const float to_z = params.avg_target_pos.z - node->pos.z;
        const float target_dist_sq = to_x * to_x + to_y * to_y + to_z * to_z;
        if (params.find_closer_node)
        {
            float furthest_sq = target_dist_sq;
            if (node->flags.off_navmesh == 0)
            {
                const float closest_point_dist_sq = vec_dist_squared(node->closest_point, params.avg_target_pos);
                if (target_dist_sq - closest_point_dist_sq < 0.0f)
                {
                    furthest_sq = closest_point_dist_sq;
                }
            }
            if (to_target_dist_sq < furthest_sq)
            {
                continue;
            }
        }
        // A node has to face the target.
        if (node->orient.fvec.y * to_y + node->orient.fvec.x * to_x + node->orient.fvec.z * to_z < 0.0f)
        {
            continue;
        }
        const float dx = node->pos.x - params.np->pos.x;
        const float dy = node->pos.y - params.np->pos.y;
        const float dz = node->pos.z - params.np->pos.z;
        const float node_dist_sq = dx * dx + dy * dy + dz * dz;
        if (node_dist_sq > max_travel_sq)
        {
            continue;
        }
        const float along = dy * to_target_dir.y + dx * to_target_dir.x + dz * to_target_dir.z;
        const float along_dot = along / (float)sqrt((double)node_dist_sq);
        // Too far from the target and leading away from it (the 0.5 metres along are magic numbers).
        if (params.max_fire_range_sq < target_dist_sq && along <= -0.5f && along_dot <= -NODE_IN_TARGET_DIRECTION_DOT)
        {
            continue;
        }
        // Too close to the target and leading towards it.
        if (target_dist_sq < params.min_fire_range_sq && along >= 0.5f && along_dot >= NODE_IN_TARGET_DIRECTION_DOT)
        {
            continue;
        }
        navpoint* npp = nullptr;
        if (node->flags.on_mover != 0)
        {
            float navpoint_dist;
            npp = find_closest_navpoint_to_cover_node(node, &navpoint_dist);
            // Magic number: how far from a navpoint a node on a mover can be and still be used.
            if (npp == nullptr || navpoint_dist > 4.0f)
            {
                continue;
            }
        }
        cover_node_data* candidate = get_new_cover_node_candidate();
        if (candidate == nullptr)
        {
            break;
        }
        candidate->set_cover_node(node);
        candidate->set_navpoint(npp);
    }
    return Num_cover_node_candidates > 0;
}

// 0x0069B430 (:444)
bool cover_search_object::do_secondary_cover_evaluation(cover_node_data* cover_data, const cover_node* node)
{
    if (cover_data->flags.evaluated != 0)
    {
        return true;
    }
    cover_data->flags.evaluated = 1;
    if (params.defend_object != nullptr && params.np->get_ai()->outside_defensive_perimeter(node->pos, false, nullptr))
    {
        return false;
    }
    if (node->is_occupied(params.np) != nullptr || cover_selection_node_is_obstructed_by_geometry(node))
    {
        return false;
    }
    cover_check_results result = CCR_NO_COVER;
    if (params.target == nullptr)
    {
        const int num_positions = params.target_positions.size();
        if (num_positions < 1)
        {
            return false;
        }
        for (int i = 0; i < num_positions; ++i)
        {
            const cover_check_results position_result = node->check_cover_type(params.np, params.target_positions[i], false, true, true, nullptr);
            if (result <= position_result)
            {
                result = position_result;
            }
            if (result == CCR_OFFENSIVE)
            {
                break;
            }
        }
    }
    else
    {
        result = node->check_cover_type(params.np, params.target, false, true, true);
    }
    if (result == CCR_NO_COVER)
    {
        return false;
    }
    if (result == CCR_DEFENSIVE_ONLY)
    {
        if (!params.allow_defensive_cover)
        {
            return false;
        }
        cover_data->flags.defense_only = 1;
    }
    cover_data->firing_cost = 0.0f;
    if (cover_data->flags.defense_only != 0 && params.np->is_armed(true))
    {
        // Magic number: an armed npc would rather have somewhere to shoot from.
        cover_data->firing_cost = 5.0f;
    }
    return true;
}

// 0x0065AB90 (:533)
bool cover_search_object::add_navcell(cover_search_node* old_node, nav_cell* ncp, const vector& cell_entry_point)
{
    cover_search_node* node = get_cover_search_node(ncp);
    if (node != nullptr && node->heap_index == -1)
    {
        return false;
    }
    cover_search_node* shortcut = get_navmesh_shortcut(cell_entry_point, old_node);
    if (shortcut != nullptr)
    {
        old_node = shortcut;
    }
    const float travel_cost = old_node->travel_cost + get_travel_cost(old_node->pos, cell_entry_point);
    const float total_cost = travel_cost + 0.0f;
    if (params.max_travel_cost < total_cost)
    {
        return false;
    }
    const bool is_new = node == nullptr;
    if (is_new)
    {
        node = get_new_cover_search_node();
        if (node == nullptr)
        {
            return false;
        }
        attach_cover_search_node(node, ncp);
    }
    else if (node->total_cost <= total_cost)
    {
        return false;
    }
    node->prev_path_node = old_node;
    node->pos = cell_entry_point;
    node->travel_cost = travel_cost;
    node->remaining_cost = 0.0f;
    node->total_cost = total_cost;
    if (is_new)
    {
        Cover_search_heap.insert(node);
    }
    else
    {
        Cover_search_heap.percolate_up(node);
    }
    return true;
}

// 0x0065AD20 (:624)
bool cover_search_object::add_navpoint(cover_search_node* old_node, navpoint* npp)
{
    cover_search_node* node = get_cover_search_node(npp);
    if (node != nullptr && node->heap_index == -1)
    {
        return false;
    }
    if (npp->type == NAVPOINT_TYPE_ON_NAVMESH_A || npp->type == NAVPOINT_TYPE_ON_NAVMESH_B)
    {
        cover_search_node* shortcut = get_navmesh_shortcut(npp->pos, old_node);
        if (shortcut != nullptr)
        {
            old_node = shortcut;
        }
    }
    const float travel_cost = old_node->travel_cost + get_travel_cost(old_node->pos, npp->pos);
    const float total_cost = travel_cost + 0.0f;
    if (params.max_travel_cost < total_cost)
    {
        return false;
    }
    const bool is_new = node == nullptr;
    if (is_new)
    {
        node = get_new_cover_search_node();
        if (node == nullptr)
        {
            return false;
        }
        attach_cover_search_node(node, npp);
    }
    else if (node->total_cost <= total_cost)
    {
        return false;
    }
    node->prev_path_node = old_node;
    node->pos = npp->pos;
    node->travel_cost = travel_cost;
    node->remaining_cost = 0.0f;
    node->total_cost = total_cost;
    if (is_new)
    {
        Cover_search_heap.insert(node);
    }
    else
    {
        Cover_search_heap.percolate_up(node);
    }
    return true;
}

// 0x0065AED0 (:713)
bool cover_search_object::add_cover_node(cover_search_node* old_node, cover_node_data* cover_data, const cover_node* cover)
{
    cover_search_node* node = get_cover_search_node(cover_data);
    if (node != nullptr && node->heap_index == -1)
    {
        return false;
    }
    if (cover->closest_nav_cell_handle != 0xFFFFFFFFFFFFFFFFull)
    {
        cover_search_node* shortcut = get_navmesh_shortcut(cover->closest_point, old_node);
        if (shortcut != nullptr)
        {
            old_node = shortcut;
        }
    }
    const float travel_cost = old_node->travel_cost + get_travel_cost(old_node->pos, cover->pos);
    const float firing_cost = cover_data->firing_cost;
    const float total_cost = firing_cost + travel_cost;
    if (params.max_travel_cost < total_cost)
    {
        return false;
    }
    const bool is_new = node == nullptr;
    if (is_new)
    {
        node = get_new_cover_search_node();
        if (node == nullptr)
        {
            return false;
        }
        attach_cover_search_node(node, cover_data);
    }
    else if (node->total_cost <= total_cost)
    {
        return false;
    }
    node->prev_path_node = old_node;
    node->pos = cover->pos;
    node->travel_cost = travel_cost;
    node->remaining_cost = firing_cost;
    node->total_cost = total_cost;
    if (is_new)
    {
        Cover_search_heap.insert(node);
    }
    else
    {
        Cover_search_heap.percolate_up(node);
    }
    if (best_cover_node == nullptr || node->total_cost < best_cover_node->total_cost)
    {
        best_cover_node = node;
    }
    cover_data->flags.searched = 1;
    return true;
}

// 0x006296C0 (:808)
cover_search_node* cover_search_object::get_navmesh_shortcut(const vector& pos, cover_search_node* old_node) const
{
    if (old_node == nullptr || old_node->type != COVER_SEARCH_NAV_CELL)
    {
        return nullptr;
    }
    cover_search_node* prev = old_node->prev_path_node;
    const nav_cell* start_nav_cell;
    if (prev->type == COVER_SEARCH_NAV_CELL)
    {
        start_nav_cell = (const nav_cell*)prev->ptr;
    }
    else
    {
        if (prev->type != COVER_SEARCH_NAV_POINT)
        {
            return nullptr;
        }
        const navpoint* npp = (const navpoint*)prev->ptr;
        if (npp == nullptr || (npp->type != NAVPOINT_TYPE_ON_NAVMESH_A && npp->type != NAVPOINT_TYPE_ON_NAVMESH_B))
        {
            return nullptr;
        }
        start_nav_cell = npp->nav_cell_ptr;
    }
    if (start_nav_cell == nullptr)
    {
        return nullptr;
    }
    vector out_pos;
    const nav_cell* out_cell = nullptr;
    if (!nav_cell_find_longest_line_of_sight_no_detour(out_pos, out_cell, prev->pos, start_nav_cell, pos, false, nullptr) && vec_xz_dist_squared(out_pos, pos) > 1.0f)
    {
        return nullptr;
    }
    return prev;
}

// 0x006B6420 (:907)
void cover_search_object::explore(cover_search_node* node)
{
    if (node->type == COVER_SEARCH_NAV_CELL)
    {
        if (node->ptr != nullptr)
        {
            explore_navcell(node, (nav_cell*)node->ptr);
        }
    }
    else if (node->type == COVER_SEARCH_NAV_POINT && node->ptr != nullptr)
    {
        explore_navpoint(node, (navpoint*)node->ptr);
    }
}

// 0x0069B710 (:933)
void cover_search_object::explore_navcell(cover_search_node* node, nav_cell* ncp)
{
    const uint64_t handle = nav_cell_get_handle(ncp);
    for (int edge = 0; edge < 3; ++edge)
    {
        nav_cell* adjacent = ncp->adjacent_cells[edge].to_pointer();
        if (adjacent != nullptr && (adjacent->flags & NAV_CELL_FLAG_DISABLED) == 0)
        {
            vector edge_vert0;
            vector edge_vert1;
            nav_cell_get_edge_verts(&edge_vert0, &edge_vert1, ncp, edge, nullptr);
            const vector midpoint((edge_vert1.x + edge_vert0.x) * 0.5f, (edge_vert0.y + edge_vert1.y) * 0.5f, (edge_vert0.z + edge_vert1.z) * 0.5f);
            add_navcell(node, adjacent, midpoint);
        }
    }
    if (ncp->navp != 0xFFFFFFFF)
    {
        navpoint* npp = (navpoint*)World.object_from_handle(ncp->navp);
        while (npp != nullptr && npp->get_obj_type() == OT_NAVPOINT)
        {
            add_navpoint(node, npp);
            const unsigned int next = npp->get_next_navpoint();
            npp = next != 0xFFFFFFFF ? (navpoint*)World.object_from_handle(next) : nullptr;
        }
    }
    for (int i = 0; i < Num_cover_node_candidates; ++i)
    {
        cover_node_data* data = &Cover_node_candidates[i];
        if (data->flags.failed_evaluation != 0)
        {
            continue;
        }
        const cover_node* cover = data->get_cover_node();
        if (cover->flags.on_mover != 0 || cover->closest_nav_cell_handle != handle)
        {
            continue;
        }
        if (data->flags.evaluated != 0 || do_secondary_cover_evaluation(data, cover))
        {
            add_cover_node(node, data, cover);
        }
        else
        {
            data->flags.failed_evaluation = 1;
        }
    }
}

// 0x0069B910 (:1003)
void cover_search_object::explore_navpoint(cover_search_node* node, navpoint* npp)
{
    if (npp->type == NAVPOINT_TYPE_ON_NAVMESH_A || npp->type == NAVPOINT_TYPE_ON_NAVMESH_B)
    {
        nav_cell* ncp = npp->nav_cell_ptr;
        if (ncp != nullptr && (ncp->flags & NAV_CELL_FLAG_DISABLED) == 0)
        {
            add_navcell(node, ncp, npp->pos);
        }
    }
    for (int i = 0; i < npp->num_links; ++i)
    {
        if (!Navpoint_treat_all_as_enabled && npp->link_disabled_by[i] != 0)
        {
            continue;
        }
        navpoint* link = npp->links[i];
        if (link != nullptr && (Navpoint_treat_all_as_enabled || link->heap_data.disabled_by == 0))
        {
            add_navpoint(node, link);
        }
    }
    for (int i = 0; i < Num_cover_node_candidates; ++i)
    {
        cover_node_data* data = &Cover_node_candidates[i];
        if (data->flags.failed_evaluation != 0 || data->get_navpoint() != npp)
        {
            continue;
        }
        const cover_node* cover = data->get_cover_node();
        if (cover == nullptr)
        {
            continue;
        }
        if (data->flags.evaluated != 0 || do_secondary_cover_evaluation(data, cover))
        {
            add_cover_node(node, data, cover);
        }
        else
        {
            data->flags.failed_evaluation = 1;
        }
    }
}

// 0x00678E60 (:1083)
bool cover_search_object::set_up_search(cover_search_node* start_node)
{
    nav_cell* ncp = nullptr;
    navpoint* npp = nullptr;
    vector navmesh_pos;
    if (!params.np->get_ai()->perception.get_nearest_navpoint_or_navcell(ncp, npp, &navmesh_pos, nullptr))
    {
        return false;
    }
    if (ncp != nullptr)
    {
        return add_navcell(start_node, ncp, navmesh_pos);
    }
    if (npp != nullptr)
    {
        return add_navpoint(start_node, npp);
    }
    return false;
}

// 0x006B6470 (:1112)
void cover_search_object::do_astar_search()
{
    cover_search_node* start_node = get_new_cover_search_node();
    if (start_node == nullptr)
    {
        return;
    }
    start_node->type = COVER_SEARCH_START;
    start_node->pos = params.np->pos;
    start_node->prev_path_node = nullptr;
    start_node->travel_cost = 0.0f;
    start_node->remaining_cost = 0.0f;
    start_node->total_cost = 0.0f;
    start_node->heap_index = -1;
    if (!set_up_search(start_node))
    {
        return;
    }
    while (Cover_search_heap.size() > 0)
    {
        cover_search_node* node = Cover_search_heap.remove_best_node();
        if (node->type == COVER_SEARCH_COVER_NODE)
        {
            best_cover_node = node;
            return;
        }
        explore(node);
    }
}

static bool is_firing_ammo_type(weapon_ammo_type type)
{
    return type == WEAPON_AMMO_TYPE_BULLET || type == WEAPON_AMMO_TYPE_PROJECTILE || type == WEAPON_AMMO_TYPE_ELECTRICITY || type == WEAPON_AMMO_TYPE_REPAIR || type == WEAPON_AMMO_TYPE_BLANKS || type == WEAPON_AMMO_TYPE_FLAME;
}

// The start of both constructors, inlined in rfg.exe: the object the npc's squad defends, and the weapon's ranges.
static void search_parameters_set_defense_and_range(cover_search_parameters& params, const npc* attacker)
{
    const gfm_orders* orders = attacker->get_ai()->get_orders();
    params.defend_object = orders != nullptr && (orders->get_orders_type() == GFM_ORDERS_TYPE_DEFEND || orders->get_orders_type() == GFM_ORDERS_TYPE_ROADBLOCK) ? orders->get_target_object() : nullptr;
    const weapon_info* info = human_weapons_get_weapon_info(attacker);
    if (info == nullptr || !is_firing_ammo_type(info->ammo_type))
    {
        // Magic numbers: 10 and 50 metres, squared.
        params.min_fire_range_sq = 100.0f;
        params.max_fire_range_sq = 2500.0f;
    }
    else
    {
        params.min_fire_range_sq = info->min_engagement_dist * info->min_engagement_dist;
        params.max_fire_range_sq = info->max_range * info->max_range;
    }
}

// The end of both constructors, inlined in rfg.exe.
static void search_parameters_set_travel(cover_search_parameters& params, const npc* attacker, float max_dist)
{
    if (attacker->get_ai()->perception.is_in_closest_building(attacker, nullptr, 0.0f, 1.25f))
    {
        // Magic numbers: 0.6 squared, and 0.75. Indoors the target can be closer.
        params.min_fire_range_sq = params.min_fire_range_sq * 0.36f;
        params.avoid_dist = params.avoid_dist * 0.75f;
    }
    params.range_penalty = 5.0f;
    params.max_travel_cost = max_dist;
    // Original bug kept: preprocess has not set initial_target_dist yet, so this reads an uninitialized stack float. The
    // parameter is clamped, so the search distance is scaled by anything from 1 to 1.5 (10 to 40 metres was meant).
    params.max_travel_cost = fl_parameter(params.initial_target_dist, 10.0f, 40.0f, 1.0f, 1.5f, true) * params.max_travel_cost;
    params.max_collect_dist = params.max_travel_cost * 0.6f;
}

// 0x00678EE0 (:1173)
cover_search_parameters::cover_search_parameters(const npc* attacker, const object* target_, float max_dist_, bool defensive, const cover_node* ignore_node_, bool has_to_be_closer)
{
    target = target_;
    np = attacker;
    allow_defensive_cover = defensive;
    ignore_node = ignore_node_;
    find_closer_node = has_to_be_closer;
    search_parameters_set_defense_and_range(*this, attacker);
    // Magic number: 5 metres unless the target's weapon says otherwise.
    avoid_dist = 5.0f;
    if (target->get_obj_type() == OT_HUMAN)
    {
        const weapon_info* target_info = human_weapons_get_weapon_info((const human*)target);
        if (target_info != nullptr && is_firing_ammo_type(target_info->ammo_type))
        {
            avoid_dist = target_info->min_engagement_dist;
        }
    }
    search_parameters_set_travel(*this, attacker, max_dist_);
}

// 0x00679120 (:1213)
cover_search_parameters::cover_search_parameters(const npc* attacker, const vector& target_pos_, float max_dist_, bool defensive, const cover_node* ignore_node_, bool has_to_be_closer)
{
    np = attacker;
    target = nullptr;
    allow_defensive_cover = defensive;
    target_positions.add(target_pos_);
    ignore_node = ignore_node_;
    find_closer_node = has_to_be_closer;
    search_parameters_set_defense_and_range(*this, attacker);
    avoid_dist = 5.0f;
    search_parameters_set_travel(*this, attacker, max_dist_);
}

// 0x0064DE60 (.h:22)
cover_search_parameters::cover_search_parameters()
{
}

// 0x006570F0
cover_search_parameters::~cover_search_parameters()
{
}

// 0x006B6580
cover_search_parameters& cover_search_parameters::operator=(const cover_search_parameters& __that)
{
    np = __that.np;
    target = __that.target;
    ignore_node = __that.ignore_node;
    allow_defensive_cover = __that.allow_defensive_cover;
    find_closer_node = __that.find_closer_node;
    defend_object = __that.defend_object;
    min_fire_range_sq = __that.min_fire_range_sq;
    max_fire_range_sq = __that.max_fire_range_sq;
    max_collect_dist = __that.max_collect_dist;
    max_travel_cost = __that.max_travel_cost;
    target_positions = __that.target_positions;
    range_penalty = __that.range_penalty;
    avoid_dist = __that.avoid_dist;
    avg_target_pos = __that.avg_target_pos;
    target_fvec = __that.target_fvec;
    initial_target_dist = __that.initial_target_dist;
    return *this;
}

// 0x00629770 (:1247)
void cover_search_parameters::preprocess()
{
    if (target != nullptr)
    {
        avg_target_pos = target->pos;
    }
    else
    {
        const int num_positions = target_positions.size();
        if (num_positions < 1)
        {
            return;
        }
        if (num_positions == 1)
        {
            avg_target_pos = target_positions[0];
        }
        else
        {
            avg_target_pos = ZERO_VECTOR;
            for (int i = 0; i < num_positions; ++i)
            {
                avg_target_pos.x += target_positions[i].x;
                avg_target_pos.y += target_positions[i].y;
                avg_target_pos.z += target_positions[i].z;
            }
            const float one_over_num = 1.0f / (float)num_positions;
            avg_target_pos.x *= one_over_num;
            avg_target_pos.y *= one_over_num;
            avg_target_pos.z *= one_over_num;
        }
    }
    vec_normalized_dir(target_fvec, np->pos, avg_target_pos);
    initial_target_dist = vec_dist(avg_target_pos, np->pos);
}

// 0x00679360 (:1278)
cover_search_object::cover_search_object()
{
    Num_cover_search_nodes = 0;
    Cover_search_heap.reset();
    best_cover_node = nullptr;
}

// 0x0065A6E0
cover_search_object::~cover_search_object()
{
}

// 0x0065B0B0 (:1285)
void cover_search_object::reset()
{
    Num_cover_search_nodes = 0;
    Cover_search_heap.reset();
    best_cover_node = nullptr;
}

// 0x006BC740 (:1297)
cover_node* cover_search_object::do_search(const cover_search_parameters& params_)
{
    params = params_;
    if (!get_candidate_cover_nodes())
    {
        return nullptr;
    }
    cover_search_begin();
    do_astar_search();
    cover_search_end();
    return get_best_cover_node();
}

// 0x006298E0 (:1342)
cover_node* cover_search_object::get_best_cover_node() const
{
    if (best_cover_node == nullptr || best_cover_node->type != COVER_SEARCH_COVER_NODE || best_cover_node->ptr == nullptr)
    {
        return nullptr;
    }
    return ((const cover_node_data*)best_cover_node->ptr)->get_cover_node();
}

// 0x006BC8A0 (:1477)
cover_node* cover_selection_do_search(cover_search_parameters& params)
{
    if (!Ai_cover_search_limiter.request(params.np))
    {
        return nullptr;
    }
    params.preprocess();
    // How far from the target the npc would still be after using all of its travel: past the weapon's range, don't search.
    float closest_dist = params.initial_target_dist - params.max_travel_cost;
    if (0.0f <= 0.0f - closest_dist)
    {
        closest_dist = 0.0f;
    }
    if (closest_dist * closest_dist > params.max_fire_range_sq)
    {
        return nullptr;
    }
    Cover_search_object.reset();
    return Cover_search_object.do_search(params);
}

// 0x00679390 (:1519)
bool cover_selection_node_is_obstructed_by_geometry(const cover_node* node)
{
    // Magic numbers: the box a human takes up at the node, lower for a crouching node that has no popup position.
    const vector bmin(-0.3f, 0.0f, -0.45f);
    const vector bmax(0.3f, (node->flags.crouch == 0 || node->flags.fire_popup != 0) ? 1.8f : 1.325f, 0.3f);
    unsigned int handle;
    return World.collect_objects<object_mover>(bmin, bmax, node->pos, node->orient, &handle, 1, clutter_collect_cb, (const void*)(uintptr_t)node->get_parent()) > 0;
}
