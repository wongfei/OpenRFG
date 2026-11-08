// Original: volition/rfg/code/ai/ai_building_logic.cpp
//
// Whole (NPC.md NP2). A building is an rfg_mover with navpoints among its children (is_ai_structure); five or more make
// it an is_ai_building. Pieces of a composite building are subpieces of a root found by walking the parents.

#include "volition/rfg/code/ai/ai_building_logic.h"

#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/objects/object_guard_node.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_restricted_area.h"
#include "volition/rfg/code/pathfinding/nav_heap.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/util/varray.h"

#include <float.h>
#include <math.h>

extern navpoint_heap Navpoint_heap; // nav_heap.cpp

// Magic number: the navpoint count that makes a structure a building, and the most the count looks for.
static const int AI_BUILDING_MIN_NAVPOINTS = 5;
// Magic number: what a link costs when it is disabled or, with allow_jump false, needs a jump.
static const float TRANSITION_BLOCKED_LINK_COST = 50.0f;

// Navpoint types (object_navpoint.cpp has the table). 19 and 20 are not in that table: the asm only allows 20 after 19.
static const signed char NAVPOINT_TYPE_PATH = 10;
static const signed char NAVPOINT_TYPE_TRANSITION = 11;
static const signed char NAVPOINT_TYPE_STAIRS = 14;

// 0x005DC220 (:129). LTCG: building is in ESI.
static bool get_ai_building_flags(rfg_mover* building)
{
    building->flags.is_ai_structure = 0;
    building->flags.is_ai_building = 0;
    building->flags.is_ai_subpiece = 0;
    building->flags.is_ai_composite = 0;
    const rfg_move_type move_type = building->get_move_type();
    if (move_type != 0 && move_type != 1)
    {
        return false;
    }
    if (building->is_killed())
    {
        return false;
    }
    object* const first = building->child_ptr;
    int navpoint_count = 0;
    object* child = first;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_NAVPOINT)
        {
            navpoint_count++;
            if (navpoint_count == AI_BUILDING_MIN_NAVPOINTS)
            {
                break;
            }
        }
        child = child->child_next;
        if (child == first)
        {
            break;
        }
    }
    if (navpoint_count == 0)
    {
        return false;
    }
    building->flags.is_ai_structure = 1;
    if (navpoint_count >= AI_BUILDING_MIN_NAVPOINTS)
    {
        building->flags.is_ai_building = 1;
    }
    return true;
}

// 0x005DC2A0 (:177). LTCG: building in EAX, area in ECX.
static void add_building_to_restricted_area(rfg_mover* building, object_restricted_area* area)
{
    building->restricted_area = area->get_handle();
    area->add_building(building->get_handle());
}

// 0x005DC2C0 (:188). LTCG: building in EAX, area in ECX.
static void remove_building_from_restricted_area(rfg_mover* building, object_restricted_area* area)
{
    building->restricted_area = 0xFFFFFFFF;
    area->remove_building(building->get_handle());
}

// 0x005FB430 (:200). LTCG: the outermost ancestor flagged as an AI structure, or building itself.
const rfg_mover* get_root_building_internal(const rfg_mover* building)
{
    const rfg_mover* root = building;
    for (;;)
    {
        if (building->flags.is_ai_structure != 0)
        {
            root = building;
        }
        if (building->get_parent() == 0xFFFFFFFF)
        {
            return root;
        }
        const object* parent = World.object_from_handle(building->get_parent());
        if (parent == nullptr || parent->get_obj_type() != OT_MOVER || parent->get_sub_type() != OT_SUB_MOVER_RFG)
        {
            return root;
        }
        building = static_cast<const rfg_mover*>(parent);
    }
}

