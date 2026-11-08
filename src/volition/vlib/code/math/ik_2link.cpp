// Original: volition/vlib/code/math/ik_2link.cpp
//
// Two-bone inverse kinematics. The original does its scalar arithmetic in double and rounds to float where a value
// is stored (the casts below follow its rounding points; single float ops are written as float ops, which round the
// same way).

#include "volition/vlib/code/math/ik_2link.h"

#include "volition/vlib/code/math/vector3.h"

#include <math.h>

// 0x005B8D10 (:98)
// Solves the anchor-joint-end chain for the target with the bone lengths kept: out of reach, both bones point at the
// target; otherwise the joint goes to the circle where the two spheres around the anchor and the target meet, on the
// side of the bend (the chain's plane normal crossed with the anchor-target direction), turned by rotation_angle
// around that direction, and the end reaches the target.
void ik_2link_solve(const vector& anchor_pos, const vector& start_joint_pos, const vector& end_pos, const vector& target_pos, float rotation_angle, vector& solve_joint_pos,
                    vector& solve_end_pos)
{
    const float ux = anchor_pos.x - start_joint_pos.x;
    const float uy = anchor_pos.y - start_joint_pos.y;
    const float uz = anchor_pos.z - start_joint_pos.z;
    const float upper_sq = (float)((double)ux * (double)ux + (double)uy * (double)uy + (double)uz * (double)uz);
    const float lx = start_joint_pos.x - end_pos.x;
    const float ly = start_joint_pos.y - end_pos.y;
    const float lz = start_joint_pos.z - end_pos.z;
    const float lower_sq = (float)((double)lx * (double)lx + (double)ly * (double)ly + (double)lz * (double)lz);
    const float lower_len = (float)sqrt((double)lower_sq);
    const float upper_len = (float)sqrt((double)upper_sq);
    const float tx = anchor_pos.x - target_pos.x;
    const float ty = anchor_pos.y - target_pos.y;
    const float tz = anchor_pos.z - target_pos.z;
    const float target_sq = (float)((double)tx * (double)tx + (double)ty * (double)ty + (double)tz * (double)tz);
    const float dist = (float)sqrt((double)target_sq);
    // the unit direction from the anchor to the target
    vector dir(target_pos.x - anchor_pos.x, target_pos.y - anchor_pos.y, target_pos.z - anchor_pos.z);
    dir.x = dir.x / dist;
    dir.y = dir.y / dist;
    dir.z = dir.z / dist;
    if ((double)lower_len + (double)upper_len <= (double)dist)
    {
        // out of reach: the chain stretched toward the target
        solve_joint_pos.x = dir.x * upper_len + anchor_pos.x;
        solve_joint_pos.y = dir.y * upper_len + anchor_pos.y;
        solve_joint_pos.z = dir.z * upper_len + anchor_pos.z;
        solve_end_pos.x = solve_joint_pos.x + dir.x * lower_len;
        solve_end_pos.y = solve_joint_pos.y + dir.y * lower_len;
        solve_end_pos.z = solve_joint_pos.z + dir.z * lower_len;
        return;
    }
    // the distance along dir to the circle's center, and the circle's radius (law of cosines)
    // Magic number: 2.0 (0x012B96D8).
    const double upper_sq_d = (double)upper_len * (double)upper_len;
    const float along = (float)(((double)dist * (double)dist + upper_sq_d - (double)lower_len * (double)lower_len) / ((double)dist * 2.0));
    float radius_sq = (float)(upper_sq_d - (double)along * (double)along);
    if (0.0f - radius_sq >= 0.0f)
    {
        radius_sq = 0.0f;
    }
    const float radius = (float)sqrt((double)radius_sq);
    // the chain's plane normal (upper bone x lower bone, both pointing away from the anchor)
    const float bx = start_joint_pos.x - anchor_pos.x;
    const float by = start_joint_pos.y - anchor_pos.y;
    const float bz = start_joint_pos.z - anchor_pos.z;
    const float ex = end_pos.x - start_joint_pos.x;
    const float ey = end_pos.y - start_joint_pos.y;
    const float ez = end_pos.z - start_joint_pos.z;
    const float nx = (float)((double)by * (double)ez - (double)bz * (double)ey);
    const float ny = (float)((double)bz * (double)ex - (double)ez * (double)bx);
    const float nz = (float)((double)ey * (double)bx - (double)by * (double)ex);
    // the bend direction: normal x dir, normalized
    vector bend((float)((double)nz * (double)dir.y - (double)ny * (double)dir.z), (float)((double)dir.z * (double)nx - (double)nz * (double)dir.x),
                (float)((double)ny * (double)dir.x - (double)dir.y * (double)nx));
    const float bend_len = (float)sqrt((double)(float)((double)bend.y * (double)bend.y + (double)bend.x * (double)bend.x + (double)bend.z * (double)bend.z));
    // Magic number: 1.0 (0x012B80F0).
    const float inv = (float)(1.0 / (double)bend_len);
    bend.x = bend.x * inv;
    bend.z = bend.z * inv;
    bend.y = bend.y * inv;
    if (rotation_angle != 0.0f)
    {
        vec_rot_point_around_line(bend, bend, rotation_angle, ZERO_VECTOR, dir);
    }
    solve_joint_pos.x = dir.x * along + anchor_pos.x + bend.x * radius;
    solve_joint_pos.y = dir.y * along + anchor_pos.y + bend.y * radius;
    solve_joint_pos.z = dir.z * along + anchor_pos.z + bend.z * radius;
    solve_end_pos = target_pos;
}
