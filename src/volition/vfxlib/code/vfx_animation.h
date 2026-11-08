// Original: volition/vfxlib/code/vfx_animation.h
//
// Animation keys (float, vector, vector4, quaternion) and their interpolation: evenly spaced keys by percentage, or keys
// with their own times (varkey). Placement of the key types inferred (the interpolators take them).
#pragma once

#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/math/vector4.h"

#include <math.h>

// size 0x8
struct vfx_float_key
{
    float time; // 0x00
    float value; // 0x04
};

// size 0x10
struct vfx_vector_key
{
    float time; // 0x00
    vector value; // 0x04
};

// size 0x20
struct vfx_vector4_key
{
    float time; // 0x00
    vector4 value; // 0x10
};

// size 0x14
struct vfx_quat_key
{
    float time; // 0x00
    quaternion value; // 0x04
};

template<typename K, typename T>
void vfx_interpolate_varkey(T& ret, int num_keys, K* keys, float cur_time); // (.h:75)

float vfx_interpolate_float(float percent, int num_keys, vfx_float_key* keys); // 0x00C9B890 (.h:48)
// template instance: void vfx_interpolate_varkey<vfx_float_key,float>(float& ret, int num_keys, vfx_float_key* keys, float cur_time); // 0x005D35E0 (.h:75)
float vfx_interpolate_float_varkey(int num_keys, vfx_float_key* keys, float cur_time); // 0x005D4370 (.h:117)
vector4 vfx_interpolate_vector4(float percent, int num_keys, vfx_vector4_key* keys); // 0x00C9B990 (.h:140)
void vfx_interpolate_vector(vector& result, float percent, int num_keys, vfx_vector_key* keys); // 0x00C9BC40 (.h:176)
void vfx_interpolate_quaternion(quaternion& result, float percent, int num_keys, vfx_quat_key* keys); // 0x00C9BE50 (.h:203)

// Evenly spaced keys: the key pair around percent * (num_keys - 1), floor and ceil of it (in double, as the original
// inlined floor/ceil).
inline void vfx_key_pair(float position, int& lo, int& hi)
{
    lo = (int)floor((double)position);
    hi = (int)ceil((double)position);
}

// <vfx_float_key,float> 0x005D35E0 (.h:75): keys with their own times; before the first key or after the last the end
// value holds.
template<typename K, typename T>
inline void vfx_interpolate_varkey(T& ret, int num_keys, K* keys, float cur_time)
{
    if (num_keys == 1 || cur_time == 0.0f)
    {
        ret = keys[0].value;
        return;
    }
    int prev = -1;
    for (int i = 0; i < num_keys; i++)
    {
        if (cur_time <= keys[i].time)
        {
            if (keys[i].time <= cur_time)
            {
                ret = keys[i].value;
                return;
            }
            if (prev > -1)
            {
                ret = ((cur_time - keys[prev].time) / (keys[i].time - keys[prev].time)) * (keys[i].value - keys[prev].value) + keys[prev].value;
                return;
            }
            ret = keys[i].value;
            return;
        }
        prev = i;
    }
    ret = keys[num_keys - 1].value;
}

// 0x00C9B890 (.h:48)
inline float vfx_interpolate_float(float percent, int num_keys, vfx_float_key* keys)
{
    if (num_keys == 1)
    {
        return keys[0].value;
    }
    const float position = (float)(num_keys - 1) * percent;
    int lo;
    int hi;
    vfx_key_pair(position, lo, hi);
    return keys[lo].value + (position - (float)lo) * (keys[hi].value - keys[lo].value);
}

// 0x005D4370 (.h:117)
inline float vfx_interpolate_float_varkey(int num_keys, vfx_float_key* keys, float cur_time)
{
    float result;
    vfx_interpolate_varkey<vfx_float_key, float>(result, num_keys, keys, cur_time);
    return result;
}

// 0x00C9B990 (.h:140)
inline vector4 vfx_interpolate_vector4(float percent, int num_keys, vfx_vector4_key* keys)
{
    vector4 result;
    if (num_keys == 1)
    {
        result.x = keys[0].value.x;
        result.y = keys[0].value.y;
        result.z = keys[0].value.z;
        result.w = keys[0].value.w;
        return result;
    }
    float position = (float)(num_keys - 1) * percent;
    int lo;
    int hi;
    vfx_key_pair(position, lo, hi);
    const float t = position - (float)lo;
    if (lo == hi)
    {
        result.x = keys[lo].value.x;
        result.y = keys[lo].value.y;
        result.z = keys[lo].value.z;
        result.w = keys[lo].value.w;
    }
    else
    {
        result.x = keys[lo].value.x + (keys[hi].value.x - keys[lo].value.x) * t;
        result.y = keys[lo].value.y + (keys[hi].value.y - keys[lo].value.y) * t;
        result.z = keys[lo].value.z + (keys[hi].value.z - keys[lo].value.z) * t;
        result.w = keys[lo].value.w + (keys[hi].value.w - keys[lo].value.w) * t;
    }
    return result;
}

// 0x00C9BC40 (.h:176)
inline void vfx_interpolate_vector(vector& result, float percent, int num_keys, vfx_vector_key* keys)
{
    if (num_keys == 1)
    {
        result.x = keys[0].value.x;
        result.y = keys[0].value.y;
        result.z = keys[0].value.z;
        return;
    }
    const float position = (float)(num_keys - 1) * percent;
    int lo;
    int hi;
    vfx_key_pair(position, lo, hi);
    const float t = position - (float)lo;
    if (lo != hi)
    {
        result.x = keys[lo].value.x + (keys[hi].value.x - keys[lo].value.x) * t;
        result.y = keys[lo].value.y + (keys[hi].value.y - keys[lo].value.y) * t;
        result.z = keys[lo].value.z + (keys[hi].value.z - keys[lo].value.z) * t;
        return;
    }
    result.x = keys[lo].value.x;
    result.y = keys[lo].value.y;
    result.z = keys[lo].value.z;
}

// 0x00C9BE50 (.h:203)
inline void vfx_interpolate_quaternion(quaternion& result, float percent, int num_keys, vfx_quat_key* keys)
{
    if (num_keys == 1)
    {
        result = keys[0].value;
        return;
    }
    const float position = (float)(num_keys - 1) * percent;
    int lo;
    int hi;
    vfx_key_pair(position, lo, hi);
    if (lo != hi)
    {
        pltind_slerp(result, keys[lo].value, keys[hi].value, position - (float)lo);
        return;
    }
    result = keys[lo].value;
}
