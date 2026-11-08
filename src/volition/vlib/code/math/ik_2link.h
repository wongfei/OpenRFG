// Original: volition/vlib/code/math/ik_2link.h
// Implementation: volition/vlib/code/math/ik_2link.cpp
//
// Two-bone inverse kinematics.
#pragma once


class vector;

void ik_2link_solve(const vector& anchor_pos, const vector& start_joint_pos, const vector& end_pos, const vector& target_pos, float rotation_angle, vector& solve_joint_pos, vector& solve_end_pos); // 0x005B8D10 (:98)
