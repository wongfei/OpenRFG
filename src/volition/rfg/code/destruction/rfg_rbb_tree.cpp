// Original: volition/rfg/code/destruction/rfg_rbb_tree.cpp
//
// The bounding box tree of a destroyable shape (decompiled; header comment in rfg_rbb_tree.h). A node with
// num_objects 0 is internal: node_data points at its two children, stored next to each other (0x14 bytes each). A
// node with num_objects > 0 is a leaf whose node_data points at that many 32-bit piece indices; num_objects -1 marks
// an empty subtree. The queries walk the tree with an explicit stack (the second child is visited first) and append
// the keys of the shape's existing pieces whose own boxes are hit. Compiled in rfg_destruction_unity.cpp.
//
// Rebuilding a tree for an instance (rfg_rbb_build_tree(shape, base_tree)) copies the base tree, keeping only the
// pieces the shape still owns and refitting the boxes, into the thread's context pool between a mark and
// rfg_rbb_build_finish, which compacts it into the caller's pool. The build state is thread local.
// Partial: the cruncher build path (rfg_rbb_build_start/_add/tree(params)) reports its error as in the game, and the
// debug rendering is not implemented.

#include "volition/rfg/code/destruction/rfg_rbb_tree.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkprayhitcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/havok/havok_game_filter.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/memory/mempool_base.h"

#include "keen/base/trace.hpp"

#include <string.h>

static const int RBB_NODE_SIZE = 0x14; // sizeof(rfg_rbb_node)
static const int RBB_MAX_STACK = 256; // depth of the query stacks

// ---- file-static globals
static int Rbb_max_render_depth = -1; // 0x01650460
// The build state: thread locals at TLS +0x84C (tree), +0x850 (pool mark), +0x854 (building).
static __declspec(thread) rfg_rbb_node* Rfg_rbb_node;
static __declspec(thread) unsigned int Rfg_rbb_mempool_mark;
static __declspec(thread) bool Rfg_rbb_building;

// The second child of an internal node.
static rfg_rbb_node* rbb_children(const rfg_rbb_node* node)
{
    return (rfg_rbb_node*)node->node_data.to_pointer();
}

// 0x00717520 (:78)
static unsigned char* rfg_rbb_alloc(int bytes)
{
    mempool_base* pool = get_this_context_virtual_mempool();
    if (pool == nullptr)
    {
        return nullptr;
    }
    return (unsigned char*)pool->alloc(bytes, 0);
}

// 0x0071FB00 (:91)
static int rfg_rbb_count_tree_nodes(const rfg_rbb_node* root)
{
    if (root->num_objects < 0)
    {
        return 0;
    }
    if (root->num_objects > 0)
    {
        return 1;
    }
    const rfg_rbb_node* children = rbb_children(root);
    return rfg_rbb_count_tree_nodes(&children[1]) + 1 + rfg_rbb_count_tree_nodes(&children[0]);
}

// 0x0071FB40 (:104)
static int rfg_rbb_count_leaf_objects(const rfg_rbb_node* root)
{
    if (root->num_objects < 0)
    {
        return 0;
    }
    if (root->num_objects > 0)
    {
        return root->num_objects;
    }
    const rfg_rbb_node* children = rbb_children(root);
    return rfg_rbb_count_leaf_objects(&children[1]) + rfg_rbb_count_leaf_objects(&children[0]);
}

// 0x007361D0 (:118)
// Fixes up a tree read in place: each internal node's children follow at offset (0x28 bytes), each leaf's indices
// (4 bytes per piece).
static void rfg_rbb_read_nodes(rfg_rbb_node* node, unsigned char* data, int& offset)
{
    if (node->num_objects < 0)
    {
        return;
    }
    while (node->num_objects == 0)
    {
        node->node_data.from_pointer(data + offset);
        offset += 2 * RBB_NODE_SIZE;
        rfg_rbb_node* children = rbb_children(node);
        rfg_rbb_read_nodes(&children[0], data, offset);
        node = &children[1];
        if (node->num_objects < 0)
        {
            return;
        }
    }
    node->node_data.from_pointer(data + offset);
    offset += node->num_objects * 4;
}

