// Original: volition/vlib/code/util/recursive_bbox.cpp
//
// Partial (NPC.md NP5): reading a packed tree and the box and sphere queries, which is what traffic.cpp asks for. Building,
// copying, writing and the other queries are not ported, as nothing compiled calls them; their declarations stay.

#include "volition/vlib/code/util/recursive_bbox.h"

// ---- file-static globals
static rbb_split_params Rbb_default_split_params = {4, 2.0f, 1.0f}; // 0x01519718
static rbb_obj* Rbb_objects; // 0x01CDA5BC
static rbb_tree* Rbb_tree; // 0x01CE2760
static int Rbb_max_objects; // 0x01CE7FC8
static unsigned int Rbb_mempool_mark; // 0x01CE7FD4
static bool Rbb_building; // 0x01CE87BA

// ---- internal functions
// LTCG: register arguments mapped in the Ghidra DB.
static unsigned char* rbb_alloc(int bytes); // 0x00590120 (:64)
static void rbb_delete(void* bytes); // 0x00590140 (:75)
static void rbb_write_endian_swizzled_node(cfile* f, const rbb_tree_node* node); // 0x00590150 (:86)
// LTCG: register arguments mapped in the Ghidra DB.
static void rbb_write_endian_swizzled_leaf(cfile* f, const rbb_tree_node* node, int obj_size); // 0x00590230 (:100)
static void rbb_write_nodes(cfile* f, rbb_tree_node* node, int obj_size); // 0x00590330 (:139)
static void rbb_copy_nodes(unsigned char* data, int& offset, rbb_tree_node* dst, rbb_tree_node* src, int obj_size); // 0x00590380 (:160)
static int rbb_get_node_data_size(rbb_tree_node* root); // 0x00590470 (:186)
static int rbb_get_leaf_data_size(rbb_tree_node* root, const int leaf_size); // 0x005904B0 (:201)

// A node's stack in the walks: 0x1000 bytes in rfg.exe.
static const int RBB_QUERY_STACK = 1024;

// 0x00590280 (:111): nodes sit in pairs in the data, right after their parent's turn; a leaf's objects follow it, padded to
// 4 bytes. A negative count means an empty node.
void rbb_tree::rbb_read_nodes(rbb_tree_node* node, unsigned char* data, int& offset)
{
    while (node->num_objects == 0)
    {
        node->node_data = data + offset;
        offset += 0x40;
        rbb_read_nodes(reinterpret_cast<rbb_tree_node*>(node->node_data), data, offset);
        node = reinterpret_cast<rbb_tree_node*>(node->node_data) + 1;
    }
    if (node->num_objects < 0)
    {
        return;
    }
    node->node_data = data + offset;
    offset += node->num_objects * obj_size;
    if (leaf_cb != nullptr)
    {
        for (int i = 0; i < node->num_objects; i++)
        {
            leaf_cb(node->node_data + obj_size * i);
        }
    }
    const int misaligned = offset % 4;
    if (misaligned != 0)
    {
        offset += 4 - misaligned;
    }
}

// 0x00591040 (:487): the walk stops when ccb answers false.
void rbb_tree_node::query_aabb(const vector& bmin, const vector& bmax, int obj_size, bool (*cb)(void*, vector&, vector&), bool (*ccb)(void*))
{
    rbb_tree_node* stack[RBB_QUERY_STACK];
    stack[0] = this;
    int depth = 1;
    do
    {
        rbb_tree_node* node = stack[--depth];
        if (bmin.x > node->mx.x || bmin.y > node->mx.y || bmin.z > node->mx.z || node->mn.x > bmax.x || node->mn.y > bmax.y || node->mn.z > bmax.z)
        {
            continue;
        }
        if (node->num_objects == 0)
        {
            rbb_tree_node* children = reinterpret_cast<rbb_tree_node*>(node->node_data);
            stack[depth++] = children;
            stack[depth++] = children + 1;
            continue;
        }
        unsigned char* objp = node->node_data;
        for (int i = 0; i < node->num_objects; i++)
        {
            vector obj_min;
            vector obj_max;
            if (cb(objp, obj_min, obj_max) && bmin.x <= obj_max.x && bmin.y <= obj_max.y && bmin.z <= obj_max.z && obj_min.x <= bmax.x && obj_min.y <= bmax.y && obj_min.z <= bmax.z && !ccb(objp))
            {
                return;
            }
            objp += obj_size;
        }
    } while (depth > 0);
}

// The squared distance of a point from a box, which rfg.exe builds axis by axis, stopping early past the limit.
static bool rbb_box_in_sphere(const vector& pos, float radius_sq, const vector& bmin, const vector& bmax)
{
    float dist = 0.0f;
    const float* p = &pos.x;
    const float* mn = &bmin.x;
    const float* mx = &bmax.x;
    for (int axis = 0; axis < 3; axis++)
    {
        if (p[axis] < mn[axis])
        {
            dist += (p[axis] - mn[axis]) * (p[axis] - mn[axis]);
        }
        else if (mx[axis] < p[axis])
        {
            dist += (p[axis] - mx[axis]) * (p[axis] - mx[axis]);
        }
        if (dist > radius_sq)
        {
            return false;
        }
    }
    return true;
}

// 0x00591570 (:587): the walk stops when ccb answers false.
void rbb_tree_node::query_sphere(const vector& pos, float radius, int obj_size, bool (*cb)(void*, vector&, vector&), bool (*ccb)(void*))
{
    const float radius_sq = radius * radius;
    rbb_tree_node* stack[RBB_QUERY_STACK];
    stack[0] = this;
    int depth = 1;
    do
    {
        rbb_tree_node* node = stack[--depth];
        if (!rbb_box_in_sphere(pos, radius_sq, node->mn, node->mx))
        {
            continue;
        }
        if (node->num_objects == 0)
        {
            rbb_tree_node* children = reinterpret_cast<rbb_tree_node*>(node->node_data);
            stack[depth++] = children;
            stack[depth++] = children + 1;
            continue;
        }
        unsigned char* objp = node->node_data;
        for (int i = 0; i < node->num_objects; i++)
        {
            vector obj_min;
            vector obj_max;
            if (cb(objp, obj_min, obj_max) && rbb_box_in_sphere(pos, radius_sq, obj_min, obj_max) && !ccb(objp))
            {
                return;
            }
            objp += obj_size;
        }
    } while (depth > 0);
}

// 0x005919A0 (:693)
void rbb_tree::query_aabb(const vector& bmin, const vector& bmax, bool (*cb)(void*, vector&, vector&), bool (*ccb)(void*))
{
    nodes->query_aabb(bmin, bmax, obj_size, cb, ccb);
}

// 0x00591A00 (:703)
void rbb_tree::query_sphere(const vector& pos, float radius, bool (*cb)(void*, vector&, vector&), bool (*ccb)(void*))
{
    nodes->query_sphere(pos, radius, obj_size, cb, ccb);
}

// 0x00591C50 (:839)
void rbb_tree::read_internal(unsigned char* data, int& offset, void (*_leaf_cb)(void*))
{
    leaf_cb = _leaf_cb;
    nodes = reinterpret_cast<rbb_tree_node*>(data + offset);
    offset += 0x20;
    rbb_read_nodes(nodes, data, offset);
}

// 0x00591C80 (:851)
rbb_tree* rbb_read_tree(unsigned char* data, int& offset, void (*leaf_cb)(void*))
{
    rbb_tree* tree = reinterpret_cast<rbb_tree*>(data + offset);
    offset += 0xC;
    tree->read_internal(data, offset, leaf_cb);
    return tree;
}
