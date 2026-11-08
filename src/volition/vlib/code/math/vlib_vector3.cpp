// Original: volition/vlib/code/math/vlib_vector3.cpp
//
// Vector math (decompiled), including the inline functions of vector3.h. Conventions: copy_rotate/rotate take a world
// direction into the matrix's frame (dot products with rvec, uvec, fvec); copy_unrotate/unrotate go back (x * rvec +
// y * uvec + z * fvec). The _safe normalizations give (1, 0, 0) for vectors shorter than 1e-7.
//
// Partial: vec_find_bounding_sphere and
// vec_find_closest_dist_squared_to_triangle are not implemented yet.

#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/math/vector4.h"
#include "volition/vlib/code/math/vector2.h"

#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"

#include <math.h>

const vector2 ZERO_VECTOR2(0.0f, 0.0f); // 0x01CE9910 (vlib_vector2.cpp, not recovered yet)

// ---- vlib_vector2.cpp (not recovered yet; the members P4 needs)
// 0x0057FAC0 (vlib_vector2.cpp:128). A zero vector becomes (1, 0).
void vector2::normalize_safe()
{
    const float len = sqrtf(x * x + y * y);
    if (len == 0.0f)
    {
        x = 1.0f;
        y = 0.0f;
        return;
    }
    x = x / len;
    y = y / len;
}

// 0x0057FFB0 (vlib_vector2.cpp:230)
void vector2::zero()
{
    x = 0.0f;
    y = 0.0f;
}

// 0x0057FFC0 (vlib_vector2.cpp:240)
bool vector2::is_null() const
{
    return x == 0.0f && y == 0.0f;
}
vector ZERO_VECTOR(0.0f, 0.0f, 0.0f); // 0x01CE9918
vector4 ZERO_VECTOR4(0.0f, 0.0f, 0.0f, 0.0f); // 0x01CE9930 (vlib_vector4.cpp, not recovered yet)
vector RIGHT_VECTOR(1.0f, 0.0f, 0.0f); // 0x0163DC98
vector UP_VECTOR(0.0f, 1.0f, 0.0f); // 0x0163DCA4
vector FORWARD_VECTOR(0.0f, 0.0f, 1.0f); // 0x0163DCB0

// Axis pairs following the largest component (find_largest_component).
static const unsigned int ij_table[3][2] = {{1, 2}, {2, 0}, {0, 1}};

static const float VECTOR_EPSILON = 1e-07f;

// Not in the binary: the squared length as rfg.exe computes it before a square root (summed in double, rounded once;
// checked by the hkdiff glass_vlib_math test).
static float vector_sum_of_squares(float x, float y, float z)
{
    return (float)((double)x * x + (double)y * y + (double)z * z);
}


// ---- vector3.h
// 0x00482C90 (.h:42)
vector::vector()
{
}

// 0x00482CA0 (.h:46)
vector::vector(const vector& v)
    : x(v.x)
    , y(v.y)
    , z(v.z)
{
}

// 0x00482CC0 (.h:52)
vector::vector(float _x, float _y, float _z)
    : x(_x)
    , y(_y)
    , z(_z)
{
}

// 0x004B4B00 (.h:60)
vector vector::operator-() const
{
    return vector(-x, -y, -z);
}

// 0x004B4B40 (.h:66)
vector vector::operator+(const vector& v) const
{
    return vector(x + v.x, y + v.y, z + v.z);
}

// 0x00482CF0 (.h:72)
vector vector::operator-(const vector& v) const
{
    return vector(x - v.x, y - v.y, z - v.z);
}

// 0x004B4BA0 (.h:78)
vector vector::operator+(float f) const
{
    return vector(x + f, y + f, z + f);
}

// 0x00563650 (.h:84)
vector vector::operator-(float f) const
{
    return vector(x - f, y - f, z - f);
}

// 0x004B4C00 (.h:90)
vector vector::operator*(float scale) const
{
    return vector(x * scale, y * scale, z * scale);
}

// 0x004B4C60 (.h:95)
// Divides each component (operator/= multiplies by the reciprocal instead; checked in the binary, P4).
vector vector::operator/(float scale) const
{
    return vector(x / scale, y / scale, z / scale);
}

// 0x004B4CC0 (.h:101)
void vector::operator+=(const vector& v)
{
    x += v.x;
    y += v.y;
    z += v.z;
}

// 0x005DB210 (.h:113)
void vector::add_scalar(float f)
{
    x += f;
    y += f;
    z += f;
}

