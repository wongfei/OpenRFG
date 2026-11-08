// Original: volition/vlib/code/math/matrix.cpp
//
// Rotation matrices (decompiled). The rows rvec, uvec and fvec are the frame's right, up and forward axes. a * b
// (operator*) unrotates each row of a by b; rotate/unrotate apply vector::rotate/unrotate to every row.
//
// Partial: matrix (constructors, setters, products, transposes and inverses, extract_angles/heading/pitch,
// orthogonalize, make from directions, rotate/unrotate, make_identity, make_ry, is_valid) and the matrix43 default,
// row, rotation and position/orientation constructors and transpose. The rest of matrix (fix, extract_axis_and_angle, the other local axis
// rotations, rand_quick), most of matrix43 and matrix34/23 are not implemented yet. matrix44: constructors, element,
// transposes, operator* and set.

#include "volition/vlib/code/math/matrix.h"

#include "volition/vlib/code/math/vector2.h"

#include "keen/base/trace.hpp"

#include <math.h>

extern const matrix IDENTITY_MATRIX(vector(1.0f, 0.0f, 0.0f), vector(0.0f, 1.0f, 0.0f), vector(0.0f, 0.0f, 1.0f)); // 0x0163B220
extern const matrix43 IDENTITY_MATRIX43(vector(1.0f, 0.0f, 0.0f), vector(0.0f, 1.0f, 0.0f), vector(0.0f, 0.0f, 1.0f), vector(0.0f, 0.0f, 0.0f)); // 0x0163B244
extern const matrix44 IDENTITY_MATRIX44(vector4(1.0f, 0.0f, 0.0f, 0.0f), vector4(0.0f, 1.0f, 0.0f, 0.0f), vector4(0.0f, 0.0f, 1.0f, 0.0f), vector4(0.0f, 0.0f, 0.0f, 1.0f)); // 0x0163B278

// 0x00576FD0 (:1203)
vector matrix43::get_translation() const
{
    return m_translation;
}

// 0x00576FF0 (:1237)
void matrix43::get_translation(vector& v) const
{
    v = m_translation;
}

// 0x00577010 (:1248)
void matrix43::get_orientation(matrix& m) const
{
    m = m_rotation;
}

// 0x00576FB0 (:1188)
void matrix43::set_translation(const vector& p)
{
    m_translation = p;
}

// 0x00577050 (:1259)
void matrix43::set_orientation(const matrix& m)
{
    m_rotation = m;
}

// 0x00577090 (:1284)
// The 12 floats as a 4x3 matrix (rows rvec, uvec, fvec, translation) rewritten in place as its 3x4 transpose (the
// row-major GraphicsMatrix43 layout of the shader constants).
void matrix43::transpose()
{
    float* elements = &m_rotation.rvec.x;
    float source[12];
    for (int i = 0; i < 12; i++)
    {
        source[i] = elements[i];
    }
    for (int row = 0; row < 3; row++)
    {
        for (int column = 0; column < 4; column++)
        {
            elements[row * 4 + column] = source[column * 3 + row];
        }
    }
}

// 0x00482E40 (.h:53)
matrix::matrix()
{
}

// 0x005755F0 (:95)
matrix::matrix(const matrix& m)
    : rvec(m.rvec)
    , uvec(m.uvec)
    , fvec(m.fvec)
{
}

// 0x00575630 (:106)
matrix::matrix(const vector& _rvec, const vector& _uvec, const vector& _fvec)
    : rvec(_rvec)
    , uvec(_uvec)
    , fvec(_fvec)
{
}

// 0x00575680 (:139)
void matrix::set(float sinp, float cosp, float sinb, float cosb, float sinh, float cosh)
{
    // Pitch p, bank b, heading h.
    rvec.x = sinb * sinh * sinp + cosb * cosh;
    uvec.z = cosb * cosh * sinp + sinb * sinh;
    uvec.x = cosb * sinh * sinp - sinb * cosh;
    rvec.z = sinb * cosh * sinp - cosb * sinh;
    fvec.x = cosp * sinh;
    rvec.y = cosp * sinb;
    uvec.y = cosp * cosb;
    fvec.z = cosp * cosh;
    fvec.y = -sinp;
}

// 0x005757E0 (:170)
// The CRT's double sin and cos (x87 __CIsin / __CIcos) each rounded to float, then the products of the rotation (the
// pairs rounded to float, each element one double expression).
void matrix::set(float p, float b, float h)
{
    const float sp = (float)sin((double)p);
    const float sb = (float)sin((double)b);
    const float sh = (float)sin((double)h);
    const float cp = (float)cos((double)p);
    const float cb = (float)cos((double)b);
    const float ch = (float)cos((double)h);
    const float sh_sb = sh * sb;
    const float cb_ch = cb * ch;
    const float cb_sh = cb * sh;
    const float ch_sb = ch * sb;
    rvec.x = (float)((double)sh_sb * (double)sp + (double)cb_ch);
    rvec.y = cp * sb;
    rvec.z = (float)((double)ch_sb * (double)sp - (double)cb_sh);
    uvec.x = (float)((double)cb_sh * (double)sp - (double)ch_sb);
    uvec.y = cp * cb;
    uvec.z = (float)((double)cb_ch * (double)sp + (double)sh_sb);
    fvec.x = cp * sh;
    fvec.y = -sp;
    fvec.z = cp * ch;
}

// 0x00576850 (:802)
void matrix::make_identity()
{
    rvec.x = 1.0f;
    rvec.y = 0.0f;
    rvec.z = 0.0f;
    uvec.x = 0.0f;
    uvec.y = 1.0f;
    uvec.z = 0.0f;
    fvec.x = 0.0f;
    fvec.y = 0.0f;
    fvec.z = 1.0f;
}

