// Original: volition/vlib/code/math/matrix.h
// Implementation: volition/vlib/code/math/matrix.cpp
//
// Matrices. matrix: 3x3 rotation stored as three row vectors (rvec = right/x, uvec = up/y, fvec = forward/z); object
// orientations are stored this way. matrix43: rotation + translation. matrix44: 4x4 floats (render side).
// matrix34: 3 rows of 4 floats. matrix23: a 2D affine transform (2 rows of 3 floats, UI).
#pragma once

#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/math/vector4.h"

class quaternion;
class vector2;

// size 0x24
class matrix
{
public:
    matrix(const vector& axis, float angle); // 0x005A10A0 (:84)
    matrix(float p, float b, float h); // 0x005A1060 (:71)
    matrix(float sinp, float cosp, float sinb, float cosb, float sinh, float cosh); // 0x005A0F00 (:58)
    matrix(const vector& _rvec, const vector& _uvec, const vector& _fvec); // 0x00575630 (:106)
    matrix(const matrix& m); // 0x005755F0 (:95)
    matrix(); // 0x00482E40 (.h:53)
    void set(const vector& axis, float angle); // 0x005759C0 (:189)
    void set(float p, float b, float h); // 0x005757E0 (:170)
    void set(float sinp, float cosp, float sinb, float cosb, float sinh, float cosh); // 0x00575680 (:139)
private:
    vector operator*(const vector&) const;
public:
    matrix operator*(const matrix& m) const; // 0x00575C00 (:234)
    void extract_angles(float& p, float& b, float& h) const; // 0x00575EF0 (:260)
    void extract_heading(float& h) const; // 0x005762E0 (:326)
    float extract_heading() const; // 0x00576190 (:297)
    void extract_pitch(float& p) const; // 0x005762F0 (:336)
    float extract_pitch() const; // 0x005762C0 (:315)
    void extract_axis_and_angle(vector& rot_axis, float& theta) const; // 0x005A10C0 (:348)
    bool check_for_zeroes() const; // 0x00576310 (:422)
    bool is_valid() const; // 0x005A14B0 (:451)
    bool is_orthonormal() const; // 0x005A15C0 (:461)
    float determinant() const; // 0x00576410 (:481)
    matrix copy_orthogonalize() const; // 0x005A17A0 (:501)
    matrix copy_fix() const; // 0x005A1BA0 (:548)
    matrix copy_transpose() const; // 0x005764F0 (:604)
    matrix copy_inverse() const; // 0x005A24C0 (:625)
    void orthogonalize(); // 0x005A2790 (:640)
    void fix(); // 0x005A2B50 (:683)
    void transpose(); // 0x00576570 (:736)
    void inverse(); // 0x005765B0 (:758)
    void rand_quick(); // 0x005B9CE0 (:748)
    void make_identity(); // 0x00576850 (:802)
    void make(const vector& _fvec); // 0x005A34C0 (:813)
    void make_quick(const vector& _fvec); // 0x005A3760 (:844)
    void make_uvec(const vector& _fvec, const vector& _uvec); // 0x005A3950 (:884)
    void make_rvec(const vector& _fvec, const vector& _rvec); // 0x005A3CA0 (:911)
    void make_ruvec(const vector& _rvec, const vector& _uvec); // 0x005A3FF0 (:940)
    void rotate(const matrix& m); // 0x00576890 (:967)
    void unrotate(const matrix& m); // 0x00576B50 (:991)
    void rotate_around_local_x(float angle_rad); // 0x005B9EE0 (:1017)
    void rotate_around_local_y(float angle_rad); // 0x005B9FD0 (:1027)
    void rotate_around_local_z(float angle_rad); // 0x005BA050 (:1037)
    void rotate_around_local_vector(const vector& axis, float ang_rad); // 0x005A4370 (:1047)
    void make_ry(float angle_rad); // 0x00576E10 (:1072)
    void make_rx(float angle_rad); // 0x00576E80 (:1094)

    vector rvec; // 0x00
    vector uvec; // 0x0C
    vector fvec; // 0x18
};

extern const matrix IDENTITY_MATRIX; // 0x0163B220

// size 0x30
class matrix43
{
public:
    matrix43(const matrix43& __that); // 0x004B8610
    matrix43(const vector& _pos, const matrix& _orient); // 0x00576F50 (:1150)
    matrix43(const vector& _rvec, const vector& _uvec, const vector& _fvec, const vector& _pos); // 0x00576EF0 (:1138)
    matrix43(const matrix& m); // 0x005A4490 (:1121)
    matrix43(); // 0x004B5AE0 (.h:488)
    matrix43 operator*(const matrix43& m) const; // 0x004B5330 (.h:208)
    vector operator*(const vector&) const;
    void multiply_two(const matrix43& m1, const matrix43& m2); // 0x004B54F0 (.h:261)
    void multiply_two_additional(const matrix43& m1, const matrix43& m2); // 0x004B56E0 (.h:315)
    bool operator==(const matrix43&) const;
    const matrix* get_orient_part() const; // 0x004B5AC0 (.h:451)
    matrix* get_orient_part(); // 0x004B5AA0 (.h:441)
    const vector* get_pos_part() const; // 0x004B5AD0 (.h:456)
    vector* get_pos_part(); // 0x004B5AB0 (.h:446)
    void transpose(); // 0x00577090 (:1284)
    matrix43 invert_orthogonal() const; // 0x005A44E0 (:1160)
    void set_translation(const vector& p); // 0x00576FB0 (:1188)
    void get_translation(vector& v) const; // 0x00576FF0 (:1237)
    vector get_translation() const; // 0x00576FD0 (:1203)
    void make_identity(); // 0x005A4680 (:1210)
    void make_from_quaternion_and_point(const quaternion& quat, const vector& point); // 0x005BA140 (:1225)
    void set_orientation(const matrix& m); // 0x00577050 (:1259)
    void get_orientation(matrix& m) const; // 0x00577010 (:1248)
    bool is_valid() const; // 0x005A46D0 (:1307)