// 0x005DB260 (.h:125)
void vector::subtract_scalar(float f)
{
    x -= f;
    y -= f;
    z -= f;
}

// 0x00482D50 (.h:137)
void vector::operator-=(const vector& v)
{
    x -= v.x;
    y -= v.y;
    z -= v.z;
}

// 0x004B4D20 (.h:149)
void vector::operator*=(float scale)
{
    x *= scale;
    y *= scale;
    z *= scale;
}

// 0x004B4D70 (.h:161)
void vector::operator/=(float divisor)
{
    const float inv = 1.0f / divisor;
    x *= inv;
    y *= inv;
    z *= inv;
}

// 0x00482DB0 (.h:174)
float vector::operator*(const vector& v) const
{
    return x * v.x + y * v.y + z * v.z;
}

// 0x004B4DE0 (.h:180)
vector vector::operator^(const vector& v) const
{
    return vector(y * v.z - z * v.y, v.x * z - x * v.z, x * v.y - v.x * y);
}

// 0x005636B0 (.h:186)
void vector::operator()(float _x, float _y, float _z)
{
    x = _x;
    y = _y;
    z = _z;
}

// 0x005DB2B0 (.h:196)
bool vector::operator==(const vector& v) const
{
    return x == v.x && y == v.y && z == v.z;
}

// 0x00482DE0 (.h:206)
bool vector::operator!=(const vector& v) const
{
    return x != v.x || y != v.y || z != v.z;
}

// 0x004B4E70 (.h:216)
void vector::set(float _x, float _y, float _z)
{
    x = _x;
    y = _y;
    z = _z;
}

// 0x004B4EA0 (.h:226)
void vector::multiply_components(const vector& v)
{
    x *= v.x;
    y *= v.y;
    z *= v.z;
}

// 0x004B4F00 (.h:238)
float vector::mag_squared() const
{
    return x * x + y * y + z * z;
}

// 0x005DB310 (.h:246)
float vector::mag_xz_squared() const
{
    return x * x + z * z;
}

// 0x004B4F30 (.h:253)
float vector::dist_squared(const vector& v) const
{
    const float dx = x - v.x;
    const float dy = y - v.y;
    const float dz = z - v.z;
    return dx * dx + dy * dy + dz * dz;
}

// 0x004B4FD0 (.h:265)
float vector::mag() const
{
    return sqrtf(vector_sum_of_squares(x, y, z));
}

// 0x005DB330 (.h:273)
float vector::mag_xz() const
{
    return sqrtf(x * x + z * z);
}

// 0x007104A0 (.h:281)
float& vector::operator[](const unsigned int index)
{
    return (&x)[index];
}

// 0x006C38F0 (.h:304)
float vector::extract_pitch()
{
    return -asinf(y);
}

// 0x005DB360 (.h:310)
float vector::extract_heading()
{
    return fl_atan2_safe(x, z);
}

// 0x004B5000 (.h:319)
float vector::dot3(float _x, float _y, float _z) const
{
    return x * _x + y * _y + z * _z;
}

// 0x004B5030 (.h:542)
vector operator*(float scale, const vector& v)
{
    return vector(v.x * scale, v.y * scale, v.z * scale);
}

// 0x005DB380 (.h:551)
vector vec_add_scalar(const vector& v, float f)
{
    return vector(v.x + f, v.y + f, v.z + f);
}

// 0x005DB3E0 (.h:557)
vector vec_subtract_scalar(const vector& v, float f)
{
    return vector(v.x - f, v.y - f, v.z - f);
}

// 0x00597250 (.h:602)
// (p1 - p0) x (p2 - p1), not normalized.
void vec_perp(vector& dest, const vector& p0, const vector& p1, const vector& p2)
{
    const float ax = p1.x - p0.x;
    const float ay = p1.y - p0.y;
    const float az = p1.z - p0.z;
    const float bx = p2.x - p1.x;
    const float by = p2.y - p1.y;
    const float bz = p2.z - p1.z;
    dest.x = ay * bz - az * by;
    dest.y = bx * az - ax * bz;
    dest.z = ax * by - bx * ay;
}

// ---- vlib_vector3.cpp
// 0x00580630 (:76)
// i0 is the axis of the largest component; i1, i2 the other two in an order that keeps the handedness.
void vector::find_largest_component(unsigned int& i0, unsigned int& i1, unsigned int& i2) const
{
    float largest;
    if (fabsf(x) <= fabsf(y))
    {
        if (fabsf(z) < fabsf(y))
        {
            i0 = 1;
            largest = y;
        }
        else
        {
            i0 = 2;
            largest = z;
        }
    }
    else if (fabsf(z) < fabsf(x))
    {
        i0 = 0;
        largest = x;
    }
    else
    {
        i0 = 2;
        largest = z;
    }
    if (0.0f < largest)
    {
        i1 = ij_table[i0][0];
        i2 = ij_table[i0][1];
        return;
    }
    i1 = ij_table[i0][1];
    i2 = ij_table[i0][0];
}

