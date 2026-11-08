// Original: volition/rfg/code/ai/combat/cover_selection.h
// Implementation: volition/rfg/code/ai/combat/cover_selection.cpp
//
// Cover selection: searching cover nodes and nav cell positions around an npc for the best cover against a threat
// (parameters, travel costs, obstruction tests).
#pragma once

#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/varray.h"

class cover_node;
struct cover_node_data;
struct cover_search_node;
struct nav_cell;
class navpoint;
class npc;
class object;

// size 0x94
struct cover_search_parameters
{
    cover_search_parameters(const npc* attacker, const vector& target_pos_, float max_dist_, bool defensive, const cover_node* ignore_node_, bool has_to_be_closer); // 0x00679120 (:1213)
    cover_search_parameters(const npc* attacker, const object* target_, float max_dist_, bool defensive, const cover_node* ignore_node_, bool has_to_be_closer); // 0x00678EE0 (:1173)
    cover_search_parameters(); // 0x0064DE60 (.h:22)
    void preprocess(); // 0x00629770 (:1247)
    ~cover_search_parameters(); // 0x006570F0
    cover_search_parameters& operator=(const cover_search_parameters& __that); // 0x006B6580

    const npc* np; // 0x00
    const object* target; // 0x04
    const cover_node* ignore_node; // 0x08
    bool allow_defensive_cover; // 0x0C
    bool find_closer_node; // 0x0D
    const object* defend_object; // 0x10
    float min_fire_range_sq; // 0x14
    float max_fire_range_sq; // 0x18
    float max_collect_dist; // 0x1C
    float max_travel_cost; // 0x20
    farray<vector,5> target_positions; // 0x24
    float range_penalty; // 0x70
    float avoid_dist; // 0x74
    vector avg_target_pos; // 0x78
    vector target_fvec; // 0x84
    float initial_target_dist; // 0x90
};

// size 0x98
class cover_search_object
{
public:
    cover_search_object(); // 0x00679360 (:1278)
    void reset(); // 0x0065B0B0 (:1285)
    cover_node* do_search(const cover_search_parameters& params_); // 0x006BC740 (:1297)
    cover_node* get_best_cover_node() const; // 0x006298E0 (:1342)
private:
    bool get_candidate_cover_nodes(); // 0x0065A6F0 (:289)
    bool do_secondary_cover_evaluation(cover_node_data* cover_data, const cover_node* node); // 0x0069B430 (:444)
    void explore(cover_search_node* node); // 0x006B6420 (:907)
    void explore_navcell(cover_search_node* node, nav_cell* ncp); // 0x0069B710 (:933)
    void explore_navpoint(cover_search_node* node, navpoint* npp); // 0x0069B910 (:1003)
    bool add_navcell(cover_search_node* old_node, nav_cell* ncp, const vector& cell_entry_point); // 0x0065AB90 (:533)
    bool add_navpoint(cover_search_node* old_node, navpoint* npp); // 0x0065AD20 (:624)
    bool add_cover_node(cover_search_node* old_node, cover_node_data* cover_data, const cover_node* cover); // 0x0065AED0 (:713)
    cover_search_node* get_navmesh_shortcut(const vector& pos, cover_search_node* old_node) const; // 0x006296C0 (:808)
    float get_travel_cost(const vector& start, const vector& end, float travel_dist) const; // 0x005E6A50 (:238)
    float get_travel_cost(const vector& start, const vector& end) const; // 0x00629680 (:232)
    void do_astar_search(); // 0x006B6470 (:1112)
    bool set_up_search(cover_search_node* start_node); // 0x00678E60 (:1083)
public:
    ~cover_search_object(); // 0x0065A6E0

private:
    cover_search_parameters params; // 0x00
    cover_search_node* best_cover_node; // 0x94
};

cover_node* cover_selection_do_search(cover_search_parameters& params); // 0x006BC8A0 (:1477)
bool cover_selection_node_is_obstructed_by_geometry(const cover_node* node); // 0x00679390 (:1519)
