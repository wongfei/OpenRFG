// Original: volition/rfg/code/pathfinding/nav_heap.h
// Implementation: volition/rfg/code/pathfinding/nav_heap.cpp
//
// The open lists of the A* searches over nav cells (nav_heap, with per-cell heap data allocated for the loaded cells)
// and navpoints (navpoint_heap, whose state lives in each navpoint as navpoint_heap_data). A search runs in a session;
// data from an older session counts as unvisited.
#pragma once

#include "volition/vlib/code/math/vector3.h"

struct nav_cell;
class navpoint;

// size 0x10
struct navpoint_heap_data
{
    float arrival_cost; // 0x00
    float est_cost_to_goal; // 0x04
    unsigned int session_id; // 0x08
    unsigned short heap_index; // 0x0C
    struct
    {
        unsigned char is_open : 1; // 0x00:0
        unsigned char on_path : 1; // 0x00:1
    } flags; // 0x0E
    unsigned char disabled_by; // 0x0F
};

// size 0x18
struct nav_heap_cell_data
{
    float arrival_cost; // 0x00
    float est_cost_to_goal; // 0x04
    vector arrival_pos; // 0x08
    char arrival_edge; // 0x14
    bool is_open; // 0x15
    unsigned short heap_index; // 0x16
};

// size 0x24
class nav_heap
{
    nav_cell** get_entry_from_index(int index); // 0x00AE9F50 (.h:59)
    float get_pathfinding_cost(const nav_cell* ncp) const; // 0x00AE9F70 (.h:65)
public:
    void init(); // 0x00AF3110 (:184)
    unsigned int start_session(); // 0x00AF97B0 (:201)
    void end_session(); // 0x00AF3120 (:247)
    bool insert_cell(nav_cell* ncp, float arrival_cost, float est_cost_to_goal, char arrival_edge, const vector& arrival_pos); // 0x00AF9870 (:270)
    void adjust_cell(int heap_index); // 0x00AF9900 (:309)
    nav_cell* remove_best_cell(); // 0x00AF9910 (:320)
    void heapify(int index); // 0x00AF3150 (:357)
    void percolate(int index); // 0x00AF32E0 (:432)
    nav_heap_cell_data* get_cell_data(const nav_cell* ncp); // 0x005DB9A0 (.h:90)

    bool in_session; // 0x00
    unsigned int session_id; // 0x04
    int num_allocated_heap_data; // 0x08
    nav_heap_cell_data* heap_data; // 0x0C
    int num_nav_cells; // 0x10
    nav_cell** nav_cells; // 0x14
    int max_size; // 0x18
    unsigned int mark; // 0x1C
    unsigned int post_alloc_mark; // 0x20
};

// size 0x80C
class navpoint_heap
{
public:
    void init(); // 0x00AF3450 (:499)
    unsigned int start_session(); // 0x00AF3470 (:510)
    void end_session(); // 0x00AF34A0 (:529)
    void insert_cell(navpoint* navp); // 0x00AF9990 (:540)
    void adjust_cell(navpoint* navp); // 0x00AF99D0 (:560)
    navpoint* remove_best_cell(); // 0x00AF99F0 (:570)
    void heapify(int index); // 0x00AF34B0 (:600)
    void percolate(int index); // 0x00AF35B0 (:669)
private:
    navpoint** get_entry_from_index(int index, bool allow_out_of_range); // 0x00AF3430 (:488)
    void validate(int index, navpoint* parent); // 0x00AF3670 (:716)

public:
    navpoint* heap[512]; // 0x00
    int num_navpoints; // 0x800
    bool in_session; // 0x804
    unsigned int session_id; // 0x808
};

void nav_heap_level_init_pre(); // 0x00AF36D0 (:744)
