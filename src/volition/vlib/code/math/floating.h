// Original: volition/vlib/code/math/floating.h
// Implementation: volition/vlib/code/math/floating.cpp
//
// Float helpers: fixed point conversions (12.4, 24.8, 4.12), fast trigonometry approximations, determinants.
#pragma once

#include <math.h>

float fl_sincos(float fl, float* _cos); // 0x0045ABD0 (.h:277)
bool fl_equal(float a, float b, float tol); // 0x004B1A20 (.h:458)
float fl_calc_scaled_epsilon(float a, float b, float epsilon); // 0x004B1A70 (.h:480)
bool fl_equal_robust(float a, float b, float epsilon); // 0x004B1B00 (.h:500)
bool fl_less_than_robust(float a, float b, float epsilon); // 0x004B1BC0 (.h:520)
bool fl_greater_than_robust(float a, float b, float epsilon); // 0x006C3820 (.h:542)
short FLOAT_TO_FIXED_4_12(float f); // 0x004B1C60 (.h:623)
short FLOAT_TO_FIXED_12_4(float f); // 0x00941890 (.h:629)
short FLOAT_TO_FIXED_12_4_FLOOR(float f); // 0x004B1C80 (.h:647)
short FLOAT_TO_FIXED_12_4_CEIL(float f); // 0x004B1D00 (.h:665)
short FLOAT_TO_FIXED_12_4_ROUND(float f); // 0x004B1D40 (.h:683)
float FIXED_12_4_TO_FLOAT(short s); // 0x004B1D70 (.h:699)
int FLOAT_TO_FIXED_24_8(float f); // 0x00710460 (.h:726)
int FLOAT_TO_FIXED_24_8_FLOOR(float f); // 0x00741CD0 (.h:732)
int INT_TO_FIXED_24_8(int i); // 0x00710480 (.h:755)
int FIXED_12_4_MUL(int a, int b); // 0x004B1D90 (.h:789)
int FIXED_24_8_MUL_TO_INT(int a, int b); // 0x00710490 (.h:840)
float uint_as_float(unsigned int val); // 0x005632B0 (.h:963)
// 0x005632C0 (.h:976): by the sign bit (-0 is negative)
inline float fl_sign(float f)
{
    return signbit(f) ? -1.0f : 1.0f;
}
float fl_continuous_interest(float rate, float time); // 0x006C38C0 (.h:986)
float fl_round(float n, float prec); // 0x0056BB50 (:347)
int fl_round(float n); // 0x0056BBD0 (:364)
unsigned int fl_uround(float f); // 0x0056BC00 (:379)
float fl_atan2_safe(float y, float x); // 0x0056BC30 (:394)
float fl_ramp(float percent_complete, float percent_ramping); // 0x0056BC70 (:412)
float fl_parametric_accel_decel(float x, int n); // 0x0056BDC0 (:482)
float fl_parameter(float x, float x0, float x1, bool clamp_parameter); // 0x0056C080 (:538)
float fl_parameter(float x, float x0, float x1, float y0, float y1, bool clamp_parameter); // 0x0056C120 (:569)
float fl_angle_cap_radians(float angle_rad, float min_ang); // 0x0056C1D0 (:577)
float fl_angle_lerp_radians(float ang_from, float ang_to, float pct); // 0x0056C2A0 (:601)
float fl_angle_between_radians(float angle_a, float angle_b); // 0x0056C4F0 (:623)
float fl_asqrt(float x); // 0x0056C5D0 (:629)
bool fl_less_than_equal_robust(float a, float b, float epsilon); // 0x0056C600 (:680)
bool fl_greater_than_equal_robust(float a, float b, float epsilon); // 0x0056C6A0 (:698)
bool fl_is_finite(float number); // 0x0056C740 (:709)
bool fl_is_nan(float number); // 0x0056C760 (:727)
float fl_nan(); // 0x0056C780 (:734)
bool fl_ramp_plateau(float& position, float& velocity, float goal, float max_speed, float damp_distance, float time_left, float tolerance, float opposite_dir_decel); // 0x0059ACA0 (:760)
bool fl_find_quadratic_roots(float& root_1, float& root_2, float a, float b, float c); // 0x0056C790 (:1063)
float fl_acos(float fl); // 0x0056C920 (:1160)
float determinant22(float a1, float a2, float b1, float b2); // 0x0056C940 (:1183)
unsigned short fl_pack16(float f); // 0x0056C960 (:1197)
float fl_unpack16(unsigned short fpacked); // 0x0056C9D0 (:1247)
float fl_unpack24f8(unsigned int fpacked); // 0x0056CA40 (:1293)
