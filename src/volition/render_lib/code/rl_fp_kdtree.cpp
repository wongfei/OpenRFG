// Original: volition/render_lib/code/rl_fp_kdtree.cpp
//
// rl_fp_kdtree: a static kd tree over 12.4 fixed point boxes. build() splits the leaves in place: each node keeps a
// contiguous leaf range, and a split takes the half box (along x, y or z) whose two sides have the smallest summed
// volume, moving the leaves that meet the half box to the front. A child's plane is its side's bound (left: its max,
// right: its min) so the queries reject a child whose side the query box cannot reach. Leaves of children with fewer
// than two leaves stay in the parent's range (offset 0), which the queries walk directly.

#include "volition/render_lib/code/rl_fp_kdtree.h"

#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_sphere.h"
#include "volition/vlib/code/memory/vmemory.h"

// Fixed point box bounds of an empty box (rl_fp_aabb_expand grows it).
static const short FP_AABB_EMPTY_MIN = 0x7FFF;
static const short FP_AABB_EMPTY_MAX = -0x8000;

// Node stack of build and the queries (int[514] / uint[513] on the stack).
static const int KDTREE_STACK_SIZE = 514;

// Split cost of a split that leaves one side empty.
static const int KDTREE_SPLIT_COST_MAX = 0x7FFFFFFF;

// 500 trees (ms_class_factory_info.m_objects_max at 0x01514B18; game_render_system_init raises it).
// <rl_fp_kdtree::factory_init> 0x0050E5F0 (:686)
// <rl_fp_kdtree::release> 0x0050E6F0 (:686)
// <rl_fp_kdtree::create> 0x005246B0 (:686)
RL_FACTORY_CLASS_IMPLEMENTATION(rl_fp_kdtree, 0xDEADB00B, 500)

// 0x007C0AF0 (:79)
void rl_fp_kdtree::set_objects_max(int objects_max)
{
    ms_class_factory_info.m_objects_max = objects_max;
}

// 0x004E50B0 (LTCG, inlined helper of rl_fp_aabb_expand)
static inline void vector_expand_fp_aabb(rl_fp_aabb* final_aabb, const rl_fp_aabb* expand_aabb)
{
    rl_fp_aabb_expand(final_aabb, expand_aabb);
}

// 0x004BECC0 (LTCG)
// Strict overlap (boxes that only touch do not intersect).
static inline unsigned int vector_intersect_fp_aabb(const rl_fp_aabb* aabb_a, const rl_fp_aabb* aabb_b)
{
    if (aabb_a->min_x < aabb_b->max_x && aabb_a->min_y < aabb_b->max_y && aabb_a->min_z < aabb_b->max_z && aabb_b->min_x < aabb_a->max_x && aabb_b->min_y < aabb_a->max_y && aabb_b->min_z < aabb_a->max_z)
    {
        return 1;
    }
    return 0;
}

static inline void set_empty(rl_fp_aabb& aabb)
{
    aabb.min_x = FP_AABB_EMPTY_MIN;
    aabb.min_y = FP_AABB_EMPTY_MIN;
    aabb.min_z = FP_AABB_EMPTY_MIN;
    aabb.max_x = FP_AABB_EMPTY_MAX;
    aabb.max_y = FP_AABB_EMPTY_MAX;
    aabb.max_z = FP_AABB_EMPTY_MAX;
}

// 0x004BEC80 (:48)
rl_fp_aabb_aligned::rl_fp_aabb_aligned(short _min_x, short _min_y, short _min_z, short _max_x, short _max_y, short _max_z)
{
    min_x = _min_x;
    min_y = _min_y;
    min_z = _min_z;
    max_x = _max_x;
    max_y = _max_y;
    max_z = _max_z;
}

// 0x004BEC70 (:47)
// Uninitialized.
rl_fp_aabb_aligned::rl_fp_aabb_aligned()
{
}

