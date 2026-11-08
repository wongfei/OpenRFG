// Original: volition/rfg/code/vehicle_ai/vehicle_lane_common.cpp

#include "volition/rfg/code/vehicle_ai/vehicle_lane_common.h"

#include <math.h>

#include "volition/rfg/code/traffic/traffic.h"
#include "volition/rfg/code/traffic/traffic_shared.h"
#include "volition/vlib/code/math/spline.h"

// A lane spline's control points as a cubic Bezier.
static cubic_bezier_spline lane_bezier(const lane_spline_base* ls)
{
    return cubic_bezier_spline(ls->num_knots, ls->knots.to_pointer(), ls->in_handles.to_pointer(), ls->out_handles.to_pointer());
}

// 0x00BCE430 (:19)
float spline_common_get_dist_remaining_on_lane_spline(const lane_spline_base* ls, const vector& pos)
{
    const cubic_bezier_spline cbs = lane_bezier(ls);
    const float u = cbs.get_closest_point_approx(pos);
    return cbs.get_length_approx(10, u, (float)(ls->num_knots - 1));
}

// 0x00BCE4A0 (:31)
float spline_common_get_dist_remaining_on_lane_spline(const lane_spline_base* ls, float u)
{
    return lane_bezier(ls).get_length_approx(10, u, (float)(ls->num_knots - 1));
}

// 0x00BCE500 (:40)
float spline_common_get_closest_point_on_spline(const lane_spline_base* ls, const vector& in_pos, vector& spline_pos)
{
    const cubic_bezier_spline cbs = lane_bezier(ls);
    const float u = cbs.get_closest_point_approx(in_pos);
    cbs.get_point(spline_pos, u);
    return u;
}

// 0x00BCE570 (:54)
float spline_common_get_dist_to_point(const lane_spline_base* ls, float u)
{
    return lane_bezier(ls).get_length_approx(10, 0.0f, u);
}

// 0x00BCE5D0 (:62)
float spline_common_get_closest_point_and_tangent_on_spline(const lane_spline_base* ls, const vector& in_pos, vector& spline_pos, vector& tangent)
{
    const cubic_bezier_spline cbs = lane_bezier(ls);
    const float u = cbs.get_closest_point_approx(in_pos);
    cbs.get_point(spline_pos, u);
    cbs.get_tangent(tangent, u);
    return u;
}

// 0x00BCE650 (:79)
float spline_common_get_spline_parametric_pos_closest_to_point(const lane_spline_base* ls, const vector& in_pos)
{
    return lane_bezier(ls).get_closest_point_approx(in_pos);
}

// 0x00BCE690 (:88)
void spline_common_get_position_on_spline(const lane_spline_base* ls, float u, vector& pos)
{
    lane_bezier(ls).get_point(pos, u);
}

// 0x00BCE6E0 (:96)
void spline_common_get_tangent_on_spline(const lane_spline_base* ls, float u, vector& tan)
{
    lane_bezier(ls).get_tangent(tan, u);
}

// 0x00BCE730 (:104)
void spline_common_get_position_and_tangent_on_spline(const lane_spline_base* ls, float u, vector& pos, vector& tan)
{
    const cubic_bezier_spline cbs = lane_bezier(ls);
    cbs.get_point(pos, u);
    cbs.get_tangent(tan, u);
}

// 0x00BCE7A0 (:113)
void spline_common_get_second_derivative_on_spline(const lane_spline_base* ls, float u, vector& sec_dir)
{
    lane_bezier(ls).get_second_derivative(sec_dir, u);
}

// 0x00BC56D0 (:128): up to 5 Newton steps of the arc length, inside a shrinking bracket.
float spline_common_get_parametric_pos_for_dist(const cubic_bezier_spline& cbs, float desired_dist, const float* spline_length)
{
    float hi = (float)(cbs.num_knots - 1);
    if (desired_dist <= 0.0f)
    {
        return 0.0f;
    }
    const float length = spline_length == NULL ? cbs.get_length_approx(5, 0.0f, hi) : *spline_length;
    if (desired_dist >= length)
    {
        return hi;
    }
    float fraction = desired_dist / length;
    if (fraction >= 1.0f)
    {
        fraction = 1.0f;
    }
    float lo = 0.0f;
    vector tangent;
    cbs.get_tangent(tangent, (float)(cbs.num_knots - 1) * fraction);
    float u = desired_dist / sqrtf(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
    if (u >= hi)
    {
        u = hi;
    }
    if (u < 0.0f)
    {
        u = 0.0f;
    }
    for (int i = 0; i < 5; i++)
    {
        const float dist = cbs.get_length_approx(5, 0.0f, u);
        cbs.get_tangent(tangent, u);
        if (dist <= desired_dist)
        {
            lo = u;
        }
        else
        {
            hi = u;
        }
        if (fabsf(desired_dist - dist) < 0.1f)
        {
            return u;
        }
        u += (desired_dist - dist) / sqrtf(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
        if (u >= hi)
        {
            u = hi;
        }
        if (u < lo)
        {
            u = lo;
        }
    }
    return u;
}

// 0x00BCE7F0 (:189)
float spline_common_get_point_fixed_distance_on_lane_spline(const lane_spline_base* lsb, float desired_dist, vector& target_pos)
{
    const cubic_bezier_spline cbs = lane_bezier(lsb);
    const lane_spline* city = get_city_spline_from_base_spline(lsb);
    const float u = spline_common_get_parametric_pos_for_dist(cbs, desired_dist, city != NULL ? city->arc_length : NULL);
    cbs.get_point(target_pos, u);
    return u;
}

// 0x00BCE880 (:204): the curvature in the xz plane, |t x s| / |t|^3 with the y of both derivatives zeroed.
float spline_common_get_curvature_on_lane_spline(const lane_spline_base* ls, float u)
{
    const cubic_bezier_spline cbs = lane_bezier(ls);
    vector tan, sec;
    cbs.get_tangent(tan, u);
    cbs.get_second_derivative(sec, u);
    const float cross_x = sec.z * 0.0f - tan.z * 0.0f;
    const float cross_z = tan.x * 0.0f - sec.x * 0.0f;
    const float cross_y = tan.z * sec.x - sec.z * tan.x;
    const float mag = sqrtf(tan.x * tan.x + 0.0f + tan.z * tan.z);
    return sqrtf(cross_x * cross_x + cross_y * cross_y + cross_z * cross_z) / (mag * mag * mag);
}