// 0x005DC2E0 (:263). LTCG: start_npp in EAX, the rest on the stack. A cost-only search (no estimate) from start_npp for
// the first navpoint of a leaving type that is on the navmesh. aabb is in the PDB's parameter list and is not passed: the
// caller's bounding box goes unused (original).
static navpoint* ai_building_do_transition_search(navpoint* start_npp, bool allow_jump, unsigned int session_id)
{
    start_npp->heap_data.flags.is_open = 1;
    start_npp->heap_data.session_id = session_id;
    start_npp->heap_data.arrival_cost = 0.0f;
    start_npp->heap_data.est_cost_to_goal = 0.0f;
    Navpoint_heap.insert_cell(start_npp);
    while (Navpoint_heap.num_navpoints > 0)
    {
        navpoint* current = Navpoint_heap.remove_best_cell();
        switch (current->type)
        {
        case 11:
        case 12:
        case 15:
        case 16:
        case 21:
            if (current->nav_cell_ptr != nullptr)
            {
                return current;
            }
            break;
        default:
            break;
        }
        for (int i = 0; i < current->num_links; i++)
        {
            navpoint* next = current->links[i];
            const bool unvisited = next->heap_data.session_id != session_id;
            if (!unvisited && next->heap_data.flags.is_open == 0)
            {
                continue;
            }
            const signed char next_type = next->type;
            if (next_type == 20 && current->type != 19)
            {
                continue;
            }
            const bool blocked = (!Navpoint_treat_all_as_enabled && current->link_disabled_by[i] != 0) ||
                (!allow_jump && next_type > 14 && (next_type < 17 || next_type == 19));
            float cost = vec_dist(current->pos, next->pos) + current->heap_data.arrival_cost;
            if (blocked)
            {
                cost += TRANSITION_BLOCKED_LINK_COST;
            }
            if (unvisited)
            {
                next->heap_data.flags.is_open = 1;
                next->heap_data.arrival_cost = cost;
                next->heap_data.est_cost_to_goal = 0.0f;
                next->heap_data.session_id = session_id;
                Navpoint_heap.insert_cell(next);
            }
            else if (cost < next->heap_data.arrival_cost)
            {
                next->heap_data.arrival_cost = cost;
                Navpoint_heap.adjust_cell(next);
            }
        }
    }
    return nullptr;
}

// 0x005DC4C0 (:414): a usable link to a path navpoint or stairs.
static bool is_valid_transition_from_outside(const navpoint* trans_npp, navpoint*& next_npp)
{
    next_npp = nullptr;
    for (int i = 0; i < trans_npp->num_links; i++)
    {
        if (Navpoint_treat_all_as_enabled || trans_npp->link_disabled_by[i] == 0)
        {
            navpoint* other = trans_npp->links[i];
            if (other->type == NAVPOINT_TYPE_PATH || other->type == NAVPOINT_TYPE_STAIRS)
            {
                next_npp = other;
                return true;
            }
        }
    }
    return false;
}

// 0x005FB480 (:447)
void ai_building_process_streaming(rfg_mover* building, bool stream_in)
{
    if (building == nullptr)
    {
        return;
    }
    if (!stream_in)
    {
        if (building->restricted_area == 0xFFFFFFFF)
        {
            return;
        }
        object* area = World.object_from_handle(building->restricted_area);
        if (area == nullptr || area->get_obj_type() != OT_RESTRICTED_AREA)
        {
            return;
        }
        remove_building_from_restricted_area(building, static_cast<object_restricted_area*>(area));
        return;
    }
    if (!get_ai_building_flags(building))
    {
        return;
    }
    // The decompile reads the root through a const pointer; the asm writes its flag directly.
    rfg_mover* root = const_cast<rfg_mover*>(get_root_building_internal(building));
    if (root != nullptr && root != building)
    {
        root->flags.is_ai_composite = 1;
        building->flags.is_ai_subpiece = 1;
    }
    // Magic number: mover type bit 2, a building a restricted area holds whatever its team.
    if ((building->team != HUMAN_TEAM_EDF || (int)building->get_importance_level() < 1) && (building->mover_type_bit_flags & 4) == 0)
    {
        return;
    }
    object_restricted_area* area = restricted_area_find_area(building->pos, false, false);
    if (area != nullptr)
    {
        add_building_to_restricted_area(building, area);
    }
}

// 0x005DC530 (:495)
float ai_building_find_bbox_dist_squared(const vector& pos, const rfg_mover* building)
{
    vector local_pos = pos;
    vector bmin;
    vector bmax;
    building->get_local_bbox(bmin, bmax);
    vec_change_to_local_coords(local_pos, building->pos, building->orient);
    return bbox_find_distance_squared(bmin, bmax, local_pos);
}

// 0x005DC5A0 (:508)
bool ai_building_is_in_bbox(const vector& pos, const rfg_mover* building)
{
    vector bmin;
    vector bmax;
    building->get_local_bbox(bmin, bmax);
    return ix_point_in_box_oriented(pos, building->pos, building->orient, bmin, bmax);
}

