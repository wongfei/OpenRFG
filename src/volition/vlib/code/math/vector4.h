// Original: volition/vlib/code/math/vector4.h
// Implementation: volition/vlib/code/math/vlib_vector4.cpp
//
// 4D vector (x, y, z, w) with vec4_* helpers mirroring the vector ones. 16-byte aligned (SIMD): render_lib places
// vector4 members at padded 16-byte offsets (rl_state_manager::m_fog_parameters at 0xC0 after members ending at 0xBC;
// see tools/hdrgen/aligncheck.py). The PDB does not record alignment; alignas(16) is inferred.
#pragma once

class matrix44;
class vector;

// size 0x10
class alignas(16) vector4
{
public:
    vector4(float _x, float _y, float _z, float _w); // 0x004B48A0 (.h:103)
    vector4(const vector& _v, float _w); // 0x00585410 (vlib_vector4.cpp:85)
    vector4(const vector& _v); // 0x005853E0 (vlib_vector4.cpp:68)
    vector4(const vector4& _v); // 0x004B4880 (.h:84)
    vector4(); // 0x004B4870 (.h:81)
    vector4 operator-(const vector4& _v) const; // 0x005633B0 (.h:155)
    vector4 operator-() const;
    vector4 operator+(const vector4& _v) const; // 0x00563330 (.h:139)
    float operator*(const vector4& _v) const; // 0x00563510 (.h:338)
    vector4 operator*(float scale) const; // 0x00563430 (.h:171)
    vector4 operator/(float scale) const; // 0x005634A0 (.h:187)
    void operator+=(const vector4& _v); // 0x004B48D0 (.h:204)
    void add_scalar(float);
    void subtract_scalar(float);
    void operator-=(const vector4& _v); // 0x004B4950 (.h:277)
    void operator*=(float scale); // 0x004B49D0 (.h:296)
    void operator/=(float divisor); // 0x004B4A40 (.h:316)
    vector4 operator^(const vector4& _v) const; // 0x00563540 (.h:357)
    void operator()(float, float, float, float);
    bool operator==(const vector4&) const;
    bool operator!=(const vector4&) const;
    void set(float _x, float _y, float _z, float _w); // 0x004B4AC0 (.h:425)
    void multiply_components(const vector4&);
    float mag_squared() const; // 0x005635E0 (.h:461)
    float mag() const; // 0x00563610 (.h:480)
    void sin_components();
    void cos_components();
    float& operator[](const unsigned int index); // 0x004B4AF0 (.h:536)
    float mag_approx() const; // 0x00585440 (vlib_vector4.cpp:166)
    float dot4(float _x, float _y, float _z, float _w) const; // 0x00585510 (vlib_vector4.cpp:217)
    float dot3(float _x, float _y, float _z) const; // 0x005855A0 (vlib_vector4.cpp:262)
    float dot3(const vector& _v) const; // 0x00585570 (vlib_vector4.cpp:249)
    float dot3(const vector4& _v) const; // 0x00585540 (vlib_vector4.cpp:237)
    bool is_null() const; // 0x005855D0 (vlib_vector4.cpp:293)
    bool is_valid() const; // 0x00585630 (vlib_vector4.cpp:314)
    vector4 copy_normalize() const; // 0x00585690 (vlib_vector4.cpp:331)
    vector4 copy_normalize_safe() const; // 0x00585760 (vlib_vector4.cpp:353)
    vector4 copy_negate() const; // 0x00585860 (vlib_vector4.cpp:396)
    vector4 copy_rotate(const matrix44& _m) const; // 0x005858B0 (vlib_vector4.cpp:417)
    vector4 copy_unrotate(const matrix44& _m) const; // 0x00585A10 (vlib_vector4.cpp:451)
    void normalize(); // 0x00585B70 (vlib_vector4.cpp:493)
    void normalize_safe(); // 0x00585CD0 (vlib_vector4.cpp:560)
    float mag_and_normalize(); // 0x00585C20 (vlib_vector4.cpp:524)
    float mag_and_normalize_safe(); // 0x00585DC0 (vlib_vector4.cpp:615)
    void negate(); // 0x00585EB0 (vlib_vector4.cpp:672)
    void rotate(const matrix44& _m); // 0x00585EF0 (vlib_vector4.cpp:694)
    void unrotate(const matrix44& _m); // 0x00586050 (vlib_vector4.cpp:726)
    void zero(); // 0x005861B0 (vlib_vector4.cpp:759)
    void rand_sphere(); // 0x005861D0 (vlib_vector4.cpp:774)
    void rand_around_dir(const vector4& dir, float outerdotfactor, float innerdotfactor); // 0x005AA100 (vlib_vector4.cpp:843)
    void rand_around_z(float outerdotfactor, float innerdotfactor); // 0x005862D0 (vlib_vector4.cpp:800)

    float x; // 0x00
    float y; // 0x04
    float z; // 0x08
    float w; // 0x0C
};

// ---- inline members

// 0x004B4870 (.h:81)
// Uninitialized.
inline vector4::vector4()
{
}

