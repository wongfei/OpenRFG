// Original: volition/rfg/code/havok/havok_cm_manager.h
// Implementation: volition/rfg/code/havok/havok_cm_manager.cpp
//
// The collision model manager: a table of shared Havok shapes indexed by collision model index (cm_index), loaded in
// place from collision model chunks (havok_cm_manager_load_all) or created for simple primitives (box, sphere,
// capsule, cylinder).
#pragma once


class hkpShape;
class quaternion;
class vector;

void havok_cm_manager_init(); // 0x00835660 (:438)
void havok_cm_manager_shutdown(); // 0x00824000 (:472)
void havok_cm_manager_read_cm_header(unsigned int* cm_chunk_size, const void* source_buffer, unsigned int* data_size, const unsigned char** data_start); // 0x0080F370 (:492)
bool havok_cm_manager_load_all(unsigned int* cm_chunk_size, const void* source_buffer, unsigned int* cm_indices, unsigned int max_cm_indices, unsigned int* out_num_collision_models, const char* asset_name); // 0x00824040 (:532)
const hkpShape* havok_cm_manager_get(unsigned int cm_index); // 0x0080F3C0 (:626)
unsigned int havok_cm_manager_add(const hkpShape* collision_model); // 0x0080F3E0 (:643)
unsigned int havok_cm_manager_get_count(); // 0x0080F450 (:688)
void havok_cm_manager_remove_from_list(unsigned int cm_index); // 0x008199E0 (:697)
void havok_cm_manager_remove_all(); // 0x00819A20 (:729)
hkpShape* havok_cm_manager_create_box_shape(const quaternion* q_rotation, const vector* position, const vector* half_extents); // 0x00832750 (:750)
hkpShape* havok_cm_manager_create_sphere_shape(const vector* position, float radius); // 0x00819A60 (:908)
hkpShape* havok_cm_manager_create_capsule_shape(const vector* endA, const vector* endB, float radius); // 0x00819B90 (:934)
hkpShape* havok_cm_manager_create_cylinder_shape(const vector* endA, const vector* endB, float radius); // 0x00819C50 (:952)