// 0x004BED10 (:148)
// How far the box reaches past the plane (> 0: it reaches the plane's side). The NEG planes bound a box from above
// (a left child's max), the POS planes from below (a right child's min); no plane always passes.
int intersect_fp_aabb_kdplane(const rl_fp_aabb* test_aabb, short plane_offset, unsigned short plane_axis)
{
    int result = 0;
    switch (plane_axis)
    {
    case RL_KDTREE_POS_X:
        return (int)test_aabb->max_x - (int)plane_offset;
    case RL_KDTREE_NEG_X:
        return (int)plane_offset - (int)test_aabb->min_x;
    case RL_KDTREE_POS_Y:
        return (int)test_aabb->max_y - (int)plane_offset;
    case RL_KDTREE_NEG_Y:
        return (int)plane_offset - (int)test_aabb->min_y;
    case RL_KDTREE_POS_Z:
        return (int)test_aabb->max_z - (int)plane_offset;
    case RL_KDTREE_NEG_Z:
        return (int)plane_offset - (int)test_aabb->min_z;
    case RL_KDTREE_NO_PLANE:
        result = 1;
    }
    return result;
}

// 0x004BEDB0 (:178)
// intersect_fp_aabb_kdplane for the sphere's box.
int intersect_fp_sphere_kdplane(const rl_fp_sphere* test_sphere, short plane_offset, unsigned short plane_axis)
{
    int result = 0;
    switch (plane_axis)
    {
    case RL_KDTREE_POS_X:
        return ((int)test_sphere->radius - (int)plane_offset) + (int)test_sphere->pt_x;
    case RL_KDTREE_NEG_X:
        return ((int)test_sphere->radius - (int)test_sphere->pt_x) + (int)plane_offset;
    case RL_KDTREE_POS_Y:
        return ((int)test_sphere->radius + (int)test_sphere->pt_y) - (int)plane_offset;
    case RL_KDTREE_NEG_Y:
        return ((int)test_sphere->radius - (int)test_sphere->pt_y) + (int)plane_offset;
    case RL_KDTREE_POS_Z:
        return ((int)test_sphere->radius + (int)test_sphere->pt_z) - (int)plane_offset;
    case RL_KDTREE_NEG_Z:
        return ((int)test_sphere->radius - (int)test_sphere->pt_z) + (int)plane_offset;
    case RL_KDTREE_NO_PLANE:
        result = 1;
    }
    return result;
}

// 0x004BCF90
// Uninitialized.
rl_fp_kdtree_leaf::rl_fp_kdtree_leaf()
{
}

// 0x004BCAE0 (.h:91)
rl_fp_kdtree::query_output::query_output()
{
    m_leaves = nullptr;
    m_num_leaves = 0;
    m_max_leaves = 0;
}

// 0x004BCAF0 (.h:94)
// Copies the leaf while there is room.
void rl_fp_kdtree::query_output::add_leaf(const rl_fp_kdtree_leaf& leaf)
{
    if (m_num_leaves < m_max_leaves && m_leaves != nullptr)
    {
        m_leaves[m_num_leaves] = leaf;
        m_num_leaves++;
    }
}

// 0x004BEE70 (:689)
rl_fp_kdtree::rl_fp_kdtree()
{
    m_num_leaves = 0;
    m_num_nodes = 0;
    m_leaves = nullptr;
    m_nodes = nullptr;
}

// 0x004BEE90 (:697)
rl_fp_kdtree::~rl_fp_kdtree()
{
}

// 0x00524720 (:706)
void rl_fp_kdtree::init(rl_fp_kdtree_node* node_array, int max_nodes, rl_fp_kdtree_leaf* leaf_array, int num_leaves)
{
    m_nodes = node_array;
    m_max_nodes = max_nodes;
    m_num_nodes = 0;
    m_leaves = leaf_array;
    m_num_leaves = num_leaves;
    build();
}

