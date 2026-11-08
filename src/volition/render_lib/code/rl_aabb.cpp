// Original: volition/render_lib/code/rl_aabb.cpp
//
// rl_aabb (a bbox) and rl_fp_aabb, a box in 12.4 fixed point (shorts in 1/16 m): set rounds the minimum down and the
// maximum up, so the fixed point box always contains the float box. rl_fp_aabb_sphere_intersect tests a fixed point box
// against a fixed point sphere. The header-inline members of rl_aabb.h are defined here as well.

#include "volition/render_lib/code/rl_aabb.h"

#include "volition/render_lib/code/rl_sphere.h"

#include <math.h>

// 0x004DCE80 (compiler-generated)
rl_aabb::rl_aabb(const rl_aabb& __that)
    : bbox(__that)
{
}

// 0x004B7BA0 (.h:49)
rl_aabb::rl_aabb(const vector& vmin, const vector& vmax)
{
    min = vmin;
    max = vmax;
}

// 0x004DCAE0 (.h:45)
// Uninitialized.
rl_aabb::rl_aabb()
{
}

// 0x004B7BD0 (.h:56)
rl_aabb::~rl_aabb()
{
}

// 0x004B7BE0 (.h:67)
// Uninitialized.
rl_fp_aabb::rl_fp_aabb()
{
}

// 0x004B7BF0 (.h:69)
rl_fp_aabb::rl_fp_aabb(short _min_x, short _min_y, short _min_z, short _max_x, short _max_y, short _max_z)
{
    min_x = _min_x;
    min_y = _min_y;
    min_z = _min_z;
    max_x = _max_x;
    max_y = _max_y;
    max_z = _max_z;
}

// 0x004B7C30 (.h:96)
void rl_fp_aabb::set(const rl_fp_aabb& aabb)
{
    min_x = aabb.min_x;
    min_y = aabb.min_y;
    min_z = aabb.min_z;
    max_x = aabb.max_x;
    max_y = aabb.max_y;
    max_z = aabb.max_z;
}

// 0x004DCAF0 (.h:90)
void rl_fp_aabb::set(const rl_aabb& aabb)
{
    set_max(aabb.max);
    set_min(aabb.min);
}

// 0x004DCBB0 (.h:106)
// Magic number: 0.0625 = 1/16, the fixed point scale.
void rl_fp_aabb::get(rl_aabb& aabb)
{
    aabb.max.x = (float)max_x * 0.0625f;
    aabb.max.y = (float)max_y * 0.0625f;
    aabb.max.z = (float)max_z * 0.0625f;
    aabb.min.x = (float)min_x * 0.0625f;
    aabb.min.y = (float)min_y * 0.0625f;
    aabb.min.z = (float)min_z * 0.0625f;
}

// 0x004B7C70 (.h:112)
// Rounded up (ceil).
void rl_fp_aabb::set_max(const vector& max)
{
    max_x = (short)(int)ceil((double)(max.x * 16.0f));
    max_y = (short)(int)ceil((double)(max.y * 16.0f));
    max_z = (short)(int)ceil((double)(max.z * 16.0f));
}

// 0x004B7D20 (.h:125)
// Rounded down (an inlined floor).
void rl_fp_aabb::set_min(const vector& min)
{
    min_x = (short)(int)floor((double)(min.x * 16.0f));
    min_y = (short)(int)floor((double)(min.y * 16.0f));
    min_z = (short)(int)floor((double)(min.z * 16.0f));
}

// 0x004B7E80 (.h:138)
void rl_fp_aabb::get_max(vector& max)
{
    max.x = (float)max_x * 0.0625f;
    max.y = (float)max_y * 0.0625f;
    max.z = (float)max_z * 0.0625f;
}

// 0x004B7EE0 (.h:145)
void rl_fp_aabb::get_min(vector& min)
{
    min.x = (float)min_x * 0.0625f;
    min.y = (float)min_y * 0.0625f;
    min.z = (float)min_z * 0.0625f;
}

// 0x004B7F40 (.h:152)
// The volume in fixed point (each product shifted back by 4 bits).
int rl_fp_aabb::get_volume()
{
    return (((max_y - min_y) * (max_x - min_x)) >> 4) * (max_z - min_z) >> 4;
}

// 0x004DCC60 (.h:171)
void rl_fp_aabb_expand(rl_fp_aabb* orig_aabb, const rl_fp_aabb* expand_aabb)
{
    const short new_min_x = orig_aabb->min_x < expand_aabb->min_x ? orig_aabb->min_x : expand_aabb->min_x;
    const short new_min_y = orig_aabb->min_y < expand_aabb->min_y ? orig_aabb->min_y : expand_aabb->min_y;
    const short new_min_z = orig_aabb->min_z < expand_aabb->min_z ? orig_aabb->min_z : expand_aabb->min_z;
    const short new_max_x = expand_aabb->max_x < orig_aabb->max_x ? orig_aabb->max_x : expand_aabb->max_x;
    const short new_max_y = expand_aabb->max_y < orig_aabb->max_y ? orig_aabb->max_y : expand_aabb->max_y;
    const short new_max_z = expand_aabb->max_z < orig_aabb->max_z ? orig_aabb->max_z : expand_aabb->max_z;
    orig_aabb->min_x = new_min_x;
    orig_aabb->min_y = new_min_y;
    orig_aabb->min_z = new_min_z;
    orig_aabb->max_x = new_max_x;
    orig_aabb->max_y = new_max_y;
    orig_aabb->max_z = new_max_z;
}

// 0x004B7F70 (.h:199)
// Strict overlap on all three axes (touching boxes do not intersect).
bool rl_fp_aabb_intersect(const rl_fp_aabb& aabb_1, const rl_fp_aabb& aabb_2)
{
    return aabb_1.min_x < aabb_2.max_x && aabb_1.min_y < aabb_2.max_y && aabb_1.min_z < aabb_2.max_z && aabb_2.min_x < aabb_1.max_x && aabb_2.min_y < aabb_1.max_y && aabb_2.min_z < aabb_1.max_z;
}

// 0x004BA040 (:5)
// The squared distance from the sphere centre to the box (fixed point, each square shifted back by 4 bits) against the
// squared radius.
bool rl_fp_aabb_sphere_intersect(const rl_fp_aabb& aabb, const rl_fp_sphere& sphere)
{
    int dist_sq = 0;
    if (sphere.pt_x < aabb.min_x)
    {
        dist_sq = ((sphere.pt_x - aabb.min_x) * (sphere.pt_x - aabb.min_x)) >> 4;
    }
    else if (aabb.max_x < sphere.pt_x)
    {
        dist_sq = ((sphere.pt_x - aabb.max_x) * (sphere.pt_x - aabb.max_x)) >> 4;
    }
    if (sphere.pt_y < aabb.min_y)
    {
        dist_sq += ((sphere.pt_y - aabb.min_y) * (sphere.pt_y - aabb.min_y)) >> 4;
    }
    else if (aabb.max_y < sphere.pt_y)
    {
        dist_sq += ((sphere.pt_y - aabb.max_y) * (sphere.pt_y - aabb.max_y)) >> 4;
    }
    if (sphere.pt_z < aabb.min_z)
    {
        dist_sq += ((sphere.pt_z - aabb.min_z) * (sphere.pt_z - aabb.min_z)) >> 4;
    }
    else if (aabb.max_z < sphere.pt_z)
    {
        dist_sq += ((sphere.pt_z - aabb.max_z) * (sphere.pt_z - aabb.max_z)) >> 4;
    }
    return dist_sq <= (sphere.radius * sphere.radius) >> 4;
}
