// Original: volition/vlib/code/math/spline.cpp
//
// Partial: the cardinal_spline default constructor (held by the camera's mini script parameters, M6), and its points,
// tension and get_point (the cloud spline projectiles, multiplayer.md MP8), its constructor from points (the backpack
// arc, VFX.md FX6), the catmull_rom_spline constructor and
// get_point (the arc welder's beam, weapons.md WP2), cubic_bezier_spline::get_point/get_tangent (stub_impl.md S3), its
// constructor, set_points and get_length_approx (the lookaround camera, S5). The internal
// functions and file-static globals are declared from rfg.pdb (no public symbol); the external interface is declared in
// spline.h, the rest is pending.

#include "volition/vlib/code/math/spline.h"
#include "volition/vlib/code/math/vector2.h"

#include <math.h>

// ---- internal functions
// LTCG: arguments in ECX,EDX (not mapped yet).
static void spline_interpolate_cubic_3d(vector& out, const vector* pts, int point_index, float c0, float c1, float c2, float c3); // 0x0057A520 (:53)
// LTCG: arguments in EAX,ECX (not mapped yet).
static void spline_interpolate_cubic_2d(vector2& out, const vector2* pts, int point_index, float c0, float c1, float c2, float c3); // 0x0057A640 (:65)
// LTCG: arguments in EAX (not mapped yet).
static float cubic_bez_get_blend(int k, float u); // 0x0057A710 (:87)
// LTCG: arguments in EDX (not mapped yet).
static int bez_fact(int n); // 0x0057A7F0 (:110)

// ---- cardinal_spline

// 0x0057A810 (:142)
// No points yet; the default tension is 1.
cardinal_spline::cardinal_spline()
{
    tension = 1.0f;
}

// 0x005A5DF0 (:148): the points are referenced, not copied (VFX.md FX6: the backpack arc effect's path)
cardinal_spline::cardinal_spline(int _num_pts, const vector* _pts, float _tension)
{
    num_pts = _num_pts;
    pts = _pts;
    tension = _tension;
}

// 0x0057A820 (:155): the points are referenced, not copied
void cardinal_spline::set_points(int _num_pts, const vector* _pts)
{
    num_pts = _num_pts;
    pts = _pts;
}

// 0x0057A830 (:165)
void cardinal_spline::set_tension(float _t)
{
    tension = _t;
}

// 0x0057A840 (:173)
// The curve through points 1 to num_pts - 2 at u (point 1 at u <= 1, the last at u >= num_pts - 2); fewer than 4
// points give zero. The cubic interpolation is inlined (spline_interpolate_cubic_3d).
void cardinal_spline::get_point(vector& out, float u) const
{
    if (num_pts < 4)
    {
        out = ZERO_VECTOR;
        return;
    }
    if (u < 1.0f)
    {
        out = pts[1];
        return;
    }
    const int last = num_pts - 2;
    if ((float)last <= u)
    {
        out = pts[last];
        return;
    }
    const int i = (int)u;
    const float t = u - (float)i;
    const float t2 = t * t;
    const float t3 = t2 * t;
    const float s = (1.0f - tension) * 0.5f;
    const float c0 = (-s * t3 + t2 * s * 2.0f) - s * t;
    const float c1 = (2.0f - s) * t3 + (s - 3.0f) * t2 + 1.0f;
    const float c2 = (s - 2.0f) * t3 + (3.0f - s * 2.0f) * t2 + s * t;
    const float c3 = s * t3 - s * t2;
    const vector* p = pts + i;
    out.x = p[-1].x * c0 + p[0].x * c1 + p[1].x * c2 + p[2].x * c3;
    out.y = p[0].y * c1 + p[-1].y * c0 + p[1].y * c2 + p[2].y * c3;
    out.z = p[0].z * c1 + p[-1].z * c0 + p[1].z * c2 + p[2].z * c3;
}

// ---- catmull_rom_spline (weapons.md WP2: the arc welder's beam)