// 0x00580710 (:123)
bool vector::is_unit(float tolerance) const
{
    const float d = 1.0f - (x * x + y * y + z * z);
    return d < tolerance && -d < tolerance;
}

// 0x00580790 (:133)
// max + 3/8 mid + 3/16 min (an approximation of the length).
float vector::mag_approx() const
{
    float a = fabsf(x);
    float b = fabsf(y);
    float c = fabsf(z);
    float hi;
    float mid;
    float lo;
    if (a < b)
    {
        hi = b;
        lo = a;
    }
    else
    {
        hi = a;
        lo = b;
    }
    mid = lo;
    if (lo < c)
    {
        mid = c;
        if (hi < c)
        {
            mid = hi;
            hi = c;
        }
    }
    else
    {
        lo = c;
    }
    const float t = mid * 0.25f + lo * 0.125f;
    return t * 0.5f + t + hi;
}

// 0x00580860 (:190)
bool vector::is_null() const
{
    return -VECTOR_EPSILON < x && x < VECTOR_EPSILON && -VECTOR_EPSILON < y && y < VECTOR_EPSILON && -VECTOR_EPSILON < z && z < VECTOR_EPSILON;
}

// 0x005808C0 (:201)
// No component is an infinity or NaN.
bool vector::is_valid() const
{
    const unsigned int* bits = (const unsigned int*)&x;
    return (bits[0] & 0x7F800000) != 0x7F800000 && (bits[1] & 0x7F800000) != 0x7F800000 && (bits[2] & 0x7F800000) != 0x7F800000;
}

// 0x00580920 (:212)
vector vector::copy_normalize() const
{
    const float inv = 1.0f / sqrtf(vector_sum_of_squares(x, y, z));
    return vector(x * inv, inv * y, z * inv);
}

// 0x005809C0 (:223)
vector vector::copy_normalize_safe() const
{
    const float len = sqrtf(vector_sum_of_squares(x, y, z));
    if (len - 0.0f < VECTOR_EPSILON && -(len - 0.0f) < VECTOR_EPSILON)
    {
        return vector(1.0f, 0.0f, 0.0f);
    }
    const float inv = 1.0f / len;
    return vector(x * inv, inv * y, z * inv);
}

// 0x00580AC0 (:244)
vector vector::copy_flatten_normalize() const
{
    const float inv = 1.0f / sqrtf(x * x + z * z);
    return vector(x * inv, 0.0f, inv * z);
}

// 0x00580B40 (:255)
vector vector::copy_flatten_normalize_safe() const
{
    const float len = sqrtf(x * x + z * z);
    if (len - 0.0f < VECTOR_EPSILON && -(len - 0.0f) < VECTOR_EPSILON)
    {
        return vector(1.0f, 0.0f, 0.0f);
    }
    const float inv = 1.0f / len;
    return vector(x * inv, 0.0f, inv * z);
}

// 0x00580C20 (:270)
vector vector::copy_negate() const
{
    return vector(-x, -y, -z);
}

// 0x00580C60 (:281)
vector vector::copy_rotate(const matrix& m) const
{
    return vector(m.rvec.y * y + m.rvec.x * x + m.rvec.z * z, m.uvec.x * x + m.uvec.y * y + m.uvec.z * z, m.fvec.x * x + m.fvec.y * y + m.fvec.z * z);
}

// 0x00580D40 (:295)
vector vector::copy_unrotate(const matrix& m) const
{
    return vector(m.uvec.x * y + m.rvec.x * x + m.fvec.x * z, m.rvec.y * x + m.uvec.y * y + m.fvec.y * z, m.rvec.z * x + m.uvec.z * y + m.fvec.z * z);
}

// 0x00580E20 (:307)
vector vector::copy_rotate_around_y_axis(float radians) const
{
    const float c = cosf(radians);
    const float s = sinf(radians);
    return vector(x * c + z * s, y, z * c - x * s);
}

// 0x00580EC0 (:322)
vector vector::copy_rotate_around_x_axis(float radians) const
{
    const float c = cosf(radians);
    const float s = sinf(radians);
    return vector(x, y * c - z * s, z * c + y * s);
}

