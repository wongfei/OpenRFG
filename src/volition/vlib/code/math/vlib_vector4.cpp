// Original: volition/vlib/code/math/vlib_vector4.cpp
//
// Partial: the constructors from a vector (VFX.md FX3; with w for the mesh recompile, stub_impl.md S3) and rotate
// (gr_3d.cpp; moved here from matrix.cpp). The rest of the file is not recovered.

#include "volition/vlib/code/math/vector4.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/math/matrix.h"

// 0x005853E0 (:68)
// w is 0 (a direction).
vector4::vector4(const vector& _v)
    : x(_v.x)
    , y(_v.y)
    , z(_v.z)
    , w(0.0f)
{
}

// 0x00585410 (:85)
vector4::vector4(const vector& _v, float _w)
    : x(_v.x)
    , y(_v.y)
    , z(_v.z)
    , w(_w)
{
}

// 0x00585EF0 (:694)
// The row vector through the matrix (row 3 weighted by w).
void vector4::rotate(const matrix44& _m)
{
    const float vx = x;
    const float vy = y;
    const float vz = z;
    const float vw = w;
    x = _m.element(1, 0) * vy + _m.element(0, 0) * vx + _m.element(2, 0) * vz + _m.element(3, 0) * vw;
    y = _m.element(1, 1) * vy + _m.element(0, 1) * vx + _m.element(2, 1) * vz + _m.element(3, 1) * vw;
    z = _m.element(1, 2) * vy + _m.element(0, 2) * vx + _m.element(2, 2) * vz + _m.element(3, 2) * vw;
    w = _m.element(1, 3) * vy + _m.element(0, 3) * vx + _m.element(2, 3) * vz + _m.element(3, 3) * vw;
}