// 0x00736230 (:204)
// Copies a tree into data (from offset on), keeping the layout of rfg_rbb_read_nodes.
static void rfg_rbb_copy_nodes(unsigned char* data, int& offset, rfg_rbb_node* dst, const rfg_rbb_node* src)
{
    if (src->num_objects < 0)
    {
        return;
    }
    while (true)
    {
        dst->num_objects = src->num_objects;
        dst->aabb = src->aabb;
        dst->node_data.set_offset(src->node_data.get_offset());
        if (src->num_objects != 0)
        {
            break;
        }
        dst->node_data.from_pointer(data + offset);
        offset += 2 * RBB_NODE_SIZE;
        rfg_rbb_copy_nodes(data, offset, &rbb_children(dst)[0], &rbb_children(src)[0]);
        src = &rbb_children(src)[1];
        dst = &rbb_children(dst)[1];
        if (src->num_objects < 0)
        {
            return;
        }
    }
    dst->node_data.from_pointer(data + offset);
    offset += src->num_objects * 4;
    memcpy(dst->node_data.to_pointer(), src->node_data.to_pointer(), src->num_objects * 4);
}

// 0x007362E0 (:230)
// LTCG: pool in ECX, buffer_size in EDX.
static rfg_rbb_node* rfg_rbb_copy_tree(mempool* pool, const rfg_rbb_node* root, int buffer_size)
{
    rfg_rbb_node* dst = (rfg_rbb_node*)((mempool_base*)pool)->alloc(buffer_size, 4);
    int offset = RBB_NODE_SIZE;
    rfg_rbb_copy_nodes((unsigned char*)dst, offset, dst, root);
    return dst;
}

// 0x00717540 (:471)
// Whether the current ray filter skips the piece: see-through (8, material flag 0x80), shatter (0x10, flag 1) or
// fence (2, shape type 3) pieces.
static bool maybe_filter(const rfg_subpiece_base* sp_base)
{
    const unsigned int ray_flags = havok_game_filter::m_current_ray_flags;
    if (ray_flags == 0)
    {
        return false;
    }
    const rfg_physical_material* material = Rfg_physical_material_default;
    if (sp_base->physical_material_index != 0xFF && sp_base->physical_material_index < Num_rfg_physical_materials)
    {
        material = &RFG_physical_materials[sp_base->physical_material_index];
    }
    return ((ray_flags & 8) != 0 && (material->flags & 0x80) != 0) || ((ray_flags & 0x10) != 0 && (material->flags & 1) != 0) ||
           ((ray_flags & 2) != 0 && sp_base->shape_type == 3);
}

// Appends a key to a hit list (hkArray::pushBack, inlined in the queries).
static void rbb_add_hit(hkArray<unsigned int,hkContainerHeapAllocator>& hits, unsigned int key)
{
    hits.pushBack(key);
}

// 0x0072F5C0 (:489)
void rfg_rbb_node::query_aabb(const rfg_destroyable_shape* shape, const vector& bmin, const vector& bmax, hkArray<unsigned int,hkContainerHeapAllocator>& hits)
{
    if (shape->m_type.m_storage != hkcdShapeType::BV_TREE)
    {
        return;
    }
    fp_aabb box;
    box.set(bmin, bmax);
    const rfg_rbb_node* stack[RBB_MAX_STACK];
    stack[0] = shape->aabb_tree;
    int depth = 1;
    rfg_subpiece_base* base_objects = shape->parent->base_objects.to_pointer();
    do
    {
        const rfg_rbb_node* node = stack[--depth];
        if (node->num_objects < 0 || !fp_aabb_intersect(box, node->aabb))
        {
            continue;
        }
        if (node->num_objects == 0)
        {
            const rfg_rbb_node* children = rbb_children(node);
            stack[depth] = &children[0];
            stack[depth + 1] = &children[1];
            depth += 2;
            continue;
        }
        const unsigned int* keys = (const unsigned int*)node->node_data.to_pointer();
        for (int i = 0; i < node->num_objects; i++)
        {
            const unsigned int key = keys[i];
            if (shape->get_shape_state((int)key) && ix_box_box_aligned(bmin, bmax, base_objects[key].prel_bmin, base_objects[key].prel_bmax))
            {
                rbb_add_hit(hits, key);
            }
        }
    } while (depth > 0);
}

