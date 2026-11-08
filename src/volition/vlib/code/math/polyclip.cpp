// Original: volition/vlib/code/math/polyclip.cpp
//
// Clipping a 3D polygon against planes (the part of the plane's positive side is removed; points within 0.0001 count
// as on the plane), clipping a 2D polygon against a convex one (VFX.md FX8, the glass shards), and polygon/sphere
// inside tests. The 2D clipper and the tests compute in double the way rfg.exe does (a plane distance is the dot
// product summed in double and rounded, then the offset added and rounded; the 0.0001 margin is a double).

#include "volition/vlib/code/math/polyclip.h"

#include "volition/vlib/code/math/plane.h"
#include "volition/vlib/code/math/vector2.h"

#include <math.h>

static const int POLYCLIP_MAX_VERTS = 32;

// ---- polyclip_2d scratch (file statics): the vertex pointer lists it ping-pongs between, the signed distances of the
// current list to the current edge, and the intersection points it creates.
static vector2* Tmp1[32]; // 0x01CDD740
static vector2* Tmp2[32]; // 0x01CD2950
static float Dist[32]; // 0x01CD34A0
static vector2 Space[64]; // 0x01CE9998

// Magic number: the on-plane margin of the 2D clipper and the tests (a double constant).
static const double POLYCLIP_EPSILON = 0.0001;

// Not in the binary: a plane distance as the tests compute it (inlined).
static float pc_plane_distance(const plane& p, const vector& v)
{
    const float dot = (float)((double)p.normal.x * (double)v.x + (double)p.normal.y * (double)v.y + (double)p.normal.z * (double)v.z);
    return (float)((double)dot + (double)p.offset);
}

// 0x005790B0 (:43)
// 1 in front of the plane (beyond the accuracy plus 0.0001), 2 behind it, 3 on it.
static int pc_which_side(const plane& plane, const vector& vert, float accuracy)
{
    const float dist = pc_plane_distance(plane, vert);
    const double margin = (double)accuracy + POLYCLIP_EPSILON;
    if ((double)dist > margin)
    {
        return 1;
    }
    return -margin > (double)dist ? 2 : 3;
}

// 0x00579150 (:79)
// Clips poly (n2 vertices, clipping_poly) against the convex polygon poly (n1 vertices): edge by edge, the vertices on
// the edge's outer side (beyond 0.0001) are dropped and the crossings added (Space). The edge normals are flipped for
// all edges when the third vertex of poly lies on the outer side of its first edge (the other winding); edges shorter
// than 0.0001 (Manhattan length) are skipped. Returns the vertex count (0 when everything is clipped or fewer than 3
// remain); which is clipping_poly when nothing was clipped, else inter, which then holds the result.
// Notes (decompiled behaviour, kept as is): the counters are 64-bit; with fewer than 3 vertices in poly the winding test
// reads past it.
int64_t polyclip_2d(int n1, vector2* poly, int n2, vector2* clipping_poly, vector2* inter, vector2*& which)
{
    vector2** cur = Tmp1;
    vector2* space = Space;
    which = clipping_poly;
    int64_t count = n2;
    bool flip = false;
    for (int64_t i = 0; i < count; i++)
    {
        Tmp1[i] = &clipping_poly[i];
    }
    for (int64_t i = 0; i < (int64_t)n1; i++)
    {
        const vector2& p0 = poly[i];
        const vector2* p1 = i + 1 < (int64_t)n1 ? &poly[i + 1] : poly;
        float nx = p1->y - p0.y;
        float ny = p0.x - p1->x;
        const float length = (float)fabs(ny) + (float)fabs(nx);
        if (POLYCLIP_EPSILON > (double)length)
        {
            continue;
        }
        nx = (float)((double)nx / (double)length);
        ny = (float)((double)ny / (double)length);
        float d = (float)((double)p0.x * (double)nx + (double)p0.y * (double)ny);
        if (i == 0 && (double)(float)((double)p1[1].y * (double)ny + (double)p1[1].x * (double)nx) - (double)d > POLYCLIP_EPSILON)
        {
            flip = true;
        }
        if (flip)
        {
            d = -d;
            nx = -nx;
            ny = -ny;
        }
        int64_t outside = 0;
        for (int64_t j = 0; j < count; j++)
        {
            const float dist = (float)((double)(float)((double)cur[j]->y * (double)ny + (double)cur[j]->x * (double)nx) - (double)d);
            Dist[j] = dist;
            if ((double)dist > POLYCLIP_EPSILON)
            {
                outside++;
            }
        }
        if (outside == count)
        {
            return 0;
        }
        if (outside == 0)
        {
            continue;
        }
        Dist[count] = Dist[0];
        cur[count] = cur[0];
        vector2** next = cur != Tmp1 ? Tmp1 : Tmp2;
        vector2** out = next;
        for (int64_t j = 0; j < count; j++)
        {
            const float d0 = Dist[j];
            if (POLYCLIP_EPSILON > (double)d0)
            {
                *out++ = cur[j];
                if (!(-POLYCLIP_EPSILON > (double)d0) || !((double)Dist[j + 1] > POLYCLIP_EPSILON))
                {
                    continue;
                }
            }
            else if (!(-POLYCLIP_EPSILON > (double)Dist[j + 1]))
            {
                which = inter;
                continue;
            }
            // The crossing of the edge cur[j] -> cur[j + 1].
            const float d1 = Dist[j + 1];
            const vector2& a = *cur[j];
            const vector2& b = *cur[j + 1];
            const double t = (double)(float)((double)d0 / ((double)d0 - (double)d1));
            space->x = (float)(((double)b.x - (double)a.x) * t + (double)a.x);
            space->y = (float)(((double)b.y - (double)a.y) * t + (double)a.y);
            *out++ = space;
            space++;
            which = inter;
        }
        count = out - next;
        cur = next;
    }
    if (count < 3)
    {
        return 0;
    }
    if (which != clipping_poly)
    {
        for (int64_t j = 0; j < count; j++)
        {
            inter[j] = *cur[j];
        }
    }
    return count;
}