// 0x0057AB00 (:217)
void cardinal_spline::get_tangent(vector& out, float u) const
{
    if (num_pts < 4)
    {
        out = ZERO_VECTOR;
        return;
    }
    const float s = (1.0f - tension) * 0.5f;
    if (u < 1.0f)
    {
        out.x = (pts[2].x - pts[0].x) * s;
        out.y = (pts[2].y - pts[0].y) * s;
        out.z = (pts[2].z - pts[0].z) * s;
        return;
    }
    if ((float)(num_pts - 2) <= u)
    {
        const vector& a = pts[num_pts - 3];
        const vector& b = pts[num_pts - 1];
        out.x = (b.x - a.x) * s;
        out.y = (b.y - a.y) * s;
        out.z = (b.z - a.z) * s;
        return;
    }
    const int i = (int)floorf(u);
    const float t = u - (float)i;
    const float t2 = t * t;
    const float c0 = (s * -3.0f * t2 + s * 4.0f * t) - s;
    const float c1 = (2.0f - s) * 3.0f * t2 + (s - 3.0f) * 2.0f * t;
    const float c2 = (s - 2.0f) * 3.0f * t2 + (3.0f - s * 2.0f) * 2.0f * t + s;
    const float c3 = s * 3.0f * t2 - t * s * 2.0f;
    const vector* p = pts + i;
    out.x = p[-1].x * c0 + c1 * p[0].x + p[1].x * c2 + p[2].x * c3;
    out.y = p[0].y * c1 + p[-1].y * c0 + p[1].y * c2 + p[2].y * c3;
    out.z = p[0].z * c1 + p[-1].z * c0 + p[1].z * c2 + p[2].z * c3;
}

// 0x005A6B60 (:580): the points are referenced, not copied
catmull_rom_spline::catmull_rom_spline(int _num_pts, const vector* _pts)
{
    num_pts = _num_pts;
    pts = _pts;
}

// 0x0057B600 (:575): leaves the points unset (VFX.md FX4: the tracers' path spline)
catmull_rom_spline::catmull_rom_spline()
{
}

// 0x0057B610 (:587): the points are referenced, not copied
void catmull_rom_spline::set_points(int _num_pts, const vector* _pts)
{
    num_pts = _num_pts;
    pts = _pts;
}

// 0x0057B620 (:599)
// The curve through points 1 to num_pts - 2 at u (point 1 at u < 1, the last at u >= num_pts - 2); fewer than 4 points
// give zero. The blend weights and the sums are computed in double and rounded to float once each, as in rfg.exe.
void catmull_rom_spline::get_point(vector& out, float u) const
{
    if (num_pts < 4)
    {
        out = ZERO_VECTOR;
        return;
    }
    const int i = (int)(float)floor((double)u);
    if (u < 1.0f)
    {
        out = pts[1];
        return;
    }
    const int last = num_pts - 2;
    if ((float)last <= u)
    {
        out = pts[last];
        return;
    }
    const float t = (float)((double)u - (double)(float)i);
    const float t2 = (float)((double)t * t);
    const float t3 = (float)((double)t2 * t);
    const float c0 = (float)(((double)t3 * -0.5 + t2) - (double)t * 0.5);
    const float c1 = (float)(((double)t3 * 1.5 + (double)t2 * -2.5) + 1.0);
    const float c2 = (float)(((double)t3 * -1.5 + (double)t2 * 2.0) + (double)t * 0.5);
    const float c3 = (float)((double)t3 * 0.5 - (double)t2 * 0.5);
    const vector* p = pts + i;
    out.x = (float)((double)p[-1].x * c0 + (double)p[0].x * c1 + (double)p[1].x * c2 + (double)p[2].x * c3);
    out.y = (float)((double)p[0].y * c1 + (double)p[-1].y * c0 + (double)p[1].y * c2 + (double)p[2].y * c3);
    out.z = (float)((double)p[0].z * c1 + (double)p[-1].z * c0 + (double)p[1].z * c2 + (double)p[2].z * c3);
}

