// Original: volition/render_lib/code/rl_fp_kdtree.h
// Implementation: volition/render_lib/code/rl_fp_kdtree.cpp
//
// rl_fp_kdtree, a static kd-tree over boxes (nodes, leaves and serialized base data) with box/frustum queries.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/vlib/code/util/endian_type.h"

class mempool;
class rl_class_factory_info;
class rl_color_float;
struct rl_fp_kdtree_base_data;
struct rl_fp_kdtree_leaf;
struct rl_fp_kdtree_node;
class rl_fp_sphere;
class rl_sphere;

enum rl_kdtree_plane_id
{
    RL_KDTREE_POS_X = 0,
    RL_KDTREE_NEG_X = 1,
    RL_KDTREE_POS_Y = 2,
    RL_KDTREE_NEG_Y = 3,
    RL_KDTREE_POS_Z = 4,
    RL_KDTREE_NEG_Z = 5,
    RL_KDTREE_NO_PLANE = 6,
    NUM_RL_KDTREE_PLANE_IDS = 7,
};

// size 0x10
class rl_fp_aabb_aligned : public rl_fp_aabb
{
public:
    rl_fp_aabb_aligned(short _min_x, short _min_y, short _min_z, short _max_x, short _max_y, short _max_z); // 0x004BEC80 (:48)
    rl_fp_aabb_aligned(); // 0x004BEC70 (:47)

private:
    int pad; // 0x0C
};

// size 0x1C
class rl_fp_kdtree : public rl_base_object
{
public:
    // size 0xC
    class query_output
    {
    public:
        query_output(); // 0x004BCAE0 (.h:91)
        void add_leaf(const rl_fp_kdtree_leaf& leaf); // 0x004BCAF0 (.h:94)

        rl_fp_kdtree_leaf* m_leaves; // 0x00
        int m_num_leaves; // 0x04
        int m_max_leaves; // 0x08
    };

protected:
    rl_fp_kdtree(); // 0x004BEE70 (:689)
    virtual ~rl_fp_kdtree(); // vtbl+0x00 0x004BEE90 (:697)
private:
    static const pool_list<rl_fp_kdtree*>& get_object_pool(); // 0x004BCAC0 (.h:79)
public:
    static void set_objects_max(int objects_max); // 0x007C0AF0 (.h:79)
    static int get_objects_max(); // 0x004BCAD0 (.h:79)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x0050E5F0 (:686)
    static rl_fp_kdtree* create_placement(rl_fp_kdtree* _data_p); // 0x004E5200 (:686)
    static rl_fp_kdtree* create(); // 0x005246B0 (:686)
    static bool can_create(); // 0x004E5230 (:686)
    virtual void release(); // vtbl+0x04 0x0050E6F0 (:686)
    void init(rl_fp_kdtree_base_data* base_data, rl_fp_kdtree_leaf* inst_leaves); // 0x0050E770 (:721)
    void init(rl_fp_kdtree_node* node_array, int max_nodes, rl_fp_kdtree_leaf* leaf_array, int num_leaves); // 0x00524720 (:706)
    void update_arrays(rl_fp_kdtree_node*, rl_fp_kdtree_leaf*);
    void query_aabb(query_output& output, const rl_aabb& aabb) const; // 0x004E5260 (:921)
    void query_sphere(query_output& output, const rl_sphere& sphere) const; // 0x004E54B0 (:1035)
    void render(rl_color_float depth_start, rl_color_float depth_end, int blend_depth); // 0x0055B670 (:736)
private:
    void build(); // 0x00524440 (:218)
    void split(rl_fp_kdtree_node& left_node, rl_fp_aabb& left_aabb, rl_fp_kdtree_node& right_node, rl_fp_aabb& right_aabb, const rl_fp_kdtree_node& parent_node, const rl_fp_aabb& parent_aabb); // 0x0050E140 (:383)
    int evalute_split_cost(const rl_fp_aabb& split_left, const rl_fp_aabb& split_right, const rl_fp_kdtree_node& parent_node); // 0x004E50C0 (:644)

public:
    static const unsigned int m___class_id; // 0x012BECD8
    static rl_class_factory_info ms_class_factory_info; // 0x01514AF8
protected:
    static pool_list<rl_fp_kdtree*> ms_object_pool; // 0x01514B28
private:
    int m_max_nodes; // 0x08
    int m_num_nodes; // 0x0C
    rl_fp_kdtree_node* m_nodes; // 0x10
    int m_num_leaves; // 0x14
    rl_fp_kdtree_leaf* m_leaves; // 0x18
};

// size 0x10
struct rl_fp_kdtree_leaf
{
    rl_fp_kdtree_leaf(); // 0x004BCF90

    rl_fp_aabb aabb; // 0x00
    unsigned int data; // 0x0C
};

// size 0x10
struct rl_fp_kdtree_node
{
    short plane_offset; // 0x00
    unsigned short plane_axis; // 0x02
    unsigned short left_offset; // 0x04
    unsigned short right_offset; // 0x06
    unsigned short leaf_offset; // 0x08
    unsigned short num_leaves; // 0x0A
    unsigned int pad; // 0x0C
};

// size 0x14
struct rl_fp_kdtree_base_data
{
    int num_nodes; // 0x00
    et_ptr_offset<rl_fp_kdtree_node,0> nodes; // 0x04
    int num_leaves; // 0x08
    et_ptr_offset<rl_fp_kdtree_leaf,0> leaves; // 0x0C
    et_ptr_offset<unsigned short,0> leaf_map; // 0x10
};

int intersect_fp_aabb_kdplane(const rl_fp_aabb* test_aabb, short plane_offset, unsigned short plane_axis); // 0x004BED10 (:148)
int intersect_fp_sphere_kdplane(const rl_fp_sphere* test_sphere, short plane_offset, unsigned short plane_axis); // 0x004BEDB0 (:178)
