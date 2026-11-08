// Original: volition/rfg/code/ai/search_utils.cpp

#include "volition/rfg/code/ai/search_utils.h"

#include "volition/rfg/code/ai/search_system_internal.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/pathfinding/nav_heap.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"

#include <math.h>

extern nav_heap Nav_heap; // nav_heap.cpp

static const float ASTAR_TARGET_EXCLUSION_RADIUS_SQ = 6.25f; // 2.5 m, 0x01642F90
static const float SEARCH_AREA_RADIUS = 40.0f; // m, beyond it a cell gets a penalty
static const float SEARCH_AREA_PENALTY = 5.0f; // per metre
static const float OTHER_SEARCHER_RADIUS = 15.0f; // m
static const float OTHER_SEARCHER_STRENGTH = 10.0f; // per metre
static const unsigned char NAV_CELL_FLAG_IMPASSABLE = 8; // the bit the walks here stop at

// 0x005F1A20 (:132). LTCG: start_pos ECX, start_navcell EAX, target_pos ESI.
static bool clear_line_of_sight_on_navmesh(const vector& start_pos, const nav_cell* start_navcell, const vector& target_pos)
{
    vector end_pos;
    const nav_cell* end_cell;
    return nav_cell_find_longest_line_of_sight_no_detour(end_pos, end_cell, start_pos, start_navcell, target_pos, false, nullptr) || vec_xz_dist_squared(end_pos, target_pos) <= 0.25f; // 0.5 m
}

// 0x005F1A70 (:156). LTCG: move_dir EAX, fvec ECX. From 0.5 ahead through 1 to the side to 5 behind.
static float get_search_dir_modifier(const vector& move_dir, const vector& fvec)
{
    const float dot = fvec * move_dir;
    return dot < 0.0f ? fl_parameter(dot, 0.0f, -1.0f, 1.0f, 5.0f, false) : fl_parameter(dot, 0.0f, 1.0f, 1.0f, 0.5f, false);
}

// 0x005F1B20 (:169). LTCG: ncp ECX, search_center EAX.
static float get_out_of_search_heuristic_penalty(const nav_cell* ncp, const vector& search_center)
{
    const float beyond = sqrtf(nav_cell_get_closest_xz_dist_squared_to_point(ncp, search_center)) - SEARCH_AREA_RADIUS;
    return (beyond > 0.0f ? beyond : 0.0f) * SEARCH_AREA_PENALTY;
}

// 0x005F1B90 (:175). LTCG: ncp ECX, target_pos EAX.
static float target_heuristic_penalty(const nav_cell* ncp, const vector& target_pos)
{
    return sqrtf(nav_cell_get_closest_xz_dist_squared_to_point(ncp, target_pos));
}

// 0x005F1BB0 (:184). LTCG: ncp ECX, repulsor_pos EAX.
static float get_distance_heuristic_penalty(const nav_cell* ncp, const vector& repulsor_pos, float repulsor_radius, float repulsor_strength)
{
    const float within = repulsor_radius - sqrtf(nav_cell_get_closest_xz_dist_squared_to_point(ncp, repulsor_pos));
    return (within > 0.0f ? within : 0.0f) * repulsor_strength;
}

// 0x00685660 (:199). LTCG: ncp EDI, search_center EAX.
static float get_next_search_locatin_heuristic_cost(const nav_cell* ncp, const npc* np, const human* target, const vector& search_center)
{
    float cost = target_heuristic_penalty(ncp, target->pos) + get_out_of_search_heuristic_penalty(ncp, search_center);
    csspinlock_scope lock(ai_search_get_searchers_cs_wrapper());
    for (const ai_search_searcher_info* info = ai_search_get_first_searcher(); info != nullptr; info = ai_search_get_next_searcher(info))
    {
        const object* objp = info->searcher_handle != 0xFFFFFFFF ? World.object_from_handle(info->searcher_handle) : nullptr;
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || human_teams_get_disposition(np, static_cast<const human*>(objp)) != TEAM_DISPOSITION_ALLIED)
        {
            continue;
        }
        const ai_search_location_info* location = ai_search_get_location_info(info->search_handle);
        if (location != nullptr && location->search_type == AI_ST_EXTERNAL_AREA && info->searcher_handle != np->get_handle() && info->location_history.has_current)
        {
            cost += get_distance_heuristic_penalty(ncp, info->location_history.current_target, OTHER_SEARCHER_RADIUS, OTHER_SEARCHER_STRENGTH);
        }
    }
    return cost;
}