// 0x00580F60 (:337)
vector vector::copy_rotate_around_z_axis(float radians) const
{
    const float c = cosf(radians);
    const float s = sinf(radians);
    return vector(x * c - y * s, y * c + x * s, z);
}

// 0x00581000 (:353): the squares summed in double and rounded to float, the root, then 1 / root in double rounded to
// float (multiplayer.md MP5: hkdiff_multi found the all-float form 1 ulp off)
void vector::normalize()
{
    const float len2 = (float)((double)x * x + (double)y * y + (double)z * z);
    const float inv = (float)(1.0 / (double)(float)sqrt((double)len2));
    x *= inv;
    y *= inv;
    z *= inv;
}

// 0x00581090 (:374)
float vector::mag_and_normalize()
{
    const float len = sqrtf(vector_sum_of_squares(x, y, z));
    const float inv = 1.0f / len;
    x *= inv;
    y *= inv;
    z *= inv;
    return len;
}

// 0x00581130 (:399)
float vector::flatten_mag_and_normalize()
{
    const float len = sqrtf(x * x + z * z);
    const float inv = 1.0f / len;
    x *= inv;
    y = 0.0f;
    z *= inv;
    return len;
}

// 0x005811B0 (:418)
void vector::normalize_safe()
{
    const float len = sqrtf(vector_sum_of_squares(x, y, z));
    if (len - 0.0f < VECTOR_EPSILON && -(len - 0.0f) < VECTOR_EPSILON)
    {
        x = 1.0f;
        y = 0.0f;
        z = 0.0f;
        return;
    }
    const float inv = 1.0f / len;
    x *= inv;
    y *= inv;
    z *= inv;
}

// 0x00581290 (:446)
void vector::flatten_normalize()
{
    const float inv = 1.0f / sqrtf(x * x + z * z);
    x *= inv;
    y = 0.0f;
    z *= inv;
}

// 0x00581310 (:462)
void vector::flatten_normalize_safe()
{
    const float len = sqrtf(x * x + z * z);
    if (len - 0.0f < VECTOR_EPSILON && -(len - 0.0f) < VECTOR_EPSILON)
    {
        x = 1.0f;
        y = 0.0f;
        z = 0.0f;
        return;
    }
    const float inv = 1.0f / len;
    x *= inv;
    y = 0.0f;
    z *= inv;
}

// 0x005813E0 (:491)
float vector::mag_and_normalize_safe()
{
    const float len = sqrtf(vector_sum_of_squares(x, y, z));
    if (len - 0.0f < VECTOR_EPSILON && -(len - 0.0f) < VECTOR_EPSILON)
    {
        x = 1.0f;
        y = 0.0f;
        z = 0.0f;
        return 0.0f;
    }
    const float inv = 1.0f / len;
    x *= inv;
    y *= inv;
    z *= inv;
    return len;
}

// 0x005814D0 (:524)
float vector::flatten_mag_and_normalize_safe()
{
    const float len = sqrtf(x * x + z * z);
    if (len - 0.0f < VECTOR_EPSILON && -(len - 0.0f) < VECTOR_EPSILON)
    {
        x = 1.0f;
        y = 0.0f;
        z = 0.0f;
        return 0.0f;
    }
    x = x * (1.0f / len);
    y = 0.0f;
    z = z * (1.0f / len);
    return len;
}

// 0x005815A0 (:552)
void vector::negate()
{
    x = -x;
    y = -y;
    z = -z;
}

// 0x005815D0 (:567)
void vector::rotate(const matrix& m)
{
    *this = copy_rotate(m);
}

// 0x005816A0 (:587)
void vector::unrotate(const matrix& m)
{
    *this = copy_unrotate(m);
}

// 0x00581770 (:606)
void vector::rotate_around_y_axis(float radians)
{
    *this = copy_rotate_around_y_axis(radians);
}

// 0x00581800 (:623)
void vector::rotate_around_x_axis(float radians)
{
    *this = copy_rotate_around_x_axis(radians);
}

// 0x00581890 (:640)
void vector::rotate_around_z_axis(float radians)
{
    *this = copy_rotate_around_z_axis(radians);
}

