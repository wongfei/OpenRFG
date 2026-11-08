// Original: volition/vlib/code/math/floating.cpp
//
// Partial: fl_uround, fl_atan2_safe, fl_parameter, fl_calc_scaled_epsilon, the robust greater-than comparisons, fl_is_finite
// and fl_angle_lerp_radians (stub_impl.md S3); fl_equal_robust (S6); fl_less_than_robust (stub_misc.md group 5).
// The rest of the float helpers is not implemented yet.

#include "volition/vlib/code/math/floating.h"

#include <math.h>
#include <string.h>

// 0x0056BB50 (:347)
// To a multiple of prec, halves away from zero. The arithmetic is double in rfg.exe, with an x87 product.
float fl_round(float n, float prec)
{
    const double half = (double)prec * 0.5;
    const int multiple = (int)(0.0f <= n ? (half + (double)n) / (double)prec : ((double)n - half) / (double)prec);
    return (float)((double)multiple * (double)prec);
}

// 0x0056BBD0 (:364)
// Halves away from zero.
int fl_round(float n)
{
    if (0.0f <= n)
    {
        return (int)(n + 0.5f);
    }
    return (int)(n - 0.5f);
}

// 0x0056BC00 (:379)
// f + 0.5 (in double), truncated.
unsigned int fl_uround(float f)
{
    return (unsigned int)(long long)((double)f + 0.5);
}

// 0x0056C920 (:1160)
// The CRT's double acos (x87 __CIacos), rounded to float once; acosf can differ in the last bit.
float fl_acos(float fl)
{
    return (float)acos((double)fl);
}

// 0x0056BC30 (:394)
// atan2 that returns 0 for the origin.
float fl_atan2_safe(float y, float x)
{
    if (y == 0.0f && x == 0.0f)
    {
        return 0.0f;
    }
    return (float)atan2((double)y, (double)x); // x87 __CIatan2 on doubles, rounded once
}

// 0x004B1A70 (.h:480)
// epsilon scaled by the larger magnitude of a and b (at least 1).
float fl_calc_scaled_epsilon(float a, float b, float epsilon)
{
    float scale = fabsf(a);
    if (fabsf(a) - fabsf(b) < 0.0f)
    {
        scale = fabsf(b);
    }
    if (0.0f <= 1.0f - scale)
    {
        scale = 1.0f;
    }
    return scale * epsilon;
}

// 0x004B1BC0 (.h:520)
// a below b by more than the scaled epsilon. The subtraction and the comparison are in double.
bool fl_less_than_robust(float a, float b, float epsilon)
{
    return (double)b - (double)fl_calc_scaled_epsilon(a, b, epsilon) > (double)a;
}

// 0x0056BDC0 (:482)
// An ease in and out over 0..1: 2^(n-1) x^n below one half, 1 + 2^(n-1) (x-1)^n (negated for even n) above. The
// powers multiply in float (each product rounded); n 2 is a double expression rounded once.
float fl_parametric_accel_decel(float x, int n)
{
    if (n == 2)
    {
        if (x < 0.5f)
        {
            return (float)((double)x * (double)x * 2.0);
        }
        const double d = (double)x - 1.0;
        return (float)(1.0 - d * 2.0 * d);
    }
    const float scale = (float)(1 << (n - 1));
    if (x < 0.5f)
    {
        float p = x;
        for (int i = 1; i < n; i++)
        {
            p = (float)((double)p * (double)x);
        }
        return (float)((double)scale * (double)p);
    }
    const float signed_scale = (n & 1) == 0 ? -scale : scale;
    const float d = (float)((double)x - 1.0);
    float p = d;
    for (int i = 1; i < n; i++)
    {
        p = (float)((double)p * (double)d);
    }
    return (float)((double)p * (double)signed_scale + 1.0);
}

// 0x0056C080 (:538)
// Where x lies between x0 and x1 (0 at x0, 1 at x1), optionally clamped to [0, 1]; 0 when x0 == x1.
float fl_parameter(float x, float x0, float x1, bool clamp_parameter)
{
    if (x1 - x0 == 0.0f)
    {
        return 0.0f;
    }
    float t = (x - x0) / (x1 - x0);
    if (clamp_parameter)
    {
        if (t - 0.0f < 0.0f)
        {
            t = 0.0f;
        }
        if (0.0f <= t - 1.0f)
        {
            t = 1.0f;
        }
    }
    return t;
}

// 0x0056C120 (:569): y0 to y1 at x's parameter between x0 and x1 (y0 when x0 == x1).
float fl_parameter(float x, float x0, float x1, float y0, float y1, bool clamp_parameter)
{
    return y0 + fl_parameter(x, x0, x1, clamp_parameter) * (y1 - y0);
}

// 0x006C3820 (.h:542)
bool fl_greater_than_robust(float a, float b, float epsilon)
{
    return fl_calc_scaled_epsilon(a, b, epsilon) + b < a; // inlined
}

