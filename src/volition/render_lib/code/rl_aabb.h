// Original: volition/render_lib/code/rl_aabb.h
// Implementation: volition/render_lib/code/rl_aabb.cpp
//
// rl_aabb and rl_fp_aabb, axis aligned boxes (the fp variant uses SIMD vectors).
#pragma once

#include "volition/vlib/code/math/bbox.h"

class rl_fp_sphere;
class vector;

// size 0x18
class rl_aabb : public bbox
{
public:
    rl_aabb(const rl_aabb& __that); // 0x004DCE80
    rl_aabb(const vector& vmin, const vector& vmax); // 0x004B7BA0 (.h:49)
    rl_aabb(); // 0x004DCAE0 (.h:45)
    ~rl_aabb(); // 0x004B7BD0 (.h:56)

};

// size 0xC
class rl_fp_aabb
{
public:
    rl_fp_aabb(short _min_x, short _min_y, short _min_z, short _max_x, short _max_y, short _max_z); // 0x004B7BF0 (.h:69)
    rl_fp_aabb(); // 0x004B7BE0 (.h:67)
    void set(const rl_fp_aabb& aabb); // 0x004B7C30 (.h:96)
    void set(const rl_aabb& aabb); // 0x004DCAF0 (.h:90)
    void get(rl_aabb& aabb); // 0x004DCBB0 (.h:106)
    void set_max(const vector& max); // 0x004B7C70 (.h:112)
    void set_min(const vector& min); // 0x004B7D20 (.h:125)
    void get_max(vector& max); // 0x004B7E80 (.h:138)
    void get_min(vector& min); // 0x004B7EE0 (.h:145)
    int get_volume(); // 0x004B7F40 (.h:152)

    short min_x; // 0x00
    short min_y; // 0x02
    short min_z; // 0x04
    short max_x; // 0x06
    short max_y; // 0x08
    short max_z; // 0x0A
};

void rl_fp_aabb_expand(rl_fp_aabb* orig_aabb, const rl_fp_aabb* expand_aabb); // 0x004DCC60 (.h:171)
bool rl_fp_aabb_intersect(const rl_fp_aabb& aabb_1, const rl_fp_aabb& aabb_2); // 0x004B7F70 (.h:199)
bool rl_fp_aabb_sphere_intersect(const rl_fp_aabb& aabb, const rl_fp_sphere& sphere); // 0x004BA040 (:5)