// 0x0072F720 (:558)
// The node boxes and the pieces are tested as boxes against the oriented box (pieces centred on their position).
void rfg_rbb_node::query_obb(const rfg_destroyable_shape* shape, const vector& pos, const matrix& orient, const vector& extents, hkArray<unsigned int,hkContainerHeapAllocator>& hits)
{
    if (shape->m_type.m_storage != hkcdShapeType::BV_TREE)
    {
        return;
    }
    const rfg_rbb_node* stack[RBB_MAX_STACK];
    stack[0] = shape->aabb_tree;
    int depth = 1;
    rfg_subpiece_base* base_objects = shape->parent->base_objects.to_pointer();
    do
    {
        const rfg_rbb_node* node = stack[--depth];
        vector nmin;
        vector nmax;
        node->aabb.get(nmin, nmax);
        const vector half((nmax.x - nmin.x) * 0.5f, (nmax.y - nmin.y) * 0.5f, (nmax.z - nmin.z) * 0.5f);
        const vector delta(pos.x - (half.x + nmin.x), pos.y - (half.y + nmin.y), pos.z - (half.z + nmin.z));
        if (ix_box_box(orient, delta, half, extents) != 0)
        {
            continue;
        }
        if (node->num_objects == 0)
        {
            const rfg_rbb_node* children = rbb_children(node);
            stack[depth] = &children[0];
            stack[depth + 1] = &children[1];
            depth += 2;
            continue;
        }
        const unsigned int* keys = (const unsigned int*)node->node_data.to_pointer();
        for (int i = 0; i < node->num_objects; i++)
        {
            const unsigned int key = keys[i];
            if (!shape->get_shape_state((int)key))
            {
                continue;
            }
            const rfg_subpiece_base& sp = base_objects[key];
            const vector sp_half((sp.prel_bmax.x - sp.prel_bmin.x) * 0.5f, (sp.prel_bmax.y - sp.prel_bmin.y) * 0.5f, (sp.prel_bmax.z - sp.prel_bmin.z) * 0.5f);
            const vector sp_delta(pos.x - sp.pos.x, pos.y - sp.pos.y, pos.z - sp.pos.z);
            if (ix_box_box(orient, sp_delta, sp_half, extents) == 0)
            {
                rbb_add_hit(hits, key);
            }
        }
    } while (depth > 0);
}

// 0x0072FA90 (:604)
void rfg_rbb_node::query_sphere(const rfg_destroyable_shape* shape, const vector& pos, float radius, hkArray<unsigned int,hkContainerHeapAllocator>& hits)
{
    if (shape->m_type.m_storage != hkcdShapeType::BV_TREE)
    {
        return;
    }
    const rfg_rbb_node* stack[RBB_MAX_STACK];
    stack[0] = shape->aabb_tree;
    int depth = 1;
    rfg_subpiece_base* base_objects = shape->parent->base_objects.to_pointer();
    do
    {
        const rfg_rbb_node* node = stack[--depth];
        vector nmin;
        vector nmax;
        node->aabb.get(nmin, nmax);
        if (!ix_sphere_in_box(pos, radius, nmin, nmax))
        {
            continue;
        }
        if (node->num_objects == 0)
        {
            const rfg_rbb_node* children = rbb_children(node);
            stack[depth] = &children[0];
            stack[depth + 1] = &children[1];
            depth += 2;
            continue;
        }
        const unsigned int* keys = (const unsigned int*)node->node_data.to_pointer();
        for (int i = 0; i < node->num_objects; i++)
        {
            const unsigned int key = keys[i];
            if (shape->get_shape_state((int)key) && ix_sphere_in_box(pos, radius, base_objects[key].prel_bmin, base_objects[key].prel_bmax))
            {
                rbb_add_hit(hits, key);
            }
        }
    } while (depth > 0);
}

