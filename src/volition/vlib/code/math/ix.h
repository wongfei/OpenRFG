// Original: volition/vlib/code/math/ix.h
// Implementation: volition/vlib/code/math/ix.cpp
//
// Intersection tests in 2D and 3D (points, lines, segments, circles, boxes, sweeps); ix_collision is the result of a
// swept test. Placement of ix_collision inferred.
#pragma once

#include "volition/vlib/code/math/vector3.h"

struct bbox2d;
struct line;
class matrix;
class plane;
class vector2;

// size 0x10
struct ix_collision
{
    ix_collision(); // 0x0056E870

    vector hit_point; // 0x00
    float fraction; // 0x0C
};

bool ix_is_normal_facing(const vector& pos, const vector& norm, const vector& plane_point); // 0x0056D0A0 (:157)
bool ix_linesegment_plane(ix_collision& collision, const vector& start, const vector& dir, const plane& p); // 0x0056D140 (:181)
bool ix_line_plane_collide(float& fraction, const vector& start, const vector& dir, const plane& p); // 0x0056D2E0 (:214)
bool ix_line_tri_face(ix_collision& collision, const line& l, const vector** verts, const plane& p); // 0x0059B360 (:244)
bool ix_linesegment_tri_face(vector& ix, const vector& p0, const vector& p1, const vector** verts, const plane& p); // 0x0059B500 (:267)
bool ix_line_tri_face_collide(ix_collision& collision, const line& l, const vector** verts, const plane& p); // 0x0059B700 (:300)
bool ix_line_line_collide(ix_collision& collision, const line& still, const line& moving, const vector& disp); // 0x005B9560 (:328)
bool ix_linesegment_sphere(vector& intp, const vector& p0, const vector& p1, const vector& sphere_pos, float sphere_rad); // 0x0059B8A0 (:374)
bool ix_point_face(const vector& checkp, int nv, const vector** verts, const vector& norm1); // 0x0056D3C0 (:544)
bool ix_point_face(const vector& checkp, int nv, const vector* verts, const vector& norm1); // 0x0056D6B0 (:656)
bool ix_point_tri_face(const vector& checkp, const vector** verts); // 0x0056D9B0 (:770)
bool ix_sphere_sphere(const vector& a_center, const vector& b_center, float a_radius, float b_radius); // 0x0059BD20 (:811)
bool ix_sphere_triangle(const vector& origin, float radius, const vector& v0, const vector& v1, const vector& v2); // 0x0059BDE0 (:825)
bool ix_sphere_plane_collide(ix_collision& collision, const vector& start, const vector& dir, float radius, const plane& p); // 0x0056DE60 (:859)
bool ix_edge_sphereline(ix_collision& collision, const vector& xs0, const vector& vs, float Rs, const vector& v0, const vector& v1, float best_sphere_time); // 0x0056E160 (:914)
bool ix_polyedge_sphereline(ix_collision& collision, const vector& xs0, const vector& vs, float Rs, int nv, const vector** verts); // 0x0059BFC0 (:1064)
bool ix_box_box_aligned(const vector& min1, const vector& max1, const vector& min2, const vector& max2); // 0x0056E880 (:1123)
bool ix_box_box_aligned_xz(const vector& min1, const vector& max1, const vector& min2, const vector& max2); // 0x0056E920 (:1138)
bool ix_box_box_intersect(const vector& min1, const vector& max1, const vector& min2, const vector& max2, vector& out_min, vector& out_max); // 0x0056E990 (:1154)
bool ix_point_in_box(const vector& pnt, const vector& mn, const vector& mx); // 0x0056EB20 (:1176)
bool ix_point_in_box_oriented(const vector& pnt, const vector& origin, const matrix& orient, const vector& bmin, const vector& bmax); // 0x0059C950 (:1193)
bool ix_sphere_in_box(const vector& sphere_center, float sphere_radius, const vector& box_mn, const vector& box_mx); // 0x0056EBB0 (:1216)
bool ix_box_completely_in_sphere(const vector& sphere_center, float sphere_radius, const vector& box_mn, const vector& box_mx); // 0x0059CB00 (:1273)
bool ix_sphere_in_box_oriented(const vector& sphere_center, float sphere_radius, const vector& box_cen, const matrix& box_orient, const vector& box_size); // 0x0059D080 (:1328)
bool ix_point_in_sphere(const vector& sphere_center, float sphere_radius, const vector& point); // 0x0056ECF0 (:1347)
bool ix_point_in_cone(const vector& pt, const vector& cone_pos, const vector& cone_fvec, float cone_dot); // 0x0056ED80 (:1367)
bool ix_line_segment_in_cone(const vector& start, const vector& end, const vector& cone_pos, const vector& cone_fvec, float cone_dot); // 0x0059D360 (:1391)
bool ix_box_in_cone(const vector& box_mn, const vector& box_mx, const vector& cone_pos, const vector& cone_fvec, float cone_dot); // 0x0059DEB0 (:1469)
bool ix_box_oriented_in_cone(const vector& box_mn, const vector& box_mx, const vector& box_pos, const matrix& box_orient, const vector& cone_pos, const vector& cone_fvec, float cone_dot); // 0x0059E1A0 (:1525)
bool ix_oriented_box_box(const vector& _extents_a, const vector& bbox_pos_a, const matrix& bbox_orient_a, const vector& _extents_b, const vector& bbox_pos_b, const matrix& bbox_orient_b); // 0x0056EF30 (:1558)
bool ix_box_cylinder_xz(const vector& bmin, const vector& bmax, const vector& cylinder_pos, float cylinder_radius); // 0x0056FB20 (:1669)
int ix_box_box(const matrix& b_orient, const vector& b_trans, const vector& a, const vector& b); // 0x0056FBF0 (:1707)
bool ix_plane_plane(line& l, const plane& pl1, const plane& pl2); // 0x00570870 (:1853)
bool ix_line_plane(vector& ix, const line& l, const plane& p); // 0x00570C50 (:1915)
bool ix_line_plane_time(float& t, const line& l, const plane& p); // 0x00570DD0 (:1946)
bool ix_line_plane_time_unsafe(float& t, const line& l, const plane& p); // 0x00570EB0 (:1976)
bool ix_ray_plane(vector& ix, const line& r, const plane& p); // 0x00570F70 (:2009)
bool ix_ray_tri_face(vector& ix, const line& r, const vector** verts, const plane& p); // 0x00571100 (:2042)
bool ix_linesegment_box_oriented(vector& ix, const vector& cen, const matrix& orient, const vector& size, const vector& p1, const vector& p2); // 0x0059E400 (:2064)
bool ix_ray_unit_cylinder(ix_collision& ix, const line& ray, vector* normal, bool capped); // 0x00571290 (:2097)
bool ix_line_cylinder(const vector& raybase, const vector& raycos, const vector& base, const vector& axis, float length, float radius, vector& intersection); // 0x0059E870 (:2302)
bool ix_ray_box_oriented(vector& ix, const line& ray, const vector& cen, const matrix& orient, const vector& size); // 0x0059F620 (:2474)
bool ix_ray_boundingbox(vector& ix, const vector& min, const vector& max, const vector& p0, const vector& pdir); // 0x00571970 (:2508)
bool ix_linesegment_boundingbox(vector& intercept, const vector& bbox_min, const vector& bbox_max, const vector& p1, const vector& p2); // 0x00571C50 (:2590)
bool ix_ray_sphere(ix_collision& collision, const line& ray, const vector& sphere_pos, float sphere_rad); // 0x005720F0 (:2675)
bool ix2d_line_line(vector2& ix, const vector2& l1s, const vector2& l1e, const vector2& l2s, const vector2& l2e); // 0x005722F0 (:2721)
float ix_closest_point_on_ray(const vector& pnt, const line& l, vector& ix); // 0x00572460 (:2750)
float ix_closest_point_on_line_segment(vector& ix, const vector& pnt, const vector& p0, const vector& p1); // 0x00572590 (:2781)
float ix_linesegment_sphere_depth(const vector& p0, const vector& p1, const vector& sphere_pos, float sphere_rad); // 0x0059F9E0 (:2822)
bool ix_closest_line_line(ix_collision& col_a, ix_collision& col_b, const vector& p1, const vector& p2, const vector& p3, const vector& p4); // 0x00572810 (:2877)
void ix_closest_linesegment_linesegment(float& s, float& t, vector& ix1, vector& ix2, const vector& p1, const vector& q1, const vector& p2, const vector& q2); // 0x00572D00 (:2942)
bool ix_point_project_cone_onto_sphere(float& tan_circle_rad, float& cone_fov, float& cdist, const vector& pnt, const vector& sphere_cen, const float sphere_rad); // 0x0059FD10 (:3027)
bool ix_ray_cylinder(ix_collision& collision, const line& ray, const vector& cylinder_base, const vector& cylinder_axis, float cylinder_height, float cylinder_radius); // 0x005A0080 (:3078)
bool ix_ray_cylinder(ix_collision& collision, const line& ray, const vector& start, const vector& end, const float& radius); // 0x005A0D90 (:3164)
bool ix2d_linesegment_line(const vector2& seg_pt_0, const vector2& seg_pt_1, const vector2& line_pt_0, const vector2& line_pt_1, vector2* ix); // 0x00573360 (:3182)
bool ix2d_linesegment_linesegment(const vector2& a0, const vector2& a1, const vector2& b0, const vector2& b1, vector2* ix); // 0x005736A0 (:3277)
bool ix2d_point_bbox(const vector2& pt, const bbox2d& box); // 0x005737A0 (:3301)
bool ix2d_point_bbox(float pt_x, float pt_y, const bbox2d& box); // 0x00573800 (:3310)
bool ix2d_point_circle(const vector2& pt, const vector2& center, const float radius); // 0x00573860 (:3319)
bool ix2d_bbox_bbox(const bbox2d& b1, const bbox2d& b2); // 0x005738E0 (:3331)
bool ix2d_circle_linesegment(const vector2& center, const float radius, const vector2& seg_pt0, const vector2& seg_pt1, float* ix_parametric); // 0x00573950 (:3351)
int ix2d_circle_linesegment_two_points(const vector2& center, float radius, const vector2& seg_pt0, const vector2& seg_pt1, float* ix_parametric); // 0x00573B70 (:3427)
bool ix2d_circle_linesegment(const vector2& center, const float radius, const vector2& seg_pt0, const vector2& seg_pt1, vector2& ix); // 0x00573DB0 (:3503)
bool ix2d_linesegment_boundingbox(const bbox2d& box, const vector2& p1, const vector2& p2, vector2& intercept); // 0x00574060 (:3519)
bool ix_box_box_2d_sweep(const vector2& box0_pos, const vector2& box0_extents, const matrix& orient0, const vector2& velocity0, const vector2& box1_pos, const vector2& box1_extents, const matrix& orient1, const vector2& velocity1, float lookahead_time, float& intersect_time, vector2& intersect, vector2& intersect_normal); // 0x005742C0 (:3605)

// 0x0056E870: nothing to set
inline ix_collision::ix_collision()
{
}
