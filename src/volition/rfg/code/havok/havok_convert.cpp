// Original: volition/rfg/code/havok/havok_convert.cpp
//
// Conversions between the game's math types and Havok's (decompiled). Havok's rotation columns are the game matrix
// rows (col0 = rvec, col1 = uvec, col2 = fvec); w components are written as 0. Havok's quaternion is the conjugate
// of the game's (w negated). Compiled in rfg_havok_unity.cpp.

#include "volition/rfg/code/havok/havok_convert.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrix3f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkrotationf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/quaternion/hkquaternionf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/plane.h"
#include "volition/vlib/code/math/quaternion.h"

// Component access of Havok's SIMD vectors.
static float hk_get(const hkVector4f& v, int i)
{
    return ((const float*)&v.m_quad)[i];
}

static void game_rows_to_havok_columns(hkMatrix3f& m, const matrix& orient)
{
    m.m_col0.set(orient.rvec.x, orient.rvec.y, orient.rvec.z, 0.0f);
    m.m_col1.set(orient.uvec.x, orient.uvec.y, orient.uvec.z, 0.0f);
    m.m_col2.set(orient.fvec.x, orient.fvec.y, orient.fvec.z, 0.0f);
}

static void havok_columns_to_game_rows(matrix& orient, const hkMatrix3f& m)
{
    orient.rvec.set(hk_get(m.m_col0, 0), hk_get(m.m_col0, 1), hk_get(m.m_col0, 2));
    orient.uvec.set(hk_get(m.m_col1, 0), hk_get(m.m_col1, 1), hk_get(m.m_col1, 2));
    orient.fvec.set(hk_get(m.m_col2, 0), hk_get(m.m_col2, 1), hk_get(m.m_col2, 2));
}

// 0x00810C90 (:20)
void havok_to_game_vector(vector& vec, const hkVector4f& hkvec)
{
    vec.set(hk_get(hkvec, 0), hk_get(hkvec, 1), hk_get(hkvec, 2));
}

// 0x00810CC0 (:26)
void game_to_havok_vector(hkVector4f& hkvec, const vector& vec)
{
    hkvec.set(vec.x, vec.y, vec.z, 0.0f);
}

// 0x00810CF0 (:32)
void havok_to_game_matrix(matrix& orientation, const hkRotationf& rot)
{
    havok_columns_to_game_rows(orientation, rot);
}

// 0x00810DA0 (:43)
void havok_to_game_matrix(matrix& gmatrix, const hkMatrix3f& hkmatrix)
{
    havok_columns_to_game_rows(gmatrix, hkmatrix);
}

// 0x00810E50 (:52)
void havok_to_game_matrix(matrix& orientation, const hkQuaternionf& q_rot)
{
    hkRotationf rot;
    rot.set(q_rot);
    havok_columns_to_game_rows(orientation, rot);
}

// 0x00810F10 (:60)
void havok_to_game_quaternion(quaternion& q_game, const hkQuaternionf& hk_quat)
{
    q_game.x = hk_get(hk_quat.m_vec, 0);
    q_game.y = hk_get(hk_quat.m_vec, 1);
    q_game.z = hk_get(hk_quat.m_vec, 2);
    q_game.w = -hk_get(hk_quat.m_vec, 3);
}

// 0x00810F40 (:68)
void havok_to_game_aabb(vector& bmin, vector& bmax, const hkAabb& hkbox)
{
    havok_to_game_vector(bmin, hkbox.m_min);
    havok_to_game_vector(bmax, hkbox.m_max);
}

// 0x00810F80 (:74)
void havok_to_game_transform(matrix43& transform, const hkTransformf& trans)
{
    havok_to_game_vector(transform.m_translation, trans.m_translation);
    havok_columns_to_game_rows(transform.m_rotation, trans.m_rotation);
}

// 0x00811040 (:80)
void havok_to_game_transform(vector& pos, matrix& orient, const hkTransformf& trans)
{
    havok_to_game_vector(pos, trans.m_translation);
    havok_columns_to_game_rows(orient, trans.m_rotation);
}

// 0x00811100 (:86)
void havok_to_game_transform(vector& pos, quaternion& qorient, const hkTransformf& trans)
{
    havok_to_game_vector(pos, trans.m_translation);
    hkQuaternionf q;
    q.set(trans.m_rotation);
    havok_to_game_quaternion(qorient, q);
}

// 0x00811190 (:94)
void havok_to_game_plane(plane& p, const hkVector4f& hkplane)
{
    p.normal.set(hk_get(hkplane, 0), hk_get(hkplane, 1), hk_get(hkplane, 2));
    p.offset = hk_get(hkplane, 3);
}

// 0x008111B0 (:102)
void game_to_havok_matrix(hkRotationf& rot, const matrix& orientation)
{
    game_rows_to_havok_columns(rot, orientation);
}

// 0x00811240 (:111)
void game_to_havok_matrix(hkQuaternionf& quat, const matrix& orientation)
{
    const quaternion q(orientation);
    quat.m_vec.set(q.x, q.y, q.z, -q.w);
}

// 0x008112A0 (:119)
void game_to_havok_tensor(hkMatrix3f& mat, const matrix& tensor)
{
    game_rows_to_havok_columns(mat, tensor);
}

// 0x00811330 (:128)
void game_to_havok_quaternion(hkQuaternionf& hk_quat, const quaternion& quat)
{
    hk_quat.m_vec.set(quat.x, quat.y, quat.z, -quat.w);
}

// 0x00811360 (:134)
void game_to_havok_aabb(hkAabb& hkbox, const vector& bmin, const vector& bmax)
{
    game_to_havok_vector(hkbox.m_min, bmin);
    game_to_havok_vector(hkbox.m_max, bmax);
}

// 0x008249C0 (:140)
void game_to_havok_transform(hkTransformf& trans, const vector& pos, const matrix& orient)
{
    game_rows_to_havok_columns(trans.m_rotation, orient);
    trans.m_translation.set(pos.x, pos.y, pos.z, 0.0f);
}

// 0x008113B0 (:150)
void game_to_havok_plane(hkVector4f& hkplane, const plane& p)
{
    hkplane.set(p.normal.x, p.normal.y, p.normal.z, p.offset);
}
