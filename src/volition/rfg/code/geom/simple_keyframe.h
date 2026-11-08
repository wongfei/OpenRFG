// Original: volition/rfg/code/geom/simple_keyframe.h
// Implementation: volition/rfg/code/geom/simple_keyframe.cpp
//
// Serialized linear key lists of floats, vectors and quaternions (values and times by offset), interpolated linearly
// (slerp for rotations).
#pragma once

#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/endian_type.h"

class matrix;

// size 0xC
class keylist_float
{
public:
    void init(); // 0x0080C130 (:32)
    float interp_linear(float time) const; // 0x00808B70 (:45)

    unsigned short num_keys; // 0x00
    et_ptr_offset<float,0> key_vals; // 0x04
    et_ptr_offset<float,0> key_times; // 0x08
};

// size 0xC
class keylist_vector
{
public:
    void init(); // 0x0080C150 (:99)
    vector interp_linear(float time) const; // 0x00808D40 (:112)

    unsigned short num_keys; // 0x00
    et_ptr_offset<vector,0> key_positions; // 0x04
    et_ptr_offset<float,0> key_times; // 0x08
};

// size 0xC
class keylist_quat
{
public:
    void init(); // 0x0080C170 (:167)
    matrix interp_slerp(float time) const; // 0x00809030 (:180)

    unsigned short num_keys; // 0x00
    et_ptr_offset<short_quat,0> key_rotations; // 0x04
    et_ptr_offset<float,0> key_times; // 0x08
};
