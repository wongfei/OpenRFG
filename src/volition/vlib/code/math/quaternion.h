// Original: volition/vlib/code/math/quaternion.h
// Implementation: volition/vlib/code/math/vlib_quaternion.cpp
//
// Rotation quaternion (x, y, z, w) and short_quat, the 16-bit fixed-point form used in compressed animation data.
#pragma once

class matrix;
class matrix43;
class short_quat;
class vector;

// size 0x10
class quaternion
{
public:
    quaternion(const matrix& m); // 0x005BB810 (vlib_quaternion.cpp:285)
    quaternion(const matrix43& m); // 0x005BB400 (vlib_quaternion.cpp:273)
    quaternion(const vector& fvec, float a); // 0x0057E9B0 (vlib_quaternion.cpp:252)
    quaternion(float _x, float _y, float _z, float _w); // 0x0057E980 (vlib_quaternion.cpp:237)
    quaternion(const short_quat& q); // 0x005639F0 (.h:157)
    quaternion(const quaternion& q); // 0x005639D0 (.h:148)
    quaternion(); // 0x005639C0 (.h:145)
    void set(const matrix& m); // 0x005A7F00 (vlib_quaternion.cpp:295)
    quaternion operator-(const quaternion& v) const; // 0x005974A0 (.h:214)
    quaternion operator-() const; // 0x005973D0 (.h:202)
    quaternion operator+(const quaternion& v) const; // 0x00597420 (.h:208)
    void operator+=(const quaternion&);
    void operator-=(const quaternion&);
    bool operator==(const quaternion&) const;
    quaternion operator*(float scalar) const; // 0x0057EC80 (vlib_quaternion.cpp:401)
    quaternion operator*(const quaternion& q1) const; // 0x0057EA20 (vlib_quaternion.cpp:366)
    void extract_matrix(matrix& dest_matrix) const; // 0x0057ECF0 (vlib_quaternion.cpp:423)
    void extract_matrix(matrix43& dest_matrix) const; // 0x005A8310 (vlib_quaternion.cpp:410)
    quaternion copy_normalize() const; // 0x0057EF20 (vlib_quaternion.cpp:444)
    quaternion copy_inverse() const; // 0x0057EFF0 (vlib_quaternion.cpp:455)
    quaternion copy_ln() const; // 0x0057F030 (vlib_quaternion.cpp:464)
    quaternion copy_exp() const; // 0x0057F0E0 (vlib_quaternion.cpp:481)
    void normalize(); // 0x0057F1E0 (vlib_quaternion.cpp:498)
    void inverse(); // 0x0057F290 (vlib_quaternion.cpp:511)
    void ln(); // 0x0057F2C0 (vlib_quaternion.cpp:520)
    void exp(); // 0x0057F3B0 (vlib_quaternion.cpp:540)
    void make_identity(); // 0x0057F4B0 (vlib_quaternion.cpp:562)
    bool is_valid() const; // 0x0057F4D0 (vlib_quaternion.cpp:574)

    float x; // 0x00
    float y; // 0x04
    float z; // 0x08
    float w; // 0x0C
};

// size 0x8
class short_quat
{
public:
    short_quat(short _x, short _y, short _z, short _w); // 0x0057E190 (vlib_quaternion.cpp:51)
    short_quat(const short_quat& q); // 0x00563990 (.h:62)
    short_quat(); // 0x00563980 (.h:59)
    short_quat operator-(const short_quat&) const;
    short_quat operator-() const;
    short_quat operator+(const short_quat&) const;
    void operator-=(const short_quat&);
    short_quat operator*(const short_quat& q1) const; // 0x0057E1C0 (vlib_quaternion.cpp:66)
    short_quat copy_normalize() const; // 0x0057E590 (vlib_quaternion.cpp:99)
    short_quat copy_inverse() const; // 0x0057E6C0 (vlib_quaternion.cpp:113)
    void normalize(); // 0x0057E6F0 (vlib_quaternion.cpp:122)
    void inverse(); // 0x0057E820 (vlib_quaternion.cpp:134)

    short x; // 0x00
    short y; // 0x02
    short z; // 0x04
    short w; // 0x06
};

void slerp(quaternion& result, const quaternion& a, const quaternion& b, float t); // 0x005B4F80 (.h:372)
void slerp_aligned(quaternion& result, const quaternion& a, const quaternion& b, float t); // 0x006C3940 (.h:387)
void slerp(short_quat& res, const short_quat& a, const short_quat& b, float t); // 0x005C1B20 (.h:401)
float pltind_quaternion_dot(const short_quat& q1, const short_quat& q2); // 0x0057E910 (vlib_quaternion.cpp:160)
void pltind_slerp(short_quat& short_result, const short_quat& short_q1, const short_quat& short_q2, float t); // 0x005BB220 (vlib_quaternion.cpp:178)
void squad(quaternion& result, const short_quat& p, const short_quat& a, const short_quat& b, const short_quat& q, float t); // 0x005C41D0 (vlib_quaternion.cpp:203)
void quaternion_init(); // 0x0057E970 (vlib_quaternion.cpp:226)
float quaternion_dot(const quaternion& q1, const quaternion& q2); // 0x0057F540 (vlib_quaternion.cpp:596)
void quat_lerp(quaternion& result, const quaternion& a, const quaternion& b, float t); // 0x005A8550 (vlib_quaternion.cpp:613)
void pltind_slerp(quaternion& result, const quaternion& q1, const quaternion& q2, float t); // 0x005A8750 (vlib_quaternion.cpp:693)
void squad(quaternion& result, const quaternion& p, const quaternion& a, const quaternion& b, const quaternion& q, float t); // 0x005BBC20 (vlib_quaternion.cpp:753)
quaternion ln_dif(const quaternion& q1, const quaternion& q2); // 0x0057F570 (vlib_quaternion.cpp:769)
quaternion operator*(float scalar, const quaternion& q); // 0x0057F8F0 (vlib_quaternion.cpp:787)

extern quaternion IDENTITY_QUATERNION; // 0x0163DC88

// 0x00563980 (.h:59): nothing to set
inline short_quat::short_quat()
{
}
