// Original: volition/rfg/code/geom/simple_keyframe.cpp
//
// Serialized key lists, interpolated by time. Before the first key a list answers its first value, from the last key on
// its last value. rfg.exe's interp functions unroll the key search four keys at a time; a plain walk finds the same key.

#include "volition/rfg/code/geom/simple_keyframe.h"

#include "volition/vlib/code/math/matrix.h"

// The first key whose time is after the given time; the caller has checked that one exists, and that it is not key 0.
static int keylist_find_next_key(const float* const key_times, const int num_keys, const float time)
{
    int key = 0;
    while (key < num_keys && !(time < key_times[key]))
    {
        key++;
    }
    return key;
}

// 0x0080C130 (:32)
void keylist_float::init()
{
    num_keys = 0;
    key_vals.from_pointer(nullptr);
    key_times.from_pointer(nullptr);
}

// 0x00808B70 (:45)
float keylist_float::interp_linear(float time) const
{
    if (num_keys == 0)
    {
        return 0.0f;
    }
    const float* const vals = key_vals.to_pointer();
    const float* const times = key_times.to_pointer();
    if (num_keys == 1 || !(times[0] < time))
    {
        return vals[0];
    }
    if (times[num_keys - 1] <= time)
    {
        return vals[num_keys - 1];
    }
    const int key = keylist_find_next_key(times, num_keys, time);
    const float span = times[key] - times[key - 1];
    if (0.0f < span)
    {
        const float pct = (time - times[key - 1]) / span;
        return (1.0f - pct) * vals[key - 1] + vals[key] * pct;
    }
    return vals[key];
}

// 0x0080C150 (:99)
void keylist_vector::init()
{
    num_keys = 0;
    key_positions.from_pointer(nullptr);
    key_times.from_pointer(nullptr);
}

// 0x00808D40 (:112)
vector keylist_vector::interp_linear(float time) const
{
    if (num_keys == 0)
    {
        return vector(0.0f, 0.0f, 0.0f);
    }
    const vector* const positions = key_positions.to_pointer();
    const float* const times = key_times.to_pointer();
    if (num_keys == 1 || !(times[0] < time))
    {
        return positions[0];
    }
    if (!(time < times[num_keys - 1]))
    {
        return positions[num_keys - 1];
    }
    const int key = keylist_find_next_key(times, num_keys, time);
    const float span = times[key] - times[key - 1];
    if (0.0f < span)
    {
        const float pct = (time - times[key - 1]) / span;
        const float inverse = 1.0f - pct;
        const vector& from = positions[key - 1];
        const vector& to = positions[key];
        return vector(from.x * inverse + to.x * pct, from.y * inverse + to.y * pct, from.z * inverse + to.z * pct);
    }
    return positions[key];
}

// 0x0080C170 (:167)
void keylist_quat::init()
{
    num_keys = 0;
    key_rotations.from_pointer(nullptr);
    key_times.from_pointer(nullptr);
}

// 0x00809030 (:180)
matrix keylist_quat::interp_slerp(float time) const
{
    matrix result;
    result.make_identity();
    if (num_keys == 0)
    {
        return result;
    }
    const short_quat* const rotations = key_rotations.to_pointer();
    const float* const times = key_times.to_pointer();
    quaternion rotation;
    if (num_keys == 1 || time <= times[0])
    {
        rotation = quaternion(rotations[0]);
    }
    else if (!(time < times[num_keys - 1]))
    {
        rotation = quaternion(rotations[num_keys - 1]);
    }
    else
    {
        const int key = keylist_find_next_key(times, num_keys, time);
        const quaternion from(rotations[key - 1]);
        const quaternion to(rotations[key]);
        const float span = times[key] - times[key - 1];
        if (0.0f < span)
        {
            pltind_slerp(rotation, from, to, (time - times[key - 1]) / span);
        }
        else
        {
            rotation = to;
        }
    }
    rotation.extract_matrix(result);
    return result;
}