// 0x005A4680 (:1210)
void matrix43::make_identity()
{
    m_rotation = IDENTITY_MATRIX;
    m_translation = ZERO_VECTOR;
}

// 0x005A0F00 (:58)
matrix::matrix(float sinp, float cosp, float sinb, float cosb, float sinh, float cosh)
{
    set(sinp, cosp, sinb, cosb, sinh, cosh);
}

// 0x005A1060 (:71)
matrix::matrix(float p, float b, float h)
{
    set(p, b, h);
}

// 0x005A10A0 (:84)
matrix::matrix(const vector& axis, float angle)
{
    set(axis, angle);
}

// 0x005759C0 (:189)
// Rotation of angle radians around the unit axis (built from the quaternion (axis * sin(angle / 2), cos(angle / 2))).
void matrix::set(const vector& axis, float angle)
{
    const float half = angle * 0.5f;
    const float c = cosf(half);
    const float s = sinf(half);
    const float x = axis.x * s;
    const float y = axis.y * s;
    const float z = axis.z * s;
    const float yy2 = y * y * 2.0f;
    const float zz2 = z * z * 2.0f;
    const float xy2 = y * x * 2.0f;
    const float zc2 = z * c * 2.0f;
    rvec.x = (1.0f - yy2) - zz2;
    uvec.x = xy2 - zc2;
    const float xx = 1.0f - x * x * 2.0f;
    uvec.y = xx - zz2;
    rvec.y = xy2 + zc2;
    const float yc2 = y * c * 2.0f;
    const float zy2 = z * y * 2.0f;
    const float zx2 = z * x * 2.0f;
    const float xc2 = x * c * 2.0f;
    uvec.z = zy2 + xc2;
    fvec.x = yc2 + zx2;
    fvec.y = zy2 - xc2;
    rvec.z = zx2 - yc2;
    fvec.z = xx - yy2;
}

// 0x00575C00 (:234). Each row of this through m: every element one double sum of the three products (rvec, uvec,
// fvec order), rounded once.
static vector matrix_row_times(const vector& row, const matrix& m)
{
    return vector((float)((double)row.x * (double)m.rvec.x + (double)row.y * (double)m.uvec.x + (double)row.z * (double)m.fvec.x),
                  (float)((double)row.x * (double)m.rvec.y + (double)row.y * (double)m.uvec.y + (double)row.z * (double)m.fvec.y),
                  (float)((double)row.x * (double)m.rvec.z + (double)row.y * (double)m.uvec.z + (double)row.z * (double)m.fvec.z));
}

matrix matrix::operator*(const matrix& m) const
{
    return matrix(matrix_row_times(rvec, m), matrix_row_times(uvec, m), matrix_row_times(fvec, m));
}

// 0x00575EF0 (:260)
// Pitch from fvec.y; heading from fvec (from rvec at the poles, where bank is 0); bank from rvec.y and uvec.y.
void matrix::extract_angles(float& p, float& b, float& h) const
{
    float fy = fvec.y;
    if (fy - -1.0f < 0.0f)
    {
        fy = -1.0f;
    }
    if (0.0f <= fy - 1.0f)
    {
        fy = 1.0f;
    }
    // x87: the CRT's double functions (__CIasin, __CIatan2, __CIcos, __CIsin), each result rounded to float.
    p = -(float)asin((double)fy);
    const float eps = 1.1920929e-07f;
    if ((eps <= fy - 1.0f || eps <= -(fy - 1.0f)) && (eps <= fy - -1.0f || eps <= -(fy - -1.0f)))
    {
        h = fvec.x == 0.0f && fvec.z == 0.0f ? 0.0f : (float)atan2((double)fvec.x, (double)fvec.z);
        const float ch = (float)cos((double)h);
        const float sh = (float)sin((double)h);
        float cp;
        if (fabsf(sh) <= fabsf(ch))
        {
            cp = fvec.z * ch;
        }
        else
        {
            cp = fvec.x * sh;
        }
        if (cp - 0.0f < eps && -(cp - 0.0f) < eps)
        {
            b = 0.0f;
            return;
        }
        b = rvec.y / cp != 0.0f || uvec.y / cp != 0.0f ? (float)atan2((double)(rvec.y / cp), (double)(uvec.y / cp)) : 0.0f;
        return;
    }
    b = 0.0f;
    h = -rvec.z != 0.0f || rvec.x != 0.0f ? (float)atan2((double)-rvec.z, (double)rvec.x) : 0.0f;
}

// 0x00576190 (:297)
float matrix::extract_heading() const
{
    float fy = fvec.y;
    if (fy - -1.0f < 0.0f)
    {
        fy = -1.0f;
    }
    if (0.0f <= fy - 1.0f)
    {
        fy = 1.0f;
    }
    const float eps = 1.1920929e-07f;
    if ((fy - 1.0f < eps && -(fy - 1.0f) < eps) || (fy - -1.0f < eps && -(fy - -1.0f) < eps))
    {
        if (-rvec.z != 0.0f || rvec.x != 0.0f)
        {
            return (float)atan2((double)-rvec.z, (double)rvec.x); // x87 __CIatan2 on doubles
        }
    }
    else if (fvec.x != 0.0f || fvec.z != 0.0f)
    {
        return (float)atan2((double)fvec.x, (double)fvec.z);
    }
    return 0.0f;
}

// 0x005762C0 (:315)
float matrix::extract_pitch() const
{
    return -asinf(fvec.y);
}

