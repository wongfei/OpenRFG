// Original: volition/vlib/code/math/ix.cpp
//
// Partial: the axis aligned box, point (also in an oriented box) and cylinder tests the world streaming uses, box/box intersection, the
// separating axis box tests (ix_box_box, ix_oriented_box_box), sphere/box, segment/box and segment/sphere (P1); the ray tests the
// weapons use (facing, closest point on a ray, ray/sphere and more; multiplayer.md MP8); the 2D segment tests
// ix2d_linesegment_line/_linesegment (stub_impl.md S3). The
// rest is not implemented yet.

#include "volition/vlib/code/math/ix.h"

#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/bbox2d.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/plane.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/render_lib/code/rl_utility.h"

#include <math.h>

// 0x0056D140 (:181)
bool ix_linesegment_plane(ix_collision& collision, const vector& start, const vector& dir, const plane& p)
{
    const float denom = -(p.normal.x * dir.x + p.normal.y * dir.y + dir.z * p.normal.z);
    if (denom != 0.0f) {
        const float fraction = (start.x * p.normal.x + start.y * p.normal.y + start.z * p.normal.z + p.offset) / denom;
        collision.fraction = fraction;
        if (0.0f <= fraction && fraction <= 1.0f) {
            collision.hit_point.x = start.x + dir.x * fraction;
            collision.hit_point.y = start.y + fraction * dir.y;
            collision.hit_point.z = start.z + dir.z * fraction;
            return true;
        }
    }
    return false;
}

// 0x00570C50 (:1915)
// The point where the line meets the plane; false when the line is parallel to it (VFX.md FX7).
bool ix_line_plane(vector& ix, const line& l, const plane& p)
{
    const float denom = l.dir.x * p.normal.x + l.dir.y * p.normal.y + l.dir.z * p.normal.z;
    if (denom == 0.0f)
    {
        return false;
    }
    const float t = -(l.point.x * p.normal.x + l.point.y * p.normal.y + l.point.z * p.normal.z + p.offset) / denom;
    ix.x = l.point.x + l.dir.x * t;
    ix.y = l.point.y + l.dir.y * t;
    ix.z = l.point.z + l.dir.z * t;
    return true;
}

// 0x0056E880 (:1123)
bool ix_box_box_aligned(const vector& min1, const vector& max1, const vector& min2, const vector& max2)
{
    return min1.x <= max2.x && min1.y <= max2.y && min1.z <= max2.z && min2.x <= max1.x && min2.y <= max1.y && min2.z <= max1.z;
}

// 0x0056E920 (:1138)
bool ix_box_box_aligned_xz(const vector& min1, const vector& max1, const vector& min2, const vector& max2)
{
    return min1.x <= max2.x && min1.z <= max2.z && min2.x <= max1.x && min2.z <= max1.z;
}

// 0x0056EB20 (:1176)
bool ix_point_in_box(const vector& pnt, const vector& mn, const vector& mx)
{
    return mn.x <= pnt.x && pnt.x <= mx.x && mn.y <= pnt.y && pnt.y <= mx.y && mn.z <= pnt.z && pnt.z <= mx.z;
}

// 0x0056ECF0 (:1347)
// The squared distance summed in double, compared with the squared radius in double.
bool ix_point_in_sphere(const vector& sphere_center, float sphere_radius, const vector& point)
{
    const float dx = point.x - sphere_center.x;
    const float dy = point.y - sphere_center.y;
    const float dz = point.z - sphere_center.z;
    const double dist_sq = ((double)dx * (double)dx + (double)dy * (double)dy) + (double)dz * (double)dz;
    return dist_sq <= (double)sphere_radius * (double)sphere_radius;
}

// 0x0059C950 (:1193)
// The point in the box's frame (each axis dot product summed in double, then rounded), tested against bmin/bmax.
bool ix_point_in_box_oriented(const vector& pnt, const vector& origin, const matrix& orient, const vector& bmin, const vector& bmax)
{
    const float dx = pnt.x - origin.x;
    const float dy = pnt.y - origin.y;
    const float dz = pnt.z - origin.z;
    const float x = (float)(((double)orient.rvec.y * (double)dy + (double)orient.rvec.x * (double)dx) + (double)orient.rvec.z * (double)dz);
    const float y = (float)(((double)orient.uvec.x * (double)dx + (double)orient.uvec.y * (double)dy) + (double)orient.uvec.z * (double)dz);
    const float z = (float)(((double)orient.fvec.x * (double)dx + (double)orient.fvec.y * (double)dy) + (double)orient.fvec.z * (double)dz);
    return bmin.x <= x && x <= bmax.x && bmin.y <= y && y <= bmax.y && bmin.z <= z && z <= bmax.z;
}

// 0x0056FB20 (:1669)
bool ix_box_cylinder_xz(const vector& bmin, const vector& bmax, const vector& cylinder_pos, float cylinder_radius)
{
    // Squared distance in xz from the cylinder axis to the box.
    float dist_sq = 0.0f;
    if (cylinder_pos.x < bmin.x)
    {
        dist_sq += (cylinder_pos.x - bmin.x) * (cylinder_pos.x - bmin.x);
    }
    else if (bmax.x < cylinder_pos.x)
    {
        dist_sq += (cylinder_pos.x - bmax.x) * (cylinder_pos.x - bmax.x);
    }
    if (cylinder_pos.z < bmin.z)
    {
        dist_sq += (cylinder_pos.z - bmin.z) * (cylinder_pos.z - bmin.z);
    }
    else if (bmax.z < cylinder_pos.z)
    {
        dist_sq += (cylinder_pos.z - bmax.z) * (cylinder_pos.z - bmax.z);
    }
    return dist_sq <= cylinder_radius * cylinder_radius;
}

// 0x0056E990 (:1154)
// Overlap test with the overlap box (touching boxes overlap).
bool ix_box_box_intersect(const vector& min1, const vector& max1, const vector& min2, const vector& max2, vector& out_min, vector& out_max)
{
    if (!(min1.x <= max2.x && min1.y <= max2.y && min1.z <= max2.z && min2.x <= max1.x && min2.y <= max1.y && min2.z <= max1.z))
    {
        return false;
    }
    out_min.y = min1.y - min2.y < 0.0f ? min2.y : min1.y;
    out_min.x = min1.x - min2.x < 0.0f ? min2.x : min1.x;
    out_min.z = min1.z - min2.z < 0.0f ? min2.z : min1.z;
    out_max.x = 0.0f <= max1.x - max2.x ? max2.x : max1.x;
    out_max.y = 0.0f <= max1.y - max2.y ? max2.y : max1.y;
    out_max.z = 0.0f <= max1.z - max2.z ? max2.z : max1.z;
    return true;
}

