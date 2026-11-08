// Original: volition/render_lib/code/rl_fp_rbb.h
// Implementation: volition/render_lib/code/rl_fp_rbb.cpp
//
// rl_fp_rbb_tree, a recursive bounding box tree (nodes and leaves) with queries.
#pragma once

#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_base_object.h"

class rl_class_dynamic_info;
class rl_color_float;
struct rl_fp_rbb_leaf;
struct rl_fp_rbb_node;
class rl_sphere;

// size 0x10
class rl_fp_aabb_aligned_rbb : public rl_fp_aabb
{
public:
    rl_fp_aabb_aligned_rbb(short _min_x, short _min_y, short _min_z, short _max_x, short _max_y, short _max_z); // 0x004BEEE0 (:47)
    rl_fp_aabb_aligned_rbb(); // 0x004BEED0 (:46)

private:
    int pad; // 0x0C
};

// size 0x1C
class rl_fp_rbb_tree : public rl_base_object
{
public:
    // size 0xC
    class query_output
    {
    public:
        query_output();
        void add_leaf(const rl_fp_rbb_leaf& leaf); // 0x004BEEA0 (.h:69)

        rl_fp_rbb_leaf* m_leaves; // 0x00
        int m_num_leaves; // 0x04
        int m_max_leaves; // 0x08
    };

protected:
    rl_fp_rbb_tree(); // 0x004BEFB0 (:556)
    virtual ~rl_fp_rbb_tree(); // vtbl+0x00 0x004BEFD0 (:564)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_fp_rbb_tree* create(); // 0x004E5880 (:553)
    static bool can_create(); // 0x004BEF80 (:553)
    virtual void release(); // vtbl+0x04 0x004BEF90 (:553)
    void init(rl_fp_rbb_node* node_array, int max_nodes, rl_fp_rbb_leaf* leaf_array, int num_leaves); // 0x00524930 (:573)
    void update_arrays(rl_fp_rbb_node*, rl_fp_rbb_leaf*);
    void query_aabb(query_output& output, const rl_aabb& aabb) const; // 0x004E58F0 (:731)
    void query_sphere(query_output& output, const rl_sphere& sphere) const; // 0x004E5AF0 (:830)
    void render(rl_color_float depth_start, rl_color_float depth_end, int blend_depth); // 0x0055C470 (:590)
private:
    void build(); // 0x00524750 (:150)
    void split(rl_fp_rbb_node& left_node, rl_fp_rbb_node& right_node, const rl_fp_rbb_node& parent_node); // 0x0050E7A0 (:282)
    int evalute_split_cost(const rl_fp_aabb& split_left, const rl_fp_aabb& split_right, const rl_fp_rbb_node& parent_node); // 0x004E5740 (:511)

public:
    static rl_class_dynamic_info ms_class_dynamic_info; // 0x01514B40
private:
    int m_max_nodes; // 0x08
    int m_num_nodes; // 0x0C
    rl_fp_rbb_node* m_nodes; // 0x10
    int m_num_leaves; // 0x14
    rl_fp_rbb_leaf* m_leaves; // 0x18
};

// size 0x10
struct rl_fp_rbb_leaf
{
    rl_fp_aabb aabb; // 0x00
    unsigned int data; // 0x0C
};

// size 0x20
struct rl_fp_rbb_node
{
    rl_fp_rbb_node(); // 0x004BEF70

    rl_fp_aabb aabb; // 0x00
    int pad; // 0x0C
    unsigned int left_offset; // 0x10
    unsigned int right_offset; // 0x14
    unsigned int leaf_offset; // 0x18
    unsigned int num_leaves; // 0x1C
};
