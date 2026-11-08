// Original: volition/vlib/code/util/recursive_bbox.h
// Implementation: volition/vlib/code/util/recursive_bbox.cpp
//
// Recursive bounding box tree ("rbb"): a static AABB tree over fixed-size leaf objects, built once and stored in asset
// files (chunk shards and parent LODs in c_chunk_base). Building: rbb_build_start(max_objects, obj_size), rbb_build_add
// per object (rbb_obj: box plus a pointer to obj_size bytes), rbb_build_finish splits nodes until thresh_count objects
// or thresh_dim size (rbb_split_params, default Rbb_default_split_params) and packs the tree into one allocation.
// rbb_read_tree fixes up a packed tree in place. Queries call cb(leaf, bmin, bmax) for each leaf hit; ccb (optional)
// stops the walk when it returns true.
#pragma once

#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/math/matrix.h"

struct cfile;
class mempool_base;
struct rbb_tree;

// size 0x24
struct rbb_obj
{
    vector bmin; // 0x00
    vector bmax; // 0x0C
    rbb_obj* next; // 0x18
    rbb_obj* prev; // 0x1C
    unsigned char* objp; // 0x20
};

// size 0xC
struct rbb_split_params
{
    int thresh_count; // 0x00
    float thresh_dim; // 0x04
    float thresh_dup; // 0x08
};

// size 0x20
struct rbb_tree_node
{
    void calc_bounding_box(rbb_obj* src_objects); // 0x00590510 (:239)
    void split(int depth, int& bytes_used, rbb_tree* t, rbb_obj* src_objects, const rbb_split_params* params); // 0x00590770 (:271)
    void query_aabb(const vector& bmin, const vector& bmax, int obj_size, bool (*cb)(void*, vector&, vector&), bool (*ccb)(void*)); // 0x00591040 (:487)
    void query_obb(const vector& pos, const matrix& orient, const vector& extents, int obj_size, bool (*cb)(void*, vector&, vector&), bool (*ccb)(void*)); // 0x00591230 (:536)
    void query_sphere(const vector& pos, float radius, int obj_size, bool (*cb)(void*, vector&, vector&), bool (*ccb)(void*)); // 0x00591570 (:587)
    void query_ray(const vector& start, const vector& end, int obj_size, bool (*cb)(void*, vector&, vector&), bool (*ccb)(void*)); // 0x005918B0 (:635)
    bool object_in_octant(const rbb_obj* objp, const vector& _bmin, const vector& _bmax); // 0x00590660 (:257)

    int num_objects; // 0x00
    vector mn; // 0x04
    vector mx; // 0x10
    unsigned char* node_data; // 0x1C
};

// size 0xC
struct rbb_tree
{
    void query_aabb(const vector& bmin, const vector& bmax, bool (*cb)(void*, vector&, vector&), bool (*ccb)(void*)); // 0x005919A0 (:693)
    void query_obb(const vector& pos, const matrix& orient, const vector& extents, bool (*cb)(void*, vector&, vector&), bool (*ccb)(void*)); // 0x005919D0 (:698)
    void query_sphere(const vector& pos, float radius, bool (*cb)(void*, vector&, vector&), bool (*ccb)(void*)); // 0x00591A00 (:703)
    void query_ray(const vector& start, const vector& end, bool (*cb)(void*, vector&, vector&), bool (*ccb)(void*)); // 0x00591A30 (:708)
    void read_internal(unsigned char* data, int& offset, void (*_leaf_cb)(void*)); // 0x00591C50 (:839)
private:
    void rbb_read_nodes(rbb_tree_node* node, unsigned char* data, int& offset); // 0x00590280 (:111)

public:
    int obj_size; // 0x00
    void (*leaf_cb)(void*); // 0x04
    rbb_tree_node* nodes; // 0x08
};

void rbb_build_start(int max_objects, int obj_size); // 0x00591A60 (:721)
bool rbb_build_add(const rbb_obj* objp); // 0x00591B10 (:744)
rbb_tree* rbb_build_finish(int& bytes_used, const rbb_split_params* params, mempool_base* pool); // 0x005B1B80 (:764)
rbb_tree* rbb_copy_tree(mempool_base* pool, const rbb_tree* root, int& bytes_used); // 0x00591B90 (:798)
void rbb_delete_tree(rbb_tree* root); // 0x00591C40 (:828)
rbb_tree* rbb_read_tree(unsigned char* data, int& offset, void (*leaf_cb)(void*)); // 0x00591C80 (:851)
void rbb_write_tree(cfile* f, const rbb_tree* root); // 0x00591CC0 (:864)
