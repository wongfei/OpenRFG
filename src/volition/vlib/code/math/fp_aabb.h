// Original: volition/vlib/code/math/fp_aabb.h
// Implementation: volition/vlib/code/math/fp_aabb.cpp
//
// Axis-aligned box quantized to signed 16-bit coordinates (destruction trees, rfg_rbb_node). set() rounds the minimum
// down and the maximum up so the box always contains the float box.
#pragma once

#include "volition/vlib/code/math/vector3.h"

// size 0xC
class fp_aabb
{
public:
    fp_aabb(short, short, short, short, short, short);
    fp_aabb(); // 0x00717470 (.h:36)
    void set(const vector& bmin, const vector& bmax); // 0x0056CAE0 (:76)
    void set(const fp_aabb& aabb); // 0x0056CAA0 (:66)
    void get(vector& bmin, vector& bmax) const; // 0x0056CCF0 (:113)
    void expand(const fp_aabb& expand_aabb); // 0x0059B2C0 (:146)
    void zero(); // 0x0056CDB0 (:165)

    short min_x; // 0x00
    short min_y; // 0x02
    short min_z; // 0x04
    short max_x; // 0x06
    short max_y; // 0x08
    short max_z; // 0x0A
};

bool fp_aabb_intersect(const fp_aabb& aabb_1, const fp_aabb& aabb_2); // 0x0056CDD0 (:180)