// 0x00685850 (:250). LTCG: search_center EAX.
static bool is_valid_search_destination(const nav_cell* ncp, const npc* np, const human*, const vector& search_center)
{
    if (nav_cell_get_closest_xz_dist_squared_to_point(ncp, search_center) > 2500.0f) // 50 m
    {
        return false;
    }
    csspinlock_scope lock(ai_search_get_searchers_cs_wrapper());
    for (const ai_search_searcher_info* info = ai_search_get_first_searcher(); info != nullptr; info = ai_search_get_next_searcher(info))
    {
        const object* objp = info->searcher_handle != 0xFFFFFFFF ? World.object_from_handle(info->searcher_handle) : nullptr;
        if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || human_teams_get_disposition(np, static_cast<const human*>(objp)) != TEAM_DISPOSITION_ALLIED)
        {
            continue;
        }
        const ai_search_location_info* location = ai_search_get_location_info(info->search_handle);
        if (location == nullptr || location->search_type != AI_ST_EXTERNAL_AREA)
        {
            continue;
        }
        if (info->searcher_handle != np->get_handle() && info->location_history.has_current && nav_cell_get_closest_xz_dist_squared_to_point(ncp, info->location_history.los_position) <= ASTAR_TARGET_EXCLUSION_RADIUS_SQ)
        {
            return false;
        }
        circ_array<vector, 8>& old = const_cast<circ_array<vector, 8>&>(info->location_history.old);
        for (int i = 0; i < old.size(); i++)
        {
            if (nav_cell_get_closest_xz_dist_squared_to_point(ncp, old[i]) <= ASTAR_TARGET_EXCLUSION_RADIUS_SQ)
            {
                return false;
            }
        }
    }
    return true;
}