// 0x005A89D0 (:662)
// Rotates around axis by |axis| * mult radians (into the axis frame, a heading turn, and back).
void vector::rotate_around_vector(const vector& axis, float mult)
{
    if (axis.x > -VECTOR_EPSILON && axis.x < VECTOR_EPSILON && axis.y > -VECTOR_EPSILON && axis.y < VECTOR_EPSILON && axis.z > -VECTOR_EPSILON && axis.z < VECTOR_EPSILON)
    {
        return;
    }
    matrix turn;
    turn.set(0.0f, sqrtf(axis.y * axis.y + axis.x * axis.x + axis.z * axis.z) * mult, 0.0f);
    matrix frame;
    frame.make(axis);
    vector local = copy_rotate(frame);
    local.unrotate(turn);
    *this = local.copy_unrotate(frame);
}

// 0x00581A40 (:720)
// A random unit vector around +z: z in [outerdotfactor, innerdotfactor], then a random heading around z. The angle is
// x87: the next table value times 1 / 2^32 rounded to float, then times 2 pi (a double constant) rounded again; the
// CRT's double cos and sin of it, each rounded to float and multiplied by the radius.
void vector::rand_around_z(float outerdotfactor, float innerdotfactor)
{
    z = fl_rand_range(outerdotfactor, innerdotfactor);
    // An inlined table draw (wrapping with == 2048); vrand reads the same entry and advances the same way.
    const unsigned int value = Vlib_rand_instance.vrand();
    const float unit = (float)((double)value * 2.3283064365386963e-10); // 0x012C35C8: 1 / 2^32
    const float angle = (float)((double)unit * 6.2831854820251465); // 0x012BEE78: (double)(2 * pi as a float)
    const float radius = (float)sqrt((double)(float)(1.0 - (double)z * (double)z));
    x = (float)cos((double)angle) * radius;
    y = (float)sin((double)angle) * radius;
}

// 0x005A8D30 (:761)
// A random unit vector around dir: rand_around_z in the frame (side, dir x side, dir), where side is the flattened
// perpendicular (z, 0, -x) normalized. dir is not normalized. For a dir along y (|x| and |z| below 1e-4) the frame is
// fixed: side (1, 0, 0), dir (0, +-1, 0) and the second axis (0, 0, -+1).
void vector::rand_around_dir(const vector& dir, float outerdotfactor, float innerdotfactor)
{
    vector side;
    vector second;
    vector forward = dir;
    // Magic number: 0.0001, the threshold for a dir along the y axis.
    if (0.0001f <= forward.x || forward.x <= -0.0001f || 0.0001f <= forward.z || forward.z <= -0.0001f)
    {
        const float neg_x = -forward.x;
        const float inv = 1.0f / sqrtf(forward.z * forward.z + 0.0f + neg_x * neg_x);
        side.x = forward.z * inv;
        side.z = neg_x * inv;
        side.y = inv * 0.0f;
        second.x = side.z * forward.y - side.y * forward.z;
        second.y = forward.z * side.x - side.z * forward.x;
        second.z = side.y * forward.x - forward.y * side.x;
    }
    else
    {
        side = vector(1.0f, 0.0f, 0.0f);
        second = vector(0.0f, 0.0f, 1.0f);
        forward.z = 0.0f;
        forward.x = 0.0f;
        if (0.0f <= forward.y)
        {
            forward.y = 1.0f;
            second.z = -1.0f;
        }
        else
        {
            forward.y = -1.0f;
        }
    }
    rand_around_z(outerdotfactor, innerdotfactor);
    const float rx = x;
    const float ry = y;
    const float rz = z;
    x = second.x * ry + rx * side.x + rz * forward.x;
    y = side.y * rx + second.y * ry + forward.y * rz;
    z = side.z * rx + second.z * ry + forward.z * rz;
}

// 0x00581920 (:686)
void vector::zero()
{
    x = 0.0f;
    y = 0.0f;
    z = 0.0f;
}

// 0x00581B20 (:792)
float vec_projection_parallel(vector& component, const vector& src, const vector& unit_vector)
{
    const float d = src.y * unit_vector.y + src.x * unit_vector.x + src.z * unit_vector.z;
    component.x = unit_vector.x * d;
    component.y = unit_vector.y * d;
    component.z = unit_vector.z * d;
    return d;
}

// 0x00581BE0 (:811)
void vec_projection_onto_plane(vector& projection, const vector& src, const vector& unit_normal)
{
    const float d = -(src.y * unit_normal.y + src.x * unit_normal.x + src.z * unit_normal.z);
    projection = src;
    projection.x += unit_normal.x * d;
    projection.y += d * unit_normal.y;
    projection.z += unit_normal.z * d;
}

