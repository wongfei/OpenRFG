// Original: volition/rfg/code/destruction/rfg_rbb_tree.h
// Implementation: volition/rfg/code/destruction/rfg_rbb_tree.cpp
//
// The bounding box tree over a destroyable shape's pieces, with fixed-point boxes (fp_aabb). Built per base shape when
// a chunk is crunched (rfg_rbb_build_*), read in place from the chunk (rfg_rbb_read_tree), and rebuilt for an instance
// whose pieces have moved (rfg_destroyable_shape::recalc_rbb_tree, rfg_rbb_build_tree(shape, base_tree)). The queries
// return the shape keys of the existing pieces whose boxes are hit; maybe_filter skips pieces whose physical material
// the current ray filter (havok_game_filter::m_current_ray_flags) ignores. node_data holds the two child nodes, or the
// leaf's 32-bit piece indices.
#pragma once

#include "volition/vlib/code/math/fp_aabb.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/util/endian_type.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"

class mempool;
class rfg_destroyable_shape;
struct rbb_obj;
struct rbb_split_params;
struct hkpShapeRayCastInput;
struct hkpShapeRayCastOutput;
class hkpCdBody;
class hkpRayHitCollector;

// size 0x14
struct rfg_rbb_node
{
    static void query_aabb(const rfg_destroyable_shape* shape, const vector& bmin, const vector& bmax, hkArray<unsigned int,hkContainerHeapAllocator>& hits); // 0x0072F5C0 (:489)
    static void query_obb(const rfg_destroyable_shape* shape, const vector& pos, const matrix& orient, const vector& extents, hkArray<unsigned int,hkContainerHeapAllocator>& hits); // 0x0072F720 (:558)
    static void query_sphere(const rfg_destroyable_shape* shape, const vector& pos, float radius, hkArray<unsigned int,hkContainerHeapAllocator>& hits); // 0x0072FA90 (:604)
    static void query_ray(const rfg_destroyable_shape* shape, const hkpShapeRayCastInput& in, const hkpCdBody& body, hkpRayHitCollector& out); // 0x00727480 (:694)
    static void query_ray(const rfg_destroyable_shape* shape, const hkpShapeRayCastInput& in, hkpShapeRayCastOutput& out); // 0x00727270 (:644)
private:
    // rfg_rbb_build_tree calls the private copy (friend inferred).
    friend int rfg_rbb_build_tree(const rfg_destroyable_shape* shape, const rfg_rbb_node* base_tree);
    void copy(const rfg_destroyable_shape* shape, const fp_aabb& new_aabb, const rfg_rbb_node* copy_node); // 0x0073CB40 (:763)
public:
    rfg_rbb_node* copy(const rfg_destroyable_shape* shape) const; // 0x0073F3C0 (:752)
    rfg_rbb_node& operator=(const rfg_rbb_node& __that); // 0x00736310

    int num_objects; // 0x00
    fp_aabb aabb; // 0x04
    et_ptr_offset<unsigned char,0> node_data; // 0x10
};

void rfg_rbb_build_start(int max_objects, int obj_size); // 0x007175A0 (:846)
void rfg_rbb_build_add(const rbb_obj* objp); // 0x007175C0 (:870)
int rfg_rbb_build_tree(const rbb_split_params* params); // 0x007175E0 (:893)
int rfg_rbb_build_tree(const rfg_destroyable_shape* shape, const rfg_rbb_node* base_tree); // 0x0073F420 (:928)
rfg_rbb_node* rfg_rbb_build_finish(mempool* pool); // 0x00736340 (:952)
int rfg_rbb_get_actual_size_req(const rfg_rbb_node* root); // 0x0071FB80 (:984)
rfg_rbb_node* rfg_rbb_read_tree(unsigned char* data, int& offset); // 0x007363F0 (:1010)
void rfg_rbb_render_debug(const rfg_rbb_node* tree); // 0x0071FCC0 (:1047)
