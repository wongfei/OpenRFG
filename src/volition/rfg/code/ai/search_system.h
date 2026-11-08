// Original: volition/rfg/code/ai/search_system.h
// Implementation: volition/rfg/code/ai/search_system.cpp
//
// The search system: when NPCs lose a target, a search of the likely locations (building interiors and external areas)
// is set up and the searchers are assigned locations until the target is found or the search expires.
// ai_system_do_frame advances the search objects (ai_search_object_manager::do_frame).
#pragma once

#include <stdint.h>
#include "volition/rfg/code/ai/ai_types.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/varray.h"

struct ai_search_location_info;
struct ai_search_searcher_info;
struct ai_search_target_info;
class cover_node;
class cs_spinlock;
class human;
struct nav_cell;
class npc;
class object;
class rfg_mover;

// size 0x8
struct ai_search_building_candidate
{
    rfg_mover* building; // 0x00
    float dist_sq; // 0x04
};

// size 0x98
struct ai_search_location_history
{
    void clear(); // 0x006418D0 (:462)
    void set_current(const vector& pos, const vector& los_pos, const nav_cell* ncp, const cover_node* c_node); // 0x005F18A0 (:468)
    void finish_current(const vector& npc_pos); // 0x006418F0 (:478)
    void abandon_current(); // 0x005F1900 (:492)
    void abandon_cover_node(); // 0x005F1910 (:498)
    ai_search_location_history(); // 0x00641850
    ~ai_search_location_history(); // 0x00641870

    bool has_current; // 0x00
    vector current_target; // 0x04
    vector los_position; // 0x10
    uint64_t navcell; // 0x20
    unsigned int cover; // 0x28
    circ_array<vector,8> old; // 0x2C
};

bool locations_are_equivalent(const ai_search_location_info& first, const ai_search_location_info& second); // 0x005F1800 (:78)
bool building_collect_cb(object* hit_objp, unsigned int shape_key, const void* data); // 0x005F1850 (:93)
bool building_has_transition_cb(object* hit_objp, unsigned int, const void*); // 0x005F1880 (:98)
void ai_search_system_level_init_pre(); // 0x006BB0A0 (:507)
void ai_search_system_do_frame(); // 0x006BB0B0 (:513)
bool ai_search_is_anyone_searching_for_target(const human* target_hp); // 0x006851F0 (:537)
bool ai_search_is_searching(const npc* np, const human* target_hp); // 0x00685210 (:551)
bool ai_search_is_fully_populated(const human* target_hp); // 0x00685250 (:581)
bool ai_search_get_best_guess_location(vector& pos, const human* target_hp); // 0x006852D0 (:613)
unsigned int ai_search_request_search(const npc* np, const human* target_hp); // 0x006A9730 (:637)
const ai_search_location_info* ai_search_get_location_info(unsigned int search_handle); // 0x00685310 (:701)
ai_search_target_info* ai_search_get_target_info(unsigned int search_handle); // 0x00685380 (:720)
void ai_search_found_target(unsigned int search_handle, const human* enemy); // 0x006A9850 (:740)
bool ai_search_update_progress(unsigned int search_handle, ai_results result); // 0x006A98A0 (:813)
bool ai_search_get_current_exterior_search_pos(vector& pos, vector& los_pos, nav_cell*& ncp, unsigned int search_handle, cover_node** c_node); // 0x0066B660 (:852)
bool ai_search_set_current_exterior_search_pos(unsigned int search_handle, const vector& pos, const vector& los_pos, const nav_cell* ncp, const cover_node* c_node); // 0x0066B720 (:880)
bool ai_search_finish_exterior_search_pos(unsigned int search_handle, const vector& current_pos); // 0x0066B7C0 (:906)
bool ai_search_abandon_exterior_search_pos(unsigned int search_handle); // 0x0066B810 (:923)
bool ai_search_abandon_exterior_search_cover(unsigned int search_handle); // 0x0066B850 (:940)
bool ai_search_set_avoidance_point(unsigned int search_handle, const vector& avoid_pos); // 0x0066B890 (:958)
cs_spinlock& ai_search_get_searchers_cs_wrapper(); // 0x005F1920 (:969)
const ai_search_searcher_info* ai_search_get_first_searcher(); // 0x00641910 (:978)
const ai_search_searcher_info* ai_search_get_next_searcher(const ai_search_searcher_info* prev_info); // 0x0066B8E0 (:990)
