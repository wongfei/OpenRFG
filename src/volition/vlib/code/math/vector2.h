// Original: volition/vlib/code/math/vector2.h
// Implementation: volition/vlib/code/math/vlib_vector2.cpp
//
// 2D vector and 2D line/segment helpers.
#pragma once

#include <math.h>

class vector;

// size 0x8
class vector2
{
public:
    vector2(float _x, float _y); // 0x0045AC10 (.h:44)
    vector2(); // 0x0045AC00 (.h:41)
    void set(float _x, float _y); // 0x0045AC30 (.h:56)
    vector2 operator-(const vector2& v) const; // 0x005636E0 (.h:88)
    vector2 operator-() const; // 0x006C3910 (.h:76)
    vector2 operator+(const vector2& v) const; // 0x0045AC50 (.h:82)
    float operator*(const vector2& v) const; // 0x00563770 (.h:172)
    vector2 operator*(float scale) const; // 0x0045ACA0 (.h:94)
    vector2 operator/(float scale) const; // 0x006F6870 (.h:99)
    void operator+=(const vector2& v); // 0x0045ACE0 (.h:105)
    void add_scalar(float);
    void subtract_scalar(float);
    void operator-=(const vector2& v); // 0x00563730 (.h:138)
    void operator*=(float scale); // 0x005DB440 (.h:149)
    void operator/=(float);
    vector operator^(const vector2&) const;
    void operator()(float _x, float _y); // 0x0083F400 (.h:184)
    bool operator==(const vector2& v) const; // 0x00563790 (.h:193)
    bool operator!=(const vector2& v) const; // 0x0083F420 (.h:203)
    float mag() const; // 0x005637D0 (.h:213)
    float mag_squared() const; // 0x00563800 (.h:221)
    float mag_and_normalize(); // 0x0057F960 (vlib_vector2.cpp:74)
    float mag_and_normalize_safe(); // 0x0057F9D0 (vlib_vector2.cpp:93)
    void normalize(); // 0x0057FA60 (vlib_vector2.cpp:114)
    void normalize_safe(); // 0x0057FAC0 (vlib_vector2.cpp:128)
    void negate(); // 0x0057FB40 (vlib_vector2.cpp:145)
    void zero(); // 0x0057FFB0 (vlib_vector2.cpp:230)
    bool is_null() const; // 0x0057FFC0 (vlib_vector2.cpp:240)
    bool is_valid() const; // 0x0057FFF0 (vlib_vector2.cpp:249)

    float x; // 0x00
    float y; // 0x04
};

// 0x0045AC00 (.h:41)
// Uninitialized.
inline vector2::vector2()
{
}

// 0x0045AC10 (.h:44)
inline vector2::vector2(float _x, float _y)
    : x(_x)
    , y(_y)
{
}

// 0x0045AC30 (.h:56)
inline void vector2::set(float _x, float _y)
{
    x = _x;
    y = _y;
}


vector2 operator*(float scale, const vector2& v); // 0x00563820 (.h:264)
float vec2_find_closest_dist_squared_to_linesegment(vector2& closest_point, const vector2& p, const vector2& v0, const vector2& v1); // 0x0057FB70 (vlib_vector2.cpp:163)
float vec2_find_closest_dist_squared_to_linesegment(float& closest_param_pos, const vector2& p, const vector2& v0, const vector2& v1); // 0x0057FD30 (vlib_vector2.cpp:194)
float vec2_dist(const vector2& v0, const vector2& v1); // 0x0057FEC0 (vlib_vector2.cpp:216)
float vec2_dist_squared(const vector2& v0, const vector2& v1); // 0x0057FF40 (vlib_vector2.cpp:223)
float vec2_get_closest_dist_sqr_point_to_line(const vector2& p, const vector2& v0, const vector2& v1); // 0x00580030 (vlib_vector2.cpp:262)
extern const vector2 ZERO_VECTOR2; // 0x01CE9910

bool vec2_find_moving_circle_point_intersect(float& s, const vector2& v0, const vector2& v1, const vector2& p, float r); // 0x00580140 (vlib_vector2.cpp:288)
float vec2_which_side_of_2d_line(const vector2& pt, const vector2& line_v1, const vector2& line_v2); // 0x005802F0 (vlib_vector2.cpp:354)
float vec2_which_side_of_2d_line(float pt_x, float pt_y, float line_v1_x, float line_v1_y, float line_v2_x, float line_v2_y); // 0x00580340 (vlib_vector2.cpp:358)
void vec2_find_closest_point_on_line(vector2& out, const vector2& pt, const vector2& line_v1, const vector2& line_v2); // 0x00580380 (vlib_vector2.cpp:374)
void vec2_find_closest_point_on_linesegment(vector2& out, const vector2& pt, const vector2& line_v1, const vector2& line_v2); // 0x005804C0 (vlib_vector2.cpp:401)

// 0x005637D0 (.h:213)
// x87 code: the sum of squares is not rounded to float before the square root.
inline float vector2::mag() const
{
    return (float)sqrt((double)x * x + (double)y * y);
}
