// Original: volition/vlib/code/math/plane.cpp
//
// Partial: the default constructor, the two setters (a plane through three points, normal (b - a) x (c - b)
// normalized; a plane from a normal and a point), rotate, copy_rotate and find_closest_point. The rest is not implemented yet.

#include "volition/vlib/code/math/plane.h"

#include "volition/vlib/code/math/matrix.h"

#include <math.h>

// 0x004B5090 (.h:27)
plane::plane()
{
}

// 0x00578F30 (compiler-generated)
plane::plane(const plane& __that)
    : normal(__that.normal)
    , offset(__that.offset)
{
}

// 0x005A4EF0 (:36)
void plane::set(const vector& a, const vector& b, const vector& c)
{
    const float ex = c.x - b.x;
    const float ey = c.y - b.y;
    const float ez = c.z - b.z;
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float dz = b.z - a.z;
    normal.x = dy * ez - dz * ey;
    normal.y = ex * dz - dx * ez;
    normal.z = dx * ey - ex * dy;
    const float inv = 1.0f / sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    normal.x *= inv;
    normal.y *= inv;
    normal.z *= inv;
    offset = (normal.y * a.y + a.x * normal.x + a.z * normal.z) * -1.0f;
}

// 0x00578EB0 (:17)
void plane::set(const vector& norm, const vector& point)
{
    normal = norm;
    offset = (norm.y * point.y + norm.x * point.x + norm.z * point.z) * -1.0f;
}

// 0x005A5110 (:48)
// Into the frame orient/pos: the offset is taken at the plane's closest point to the origin (normal * -offset, here
// normal * offset with the sign convention of set) moved by pos, and the normal is rotated by orient (as rows).
// Notes (decompiled behaviour, kept as is): the offset is not negated back.
void plane::rotate(const matrix& orient, const vector& pos)
{
    const float old_offset = offset;
    const float nx = normal.x;
    const float ny = normal.y;
    const float nz = normal.z;
    offset = ny * (pos.y + ny * old_offset) + nx * (pos.x + nx * old_offset) + nz * (pos.z + nz * old_offset);
    normal.x = orient.rvec.y * ny + orient.rvec.x * nx + orient.rvec.z * nz;
    normal.y = orient.uvec.y * ny + orient.uvec.x * nx + orient.uvec.z * nz;
    normal.z = orient.fvec.y * ny + orient.fvec.x * nx + orient.fvec.z * nz;
}

// 0x005A5290 (:64)
plane plane::copy_rotate(const matrix& orient, const vector& pos) const
{
    plane result;
    result.offset = normal.y * (pos.y + normal.y * offset) + normal.x * (pos.x + normal.x * offset) + normal.z * (pos.z + normal.z * offset);
    result.normal.x = orient.rvec.y * normal.y + orient.rvec.x * normal.x + orient.rvec.z * normal.z;
    result.normal.y = orient.uvec.y * normal.y + orient.uvec.x * normal.x + orient.uvec.z * normal.z;
    result.normal.z = orient.fvec.y * normal.y + orient.fvec.x * normal.x + orient.fvec.z * normal.z;
    return result;
}

// 0x00578F50 (:152)
// v moved along the normal onto the plane (the normal is taken to be unit length).
vector plane::find_closest_point(const vector& v) const
{
    const float distance = v.y * normal.y + v.x * normal.x + v.z * normal.z + offset;
    return vector(v.x + -normal.x * distance, v.y + -normal.y * distance, v.z + -normal.z * distance);
}