// 0x0057B900 (:641)
void catmull_rom_spline::get_tangent(vector& out, float u) const
{
    if (num_pts < 4)
    {
        out = ZERO_VECTOR;
        return;
    }
    const int i = (int)(float)floor((double)u);
    if (u < 1.0f)
    {
        out.x = (pts[2].x - pts[0].x) * 0.5f;
        out.y = (pts[2].y - pts[0].y) * 0.5f;
        out.z = (pts[2].z - pts[0].z) * 0.5f;
        return;
    }
    if ((float)(num_pts - 2) <= u)
    {
        const vector& a = pts[num_pts - 3];
        const vector& b = pts[num_pts - 1];
        out.x = (b.x - a.x) * 0.5f;
        out.y = (b.y - a.y) * 0.5f;
        out.z = (b.z - a.z) * 0.5f;
        return;
    }
    const float t = u - (float)i;
    const float t2 = t * t;
    const float c0 = (t2 * -1.5f + t * 2.0f) - 0.5f;
    const float c1 = t2 * 4.5f + t * -5.0f;
    const float c2 = t2 * -4.5f + t * 4.0f + 0.5f;
    const float c3 = t2 * 1.5f - t;
    const vector* p = pts + i;
    out.x = p[-1].x * c0 + p[0].x * c1 + p[1].x * c2 + p[2].x * c3;
    out.y = p[0].y * c1 + p[-1].y * c0 + p[1].y * c2 + p[2].y * c3;
    out.z = p[0].z * c1 + p[-1].z * c0 + p[1].z * c2 + p[2].z * c3;
}

// ---- cubic_bezier_spline

// 0x0057C980 (:1060)
// 0x0057BD00 (:687): leaves the points unset (turret::turret)
catmull_rom_spline_2d::catmull_rom_spline_2d()
{
}

// 0x0057BD10 (:698): the points are referenced, not copied
void catmull_rom_spline_2d::set_points(int _num_pts, const vector2* _pts)
{
    num_pts = _num_pts;
    pts = _pts;
}

// 0x0057BD20 (:710): u counts along the points; before point 1 and past point num_pts - 2 it stays at those ends, as the
// curve needs a neighbour on each side.
void catmull_rom_spline_2d::get_point(vector2& out, float u) const
{
    if (num_pts < 4)
    {
        out = ZERO_VECTOR2;
        return;
    }
    const int i = (int)floor((double)u);
    if (u < 1.0f)
    {
        out = pts[1];
        return;
    }
    const int last = num_pts - 2;
    if ((float)last <= u)
    {
        out = pts[last];
        return;
    }
    const float t = u - (float)i;
    const float t2 = t * t;
    const float t3 = t2 * t;
    const float b1 = t3 * 1.5f + t2 * -2.5f + 1.0f;
    const float b2 = t3 * -1.5f + t2 * 2.0f + t * 0.5f;
    const float b3 = t3 * 0.5f - t2 * 0.5f;
    const float b0 = (t3 * -0.5f + t2) - t * 0.5f;
    out.x = pts[i - 1].x * b0 + pts[i].x * b1 + pts[i + 1].x * b2 + pts[i + 2].x * b3;
    out.y = pts[i].y * b1 + pts[i - 1].y * b0 + pts[i + 1].y * b2 + pts[i + 2].y * b3;
}

// 0x0057C950 (:1033)
cubic_bezier_spline::cubic_bezier_spline()
{
    num_knots = 0;
    knots = nullptr;
    incoming_handles = nullptr;
    outgoing_handles = nullptr;
}

// 0x0057C960 (:1048): the arrays are referenced, not copied (the lookaround camera's swoop, stub_impl.md S5)
void cubic_bezier_spline::set_points(int _num_pts, const vector* _pts, const vector* _in_pts, const vector* _out_pts)
{
    num_knots = _num_pts;
    knots = _pts;
    incoming_handles = _in_pts;
    outgoing_handles = _out_pts;
}

