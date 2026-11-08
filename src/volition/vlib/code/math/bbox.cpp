// Original: volition/vlib/code/math/bbox.cpp
//
// Partial: the constructor, bbox_create_from_points, bbox_expand, bbox_find_silhouette, bbox_instance,
// bbox_transform, bbox_find_ith_vertex, bbox_find_distance_squared, bbox_find_closest_point and
// bbox_extract_obb_info(min, max, ...). The rest is not implemented yet.

#include "volition/vlib/code/math/bbox.h"

#include "volition/vlib/code/math/matrix.h"

#include "keen/base/trace.hpp"

// 0x004B7B90
bbox::bbox()
{
}

// 0x004B8210
// Compiler-generated: memberwise.
bbox::bbox(const bbox& __that) = default;

// 0x0056AF20 (:797)
// The eight corners: z min then z max, each (max x, max y), (max x, min y), (min x, min y), (min x, max y) (VFX.md
// FX7; Bbox_neighbors in rl_render_lib.cpp indexes them).
void bbox_get_verts(vector (&out_verts)[8], const vector& min, const vector& max)
{
    out_verts[0].set(max.x, max.y, min.z);
    out_verts[1].set(max.x, min.y, min.z);
    out_verts[2].set(min.x, min.y, min.z);
    out_verts[3].set(min.x, max.y, min.z);
    out_verts[4].set(max.x, max.y, max.z);
    out_verts[5].set(max.x, min.y, max.z);
    out_verts[6].set(min.x, min.y, max.z);
    out_verts[7].set(min.x, max.y, max.z);
}

// 0x00569BE0 (:305)
void bbox_expand(vector& min, vector& max, const vector& pnt_min, const vector& pnt_max)
{
    // Compared through the difference, as decompiled.
    min.x = 0.0f <= min.x - pnt_min.x ? pnt_min.x : min.x;
    min.y = 0.0f <= min.y - pnt_min.y ? pnt_min.y : min.y;
    min.z = 0.0f <= min.z - pnt_min.z ? pnt_min.z : min.z;
    max.x = max.x - pnt_max.x < 0.0f ? pnt_max.x : max.x;
    max.y = max.y - pnt_max.y < 0.0f ? pnt_max.y : max.y;
    max.z = max.z - pnt_max.z < 0.0f ? pnt_max.z : max.z;
}

// 0x005697F0 (:211)
void bbox_create_from_points(vector& min, vector& max, const vector& pt1, const vector& pt2)
{
    if (pt2.x <= pt1.x)
    {
        min.x = pt2.x;
        max.x = pt1.x;
    }
    else
    {
        min.x = pt1.x;
        max.x = pt2.x;
    }
    if (pt2.y <= pt1.y)
    {
        min.y = pt2.y;
        max.y = pt1.y;
    }
    else
    {
        min.y = pt1.y;
        max.y = pt2.y;
    }
    if (pt1.z < pt2.z)
    {
        min.z = pt1.z;
        max.z = pt2.z;
    }
    else
    {
        min.z = pt2.z;
        max.z = pt1.z;
    }
}

// Silhouette corner lists for the 27 regions around a box (index from the viewpoint's side per axis: x 9/18, y 3/6,
// z 1/2), up to 6 corners each, 8 = end.
static const signed char Bbox_silhouette_edges[27][6] = { // 0x012C3240
    {8, 8, 8, 8, 8, 8}, {3, 0, 1, 2, 8, 8}, {4, 7, 6, 5, 8, 8}, {2, 1, 5, 6, 8, 8}, {6, 2, 3, 0, 1, 5}, {1, 5, 4, 7, 6, 2},
    {7, 4, 0, 3, 8, 8}, {2, 3, 7, 4, 0, 1}, {3, 7, 6, 5, 4, 0}, {7, 3, 2, 6, 8, 8}, {7, 3, 0, 1, 2, 6}, {4, 7, 3, 2, 6, 5},
    {3, 2, 1, 5, 6, 7}, {3, 0, 1, 5, 6, 7}, {3, 2, 1, 5, 4, 7}, {6, 7, 4, 0, 3, 2}, {6, 7, 4, 0, 1, 2}, {4, 0, 3, 2, 6, 5},
    {0, 4, 5, 1, 8, 8}, {3, 0, 4, 5, 1, 2}, {0, 4, 7, 6, 5, 1}, {4, 5, 6, 2, 1, 0}, {4, 5, 6, 2, 3, 0}, {0, 4, 7, 6, 2, 1},
    {1, 0, 3, 7, 4, 5}, {3, 7, 4, 5, 1, 2}, {0, 3, 7, 6, 5, 1},
};

