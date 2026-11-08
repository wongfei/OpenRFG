// Original: volition/vlib/code/math/spline.h
// Implementation: volition/vlib/code/math/spline.cpp
//
// Splines over caller-owned control points: cardinal (projectile arcs), 2D Catmull-Rom (turret sweeps) and cubic Bezier
// (vehicle route segments), plus the 2D cardinal and Hermite splines, the 3D Catmull-Rom spline, a fixed 20-point Bezier
// curve (bez_spline) and bicubic Bezier patches.
#pragma once

#include "volition/vlib/code/math/vector3.h"

class vector2;

// size 0xC
class cardinal_spline
{
public:
    cardinal_spline(int _num_pts, const vector* _pts, float _tension); // 0x005A5DF0 (:148)
    cardinal_spline(); // 0x0057A810 (:142)
    void set_points(int _num_pts, const vector* _pts); // 0x0057A820 (:155)
    void get_point(vector& out, float u) const; // 0x0057A840 (:173)
    void get_tangent(vector& out, float u) const; // 0x0057AB00 (:217)
    void set_tension(float _t); // 0x0057A830 (:165)
    float get_closest_point_approx(vector p, bool two_d) const; // 0x005A5E10 (:266)

    const vector* pts; // 0x00
    int num_pts; // 0x04
    float tension; // 0x08
};

// size 0x8
class catmull_rom_spline_2d
{
public:
    catmull_rom_spline_2d(int _num_pts, const vector2* _pts); // 0x005A6B80 (:692)
    catmull_rom_spline_2d(); // 0x0057BD00 (:687)
    void set_points(int _num_pts, const vector2* _pts); // 0x0057BD10 (:698)
    void get_point(vector2& out, float u) const; // 0x0057BD20 (:710)
    void get_tangent(vector2& out, float u) const; // 0x0057BF90 (:752)

    const vector2* pts; // 0x00
    int num_pts; // 0x04
};

// size 0x10
class cubic_bezier_spline
{
public:
    cubic_bezier_spline(int _num_pts, const vector* _pts, const vector* _in_pts, const vector* _out_pts); // 0x005A6D20 (:1042)
    cubic_bezier_spline(); // 0x0057C950 (:1033)
    void set_points(int _num_pts, const vector* _pts, const vector* _in_pts, const vector* _out_pts); // 0x0057C960 (:1048)
    void get_point(vector& out, float u) const; // 0x0057C980 (:1060)
    void get_tangent(vector& out, float u) const; // 0x0057CCE0 (:1126)
    void get_second_derivative(vector& out, float u) const; // 0x0057D100 (:1168)
    float get_closest_point_approx(const vector& p) const; // 0x005A6D40 (:1214)
    float get_length_approx(int num_steps, float start_pos, float end_pos) const; // 0x0057D660 (:1332)
    void split_at_parametric_pos(float t, cubic_bezier_spline* spline1, cubic_bezier_spline* spline2) const; // 0x0057D7F0 (:1376)

    const vector* knots; // 0x00
    const vector* incoming_handles; // 0x04
    const vector* outgoing_handles; // 0x08
    int num_knots; // 0x0C
};

// size 0xC
class cardinal_spline_2d
{
public:
    cardinal_spline_2d(int _num_pts, const vector2* _pts, float _tension); // 0x005A64E0 (:365)
    cardinal_spline_2d(); // 0x0057AFB0 (:359)
    void set_points(int _num_pts, const vector2* _pts); // 0x0057AFC0 (:372)
    void get_point(vector2& out, float u) const; // 0x0057AFE0 (:386)
    void get_tangent(vector2& out, float u) const; // 0x0057B2A0 (:430)
    void set_tension(float _t); // 0x0057AFD0 (:381)
    float get_closest_point_approx(vector2 p) const; // 0x005A6500 (:480)

    const vector2* pts; // 0x00
    int num_pts; // 0x04
    float tension; // 0x08
};

// size 0x8
class catmull_rom_spline
{
public:
    catmull_rom_spline(int _num_pts, const vector* _pts); // 0x005A6B60 (:580)
    catmull_rom_spline(); // 0x0057B600 (:575)
    void set_points(int _num_pts, const vector* _pts); // 0x0057B610 (:587)
    void get_point(vector& out, float u) const; // 0x0057B620 (:599)
    void get_tangent(vector& out, float u) const; // 0x0057B900 (:641)

    const vector* pts; // 0x00
    int num_pts; // 0x04
};

// size 0xC
class hermite_spline_2d
{
public:
    hermite_spline_2d(int _num_pts, const vector2* _pts, const vector2* _tangents); // 0x005A6BA0 (:815)
    hermite_spline_2d(); // 0x0057C270 (:810)
    void set_points(int _num_pts, const vector2* _pts, const vector2* _tangents); // 0x0057C280 (:821)
    void get_point(vector2& out, float u) const; // 0x0057C2A0 (:833)
    void get_tangent(vector2& out, float u) const; // 0x0057C580 (:876)

    const vector2* pts; // 0x00
    const vector2* tangents; // 0x04
    int num_pts; // 0x08
};

// size 0xF4
class bez_spline
{
public:
    bez_spline(int _num_pts, const vector* _pts); // 0x005A6BC0 (:936)
    bez_spline(); // 0x0057C840 (:930)
    void bez_set_points(int _num_pts, const vector* _pts); // 0x0057C850 (:942)
    float BEZ(int k, int n, float u) const; // 0x0057C890 (:961)
    void bez_get_point(vector* out, float u) const; // 0x005A6C00 (:976)

    vector pts[20]; // 0x00
    int num_pts; // 0xF0
};

// size 0xC
class bicubic_bezier_patch
{
public:
    bicubic_bezier_patch(int _num_control_points_u, int _num_control_points_v, const vector* _control_points); // 0x005A7370 (:1431)
    bicubic_bezier_patch(); // 0x0057DDC0 (:1416)
    void set_points(int _num_control_points_u, int _num_control_points_v, const vector* _control_points); // 0x0057DDD0 (:1444)
    void get_point(vector& out, float u, float v) const; // 0x005A7390 (:1462)

    int num_control_points_u; // 0x00
    int num_control_points_v; // 0x04
    const vector* control_points; // 0x08
};