// 0x00685A20 (:313): a best first walk out from the npc over the cells it can see; the first destination is where the view ends.
static bool do_next_location_astar_search(vector& chosen_pos, vector& direct_path_point, nav_cell*& ncp, const npc* np, const human* target)
{
    if (target == nullptr)
    {
        return false;
    }
    const ai_search_target* search = Ai_search_object_manager.find_target(target->get_handle());
    if (search == nullptr)
    {
        return false;
    }
    const vector search_center = search->best_guess_position;
    vector start = np->pos;
    nav_cell* start_cell = bb_pf_get_nav_cell_from_hint(&start, np->pf.last_visited_nav_cell_handle, np->pos, false, nullptr, true, false, NAV_CELL_TRAVERSAL_ENABLED);
    if (start_cell == nullptr && !bb_pf_find_closest_point_on_nav_mesh(np->pos, start, start_cell, false))
    {
        return false;
    }
    Nav_heap.insert_cell(start_cell, 0.0f, get_next_search_locatin_heuristic_cost(start_cell, np, target, search_center), -1, start);
    nav_cell* best = nullptr;
    float best_score = -1.0f;
    bool best_valid = false;
    while (Nav_heap.num_nav_cells > 0)
    {
        nav_cell* cur = Nav_heap.remove_best_cell();
        nav_heap_cell_data* cur_data = Nav_heap.get_cell_data(cur);
        const int arrival_edge = cur_data->arrival_edge;
        const vector arrival_pos = cur_data->arrival_pos;
        const bool valid = is_valid_search_destination(cur, np, target, search_center);
        const float score = cur_data->est_cost_to_goal - vec_dist(np->pos, arrival_pos);
        if ((best == nullptr || (valid && !best_valid) || score < best_score) && (valid || !best_valid))
        {
            best_valid = best_valid || valid;
            best = cur;
            best_score = score;
        }
        for (int edge = 0; edge < 3; edge++)
        {
            if (edge == arrival_edge)
            {
                continue;
            }
            vector vert0, vert1;
            nav_cell_get_edge_verts(&vert0, &vert1, cur, edge, nullptr);
            const vector edge_mid = (vert0 + vert1) * 0.5f;
            vector edge_dir;
            nav_cell_get_edge_dir(&edge_dir, cur, edge);
            nav_cell* adjacent = cur->adjacent_cells[edge].to_pointer();
            if (adjacent == nullptr)
            {
                continue;
            }
            nav_heap_cell_data* adjacent_data = Nav_heap.get_cell_data(adjacent);
            if (adjacent_data != nullptr && !adjacent_data->is_open)
            {
                continue;
            }
            if ((adjacent->flags & NAV_CELL_FLAG_IMPASSABLE) == 0)
            {
                vector seen_pos;
                const nav_cell* seen_cell;
                if (arrival_edge == -1 || nav_cell_find_longest_line_of_sight_no_detour(seen_pos, seen_cell, edge_mid, cur, start, false, nullptr) || vec_xz_dist_squared(seen_pos, start) <= 0.25f)
                {
                    // The cell is in view: walk on.
                    int entry_edge = -1;
                    for (int i = 0; i < 3; i++)
                    {
                        if (adjacent->adjacent_cells[i].to_pointer() == cur)
                        {
                            entry_edge = i;
                            break;
                        }
                    }
                    const vector entry_pos = nav_cell_force_point_to_cell(adjacent, edge_mid);
                    vector dir;
                    const float dist = vec_normalized_dir(dir, entry_pos, start);
                    const float cost = get_search_dir_modifier(dir, np->orient.fvec) * dist;
                    if (dist <= 15.0f || cost <= 30.0f) // m
                    {
                        if (adjacent_data != nullptr)
                        {
                            if (cost < adjacent_data->arrival_cost)
                            {
                                adjacent_data->arrival_edge = (char)entry_edge;
                                adjacent_data->arrival_cost = cost;
                                adjacent_data->arrival_pos = entry_pos;
                                // Original bug kept: the heap is fixed at the index of the cell being expanded, not of the one that changed.
                                Nav_heap.adjust_cell(cur_data->heap_index);
                            }
                        }
                        else
                        {
                            Nav_heap.insert_cell(adjacent, cost, get_next_search_locatin_heuristic_cost(adjacent, np, target, search_center), (char)entry_edge, entry_pos);
                        }
                    }
                }
                else if (valid)
                {
                    // Out of view from the npc: this cell's far edge is a place to go and look.
                    vector entry0, entry1;
                    nav_cell_get_edge_verts(&entry0, &entry1, cur, arrival_edge, nullptr);
                    const float entry_len = vec_dist(entry0, entry1);
                    if (entry_len <= 1.0f)
                    {
                        nav_cell_get_edge_midpoint(&direct_path_point, cur, arrival_edge);
                    }
                    else
                    {
                        vector closest;
                        const float along = ix_closest_point_on_line_segment(closest, np->pos, entry0, entry1);
                        vector entry_dir;
                        nav_cell_get_edge_dir(&entry_dir, cur, arrival_edge);
                        if (along <= 0.5f)
                        {
                            direct_path_point = entry0 + entry_dir * 0.5f;
                        }
                        else if (along < entry_len - 0.5f)
                        {
                            direct_path_point = closest;
                        }
                        else
                        {
                            direct_path_point = entry1 - entry_dir * 0.5f;
                        }
                        if (!clear_line_of_sight_on_navmesh(direct_path_point, cur, start))
                        {
                            nav_cell_get_edge_midpoint(&direct_path_point, cur, arrival_edge);
                        }
                    }
                    chosen_pos = nav_cell_force_point_to_cell(cur, edge_mid);
                    direct_path_point = nav_cell_force_point_to_cell(cur, direct_path_point);
                    ncp = cur;
                    return true;
                }
            }
            else if (arrival_edge != -1 && valid)
            {
                // A wall: go and look along it when the npc is on the far side of its line.
                const vector vert = *nav_cell_get_vertex(cur, edge, nullptr);
                if ((start.z - vert.z) * edge_dir.x - edge_dir.z * (start.x - vert.x) <= 0.0f)
                {
                    continue;
                }
                const vector next_vert = *nav_cell_get_vertex(cur, (edge + 1) % 3, nullptr);
                vector closest;
                ix_closest_point_on_line_segment(closest, start, vert, next_vert);
                if (vec_xz_dist_squared(closest, start) <= 0.25f)
                {
                    continue;
                }
                vector entry_dir;
                nav_cell_get_edge_dir(&entry_dir, cur, arrival_edge);
                const int next_edge = (arrival_edge + 1) % 3;
                const vector corner = *nav_cell_get_vertex(cur, edge == next_edge ? next_edge : arrival_edge, nullptr);
                if (vec_dist(corner, arrival_pos) < 0.5f)
                {
                    direct_path_point = arrival_pos;
                }
                else
                {
                    direct_path_point = edge == next_edge ? corner - entry_dir * 0.5f : corner + entry_dir * 0.5f;
                    if (!clear_line_of_sight_on_navmesh(direct_path_point, cur, start))
                    {
                        direct_path_point = arrival_pos;
                    }
                }
                const vector far_vert = *nav_cell_get_vertex(cur, (arrival_edge + 2) % 3, nullptr);
                chosen_pos = nav_cell_force_point_to_cell(cur, far_vert);
                direct_path_point = nav_cell_force_point_to_cell(cur, direct_path_point);
                ncp = cur;
                return true;
            }
        }
    }
    if (best == nullptr)
    {
        return false;
    }
    const nav_heap_cell_data* best_data = Nav_heap.get_cell_data(best);
    if (best_data == nullptr)
    {
        return false;
    }
    chosen_pos = nav_cell_force_point_to_cell(best, best_data->arrival_pos);
    direct_path_point = chosen_pos;
    ncp = best;
    return true;
}

