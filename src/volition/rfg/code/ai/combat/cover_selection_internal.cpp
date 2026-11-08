// Original: volition/rfg/code/ai/combat/cover_selection_internal.cpp

#include "volition/rfg/code/ai/combat/cover_selection_internal.h"

#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/pathfinding/nav_heap.h"

#include <stdint.h>

extern nav_heap Nav_heap; // nav_heap.cpp
extern navpoint_heap Navpoint_heap; // nav_heap.cpp

cover_search_node Cover_search_nodes[COVER_SEARCH_MAX_NODES]; // 0x01DA0760
int Num_cover_search_nodes; // 0x01D67DC0
cover_search_heap Cover_search_heap; // 0x01D671A0
cover_node_data Cover_node_candidates[COVER_SEARCH_MAX_CANDIDATES]; // 0x01D67758
int Num_cover_node_candidates; // 0x01D67DC4
unsigned int Navcell_heap_session_id; // 0x01D65948
unsigned int Navpoint_heap_session_id; // 0x01D65214

// 0x005E69F0 (.h:108)
cover_search_node::cover_search_node()
{
    ptr = nullptr;
}

// 0x005E69B0 (.h:69)
navpoint* cover_node_data::get_navpoint() const
{
    return npp;
}

// 0x005E69C0 (.h:70)
cover_node* cover_node_data::get_cover_node() const
{
    return cnp;
}

// 0x005E69D0 (.h:71)
void cover_node_data::set_navpoint(navpoint* new_npp)
{
    npp = new_npp;
}

// 0x005E69E0 (.h:72)
void cover_node_data::set_cover_node(cover_node* new_cnp)
{
    cnp = new_cnp;
}

// 0x00629900 (:43)
void cover_search_reset_all()
{
    Num_cover_search_nodes = 0;
    Cover_search_heap.reset();
}

// 0x005E6C50 (:50)
void cover_search_reset_candidates()
{
    Num_cover_node_candidates = 0;
}

// 0x00629910 (:55)
void cover_search_begin()
{
    Num_cover_search_nodes = 0;
    Cover_search_heap.reset();
    Navcell_heap_session_id = Nav_heap.start_session();
    Navpoint_heap_session_id = Navpoint_heap.start_session();
}

// 0x005E6C60 (:63)
void cover_search_end()
{
    Nav_heap.end_session();
    Navpoint_heap.end_session();
}

// 0x005E6C80 (:72)
nav_cell* cover_search_node::get_nav_cell()
{
    return type == COVER_SEARCH_NAV_CELL ? (nav_cell*)ptr : nullptr;
}

// 0x005E6C90 (:82)
navpoint* cover_search_node::get_navpoint()
{
    return type == COVER_SEARCH_NAV_POINT ? (navpoint*)ptr : nullptr;
}

// 0x005E6CA0 (:92)
cover_node_data* cover_search_node::get_cover_data()
{
    return type == COVER_SEARCH_COVER_NODE ? (cover_node_data*)ptr : nullptr;
}

// 0x005E6CB0 (:104)
cover_search_node* get_new_cover_search_node()
{
    if (Num_cover_search_nodes >= COVER_SEARCH_MAX_NODES)
    {
        return nullptr;
    }
    cover_search_node* node = &Cover_search_nodes[Num_cover_search_nodes];
    ++Num_cover_search_nodes;
    node->heap_index = -1;
    node->ptr = nullptr;
    return node;
}

// 0x005E6CF0 (:131)
int get_cover_search_node_index(const cover_search_node* node)
{
    if (node != nullptr)
    {
        const unsigned int index = (unsigned int)(node - Cover_search_nodes);
        if (index < COVER_SEARCH_MAX_NODES)
        {
            return (int)index;
        }
    }
    return -1;
}

// 0x005E6D20 (:146)
cover_search_node* get_cover_search_node(const navpoint* npp)
{
    if (npp != nullptr && npp->heap_data.session_id == Navpoint_heap_session_id)
    {
        return &Cover_search_nodes[npp->heap_data.heap_index];
    }
    return nullptr;
}

// 0x005E6D50 (:158)
cover_search_node* get_cover_search_node(const nav_cell* ncp)
{
    if (ncp != nullptr && ncp->heap_session_id == Navcell_heap_session_id)
    {
        return &Cover_search_nodes[ncp->heap_data_index];
    }
    return nullptr;
}

// 0x005E6D80 (:170)
cover_search_node* get_cover_search_node(const cover_node_data* cover)
{
    if (cover != nullptr && cover->flags.searched != 0)
    {
        return &Cover_search_nodes[cover->node_data_index];
    }
    return nullptr;
}