// Knot i to knot i + 1 through outgoing_handles[i] and incoming_handles[i + 1] at the fraction of u; u clamps to the
// first and last knot, fewer than 2 knots give zero. The Bernstein weights are evaluated in double (the source's 3.0
// literals) and rounded; the weighted sum is in float.
void cubic_bezier_spline::get_point(vector& out, float u) const
{
    if (num_knots < 2)
    {
        out = ZERO_VECTOR;
        return;
    }
    if (u <= 0.0f)
    {
        out = knots[0];
        return;
    }
    if (u >= (float)(num_knots - 1))
    {
        out = knots[num_knots - 1];
        return;
    }
    const int i = (int)u;
    const float t = u - (float)i;
    const float t2 = t * t;
    const float t3 = t2 * t;
    const float b0 = (float)((((double)t2 - (double)t) * 3.0 - (double)t3) + 1.0); // (1 - t)^3
    const float b1 = (float)(((((double)t3 - (double)t2) - (double)t2) + (double)t) * 3.0); // 3t (1 - t)^2
    const float b2 = (float)(((double)t2 - (double)t3) * 3.0); // 3t^2 (1 - t)
    const vector& k0 = knots[i];
    const vector& k1 = knots[i + 1];
    const vector& h0 = outgoing_handles[i];
    const vector& h1 = incoming_handles[i + 1];
    out.x = ((k0.x * b0 + k1.x * t3) + h0.x * b1) + h1.x * b2;
    out.y = ((k0.y * b0 + k1.y * t3) + h0.y * b1) + h1.y * b2;
    out.z = ((k0.z * b0 + k1.z * t3) + h0.z * b1) + h1.z * b2;
}

// 0x0057CCE0 (:1126)
// The derivative of get_point at u (not normalized); at or before the first knot 3 (outgoing_handles[0] - knots[0]),
// at or after the last 3 (knots[n - 1] - incoming_handles[n - 1]).
void cubic_bezier_spline::get_tangent(vector& out, float u) const
{
    if (num_knots < 2)
    {
        out = ZERO_VECTOR;
        return;
    }
    if (u <= 0.0f)
    {
        out.x = (outgoing_handles[0].x - knots[0].x) * 3.0f;
        out.y = (outgoing_handles[0].y - knots[0].y) * 3.0f;
        out.z = (outgoing_handles[0].z - knots[0].z) * 3.0f;
        return;
    }
    if (u >= (float)(num_knots - 1))
    {
        const vector& k = knots[num_knots - 1];
        const vector& h = incoming_handles[num_knots - 1];
        out.x = (k.x - h.x) * 3.0f;
        out.y = (k.y - h.y) * 3.0f;
        out.z = (k.z - h.z) * 3.0f;
        return;
    }
    const int i = (int)u;
    const float t = u - (float)i;
    const float t2 = t * t;
    // the derivatives of the Bernstein weights, in double and rounded
    const float d_in = (float)((double)t2 * -9.0 + (double)t * 6.0);
    const float d_k0 = (float)(((double)t2 * -3.0 + (double)t * 6.0) - 3.0);
    const float d_out = (float)(((double)t2 * 9.0 + (double)t * -12.0) + 3.0);
    const float d_k1 = (float)(((double)t2 + (double)t2) + (double)t2);
    const vector& k0 = knots[i];
    const vector& k1 = knots[i + 1];
    const vector& h0 = outgoing_handles[i];
    const vector& h1 = incoming_handles[i + 1];
    out.x = ((k0.x * d_k0 + k1.x * d_k1) + h0.x * d_out) + h1.x * d_in;
    out.y = ((k0.y * d_k0 + k1.y * d_k1) + h0.y * d_out) + h1.y * d_in;
    out.z = ((k0.z * d_k0 + k1.z * d_k1) + h0.z * d_out) + h1.z * d_in;
}

// 0x005A6D20 (:1042)
cubic_bezier_spline::cubic_bezier_spline(int _num_pts, const vector* _pts, const vector* _in_pts, const vector* _out_pts)
    : knots(_pts), incoming_handles(_in_pts), outgoing_handles(_out_pts), num_knots(_num_pts)
{
}