// 0x0050E770 (:721)
// A prebuilt tree: the nodes of the base data, the instance's own leaves.
void rl_fp_kdtree::init(rl_fp_kdtree_base_data* base_data, rl_fp_kdtree_leaf* inst_leaves)
{
    m_nodes = base_data->nodes.to_pointer();
    m_num_nodes = base_data->num_nodes;
    m_max_nodes = base_data->num_nodes;
    m_leaves = inst_leaves;
    m_num_leaves = base_data->num_leaves;
}

// 0x00524440 (:218)
// The node boxes live in the thread scratch pool for the duration of the build. The root is node 0 (all leaves, no
// plane); split nodes are pushed until their children have fewer than three leaves.
// Notes (decompiled behaviour, kept as is): with a single leaf (or more leaves than max_nodes + 1) nothing is built and
// m_num_nodes stays 0, so the queries walk node 0 of whatever the node array holds; the stack is not bounds checked.
void rl_fp_kdtree::build()
{
    mempool_base* pool = Vlib_scratch_pool;
    mempool_mark_scope scope(pool);
    if (m_leaves == nullptr || m_nodes == nullptr)
    {
        return;
    }
    if (m_num_leaves - 1 > m_max_nodes)
    {
        return;
    }
    rl_fp_aabb_aligned* node_aabbs = (rl_fp_aabb_aligned*)pool->alloc(m_max_nodes * sizeof(rl_fp_aabb_aligned), 0);
    if (m_num_leaves <= 1)
    {
        return;
    }
    m_num_nodes = 1;
    (rl_fp_aabb&)node_aabbs[0] = m_leaves[0].aabb;
    for (int i = 1; i < m_num_leaves; i++)
    {
        rl_fp_aabb_expand(&node_aabbs[0], &m_leaves[i].aabb);
    }
    m_nodes[0].leaf_offset = 0;
    m_nodes[0].num_leaves = (unsigned short)m_num_leaves;
    m_nodes[0].left_offset = 0;
    m_nodes[0].right_offset = 0;
    m_nodes[0].plane_axis = RL_KDTREE_NO_PLANE;

    int stack[KDTREE_STACK_SIZE];
    int top = 1;
    stack[1] = 0;
    do
    {
        const int parent_index = stack[top];
        rl_fp_kdtree_node& parent_node = m_nodes[parent_index];
        top--;
        rl_fp_kdtree_node left_node;
        rl_fp_aabb_aligned left_aabb;
        rl_fp_kdtree_node right_node;
        rl_fp_aabb_aligned right_aabb;
        split(left_node, left_aabb, right_node, right_aabb, parent_node, node_aabbs[parent_index]);
        if (left_node.num_leaves == 0 || right_node.num_leaves == 0)
        {
            parent_node.left_offset = 0;
            parent_node.right_offset = 0;
            continue;
        }
        if (left_node.num_leaves < 2)
        {
            parent_node.left_offset = 0;
        }
        else
        {
            rl_fp_kdtree_node& node = m_nodes[m_num_nodes];
            node = left_node;
            (rl_fp_aabb&)node_aabbs[m_num_nodes] = left_aabb;
            if (left_node.num_leaves < 3)
            {
                node.left_offset = 0;
                node.right_offset = 0;
            }
            else
            {
                top++;
                stack[top] = m_num_nodes;
            }
            parent_node.left_offset = (unsigned short)m_num_nodes;
            m_num_nodes++;
        }
        if (right_node.num_leaves < 2)
        {
            parent_node.right_offset = 0;
        }
        else
        {
            rl_fp_kdtree_node& node = m_nodes[m_num_nodes];
            node = right_node;
            (rl_fp_aabb&)node_aabbs[m_num_nodes] = right_aabb;
            if (right_node.num_leaves < 3)
            {
                node.left_offset = 0;
                node.right_offset = 0;
            }
            else
            {
                top++;
                stack[top] = m_num_nodes;
            }
            parent_node.right_offset = (unsigned short)m_num_nodes;
            m_num_nodes++;
        }
    } while (top > 0);
}

