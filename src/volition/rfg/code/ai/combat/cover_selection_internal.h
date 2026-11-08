// Original: volition/rfg/code/ai/combat/cover_selection_internal.h
// Implementation: volition/rfg/code/ai/combat/cover_selection_internal.cpp
//
// The cover search: search nodes on a heap and per-node cover data.
#pragma once

#include "volition/vlib/code/math/vector3.h"

class cover_node;
struct cover_node_data;
struct nav_cell;
class navpoint;

enum cover_search_node_types
{
    COVER_SEARCH_START = 0,
    COVER_SEARCH_NAV_CELL = 1,
    COVER_SEARCH_NAV_POINT = 2,
    COVER_SEARCH_COVER_NODE = 3,
    NUM_COVER_SEARCH_NODE_TYPES = 4,
};

// size 0x28
struct cover_search_node
{
    cover_search_node(); // 0x005E69F0 (.h:108)
    nav_cell* get_nav_cell(); // 0x005E6C80 (:72)
    navpoint* get_navpoint(); // 0x005E6C90 (:82)
    cover_node_data* get_cover_data(); // 0x005E6CA0 (:92)

    cover_search_node_types type; // 0x00
    void* ptr; // 0x04
    vector pos; // 0x08
    cover_search_node* prev_path_node; // 0x14
    float travel_cost; // 0x18
    float remaining_cost; // 0x1C
    float total_cost; // 0x20
    int heap_index; // 0x24
};

// size 0x57C
class cover_search_heap
{
public:
    void reset(); // 0x005E6F30 (:280)
    int size() const; // 0x005E6A00 (.h:188)
    void insert(cover_search_node* node); // 0x00629940 (:287)
    void percolate_up(cover_search_node* node); // 0x005E6F40 (:309)
    void percolate_down(cover_search_node* node); // 0x005E6F90 (:334)
    cover_search_node* get_best_node() const;
    cover_search_node* remove_best_node(); // 0x005E7040 (:384)

private:
    cover_search_node* heap_data[350]; // 0x00
    int heap_size; // 0x578
};

// size 0x10
struct cover_node_data
{
    // size 0x1
    struct cover_node_data_flags
    {
        unsigned char searched : 1; // 0x00:0
        unsigned char evaluated : 1; // 0x00:1
        unsigned char defense_only : 1; // 0x00:2
        unsigned char failed_evaluation : 1; // 0x00:3
    };

    navpoint* get_navpoint() const; // 0x005E69B0 (.h:69)
    cover_node* get_cover_node() const; // 0x005E69C0 (.h:70)
    void set_navpoint(navpoint* new_npp); // 0x005E69D0 (.h:71)
    void set_cover_node(cover_node* new_cnp); // 0x005E69E0 (.h:72)

private:
    cover_node* cnp; // 0x00
    navpoint* npp; // 0x04
public:
    float firing_cost; // 0x08
    short node_data_index; // 0x0C
    cover_node_data_flags flags; // 0x0E
};

// Magic numbers: the sizes of Cover_search_nodes and Cover_node_candidates.
static const int COVER_SEARCH_MAX_NODES = 350;
static const int COVER_SEARCH_MAX_CANDIDATES = 100;

extern cover_search_node Cover_search_nodes[COVER_SEARCH_MAX_NODES]; // 0x01DA0760
extern int Num_cover_search_nodes; // 0x01D67DC0
extern cover_search_heap Cover_search_heap; // 0x01D671A0
extern cover_node_data Cover_node_candidates[COVER_SEARCH_MAX_CANDIDATES]; // 0x01D67758
extern int Num_cover_node_candidates; // 0x01D67DC4
extern unsigned int Navcell_heap_session_id; // 0x01D65948
extern unsigned int Navpoint_heap_session_id; // 0x01D65214

void cover_search_reset_all(); // 0x00629900 (:43)
void cover_search_reset_candidates(); // 0x005E6C50 (:50)
void cover_search_begin(); // 0x00629910 (:55)
void cover_search_end(); // 0x005E6C60 (:63)
cover_search_node* get_new_cover_search_node(); // 0x005E6CB0 (:104)
int get_cover_search_node_index(const cover_search_node* node); // 0x005E6CF0 (:131)
cover_search_node* get_cover_search_node(const navpoint* npp); // 0x005E6D20 (:146)
cover_search_node* get_cover_search_node(const nav_cell* ncp); // 0x005E6D50 (:158)
cover_search_node* get_cover_search_node(const cover_node_data* cover); // 0x005E6D80 (:170)
void attach_cover_search_node(cover_search_node* node, navpoint* npp); // 0x005E6DA0 (:186)
void attach_cover_search_node(cover_search_node* node, nav_cell* ncp); // 0x005E6E00 (:204)
void attach_cover_search_node(cover_search_node* node, cover_node_data* cover); // 0x005E6E50 (:222)
cover_node_data* get_new_cover_node_candidate(); // 0x005E6E90 (:243)
int get_cover_node_candidate_index(const cover_node_data* data); // 0x005E6EC0 (:258)
void remove_cover_node_candidate(cover_node_data* candidate); // 0x005E6EE0 (:270)