// 0x005762E0 (:326)
void matrix::extract_heading(float& h) const
{
    h = extract_heading();
}

// 0x005762F0 (:336)
void matrix::extract_pitch(float& p) const
{
    p = extract_pitch();
}

// 0x005A10C0 (:348)
// The rotation's axis and angle from the trace (cos theta = (trace - 1) / 2). Near 0 the axis is +x and the angle 0;
// near pi the axis comes from the largest diagonal element. Each step in double, rounded to float where the asm stores.
void matrix::extract_axis_and_angle(vector& rot_axis, float& theta) const
{
    const float trace = (float)((double)rvec.x + (double)uvec.y + (double)fvec.z);
    const float cos_theta = (float)(((double)trace - 1.0) * 0.5);
    if ((double)cos_theta > (double)0.99999976f) // Magic number: float 0x3F7FFFFC
    {
        theta = 0.0f;
        rot_axis.x = 1.0f;
        rot_axis.y = 0.0f;
        rot_axis.z = 0.0f;
        return;
    }
    if ((double)cos_theta > (double)-0.99999976f)
    {
        theta = (float)acos((double)cos_theta);
        rot_axis.x = (float)((double)uvec.z - (double)fvec.y);
        rot_axis.y = (float)((double)fvec.x - (double)rvec.z);
        rot_axis.z = (float)((double)rvec.y - (double)uvec.x);
        const float len2 = (float)((double)rot_axis.x * (double)rot_axis.x + (double)rot_axis.y * (double)rot_axis.y + (double)rot_axis.z * (double)rot_axis.z);
        if (1e-8f > len2)
        {
            theta = 0.0f;
            rot_axis.x = 1.0f;
            rot_axis.y = 0.0f;
            rot_axis.z = 0.0f;
            return;
        }
        const float inv_len = (float)(1.0 / (double)(float)sqrt((double)len2));
        rot_axis.x = (float)((double)rot_axis.x * (double)inv_len);
        rot_axis.y = (float)((double)inv_len * (double)rot_axis.y);
        rot_axis.z = (float)((double)rot_axis.z * (double)inv_len);
        return;
    }
    theta = 3.1415927f;
    int largest = 0;
    float largest_value = rvec.x;
    if ((double)uvec.y > (double)rvec.x)
    {
        largest = 1;
        largest_value = uvec.y;
    }
    if ((double)fvec.z > (double)largest_value)
    {
        largest = 2;
    }
    switch (largest)
    {
    case 0:
        rot_axis.x = (float)sqrt((double)(float)((double)rvec.x + 1.0));
        rot_axis.y = (float)((double)rvec.y / (double)rot_axis.x);
        rot_axis.z = (float)((double)rvec.z / (double)rot_axis.x);
        break;
    case 1:
        rot_axis.y = (float)sqrt((double)(float)((double)uvec.y + 1.0));
        rot_axis.x = (float)((double)uvec.x / (double)rot_axis.y);
        rot_axis.z = (float)((double)uvec.z / (double)rot_axis.y);
        break;
    case 2:
        rot_axis.z = (float)sqrt((double)(float)((double)fvec.z + 1.0));
        rot_axis.x = (float)((double)fvec.x / (double)rot_axis.z);
        rot_axis.y = (float)((double)fvec.y / (double)rot_axis.z);
        break;
    default:
        keen::debug::nativePrintArgs("invalid index - this should never happen");
        break;
    }
    const float len = (float)sqrt((double)(float)((double)rot_axis.x * (double)rot_axis.x + (double)rot_axis.y * (double)rot_axis.y + (double)rot_axis.z * (double)rot_axis.z));
    const float inv_len = (float)(1.0 / (double)len);
    rot_axis.x = (float)((double)rot_axis.x * (double)inv_len);
    rot_axis.y = (float)((double)inv_len * (double)rot_axis.y);
    rot_axis.z = (float)((double)rot_axis.z * (double)inv_len);
}

// 0x00576410 (:481)
float matrix::determinant() const
{
    return (((rvec.z * uvec.x * fvec.y + fvec.x * uvec.z * rvec.y + fvec.z * uvec.y * rvec.x) - fvec.x * rvec.z * uvec.y) - fvec.y * rvec.x * uvec.z) -
           uvec.x * rvec.y * fvec.z;
}

// 0x005764F0 (:604)
matrix matrix::copy_transpose() const
{
    matrix m(*this);
    m.transpose();
    return m;
}

// 0x005A24C0 (:625)
matrix matrix::copy_inverse() const
{
    matrix m(*this);
    m.inverse();
    return m;
}

// 0x005A2790 (:640)
// Normalizes fvec, keeps uvec when it is not zero (otherwise rebuilds it from rvec, or from world up), then makes
// rvec = uvec x fvec and uvec = fvec x rvec.
void matrix::orthogonalize()
{
    fvec.normalize();
    if (0.0f < uvec * uvec)
    {
        uvec.normalize();
    }
    else if (0.0f < rvec * rvec)
    {
        uvec = fvec ^ rvec;
    }
    else if (fvec.x != 0.0f || fvec.z != 0.0f || fvec.y == 0.0f)
    {
        uvec.set(0.0f, 1.0f, 0.0f);
    }
    else
    {
        uvec.set(0.0f, 0.0f, 1.0f);
    }
    rvec = uvec ^ fvec;
    uvec = fvec ^ rvec;
}