// 0x005693B0 (:135)
// LTCG: register arguments mapped in the Ghidra DB. Bounds of the box rotated by orient (per output axis, the sum of
// the smaller/larger products of each input extent).
static void transform_core(bbox& out_box, const matrix& orient, const vector& in_box_min, const vector& in_box_max)
{
    const float* rows[3] = {&orient.rvec.x, &orient.uvec.x, &orient.fvec.x};
    const float in_min[3] = {in_box_min.x, in_box_min.y, in_box_min.z};
    const float in_max[3] = {in_box_max.x, in_box_max.y, in_box_max.z};
    float out_min[3];
    float out_max[3];
    for (int axis = 0; axis < 3; axis++)
    {
        float lo = 0.0f;
        float hi = 0.0f;
        for (int k = 0; k < 3; k++)
        {
            const float a = rows[k][axis] * in_min[k];
            const float b = rows[k][axis] * in_max[k];
            lo += 0.0f <= a - b ? b : a;
            hi += a - b < 0.0f ? b : a;
        }
        out_min[axis] = lo;
        out_max[axis] = hi;
    }
    out_box.min = vector(out_min[0], out_min[1], out_min[2]);
    out_box.max = vector(out_max[0], out_max[1], out_max[2]);
}

// 0x00569880 (:244)
void bbox_create_from_points(bbox& box, const vector& pt1, const vector& pt2)
{
    bbox_create_from_points(box.min, box.max, pt1, pt2);
}

// 0x00569910 (:254)
void bbox_expand(bbox& box, const vector& pt)
{
    bbox_expand(box.min, box.max, pt);
}

// 0x00569A00 (:271)
void bbox_expand(vector& min, vector& max, const vector& pt)
{
    min.x = 0.0f <= min.x - pt.x ? pt.x : min.x;
    min.y = 0.0f <= min.y - pt.y ? pt.y : min.y;
    min.z = 0.0f <= min.z - pt.z ? pt.z : min.z;
    max.x = max.x - pt.x < 0.0f ? pt.x : max.x;
    max.y = max.y - pt.y < 0.0f ? pt.y : max.y;
    max.z = max.z - pt.z < 0.0f ? pt.z : max.z;
}

// 0x00569AF0 (:288)
void bbox_expand(bbox& box, const vector& pnt_min, const vector& pnt_max)
{
    bbox_expand(box.min, box.max, pnt_min, pnt_max);
}

// 0x00569CD0 (:321)
void bbox_expand(bbox& box, const bbox& grow_box)
{
    bbox_expand(box.min, box.max, grow_box.min, grow_box.max);
}

// 0x0059A5E0 (:378)
// The corners of the box's outline seen from viewpoint (0 when it is inside the box).
int bbox_find_silhouette(vector* out_pnts, const vector& viewpoint, const vector& min, const vector& max)
{
    int region = 0;
    if (viewpoint.x < min.x)
    {
        region = 9;
    }
    else if (max.x < viewpoint.x)
    {
        region = 0x12;
    }
    if (viewpoint.y < min.y)
    {
        region += 3;
    }
    else if (max.y < viewpoint.y)
    {
        region += 6;
    }
    if (viewpoint.z < min.z)
    {
        region += 1;
    }
    else if (max.z < viewpoint.z)
    {
        region += 2;
    }
    if (region == 0)
    {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < 6; i++)
    {
        if (Bbox_silhouette_edges[region][i] > 7)
        {
            return count;
        }
        bbox_find_ith_vertex(out_pnts[count], Bbox_silhouette_edges[region][i], min, max);
        count++;
    }
    return count;
}