// 0x00581CD0 (:827)
float vec_dist(const vector& v0, const vector& v1)
{
    // The squares add in x87 double precision (y first), the sum rounds to float before the CRT square root (rfg.exe).
    const float dx = v0.x - v1.x;
    const float dy = v0.y - v1.y;
    const float dz = v0.z - v1.z;
    const float sum = (float)((double)dy * (double)dy + (double)dx * (double)dx + (double)dz * (double)dz);
    return (float)sqrt((double)sum);
}

// 0x00581D80 (:836)
float vec_dist_squared(const vector& v0, const vector& v1)
{
    const float dx = v0.x - v1.x;
    const float dy = v0.y - v1.y;
    const float dz = v0.z - v1.z;
    return dx * dx + dy * dy + dz * dz;
}

// 0x00581E20 (:843)
float vec_xz_dist_squared(const vector& v0, const vector& v1)
{
    const float dx = v0.x - v1.x;
    const float dz = v0.z - v1.z;
    return dx * dx + dz * dz;
}

// 0x00581E90 (:852)
float vec_xz_dist(const vector& v0, const vector& v1)
{
    return sqrtf(vec_xz_dist_squared(v0, v1));
}

// 0x00581F10 (:863)
float vec_dist_approx(const vector& v0, const vector& v1)
{
    const vector d(v0.x - v1.x, v0.y - v1.y, v0.z - v1.z);
    return d.mag_approx();
}

// 0x00582040 (:876)
float vec_dist_to_plane(const vector& checkp, const vector& norm, const vector& planep)
{
    return norm.z * (checkp.z - planep.z) + norm.x * (checkp.x - planep.x) + norm.y * (checkp.y - planep.y);
}

// 0x00582090 (:891)
float vec_normalized_dir(vector& dest, const vector& end, const vector& start)
{
    dest.x = end.x - start.x;
    dest.y = end.y - start.y;
    dest.z = end.z - start.z;
    return dest.mag_and_normalize();
}

// 0x005821A0 (:897)
// A zero direction stays zero (length 0).
float vec_normalized_dir_safe(vector& dest, const vector& end, const vector& start)
{
    dest.x = end.x - start.x;
    dest.y = end.y - start.y;
    dest.z = end.z - start.z;
    if (dest.is_null())
    {
        dest.zero();
        return 0.0f;
    }
    return dest.mag_and_normalize();
}

// 0x00582320 (:916)
bool vec_test_parallel(const vector& v0, const vector& v1, float epsilon)
{
    return fabsf(v1.z * v0.y - v1.y * v0.z) < epsilon && fabsf(v1.x * v0.z - v0.x * v1.z) < epsilon && fabsf(v0.x * v1.y - v1.x * v0.y) < epsilon;
}

// 0x005A9020 (:942)
void vec_surface_normal(vector& dest, const vector& p0, const vector& p1, const vector& p2)
{
    vec_perp(dest, p0, p1, p2);
    dest.normalize();
}

// 0x00582400 (:956)
void pltind_vec_perp(vector& dest, const vector& p0, const vector& p1, const vector& p2)
{
    vec_perp(dest, p0, p1, p2);
}

// 0x00582530 (:973)
// Angle from v0 to v1, negative when (v0 x v1) points against fvec.
float vec_delta_ang(const vector& v0, const vector& v1, const vector& fvec)
{
    const vector a = v0.copy_normalize();
    const vector b = v1.copy_normalize();
    const vector f = fvec.copy_normalize();
    const float d = b.y * a.y + b.x * a.x + b.z * a.z;
    if (1.0f <= d)
    {
        return 0.0f;
    }
    if (d <= -1.0f)
    {
        return 3.1415927f;
    }
    float angle = acosf(d);
    if ((v0.z * v1.x - v1.z * v0.x) * f.y + (v1.z * v0.y - v1.y * v0.z) * f.x + (v1.y * v0.x - v0.y * v1.x) * f.z < 0.0f)
    {
        angle = -angle;
    }
    return angle;
}

// 0x00582880 (:1007)
// As vec_delta_ang for the xz projections, around UP_VECTOR.
float vec_delta_ang_flattened(const vector& v0, const vector& v1)
{
    const vector a = vector(v0.x, 0.0f, v0.z).copy_normalize_safe();
    const vector b = vector(v1.x, 0.0f, v1.z).copy_normalize_safe();
    return vec_delta_ang(a, b, UP_VECTOR);
}

// 0x00582D00 (:1025)
float vec_dot_to_point(const vector& dir, const vector& p1, const vector& p2)
{
    const vector d = vector(p2.x - p1.x, p2.y - p1.y, p2.z - p1.z).copy_normalize();
    return dir * d;
}

