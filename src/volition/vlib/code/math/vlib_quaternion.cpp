// Original: volition/vlib/code/math/vlib_quaternion.cpp
//
// Partial: the quaternion constructors, set/extract to and from matrices (extract to matrix43 too), products, normalize, inverse, identity,
// dot product, lerp and slerp. short_quat, ln/exp and squad are not implemented yet.

#include "volition/vlib/code/math/quaternion.h"

#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"

#include <math.h>

quaternion IDENTITY_QUATERNION(0.0f, 0.0f, 0.0f, 1.0f); // 0x0163DC88

// 0x005639C0 (.h:145)
quaternion::quaternion()
{
}

// 0x005639F0 (.h:157)
quaternion::quaternion(const short_quat& q)
{
    // Magic number: 1 / 16383, the fixed point scale of a short_quat.
    x = (float)q.x * 6.103888e-05f;
    y = (float)q.y * 6.103888e-05f;
    z = (float)q.z * 6.103888e-05f;
    w = (float)q.w * 6.103888e-05f;
}

// 0x005639D0 (.h:148)
quaternion::quaternion(const quaternion& q)
    : x(q.x)
    , y(q.y)
    , z(q.z)
    , w(q.w)
{
}

// 0x0057E970 (:226)
void quaternion_init()
{
}

// 0x0057E980 (:237)
quaternion::quaternion(float _x, float _y, float _z, float _w)
    : x(_x)
    , y(_y)
    , z(_z)
    , w(_w)
{
}

// 0x0057E9B0 (:252)
// A rotation of a radians around the unit axis fvec.
quaternion::quaternion(const vector& fvec, float a)
{
    const float half = a * 0.5f;
    const float s = sinf(half);
    x = s * fvec.x;
    y = fvec.y * s;
    z = s * fvec.z;
    w = cosf(half);
}

// 0x005BB810 (:285)
quaternion::quaternion(const matrix& m)
{
    set(m);
}

// 0x005A7F00 (:295)
// The largest diagonal term picks the stable formula when the trace is small; the result is renormalized.
void quaternion::set(const matrix& m)
{
    // double chains rounded once per value; the square roots of float-rounded sums (as the binary's SQRTSD)
    const double rx = m.rvec.x;
    const double uy = m.uvec.y;
    const double fz = m.fvec.z;
    const float trace = (float)(rx + 1.0 + uy + fz);
    if (trace < 0.25f)
    {
        int largest = rx < uy ? 1 : 0;
        if ((rx < uy ? uy : rx) < fz)
        {
            largest = 2;
        }
        float s;
        float scale;
        if (largest == 0)
        {
            s = sqrtf((float)(rx - (fz + uy) + 1.0));
            scale = (float)(0.5 / (double)s);
            x = (float)((double)s * 0.5);
            y = (float)(((double)m.uvec.x + (double)m.rvec.y) * (double)scale);
            z = (float)(((double)m.fvec.x + (double)m.rvec.z) * (double)scale);
            w = (float)(((double)m.fvec.y - (double)m.uvec.z) * (double)scale);
        }
        else if (largest == 1)
        {
            s = sqrtf((float)(uy - (fz + rx) + 1.0));
            scale = (float)(0.5 / (double)s);
            y = (float)((double)s * 0.5);
            z = (float)(((double)m.fvec.y + (double)m.uvec.z) * (double)scale);
            x = (float)(((double)m.uvec.x + (double)m.rvec.y) * (double)scale);
            w = (float)(((double)m.rvec.z - (double)m.fvec.x) * (double)scale);
        }
        else
        {
            s = sqrtf((float)(fz - (uy + rx) + 1.0));
            scale = (float)(0.5 / (double)s);
            z = (float)((double)s * 0.5);
            x = (float)(((double)m.fvec.x + (double)m.rvec.z) * (double)scale);
            y = (float)(((double)m.fvec.y + (double)m.uvec.z) * (double)scale);
            w = (float)(((double)m.uvec.x - (double)m.rvec.y) * (double)scale);
        }
    }
    else
    {
        const float s = sqrtf(trace);
        const float scale = (float)(0.5 / (double)s);
        w = (float)((double)s * 0.5);
        x = (float)(((double)m.fvec.y - (double)m.uvec.z) * (double)scale);
        y = (float)(((double)m.rvec.z - (double)m.fvec.x) * (double)scale);
        z = (float)(((double)m.uvec.x - (double)m.rvec.y) * (double)scale);
    }
    normalize();
}

// 0x0057EA20 (:366)
// The cross product terms and the dot product each one double expression rounded to float; the scaled terms are float
// products, summed in pairs.
quaternion quaternion::operator*(const quaternion& q1) const
{
    const float cx = (float)((double)q1.z * (double)y - (double)q1.y * (double)z);
    const float cy = (float)((double)q1.x * (double)z - (double)q1.z * (double)x);
    const float cz = (float)((double)q1.y * (double)x - (double)q1.x * (double)y);
    const float sx = w * q1.x + q1.w * x;
    const float sy = w * q1.y + q1.w * y;
    const float sz = w * q1.z + q1.w * z;
    const float dot = (float)((double)q1.y * (double)y + (double)q1.x * (double)x + (double)q1.z * (double)z);
    quaternion result;
    result.x = sx + cx;
    result.y = sy + cy;
    result.z = sz + cz;
    result.w = (float)((double)w * (double)q1.w - (double)dot);
    return result;
}