// 0x0056A1E0 (:499)
void bbox_instance(bbox& out_box, const matrix& orient, const vector& trans, const bbox& in_box)
{
    transform_core(out_box, orient, in_box.min, in_box.max);
    out_box.min += trans;
    out_box.max += trans;
}

// 0x0059A810 (:507)
// The world box of a local box placed at trans with orient.
void bbox_instance(vector& out_min, vector& out_max, const matrix& orient, const vector& trans, const vector& in_min, const vector& in_max)
{
    bbox box;
    transform_core(box, orient, in_min, in_max);
    out_min = vector(trans.x + box.min.x, trans.y + box.min.y, trans.z + box.min.z);
    out_max = vector(trans.x + box.max.x, trans.y + box.max.y, trans.z + box.max.z);
}

// 0x0059A920 (:545)
// The local box (in orient at trans) of a world box: the inverse of bbox_instance.
void bbox_deinstance(bbox& out_box, const matrix& orient, const vector& trans, const bbox& in_box)
{
    transform_core(out_box, orient.copy_transpose(), in_box.min - trans, in_box.max - trans);
}

// 0x0056A2B0 (:530)
// Rotates a world box about trans.
void bbox_transform(vector& out_min, vector& out_max, const matrix& orient, const vector& trans, const vector& in_min, const vector& in_max)
{
    bbox box;
    transform_core(box, orient, in_min - trans, in_max - trans);
    out_min = vector(trans.x + box.min.x, box.min.y + trans.y, trans.z + box.min.z);
    out_max = vector(trans.x + box.max.x, box.max.y + trans.y, trans.z + box.max.z);
}

// 0x0056ADF0 (:777)
// Corner i: 0 (max x, max y), 1 (max x, min y), 2 (min x, min y), 3 (min x, max y) at min z; 4-7 the same at max z.
void bbox_find_ith_vertex(vector& out_vertex, int i, const vector& min, const vector& max)
{
    switch (i)
    {
        case 0:
            out_vertex = vector(max.x, max.y, min.z);
            break;
        case 1:
            out_vertex = vector(max.x, min.y, min.z);
            break;
        case 2:
            out_vertex = vector(min.x, min.y, min.z);
            break;
        case 3:
            out_vertex = vector(min.x, max.y, min.z);
            break;
        case 4:
            out_vertex = vector(max.x, max.y, max.z);
            break;
        case 5:
            out_vertex = vector(max.x, min.y, max.z);
            break;
        case 6:
            out_vertex = vector(min.x, min.y, max.z);
            break;
        case 7:
            out_vertex = vector(min.x, max.y, max.z);
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
    }
}

// 0x0056AD00 (:738)
// The corner indices (bbox_find_ith_vertex numbering) of the box corner farthest against the normal (neg_index) and
// along it (pos_index).
void bbox_find_np_vertices(int& neg_index, int& pos_index, const vector& normal)
{
    if (normal.x <= 0.0f)
    {
        if (normal.y <= 0.0f)
        {
            if (0.0f < normal.z)
            {
                pos_index = 6;
                neg_index = 0;
                return;
            }
            pos_index = 2;
            neg_index = 4;
            return;
        }
        if (0.0f < normal.z)
        {
            pos_index = 7;
            neg_index = 1;
            return;
        }
        pos_index = 3;
        neg_index = 5;
        return;
    }
    if (normal.y <= 0.0f)
    {
        if (0.0f < normal.z)
        {
            pos_index = 5;
            neg_index = 3;
            return;
        }
        pos_index = 1;
        neg_index = 7;
        return;
    }
    if (0.0f < normal.z)
    {
        pos_index = 4;
        neg_index = 2;
        return;
    }
    pos_index = 0;
    neg_index = 6;
}

