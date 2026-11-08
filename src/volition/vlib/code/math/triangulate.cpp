// Original: volition/vlib/code/math/triangulate.cpp
//
// Polygon triangulation (VFX.md FX8, the glass shards): fans of convex polygons and ear clipping of concave ones in the
// xz plane. The concave polygon becomes a ring of tess_verts (Tess_polygon_verts) whose cached winding (the turn at the
// vertex along the polygon's winding direction, infinity when stale) decides the ears; an ear's new edge must not
// cross another edge. The arithmetic follows rfg.exe (products and sums in double, rounded per step).

#include "volition/vlib/code/math/triangulate.h"

#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/vector3.h"

#include <float.h>
#include <math.h>
#include <string.h>

// ---- file-static globals
static tess_vert* Tess_polygon; // 0x01518F84, initial data b863cd01
// char_code Scanner_char_table[256] (0x01CD5FB8) is used here too; placed in vconsole.cpp.
static tess_vert Tess_polygon_verts[1024]; // 0x01CD63B8

// Not in the binary: a stale (infinite or NaN) cached winding.
static bool tess_winding_stale(float winding)
{
    unsigned int bits;
    memcpy(&bits, &winding, sizeof(bits));
    return (bits & 0x7F800000) == 0x7F800000;
}

// ---- internal functions

// 0x0057DDF0 (:45)
static int tess_vert_idx(tess_vert* tv)
{
    return (int)(tv - Tess_polygon_verts);
}

// 0x0057DE00 (:52)
// Unlinks the vertex; its neighbours' windings become stale.
static void tess_vert_remove(tess_vert* tv)
{
    tv->next->winding = INFINITY;
    tv->prev->winding = INFINITY;
    tv->next->prev = tv->prev;
    tv->prev->next = tv->next;
}

// 0x0057DE40 (:70)
// Whether the edge v1-v0 crosses no edge of the ring (edges touching v0 or v1 are skipped; a crossing is a closest
// point pair within both segments).
static bool triangulate_edge_enclosed(tess_vert* v0, tess_vert* v1)
{
    tess_vert* tv = v1;
    while (tv != nullptr)
    {
        tess_vert* next = tv->next;
        if (tv != v1 && tv != v0 && next != v1 && next != v0)
        {
            ix_collision col_a;
            ix_collision col_b;
            if (ix_closest_line_line(col_a, col_b, *v1->v, *v0->v, *tv->v, *next->v) && 0.0f <= col_a.fraction && col_a.fraction <= 1.0f && 0.0f <= col_b.fraction && col_b.fraction <= 1.0f)
            {
                return false;
            }
        }
        tv = tv->next;
        if (tv == v1)
        {
            return true;
        }
    }
    return true;
}

// 0x005A76B0 (:103)
// Refreshes a stale winding (winding_dir . ((v - prev) x (next - v))); true when it is not negative (or NaN).
static bool triangulate_winding_check(tess_vert* tv, const vector& winding_dir)
{
    if (tess_winding_stale(tv->winding))
    {
        const vector& prev = *tv->prev->v;
        const vector& cur = *tv->v;
        const vector& next = *tv->next->v;
        const float ax = next.x - cur.x;
        const float ay = next.y - cur.y;
        const float az = next.z - cur.z;
        const float bx = cur.x - prev.x;
        const float by = cur.y - prev.y;
        const float bz = cur.z - prev.z;
        const float cx = (float)((double)by * az - (double)bz * ay);
        const float cy = (float)((double)bz * ax - (double)az * bx);
        const float cz = (float)((double)ay * bx - (double)by * ax);
        tv->winding = (float)((double)winding_dir.y * cy + (double)cx * winding_dir.x + (double)winding_dir.z * cz);
    }
    return !(0.0f > tv->winding);
}

// ---- public functions

