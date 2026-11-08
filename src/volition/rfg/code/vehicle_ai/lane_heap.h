// Original: volition/rfg/code/vehicle_ai/lane_heap.h
// Implementation: volition/rfg/code/vehicle_ai/lane_heap.cpp
//
// The open list of the A* search over traffic lane splines, with per-lane heap data.
#pragma once

#include "volition/rfg/code/traffic/traffic_shared.h"
#include "volition/vlib/code/util/endian_type.h"

// size 0x14
struct _heap_data
{
    float arrival_cost; // 0x00
    float est_cost_to_goal; // 0x04
    int session_id; // 0x08
    int heap_index; // 0x0C
    struct
    {
        unsigned char is_open : 1; // 0x00:0
        unsigned char opposing : 1; // 0x00:1
        unsigned char adjacent : 1; // 0x00:2
    } flags; // 0x10
};

// size 0x28
struct lane_search_node
{
    et_ptr_offset<lane_spline,0> lane; // 0x00
    et_ptr_offset<lane_search_node,0> parent; // 0x04
    short num_forward_links; // 0x08
    short num_backward_links; // 0x0A
    short left_link_index; // 0x0C
    short right_link_index; // 0x0E
    et_ptr_offset<et_ptr_offset<lane_search_node,0>,0> links; // 0x10
    _heap_data heap_data; // 0x14
};

// size 0x14
class lane_heap
{
    lane_search_node** get_entry_from_index(int index, bool allow_out_of_range); // 0x00BC3740 (:103)
public:
    void init(); // 0x00BC3780 (:280)
    void start_session(); // 0x00BC3790 (:289)
    void end_session(); // 0x00BC37B0 (:301)
    void add_buffer(lane_search_node** _nodes, int _max_nodes); // 0x00BC3760 (:258)
    void remove_buffer(); // 0x00BC3770 (:270)
    void insert_node(lane_search_node* ncp); // 0x00BC8F90 (:312)
    void adjust_node(lane_search_node* ncp); // 0x00BC8FC0 (:332)
    lane_search_node* remove_best_node(); // 0x00BC8FD0 (:342)
    void heapify(int index); // 0x00BC37C0 (:372)
    void percolate(int index); // 0x00BC38C0 (:443)

    lane_search_node** nodes; // 0x00
    int num_nodes; // 0x04
    int max_nodes; // 0x08
    bool in_session; // 0x0C
    int session_id; // 0x10
};

extern lane_heap Lane_heap; // 0x03871108

float lane_pathfinding_cost(const lane_search_node* node); // 0x00BC3720 (:85)
void lane_heap_city_init(); // 0x00BC3970 (:492)
void lane_heap_add_buffer(lane_search_node** _nodes, int _max_nodes); // 0x00BC3990 (:497)