// 0x00686670 (:690)
bool ai_search_get_next_search_location(vector& pos, vector& direct_pos, nav_cell*& ncp, const npc* np, const human* target)
{
    Nav_heap.start_session();
    const bool found = do_next_location_astar_search(pos, direct_pos, ncp, np, target);
    Nav_heap.end_session();
    return found;
}

// 0x0066BD60 (:715)
cover_node* ai_search_get_cover_node_for_location(const npc* np, const vector& pos, const vector& direct_pos, const nav_cell* ncp)
{
    vector start = np->pos;
    nav_cell* start_cell = bb_pf_get_nav_cell(start, false, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
    if (start_cell == nullptr && !bb_pf_find_closest_point_on_nav_mesh(np->pos, start, start_cell, false))
    {
        return nullptr;
    }
    vector to_pos;
    const float pos_dist = vec_normalized_dir_safe(to_pos, pos, start);
    const float direct_dist_sq = vec_dist_squared(direct_pos, start);
    const float direct_dist = sqrtf(direct_dist_sq);
    if (pos_dist < 5.0f) // m
    {
        return nullptr;
    }
    float radius = pos_dist * 0.5f;
    if (6.0f - radius < 0.0f)
    {
        radius = 6.0f;
    }
    float along = pos_dist - radius;
    if (direct_dist - along < 0.0f)
    {
        along = direct_dist;
    }
    const vector center = np->pos + to_pos * along;
    unsigned int handles[25];
    const int num = World.collect_objects<cover_node>(center, radius, handles, 25, nullptr, nullptr);
    cover_node* best = nullptr;
    bool best_offensive = false;
    float best_dist_sq = -1.0f;
    for (int i = 0; i < num; i++)
    {
        object* objp = handles[i] != 0xFFFFFFFF ? World.object_from_handle(handles[i]) : nullptr;
        if (objp == nullptr || objp->get_obj_type() != OT_COVER_NODE)
        {
            continue;
        }
        cover_node* node = static_cast<cover_node*>(objp);
        if (node->is_occupied(np) != nullptr || node->is_disabled() || node->flags.on_mover || node->flags.crouch || node->flags.fire_left == node->flags.fire_right)
        {
            continue;
        }
        const vector node_to_pos = pos - node->pos;
        const float pos_dist_sq = node_to_pos.mag_squared();
        if (pos_dist_sq < 2.25f) // 1.5 m
        {
            continue;
        }
        const vector start_to_node = node->pos - start;
        const float node_dist_sq = start_to_node.mag_squared();
        if (node_dist_sq < 4.0f || node_dist_sq > direct_dist_sq) // 2 m
        {
            continue;
        }
        const vector side = node->flags.fire_right ? node->orient.rvec : -node->orient.rvec;
        if ((direct_pos - node->pos) * side < 0.0f || side * start_to_node < 0.0f || node_to_pos * node->orient.fvec < 0.0f || start_to_node * node->orient.fvec < 0.0f)
        {
            continue;
        }
        const bool offensive = node->will_provide_offensive_cover(np, pos, false, true, true, nullptr);
        bool better;
        if (offensive)
        {
            better = !best_offensive;
        }
        else
        {
            if (best_offensive || side * node_to_pos < 0.0f)
            {
                continue;
            }
            better = best == nullptr;
        }
        if ((better || pos_dist_sq < best_dist_sq) && clear_line_of_sight_on_navmesh(direct_pos, ncp, node->closest_point) && clear_line_of_sight_on_navmesh(direct_pos, ncp, node->closest_point + side * 1.3f))
        {
            best_offensive = best_offensive || offensive;
            best_dist_sq = pos_dist_sq;
            best = node;
        }
    }
    return best;
}

// 0x005F1C20 (:891)
bool ai_search_adjust_direct_pos_from_cover_node(vector& adjusted_direct_pos, const npc*, const cover_node* c_node, const vector&, const vector&, const nav_cell*)
{
    const vector side = c_node->flags.fire_right ? c_node->orient.rvec : -c_node->orient.rvec;
    adjusted_direct_pos = c_node->pos + side * 2.0f;
    if (c_node->closest_nav_cell_handle == 0xFFFFFFFFFFFFFFFFull)
    {
        return false;
    }
    return bb_pf_get_nav_cell_from_hint(&adjusted_direct_pos, c_node->closest_nav_cell_handle, adjusted_direct_pos, false, nullptr, false, false, NAV_CELL_TRAVERSAL_ENABLED) != nullptr;
}
