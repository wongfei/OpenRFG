// Original: volition/vlib/code/math/triangulate.h
// Implementation: volition/vlib/code/math/triangulate.cpp
//
// Polygon triangulation (convex fans and ear clipping of concave polygons); tess_vert is a vertex of the polygon being
// clipped. Placement of tess_vert inferred.
#pragma once


class vector;

// size 0x10
struct tess_vert
{
    vector* v; // 0x00
    float winding; // 0x04
    tess_vert* next; // 0x08
    tess_vert* prev; // 0x0C
};

bool triangulate_winding_direction(vector& winding, vector* polygon, int num_points); // 0x0057DED0 (:134)
int triangulate_convex_polygon(int* dest, int num_points); // 0x0057E150 (:183)
int triangulate_concave_polygon_ear(int* dest, int num_points, vector& winding_dir); // 0x005A7830 (:209)
int triangulate_concave_polygon(int* dest, vector* polygon, int num_points); // 0x005A7E50 (:291)