// 0x005FB550 (:671)
const rfg_mover* ai_building_get_root_building(const rfg_mover* building)
{
    if (building != nullptr && building->flags.is_ai_subpiece != 0)
    {
        return get_root_building_internal(building);
    }
    return building;
}

// 0x0064BA40 (:531). A null building is read through (original). min_dist_sq ends as the closest box met so far, so with a
// full piece list it is not the true minimum.
bool ai_building_find_dist_squared(float& min_dist_sq, const vector& pos, const rfg_mover* building, float threshold)
{
    const float threshold_sq = threshold * threshold;
    const rfg_mover* root = ai_building_get_root_building(building);
    min_dist_sq = ai_building_find_bbox_dist_squared(pos, root);
    if (min_dist_sq <= threshold_sq)
    {
        return true;
    }
    if (root->flags.is_ai_composite == 0)
    {
        return false;
    }
    farray<const rfg_mover*, 200> pieces;
    pieces.add(root);
    for (int i = 0; i < pieces.size(); i++)
    {
        object* const first = pieces[i]->child_ptr;
        object* child = first;
        while (child != nullptr)
        {
            if (child->get_obj_type() == OT_MOVER && child->get_sub_type() == OT_SUB_MOVER_RFG)
            {
                const rfg_mover* piece = static_cast<const rfg_mover*>(child);
                if (piece->flags.is_ai_structure != 0)
                {
                    const float dist_sq = ai_building_find_bbox_dist_squared(pos, piece);
                    if (0.0f <= min_dist_sq - dist_sq)
                    {
                        min_dist_sq = dist_sq;
                    }
                    if (min_dist_sq <= threshold_sq)
                    {
                        return true;
                    }
                }
                if (pieces.full())
                {
                    return false;
                }
                pieces.add(piece);
            }
            child = child->child_next;
            if (child == first)
            {
                break;
            }
        }
    }
    return false;
}

// 0x0064BC90 (:607). A null building is read through (original).
bool ai_building_is_in_building(const vector& pos, const rfg_mover* building)
{
    const rfg_mover* root = ai_building_get_root_building(building);
    if (ai_building_is_in_bbox(pos, root))
    {
        return true;
    }
    if (root->flags.is_ai_composite == 0)
    {
        return false;
    }
    farray<const rfg_mover*, 200> pieces;
    pieces.add(root);
    for (int i = 0; i < pieces.size(); i++)
    {
        object* const first = pieces[i]->child_ptr;
        object* child = first;
        while (child != nullptr)
        {
            if (child->get_obj_type() == OT_MOVER && child->get_sub_type() == OT_SUB_MOVER_RFG)
            {
                const rfg_mover* piece = static_cast<const rfg_mover*>(child);
                if (piece->flags.is_ai_structure != 0 && ai_building_is_in_bbox(pos, piece))
                {
                    return true;
                }
                if (pieces.full())
                {
                    return false;
                }
                pieces.add(piece);
            }
            child = child->child_next;
            if (child == first)
            {
                break;
            }
        }
    }
    return false;
}

// 0x005DC5E0 (:693)
bool ai_building_object_belongs_to_building(const object* obj, const rfg_mover* building)
{
    while (obj != nullptr)
    {
        if (obj == building)
        {
            return true;
        }
        obj = World.object_from_handle(obj->get_parent());
    }
    return false;
}

// 0x0064BE20 (:715). Answers whether the list is full. The composite flag is read from structure, not from its root
// (original), and the root goes in without asking the callback.
bool ai_building_collect_substructures(base_array<rfg_mover*>& substructure_list, const rfg_mover* structure, bool (*cb_func)(object*, unsigned int, const void*), void* cb_data)
{
    if (substructure_list.full())
    {
        return true;
    }
    // The list holds non-const pointers, and rfg.exe adds the root to it as it is.
    rfg_mover* root = const_cast<rfg_mover*>(ai_building_get_root_building(structure));
    substructure_list.add(root);
    if (substructure_list.full())
    {
        return true;
    }
    if (structure->flags.is_ai_composite == 0)
    {
        return false;
    }
    farray<rfg_mover*, 200> pieces;
    pieces.add(root);
    for (int i = 0; i < pieces.size(); i++)
    {
        object* const first = pieces[i]->child_ptr;
        object* child = first;
        while (child != nullptr)
        {
            rfg_mover* piece = World.object_cast<rfg_mover>(child);
            if (piece != nullptr)
            {
                if (piece->flags.is_ai_structure != 0 && (cb_func == nullptr || cb_func(child, 0xFFFFFFFF, cb_data)))
                {
                    substructure_list.add(piece);
                    if (substructure_list.full())
                    {
                        return true;
                    }
                }
                if (pieces.full())
                {
                    return false;
                }
                pieces.add(piece);
            }
            child = child->child_next;
            if (child == first)
            {
                break;
            }
        }
    }
    return false;
}