// 0x005A2B50 (:683)
// orthogonalize for degenerate rows: a zero fvec is rebuilt as uvec x rvec (or world up/forward), uvec is kept only
// when not parallel to fvec (else rebuilt from fvec x rvec or world up/back), then rvec = uvec x fvec and uvec = fvec x
// rvec, normalized. The lengths and rvec are those on entry; the parallel tests allow 0.0001 per component.
void matrix::fix()
{
    const float uu = uvec.x * uvec.x + uvec.y * uvec.y + uvec.z * uvec.z;
    const float r_x = rvec.x;
    const float r_y = rvec.y;
    const float r_z = rvec.z;
    const float rr = r_x * r_x + r_y * r_y + r_z * r_z;
    const float eps = 0.0001f; // Magic number: the parallel tolerance
    if (0.0f < fvec.x * fvec.x + fvec.y * fvec.y + fvec.z * fvec.z)
    {
        const float inv = 1.0f / sqrtf(fvec.x * fvec.x + fvec.y * fvec.y + fvec.z * fvec.z);
        fvec.x = fvec.x * inv;
        fvec.y = fvec.y * inv;
        fvec.z = fvec.z * inv;
    }
    else if (0.0f < uu)
    {
        bool rebuilt = false;
        if (0.0f < rr)
        {
            const float cx = uvec.y * r_z - uvec.z * r_y;
            const float cy = uvec.z * r_x - uvec.x * r_z;
            const float cz = uvec.x * r_y - uvec.y * r_x;
            if (eps <= fabsf(cx) || eps <= fabsf(cy) || eps <= fabsf(cz))
            {
                fvec.set(cx, cy, cz);
                const float inv = 1.0f / sqrtf(fvec.x * fvec.x + fvec.y * fvec.y + fvec.z * fvec.z);
                fvec.x = fvec.x * inv;
                fvec.y = fvec.y * inv;
                fvec.z = fvec.z * inv;
                rebuilt = true;
            }
        }
        if (!rebuilt)
        {
            if (uvec.x != 0.0f || uvec.y != 0.0f || uvec.z == 0.0f)
            {
                fvec.set(0.0f, 0.0f, 1.0f);
            }
            else
            {
                fvec.set(1.0f, 0.0f, 0.0f);
            }
        }
    }
    bool have_uvec = false;
    if (0.0f < uu)
    {
        if (eps <= fabsf(uvec.z * fvec.y - uvec.y * fvec.z) || eps <= fabsf(uvec.x * fvec.z - fvec.x * uvec.z) || eps <= fabsf(fvec.x * uvec.y - uvec.x * fvec.y))
        {
            const float inv = 1.0f / sqrtf(uvec.x * uvec.x + uvec.y * uvec.y + uvec.z * uvec.z);
            uvec.x = inv * uvec.x;
            uvec.y = inv * uvec.y;
            uvec.z = inv * uvec.z;
            have_uvec = true;
        }
    }
    if (!have_uvec && 0.0f < rr)
    {
        const float cx = fvec.y * r_z - fvec.z * r_y;
        const float cy = fvec.z * r_x - fvec.x * r_z;
        const float cz = fvec.x * r_y - fvec.y * r_x;
        if (eps <= fabsf(cx) || eps <= fabsf(cy) || eps <= fabsf(cz))
        {
            uvec.set(cx, cy, cz);
            const float inv = 1.0f / sqrtf(uvec.x * uvec.x + uvec.y * uvec.y + uvec.z * uvec.z);
            uvec.x = uvec.x * inv;
            uvec.y = uvec.y * inv;
            uvec.z = uvec.z * inv;
            have_uvec = true;
        }
    }
    if (!have_uvec)
    {
        if (fvec.x == 0.0f && fvec.y != 0.0f && fvec.z == 0.0f)
        {
            uvec.y = 0.0f;
            uvec.z = -1.0f;
        }
        else
        {
            uvec.y = 1.0f;
            uvec.z = 0.0f;
        }
        uvec.x = 0.0f;
    }
    rvec.x = uvec.y * fvec.z - uvec.z * fvec.y;
    rvec.y = fvec.x * uvec.z - uvec.x * fvec.z;
    rvec.z = uvec.x * fvec.y - fvec.x * uvec.y;
    const float inv = 1.0f / sqrtf(rvec.x * rvec.x + rvec.y * rvec.y + rvec.z * rvec.z);
    rvec.x = inv * rvec.x;
    rvec.y = rvec.y * inv;
    rvec.z = inv * rvec.z;
    uvec.x = rvec.z * fvec.y - rvec.y * fvec.z;
    uvec.y = rvec.x * fvec.z - fvec.x * rvec.z;
    uvec.z = fvec.x * rvec.y - rvec.x * fvec.y;
}

// 0x00576570 (:736)
void matrix::transpose()
{
    float t = uvec.x;
    uvec.x = rvec.y;
    rvec.y = t;
    t = fvec.x;
    fvec.x = rvec.z;
    rvec.z = t;
    t = fvec.y;
    fvec.y = uvec.z;
    uvec.z = t;
}

// 0x005765B0 (:758)
// Cofactor inverse; a singular matrix is left unchanged.
void matrix::inverse()
{
    const float det = determinant();
    if (det == 0.0f)
    {
        return;
    }
    float* f = &rvec.x;
    const float a0 = f[0];
    const float a1 = f[1];
    const float a2 = f[2];
    const float a3 = f[3];
    const float a4 = f[4];
    const float a5 = f[5];
    const float a6 = f[6];
    const float a7 = f[7];
    const float a8 = f[8];
    f[0] = (a4 * a8 - a5 * a7) / det;
    f[1] = -(a1 * a8 - a2 * a7) / det;
    f[2] = (a1 * a5 - a2 * a4) / det;
    f[3] = -(a3 * a8 - a6 * a5) / det;
    f[4] = (a0 * a8 - a6 * a2) / det;
    f[5] = -(a0 * a5 - a3 * a2) / det;
    f[6] = (a3 * a7 - a6 * a4) / det;
    f[7] = -(a0 * a7 - a6 * a1) / det;
    f[8] = (a0 * a4 - a3 * a1) / det;
}