// 0x0056EBB0 (:1216)
// Sphere against a box (the squared distance to the box, with an early out per axis).
bool ix_sphere_in_box(const vector& sphere_center, float sphere_radius, const vector& box_mn, const vector& box_mx)
{
    const float r2 = sphere_radius * sphere_radius;
    float d = 0.0f;
    const float c[3] = {sphere_center.x, sphere_center.y, sphere_center.z};
    const float lo[3] = {box_mn.x, box_mn.y, box_mn.z};
    const float hi[3] = {box_mx.x, box_mx.y, box_mx.z};
    for (int k = 0; k < 3; k++)
    {
        if (c[k] < lo[k])
        {
            d = (c[k] - lo[k]) * (c[k] - lo[k]) + d;
        }
        else if (hi[k] < c[k])
        {
            d = (c[k] - hi[k]) * (c[k] - hi[k]) + d;
        }
        if (r2 < d)
        {
            return false;
        }
    }
    return true;
}

// 0x0056FBF0 (:1707)
// Separating axis test of box a (half extents a at the origin, axis aligned) and box b (half extents b, rotated by
// b_orient, centred at b_trans in a's frame). Returns 0 when they overlap, otherwise the number (1-15) of the first
// separating axis found. The rotation terms get 1e-6 added against parallel edges.
int ix_box_box(const matrix& b_orient, const vector& b_trans, const vector& a, const vector& b)
{
    const float r00 = b_orient.rvec.x;
    const float r01 = b_orient.rvec.y;
    const float r02 = b_orient.rvec.z;
    const float r10 = b_orient.uvec.x;
    const float r11 = b_orient.uvec.y;
    const float r12 = b_orient.uvec.z;
    const float r20 = b_orient.fvec.x;
    const float r21 = b_orient.fvec.y;
    const float r22 = b_orient.fvec.z;
    const float a00 = fabsf(r00) + 1e-06f;
    const float a01 = fabsf(r01) + 1e-06f;
    const float a02 = fabsf(r02) + 1e-06f;
    const float a10 = fabsf(r10) + 1e-06f;
    const float a11 = fabsf(r11) + 1e-06f;
    const float a12 = fabsf(r12) + 1e-06f;
    const float a20 = fabsf(r20) + 1e-06f;
    const float a21 = fabsf(r21) + 1e-06f;
    const float a22 = fabsf(r22) + 1e-06f;
    const float tx = b_trans.x;
    const float ty = b_trans.y;
    const float tz = b_trans.z;
    if (b.x * a00 + a.x + b.y * a01 + b.z * a02 < fabsf(tx))
    {
        return 1;
    }
    if (!(fabsf(ty * r10 + tx * r00 + tz * r20) <= a.x * a00 + b.x + a.y * a10 + a.z * a20))
    {
        return 2;
    }
    if (!(fabsf(ty) <= b.x * a10 + a.y + b.y * a11 + b.z * a12))
    {
        return 3;
    }
    if (!(fabsf(tz) <= b.x * a20 + a.z + b.y * a21 + b.z * a22))
    {
        return 4;
    }
    if (!(fabsf(ty * r11 + tx * r01 + tz * r21) <= a.x * a01 + b.y + a.y * a11 + a.z * a21))
    {
        return 5;
    }
    if (!(fabsf(ty * r12 + tx * r02 + tz * r22) <= a.x * a02 + b.z + a.y * a12 + a.z * a22))
    {
        return 6;
    }
    if (!(fabsf(tz * r10 - ty * r20) <= a.z * a10 + a.y * a20 + b.y * a02 + b.z * a01))
    {
        return 7;
    }
    if (!(fabsf(tz * r11 - ty * r21) <= a.z * a11 + a.y * a21 + b.x * a02 + b.z * a00))
    {
        return 8;
    }
    if (!(fabsf(tz * r12 - ty * r22) <= a.z * a12 + a.y * a22 + b.x * a01 + b.y * a00))
    {
        return 9;
    }
    if (!(fabsf(tx * r20 - tz * r00) <= a.z * a00 + a.x * a20 + b.y * a12 + b.z * a11))
    {
        return 10;
    }
    if (!(fabsf(tx * r21 - tz * r01) <= a.z * a01 + a.x * a21 + b.x * a12 + b.z * a10))
    {
        return 11;
    }
    if (!(fabsf(tx * r22 - tz * r02) <= a.z * a02 + a.x * a22 + b.x * a11 + b.y * a10))
    {
        return 12;
    }
    if (!(fabsf(ty * r00 - tx * r10) <= a.y * a00 + a.x * a10 + b.y * a22 + b.z * a21))
    {
        return 13;
    }
    if (!(fabsf(ty * r01 - tx * r11) <= a.y * a01 + a.x * a11 + b.x * a22 + b.z * a20))
    {
        return 14;
    }
    if (a.y * a02 + a.x * a12 + b.x * a21 + b.y * a20 < fabsf(ty * r02 - tx * r12))
    {
        return 15;
    }
    return 0;
}

