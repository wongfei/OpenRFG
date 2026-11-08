// Original: volition/rfg/code/pathfinding/road_heap.h
// Implementation: volition/rfg/code/pathfinding/road_heap.cpp
//
// The open list of the A* search over road spline points (road routes), with per-point data (costs, the spline and
// point it was reached from, open flag and heap index).
#pragma once


class object_path_road;
struct road_spline_identifier;

// size 0x14
struct road_heap_data
{
    float arrival_cost; // 0x00
    float est_cost_to_goal; // 0x04
    unsigned int arrival_spline; // 0x08
    unsigned short arrival_point; // 0x0C
    bool is_open; // 0x0E
    unsigned short heap_index; // 0x10
};

// size 0x24
class road_heap
{
    road_spline_identifier* get_entry_from_index(int index); // 0x00AE9FA0 (.h:85)
    float get_pathfinding_cost(const road_spline_identifier* rsip) const; // 0x00AE9FC0 (.h:102)
public:
    void init(); // 0x00AF3700 (:93)
    unsigned int start_session(); // 0x00AF9A30 (:110)
    void end_session(); // 0x00AF3710 (:156)
    bool insert_road_spline(object_path_road* road_spline, unsigned short point, object_path_road* arrival_spline, float arrival_cost, float est_cost_to_goal, unsigned short arrival_point); // 0x00AF9AF0 (:402)
    void adjust_road_spline(int heap_index); // 0x00AF9BB0 (:447)
    road_spline_identifier remove_best_road_spline(); // 0x00AF9BC0 (:458)
    void heapify(int index); // 0x00AF3740 (:498)
    void percolate(int index); // 0x00AF3920 (:569)
    road_heap_data* get_road_spline_data(const road_spline_identifier* rsip); // 0x005DFB30 (.h:140)

    bool in_session; // 0x00
    unsigned int session_id; // 0x04
    int num_allocated_heap_data; // 0x08
    road_heap_data* heap_data; // 0x0C
    int num_road_splines; // 0x10
    road_spline_identifier* rsi; // 0x14
    int max_size; // 0x18
    unsigned int mark; // 0x1C
    unsigned int post_alloc_mark; // 0x20
};

void road_heap_level_init_pre(); // 0x00AF3AB0 (:624)

extern road_heap Road_heap; // 0x035B6058