// 0x00579640 (:209)
int polyclip_with_plane_3d(int nverts, const vector* inverts, vector* outverts, const plane& p)
{
    int count = 0;
    int next = 1;
    for (int i = 0; i < nverts; i++)
    {
        const vector& a = inverts[i];
        const vector& b = inverts[next];
        const float da = a.x * p.normal.x + a.y * p.normal.y + p.normal.z * a.z + p.offset;
        const int side_a = da > 0.0001f ? 1 : (-0.0001f <= da ? 3 : 2);
        const float db = p.normal.x * b.x + b.y * p.normal.y + b.z * p.normal.z + p.offset;
        const int side_b = db > 0.0001f ? 1 : (-0.0001f <= db ? 3 : 2);
        if (side_a != 1)
        {
            *outverts++ = a;
            count++;
        }
        if ((side_b != 1) != (side_a != 1))
        {
            // Intersection of the edge with the plane.
            const float na = -(p.normal.y * a.y + p.normal.x * a.x + p.normal.z * a.z);
            const float t = (p.offset - na) / (-(b.y * p.normal.y + b.x * p.normal.x + b.z * p.normal.z) - na);
            outverts->x = a.x + (b.x - a.x) * t;
            outverts->y = a.y + (b.y - a.y) * t;
            outverts->z = (b.z - a.z) * t + a.z;
            outverts++;
            count++;
        }
        next++;
        if (next == nverts)
        {
            next = 0;
        }
    }
    return count;
}

// 0x005799B0 (:271)
// Clips against each plane in turn (ping-ponging between outverts and a local buffer); 0 when fewer than 3 vertices
// remain or the polygon grows past 31 vertices.
int polyclip_with_plane_list_3d(int nverts, const vector* inverts, vector* outverts, const plane* plane_list, int plane_count)
{
    if (nverts >= POLYCLIP_MAX_VERTS)
    {
        return 0;
    }
    vector* result = outverts;
    for (int i = 0; i < nverts; i++)
    {
        outverts[i] = inverts[i];
    }
    vector buffer[POLYCLIP_MAX_VERTS];
    vector* src = outverts;
    vector* dst = buffer;
    for (int i = 0; i < plane_count; i++)
    {
        if (nverts >= POLYCLIP_MAX_VERTS)
        {
            return 0;
        }
        nverts = polyclip_with_plane_3d(nverts, src, dst, plane_list[i]);
        if (nverts < 3)
        {
            return 0;
        }
        vector* t = src;
        src = dst;
        dst = t;
    }
    if (plane_count > 0 && src != result)
    {
        for (int i = 0; i < nverts; i++)
        {
            result[i] = src[i];
        }
    }
    return nverts;
}

// 0x00579AB0 (:327)
// Whether any vertex is in front of the plane (beyond 0.0001).
bool polyclip_test_poly_outside_plane(int nverts, const vector* inverts, const plane& p)
{
    for (int i = 0; i < nverts; i++)
    {
        if ((double)pc_plane_distance(p, inverts[i]) > POLYCLIP_EPSILON)
        {
            return true;
        }
    }
    return false;
}

// 0x00579B60 (:347)
// Whether every plane has a vertex behind it (beyond the accuracy plus 0.0001); true without planes.
bool polyclip_test_poly_inside_plane_list(int nverts, const vector* inverts, const plane* plane_list, int plane_count, float accuracy)
{
    for (int i = 0; i < plane_count; i++)
    {
        int v = 0;
        while (v < nverts && pc_which_side(plane_list[i], inverts[v], accuracy) != 2)
        {
            v++;
        }
        if (v == nverts)
        {
            return false;
        }
    }
    return true;
}

// 0x00579EC0 (:384)
// Whether the sphere's center is behind every plane by more than radius - (accuracy + 0.0001), i.e. the sphere reaches
// inside all of them.
bool polyclip_test_sphere_inside_plane_list(const vector& center, float radius, const plane* plane_list, int plane_count, float accuracy)
{
    const double limit = (double)radius - ((double)accuracy + POLYCLIP_EPSILON);
    for (int i = 0; i < plane_count; i++)
    {
        if ((double)pc_plane_distance(plane_list[i], center) >= limit)
        {
            return false;
        }
    }
    return true;
}

// 0x00579F80 (:404)
// As polyclip_test_sphere_inside_plane_list with -radius: the whole sphere is behind every plane.
bool polyclip_test_sphere_completely_inside_plane_list(const vector& center, float radius, const plane* plane_list, int plane_count, float accuracy)
{
    const double limit = (double)-radius - ((double)accuracy + POLYCLIP_EPSILON);
    for (int i = 0; i < plane_count; i++)
    {
        if ((double)pc_plane_distance(plane_list[i], center) >= limit)
        {
            return false;
        }
    }
    return true;
}
