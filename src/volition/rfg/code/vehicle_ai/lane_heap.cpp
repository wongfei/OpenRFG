// Original: volition/rfg/code/vehicle_ai/lane_heap.cpp
//
// The open list of the lane search (NPC.md NP5): a binary min-heap of lane_search_node, keyed on arrival cost plus the
// estimate to the goal, with each node holding its heap index so a cheaper arrival can move it up.

#include "volition/rfg/code/vehicle_ai/lane_heap.h"

lane_heap Lane_heap; // 0x03871108

// 0x00BC3720 (:85)
float lane_pathfinding_cost(const lane_search_node* node)
{
    return node->heap_data.est_cost_to_goal + node->heap_data.arrival_cost;
}

// 0x00BC3740 (:103)
lane_search_node** lane_heap::get_entry_from_index(int index, bool allow_out_of_range)
{
    if (!allow_out_of_range && num_nodes <= index)
    {
        return nullptr;
    }
    return nodes + index;
}

// 0x00BC3760 (:258)
void lane_heap::add_buffer(lane_search_node** _nodes, int _max_nodes)
{
    max_nodes = _max_nodes;
    nodes = _nodes;
}

// 0x00BC3770 (:270)
void lane_heap::remove_buffer()
{
    max_nodes = 0;
    nodes = nullptr;
}

// 0x00BC3780 (:280)
void lane_heap::init()
{
    num_nodes = 0;
    in_session = false;
    session_id = 0;
}

// 0x00BC3790 (:289)
void lane_heap::start_session()
{
    session_id++;
    num_nodes = 0;
    in_session = true;
}

// 0x00BC37B0 (:301)
void lane_heap::end_session()
{
    in_session = false;
}

// 0x00BC8F90 (:312)
void lane_heap::insert_node(lane_search_node* ncp)
{
    nodes[num_nodes] = ncp;
    ncp->heap_data.heap_index = num_nodes;
    num_nodes++;
    percolate(num_nodes - 1);
}

// 0x00BC8FC0 (:332)
void lane_heap::adjust_node(lane_search_node* ncp)
{
    percolate(ncp->heap_data.heap_index);
}

// 0x00BC8FD0 (:342)
lane_search_node* lane_heap::remove_best_node()
{
    if (num_nodes <= 0 || nodes == nullptr)
    {
        return nullptr;
    }
    lane_search_node* best = nodes[0];
    lane_search_node* last = *get_entry_from_index(num_nodes - 1, false);
    nodes[0] = last;
    last->heap_data.heap_index = 0;
    num_nodes--;
    heapify(0);
    return best;
}

// 0x00BC37C0 (:372)
void lane_heap::heapify(int index)
{
    if (index >= num_nodes)
    {
        return;
    }
    while (true)
    {
        const int left = index * 2 + 1;
        const int right = index * 2 + 2;
        int smallest = index;
        float smallest_cost = lane_pathfinding_cost(nodes[index]);
        if (left < num_nodes && lane_pathfinding_cost(nodes[left]) < smallest_cost)
        {
            smallest = left;
            smallest_cost = lane_pathfinding_cost(nodes[left]);
        }
        if (right < num_nodes && lane_pathfinding_cost(nodes[right]) < smallest_cost)
        {
            smallest = right;
        }
        if (smallest == index)
        {
            return;
        }
        lane_search_node* moved_down = nodes[index];
        lane_search_node* moved_up = nodes[smallest];
        nodes[smallest] = moved_down;
        moved_down->heap_data.heap_index = smallest;
        nodes[index] = moved_up;
        moved_up->heap_data.heap_index = index;
        index = smallest;
    }
}

// 0x00BC38C0 (:443)
void lane_heap::percolate(int index)
{
    if (index >= num_nodes)
    {
        return;
    }
    lane_search_node* node = nodes[index];
    const float cost = lane_pathfinding_cost(node);
    // Original: (index - 1) >> 1 at index 0 is -1, which only forms a pointer; the index test stops it being read.
    int parent = (index - 1) >> 1;
    while (index > 0 && cost < lane_pathfinding_cost(nodes[parent]))
    {
        nodes[index] = nodes[parent];
        nodes[index]->heap_data.heap_index = index;
        index = parent;
        parent = (index - 1) >> 1;
    }
    nodes[index] = node;
    node->heap_data.heap_index = index;
    heapify(index);
}

// 0x00BC3970 (:492)
void lane_heap_city_init()
{
    Lane_heap.init();
}

// 0x00BC3990 (:497)
void lane_heap_add_buffer(lane_search_node** _nodes, int _max_nodes)
{
    Lane_heap.add_buffer(_nodes, _max_nodes);
}