// 0x00727270 (:644)
// Casts the ray against the child shapes of the pieces whose boxes it crosses and keeps the closest hit.
void rfg_rbb_node::query_ray(const rfg_destroyable_shape* shape, const hkpShapeRayCastInput& in, hkpShapeRayCastOutput& out)
{
    if (shape->m_type.m_storage != hkcdShapeType::BV_TREE)
    {
        return;
    }
    vector from;
    vector to;
    havok_to_game_vector(from, in.m_from);
    havok_to_game_vector(to, in.m_to);
    const rfg_rbb_node* stack[RBB_MAX_STACK];
    stack[0] = shape->aabb_tree;
    int depth = 1;
    rfg_subpiece_base* base_objects = shape->parent->base_objects.to_pointer();
    rfg_subpiece_base_extra_data* extra = shape->parent->base_extra_data.to_pointer();
    do
    {
        const rfg_rbb_node* node = stack[--depth];
        vector nmin;
        vector nmax;
        vector intercept;
        node->aabb.get(nmin, nmax);
        if (!ix_linesegment_boundingbox(intercept, nmin, nmax, from, to))
        {
            continue;
        }
        if (node->num_objects == 0)
        {
            const rfg_rbb_node* children = rbb_children(node);
            stack[depth] = &children[0];
            stack[depth + 1] = &children[1];
            depth += 2;
            continue;
        }
        const unsigned int* keys = (const unsigned int*)node->node_data.to_pointer();
        for (int i = 0; i < node->num_objects; i++)
        {
            const unsigned int key = keys[i];
            const rfg_subpiece_base& sp = base_objects[key];
            if (!shape->get_shape_state((int)key) || maybe_filter(&sp) || !ix_linesegment_boundingbox(intercept, sp.prel_bmin, sp.prel_bmax, from, to))
            {
                continue;
            }
            hkpShapeRayCastOutput piece_out;
            piece_out.m_hitFraction = 1.0f;
            piece_out.m_extraInfo = -1;
            piece_out.m_shapeKeyIndex = 0;
            piece_out.m_shapeKeys[0] = 0xFFFFFFFF;
            const hkpShape* child = extra[key].shape.to_pointer();
            if (child->castRay(in, piece_out).m_bool != 0 && piece_out.m_hitFraction < out.m_hitFraction)
            {
                out.m_hitFraction = piece_out.m_hitFraction;
                out.m_normal = piece_out.m_normal;
                out.m_shapeKeys[out.m_shapeKeyIndex] = key;
            }
        }
    } while (depth > 0);
}

// 0x00727480 (:694)
// As above, reporting every hit within the collector's early-out fraction (normal rotated into world space by the
// body's transform); stops when the collector's early out reaches 0.
void rfg_rbb_node::query_ray(const rfg_destroyable_shape* shape, const hkpShapeRayCastInput& in, const hkpCdBody& body, hkpRayHitCollector& out)
{
    if (shape->m_type.m_storage != hkcdShapeType::BV_TREE)
    {
        return;
    }
    vector from;
    vector to;
    havok_to_game_vector(from, in.m_from);
    havok_to_game_vector(to, in.m_to);
    const rfg_rbb_node* stack[RBB_MAX_STACK];
    stack[0] = shape->aabb_tree;
    int depth = 1;
    rfg_subpiece_base* base_objects = shape->parent->base_objects.to_pointer();
    rfg_subpiece_base_extra_data* extra = shape->parent->base_extra_data.to_pointer();
    do
    {
        const rfg_rbb_node* node = stack[--depth];
        vector nmin;
        vector nmax;
        vector intercept;
        node->aabb.get(nmin, nmax);
        if (!ix_linesegment_boundingbox(intercept, nmin, nmax, from, to))
        {
            continue;
        }
        if (node->num_objects == 0)
        {
            const rfg_rbb_node* children = rbb_children(node);
            stack[depth] = &children[0];
            stack[depth + 1] = &children[1];
            depth += 2;
            continue;
        }
        const unsigned int* keys = (const unsigned int*)node->node_data.to_pointer();
        for (int i = 0; i < node->num_objects; i++)
        {
            const unsigned int key = keys[i];
            const rfg_subpiece_base& sp = base_objects[key];
            if (!shape->get_shape_state((int)key) || maybe_filter(&sp) || !ix_linesegment_boundingbox(intercept, sp.prel_bmin, sp.prel_bmax, from, to))
            {
                continue;
            }
            hkpShapeRayCastOutput piece_out;
            piece_out.m_hitFraction = 1.0f;
            piece_out.m_extraInfo = -1;
            piece_out.m_shapeKeyIndex = 0;
            piece_out.m_shapeKeys[0] = 0xFFFFFFFF;
            const hkpShape* child = extra[key].shape.to_pointer();
            if (child->castRay(in, piece_out).m_bool != 0 && piece_out.m_hitFraction <= out.m_earlyOutHitFraction)
            {
                piece_out.m_extraInfo = (int)key;
                const float* m = (const float*)body.m_motion;
                const float n[4] = {((float*)&piece_out.m_normal.m_quad)[0], ((float*)&piece_out.m_normal.m_quad)[1], ((float*)&piece_out.m_normal.m_quad)[2], 0.0f};
                piece_out.m_normal.set(n[1] * m[4] + n[0] * m[0] + n[2] * m[8], n[1] * m[5] + n[0] * m[1] + n[2] * m[9],
                                       n[1] * m[6] + n[0] * m[2] + n[2] * m[10], n[1] * m[7] + n[0] * m[3] + n[2] * m[11]);
                out.addRayHit(body, piece_out);
                if (out.m_earlyOutHitFraction <= 0.0f)
                {
                    return;
                }
            }
        }
    } while (depth > 0);
}

