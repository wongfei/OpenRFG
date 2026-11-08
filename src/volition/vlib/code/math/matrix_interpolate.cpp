// Original: volition/vlib/code/math/matrix_interpolate.cpp
//
// Interpolation between transforms (decompiled): the rotations are slerped as quaternions (pltind_slerp), the
// translations lerped.

#include "volition/vlib/code/math/matrix_interpolate.h"

#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/math/vector3.h"

// 0x005C37B0 (:25)
matrix43 m43_slerp_matrix(const matrix43& m1, const matrix43& m2, float time)
{
    const quaternion q1(m1.m_rotation); // inlined quaternion::set
    const quaternion q2(m2.m_rotation);
    return m43_slerp_matrix(q1, q2, m1.m_translation, m2.m_translation, time); // inlined
}

// 0x005BA3A0 (:53)
matrix43 m43_slerp_matrix(const quaternion& q1, const quaternion& q2, const vector& p1, const vector& p2, float time)
{
    matrix43 result;
    m43_slerp_matrix(result, q1, q2, p1, p2, time); // inlined
    return result;
}

// 0x005BA600 (:81)
void m43_slerp_matrix(matrix43& tempm, const quaternion& q1, const quaternion& q2, const vector& p1, const vector& p2, float time)
{
    quaternion q;
    pltind_slerp(q, q1, q2, time);
    q.extract_matrix(tempm); // inlined
    tempm.m_translation.x = p1.x + (p2.x - p1.x) * time;
    tempm.m_translation.y = p1.y + (p2.y - p1.y) * time;
    tempm.m_translation.z = p1.z + (p2.z - p1.z) * time;
}