// 0x0057D100 (:1168)
void cubic_bezier_spline::get_second_derivative(vector& out, float u) const
{
    if (num_knots < 2)
    {
        out = ZERO_VECTOR;
        return;
    }
    if (u < 0.0f)
    {
        const vector& k0 = knots[0];
        const vector& h0 = outgoing_handles[0];
        const vector& h1 = incoming_handles[1];
        out.x = (h1.x + (k0.x - h0.x * 2.0f)) * 6.0f;
        out.y = (h1.y + (k0.y - h0.y * 2.0f)) * 6.0f;
        out.z = (h1.z + (k0.z - h0.z * 2.0f)) * 6.0f;
        return;
    }
    if ((float)(num_knots - 1) <= u)
    {
        const vector& k1 = knots[num_knots - 1];
        const vector& h0 = outgoing_handles[num_knots - 2];
        const vector& h1 = incoming_handles[num_knots - 1];
        out.x = (k1.x + (h0.x - h1.x * 2.0f)) * 6.0f;
        out.y = (k1.y + (h0.y - h1.y * 2.0f)) * 6.0f;
        out.z = (k1.z + (h0.z - h1.z * 2.0f)) * 6.0f;
        return;
    }
    const int index = (int)u;
    const float t = u - (float)index;
    const float d_k0 = t * -6.0f + 6.0f;
    const float d_out = t * 18.0f - 12.0f;
    const float d_in = t * -18.0f + 6.0f;
    const float d_k1 = t * 6.0f;
    const vector& k0 = knots[index];
    const vector& k1 = knots[index + 1];
    const vector& h0 = outgoing_handles[index];
    const vector& h1 = incoming_handles[index + 1];
    out.x = k0.x * d_k0 + h0.x * d_out + h1.x * d_in + k1.x * d_k1;
    out.y = k0.y * d_k0 + h0.y * d_out + h1.y * d_in + k1.y * d_k1;
    out.z = k0.z * d_k0 + h0.z * d_out + h1.z * d_in + k1.z * d_k1;
}

// The closest parameter on a chord, clamped: 0.5 for a chord shorter than about 1e-5.
static float closest_param_on_chord(const vector& start, const vector& end, const vector& p)
{
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float dz = end.z - start.z;
    const float len_sq = dx * dx + dy * dy + dz * dz;
    if (len_sq < 1e-10f)
    {
        return 0.5f;
    }
    float t = (dy * (p.y - start.y) + dx * (p.x - start.x) + dz * (p.z - start.z)) / len_sq;
    if (t < 0.0f)
    {
        t = 0.0f;
    }
    if (t >= 1.0f)
    {
        t = 1.0f;
    }
    return t;
}

// 0x005A6D40 (:1214): the closest chord between knots, then 5 refinements on the curve.
float cubic_bezier_spline::get_closest_point_approx(const vector& p) const
{
    const int num_segments = num_knots - 1;
    int best_segment = 0;
    float best_dist = 3.4028235e+38f;
    float best_param = 0.0f;
    for (int i = 0; i < num_segments; i++)
    {
        const vector& k0 = knots[i];
        const vector& k1 = knots[i + 1];
        const float t = closest_param_on_chord(k0, k1, p);
        const float dx = (k0.x + (k1.x - k0.x) * t) - p.x;
        const float dy = (k0.y + (k1.y - k0.y) * t) - p.y;
        const float dz = (k0.z + (k1.z - k0.z) * t) - p.z;
        const float dist = dx * dx + dy * dy + dz * dz;
        if (dist < best_dist)
        {
            best_segment = i;
            best_dist = dist;
            best_param = t;
        }
    }
    float u = (float)best_segment + best_param;
    const float max_u = (float)num_segments;
    float step = 0.25f;
    for (int i = 0; i < 5; i++)
    {
        float hi = step + u;
        if (hi < 0.0f)
        {
            hi = 0.0f;
        }
        if (hi >= max_u)
        {
            hi = max_u;
        }
        float lo = u - step;
        if (lo < 0.0f)
        {
            lo = 0.0f;
        }
        if (lo >= max_u)
        {
            lo = max_u;
        }
        vector hi_point, lo_point;
        get_point(hi_point, hi);
        get_point(lo_point, lo);
        u = (hi - lo) * closest_param_on_chord(lo_point, hi_point, p) + lo;
        step *= 0.5f;
    }
    return u;
}

