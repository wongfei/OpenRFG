// Original: volition/vlib/code/util/solid_bsp.h
// Implementation: volition/vlib/code/util/solid_bsp.cpp
//
// Solid BSP trees (solid_bsp_tree, solid_bsp_node) loaded from data, with point and edge containment tests. Placement
// of the types inferred.
#pragma once


struct solid_bsp_plane;
class vector;

// size 0x6
struct solid_bsp_node
{
    unsigned short plane_index; // 0x00
    unsigned short back; // 0x02
    unsigned short front; // 0x04
};

// size 0x10
struct solid_bsp_tree
{
    int num_planes; // 0x00
    int num_nodes; // 0x04
    solid_bsp_plane* planes; // 0x08
    solid_bsp_node* nodes; // 0x0C
};

solid_bsp_tree* solid_bsp_load(const unsigned char*& data); // 0x00592570 (:213)
bool solid_bsp_test_point_inside(const solid_bsp_tree& bsp, const vector& pt, float solid_bsp_delta); // 0x005925A0 (:236)
bool does_solid_bsp_contain_edge(solid_bsp_tree* t, vector& p1, vector& p2, float solid_bsp_delta); // 0x00592680 (:275)