// 0x0057EC80 (:401)
quaternion quaternion::operator*(float scalar) const
{
    return quaternion(x * scalar, y * scalar, z * scalar, w * scalar);
}

// The rotation matrix of a unit quaternion; each element one double expression rounded once (as both overloads
// compute it in rfg.exe).
static void quaternion_rotation(const quaternion& q, matrix& m)
{
    const double x = q.x;
    const double y = q.y;
    const double z = q.z;
    const double w = q.w;
    m.rvec.x = (float)(1.0 - y * 2.0 * y - z * 2.0 * z);
    m.rvec.y = (float)(x * 2.0 * y - z * 2.0 * w);
    m.rvec.z = (float)(y * 2.0 * w + z * 2.0 * x);
    m.uvec.x = (float)(z * 2.0 * w + x * 2.0 * y);
    m.uvec.y = (float)(1.0 - x * 2.0 * x - z * 2.0 * z);
    m.uvec.z = (float)(z * 2.0 * y - x * 2.0 * w);
    m.fvec.x = (float)(x * 2.0 * z - y * 2.0 * w);
    m.fvec.y = (float)(z * 2.0 * y + x * 2.0 * w);
    m.fvec.z = (float)(1.0 - x * 2.0 * x - y * 2.0 * y);
}

// 0x0057ECF0 (:423)
void quaternion::extract_matrix(matrix& dest_matrix) const
{
    quaternion_rotation(*this, dest_matrix);
}

// 0x005A8310 (:410)
// The rotation, with no translation.
void quaternion::extract_matrix(matrix43& dest_matrix) const
{
    quaternion_rotation(*this, dest_matrix.m_rotation);
    dest_matrix.m_translation.x = 0.0f;
    dest_matrix.m_translation.y = 0.0f;
    dest_matrix.m_translation.z = 0.0f;
}

// 0x0057EF20 (:444)
quaternion quaternion::copy_normalize() const
{
    const float scale = sqrtf(1.0f / (x * x + y * y + z * z + w * w));
    return quaternion(x * scale, scale * y, z * scale, scale * w);
}

// 0x0057EFF0 (:455)
quaternion quaternion::copy_inverse() const
{
    return quaternion(-x, -y, -z, w);
}

// 0x0057F1E0 (:498)
// The squared length summed at double precision (x, y, z, w); the inverse rounded to float before the square root.
void quaternion::normalize()
{
    const float inv = (float)(1.0 / ((double)x * (double)x + (double)y * (double)y + (double)z * (double)z + (double)w * (double)w));
    const float scale = sqrtf(inv);
    x *= scale;
    y *= scale;
    z *= scale;
    w *= scale;
}

// 0x0057F290 (:511)
void quaternion::inverse()
{
    x = -x;
    y = -y;
    z = -z;
}

// 0x0057F4B0 (:562)
void quaternion::make_identity()
{
    x = 0.0f;
    y = 0.0f;
    z = 0.0f;
    w = 1.0f;
}

// 0x0057F540 (:596)
// x87 code: the sum of the products (y, x, z, w) at double precision, rounded to float once.
float quaternion_dot(const quaternion& q1, const quaternion& q2)
{
    return (float)((double)q1.y * (double)q2.y + (double)q1.x * (double)q2.x + (double)q1.z * (double)q2.z + (double)q1.w * (double)q2.w);
}

// 0x005A8550 (:613)
void quat_lerp(quaternion& result, const quaternion& a, const quaternion& b, float t)
{
    result.x = a.x + (b.x - a.x) * t;
    result.y = a.y + (b.y - a.y) * t;
    result.z = a.z + (b.z - a.z) * t;
    result.w = a.w + (b.w - a.w) * t;
    result.normalize();
}

// 0x005A8750 (:693)
// Takes the short way round (q2 negated when the dot product is negative); nearly equal rotations are lerped.
void pltind_slerp(quaternion& result, const quaternion& q1, const quaternion& q2, float t)
{
    // the dot product at double precision, rounded once
    float cosom = (float)((double)q2.x * (double)q1.x + (double)q2.y * (double)q1.y + (double)q1.z * (double)q2.z + (double)q1.w * (double)q2.w);
    quaternion to = q2;
    if (cosom < 0.0f)
    {
        to = quaternion(-q2.x, -q2.y, -q2.z, -q2.w);
        cosom = -cosom;
    }
    float scale0;
    float scale1;
    if ((double)1e-07f < 1.0 - (double)cosom) // the float constant 1e-07 compared in double
    {
        // x87 code: the CRT's double acos and sin, every intermediate stored as float
        const float omega = (float)acos((double)cosom);
        const float sinom = (float)sin((double)omega);
        const float inv_sinom = (float)(1.0 / (double)sinom);
        const float a0 = (float)((1.0 - (double)t) * (double)omega);
        scale0 = (float)((double)(float)sin((double)a0) * (double)inv_sinom);
        const float a1 = (float)((double)omega * (double)t);
        scale1 = (float)((double)(float)sin((double)a1) * (double)inv_sinom);
    }
    else
    {
        scale0 = (float)(1.0 - (double)t);
        scale1 = t;
    }
    result.x = (float)((double)q1.x * (double)scale0 + (double)to.x * (double)scale1);
    result.y = (float)((double)to.y * (double)scale1 + (double)scale0 * (double)q1.y);
    result.z = (float)((double)q1.z * (double)scale0 + (double)to.z * (double)scale1);
    result.w = (float)((double)q1.w * (double)scale0 + (double)to.w * (double)scale1);
}