// 0x0050E140 (:383)
// Tries the lower half of the parent box along x, y and z and keeps the cheapest; then partitions the parent's leaf
// range in place (two cursors: the leaves meeting the half box go to the front) and sets the child planes.
// Notes (decompiled behaviour, kept as is): left_node/right_node only get num_leaves, leaf_offset and the plane; their
// child offsets and pad are left as the caller's uninitialized locals (build zeroes the offsets of the nodes it keeps
// as leaves and overwrites the others).
void rl_fp_kdtree::split(rl_fp_kdtree_node& left_node, rl_fp_aabb& left_aabb, rl_fp_kdtree_node& right_node, rl_fp_aabb& right_aabb, const rl_fp_kdtree_node& parent_node, const rl_fp_aabb& parent_aabb)
{
    int best_cost = KDTREE_SPLIT_COST_MAX;
    int best_axis = -1;
    rl_fp_aabb best_split;
    for (int axis = 0; axis < 3; axis++)
    {
        rl_fp_aabb split_aabb = parent_aabb;
        if (axis == 0)
        {
            split_aabb.max_x = (short)(((int)parent_aabb.max_x - (int)parent_aabb.min_x) >> 1) + parent_aabb.min_x;
        }
        else if (axis == 1)
        {
            split_aabb.max_y = (short)(((int)parent_aabb.max_y - (int)parent_aabb.min_y) >> 1) + parent_aabb.min_y;
        }
        else if (axis == 2)
        {
            split_aabb.max_z = (short)(((int)parent_aabb.max_z - (int)parent_aabb.min_z) >> 1) + parent_aabb.min_z;
        }
        const int cost = evalute_split_cost(split_aabb, best_split, parent_node);
        if (cost < best_cost)
        {
            best_split = split_aabb;
            best_cost = cost;
            best_axis = axis;
        }
    }
    set_empty(left_aabb);
    left_node.num_leaves = 0;
    set_empty(right_aabb);
    right_node.num_leaves = 0;
    if (best_cost == KDTREE_SPLIT_COST_MAX)
    {
        return;
    }

    rl_fp_kdtree_leaf* leaves = m_leaves + parent_node.leaf_offset;
    int hi = parent_node.num_leaves - 1;
    int lo = -1;
    int num_left = 0;
    if (hi >= 0)
    {
        for (;;)
        {
            if (lo < hi)
            {
                if (vector_intersect_fp_aabb(&best_split, &leaves[hi].aabb) == 0)
                {
                    right_node.num_leaves++;
                    rl_fp_aabb_expand(&right_aabb, &leaves[hi].aabb);
                    hi--;
                    continue;
                }
                left_node.num_leaves++;
                rl_fp_aabb_expand(&left_aabb, &leaves[hi].aabb);
            }
            // leaves[hi] stays on the left: advance lo to the next leaf that belongs on the right.
            num_left = hi + 1;
            if (hi == lo)
            {
                break;
            }
            bool done = false;
            for (;;)
            {
                lo++;
                if (hi <= lo)
                {
                    done = true;
                    break;
                }
                if (vector_intersect_fp_aabb(&best_split, &leaves[lo].aabb) == 0)
                {
                    break;
                }
                left_node.num_leaves++;
                rl_fp_aabb_expand(&left_aabb, &leaves[lo].aabb);
            }
            if (done)
            {
                break;
            }
            right_node.num_leaves++;
            rl_fp_aabb_expand(&right_aabb, &leaves[lo].aabb);
            const rl_fp_kdtree_leaf swap = leaves[lo];
            leaves[lo] = leaves[hi];
            leaves[hi] = swap;
            num_left = hi;
            if (hi - 1 <= lo)
            {
                break;
            }
            hi--;
        }
    }

    left_node.leaf_offset = parent_node.leaf_offset;
    right_node.leaf_offset = parent_node.leaf_offset + (unsigned short)num_left;
    if (best_axis == 0)
    {
        left_node.plane_offset = left_aabb.max_x;
        left_node.plane_axis = RL_KDTREE_NEG_X;
        right_node.plane_offset = right_aabb.min_x;
        right_node.plane_axis = RL_KDTREE_POS_X;
    }
    else if (best_axis == 1)
    {
        left_node.plane_offset = left_aabb.max_y;
        left_node.plane_axis = RL_KDTREE_NEG_Y;
        right_node.plane_offset = right_aabb.min_y;
        right_node.plane_axis = RL_KDTREE_POS_Y;
    }
    else
    {
        left_node.plane_offset = left_aabb.max_z;
        left_node.plane_axis = RL_KDTREE_NEG_Z;
        right_node.plane_offset = right_aabb.min_z;
        right_node.plane_axis = RL_KDTREE_POS_Z;
    }
}

