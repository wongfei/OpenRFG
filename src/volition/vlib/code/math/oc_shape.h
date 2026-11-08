// Original: volition/vlib/code/math/oc_shape.h
// Implementation: volition/vlib/code/math/oc_shape.cpp
//
// oc_shape, convex occlusion shapes (boxes, wedges) with silhouette and containment tests.
#pragma once

#include "volition/vlib/code/math/vector3.h"

struct cfile;
class matrix;

// size 0xAC
struct oc_shape
{
    unsigned int type; // 0x00
    vector pts[8]; // 0x04
    vector norms[6]; // 0x64
};

void ocs_make_box(oc_shape& out, const vector& bmin, const vector& bmax, const vector* _offset, const matrix* _orient); // 0x005BA860 (:161)
void ocs_make_wedge(oc_shape& out, const vector& bmin, const vector& bmax, const vector* _offset, const matrix* _orient); // 0x005BAC90 (:221)
unsigned char ocs_get_face_bits(const oc_shape& s, const vector& eye); // 0x00578720 (:279)
unsigned char* ocs_find_silhouette(int& num_verts, const oc_shape& s, const vector& eye); // 0x005A4990 (:310)
unsigned char* ocs_find_silhouette(int& num_verts, const oc_shape& s, unsigned char fbits); // 0x00578B00 (:329)
int ocs_shape_contains_shape(const oc_shape& container, const oc_shape& containee); // 0x005A49F0 (:357)
int ocs_shape_contains_point(const oc_shape& container, const vector& containee); // 0x005A4B60 (:389)
void ocs_get_shape_center(vector& cen, const oc_shape& s); // 0x00578B50 (:411)
void ocs_get_shape_bbox(vector& bmin, vector& bmax, const oc_shape& s); // 0x00578C50 (:429)
void ocs_shape_transform(oc_shape& sh, const vector& offset, const matrix& orient); // 0x005A4CA0 (:454)
bool ocs_write_shape(const oc_shape& sh, cfile* f); // 0x00578D90 (:473)