// 0x00571C50 (:2590)
// Whether the segment p1-p2 touches the box; intercept is the entry point (an end point that is inside, or the first
// face crossing found by outcode).
bool ix_linesegment_boundingbox(vector& intercept, const vector& bbox_min, const vector& bbox_max, const vector& p1, const vector& p2)
{
    unsigned int code1 = 0;
    if (bbox_max.x < p1.x)
    {
        code1 = 1;
    }
    else if (p1.x < bbox_min.x)
    {
        code1 = 2;
    }
    if (bbox_max.y < p1.y)
    {
        code1 |= 4;
    }
    else if (p1.y < bbox_min.y)
    {
        code1 |= 8;
    }
    if (bbox_max.z < p1.z)
    {
        code1 |= 0x20;
    }
    else if (p1.z < bbox_min.z)
    {
        code1 |= 0x10;
    }
    if (code1 == 0)
    {
        intercept = p1;
        return true;
    }
    unsigned int code2 = 0;
    if (bbox_max.x < p2.x)
    {
        code2 = 1;
    }
    else if (p2.x < bbox_min.x)
    {
        code2 = 2;
    }
    if (bbox_max.y < p2.y)
    {
        code2 |= 4;
    }
    else if (p2.y < bbox_min.y)
    {
        code2 |= 8;
    }
    if (bbox_max.z < p2.z)
    {
        code2 |= 0x20;
    }
    else if (p2.z < bbox_min.z)
    {
        code2 |= 0x10;
    }
    if (code2 == 0)
    {
        intercept = p2;
        return true;
    }
    if ((code1 & code2) != 0)
    {
        return false;
    }
    if ((code1 & 3) != 0)
    {
        intercept.x = (code1 & 1) == 0 ? bbox_min.x : bbox_max.x;
        const float t = intercept.x - p1.x;
        const float d = p2.x - p1.x;
        intercept.y = ((p2.y - p1.y) * t) / d + p1.y;
        intercept.z = ((p2.z - p1.z) * t) / d + p1.z;
        if (intercept.y <= bbox_max.y && bbox_min.y <= intercept.y && intercept.z <= bbox_max.z && bbox_min.z <= intercept.z)
        {
            return true;
        }
    }
    if ((code1 & 0xC) != 0)
    {
        intercept.y = (code1 & 4) == 0 ? bbox_min.y : bbox_max.y;
        const float t = intercept.y - p1.y;
        const float d = p2.y - p1.y;
        intercept.x = ((p2.x - p1.x) * t) / d + p1.x;
        intercept.z = ((p2.z - p1.z) * t) / d + p1.z;
        if (intercept.x <= bbox_max.x && bbox_min.x <= intercept.x && intercept.z <= bbox_max.z && bbox_min.z <= intercept.z)
        {
            return true;
        }
    }
    if ((code1 & 0x30) != 0)
    {
        intercept.z = (code1 & 0x20) == 0 ? bbox_min.z : bbox_max.z;
        const float t = intercept.z - p1.z;
        const float d = p2.z - p1.z;
        intercept.x = ((p2.x - p1.x) * t) / d + p1.x;
        intercept.y = ((p2.y - p1.y) * t) / d + p1.y;
        if (intercept.x <= bbox_max.x && bbox_min.x <= intercept.x && intercept.y <= bbox_max.y && bbox_min.y <= intercept.y)
        {
            return true;
        }
    }
    return false;
}

// 0x0056EF30 (:1558)
// Separating axis test of two oriented boxes (half extents, centres, orientations): the 3 face axes of each box, then
// the 9 edge cross products. B's axes are expressed in A's frame; 1e-6 is added to the absolute rotation terms so
// near-parallel edges do not produce false separations.
bool ix_oriented_box_box(const vector& _extents_a, const vector& bbox_pos_a, const matrix& bbox_orient_a, const vector& _extents_b, const vector& bbox_pos_b, const matrix& bbox_orient_b)
{
    const vector* axes_a = &bbox_orient_a.rvec;
    const vector* axes_b = &bbox_orient_b.rvec;
    const float a[3] = { _extents_a.x, _extents_a.y, _extents_a.z };
    const float b[3] = { _extents_b.x, _extents_b.y, _extents_b.z };
    const vector d(bbox_pos_b.x - bbox_pos_a.x, bbox_pos_b.y - bbox_pos_a.y, bbox_pos_b.z - bbox_pos_a.z);
    const float t[3] = { d.x * axes_a[0].x + d.y * axes_a[0].y + d.z * axes_a[0].z,
                         d.x * axes_a[1].x + d.y * axes_a[1].y + d.z * axes_a[1].z,
                         d.x * axes_a[2].x + d.y * axes_a[2].y + d.z * axes_a[2].z };
    float r[3][3];
    float abs_r[3][3];
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            r[i][j] = axes_a[i].y * axes_b[j].y + axes_a[i].x * axes_b[j].x + axes_a[i].z * axes_b[j].z;
            abs_r[i][j] = fabsf(r[i][j]) + 1e-06f;
        }
    }
    // A's face axes.
    for (int i = 0; i < 3; i++)
    {
        if (abs_r[i][1] * b[1] + abs_r[i][0] * b[0] + abs_r[i][2] * b[2] + a[i] < fabsf(t[i]))
        {
            return false;
        }
    }
    // B's face axes.
    for (int j = 0; j < 3; j++)
    {
        if (abs_r[1][j] * a[1] + abs_r[0][j] * a[0] + abs_r[2][j] * a[2] + b[j] < fabsf(r[1][j] * t[1] + r[0][j] * t[0] + r[2][j] * t[2]))
        {
            return false;
        }
    }
    // A0 x B0 .. A2 x B2.
    return fabsf(r[1][0] * t[2] - r[2][0] * t[1]) <= abs_r[0][1] * b[2] + abs_r[0][2] * b[1] + abs_r[1][0] * a[2] + abs_r[2][0] * a[1] &&
           fabsf(r[1][1] * t[2] - r[2][1] * t[1]) <= abs_r[0][0] * b[2] + abs_r[0][2] * b[0] + abs_r[1][1] * a[2] + abs_r[2][1] * a[1] &&
           fabsf(r[1][2] * t[2] - r[2][2] * t[1]) <= abs_r[0][0] * b[1] + abs_r[0][1] * b[0] + abs_r[1][2] * a[2] + abs_r[2][2] * a[1] &&
           fabsf(r[2][0] * t[0] - r[0][0] * t[2]) <= abs_r[1][2] * b[1] + abs_r[1][1] * b[2] + abs_r[0][0] * a[2] + abs_r[2][0] * a[0] &&
           fabsf(r[2][1] * t[0] - r[0][1] * t[2]) <= abs_r[1][2] * b[0] + abs_r[1][0] * b[2] + abs_r[0][1] * a[2] + abs_r[2][1] * a[0] &&
           fabsf(r[2][2] * t[0] - r[0][2] * t[2]) <= abs_r[1][1] * b[0] + abs_r[1][0] * b[1] + abs_r[2][2] * a[0] + abs_r[0][2] * a[2] &&
           fabsf(r[0][0] * t[1] - r[1][0] * t[0]) <= abs_r[2][2] * b[1] + abs_r[2][1] * b[2] + abs_r[0][0] * a[1] + abs_r[1][0] * a[0] &&
           fabsf(r[0][1] * t[1] - r[1][1] * t[0]) <= abs_r[2][2] * b[0] + abs_r[2][0] * b[2] + abs_r[1][1] * a[0] + abs_r[0][1] * a[1] &&
           fabsf(r[0][2] * t[1] - r[1][2] * t[0]) <= abs_r[2][1] * b[0] + abs_r[2][0] * b[1] + abs_r[1][2] * a[0] + abs_r[0][2] * a[1];
}