// 0x004E50C0 (:644)
// The summed fixed point volumes (each product shifted back by 4 bits) of the leaves meeting split_left and of the rest.
// split_right is unused.
int rl_fp_kdtree::evalute_split_cost(const rl_fp_aabb& split_left, const rl_fp_aabb& split_right, const rl_fp_kdtree_node& parent_node)
{
    int num_right = 0;
    int num_left = 0;
    rl_fp_aabb left_aabb;
    rl_fp_aabb right_aabb;
    set_empty(left_aabb);
    set_empty(right_aabb);
    const unsigned int num_leaves = parent_node.num_leaves;
    if (num_leaves == 0)
    {
        return KDTREE_SPLIT_COST_MAX;
    }
    const rl_fp_kdtree_leaf* leaf = m_leaves + parent_node.leaf_offset;
    for (unsigned int i = 0; i < num_leaves; i++, leaf++)
    {
        if (vector_intersect_fp_aabb(&split_left, &leaf->aabb) != 0)
        {
            num_left++;
            rl_fp_aabb_expand(&left_aabb, &leaf->aabb);
        }
        else
        {
            num_right++;
            rl_fp_aabb_expand(&right_aabb, &leaf->aabb);
        }
    }
    if (num_right == 0 || num_left == 0)
    {
        return KDTREE_SPLIT_COST_MAX;
    }
    const int right_volume = ((((int)right_aabb.max_y - (int)right_aabb.min_y) * ((int)right_aabb.max_x - (int)right_aabb.min_x) >> 4) * ((int)right_aabb.max_z - (int)right_aabb.min_z)) >> 4;
    const int left_volume = ((((int)left_aabb.max_x - (int)left_aabb.min_x) * ((int)left_aabb.max_y - (int)left_aabb.min_y) >> 4) * ((int)left_aabb.max_z - (int)left_aabb.min_z)) >> 4;
    return right_volume + left_volume;
}

// 0x004E5260 (:921)
// The leaves whose boxes strictly overlap the fixed point query box. Nodes are walked from a stack (right child first);
// a split child with offset 0 contributes its leaf range directly.
void rl_fp_kdtree::query_aabb(query_output& output, const rl_aabb& aabb) const
{
    if (m_leaves == nullptr || m_nodes == nullptr || output.m_leaves == nullptr)
    {
        return;
    }
    rl_fp_aabb fp_aabb;
    fp_aabb.set(aabb);
    if (m_num_leaves < 2)
    {
        for (int i = 0; i < m_num_leaves; i++)
        {
            if (vector_intersect_fp_aabb(&m_leaves[i].aabb, &fp_aabb) != 0)
            {
                output.add_leaf(m_leaves[i]);
            }
        }
        return;
    }
    unsigned int stack[KDTREE_STACK_SIZE - 1];
    stack[0] = 0;
    int top = 1;
    do
    {
        const rl_fp_kdtree_node& node = m_nodes[stack[top - 1]];
        top--;
        if (intersect_fp_aabb_kdplane(&fp_aabb, node.plane_offset, node.plane_axis) <= 0)
        {
            continue;
        }
        if (node.left_offset == 0 && node.right_offset == 0)
        {
            for (int i = node.leaf_offset; i < (int)node.num_leaves + (int)node.leaf_offset; i++)
            {
                if (vector_intersect_fp_aabb(&m_leaves[i].aabb, &fp_aabb) != 0)
                {
                    output.add_leaf(m_leaves[i]);
                }
            }
            continue;
        }
        const rl_fp_kdtree_node& left_node = m_nodes[node.left_offset];
        if (node.left_offset == 0)
        {
            for (int i = node.leaf_offset; i < (int)m_nodes[node.right_offset].leaf_offset; i++)
            {
                if (vector_intersect_fp_aabb(&m_leaves[i].aabb, &fp_aabb) != 0)
                {
                    output.add_leaf(m_leaves[i]);
                }
            }
        }
        else
        {
            stack[top] = node.left_offset;
            top++;
        }
        if (node.right_offset == 0)
        {
            for (int i = (int)left_node.num_leaves + (int)left_node.leaf_offset; i < (int)node.num_leaves + (int)node.leaf_offset; i++)
            {
                if (vector_intersect_fp_aabb(&m_leaves[i].aabb, &fp_aabb) != 0)
                {
                    output.add_leaf(m_leaves[i]);
                }
            }
        }
        else
        {
            stack[top] = node.right_offset;
            top++;
        }
    } while (top > 0);
}