// 0x00582E00 (:1038)
void vec_project_point_onto_plane(vector& new_point, const vector& point, const vector& plane_normal, const vector& plane_point)
{
    const float d = -(-(plane_point * plane_normal) + point * plane_normal);
    new_point = point;
    new_point.x += plane_normal.x * d;
    new_point.y += d * plane_normal.y;
    new_point.z += plane_normal.z * d;
}

// 0x005A91E0 (:1062)
void vec_rot_point_around_line(vector& out, const vector& in, float angle, const vector& line_point, const vector& line_dir)
{
    matrix turn;
    turn.set(0.0f, angle, 0.0f);
    matrix frame;
    frame.make(line_dir);
    vector local = vector(in.x - line_point.x, in.y - line_point.y, in.z - line_point.z).copy_rotate(frame);
    local.unrotate(turn);
    const vector world = local.copy_unrotate(frame);
    out.x = line_point.x + world.x;
    out.y = line_point.y + world.y;
    out.z = line_point.z + world.z;
}

// 0x005BBCB0 (:1086)
void vec_interp_const(vector& out, const vector& v0, const vector& v1, float t)
{
    vec_interp_const_norm(out, v0.copy_normalize_safe(), v1.copy_normalize_safe(), t);
}

// 0x005A9540 (:1105)
// Rotates v0 towards v1 by t of the angle between them (parallel vectors give v0).
void vec_interp_const_norm(vector& out, const vector& v0, const vector& v1, float t)
{
    vector axis((float)((double)v1.z * v0.y - (double)v1.y * v0.z), (float)((double)v1.x * v0.z - (double)v0.x * v1.z), (float)((double)v0.x * v1.y - (double)v1.x * v0.y));
    if (axis.is_null())
    {
        out = v0;
        return;
    }
    axis.normalize();
    // Clamped before the acos, which the decompile of __CIacos does not show (0x005A972C). Nearly parallel unit vectors
    // often give a dot just above 1, and a NaN here reaches formation_dir and hangs the nav cell walks. An unordered dot
    // becomes -1 as in rfg.exe.
    float cos_angle = (float)((double)v1.y * v0.y + (double)v0.x * v1.x + (double)v0.z * v1.z);
    if (!(cos_angle >= -1.0f))
    {
        cos_angle = -1.0f;
    }
    if (cos_angle >= 1.0f)
    {
        cos_angle = 1.0f;
    }
    vec_rot_point_around_line(out, v0, (float)acos((double)cos_angle) * t, ZERO_VECTOR, axis);
}

// 0x005A97B0 (:1232)
// Area-weighted centroid of a convex polygon (a fan from src[0]); returns the area.
float vec_get_centroid(vector& centroid, unsigned int nv, const vector* src)
{
    centroid.zero();
    float area = 0.0f;
    for (unsigned int i = 1; i + 1 < nv; i++)
    {
        vector n;
        vec_perp(n, src[0], src[i], src[i + 1]);
        area += n.mag() * 0.5f;
    }
    for (unsigned int i = 1; i + 1 < nv; i++)
    {
        vector n;
        vec_perp(n, src[0], src[i], src[i + 1]);
        const float w = (n.mag() * 0.5f) / area;
        centroid.x += w * (src[i + 1].x + src[i].x + src[0].x + 0.0f);
        centroid.y += (src[i + 1].y + src[i].y + src[0].y + 0.0f) * w;
        centroid.z += (src[i + 1].z + src[0].z + 0.0f + src[i].z) * w;
    }
    centroid.x *= 0.33333334f;
    centroid.y *= 0.33333334f;
    centroid.z *= 0.33333334f;
    return area;
}

// 0x005839B0 (:1293)
// Parameter of p along start..end, measured on the axis where the segment is longest.
float vec_calc_parametric_pos(const vector& p, const vector& start, const vector& end)
{
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float dz = end.z - start.z;
    if (fabsf(dx) <= fabsf(dy))
    {
        if (fabsf(dz) <= fabsf(dy))
        {
            return (p.y - start.y) / dy;
        }
    }
    else if (fabsf(dz) < fabsf(dx))
    {
        return (p.x - start.x) / dx;
    }
    return (p.z - start.z) / dz;
}

// 0x00583B10 (:1340)
void vec_change_to_local_coords(vector& v, const vector& coord_system_pos, const matrix& coord_system_orient)
{
    v -= coord_system_pos;
    v.rotate(coord_system_orient);
}