// 0x005E6DA0 (:186)
void attach_cover_search_node(cover_search_node* node, navpoint* npp)
{
    node->type = COVER_SEARCH_NAV_POINT;
    node->ptr = npp;
    npp->heap_data.heap_index = (unsigned short)get_cover_search_node_index(node);
    npp->heap_data.session_id = Navpoint_heap_session_id;
}

// 0x005E6E00 (:204)
void attach_cover_search_node(cover_search_node* node, nav_cell* ncp)
{
    node->type = COVER_SEARCH_NAV_CELL;
    node->ptr = ncp;
    ncp->heap_data_index = (unsigned short)get_cover_search_node_index(node);
    ncp->heap_session_id = Navcell_heap_session_id;
}

// 0x005E6E50 (:222)
void attach_cover_search_node(cover_search_node* node, cover_node_data* cover)
{
    node->type = COVER_SEARCH_COVER_NODE;
    node->ptr = cover;
    cover->node_data_index = (short)get_cover_search_node_index(node);
}

// 0x005E6E90 (:243)
cover_node_data* get_new_cover_node_candidate()
{
    if (Num_cover_node_candidates >= COVER_SEARCH_MAX_CANDIDATES)
    {
        return nullptr;
    }
    cover_node_data* data = &Cover_node_candidates[Num_cover_node_candidates];
    ++Num_cover_node_candidates;
    data->flags = cover_node_data::cover_node_data_flags();
    data->node_data_index = -1;
    return data;
}

// 0x005E6EC0 (:258)
int get_cover_node_candidate_index(const cover_node_data* data)
{
    if (data != nullptr)
    {
        const unsigned int index = (unsigned int)(data - Cover_node_candidates);
        if (index < COVER_SEARCH_MAX_CANDIDATES)
        {
            return (int)index;
        }
    }
    return -1;
}

// 0x005E6EE0 (:270)
void remove_cover_node_candidate(cover_node_data* candidate)
{
    const int index = get_cover_node_candidate_index(candidate);
    if (index >= 0)
    {
        --Num_cover_node_candidates;
        Cover_node_candidates[index] = Cover_node_candidates[Num_cover_node_candidates];
    }
}

// 0x005E6F30 (:280)
void cover_search_heap::reset()
{
    heap_size = 0;
}

// 0x005E6A00 (.h:188)
int cover_search_heap::size() const
{
    return heap_size;
}

// 0x00629940 (:287)
void cover_search_heap::insert(cover_search_node* node)
{
    node->heap_index = heap_size;
    heap_data[heap_size] = node;
    ++heap_size;
    percolate_up(node);
}

// 0x005E6F40 (:309)
void cover_search_heap::percolate_up(cover_search_node* node)
{
    while (node->heap_index >= 1)
    {
        const int index = node->heap_index;
        const int parent_index = (index - 1) / 2;
        cover_search_node* parent = heap_data[parent_index];
        if (parent->total_cost <= node->total_cost)
        {
            return;
        }
        heap_data[parent_index] = node;
        node->heap_index = parent_index;
        heap_data[index] = parent;
        parent->heap_index = index;
    }
}

// 0x005E6F90 (:334)
void cover_search_heap::percolate_down(cover_search_node* node)
{
    int index = node->heap_index;
    int child_index = index * 2 + 1;
    if (child_index >= heap_size)
    {
        return;
    }
    while (index * 2 + 2 < heap_size)
    {
        cover_search_node* child = heap_data[child_index];
        if (heap_data[index * 2 + 2]->total_cost <= heap_data[child_index]->total_cost)
        {
            child = heap_data[index * 2 + 2];
        }
        if (node->total_cost <= child->total_cost)
        {
            return;
        }
        const int new_index = child->heap_index;
        heap_data[new_index] = node;
        node->heap_index = new_index;
        heap_data[index] = child;
        child->heap_index = index;
        index = node->heap_index;
        child_index = index * 2 + 1;
        if (heap_size <= child_index)
        {
            return;
        }
    }
    cover_search_node* child = heap_data[child_index];
    if (child->total_cost < node->total_cost)
    {
        heap_data[child_index] = node;
        node->heap_index = child_index;
        heap_data[index] = child;
        child->heap_index = index;
    }
}

// 0x005E7040 (:384)
cover_search_node* cover_search_heap::remove_best_node()
{
    if (heap_size < 1)
    {
        return nullptr;
    }
    cover_search_node* best = heap_data[0];
    best->heap_index = -1;
    --heap_size;
    cover_search_node* last = heap_data[heap_size];
    heap_data[0] = last;
    // Original bug kept: when the heap had one node, last is best, so the removed node reads as still open (index 0).
    // add_navcell and friends then treat it as a candidate for a cheaper path instead of as closed.
    last->heap_index = 0;
    percolate_down(last);
    return best;
}
