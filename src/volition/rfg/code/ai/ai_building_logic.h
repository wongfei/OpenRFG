// Original: volition/rfg/code/ai/ai_building_logic.h
// Implementation: volition/rfg/code/ai/ai_building_logic.cpp
//
// AI building data: distances to and containment in buildings (bounding boxes, root building, substructures), and the
// navpoints, guard nodes, cover nodes and transitions collected from a building. ai_building_process_streaming follows
// the streaming of buildings.
#pragma once

#include "volition/vlib/code/util/varray.h"

class cover_node;
class navpoint;
class object;
class object_guard_node;
class rfg_mover;
class vector;

void ai_building_process_streaming(rfg_mover* building, bool stream_in); // 0x005FB480 (:447)
float ai_building_find_bbox_dist_squared(const vector& pos, const rfg_mover* building); // 0x005DC530 (:495)
const rfg_mover* get_root_building_internal(const rfg_mover* building); // 0x005FB430 (:200)
bool ai_building_is_in_bbox(const vector& pos, const rfg_mover* building); // 0x005DC5A0 (:508)
bool ai_building_find_dist_squared(float& min_dist_sq, const vector& pos, const rfg_mover* building, float threshold); // 0x0064BA40 (:531)
bool ai_building_is_in_building(const vector& pos, const rfg_mover* building); // 0x0064BC90 (:607)
const rfg_mover* ai_building_get_root_building(const rfg_mover* building); // 0x005FB550 (:671)
bool ai_building_object_belongs_to_building(const object* obj, const rfg_mover* building); // 0x005DC5E0 (:693)
bool ai_building_collect_substructures(base_array<rfg_mover*>& substructure_list, const rfg_mover* structure, bool (*cb_func)(object*, unsigned int, const void*), void* cb_data); // 0x0064BE20 (:715)
bool ai_building_collect_navpoints(base_array<navpoint*>& navpoint_list, const rfg_mover* structure, bool from_root, bool (*cb_func)(object*, unsigned int, const void*), void* cb_data); // 0x0064BFA0 (:782)
bool ai_building_collect_guard_nodes(base_array<object_guard_node*>& guard_node_list, const rfg_mover* structure, bool from_root, bool (*cb_func)(object*, unsigned int, const void*), void* cb_data); // 0x0064C0E0 (:841)
bool ai_building_collect_cover_nodes(base_array<cover_node*>& cover_node_list, const rfg_mover* structure, bool from_root, bool (*cb_func)(object*, unsigned int, const void*), void* cb_data); // 0x0064C230 (:901)
navpoint* ai_building_find_closest_navpoint(const vector& pos, const rfg_mover* structure, bool from_root, float* actual_xz_dist_sq); // 0x0064C380 (:964)
navpoint* ai_building_find_closest_transition_from_inside(const vector& pos, const rfg_mover* structure, bool allow_jump); // 0x0064C730 (:1075)
navpoint* ai_building_find_closest_transition_from_outside(const vector& pos, const rfg_mover* structure, navpoint** next_navp); // 0x0064C7C0 (:1121)