// 0x0057DED0 (:134)
// The polygon's winding in the xz plane from the sum of its turn angles: (0, 1, 0) above 0.1, (0, -1, 0) below; false
// at exactly 0.1. The angles are atan2 of the edge to each neighbour, the second unwrapped to within pi of the first.
// Notes (decompiled behaviour, kept as is): rfg.exe computes atan2 on the x87 (the result is rounded to float).
bool triangulate_winding_direction(vector& winding, vector* polygon, int num_points)
{
    // Magic numbers: pi and 2 pi as rfg.exe's double constants (pi rounded to float, 2 pi as float).
    const double pi = 3.1415927410125732;
    const double two_pi = 6.2831854820251465;
    float sum = 0.0f;
    for (int i = 0; i < num_points; i++)
    {
        const int prev = i != 0 ? i - 1 : num_points - 1;
        const int next = i == num_points - 1 ? 0 : i + 1;
        const vector& cur = polygon[i];
        const float a1 = (float)atan2((double)(polygon[prev].z - cur.z), (double)(polygon[prev].x - cur.x));
        float a2 = (float)atan2((double)(polygon[next].z - cur.z), (double)(polygon[next].x - cur.x));
        while ((double)a2 > (double)a1 + pi)
        {
            a2 = (float)((double)a2 - two_pi);
        }
        while ((double)a1 - pi > (double)a2)
        {
            a2 = (float)((double)a2 + two_pi);
        }
        const double diff = (double)a2 - (double)a1;
        if (0.0 > diff)
        {
            sum = (float)((double)sum - (diff + pi));
        }
        else
        {
            sum = (float)((pi - diff) + (double)sum);
        }
    }
    // Magic number: the turn sum threshold (a double constant).
    if ((double)sum > 0.1)
    {
        winding.set(0.0f, 1.0f, 0.0f);
        return true;
    }
    if (0.1 > (double)sum)
    {
        winding.set(0.0f, -1.0f, 0.0f);
        return true;
    }
    return false;
}

// 0x0057E150 (:183)
// A fan around vertex 0; returns the triangle count.
int triangulate_convex_polygon(int* dest, int num_points)
{
    if (num_points < 3)
    {
        return 0;
    }
    for (int i = 2; i <= num_points - 1; i++)
    {
        dest[0] = 0;
        dest[1] = i - 1;
        dest[2] = i;
        dest += 3;
    }
    return num_points - 2;
}

// 0x005A7830 (:209)
// Ear clipping of the ring Tess_polygon. At a vertex whose winding is negative, the triangle with the next two vertices
// is cut when the next vertex's winding is not negative and the new edge crosses nothing; else the triangle with the
// previous two vertices under the same conditions. A cut removes the middle vertex and starts the count of vertices
// tried again at the same vertex; the walk moves on otherwise and stops when the tries and cuts reach the vertex count.
// The remaining triangles (up to n - 2) are a fan from the current vertex.
// Notes (decompiled behaviour, kept as is): the fan at the end only unlinks next vertices (no winding update).
int triangulate_concave_polygon_ear(int* dest, int num_points, vector& winding_dir)
{
    int num_tris = 0;
    int tries = 0;
    tess_vert* tv = Tess_polygon;
    while (num_tris < num_points - 3)
    {
        tess_vert* removed = nullptr;
        if (!triangulate_winding_check(tv, winding_dir))
        {
            if (triangulate_winding_check(tv->next, winding_dir) && triangulate_edge_enclosed(tv->next->next, tv))
            {
                dest[0] = tess_vert_idx(tv);
                dest[1] = tess_vert_idx(tv->next);
                dest[2] = tess_vert_idx(tv->next->next);
                removed = tv->next;
            }
            else if (triangulate_winding_check(tv->prev, winding_dir) && triangulate_edge_enclosed(tv->prev->prev, tv))
            {
                dest[0] = tess_vert_idx(tv->prev->prev);
                dest[1] = tess_vert_idx(tv->prev);
                dest[2] = tess_vert_idx(tv);
                removed = tv->prev;
            }
        }
        if (removed != nullptr)
        {
            tess_vert_remove(removed);
            dest += 3;
            num_tris++;
            tries = 0;
            continue;
        }
        tv = tv->next;
        tries++;
        if (num_points <= tries + num_tris)
        {
            break;
        }
    }
    for (; num_tris < num_points - 2; num_tris++)
    {
        dest[0] = tess_vert_idx(tv);
        dest[1] = tess_vert_idx(tv->next);
        dest[2] = tess_vert_idx(tv->next->next);
        dest += 3;
        tv->next = tv->next->next;
    }
    return num_tris;
}

// 0x005A7E50 (:291)
// Ear clipping of a concave polygon (up to 1,024 points) into dest (3 indices per triangle); returns the triangle count
// (0 with fewer than 3 points or no winding).
int triangulate_concave_polygon(int* dest, vector* polygon, int num_points)
{
    if (num_points < 3)
    {
        return 0;
    }
    vector winding;
    if (!triangulate_winding_direction(winding, polygon, num_points))
    {
        return 0;
    }
    Tess_polygon = Tess_polygon_verts;
    for (int i = 0; i < num_points; i++)
    {
        Tess_polygon_verts[i].v = &polygon[i];
        Tess_polygon_verts[i].winding = INFINITY;
        Tess_polygon_verts[i].next = &Tess_polygon_verts[i + 1];
        Tess_polygon_verts[i].prev = &Tess_polygon_verts[i - 1];
    }
    Tess_polygon_verts[0].prev = &Tess_polygon_verts[num_points - 1];
    Tess_polygon_verts[num_points - 1].next = &Tess_polygon_verts[0];
    return triangulate_concave_polygon_ear(dest, num_points, winding);
}