// 0x005A34C0 (:813)
// A frame looking along fvec with no bank (rvec horizontal); straight up or down uses rvec = +x.
void matrix::make(const vector& _fvec)
{
    make_quick(_fvec.copy_normalize());
}

// 0x005A3760 (:844)
// make for a unit fvec.
void matrix::make_quick(const vector& _fvec)
{
    fvec = _fvec;
    if (fvec.x < 0.0001f && -0.0001f < fvec.x && fvec.z < 0.0001f && -0.0001f < fvec.z)
    {
        const float fy = fvec.y;
        fvec.z = 0.0f;
        fvec.x = 0.0f;
        rvec.x = 1.0f;
        if (0.0f < fy || fy == 0.0f)
        {
            fvec.y = 1.0f;
            uvec.z = -1.0f;
        }
        else
        {
            fvec.y = -1.0f;
            uvec.z = 1.0f;
        }
        uvec.y = 0.0f;
        uvec.x = 0.0f;
        rvec.z = 0.0f;
        rvec.y = 0.0f;
        return;
    }
    const float inv = 1.0f / sqrtf(fvec.z * fvec.z + 0.0f + fvec.x * fvec.x);
    rvec.x = fvec.z * inv;
    rvec.y = inv * 0.0f;
    rvec.z = -fvec.x * inv;
    uvec = fvec ^ rvec;
}

// 0x005A3950 (:884)
void matrix::make_uvec(const vector& _fvec, const vector& _uvec)
{
    fvec = _fvec.copy_normalize();
    const float ulen = _uvec.mag();
    uvec = _uvec * (1.0f / ulen);
    if (ulen != 0.0f)
    {
        rvec = uvec ^ fvec;
        const float rlen = rvec.mag();
        rvec = rvec * (1.0f / rlen);
        if (rlen != 0.0f)
        {
            uvec = fvec ^ rvec;
            return;
        }
    }
    make(_fvec);
}

// 0x005A3CA0 (:911)
void matrix::make_rvec(const vector& _fvec, const vector& _rvec)
{
    fvec = _fvec.copy_normalize();
    const float rlen = _rvec.mag();
    rvec = _rvec * (1.0f / rlen);
    if (rlen != 0.0f)
    {
        uvec = fvec ^ rvec;
        const float ulen = uvec.mag();
        uvec = uvec * (1.0f / ulen);
        if (ulen != 0.0f)
        {
            rvec = uvec ^ fvec;
            return;
        }
    }
    make(_fvec);
}

// 0x005A3FF0 (:940)
void matrix::make_ruvec(const vector& _rvec, const vector& _uvec)
{
    const float rlen = _rvec.mag();
    rvec = _rvec * (1.0f / rlen);
    if (rlen != 0.0f)
    {
        const float ulen = _uvec.mag();
        uvec = _uvec * (1.0f / ulen);
        if (ulen != 0.0f)
        {
            fvec = rvec ^ uvec;
            const float flen = fvec.mag();
            fvec = fvec * (1.0f / flen);
            if (flen != 0.0f)
            {
                uvec = fvec ^ rvec;
                return;
            }
        }
    }
    *this = IDENTITY_MATRIX;
}

// 0x00576890 (:967)
void matrix::rotate(const matrix& m)
{
    rvec.rotate(m);
    uvec.rotate(m);
    fvec.rotate(m);
}

// 0x00576B50 (:991)
void matrix::unrotate(const matrix& m)
{
    rvec.unrotate(m);
    uvec.unrotate(m);
    fvec.unrotate(m);
}

// 0x005A14B0 (:451)
// No element is an infinity or NaN.
bool matrix::is_valid() const
{
    return fvec.is_valid() && uvec.is_valid() && rvec.is_valid(); // inlined vector::is_valid
}

// 0x005A15C0 (:461)
// Each row of unit length and the forward row the cross of up and right, within 0.0001.
bool matrix::is_orthonormal() const
{
    const auto near_unit = [](const vector& v)
    {
        const float excess = (v.x * v.x + v.y * v.y + v.z * v.z) - 1.0f;
        return excess < 0.0001f && -excess < 0.0001f;
    };
    if (!near_unit(fvec) || !near_unit(uvec) || !near_unit(rvec))
    {
        return false;
    }
    vector cross;
    cross.x = uvec.z * rvec.y - uvec.y * rvec.z;
    cross.y = uvec.x * rvec.z - uvec.z * rvec.x;
    cross.z = uvec.y * rvec.x - uvec.x * rvec.y;
    return vec_equal_robust(cross, fvec, 0.0001f);
}

// 0x004B51D0 (.h:146)
vector operator*(const vector& v, const matrix& m)
{
    return v.copy_unrotate(m);
}

// 0x004B5AE0 (.h:488)
matrix43::matrix43()
{
}

// 0x004B8610 (compiler-generated)
matrix43::matrix43(const matrix43& __that)
    : m_rotation(__that.m_rotation)
    , m_translation(__that.m_translation)
{
}

// 0x00576E10 (:1072)
// A rotation about y (the up axis) by angle_rad.
void matrix::make_ry(float angle_rad)
{
    const float c = cosf(angle_rad);
    const float s = sinf(angle_rad);
    rvec.set(c, 0.0f, s);
    uvec.set(0.0f, 1.0f, 0.0f);
    fvec.set(-s, 0.0f, c);
}