    matrix m_rotation; // 0x00
    vector m_translation; // 0x24
};

extern const matrix43 IDENTITY_MATRIX43; // 0x0163B244

// size 0x40
class matrix44
{
public:
    matrix44(const matrix&, const vector&);
    matrix44(const vector& v0, const vector& v1, const vector& v2, const vector& v3); // 0x004B5E70 (.h:837)
    matrix44(const vector4& v0, const vector4& v1, const vector4& v2, const vector4& v3); // 0x004B5E00 (.h:816)
    matrix44(const matrix _m); // 0x004B5D60 (.h:795)
    matrix44(float e00, float e01, float e02, float e03, float e10, float e11, float e12, float e13, float e20, float e21, float e22, float e23, float e30, float e31, float e32, float e33); // 0x004B5CA0 (.h:778)
    matrix44(); // 0x004B5C90 (.h:776)
    const float& element(unsigned int row, unsigned int col) const; // 0x004B5EF0 (.h:893)
    float& element(unsigned int, unsigned int);
    void make_identity();
    void make_translation(float x, float y, float z); // 0x004B5F10 (.h:925)
    float determinant(); // 0x005774D0 (:1529)
    void inverse(); // 0x00577760 (:1554)
    void transpose(); // 0x004B5F80 (.h:946)
    matrix44 copy_transpose() const; // 0x004B5FF0 (.h:998)
    matrix44 operator*(const matrix44& _m) const; // 0x004B6060 (.h:1056)
    operator float*();
    operator const float*() const;
    void set(float m00, float m01, float m02, float m03, float m10, float m11, float m12, float m13, float m20, float m21, float m22, float m23, float m30, float m31, float m32, float m33); // 0x004B6600 (.h:1166)
    bool is_valid() const; // 0x00577480 (:1461)

private:
    union
    {
        float elem[4][4]; // 0x00
        struct
        {
            float _00, _01, _02, _03; // 0x00
            float _10, _11, _12, _13; // 0x10
            float _20, _21, _22, _23; // 0x20
            float _30, _31, _32, _33; // 0x30
        };
    };
};

extern const matrix44 IDENTITY_MATRIX44; // 0x0163B278

// size 0x30
class matrix34
{
public:
    matrix34(const matrix&, const vector&);
    matrix34(const vector&, const vector&, const vector&, const vector&);
    matrix34(const float (*m)[4]); // 0x00563910 (.h:556)
    matrix34(const matrix&);
    matrix34();
    matrix34 operator*(const matrix34&) const;
    vector operator*(const vector&) const;
    vector get_rvec() const;
    vector get_uvec() const;
    vector get_fvec() const;
    vector get_pos() const;
    void get_3x3_matrix(matrix&, vector&);
    float& operator()(unsigned int r, unsigned int c); // 0x00563960 (.h:735)
    const float& operator()(unsigned int, unsigned int) const;
    void make_identity();
    bool is_valid() const; // 0x005770F0 (:1326)

    float elem[3][4]; // 0x00
    static const float identity[3][4]; // 0x012C32E4
};

matrix34 m34_identity(); // 0x00577140 (:1343)
matrix34 m34_rotate_x(float angle); // 0x005771A0 (:1354)
matrix34 m34_rotate_y(float angle); // 0x00577240 (:1376)
matrix34 m34_rotate_z(float angle); // 0x005772E0 (:1398)
matrix34 m34_translate(float x, float y, float z); // 0x00577380 (:1422)
matrix34 m34_scale(float x, float y, float z); // 0x00577400 (:1442)
bool mat_equal_robust(const matrix& a, const matrix& b, float epsilon); // 0x005A4740 (:1654)
vector operator*(const vector& v, const matrix& m); // 0x004B51D0 (.h:146)
vector operator*(const vector& v, const matrix43& m); // 0x004B5AF0 (.h:502)

// size 0x18
class matrix23
{
public:
    matrix23(); // 0x0045AD20 (.h:1197)
    void make_identity(); // 0x0045AD30 (.h:1200)
    void make_rotation(float ang); // 0x0045AD60 (.h:1207)
    void make_scale(float, float);
    void make_scale(const vector2& scale); // 0x0045ADC0 (.h:1216)
    void make_translate(float, float);
    void make_translate(vector2&);
    vector2 operator*(const vector2& v) const; // 0x0045AF20 (.h:1256)
    matrix23 operator*(const matrix23& m) const; // 0x0045ADF0 (.h:1240)
    void inverse_transposed();

    float elem[2][3]; // 0x00
};