// 0x00572590 (:2781)
// The point of the segment closest to pnt; returns its distance from p0 along the segment.
float ix_closest_point_on_line_segment(vector& ix, const vector& pnt, const vector& p0, const vector& p1)
{
    const vector delta = p1 - p0;
    const float len = sqrtf(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    if (len == 0.0f)
    {
        ix = p0;
        return 0.0f;
    }
    const vector dir(delta.x / len, delta.y / len, delta.z / len);
    float t = (pnt.y - p0.y) * dir.y + (pnt.x - p0.x) * dir.x + (pnt.z - p0.z) * dir.z;
    if (t - 0.0f < 0.0f)
    {
        t = 0.0f;
    }
    if (t - len >= 0.0f)
    {
        t = len;
    }
    ix.x = p0.x + dir.x * t;
    ix.y = p0.y + dir.y * t;
    ix.z = p0.z + dir.z * t;
    return t;
}

// Whether a point (relative to the apex) is inside the cone; a negative cone_dot is a cone wider than a hemisphere
// (inlined twice in ix_line_segment_in_cone).
static bool ix_point_in_cone(const vector& rel, const vector& cone_fvec, float cone_dot)
{
    const float len_sq = rel.y * rel.y + rel.x * rel.x + rel.z * rel.z;
    if (0.0f <= cone_dot)
    {
        const float d = cone_fvec.x * rel.x + rel.y * cone_fvec.y + rel.z * cone_fvec.z;
        return 0.0f <= d && len_sq * cone_dot * cone_dot <= d * d;
    }
    const float d = -cone_fvec.x * rel.x + -cone_fvec.y * rel.y + -cone_fvec.z * rel.z;
    return d < 0.0f || d * d < len_sq * cone_dot * cone_dot;
}

// 0x0059D360 (:1391)
// Either end inside the cone, or the segment crossing the cone's surface (on the cone_dot side of the apex) between
// its ends.
bool ix_line_segment_in_cone(const vector& start, const vector& end, const vector& cone_pos, const vector& cone_fvec, float cone_dot)
{
    vector dir = end - start;
    float len = sqrtf(dir.y * dir.y + dir.x * dir.x + dir.z * dir.z);
    if (1e-07f <= len - 0.0f || 1e-07f <= -(len - 0.0f))
    {
        const float inv = 1.0f / len;
        dir.x *= inv;
        dir.y *= inv;
        dir.z *= inv;
    }
    else
    {
        dir.set(1.0f, 0.0f, 0.0f);
        len = 0.0f;
    }
    const vector rel_start = start - cone_pos;
    if (ix_point_in_cone(rel_start, cone_fvec, cone_dot)) // inlined
    {
        return true;
    }
    if (ix_point_in_cone(end - cone_pos, cone_fvec, cone_dot)) // inlined
    {
        return true;
    }
    // |rel + dir * t| cone_dot == (rel + dir * t) . fvec, squared: a t^2 + b t + c == 0
    const float rel_dot_fvec = cone_fvec.x * rel_start.x + cone_fvec.y * rel_start.y + cone_fvec.z * rel_start.z;
    const float dir_dot_fvec = cone_fvec.x * dir.x + cone_fvec.y * dir.y + cone_fvec.z * dir.z;
    const float dot_sq = cone_dot * cone_dot;
    const float a = dir_dot_fvec * dir_dot_fvec - (dir.y * dir.y + dir.x * dir.x + dir.z * dir.z) * dot_sq;
    const float b = (dir_dot_fvec * rel_dot_fvec - (rel_start.x * dir.x + rel_start.y * dir.y + rel_start.z * dir.z) * dot_sq) * 2.0f;
    const float c = (float)((double)rel_dot_fvec * (double)rel_dot_fvec - (double)(rel_start.x * rel_start.x + rel_start.y * rel_start.y + rel_start.z * rel_start.z) * (double)dot_sq);
    vector hit;
    if (1.1920929e-07f <= a - 0.0f || 1.1920929e-07f <= -(a - 0.0f)) // FLT_EPSILON
    {
        float disc = b * b - a * 4.0f * c;
        if (disc < 0.0f)
        {
            return false;
        }
        disc = sqrtf(disc);
        const float t0 = (b * -1.0f - disc) / (a * 2.0f);
        const float t1 = (disc - b) / (a * 2.0f);
        hit.set(dir.x * t1 + rel_start.x, dir.y * t1 + rel_start.y, dir.z * t1 + rel_start.z);
        if (0.0f < t1 && t1 < len && 0.0f <= (hit * cone_fvec) * cone_dot)
        {
            return true;
        }
        hit.set(dir.x * t0 + rel_start.x, dir.y * t0 + rel_start.y, dir.z * t0 + rel_start.z);
        if (t0 <= 0.0f || len <= t0)
        {
            return false;
        }
    }
    else
    {
        if (b - 0.0f < 1.1920929e-07f && -(b - 0.0f) < 1.1920929e-07f)
        {
            // parallel to the surface: inside only when on it
            return c - 0.0f < 1.1920929e-07f && -(c - 0.0f) < 1.1920929e-07f;
        }
        const float t = (c * -1.0f) / b;
        hit.set(dir.x * t + rel_start.x, dir.y * t + rel_start.y, dir.z * t + rel_start.z);
        if (t <= 0.0f || len <= t)
        {
            return false;
        }
    }
    return 0.0f <= (hit * cone_fvec) * cone_dot;
}

// 0x0059DEB0 (:1469)
// The apex inside the box, or any silhouette edge (seen from the apex) facing the cone axis crossing the cone; true
// as well when no edge faces it (the box surrounds the axis).
bool ix_box_in_cone(const vector& box_mn, const vector& box_mx, const vector& cone_pos, const vector& cone_fvec, float cone_dot)
{
    if (box_mn.x <= cone_pos.x && cone_pos.x <= box_mx.x && box_mn.y <= cone_pos.y && cone_pos.y <= box_mx.y && box_mn.z <= cone_pos.z && cone_pos.z <= box_mx.z)
    {
        return true;
    }
    const vector rel_mn = box_mn - cone_pos;
    const vector rel_mx = box_mx - cone_pos;
    vector silhouette[6];
    const int num = bbox_find_silhouette(silhouette, ZERO_VECTOR, rel_mn, rel_mx);
    bool surrounds = true;
    int prev = num - 1;
    for (int i = 0; i < num; i++)
    {
        const vector& p0 = silhouette[prev];
        const vector& p1 = silhouette[i];
        // (p1 x p0) . fvec
        if (cone_fvec.y * (p1.z * p0.x - p1.x * p0.z) + cone_fvec.x * (p1.y * p0.z - p1.z * p0.y) + cone_fvec.z * (p1.x * p0.y - p1.y * p0.x) < 0.0f)
        {
            surrounds = false;
            if (ix_line_segment_in_cone(p0, p1, ZERO_VECTOR, cone_fvec, cone_dot))
            {
                return true;
            }
        }
        prev = i;
    }
    return surrounds;
}

// 0x0059BD20 (:811)
bool ix_sphere_sphere(const vector& a_center, const vector& b_center, float a_radius, float b_radius)
{
    const float dx = a_center.x - b_center.x;
    const float dy = a_center.y - b_center.y;
    const float dz = a_center.z - b_center.z;
    return dz * dz + dy * dy + dx * dx < (a_radius + b_radius) * (a_radius + b_radius);
}

// 0x0059B8A0 (:374)
// The first point of the segment p0-p1 inside the sphere (p0 when p0 is inside, or for a zero length segment when p0
// is inside). The squared lengths are summed in double and every other step rounds to float (the source's double
// literals); the end test adds the length and the radius in double.
bool ix_linesegment_sphere(vector& intp, const vector& p0, const vector& p1, const vector& sphere_pos, float sphere_rad)
{
    const float dx = p1.x - p0.x;
    const float dy = p1.y - p0.y;
    const float dz = p1.z - p0.z;
    const float sx = sphere_pos.x - p0.x;
    const float sy = sphere_pos.y - p0.y;
    const float sz = sphere_pos.z - p0.z;
    const float len = (float)sqrt((double)(float)(((double)dx * (double)dx + (double)dy * (double)dy) + (double)dz * (double)dz));
    if (!(0.0f < len))
    {
        intp = p0;
        const float dist = (float)sqrt((double)(float)(((double)sx * (double)sx + (double)sy * (double)sy) + (double)sz * (double)sz));
        return sphere_rad > dist;
    }
    const float nx = (float)((double)dx / (double)len);
    const float ny = (float)((double)dy / (double)len);
    const float nz = (float)((double)dz / (double)len);
    const float t = (float)(((double)sy * (double)ny + (double)sx * (double)nx) + (double)sz * (double)nz);
    if (-sphere_rad > t || (double)t > (double)len + (double)sphere_rad)
    {
        return false;
    }
    const float cx = (p0.x + t * nx) - sphere_pos.x;
    const float cy = (p0.y + t * ny) - sphere_pos.y;
    const float cz = (p0.z + t * nz) - sphere_pos.z;
    const float dist = (float)sqrt((double)(float)(((double)cx * (double)cx + (double)cy * (double)cy) + (double)cz * (double)cz));
    if (!(sphere_rad > dist))
    {
        return false;
    }
    const float t2 = t - (float)sqrt((double)(sphere_rad * sphere_rad - dist * dist));
    if (t2 > len || 0.0f > t2)
    {
        intp = p0;
        return true;
    }
    intp.x = p0.x + nx * t2;
    intp.y = p0.y + ny * t2;
    intp.z = p0.z + nz * t2;
    return true;
}

// 0x0059CB00 (:1273)
// All eight corners strictly inside the sphere (corners rebuilt from the box center and half extents).
bool ix_box_completely_in_sphere(const vector& sphere_center, float sphere_radius, const vector& box_mn, const vector& box_mx)
{
    const float cz = (box_mn.z + box_mx.z) * 0.5f;
    const float cx = (box_mx.x + box_mn.x) * 0.5f;
    const float cy = (box_mn.y + box_mx.y) * 0.5f;
    const float hx = box_mx.x - cx;
    const float hy = box_mx.y - cy;
    const float hz = box_mx.z - cz;
    const float r2 = sphere_radius * sphere_radius;
    const float xs[2] = {hx + cx, cx - hx};
    const float ys[2] = {hy + cy, cy - hy};
    const float zs[2] = {hz + cz, cz - hz};
    for (int iz = 0; iz < 2; iz++)
    {
        const float dz = sphere_center.z - zs[iz];
        for (int iy = 0; iy < 2; iy++)
        {
            const float dy = sphere_center.y - ys[iy];
            for (int ix = 0; ix < 2; ix++)
            {
                const float dx = sphere_center.x - xs[ix];
                if (!(dz * dz + dy * dy + dx * dx < r2))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

// ---- ray tests of the weapons (multiplayer.md MP8)

// 0x0056D0A0 (:157): the point is on the side of the plane the normal points to.
bool ix_is_normal_facing(const vector& pos, const vector& norm, const vector& plane_point)
{
    return 0.0f < norm.y * (pos.y - plane_point.y) + norm.x * (pos.x - plane_point.x) + norm.z * (pos.z - plane_point.z);
}

// 0x00572460 (:2750): the closest point on a ray (unbounded both ways); returns its parameter along the direction.
float ix_closest_point_on_ray(const vector& pnt, const line& l, vector& ix)
{
    const float t = l.dir.x * (pnt.x - l.point.x) + l.dir.y * (pnt.y - l.point.y) + l.dir.z * (pnt.z - l.point.z);
    ix.x = l.point.x + l.dir.x * t;
    ix.y = l.point.y + l.dir.y * t;
    ix.z = l.point.z + l.dir.z * t;
    return t;
}

// 0x00572D00 (:2942): the closest points of two segments, s along p1-q1 and t along p2-q2. A segment under the margin counts as its first point.
void ix_closest_linesegment_linesegment(float& s, float& t, vector& ix1, vector& ix2, const vector& p1, const vector& q1, const vector& p2, const vector& q2)
{
    // Magic number: the degenerate margin (a double constant).
    static const double MARGIN = 0.0001;
    const vector d1 = q1 - p1;
    const vector d2 = q2 - p2;
    const vector r = p1 - p2;
    const float a = d1.x * d1.x + d1.y * d1.y + d1.z * d1.z;
    const float e = d2.x * d2.x + d2.y * d2.y + d2.z * d2.z;
    const float f = r.y * d2.y + r.x * d2.x + r.z * d2.z;
    const auto clamp01 = [](float value)
    {
        if (value - 0.0f < 0.0f)
        {
            value = 0.0f;
        }
        if (0.0f <= value - 1.0f)
        {
            value = 1.0f;
        }
        return value;
    };
    if (MARGIN < a)
    {
        const float c = r.y * d1.y + r.x * d1.x + r.z * d1.z;
        if (MARGIN < e)
        {
            const float b = d2.y * d1.y + d2.x * d1.x + d2.z * d1.z;
            const float denom = e * a - b * b;
            s = denom == 0.0f ? 0.0f : clamp01((b * f - c * e) / denom);
            t = (b * s + f) / e;
            if (t < 0.0f)
            {
                t = 0.0f;
                s = clamp01(-c / a);
            }
            else if (1.0f < t)
            {
                t = 1.0f;
                s = clamp01((b - c) / a);
            }
        }
        else
        {
            t = 0.0f;
            s = clamp01(-c / a);
        }
    }
    else if (MARGIN < e)
    {
        s = 0.0f;
        t = clamp01(f / e);
    }
    else
    {
        t = 0.0f;
        s = 0.0f;
        ix1 = p1;
        ix2 = p2;
        return;
    }
    ix1.x = p1.x + s * d1.x;
    ix1.y = d1.y * s + p1.y;
    ix1.z = p1.z + d1.z * s;
    ix2.x = p2.x + t * d2.x;
    ix2.y = d2.y * t + p2.y;
    ix2.z = p2.z + d2.z * t;
}

// 0x00572810 (:2877)
// The closest points of the lines p1-p2 and p3-p4 (VFX.md FX8, the triangulation): fractions along each segment and the
// points. False when a segment is shorter than 0.0001 on every axis or the lines are parallel (|denominator| < 0.0001).
// The dot products are summed in double and rounded, as rfg.exe does.
bool ix_closest_line_line(ix_collision& col_a, ix_collision& col_b, const vector& p1, const vector& p2, const vector& p3, const vector& p4)
{
    // Magic number: the degenerate margin (a double constant).
    const double eps = 0.0001;
    const float d13x = p1.x - p3.x;
    const float d13y = p1.y - p3.y;
    const float d13z = p1.z - p3.z;
    const float d43x = p4.x - p3.x;
    const float d43y = p4.y - p3.y;
    const float d43z = p4.z - p3.z;
    if (eps > (double)fabsf(d43x) && eps > (double)fabsf(d43y) && eps > (double)fabsf(d43z))
    {
        return false;
    }
    const float d21x = p2.x - p1.x;
    const float d21y = p2.y - p1.y;
    const float d21z = p2.z - p1.z;
    if (eps > (double)fabsf(d21x) && eps > (double)fabsf(d21y) && eps > (double)fabsf(d21z))
    {
        return false;
    }
    const float d1343 = (float)((double)d43x * d13x + (double)d43y * d13y + (double)d43z * d13z);
    const float d4321 = (float)((double)d21y * d43y + (double)d21x * d43x + (double)d21z * d43z);
    const float d4343 = (float)((double)d43y * d43y + (double)d43x * d43x + (double)d43z * d43z);
    const float d2121 = (float)((double)d21y * d21y + (double)d21x * d21x + (double)d21z * d21z);
    const float denom = (float)((double)d2121 * d4343 - (double)d4321 * d4321);
    if (eps > (double)fabsf(denom))
    {
        return false;
    }
    const float d1321 = (float)((double)d21y * d13y + (double)d21x * d13x + (double)d21z * d13z);
    const float numer = (float)((double)d4321 * d1343 - (double)d1321 * d4343);
    col_a.fraction = (float)((double)numer / (double)denom);
    col_b.fraction = (float)(((double)col_a.fraction * d4321 + d1343) / (double)d4343);
    const float mua = col_a.fraction;
    col_a.hit_point.set(p1.x + d21x * mua, p1.y + d21y * mua, p1.z + d21z * mua);
    const float mub = col_b.fraction;
    col_b.hit_point.set(p3.x + d43x * mub, p3.y + d43y * mub, p3.z + d43z * mub);
    return true;
}

// 0x005720F0 (:2675): a ray starting inside the sphere hits at its start; else the entry point in front of the start.
bool ix_ray_sphere(ix_collision& collision, const line& ray, const vector& sphere_pos, float sphere_rad)
{
    const float dx = sphere_pos.x - ray.point.x;
    const float dy = sphere_pos.y - ray.point.y;
    const float dz = sphere_pos.z - ray.point.z;
    const float dist_sq = dx * dx + dy * dy + dz * dz;
    if (dist_sq <= sphere_rad * sphere_rad)
    {
        collision.hit_point = ray.point;
        return true;
    }
    float t = dy * ray.dir.y + dx * ray.dir.x + dz * ray.dir.z;
    if (t < 0.0f)
    {
        return false;
    }
    const float h = sphere_rad * sphere_rad - (dist_sq - t * t);
    if (h < 0.0f)
    {
        return false;
    }
    t = t - sqrtf(h);
    collision.fraction = t;
    collision.hit_point = vector(ray.point.x + t * ray.dir.x, t * ray.dir.y + ray.point.y, ray.point.z + t * ray.dir.z);
    return true;
}

// 0x00571970 (:2508)
// The first point where a ray from p0 enters an axis aligned box (p0 itself when inside): the candidate planes facing
// the ray, the farthest of their hit distances, then the other coordinates checked against the box.
bool ix_ray_boundingbox(vector& ix, const vector& min, const vector& max, const vector& p0, const vector& pdir)
{
    const float* mn = &min.x;
    const float* mx = &max.x;
    const float* p = &p0.x;
    const float* d = &pdir.x;
    float* out = &ix.x;
    bool inside = true;
    bool middle[3];
    float candidate[3] = {0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 2; i++)
    {
        if (p[i] < mn[i])
        {
            middle[i] = false;
            candidate[i] = mn[i];
            inside = false;
        }
        else if (mx[i] < p[i])
        {
            middle[i] = false;
            candidate[i] = mx[i];
            inside = false;
        }
        else
        {
            middle[i] = true;
        }
    }
    if (p[2] < mn[2] || mx[2] < p[2])
    {
        middle[2] = false;
        candidate[2] = p[2] < mn[2] ? mn[2] : mx[2];
    }
    else
    {
        middle[2] = true;
        if (inside)
        {
            ix = p0;
            return true;
        }
    }
    float max_t[3] = {-1.0f, -1.0f, -1.0f};
    for (int i = 0; i < 3; i++)
    {
        if (!middle[i] && d[i] != 0.0f)
        {
            max_t[i] = (candidate[i] - p[i]) / d[i];
        }
    }
    int plane = max_t[0] < max_t[1] ? 1 : 0;
    if (max_t[plane] < max_t[2])
    {
        plane = 2;
    }
    const float t = max_t[plane];
    if (t < 0.0f)
    {
        return false;
    }
    for (int i = 0; i < 3; i++)
    {
        if (plane == i)
        {
            out[i] = candidate[i];
        }
        else
        {
            out[i] = d[i] * t + p[i];
            if (out[i] < mn[i] || mx[i] < out[i])
            {
                return false;
            }
        }
    }
    return true;
}

// 0x0059E870 (:2302)
// A line against a capped cylinder of the given length centred at base along the (unit) axis: the ray is taken into
// the cylinder's frame. Parallel rays hit the top cap; otherwise the hits on the caps and on the side (within the caps'
// range) are collected and the first one collected wins (original: not the nearest).
bool ix_line_cylinder(const vector& raybase, const vector& raycos, const vector& base, const vector& axis, float length, float radius, vector& intersection)
{
    const float ax = axis.x;
    const float ay = axis.y;
    const float az = axis.z;
    float ux;
    float uy;
    float uz;
    if (ax < ay)
    {
        const float inv = 1.0f / sqrtf(az * az + ay * ay);
        ux = 0.0f;
        uy = az * inv;
        uz = -ay * inv;
    }
    else
    {
        const float inv = 1.0f / sqrtf(az * az + ax * ax);
        ux = -az * inv;
        uy = 0.0f;
        uz = inv * ax;
    }
    const float vx = uz * ay - uy * az;
    const float vy = ux * az - uz * ax;
    const float vz = uy * ax - ux * ay;
    const float dx = raycos.x;
    float du = dx * ux + raycos.y * uy + raycos.z * uz;
    float dv = raycos.y * vy + dx * vx + raycos.z * vz;
    float da = dx * ax + raycos.y * ay + raycos.z * az;
    const float dlen = sqrtf(du * du + dv * dv + da * da);
    const float inv_dlen = 1.0f / dlen;
    du = inv_dlen * du;
    dv = inv_dlen * dv;
    da = inv_dlen * da;
    const float scale = 1.0f / dlen;
    const float rx = raybase.x - base.x;
    const float ry = raybase.y - base.y;
    const float rz = raybase.z - base.z;
    const float pu = ry * uy + rx * ux + rz * uz;
    const float pv = rx * vx + ry * vy + rz * vz;
    const float half = length * 0.5f;
    const float pa = ry * ay + rx * ax + rz * az;
    const float r2 = radius * radius;
    float hits[3];
    const auto hit_at = [&](float t) {
        intersection.x = raybase.x + dx * t;
        intersection.y = raybase.y + t * raycos.y;
        intersection.z = raybase.z + t * raycos.z;
    };
    if (1.0f <= fabsf(da))
    {
        if (r2 < pu * pu + pv * pv)
        {
            return false;
        }
        hit_at((scale / da) * (half - pa));
        return true;
    }
    if (fabsf(da) <= 1e-12f)
    {
        if (half < fabsf(pa))
        {
            return false;
        }
        const float a = du * du + dv * dv;
        const float b = dv * pv + du * pu;
        const float disc = b * b - ((pu * pu + pv * pv) - r2) * a;
        if (disc < 0.0f)
        {
            return false;
        }
        if (0.0f < disc)
        {
            hit_at((-b - sqrtf(disc)) * (scale / a));
            return true;
        }
        hit_at((-b * scale) / a);
        return true;
    }
    const float t_top = (half - pa) * (1.0f / da);
    const float top_u = t_top * du + pu;
    const float top_v = t_top * dv + pv;
    int count = 0;
    if (top_u * top_u + top_v * top_v <= r2)
    {
        hits[count++] = t_top * scale;
    }
    const float t_bottom = (-half - pa) * (1.0f / da);
    const float bottom_u = t_bottom * du + pu;
    const float bottom_v = t_bottom * dv + pv;
    if (bottom_u * bottom_u + bottom_v * bottom_v <= r2)
    {
        hits[count++] = t_bottom * scale;
    }
    if (count == 2)
    {
        hit_at(hits[0]);
        return true;
    }
    const float a = du * du + dv * dv;
    const float b = dv * pv + du * pu;
    const float disc = b * b - ((pu * pu + pv * pv) - r2) * a;
    if (disc < 0.0f)
    {
        return false;
    }
    const float lo = t_top <= t_bottom ? t_top : t_bottom;
    const float hi = t_top <= t_bottom ? t_bottom : t_top;
    if (disc <= 0.0f)
    {
        const float s = -b / a;
        if (lo <= s && s <= hi)
        {
            hits[count++] = s * scale;
        }
    }
    else
    {
        const float s1 = (-b - sqrtf(disc)) * (1.0f / a);
        if (lo <= s1 && s1 <= hi)
        {
            hits[count++] = s1 * scale;
        }
        if (count == 2)
        {
            hit_at(hits[0]);
            return true;
        }
        const float s2 = (sqrtf(disc) - b) * (1.0f / a);
        if (lo <= s2 && s2 <= hi)
        {
            hits[count++] = s2 * scale;
        }
    }
    if (count != 0)
    {
        hit_at(hits[0]);
    }
    return count != 0;
}

// v between the ends e0 and e1, in either order (each comparison in double).
static bool ix2d_between(float v, float e0, float e1)
{
    if ((double)e0 >= (double)v && (double)v >= (double)e1)
    {
        return true;
    }
    return !((double)v < (double)e0) && !((double)e1 < (double)v);
}

// 0x00573360 (:3182)
// Where the segment meets the (infinite) line; false when they are parallel or the point is off the segment. The slopes
// are rounded to float; every other step is in double, rounded where the source assigns a float.
bool ix2d_linesegment_line(const vector2& seg_pt_0, const vector2& seg_pt_1, const vector2& line_pt_0, const vector2& line_pt_1, vector2* ix)
{
    const float s0x = seg_pt_0.x;
    const float s0y = seg_pt_0.y;
    const float s1x = seg_pt_1.x;
    const float s1y = seg_pt_1.y;
    const float l0x = line_pt_0.x;
    const float l0y = line_pt_0.y;
    const float l1x = line_pt_1.x;
    const float seg_dx = (float)((double)s0x - (double)s1x);
    const float seg_slope = (float)(((double)s0y - (double)s1y) / (double)seg_dx);
    const float line_slope = (float)(((double)l0y - (double)line_pt_1.y) / ((double)l0x - (double)l1x));
    if ((double)seg_slope == (double)line_slope)
    {
        return false;
    }
    if (0.0003f > (float)fabs((double)seg_dx)) // magic number: below this the segment counts as vertical
    {
        if ((double)l0x == (double)l1x)
        {
            return false;
        }
        const float y = (float)(((double)s0x - (double)l0x) * (double)line_slope + (double)l0y);
        if (!ix2d_between(y, s0y, s1y))
        {
            return false;
        }
        if (ix != nullptr)
        {
            ix->y = y;
            ix->x = seg_pt_0.x;
        }
        return true;
    }
    if ((double)l0x == (double)l1x) // vertical line
    {
        if (seg_slope == 0.0f)
        {
            if (!ix2d_between(l0x, s0x, s1x))
            {
                return false;
            }
            if (ix != nullptr)
            {
                ix->x = l0x;
                ix->y = seg_pt_0.y;
            }
            return true;
        }
        const float y = (float)(((double)l0x - (double)s0x) * (double)seg_slope + (double)seg_pt_0.y);
        if (!ix2d_between(y, seg_pt_0.y, s1y))
        {
            return false;
        }
        if (ix != nullptr)
        {
            ix->y = y;
            ix->x = line_pt_0.x;
        }
        return true;
    }
    const float num = (float)(((double)s0x * (double)seg_slope + ((double)l0y - (double)s0y)) - (double)l0x * (double)line_slope);
    const float x = (float)((double)num / ((double)seg_slope - (double)line_slope));
    if (!ix2d_between(x, s0x, s1x))
    {
        return false;
    }
    const float y = (float)(((double)x - (double)s0x) * (double)seg_slope + (double)s0y);
    if (ix != nullptr)
    {
        ix->x = x;
        ix->y = y;
    }
    return true;
}

// 0x005736A0 (:3277)
// The segments' lines meet on segment a (ix2d_linesegment_line) and the point lies on segment b, checked along b's
// longer axis.
bool ix2d_linesegment_linesegment(const vector2& a0, const vector2& a1, const vector2& b0, const vector2& b1, vector2* ix)
{
    if (!ix2d_linesegment_line(a0, a1, b0, b1, ix))
    {
        return false;
    }
    // original: ix is dereferenced here without the null check ix2d_linesegment_line makes
    const float dx = (float)fabs((double)b0.x - (double)b1.x);
    const float dy = (float)fabs((double)b0.y - (double)b1.y);
    if ((double)dx > (double)dy)
    {
        return ix2d_between(ix->x, b0.x, b1.x);
    }
    return ix2d_between(ix->y, b0.y, b1.y);
}

// 0x00573860
bool ix2d_point_circle(const vector2& pt, const vector2& center, const float radius)
{
    const float dx = pt.x - center.x;
    const float dy = pt.y - center.y;
    return dy * dy + dx * dx <= radius * radius;
}

// 0x005722F0 (:2721)
bool ix2d_line_line(vector2& ix, const vector2& l1s, const vector2& l1e, const vector2& l2s, const vector2& l2e)
{
    float l2_dx = l2e.x - l2s.x;
    float l1_dx = l1e.x - l1s.x;
    float denominator = (l2e.y - l2s.y) * l1_dx - (l1e.y - l1s.y) * l2_dx;
    if (denominator == 0.0f) {
        return false;
    }
    float t = ((l1s.y - l2s.y) * l2_dx - (l2e.y - l2s.y) * (l1s.x - l2s.x)) / denominator;
    ix.x = l1s.x + l1_dx * t;
    ix.y = l1s.y + (l1e.y - l1s.y) * t;
    return true;
}

// 0x005738E0 (:3331)
bool ix2d_bbox_bbox(const bbox2d& b1, const bbox2d& b2)
{
    return b1.min.x <= b2.max.x && b2.min.x <= b1.max.x && b1.min.y <= b2.max.y && b2.min.y <= b1.max.y;
}

// 0x0059D080 (:1328)
bool ix_sphere_in_box_oriented(const vector& sphere_center, float sphere_radius, const vector& box_cen, const matrix& box_orient, const vector& box_size)
{
    float dx = sphere_center.x - box_cen.x;
    float dy = sphere_center.y - box_cen.y;
    float dz = sphere_center.z - box_cen.z;
    float local_x = box_orient.rvec.y * dy + box_orient.rvec.x * dx + box_orient.rvec.z * dz;
    float local_y = box_orient.uvec.y * dy + box_orient.uvec.x * dx + box_orient.uvec.z * dz;
    float local_z = box_orient.fvec.y * dy + box_orient.fvec.x * dx + box_orient.fvec.z * dz;
    float half_x = box_size.x * 0.5f;
    float half_y = box_size.y * 0.5f;
    float half_z = box_size.z * 0.5f;
    float radius_sq = sphere_radius * sphere_radius;
    float dist_sq = 0.0f;
    if (local_x < -half_x) {
        dist_sq = (local_x + half_x) * (local_x + half_x) + 0.0f;
    } else if (half_x < local_x) {
        dist_sq = (local_x - half_x) * (local_x - half_x) + 0.0f;
    }
    if (radius_sq < dist_sq) {
        return false;
    }
    if (local_y < -half_y) {
        dist_sq += (local_y + half_y) * (local_y + half_y);
    } else if (half_y < local_y) {
        dist_sq += (local_y - half_y) * (local_y - half_y);
    }
    if (radius_sq < dist_sq) {
        return false;
    }
    if (local_z < -half_z) {
        dist_sq += (local_z + half_z) * (local_z + half_z);
    } else if (half_z < local_z) {
        dist_sq += (local_z - half_z) * (local_z - half_z);
    }
    return dist_sq <= radius_sq;
}

// 0x005737A0 (:3301)
bool ix2d_point_bbox(const vector2& pt, const bbox2d& box)
{
    return pt.x <= box.max.x && box.min.x <= pt.x && pt.y <= box.max.y && box.min.y <= pt.y;
}

// 0x00573800 (:3310)
bool ix2d_point_bbox(float pt_x, float pt_y, const bbox2d& box)
{
    return pt_x <= box.max.x && box.min.x <= pt_x && pt_y <= box.max.y && box.min.y <= pt_y;
}