// 0x00576E80 (:1094)
// A rotation about x (the right axis) by angle_rad.
void matrix::make_rx(float angle_rad)
{
    const float c = cosf(angle_rad);
    const float s = sinf(angle_rad);
    rvec.set(1.0f, 0.0f, 0.0f);
    uvec.set(0.0f, c, -s);
    fvec.set(0.0f, s, c);
}

// 0x00576EF0 (:1138)
matrix43::matrix43(const vector& _rvec, const vector& _uvec, const vector& _fvec, const vector& _pos)
{
    m_rotation.rvec = _rvec;
    m_rotation.uvec = _uvec;
    m_rotation.fvec = _fvec;
    m_translation = _pos;
}

// 0x005A44E0 (:1160): the inverse of a rotation and translation (the transposed rotation, the translation rotated
// back and negated).
matrix43 matrix43::invert_orthogonal() const
{
    matrix43 result;
    const float tx = -m_translation.x;
    const float ty = -m_translation.y;
    const float tz = -m_translation.z;
    result.m_rotation.rvec.x = m_rotation.rvec.x;
    result.m_rotation.rvec.y = m_rotation.uvec.x;
    result.m_rotation.rvec.z = m_rotation.fvec.x;
    result.m_rotation.uvec.x = m_rotation.rvec.y;
    result.m_rotation.uvec.y = m_rotation.uvec.y;
    result.m_rotation.uvec.z = m_rotation.fvec.y;
    result.m_rotation.fvec.x = m_rotation.rvec.z;
    result.m_rotation.fvec.y = m_rotation.uvec.z;
    result.m_rotation.fvec.z = m_rotation.fvec.z;
    // Each dot product in double, rounded once (rfg.exe's arithmetic; the hkdiff glass_vlib_math check).
    result.m_translation.x = (float)((double)m_rotation.rvec.y * ty + (double)m_rotation.rvec.x * tx + (double)m_rotation.rvec.z * tz);
    result.m_translation.y = (float)((double)m_rotation.uvec.y * ty + (double)m_rotation.uvec.x * tx + (double)m_rotation.uvec.z * tz);
    result.m_translation.z = (float)((double)m_rotation.fvec.y * ty + (double)m_rotation.fvec.x * tx + (double)m_rotation.fvec.z * tz);
    return result;
}

// 0x005A4490 (:1121)
// The rotation m with no translation.
matrix43::matrix43(const matrix& m)
{
    m_rotation = m;
    m_translation.x = 0.0f;
    m_translation.y = 0.0f;
    m_translation.z = 0.0f;
}

// 0x00576F50 (:1150)
matrix43::matrix43(const vector& _pos, const matrix& _orient)
{
    m_rotation = _orient;
    m_translation = _pos;
}

// 0x004B54F0 (.h:261)
// this = m1 then m2: the rotation is m1 * m2 and the translation is m1's, rotated by m2 and moved by m2's.
void matrix43::multiply_two(const matrix43& m1, const matrix43& m2)
{
    m_rotation = m1.m_rotation * m2.m_rotation;
    const vector t = m1.m_translation;
    // rvec * t.x + uvec * t.y + fvec * t.z + translation, each step rounded to float as the binary's vector operators
    // do (player_minimal.md P2; the earlier order added the translation first)
    m_translation.x = ((m2.m_rotation.rvec.x * t.x + m2.m_rotation.uvec.x * t.y) + m2.m_rotation.fvec.x * t.z) + m2.m_translation.x;
    m_translation.y = ((m2.m_rotation.rvec.y * t.x + m2.m_rotation.uvec.y * t.y) + m2.m_rotation.fvec.y * t.z) + m2.m_translation.y;
    m_translation.z = ((m2.m_rotation.rvec.z * t.x + m2.m_rotation.uvec.z * t.y) + m2.m_rotation.fvec.z * t.z) + m2.m_translation.z;
}

// 0x005B9EE0 (:1017). Turns the matrix about its own right axis (uvec rotated around rvec, fvec = rvec x uvec).
void matrix::rotate_around_local_x(float angle_rad)
{
    const vector origin(0.0f, 0.0f, 0.0f);
    vector new_uvec;
    vec_rot_point_around_line(new_uvec, uvec, angle_rad, origin, rvec);
    uvec = new_uvec;
    fvec.x = uvec.z * rvec.y - uvec.y * rvec.z;
    fvec.y = uvec.x * rvec.z - uvec.z * rvec.x;
    fvec.z = uvec.y * rvec.x - uvec.x * rvec.y;
}

// 0x005B9FD0 (:1027). Turns the matrix about its own up axis (fvec rotated around uvec, then rebuilt keeping uvec).
void matrix::rotate_around_local_y(float angle_rad)
{
    const vector up = uvec;
    const vector origin(0.0f, 0.0f, 0.0f);
    vector new_fvec;
    vec_rot_point_around_line(new_fvec, fvec, angle_rad, origin, uvec);
    make_uvec(new_fvec, up);
}

// 0x005BA050 (:1037). Turns the matrix about its own forward axis: uvec rotated around fvec, rvec = uvec x fvec
// (VFX.md FX7: the random decal rotation).
void matrix::rotate_around_local_z(float angle_rad)
{
    const vector origin(0.0f, 0.0f, 0.0f);
    vector new_uvec;
    vec_rot_point_around_line(new_uvec, uvec, angle_rad, origin, fvec);
    uvec = new_uvec;
    rvec.set(uvec.y * fvec.z - uvec.z * fvec.y, fvec.x * uvec.z - uvec.x * fvec.z, uvec.x * fvec.y - fvec.x * uvec.y);
}

