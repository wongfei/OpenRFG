// Original: volition/rfg/code/geom/keyframe.h
// Implementation: volition/rfg/code/geom/keyframe.cpp
//
// Keyframe lists of a bone track: translation and scale keys with Hermite tangents, rotation keys as short_quat with
// precomputed derivatives for smooth interpolation; get_local_* evaluate a track at a time in milliseconds.
#pragma once

#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/math/vector3.h"

// size 0x4
class keyframe
{
public:
    keyframe(int);

    int time; // 0x00
};

// size 0x28
class translation_key : public keyframe
{
public:
    vector translation; // 0x04
    vector in_tangent; // 0x10
    vector out_tangent; // 0x1C
};

// size 0xC
class rotation_key : public keyframe
{
public:
    short_quat rotation; // 0x04
};

// size 0x28
class scale_key : public keyframe
{
public:
    vector scale; // 0x04
    vector in_tangent; // 0x10
    vector out_tangent; // 0x1C
};

// size 0x1C
class keyframe_list
{
    void interpolate_scale_value(vector& val, int n0, int n1, float u); // 0x007F3600 (:146)
public:
    void init(); // 0x007F3AE0 (:171)
    int get_translation_key_time(int);
    int get_rotation_key_time(int);
    int get_scale_key_time(int);
    void get_rotation_range(int& start, int& end); // 0x007F3B00 (:188)
    vector& get_key_translation(int n); // 0x007F20C0 (.h:138)
    short_quat& get_key_rotation(int n); // 0x007F20E0 (.h:139)
    vector& get_key_scale(int n); // 0x007F2100 (.h:140)
    vector& get_translation_key_in_tangent(int n); // 0x007F2120 (.h:143)
    vector& get_translation_key_out_tangent(int n); // 0x007F2140 (.h:144)
    short_quat& get_rot_derivative_of_a(int n); // 0x007F2160 (.h:145)
    short_quat& get_rot_derivative_of_b(int n); // 0x007F2170 (.h:146)
    vector& get_scale_key_in_tangent(int n); // 0x007F2180 (.h:147)
    vector& get_scale_key_out_tangent(int n); // 0x007F21A0 (.h:148)
    void prepare(); // 0x007F3B40 (:200)
    void prepare_anim(); // 0x007F3B30 (:194)
    vector get_local_translation(int time); // 0x007F3D00 (:221)
    vector get_translation_difference(int time_start, int time_end); // 0x007F3E30 (:266)
    quaternion get_local_rotation(int time, bool smooth_interpolation); // 0x007F3EF0 (:285)
    vector get_local_scale(int time); // 0x007F4000 (:330)

    unsigned short num_translation_keys; // 0x00
    unsigned short num_rotation_keys; // 0x02
    unsigned short num_scale_keys; // 0x04
    translation_key* translation_keys; // 0x08
    rotation_key* rotation_keys; // 0x0C
    scale_key* scale_keys; // 0x10
    short_quat* rot_derivatives_a; // 0x14
    short_quat* rot_derivatives_b; // 0x18
};
