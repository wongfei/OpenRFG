// Original: volition/rfg/code/cutscene/cutscene_tween.h
//
// Tweens of cutscene values (float, vector, matrix, depth of field) between keys, with a blend mode, interpolated over
// the shot (the times are floats whatever T is). Written from the tweener<vector> instantiation.
#pragma once

#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/string_comp.h"

#include "keen/base/trace.hpp"

// 0x00705150 (:39)
// LTCG: p0 in ECX, p1 in EAX, the result in ESI. Only the forward vectors are interpolated.
static inline matrix lerp(const matrix& p0, const matrix& p1, float f)
{
    vector fvec;
    fvec.x = p0.fvec.x + (p1.fvec.x - p0.fvec.x) * f;
    fvec.y = p0.fvec.y + (p1.fvec.y - p0.fvec.y) * f;
    fvec.z = p0.fvec.z + (p1.fvec.z - p0.fvec.z) * f;
    fvec.normalize_safe();
    matrix result;
    result.make_quick(fvec);
    return result;
}

// <float> 0x00708210, <vector> 0x00708130 (:58)
// LTCG for <vector>: p0 in EDX, p1 in ECX.
template<typename T>
static inline T lerp(const T& p0, const T& p1, float f)
{
    return (1.0f - f) * p0 + f * p1;
}

// 0x00708210: the <float> instantiation evaluates at double precision and narrows once; <vector> narrows each step.
template<>
inline float lerp<float>(const float& p0, const float& p1, float f)
{
    return (float)((1.0 - (double)f) * (double)p0 + (double)p1 * (double)f);
}

// size (<vector>) 0x28
template<typename T>
class tweener
{
public:
    enum e_blend_mode
    {
        BLEND_LINEAR = 0,
        BLEND_PARAM_SOFT = 1,
        BLEND_PARAM_HARD = 2,
        BLEND_ACCEL = 3,
    };

    tweener(); // <vector> 0x00707380 (.h:68)
    bool is_keyed() const; // <vector> 0x00707390 (.h:70)
    bool done_blending(float) const;
    void set_values(const T&, const float, const T&, const float);
    void set_values(const T&);
    T& get_start_value();
    T& get_end_value();
    float get_current_scale(float cur_time_sec) const; // <vector> 0x00707A90 (.h:108)
    T get_current_value(float cur_time) const; // <vector> 0x0070A850 (.h:127)
    void set_blend_mode(const char* const);
    // Not in rfg.exe, which reads the two fields directly in cs_update_framebuffer_effects_in_shot.
    float get_start_time() const { return m_start_time; }
    float get_end_time() const { return m_end_time; }

private:
    bool m_is_keyed; // 0x00
    e_blend_mode blend_mode; // 0x04
    float m_start_time; // 0x08
    T m_start_value; // 0x0C
    float m_end_time; // 0x18
    T m_end_value; // 0x1C
};

// <vector> 0x00707380, <matrix> 0x00707740, <float> 0x007073A0, <cs_depth_of_field> 0x007073C0 (.h:68)
template<typename T>
tweener<T>::tweener() : m_is_keyed(false)
{
}

// <vector> 0x00707390, <float> 0x007073B0, <matrix> 0x00707750 (.h:70)
template<typename T>
bool tweener<T>::is_keyed() const
{
    return m_is_keyed;
}

// No instantiation is in rfg.exe.
template<typename T>
bool tweener<T>::done_blending(float cur_time) const
{
    return !m_is_keyed || m_end_time <= cur_time;
}

// <cs_depth_of_field> 0x00708370 (.h:88)
template<typename T>
void tweener<T>::set_values(const T& start_val, const float start_time, const T& end_val, const float end_time)
{
    m_is_keyed = true;
    m_start_value = start_val;
    m_end_value = end_val;
    m_start_time = start_time;
    m_end_time = end_time;
}

// <cs_depth_of_field> 0x00708310 (.h:79)
template<typename T>
void tweener<T>::set_values(const T& val)
{
    m_is_keyed = false;
    m_start_value = val;
    m_end_value = val;
    m_end_time = 0.0f;
    m_start_time = 0.0f;
}

// No instantiation is in rfg.exe.
template<typename T>
T& tweener<T>::get_start_value()
{
    return m_start_value;
}

// No instantiation is in rfg.exe.
template<typename T>
T& tweener<T>::get_end_value()
{
    return m_end_value;
}

// <vector> 0x00707A90, <float> 0x00707B30, <matrix> 0x00707D70 (.h:108)
template<typename T>
float tweener<T>::get_current_scale(float cur_time_sec) const
{
    const float scale = (float)(((double)cur_time_sec - (double)m_start_time) / ((double)m_end_time - (double)m_start_time));
    if (blend_mode == BLEND_LINEAR)
    {
        return scale;
    }
    if (blend_mode == BLEND_ACCEL)
    {
        return fl_ramp(scale, 1.0f);
    }
    // Magic numbers: the exponents of the soft and hard parametric curves.
    return fl_parametric_accel_decel(scale, blend_mode == BLEND_PARAM_SOFT ? 2 : 4);
}

// <vector> 0x0070A850, <float> 0x0070A9B0, <matrix> 0x0070ACE0 (.h:127)
template<typename T>
T tweener<T>::get_current_value(float cur_time) const
{
    if (m_start_time < cur_time && m_is_keyed)
    {
        if (m_end_time <= cur_time)
        {
            return m_end_value;
        }
        return lerp(m_start_value, m_end_value, get_current_scale(cur_time));
    }
    return m_start_value;
}

// <cs_depth_of_field> 0x007083D0 (.h:142)
template<typename T>
void tweener<T>::set_blend_mode(const char* const blend_name)
{
    if (v_stricmp(blend_name, "Linear") != 0)
    {
        if (v_stricmp(blend_name, "Soft Param") == 0)
        {
            blend_mode = BLEND_PARAM_SOFT;
            return;
        }
        if (v_stricmp(blend_name, "Hard Param") == 0)
        {
            blend_mode = BLEND_PARAM_HARD;
            return;
        }
        if (v_stricmp(blend_name, "Accel") == 0)
        {
            blend_mode = BLEND_ACCEL;
            return;
        }
        keen::debug::nativePrintArgs("Unknown blend mode");
    }
    blend_mode = BLEND_LINEAR;
}