// 0x00736310
// The node data offset is re-based so the copy points at the same children.
rfg_rbb_node& rfg_rbb_node::operator=(const rfg_rbb_node& __that)
{
    num_objects = __that.num_objects;
    aabb = __that.aabb;
    node_data.from_pointer(__that.node_data.to_pointer());
    return *this;
}

// 0x0073F3C0 (:752)
rfg_rbb_node* rfg_rbb_node::copy(const rfg_destroyable_shape* shape) const
{
    mempool_base* pool = get_this_context_virtual_mempool();
    rfg_rbb_node* node = pool != nullptr ? (rfg_rbb_node*)pool->alloc(RBB_NODE_SIZE, 0) : nullptr;
    fp_aabb box;
    box.set(shape->local_bmin, shape->local_bmax);
    node->copy(shape, box, this);
    return node;
}

// 0x0073CB40 (:763)
// Copies the part of copy_node that overlaps new_aabb and refits it to the pieces the shape still owns; empty
// subtrees become -1 and an internal node with one non-empty child takes that child's place.
void rfg_rbb_node::copy(const rfg_destroyable_shape* shape, const fp_aabb& new_aabb, const rfg_rbb_node* copy_node)
{
    if (!fp_aabb_intersect(new_aabb, copy_node->aabb))
    {
        num_objects = -1;
        return;
    }
    num_objects = 0;
    if (copy_node->num_objects == 0)
    {
        mempool_base* pool = get_this_context_virtual_mempool();
        rfg_rbb_node* children = pool != nullptr ? (rfg_rbb_node*)pool->alloc(2 * RBB_NODE_SIZE, 0) : nullptr;
        node_data.from_pointer((unsigned char*)children);
        const rfg_rbb_node* source = rbb_children(copy_node);
        children[0].copy(shape, new_aabb, &source[0]);
        children[1].copy(shape, new_aabb, &source[1]);
        if (children[0].num_objects != -1)
        {
            if (children[1].num_objects != -1)
            {
                aabb.set(children[0].aabb);
                aabb.expand(children[1].aabb);
                return;
            }
            *this = children[0];
            return;
        }
        if (children[1].num_objects != -1)
        {
            *this = children[1];
            return;
        }
    }
    else
    {
        mempool_base* pool = get_this_context_virtual_mempool();
        unsigned int* keys = pool != nullptr ? (unsigned int*)pool->alloc(copy_node->num_objects * 4, 0) : nullptr;
        node_data.from_pointer((unsigned char*)keys);
        const unsigned int* source = (const unsigned int*)copy_node->node_data.to_pointer();
        vector bmin(3.4028235e+38f, 3.4028235e+38f, 3.4028235e+38f);
        vector bmax(-3.4028235e+38f, -3.4028235e+38f, -3.4028235e+38f);
        rfg_subpiece* objects = shape->get_object_buffer();
        rfg_subpiece_base* base_objects = shape->parent->base_objects.to_pointer();
        for (int i = 0; i < copy_node->num_objects; i++)
        {
            const unsigned int key = source[i];
            if (objects[key].get_destroyable() == shape)
            {
                *keys++ = key;
                num_objects++;
                bbox_expand(bmin, bmax, base_objects[key].prel_bmin, base_objects[key].prel_bmax);
            }
        }
        if (num_objects != 0)
        {
            aabb.set(bmin, bmax);
            return;
        }
    }
    num_objects = -1;
}

