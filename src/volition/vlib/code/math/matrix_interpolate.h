// Original: volition/vlib/code/math/matrix_interpolate.h
// Implementation: volition/vlib/code/math/matrix_interpolate.cpp
//
// Matrix interpolation (slerp of a matrix43).
#pragma once


class matrix43;
class quaternion;
class vector;

matrix43 m43_slerp_matrix(const matrix43& m1, const matrix43& m2, float time); // 0x005C37B0 (:25)
matrix43 m43_slerp_matrix(const quaternion& q1, const quaternion& q2, const vector& p1, const vector& p2, float time); // 0x005BA3A0 (:53)
void m43_slerp_matrix(matrix43& tempm, const quaternion& q1, const quaternion& q2, const vector& p1, const vector& p2, float time); // 0x005BA600 (:81)
