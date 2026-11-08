// Original: volition/rfg/code/ai/guard_selection.cpp
//
// Group 0 holds free nodes, taken closest first. The other groups fill evenly: the least occupied groups come first, and
// within them the closest free node wins.

#include "volition/rfg/code/ai/guard_selection.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/sorted_list.h"
#include "volition/rfg/code/ai/sorted_list.inl"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_guard_node.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/util/varray.h"

#include <float.h>

static const unsigned char NO_GROUP = 0xFF;

// 0x005EAF30 (:31)
guard_node_group_list::guard_node_group_list()
{
    num_occupied_nodes = 0;
    closest_node_dist_sq = FLT_MAX;
    closest_node = nullptr;
    group_id = NO_GROUP;
}

// 0x00638D80 (:52): the free node of the group with the largest handle difference, a stable way to walk the group.
object_guard_node* next_node_in_group(unsigned int np_handle, const base_array<object_guard_node*>& nodes, const object_guard_node* cur_node)
{
    const unsigned char group = cur_node->get_group_id();
    const unsigned int cur_handle = cur_node->get_handle();
    unsigned int best_difference = 0;
    object_guard_node* best = nullptr;
    for (int i = 0; i < nodes.size(); i++)
    {
        object_guard_node* node = nodes[i];
        if (node != cur_node && node->get_group_id() == group && node->can_reserve(np_handle))
        {
            const unsigned int difference = cur_handle - node->get_handle();
            if (difference > best_difference)
            {
                best_difference = difference;
                best = node;
            }
        }
    }
    return best;
}

// 0x00661160 (:80). LTCG: guard_obj ECX.
static object_guard_node* pick_best_node(unsigned int np_handle, const vector& np_pos, const object* guard_obj, guard_node_selection_modes mode)
{
    farray<object_guard_node*, 256> nodes;
    guard_obj->collect_children(nodes, nullptr, nullptr);
    const int num_nodes = nodes.size();
    if (num_nodes < 1)
    {
        return nullptr;
    }

    object_guard_node* best = nullptr;
    float best_dist_sq = FLT_MAX;
    for (int i = 0; i < num_nodes; i++)
    {
        object_guard_node* node = nodes[i];
        if (node->get_group_id() == 0 && node->can_reserve(np_handle))
        {
            const float dist_sq = vec_dist_squared(np_pos, node->pos);
            if (dist_sq < best_dist_sq)
            {
                best = node;
                best_dist_sq = dist_sq;
            }
        }
    }
    if (best != nullptr)
    {
        return best;
    }

    sorted_list_element<const guard_node_group_list*, int> list_elements[16];
    sorted_list_base<const guard_node_group_list*, int> groups_by_occupancy(true, list_elements, 16);
    guard_node_group_list groups[256];
    unsigned char cur_group = NO_GROUP;
    object_guard_node* cur_node = nullptr;
    for (int i = 0; i < num_nodes; i++)
    {
        object_guard_node* node = nodes[i];
        const unsigned char id = node->get_group_id();
        guard_node_group_list& group = groups[id];
        if (node->is_reserved(np_handle))
        {
            group.group_id = id;
            cur_group = id;
            cur_node = node;
            if (mode == GN_SELECT_DIFFERENT_GROUP)
            {
                const float dist_sq = vec_dist_squared(np_pos, node->pos);
                if (dist_sq < group.closest_node_dist_sq)
                {
                    group.closest_node_dist_sq = dist_sq;
                    group.closest_node = node;
                }
            }
        }
        else if (node->can_reserve(np_handle))
        {
            const float dist_sq = vec_dist_squared(np_pos, node->pos);
            if (dist_sq < group.closest_node_dist_sq)
            {
                group.closest_node_dist_sq = dist_sq;
                group.closest_node = node;
                group.group_id = id;
            }
        }
        else if (node->get_reserving_npc() != nullptr)
        {
            group.num_occupied_nodes++;
            group.group_id = id;
        }
    }

    // Group 0 was handled above.
    for (int i = 1; i < 256; i++)
    {
        if (groups[i].group_id != NO_GROUP)
        {
            const guard_node_group_list* group = &groups[i];
            groups_by_occupancy.add(group, group->num_occupied_nodes);
        }
    }
    if (groups_by_occupancy.size() < 1)
    {
        return nullptr;
    }

    const int least_occupied = (*groups_by_occupancy.get_first())->num_occupied_nodes;
    if (cur_node != nullptr && groups[cur_group].num_occupied_nodes == least_occupied)
    {
        if (mode == GN_SELECT_PREFER_CURRENT_NODE)
        {
            return cur_node;
        }
        if (mode == GN_SELECT_NEW_NODE_SAME_GROUP)
        {
            return groups[cur_group].closest_node == nullptr ? cur_node : next_node_in_group(np_handle, nodes, cur_node);
        }
    }

    for (const guard_node_group_list* const* element = groups_by_occupancy.get_first(); element != nullptr; element = groups_by_occupancy.get_next(element))
    {
        const guard_node_group_list* group = *element;
        if (group->num_occupied_nodes > least_occupied)
        {
            return best;
        }
        if (mode == GN_SELECT_DIFFERENT_GROUP && cur_group == group->group_id)
        {
            // Leaving the current group: its node only when nothing else turned up, whatever its distance.
            if (best == nullptr)
            {
                best = group->closest_node;
            }
        }
        else if (group->closest_node_dist_sq < best_dist_sq)
        {
            best = group->closest_node;
            best_dist_sq = group->closest_node_dist_sq;
        }
    }
    return best;
}