// 0x004B4880 (.h:84)
inline vector4::vector4(const vector4& _v)
    : x(_v.x)
    , y(_v.y)
    , z(_v.z)
    , w(_v.w)
{
}

// 0x004B48A0 (.h:103)
inline vector4::vector4(float _x, float _y, float _z, float _w)
    : x(_x)
    , y(_y)
    , z(_z)
    , w(_w)
{
}

// 0x004B4AC0 (.h:425)
inline void vector4::set(float _x, float _y, float _z, float _w)
{
    x = _x;
    y = _y;
    z = _z;
    w = _w;
}

// 0x00563330 (.h:139)
inline vector4 vector4::operator+(const vector4& _v) const
{
    return vector4(_v.x + x, _v.y + y, _v.z + z, _v.w + w);
}

// 0x00563430 (.h:171)
inline vector4 vector4::operator*(float scale) const
{
    return vector4(x * scale, y * scale, z * scale, w * scale);
}

vector4 operator*(float scale, const vector4& _v); // 0x00C9B4B0 (.h:684)
void vec4_perp(vector4& dest, const vector4& p0, const vector4& p1, const vector4& p2); // 0x00597110 (.h:792)
float vec4_projection_parallel(vector4& component, const vector4& src, const vector4& unit_vector4); // 0x005863C0 (vlib_vector4.cpp:872)
void vec4_projection_onto_plane(vector4& projection, const vector4& src, const vector4& unit_normal); // 0x005864B0 (vlib_vector4.cpp:894)
float vec4_dist(const vector4& v0, const vector4& v1); // 0x005865F0 (vlib_vector4.cpp:913)
float vec4_dist_squared(const vector4& v0, const vector4& v1); // 0x005866C0 (vlib_vector4.cpp:925)
float vec4_xz_dist_squared(const vector4& v0, const vector4& v1); // 0x00586790 (vlib_vector4.cpp:935)
float vec4_xz_dist(const vector4& v0, const vector4& v1); // 0x00586800 (vlib_vector4.cpp:947)
float vec4_dist_approx(const vector4& v0, const vector4& v1); // 0x00586880 (vlib_vector4.cpp:961)
float vec4_dist_to_plane(const vector4& checkp, const vector4& norm, const vector4& planep); // 0x005869B0 (vlib_vector4.cpp:977)
float vec4_normalized_dir(vector4& dest, const vector4& end, const vector4& start); // 0x00586A20 (vlib_vector4.cpp:995)
bool vec4_test_parallel(const vector4& v0, const vector4& v1, float epsilon); // 0x00586B70 (vlib_vector4.cpp:1012)
void vec4_surface_normal(vector4& dest, const vector4& p0, const vector4& p1, const vector4& p2); // 0x005AA180 (vlib_vector4.cpp:1041)
void pltind_vec4_perp(vector4& dest, const vector4& p0, const vector4& p1, const vector4& p2); // 0x00586C50 (vlib_vector4.cpp:1058)
float vec4_delta_ang(const vector4& v0, const vector4& v1, const vector4& fvec); // 0x00586D90 (vlib_vector4.cpp:1078)
float vec4_dot_to_point(const vector4& dir, const vector4& p1, const vector4& p2); // 0x005871A0 (vlib_vector4.cpp:1121)
void vec4_project_point_onto_plane(vector4& new_point, const vector4& point, const vector4& plane_normal, const vector4& plane_point); // 0x005872F0 (vlib_vector4.cpp:1137)
void vec4_find_bounding_sphere(vector4& center, float& radius, const vector4* pnts, unsigned int num_pnts); // 0x005874A0 (vlib_vector4.cpp:1252)
float vec4_get_centroid(vector4& centroid, unsigned int nv, const vector4* src); // 0x005AA370 (vlib_vector4.cpp:1353)
float vec4_calc_parametric_pos(const vector4& p, const vector4& start, const vector4& end); // 0x00588070 (vlib_vector4.cpp:1417)
void vec4_change_to_local_coords(vector4& v, const vector4& coord_system_pos, const matrix44& coord_system_orient); // 0x005881D0 (vlib_vector4.cpp:1468)
void vec4_change_to_global_coords(vector4& v, const vector4& coord_system_pos, const matrix44& coord_system_orient); // 0x005883D0 (vlib_vector4.cpp:1480)
float vec4_find_closest_dist_squared_to_line(const vector4& p, const vector4& seg1, const vector4& seg2, vector4* closest_pt); // 0x005885B0 (vlib_vector4.cpp:1494)
float vec4_find_closest_dist_squared_to_linesegment(const vector4& p, const vector4& seg1, const vector4& seg2, vector4* closest_pt); // 0x005888D0 (vlib_vector4.cpp:1539)
float vec4_find_closest_dist_squared_to_linesegment(float& closest_param_pos, const vector4& p, const vector4& seg1, const vector4& seg2); // 0x00588CB0 (vlib_vector4.cpp:1599)

extern vector4 ZERO_VECTOR4; // 0x01CE9930 (vlib_vector4.cpp; defined in vlib_vector3.cpp until that file is recovered)