// 0x0056B160 (:917)
float bbox_find_distance_squared(const vector& bmin, const vector& bmax, const vector& point)
{
    float d = 0.0f;
    const float p[3] = {point.x, point.y, point.z};
    const float lo[3] = {bmin.x, bmin.y, bmin.z};
    const float hi[3] = {bmax.x, bmax.y, bmax.z};
    for (int k = 0; k < 3; k++)
    {
        if (hi[k] < p[k])
        {
            d += (p[k] - hi[k]) * (p[k] - hi[k]);
        }
        else if (p[k] < lo[k])
        {
            d += (p[k] - lo[k]) * (p[k] - lo[k]);
        }
    }
    return d;
}

// 0x0059AA60 (:1048)
// Half extents of a local box and its centre in world space.
void bbox_extract_obb_info(const vector& local_bmin, const vector& local_bmax, const vector& world_pos, const matrix& orient, vector& extents, vector& center)
{
    center = vector((local_bmin.x + local_bmax.x) * 0.5f, (local_bmin.y + local_bmax.y) * 0.5f, (local_bmin.z + local_bmax.z) * 0.5f);
    extents = vector(local_bmax.x - center.x, local_bmax.y - center.y, local_bmax.z - center.z);
    center = center.copy_unrotate(orient);
    center += world_pos;
}

// 0x0056B330 (:1022)
// The point clamped into the box; returns its squared distance to the box.
float bbox_find_closest_point(vector& closest_point, const vector& bmin, const vector& bmax, const vector& point)
{
    closest_point = point;
    const float above_x = closest_point.x - bmax.x;
    const float above_y = closest_point.y - bmax.y;
    const float above_z = closest_point.z - bmax.z;
    const float below_x = bmin.x - closest_point.x;
    const float below_y = bmin.y - closest_point.y;
    const float below_z = bmin.z - closest_point.z;
    const float dx = (below_x >= 0.0f ? below_x : 0.0f) + (above_x < 0.0f ? 0.0f : -above_x);
    const float dy = (below_y < 0.0f ? 0.0f : below_y) + (above_y >= 0.0f ? -above_y : 0.0f);
    const float dz = (below_z < 0.0f ? 0.0f : below_z) + (above_z < 0.0f ? 0.0f : -above_z);
    closest_point.x += dx;
    closest_point.y += dy;
    closest_point.z += dz;
    return dz * dz + dx * dx + dy * dy;
}

// 0x00569DC0 (:333)
void bbox_grow(vector& min, vector& max, const vector& disp)
{
    if (0.0f <= disp.x) {
        max.x += disp.x;
    } else {
        min.x += disp.x;
    }
    if (0.0f <= disp.y) {
        max.y += disp.y;
    } else {
        min.y += disp.y;
    }
    if (disp.z < 0.0f) {
        min.z += disp.z;
    } else {
        max.z += disp.z;
    }
}

// 0x0056B270 (:972)
float bbox_find_xz_distance_squared(const vector& bmin, const vector& bmax, const vector& point)
{
    float dist_sq = 0.0f;
    if (bmax.x < point.x) {
        dist_sq = (float)((double)(point.x - bmax.x) * (double)(point.x - bmax.x));
    } else if (point.x < bmin.x) {
        dist_sq = (float)((double)(point.x - bmin.x) * (double)(point.x - bmin.x));
    }
    if (bmax.z < point.z) {
        dist_sq += (point.z - bmax.z) * (point.z - bmax.z);
    } else if (point.z < bmin.z) {
        dist_sq += (point.z - bmin.z) * (point.z - bmin.z);
    }
    return dist_sq;
}