// 0x005A4370 (:1047)
// Turns the matrix about an axis in its own frame by the angle times the axis length (stub_impl.md S5: the death
// camera): into the axis' frame, a bank, and back. A zero axis (every component within 1e-7) does nothing.
void matrix::rotate_around_local_vector(const vector& axis, float ang_rad)
{
    // Magic number: 1e-07 is a double constant.
    if (-1e-07 < (double)axis.x && (double)axis.x < 1e-07 && -1e-07 < (double)axis.y && (double)axis.y < 1e-07 && -1e-07 < (double)axis.z && (double)axis.z < 1e-07)
    {
        return;
    }
    matrix to_axis;
    to_axis.make(axis);
    const float len = (float)sqrt((double)(float)((double)axis.y * (double)axis.y + (double)axis.x * (double)axis.x + (double)axis.z * (double)axis.z));
    matrix spin;
    spin.set(0.0f, (float)((double)len * (double)ang_rad), 0.0f);
    rotate(to_axis);
    unrotate(spin);
    unrotate(to_axis);
}

// 0x004B5AF0 (.h:502). A point through the matrix: rvec * x + uvec * y + fvec * z + translation, each step rounded to
// float as the binary's vector operators do (player_minimal.md P2).
vector operator*(const vector& v, const matrix43& m)
{
    return vector(((m.m_rotation.rvec.x * v.x + m.m_rotation.uvec.x * v.y) + m.m_rotation.fvec.x * v.z) + m.m_translation.x,
                  ((m.m_rotation.rvec.y * v.x + m.m_rotation.uvec.y * v.y) + m.m_rotation.fvec.y * v.z) + m.m_translation.y,
                  ((m.m_rotation.rvec.z * v.x + m.m_rotation.uvec.z * v.y) + m.m_rotation.fvec.z * v.z) + m.m_translation.z);
}

// 0x004B5330 (.h:208). this then m: the translation through m (as operator*(vector, matrix43)), the rotation this * m.
matrix43 matrix43::operator*(const matrix43& m) const
{
    const vector pos = m_translation * m;
    return matrix43(pos, m_rotation * m.m_rotation);
}

// 0x004B56E0 (.h:315)
// this = this then m1 then m2 (each step: the translation moved by the step, the rotation this * step).
void matrix43::multiply_two_additional(const matrix43& m1, const matrix43& m2)
{
    const matrix43* steps[2] = {&m1, &m2};
    for (int i = 0; i < 2; i++)
    {
        const matrix43& m = *steps[i];
        const vector t = m_translation;
        m_translation.x = m.m_translation.x + (m.m_rotation.rvec.x * t.x + m.m_rotation.uvec.x * t.y + m.m_rotation.fvec.x * t.z);
        m_translation.y = m.m_translation.y + m.m_rotation.rvec.y * t.x + m.m_rotation.uvec.y * t.y + m.m_rotation.fvec.y * t.z;
        m_translation.z = m.m_translation.z + m.m_rotation.rvec.z * t.x + m.m_rotation.uvec.z * t.y + m.m_rotation.fvec.z * t.z;
        m_rotation = m_rotation * m.m_rotation;
    }
}

// ---- matrix44 (row-major 4x4; points are row vectors, so a * b applies a first)

// 0x004B5C90 (.h:776)
// The elements are left uninitialized.
matrix44::matrix44()
{
}

// 0x004B5E00 (.h:816)
matrix44::matrix44(const vector4& v0, const vector4& v1, const vector4& v2, const vector4& v3)
{
    set(v0.x, v0.y, v0.z, v0.w, v1.x, v1.y, v1.z, v1.w, v2.x, v2.y, v2.z, v2.w, v3.x, v3.y, v3.z, v3.w);
}

// 0x004B5EF0 (.h:893)
const float& matrix44::element(unsigned int row, unsigned int col) const
{
    return elem[row][col];
}

// The writable element (no separate copy in the retail build; the const one's body).
float& matrix44::element(unsigned int row, unsigned int col)
{
    return elem[row][col];
}

// 0x004B5F80 (.h:946)
void matrix44::transpose()
{
    for (int row = 0; row < 4; row++)
    {
        for (int col = row + 1; col < 4; col++)
        {
            const float t = elem[row][col];
            elem[row][col] = elem[col][row];
            elem[col][row] = t;
        }
    }
}

// Not in the binary: the 3x3 minor of a 4x4 matrix without one row and column (as rows of a matrix).
static matrix matrix44_minor(const matrix44& m, int skip_row, int skip_col)
{
    float values[9];
    int n = 0;
    for (int row = 0; row < 4; row++)
    {
        if (row == skip_row)
        {
            continue;
        }
        for (int col = 0; col < 4; col++)
        {
            if (col != skip_col)
            {
                values[n++] = m.element(row, col);
            }
        }
    }
    matrix minor;
    minor.rvec = vector(values[0], values[1], values[2]);
    minor.uvec = vector(values[3], values[4], values[5]);
    minor.fvec = vector(values[6], values[7], values[8]);
    return minor;
}

// 0x005774D0 (:1529)
// Expansion along the first row.
float matrix44::determinant()
{
    const float d0 = matrix44_minor(*this, 0, 0).determinant();
    const float d1 = matrix44_minor(*this, 0, 1).determinant();
    const float d2 = matrix44_minor(*this, 0, 2).determinant();
    const float d3 = matrix44_minor(*this, 0, 3).determinant();
    return (d2 * elem[0][2] + (d0 * elem[0][0] - d1 * elem[0][1])) - d3 * elem[0][3];
}