static bool navpoint_is_collectable(const navpoint*)
{
    return true;
}

static bool guard_node_is_collectable(const object_guard_node* node)
{
    return !node->is_disabled();
}

static bool cover_node_is_collectable(const cover_node* node)
{
    return !node->is_disabled();
}

// The three collectors of rfg.exe are one loop apart from the type and the disabled check, written out three times there.
// Answers whether the list is full. With from_root false the walk still stops at structure's own pieces.
template<typename T>
static bool collect_building_children(base_array<T*>& list, const rfg_mover* structure, bool from_root, object_type type, bool (*usable)(const T*), bool (*cb_func)(object*, unsigned int, const void*), void* cb_data)
{
    if (list.full())
    {
        return true;
    }
    const rfg_mover* start = from_root ? ai_building_get_root_building(structure) : structure;
    farray<const rfg_mover*, 200> pieces;
    pieces.add(start);
    for (int i = 0; i < pieces.size(); i++)
    {
        object* const first = pieces[i]->child_ptr;
        object* child = first;
        while (child != nullptr)
        {
            if (child->get_obj_type() == type)
            {
                T* node = static_cast<T*>(child);
                if (usable(node) && (cb_func == nullptr || cb_func(child, 0xFFFFFFFF, cb_data)))
                {
                    list.add(node);
                    if (list.full())
                    {
                        return true;
                    }
                }
            }
            else if (start->flags.is_ai_composite != 0 && child->get_sub_type() == OT_SUB_MOVER_RFG)
            {
                // The type is not checked here (original): any child with this sub type is walked as a mover.
                if (pieces.full())
                {
                    return false;
                }
                pieces.add(static_cast<const rfg_mover*>(child));
            }
            child = child->child_next;
            if (child == first)
            {
                break;
            }
        }
    }
    return false;
}

// 0x0064BFA0 (:782)
bool ai_building_collect_navpoints(base_array<navpoint*>& navpoint_list, const rfg_mover* structure, bool from_root, bool (*cb_func)(object*, unsigned int, const void*), void* cb_data)
{
    return collect_building_children<navpoint>(navpoint_list, structure, from_root, OT_NAVPOINT, navpoint_is_collectable, cb_func, cb_data);
}

// 0x0064C0E0 (:841)
bool ai_building_collect_guard_nodes(base_array<object_guard_node*>& guard_node_list, const rfg_mover* structure, bool from_root, bool (*cb_func)(object*, unsigned int, const void*), void* cb_data)
{
    return collect_building_children<object_guard_node>(guard_node_list, structure, from_root, OT_GUARD_NODE, guard_node_is_collectable, cb_func, cb_data);
}

// 0x0064C230 (:901)
bool ai_building_collect_cover_nodes(base_array<cover_node*>& cover_node_list, const rfg_mover* structure, bool from_root, bool (*cb_func)(object*, unsigned int, const void*), void* cb_data)
{
    return collect_building_children<cover_node>(cover_node_list, structure, from_root, OT_COVER_NODE, cover_node_is_collectable, cb_func, cb_data);
}