// 0x004B1B00 (.h:500): within the epsilon scaled by the larger magnitude (at least 1)
bool fl_equal_robust(float a, float b, float epsilon)
{
    const float scaled = fl_calc_scaled_epsilon(a, b, epsilon); // inlined
    return (double)b - (double)scaled <= (double)a && (double)a <= (double)scaled + (double)b;
}

// 0x0056C6A0 (:698)
bool fl_greater_than_equal_robust(float a, float b, float epsilon)
{
    return b - fl_calc_scaled_epsilon(a, b, epsilon) <= a; // inlined
}

// 0x0056C760 (:727)
// Original: true for every value with an all-ones exponent, infinities included.
bool fl_is_nan(float number)
{
    unsigned int bits;
    memcpy(&bits, &number, sizeof(bits));
    return (bits & 0x7F800000) == 0x7F800000; // the exponent field
}

// 0x0056C740 (:709)
// Not an infinity or NaN (exponent bits not all set).
bool fl_is_finite(float number)
{
    union
    {
        float f;
        unsigned int u;
    } bits;
    bits.f = number;
    return (bits.u & 0x7F800000) != 0x7F800000;
}

// 0x0056BC70 (:412). An ease in / ease out curve: percent_ramping of the range is spent accelerating (the first half)
// and decelerating (the last half), linear between. The chains are evaluated in double as the binary does
// (player_minimal.md P2).
float fl_ramp(float percent_complete, float percent_ramping)
{
    if (percent_ramping == 0.0f) {
        return percent_complete;
    }
    const float half = (float)((double)percent_ramping * 0.5);
    const float linear_end = (float)(1.0 - (double)half);
    const float slope = (float)(1.0 / (double)linear_end);
    if (percent_complete < half) {
        return (float)((double)percent_complete * (double)percent_complete * ((double)slope / (double)half) * 0.5);
    }
    const float accel = (float)((double)slope / (double)half * 0.5);
    const float ramp_in = (float)((double)half * (double)accel * (double)half);
    if (percent_complete < linear_end) {
        return (float)(((double)percent_complete - (double)half) * (double)slope + (double)ramp_in);
    }
    const float t = (float)((double)percent_complete - (double)linear_end);
    return (float)((double)slope * (double)t - (double)t * (double)accel * (double)t + (double)slope * (1.0 - (double)percent_ramping) + (double)ramp_in);
}

// 0x0056C1D0 (:577). The angle wrapped into [min_ang, min_ang + 2 pi), in double as the binary does (2 pi as the float
// constant).
float fl_angle_cap_radians(float angle_rad, float min_ang)
{
    const double two_pi = (double)6.2831855f;
    if ((double)min_ang + two_pi <= (double)angle_rad) {
        return (float)((double)angle_rad - (double)(float)(int)(((double)angle_rad - (double)min_ang) / two_pi) * two_pi);
    }
    if (angle_rad < min_ang) {
        angle_rad = (float)((double)(float)(int)(((double)min_ang + two_pi - (double)angle_rad) / two_pi) * two_pi + (double)angle_rad);
    }
    return angle_rad;
}

// 0x0056C4F0 (:623). a - b wrapped into [-pi, pi), in double as the binary does (pi and 2 pi as the float constants;
// each partial result rounded to float where the asm stores one).
float fl_angle_between_radians(float angle_a, float angle_b)
{
    const double pi = (double)3.1415927f;
    const double two_pi = (double)6.2831855f;
    float d = (float)((double)angle_a - (double)angle_b);
    if ((double)d >= pi) {
        const float over = (float)((double)d + pi);
        return (float)((double)d - (double)(float)(int)((double)over / two_pi) * two_pi);
    }
    if (-pi > (double)d) {
        const float under = (float)(pi - (double)d);
        d = (float)((double)(float)(int)((double)under / two_pi) * two_pi + (double)d);
    }
    return d;
}

// 0x0045ABD0 (.h:277): the sine, the cosine through _cos (x87 fcos/fsin on the float)
float fl_sincos(float fl, float* _cos)
{
    *_cos = (float)cos((double)fl);
    return (float)sin((double)fl);
}

// Wraps an angle into [0, 2pi) the way fl_angle_lerp_radians does (inlined three times there): the constants are the
// float 2pi and pi widened to double, the steps run in double and round where the source assigns a float.
static float fl_angle_wrap_radians(float a)
{
    const double two_pi = (double)6.2831855f; // magic number: the float 2pi
    const float hi = (float)(0.0 + two_pi);
    if ((double)a >= (double)hi)
    {
        const float range = (float)((double)a - 0.0);
        const int n = (int)((double)range / two_pi);
        return (float)((double)a - (double)(float)n * two_pi);
    }
    if (0.0f > a)
    {
        const float range = (float)((double)hi - (double)a);
        const int n = (int)((double)range / two_pi);
        return (float)((double)(float)n * two_pi + (double)a);
    }
    return a;
}

