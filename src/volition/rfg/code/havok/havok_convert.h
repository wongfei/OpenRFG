// Original: volition/rfg/code/havok/havok_convert.h
// Implementation: volition/rfg/code/havok/havok_convert.cpp
//
// Conversions between the game's math types (vector, matrix, matrix43, quaternion, plane) and Havok's (hkVector4f,
// hkRotationf, hkMatrix3f, hkQuaternionf, hkTransformf, hkAabb).
#pragma once


class hkAabb;
class hkMatrix3f;
class hkQuaternionf;
class hkRotationf;
class hkTransformf;
class hkVector4f;
class matrix;
class matrix43;
class plane;
class quaternion;
class vector;

void havok_to_game_vector(vector& vec, const hkVector4f& hkvec); // 0x00810C90 (:20)
void game_to_havok_vector(hkVector4f& hkvec, const vector& vec); // 0x00810CC0 (:26)
void havok_to_game_matrix(matrix& orientation, const hkRotationf& rot); // 0x00810CF0 (:32)
void havok_to_game_matrix(matrix& gmatrix, const hkMatrix3f& hkmatrix); // 0x00810DA0 (:43)
void havok_to_game_matrix(matrix& orientation, const hkQuaternionf& q_rot); // 0x00810E50 (:52)
void havok_to_game_quaternion(quaternion& q_game, const hkQuaternionf& hk_quat); // 0x00810F10 (:60)
void havok_to_game_aabb(vector& bmin, vector& bmax, const hkAabb& hkbox); // 0x00810F40 (:68)
void havok_to_game_transform(matrix43& transform, const hkTransformf& trans); // 0x00810F80 (:74)
void havok_to_game_transform(vector& pos, matrix& orient, const hkTransformf& trans); // 0x00811040 (:80)
void havok_to_game_transform(vector& pos, quaternion& qorient, const hkTransformf& trans); // 0x00811100 (:86)
void havok_to_game_plane(plane& p, const hkVector4f& hkplane); // 0x00811190 (:94)
void game_to_havok_matrix(hkRotationf& rot, const matrix& orientation); // 0x008111B0 (:102)
void game_to_havok_matrix(hkQuaternionf& quat, const matrix& orientation); // 0x00811240 (:111)
void game_to_havok_tensor(hkMatrix3f& mat, const matrix& tensor); // 0x008112A0 (:119)
void game_to_havok_quaternion(hkQuaternionf& hk_quat, const quaternion& quat); // 0x00811330 (:128)
void game_to_havok_aabb(hkAabb& hkbox, const vector& bmin, const vector& bmax); // 0x00811360 (:134)
void game_to_havok_transform(hkTransformf& trans, const vector& pos, const matrix& orient); // 0x008249C0 (:140)
void game_to_havok_plane(hkVector4f& hkplane, const plane& p); // 0x008113B0 (:150)