// 0x00577760 (:1554)
// The adjugate over the determinant (cofactors of the 3x3 minors); a singular matrix is left as it is.
void matrix44::inverse()
{
    const float det = determinant();
    if (det == 0.0f)
    {
        return;
    }
    const matrix44 m = *this;
    for (int row = 0; row < 4; row++)
    {
        for (int col = 0; col < 4; col++)
        {
            const float minor_det = matrix44_minor(m, col, row).determinant();
            elem[row][col] = ((row + col) & 1) != 0 ? -minor_det / det : minor_det / det;
        }
    }
}

// 0x005A4740 (:1654): each row within epsilon (vec_equal_robust; rfg.exe inlines it for the right vector)
bool mat_equal_robust(const matrix& a, const matrix& b, float epsilon)
{
    return vec_equal_robust(a.rvec, b.rvec, epsilon) && vec_equal_robust(a.uvec, b.uvec, epsilon) && vec_equal_robust(a.fvec, b.fvec, epsilon);
}

// 0x004B5FF0 (.h:998)
matrix44 matrix44::copy_transpose() const
{
    matrix44 result;
    for (int row = 0; row < 4; row++)
    {
        for (int col = 0; col < 4; col++)
        {
            result.elem[row][col] = elem[col][row];
        }
    }
    return result;
}

// 0x004B6060 (.h:1056)
matrix44 matrix44::operator*(const matrix44& _m) const
{
    matrix44 result;
    for (int row = 0; row < 4; row++)
    {
        for (int col = 0; col < 4; col++)
        {
            result.elem[row][col] = elem[row][0] * _m.elem[0][col] + elem[row][1] * _m.elem[1][col] + elem[row][2] * _m.elem[2][col] + elem[row][3] * _m.elem[3][col];
        }
    }
    return result;
}

// 0x004B6600 (.h:1166)
void matrix44::set(float m00, float m01, float m02, float m03, float m10, float m11, float m12, float m13, float m20, float m21, float m22, float m23, float m30, float m31, float m32, float m33)
{
    elem[0][0] = m00;
    elem[0][1] = m01;
    elem[0][2] = m02;
    elem[0][3] = m03;
    elem[1][0] = m10;
    elem[1][1] = m11;
    elem[1][2] = m12;
    elem[1][3] = m13;
    elem[2][0] = m20;
    elem[2][1] = m21;
    elem[2][2] = m22;
    elem[2][3] = m23;
    elem[3][0] = m30;
    elem[3][1] = m31;
    elem[3][2] = m32;
    elem[3][3] = m33;
}

// ---- matrix23 (UI.md UI2: the vint element transforms). rfg.exe's SSE2 code computes each element in double and
// rounds it to float once.

// 0x0045AD20 (.h:1197)
matrix23::matrix23()
{
}

// 0x0045AD30 (.h:1200)
void matrix23::make_identity()
{
    elem[0][0] = 1.0f;
    elem[0][1] = 0.0f;
    elem[0][2] = 0.0f;
    elem[1][0] = 0.0f;
    elem[1][1] = 1.0f;
    elem[1][2] = 0.0f;
}

// 0x0045AD60 (.h:1207)
void matrix23::make_rotation(float ang)
{
    // x87 fcos/fsin of the float angle, rounded to float
    float c = (float)cos((double)ang);
    float s = (float)sin((double)ang);
    elem[0][2] = 0.0f;
    elem[1][2] = 0.0f;
    elem[0][0] = c;
    elem[0][1] = -s;
    elem[1][0] = s;
    elem[1][1] = c;
}

// 0x0045ADC0 (.h:1216)
void matrix23::make_scale(const vector2& scale)
{
    elem[0][0] = scale.x;
    elem[0][1] = 0.0f;
    elem[0][2] = 0.0f;
    elem[1][0] = 0.0f;
    elem[1][1] = scale.y;
    elem[1][2] = 0.0f;
}

// 0x0045AF20 (.h:1256)
vector2 matrix23::operator*(const vector2& v) const
{
    vector2 result;
    result.x = (float)((double)elem[0][1] * (double)v.y + (double)v.x * (double)elem[0][0] + (double)elem[0][2]);
    result.y = (float)((double)elem[1][0] * (double)v.x + (double)elem[1][1] * (double)v.y + (double)elem[1][2]);
    return result;
}

// 0x0045ADF0 (.h:1240)
matrix23 matrix23::operator*(const matrix23& m) const
{
    matrix23 r;
    r.elem[0][0] = (float)((double)elem[0][0] * (double)m.elem[0][0] + (double)elem[0][1] * (double)m.elem[1][0]);
    r.elem[0][1] = (float)((double)m.elem[0][1] * (double)elem[0][0] + (double)m.elem[1][1] * (double)elem[0][1]);
    r.elem[0][2] = (float)((double)m.elem[0][2] * (double)elem[0][0] + (double)m.elem[1][2] * (double)elem[0][1] + (double)elem[0][2]);
    r.elem[1][0] = (float)((double)elem[1][0] * (double)m.elem[0][0] + (double)elem[1][1] * (double)m.elem[1][0]);
    r.elem[1][1] = (float)((double)elem[1][0] * (double)m.elem[0][1] + (double)elem[1][1] * (double)m.elem[1][1]);
    r.elem[1][2] = (float)((double)elem[1][0] * (double)m.elem[0][2] + (double)elem[1][1] * (double)m.elem[1][2] + (double)elem[1][2]);
    return r;
}
