// Original: volition/vlib/code/math/fp_aabb.cpp
//
// Fixed-point boxes (decompiled): coordinates are 1/16 m in shorts; set() rounds the minimum down and the maximum up,
// so the box always contains the float box. The intersection test is strict (touching boxes do not intersect).

#include "volition/vlib/code/math/fp_aabb.h"

#include <math.h>

// 0x00717470 (.h:36)
fp_aabb::fp_aabb()
{
}

// 0x0056CAE0 (:76)
void fp_aabb::set(const vector& bmin, const vector& bmax)
{
    max_x = (short)(int)ceil((double)(bmax.x * 16.0f));
    max_y = (short)(int)ceil((double)(bmax.y * 16.0f));
    max_z = (short)(int)ceil((double)(bmax.z * 16.0f));
    min_x = (short)(int)floor((double)(bmin.x * 16.0f));
    min_y = (short)(int)floor((double)(bmin.y * 16.0f));
    min_z = (short)(int)floor((double)(bmin.z * 16.0f));
}

// 0x0056CAA0 (:66)
void fp_aabb::set(const fp_aabb& aabb)
{
    min_x = aabb.min_x;
    min_y = aabb.min_y;
    min_z = aabb.min_z;
    max_x = aabb.max_x;
    max_y = aabb.max_y;
    max_z = aabb.max_z;
}

// 0x0056CCF0 (:113)
void fp_aabb::get(vector& bmin, vector& bmax) const
{
    bmax.x = (float)(int)max_x * 0.0625f;
    bmax.y = (float)(int)max_y * 0.0625f;
    bmax.z = (float)(int)max_z * 0.0625f;
    bmin.x = (float)(int)min_x * 0.0625f;
    bmin.y = (float)(int)min_y * 0.0625f;
    bmin.z = (float)(int)min_z * 0.0625f;
}

// 0x0059B2C0 (:146)
void fp_aabb::expand(const fp_aabb& expand_aabb)
{
    min_x = min_x < expand_aabb.min_x ? min_x : expand_aabb.min_x;
    min_y = min_y < expand_aabb.min_y ? min_y : expand_aabb.min_y;
    min_z = min_z < expand_aabb.min_z ? min_z : expand_aabb.min_z;
    max_x = expand_aabb.max_x < max_x ? max_x : expand_aabb.max_x;
    max_y = expand_aabb.max_y < max_y ? max_y : expand_aabb.max_y;
    max_z = expand_aabb.max_z < max_z ? max_z : expand_aabb.max_z;
}

// 0x0056CDB0 (:165)
void fp_aabb::zero()
{
    min_x = 0;
    min_y = 0;
    min_z = 0;
    max_x = 0;
    max_y = 0;
    max_z = 0;
}

// 0x0056CDD0 (:180)
bool fp_aabb_intersect(const fp_aabb& aabb_1, const fp_aabb& aabb_2)
{
    return aabb_1.min_x < aabb_2.max_x && aabb_1.min_y < aabb_2.max_y && aabb_1.min_z < aabb_2.max_z && aabb_2.min_x < aabb_1.max_x &&
           aabb_2.min_y < aabb_1.max_y && aabb_2.min_z < aabb_1.max_z;
}