// 0x0064C380 (:964). Over the building's own navpoints: the closest on the floor, within 5 m of its edge; stairs
// (type 14) count from their lower half, within 1 m. Types 10 to 13 are path, transition, ladder transition and ladder.
navpoint* ai_building_find_closest_navpoint(const vector& pos, const rfg_mover* structure, bool from_root, float* actual_xz_dist_sq)
{
    navpoint* closest = nullptr;
    float closest_dist = FLT_MAX;
    farray<const rfg_mover*, 200> buildings;
    buildings.add(from_root && structure != nullptr && structure->flags.is_ai_subpiece != 0 ? get_root_building_internal(structure) : structure);
    for (int i = 0; i < buildings.size(); i++)
    {
        const object* first = buildings[i]->child_ptr;
        object* child = buildings[i]->child_ptr;
        while (child != nullptr)
        {
            if (child->get_obj_type() == OT_NAVPOINT)
            {
                navpoint* npp = static_cast<navpoint*>(child);
                float dist = FLT_MAX;
                float limit = closest_dist;
                if (10 <= npp->type && npp->type <= 13)
                {
                    if (fabsf(pos.y - npp->pos.y) <= 1.5f)
                    {
                        dist = vec_xz_dist(npp->pos, pos) - npp->radius;
                        if (5.0f < limit)
                        {
                            limit = 5.0f;
                        }
                    }
                }
                else if (npp->type == 14)
                {
                    for (int link = 0; link < npp->num_links; link++)
                    {
                        const navpoint* other = npp->links[link];
                        if (other->type == 14)
                        {
                            const vector middle = (npp->pos + other->pos) * 0.5f;
                            vector on_stairs;
                            ix_closest_point_on_line_segment(on_stairs, pos, npp->pos, middle);
                            if (fabsf(pos.y - on_stairs.y) <= 0.5f)
                            {
                                dist = vec_xz_dist(on_stairs, pos) - npp->radius;
                                if (1.0f < limit)
                                {
                                    limit = 1.0f;
                                }
                            }
                            break;
                        }
                    }
                }
                if (dist < limit)
                {
                    closest_dist = dist;
                    closest = npp;
                }
            }
            else if (buildings[0]->flags.is_ai_composite != 0 && child->get_sub_type() == OT_SUB_MOVER_RFG)
            {
                if (buildings.full())
                {
                    return nullptr;
                }
                buildings.add(static_cast<const rfg_mover*>(child));
            }
            child = child->child_next;
            if (child == first)
            {
                break;
            }
        }
    }
    if (actual_xz_dist_sq != nullptr)
    {
        const float clamped = closest_dist > 0.0f ? closest_dist : 0.0f;
        *actual_xz_dist_sq = clamped * clamped;
    }
    return closest;
}

// 0x0064C730 (:1075). A null structure is read through (original). The bounding box rfg.exe fetches here is passed to no
// one (see ai_building_do_transition_search), so only the virtual call goes.
navpoint* ai_building_find_closest_transition_from_inside(const vector& pos, const rfg_mover* structure, bool allow_jump)
{
    const rfg_mover* root = ai_building_get_root_building(structure);
    navpoint* start = ai_building_find_closest_navpoint(pos, root, true, nullptr);
    if (start == nullptr)
    {
        return nullptr;
    }
    const unsigned int session_id = Navpoint_heap.start_session();
    navpoint* found = ai_building_do_transition_search(start, allow_jump, session_id);
    Navpoint_heap.end_session();
    return found;
}

// 0x0064C7C0 (:1121)
navpoint* ai_building_find_closest_transition_from_outside(const vector& pos, const rfg_mover* structure, navpoint** next_navp)
{
    const rfg_mover* root = ai_building_get_root_building(structure);
    navpoint* closest = nullptr;
    navpoint* closest_next = nullptr;
    float closest_dist_sq = -1.0f;
    farray<const rfg_mover*, 200> pieces;
    pieces.add(root);
    for (int i = 0; i < pieces.size(); i++)
    {
        object* const first = pieces[i]->child_ptr;
        object* child = first;
        while (child != nullptr)
        {
            if (child->get_obj_type() == OT_NAVPOINT)
            {
                navpoint* npp = static_cast<navpoint*>(child);
                navpoint* next = nullptr;
                if (npp->type == NAVPOINT_TYPE_TRANSITION && is_valid_transition_from_outside(npp, next))
                {
                    const float dist_sq = vec_dist_squared(pos, npp->pos);
                    if (closest == nullptr || dist_sq < closest_dist_sq)
                    {
                        closest_next = next;
                        closest = npp;
                        closest_dist_sq = dist_sq;
                    }
                }
            }
            else if (root->flags.is_ai_composite != 0 && child->get_sub_type() == OT_SUB_MOVER_RFG)
            {
                if (pieces.full())
                {
                    return nullptr;
                }
                pieces.add(static_cast<const rfg_mover*>(child));
            }
            child = child->child_next;
            if (child == first)
            {
                break;
            }
        }
    }
    if (next_navp != nullptr)
    {
        *next_navp = closest_next;
    }
    return closest;
}