// 0x0056C2A0 (:601)
// From ang_from to ang_to by pct the short way round; both and the result in [0, 2pi).
float fl_angle_lerp_radians(float ang_from, float ang_to, float pct)
{
    const double two_pi = (double)6.2831855f; // magic number: the float 2pi
    const double pi = (double)3.1415927f; // magic number: the float pi
    ang_from = fl_angle_wrap_radians(ang_from);
    ang_to = fl_angle_wrap_radians(ang_to);
    if ((double)(float)fabs((double)ang_from - (double)ang_to) > pi)
    {
        if ((double)ang_to > (double)ang_from)
        {
            ang_to = (float)((double)ang_to - two_pi);
        }
        else
        {
            ang_to = (float)((double)ang_to + two_pi);
        }
    }
    const float result = (float)(((double)ang_to - (double)ang_from) * (double)pct + (double)ang_from);
    return fl_angle_wrap_radians(result);
}

// 0x0056C790 (:1063)
bool fl_find_quadratic_roots(float& root_1, float& root_2, float a, float b, float c)
{
    if (a == 0.0f) {
        if (b != 0.0f) {
            root_1 = -c / b;
            root_2 = -c / b;
            return true;
        }
    }
    else if (b == 0.0f) {
        if (0.0f <= -c / a) {
            const float root = sqrtf(-c / a);
            root_1 = root;
            root_2 = -root;
            return true;
        }
    }
    else if (a * 2.0f != 0.0f) {
        const float half_b_over_a = b / (a * 2.0f);
        const float scale = half_b_over_a * a * half_b_over_a;
        if (scale != 0.0f) {
            const float ratio = c / scale;
            const float discriminant = 1.0f - ratio;
            if (0.0f <= discriminant) {
                const float q = -1.0f - sqrtf(discriminant);
                root_1 = q * half_b_over_a;
                root_2 = (ratio * half_b_over_a) / q;
                return true;
            }
        }
    }
    return false;
}

// 0x0059ACA0 (:760): moves position and velocity towards a goal at up to max_speed, slowing over damp_distance. Works in
// up to 8 constant acceleration segments per call; true when the goal was reached.
bool fl_ramp_plateau(float& position, float& velocity, float goal, float max_speed, float damp_distance, float time_left, float tolerance, float opposite_dir_decel)
{
    const float decel = (max_speed * max_speed) / (damp_distance * 2.0f);
    for (unsigned int segment = 0; (double)tolerance < (double)time_left && segment <= 7; segment++) {
        const float delta = goal - position;
        const float dist = fabsf(delta);
        if (dist <= tolerance) {
            position = goal;
            velocity = 0.0f;
            return true;
        }
        const float dir = delta < 0.0f ? -1.0f : 1.0f;
        float accel = dir * decel;
        const float v = velocity;
        float step;
        if (v * delta < -tolerance) {
            // Going the wrong way.
            if (0.0f < opposite_dir_decel) {
                accel = dir * opposite_dir_decel;
            }
            step = (float)((double)-v / (double)accel);
        }
        else if (dist <= damp_distance + tolerance) {
            // Slowing down to the goal.
            const float ideal = sqrtf(v * 0.5f * v + accel * delta) * dir;
            double difference = (double)ideal - (double)v;
            if (tolerance < fabsf((float)difference)) {
                if (dir != ((float)difference < 0.0f ? -1.0f : 1.0f)) {
                    difference = (double)-v;
                    accel = (-v * v) / (delta * 2.0f);
                }
                step = (float)(difference / (double)accel);
            }
            else {
                velocity = ideal;
                step = (float)((double)delta * 2.0 / (double)ideal);
                accel = -accel;
            }
        }
        else {
            const float cruise = dir * max_speed;
            const double difference = (double)cruise - (double)v;
            const float to_damp = delta - dir * damp_distance;
            if (fabsf((float)difference) <= tolerance) {
                // At cruising speed.
                accel = 0.0f;
                velocity = cruise;
                step = time_left;
                if (fabsf(to_damp) < fabsf(cruise * time_left)) {
                    step = (float)((double)to_damp / (double)cruise);
                }
            }
            else {
                if (max_speed < fabsf(v)) {
                    accel = -accel;
                }
                if (fabsf((cruise * cruise - v * v) / (accel * 2.0f)) <= fabsf(to_damp)) {
                    step = (float)(difference / (double)accel);
                }
                else {
                    float root_1;
                    float root_2;
                    if (!fl_find_quadratic_roots(root_1, root_2, accel * 0.5f, v, -to_damp)) {
                        return true;
                    }
                    step = root_2;
                    if (fl_sign(root_1) == fl_sign(root_2)) {
                        if (root_1 - root_2 < 0.0f) {
                            step = root_1;
                        }
                    }
                    else if (0.0f <= root_1 - root_2) {
                        step = root_1;
                    }
                }
            }
        }
        if (0.0f <= step - time_left) {
            step = time_left;
        }
        position = accel * 0.5f * step * step + velocity * step + position;
        velocity = step * accel + velocity;
        time_left = time_left - step;
    }
    if (tolerance < fabsf(goal - position)) {
        return false;
    }
    position = goal;
    velocity = 0.0f;
    return true;
}

// 0x0056C780 (:734)
// Sic: despite the name, the value is +infinity (rfg.exe's constant 0x7F800000).
float fl_nan()
{
    return (float)HUGE_VAL;
}