// 0x005EAF60 (:215)
static bool guard_node_elevation_greater_or_equal(object_guard_node* const& node1, object_guard_node* const& node2)
{
    return node2->pos.y <= node1->pos.y;
}

// What rfg.exe inlines in both functions below: the object the squad's defend or roadblock orders are about.
static const object* get_guard_object(const npc* np)
{
    if (np == nullptr)
    {
        return nullptr;
    }
    const object_squad* squad = np->get_ai()->get_squad();
    if (squad == nullptr)
    {
        return nullptr;
    }
    gfm_orders* orders = squad->get_squad_orders();
    if (orders == nullptr || (orders->get_orders_type() != GFM_ORDERS_TYPE_DEFEND && orders->get_orders_type() != GFM_ORDERS_TYPE_ROADBLOCK))
    {
        return nullptr;
    }
    return orders->get_target_object();
}

// 0x006614E0 (:233); pick_best_node(np, guard_obj, mode) (0x006614C0, :210) inlined.
object_guard_node* guard_selection_choose_node(const npc* np, guard_node_selection_modes mode)
{
    const object* guard_obj = get_guard_object(np);
    if (guard_obj == nullptr)
    {
        return nullptr;
    }
    return pick_best_node(np->get_handle(), np->pos, guard_obj, mode);
}

// 0x00661550 (:252): a node within 1 m if there is one, otherwise any node within 1.5 m of the highest elevation.
object_guard_node* guard_selection_choose_sniper_node(const npc* np, const object_guard_node* ignore_node)
{
    const object* guard_obj = get_guard_object(np);
    if (guard_obj == nullptr)
    {
        return nullptr;
    }
    farray<object_guard_node*, 256> nodes;
    guard_obj->collect_children(nodes, nullptr, nullptr);
    if (nodes.size() < 1)
    {
        return nullptr;
    }
    const unsigned int np_handle = np->get_handle();
    for (int i = nodes.size() - 1; i >= 0; i--)
    {
        object_guard_node* node = nodes[i];
        if (node == ignore_node || !node->can_reserve(np_handle))
        {
            nodes.remove_unsorted(i);
        }
        else if (vec_dist_squared(node->pos, np->pos) < 1.0f)
        {
            return node;
        }
    }
    nodes.sort(guard_node_elevation_greater_or_equal);
    if (nodes.size() < 1)
    {
        return nullptr;
    }
    int count = 0;
    while (count < nodes.size() && nodes[0]->pos.y - nodes[count]->pos.y <= 1.5f)
    {
        count++;
    }
    return nodes[int_rand_range(0, count - 1)];
}