// 0x0057D660 (:1332)
// The length from start_pos to end_pos as num_steps chords (the last step clamps to end_pos).
float cubic_bezier_spline::get_length_approx(int num_steps, float start_pos, float end_pos) const
{
    const float step = (float)(((double)end_pos - (double)start_pos) / (double)(float)num_steps);
    float len = 0.0f;
    float u = start_pos;
    vector last;
    get_point(last, u);
    for (int i = num_steps; i > 0; i--)
    {
        u = (float)((double)u + (double)step);
        if ((float)((double)u - (double)end_pos) >= 0.0f)
        {
            u = end_pos;
        }
        vector p;
        get_point(p, u);
        const float dx = (float)((double)p.x - (double)last.x);
        const float dy = (float)((double)p.y - (double)last.y);
        const float dz = (float)((double)p.z - (double)last.z);
        const float sq = (float)((double)dx * (double)dx + (double)dy * (double)dy + (double)dz * (double)dz);
        len = (float)((double)(float)sqrt((double)sq) + (double)len);
        last = p;
    }
    return len;
}

// 0x005A64E0 (:365)
cardinal_spline_2d::cardinal_spline_2d(int _num_pts, const vector2* _pts, float _tension)
{
    num_pts = _num_pts;
    pts = _pts;
    tension = _tension;
}

// 0x0057AFE0 (:386)
void cardinal_spline_2d::get_point(vector2& out, float u) const
{
    if (num_pts < 4)
    {
        out = ZERO_VECTOR2;
        return;
    }
    const int index = (int)floor((double)u);
    if (u < 1.0f)
    {
        out = pts[1];
        return;
    }
    const int last = num_pts - 2;
    if ((float)last <= u)
    {
        out = pts[last];
        return;
    }
    const float t = u - (float)index;
    const float t2 = t * t;
    const float t3 = t2 * t;
    const float s = (1.0f - tension) * 0.5f;
    const float b0 = (-s * t3 + t2 * s * 2.0f) - s * t;
    const float b1 = (2.0f - s) * t3 + (s - 3.0f) * t2 + 1.0f;
    const float b2 = (s - 2.0f) * t3 + (3.0f - s * 2.0f) * t2 + s * t;
    const float b3 = s * t3 - s * t2;
    out.x = pts[index - 1].x * b0 + pts[index].x * b1 + pts[index + 1].x * b2 + pts[index + 2].x * b3;
    out.y = pts[index].y * b1 + pts[index - 1].y * b0 + pts[index + 1].y * b2 + pts[index + 2].y * b3;
}

// 0x0057D7F0 (:1376) de Casteljau. The splines' pointers are const, but a split writes through them to its caller's arrays.
void cubic_bezier_spline::split_at_parametric_pos(float t, cubic_bezier_spline* spline1, cubic_bezier_spline* spline2) const
{
    if (num_knots != 2 || knots == nullptr || incoming_handles == nullptr || outgoing_handles == nullptr) {
        return;
    }
    const vector& p0 = knots[0];
    const vector& p1 = outgoing_handles[0];
    const vector& p2 = incoming_handles[1];
    const vector& p3 = knots[1];
    float s = (float)(1.0 - (double)t);
    vector a(p0.x * s + t * p1.x, p1.y * t + p0.y * s, p0.z * s + p1.z * t);
    vector b(s * p1.x + p2.x * t, p1.y * s + p2.y * t, p1.z * s + p2.z * t);
    vector c(p2.x * s + p3.x * t, p2.y * s + p3.y * t, p2.z * s + p3.z * t);
    vector d(a.x * s + b.x * t, a.y * s + b.y * t, a.z * s + b.z * t);
    vector e(b.x * s + c.x * t, b.y * s + c.y * t, b.z * s + c.z * t);
    vector mid(d.x * s + e.x * t, d.y * s + e.y * t, d.z * s + e.z * t);
    if (spline1 != nullptr) {
        const_cast<vector*>(spline1->knots)[0] = p0;
        const_cast<vector*>(spline1->knots)[1] = mid;
        const_cast<vector*>(spline1->outgoing_handles)[0] = a;
        const_cast<vector*>(spline1->incoming_handles)[1] = d;
    }
    if (spline2 != nullptr) {
        const_cast<vector*>(spline2->knots)[0] = mid;
        const_cast<vector*>(spline2->knots)[1] = knots[1];
        const_cast<vector*>(spline2->outgoing_handles)[0] = e;
        const_cast<vector*>(spline2->incoming_handles)[1] = c;
    }
}