// 0x004E54B0 (:1035)
// query_aabb with the fixed point sphere (rl_fp_aabb_sphere_intersect per leaf).
void rl_fp_kdtree::query_sphere(query_output& output, const rl_sphere& sphere) const
{
    if (m_leaves == nullptr || m_nodes == nullptr || output.m_leaves == nullptr)
    {
        return;
    }
    rl_fp_sphere fp_sphere;
    fp_sphere.set(sphere);
    if (m_num_leaves < 2)
    {
        for (int i = 0; i < m_num_leaves; i++)
        {
            if (rl_fp_aabb_sphere_intersect(m_leaves[i].aabb, fp_sphere))
            {
                output.add_leaf(m_leaves[i]);
            }
        }
        return;
    }
    unsigned int stack[KDTREE_STACK_SIZE - 1];
    stack[0] = 0;
    int top = 1;
    do
    {
        const rl_fp_kdtree_node& node = m_nodes[stack[top - 1]];
        top--;
        if (intersect_fp_sphere_kdplane(&fp_sphere, node.plane_offset, node.plane_axis) <= 0)
        {
            continue;
        }
        if (node.left_offset == 0 && node.right_offset == 0)
        {
            for (int i = node.leaf_offset; i < (int)node.num_leaves + (int)node.leaf_offset; i++)
            {
                if (rl_fp_aabb_sphere_intersect(m_leaves[i].aabb, fp_sphere))
                {
                    output.add_leaf(m_leaves[i]);
                }
            }
            continue;
        }
        const rl_fp_kdtree_node& left_node = m_nodes[node.left_offset];
        if (node.left_offset == 0)
        {
            for (int i = node.leaf_offset; i < (int)m_nodes[node.right_offset].leaf_offset; i++)
            {
                if (rl_fp_aabb_sphere_intersect(m_leaves[i].aabb, fp_sphere))
                {
                    output.add_leaf(m_leaves[i]);
                }
            }
        }
        else
        {
            stack[top] = node.left_offset;
            top++;
        }
        if (node.right_offset == 0)
        {
            for (int i = (int)left_node.num_leaves + (int)left_node.leaf_offset; i < (int)node.num_leaves + (int)node.leaf_offset; i++)
            {
                if (rl_fp_aabb_sphere_intersect(m_leaves[i].aabb, fp_sphere))
                {
                    output.add_leaf(m_leaves[i]);
                }
            }
        }
        else
        {
            stack[top] = node.right_offset;
            top++;
        }
    } while (top > 0);
}

// 0x0055B670 (:736)
// Debug drawing of the node boxes, only called when RL_show_kdtree is set (off in retail); not ported.
void rl_fp_kdtree::render(rl_color_float depth_start, rl_color_float depth_end, int blend_depth)
{
}