// 0x007175A0 (:846)
void rfg_rbb_build_start(int max_objects, int obj_size)
{
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
}

// 0x007175C0 (:870)
void rfg_rbb_build_add(const rbb_obj* objp)
{
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
}

// 0x007175E0 (:893)
int rfg_rbb_build_tree(const rbb_split_params* params)
{
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    return 0;
}

// 0x0073F420 (:928)
// Builds the instance tree in the thread's context pool after a mark; returns the size of the compact copy.
int rfg_rbb_build_tree(const rfg_destroyable_shape* shape, const rfg_rbb_node* base_tree)
{
    Rfg_rbb_building = true;
    mempool_base* pool = get_this_context_virtual_mempool();
    if (pool == nullptr)
    {
        return 0;
    }
    Rfg_rbb_mempool_mark = pool->mark();
    pool = get_this_context_virtual_mempool();
    rfg_rbb_node* root = pool != nullptr ? (rfg_rbb_node*)pool->alloc(RBB_NODE_SIZE, 0) : nullptr;
    fp_aabb box;
    box.set(shape->local_bmin, shape->local_bmax);
    root->copy(shape, box, base_tree);
    Rfg_rbb_node = root;
    return rfg_rbb_get_actual_size_req(root);
}

// 0x00736340 (:952)
// Compacts the built tree into pool (or drops it when the pool is too small) and releases the build memory.
rfg_rbb_node* rfg_rbb_build_finish(mempool* pool)
{
    if (pool == nullptr)
    {
        return nullptr;
    }
    mempool_base* target = (mempool_base*)pool;
    const int size = rfg_rbb_get_actual_size_req(Rfg_rbb_node);
    if ((int)target->space_free() < size)
    {
        Rfg_rbb_node = nullptr;
    }
    else
    {
        const rfg_rbb_node* source = Rfg_rbb_node;
        unsigned char* dst = (unsigned char*)target->alloc(size, 4);
        int offset = RBB_NODE_SIZE;
        rfg_rbb_copy_nodes(dst, offset, (rfg_rbb_node*)dst, source);
        Rfg_rbb_node = (rfg_rbb_node*)dst;
    }
    get_this_context_virtual_mempool()->restore_to_mark(Rfg_rbb_mempool_mark);
    Rfg_rbb_building = false;
    return Rfg_rbb_node;
}

// 0x0071FB80 (:984)
// Bytes of the compact tree: 0x14 per node plus 4 per leaf index.
int rfg_rbb_get_actual_size_req(const rfg_rbb_node* root)
{
    const int num = root->num_objects;
    int nodes;
    if (num < 0)
    {
        nodes = 0;
    }
    else if (num < 1)
    {
        const rfg_rbb_node* children = rbb_children(root);
        nodes = rfg_rbb_count_tree_nodes(&children[1]) + 1 + rfg_rbb_count_tree_nodes(&children[0]);
    }
    else
    {
        nodes = 1;
    }
    if (num < 0)
    {
        return nodes * RBB_NODE_SIZE;
    }
    int leaves = num;
    if (num == 0)
    {
        const rfg_rbb_node* children = rbb_children(root);
        leaves = rfg_rbb_count_leaf_objects(&children[1]) + rfg_rbb_count_leaf_objects(&children[0]);
    }
    return (leaves + nodes * 5) * 4;
}

// 0x007363F0 (:1010)
rfg_rbb_node* rfg_rbb_read_tree(unsigned char* data, int& offset)
{
    const int start = offset;
    offset = start + RBB_NODE_SIZE;
    rfg_rbb_read_nodes((rfg_rbb_node*)(data + start), data, offset);
    return (rfg_rbb_node*)(data + start);
}