// 0x00583C50 (:1352)
void vec_change_to_global_coords(vector& v, const vector& coord_system_pos, const matrix& coord_system_orient)
{
    v.unrotate(coord_system_orient);
    v += coord_system_pos;
}

// 0x00583D80 (:1366)
float vec_find_closest_dist_squared_to_line(const vector& p, const vector& seg1, const vector& seg2, vector* closest_pt)
{
    const vector d(p.x - seg1.x, p.y - seg1.y, p.z - seg1.z);
    const vector line(seg2.x - seg1.x, seg2.y - seg1.y, seg2.z - seg1.z);
    const float len_sq = line.y * line.y + line.x * line.x + line.z * line.z;
    const float proj = line.x * d.x + line.y * d.y + line.z * d.z;
    if (closest_pt != nullptr)
    {
        closest_pt->x = seg1.x + (line.x * proj) / len_sq;
        closest_pt->y = (line.y * proj) / len_sq + seg1.y;
        closest_pt->z = seg1.z + (line.z * proj) / len_sq;
    }
    return (d.y * d.y + d.x * d.x + d.z * d.z) - (proj * proj) / len_sq;
}

// 0x00583FC0 (:1407)
float vec_find_closest_dist_squared_to_linesegment(const vector& p, const vector& seg1, const vector& seg2, vector* closest_pt)
{
    const vector line(seg2.x - seg1.x, seg2.y - seg1.y, seg2.z - seg1.z);
    const float len_sq = line.y * line.y + line.x * line.x + line.z * line.z;
    if (len_sq < 1e-10f)
    {
        if (closest_pt != nullptr)
        {
            *closest_pt = seg1;
        }
        return p.dist_squared(seg1);
    }
    float t = (line.x * (p.x - seg1.x) + line.y * (p.y - seg1.y) + line.z * (p.z - seg1.z)) / len_sq;
    if (t - 0.0f < 0.0f)
    {
        t = 0.0f;
    }
    if (0.0f <= t - 1.0f)
    {
        t = 1.0f;
    }
    const vector c(seg1.x + line.x * t, line.y * t + seg1.y, seg1.z + line.z * t);
    if (closest_pt != nullptr)
    {
        *closest_pt = c;
    }
    return c.dist_squared(p);
}

// 0x005842E0 (:1455)
float vec_find_closest_dist_squared_to_linesegment(float& closest_param_pos, const vector& p, const vector& seg1, const vector& seg2)
{
    const vector line(seg2.x - seg1.x, seg2.y - seg1.y, seg2.z - seg1.z);
    const float len_sq = line.x * line.x + line.y * line.y + line.z * line.z;
    if (len_sq < 1e-10f)
    {
        closest_param_pos = 0.5f;
        return p.dist_squared(seg1);
    }
    float t = (line.y * (p.y - seg1.y) + line.x * (p.x - seg1.x) + line.z * (p.z - seg1.z)) / len_sq;
    closest_param_pos = t;
    if (t - 0.0f < 0.0f)
    {
        t = 0.0f;
    }
    if (0.0f <= t - 1.0f)
    {
        t = 1.0f;
    }
    closest_param_pos = t;
    const vector c(seg1.x + t * line.x, t * line.y + seg1.y, seg1.z + t * line.z);
    return c.dist_squared(p);
}

// 0x005851D0 (:1671)
// Componentwise equality within epsilon relative to the larger magnitude (at least 1).
bool vec_equal_robust(const vector& a, const vector& b, float epsilon)
{
    const float pa[3] = {a.x, a.y, a.z};
    const float pb[3] = {b.x, b.y, b.z};
    for (int i = 0; i < 3; i++)
    {
        float scale = fabsf(pa[i]) < fabsf(pb[i]) ? fabsf(pb[i]) : fabsf(pa[i]);
        if (0.0f <= 1.0f - scale)
        {
            scale = 1.0f;
        }
        if (!(pb[i] - scale * epsilon <= pa[i] && pa[i] <= scale * epsilon + pb[i]))
        {
            return false;
        }
    }
    return true;
}

// 0x00581940 (:698)
// A uniform direction on the unit sphere: z uniform in [-1, 1], the angle around z uniform.
void vector::rand_sphere()
{
    z = Vlib_rand_instance.fl_rand() * 2.0f - 1.0f; // inlined
    const float angle = Vlib_rand_instance.fl_rand() * 6.2831855f; // inlined; 2 pi
    const float radius = (float)sqrt(1.0 - (double)z * (double)z);
    x = cosf(angle) * radius;
    y = sinf(angle) * radius;
}
