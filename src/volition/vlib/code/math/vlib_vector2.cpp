// Original: volition/vlib/code/math/vlib_vector2.cpp
//
// Partial: vec2_dist_squared (human_collide's debris crossing test). The other 2D vector helpers are not implemented
// yet.

#include "volition/vlib/code/math/vector2.h"

#include <math.h>

// 0x0057FF40 (:223)
// The differences rounded to float, squared and summed in double (x87), rounded once.
float vec2_dist_squared(const vector2& v0, const vector2& v1)
{
    const double dx = (double)(float)((double)v1.x - (double)v0.x);
    const double dy = (double)(float)((double)v1.y - (double)v0.y);
    return (float)(dy * dy + dx * dx);
}

// 0x0057FA60 (:114)
void vector2::normalize()
{
    const float length = sqrtf(x * x + y * y);
    x = x / length;
    y = y / length;
}

// 0x0057FEC0 (:216)
float vec2_dist(const vector2& v0, const vector2& v1)
{
    const float dx = v1.x - v0.x;
    const float dy = v1.y - v0.y;
    return (float)sqrt((double)(dy * dy + dx * dx));
}

// 0x0057F9D0 (:93)
float vector2::mag_and_normalize_safe()
{
    float saved_x = x;
    float saved_y = y;
    float length = sqrtf(saved_x * saved_x + saved_y * saved_y);
    if (length == 0.0f) {
        x = 1.0f;
        y = 0.0f;
        return length;
    }
    x = saved_x / length;
    y = saved_y / length;
    return length;
}

// 0x0057FB70 (:163)
float vec2_find_closest_dist_squared_to_linesegment(vector2& closest_point, const vector2& p, const vector2& v0, const vector2& v1)
{
    float dx = v1.x - v0.x;
    float dy = v1.y - v0.y;
    float t = (dy * (p.y - v0.y) + dx * (p.x - v0.x)) / (dx * dx + dy * dy);
    if (t - 0.0f < 0.0f) {
        t = 0.0f;
    }
    if (0.0f <= t - 1.0f) {
        t = 1.0f;
    }
    closest_point.x = v0.x + t * dx;
    closest_point.y = t * dy + v0.y;
    float to_x = closest_point.x - p.x;
    float to_y = closest_point.y - p.y;
    return to_x * to_x + to_y * to_y;
}

// 0x0057FD30 (:194)
float vec2_find_closest_dist_squared_to_linesegment(float& closest_param_pos, const vector2& p, const vector2& v0, const vector2& v1)
{
    float dx = v1.x - v0.x;
    float dy = v1.y - v0.y;
    float to_x = p.x - v0.x;
    float to_y = p.y - v0.y;
    float t = (dy * to_y + dx * to_x) / (dx * dx + dy * dy);
    if (t - 0.0f < 0.0f) {
        t = 0.0f;
    }
    if (0.0f <= t - 1.0f) {
        t = 1.0f;
    }
    closest_param_pos = t;
    to_x = t * dx - to_x;
    to_y = t * dy - to_y;
    return to_x * to_x + to_y * to_y;
}

// 0x005802F0 (:354)
float vec2_which_side_of_2d_line(const vector2& pt, const vector2& line_v1, const vector2& line_v2)
{
    return (pt.y - line_v1.y) * (line_v2.x - line_v1.x) - (line_v2.y - line_v1.y) * (pt.x - line_v1.x);
}

// 0x00580340 (:358)
float vec2_which_side_of_2d_line(float pt_x, float pt_y, float line_v1_x, float line_v1_y, float line_v2_x, float line_v2_y)
{
    return (pt_y - line_v1_y) * (line_v2_x - line_v1_x) - (pt_x - line_v1_x) * (line_v2_y - line_v1_y);
}

// 0x005804C0 (:401)
void vec2_find_closest_point_on_linesegment(vector2& out, const vector2& pt, const vector2& line_v1, const vector2& line_v2)
{
    if (line_v1.x == line_v2.x && line_v1.y == line_v2.y) {
        out.x = line_v1.x;
        out.y = line_v1.y;
        return;
    }
    float dx = line_v2.x - line_v1.x;
    float dy = line_v2.y - line_v1.y;
    float t = ((pt.y - line_v1.y) * dy + (pt.x - line_v1.x) * dx) / (dy * dy + dx * dx);
    if (t - 0.0f < 0.0f) {
        t = 0.0f;
    }
    if (0.0f <= t - 1.0f) {
        t = 1.0f;
    }
    out.x = t * dx + line_v1.x;
    out.y = dy * t + line_v1.y;
}
