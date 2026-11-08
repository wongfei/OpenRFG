// Original: volition/vlib/code/math/bbox.h
// Implementation: volition/vlib/code/math/bbox.cpp
//
// Axis-aligned bounding box (min, max corners) and helpers that take either a bbox or min/max vectors.
#pragma once

#include "volition/vlib/code/math/vector3.h"

class matrix;
class plane;

// size 0x18
struct bbox
{
    bbox(const bbox& __that); // 0x004B8210
    bbox(); // 0x004B7B90

    vector min; // 0x00
    vector max; // 0x0C
};

void bbox_create_from_points(vector& min, vector& max, const vector& pt1, const vector& pt2); // 0x005697F0 (:211)
void bbox_create_from_points(bbox& box, const vector& pt1, const vector& pt2); // 0x00569880 (:244)
void bbox_expand(bbox& box, const vector& pt); // 0x00569910 (:254)
void bbox_expand(vector& min, vector& max, const vector& pt); // 0x00569A00 (:271)
void bbox_expand(bbox& box, const vector& pnt_min, const vector& pnt_max); // 0x00569AF0 (:288)
void bbox_expand(vector& min, vector& max, const vector& pnt_min, const vector& pnt_max); // 0x00569BE0 (:305)
void bbox_expand(bbox& box, const bbox& grow_box); // 0x00569CD0 (:321)
void bbox_grow(vector& min, vector& max, const vector& disp); // 0x00569DC0 (:333)
void bbox_grow(bbox& box, const vector& disp); // 0x00569E90 (:360)
int bbox_find_silhouette(vector* out_pnts, const vector& viewpoint, const vector& min, const vector& max); // 0x0059A5E0 (:378)
int bbox_find_silhouette_index(const vector& viewpoint, const vector& min, const vector& max); // 0x00569F50 (:416)
bool bbox_intersects_plane(const vector& min, const vector& max, plane& p); // 0x0059A6E0 (:450)
void bbox_rotate_about_center(bbox& out_box, const matrix& orient, const bbox& in_box); // 0x0056A000 (:486)
void bbox_instance(bbox& out_box, const matrix& orient, const vector& trans, const bbox& in_box); // 0x0056A1E0 (:499)
void bbox_instance(vector& out_min, vector& out_max, const matrix& orient, const vector& trans, const vector& in_min, const vector& in_max); // 0x0059A810 (:507)
void bbox_transform(vector& out_min, vector& out_max, const matrix& orient, const vector& trans, const vector& in_min, const vector& in_max); // 0x0056A2B0 (:530)
void bbox_deinstance(bbox& out_box, const matrix& orient, const vector& trans, const bbox& in_box); // 0x0059A920 (:545)
bool bbox_clip_line(vector& out1, vector& out2, int& inside_points, const vector& bbox_min, const vector& bbox_max, const vector& p1, const vector& p2); // 0x0056A470 (:564)
void bbox_find_np_vertices(int& neg_index, int& pos_index, const vector& normal); // 0x0056AD00 (:738)
void bbox_find_ith_vertex(vector& out_vertex, int i, const vector& min, const vector& max); // 0x0056ADF0 (:777)
void bbox_get_verts(vector (&out_verts)[8], const vector& min, const vector& max); // 0x0056AF20 (:797)
float bbox_find_closest_point_fast_double(vector& closest_point, const vector& bmin, const vector& bmax, const vector& point); // 0x0056B000 (:811)
float bbox_find_closest_point_normal(vector& closest_point, const vector& bmin, const vector& bmax, const vector& point); // 0x0056B010 (:850)
float bbox_find_distance_squared(const vector& bmin, const vector& bmax, const vector& point); // 0x0056B160 (:917)
float bbox_find_xz_distance_squared(const vector& bmin, const vector& bmax, const vector& point); // 0x0056B270 (:972)
float bbox_find_closest_point(vector& closest_point, const vector& bmin, const vector& bmax, const vector& point); // 0x0056B330 (:1022)
void bbox_extract_obb_info(const bbox& box, const vector& world_pos, const matrix& orient, vector& extents, vector& center); // 0x005B8AD0 (:1043)
void bbox_extract_obb_info(const vector& local_bmin, const vector& local_bmax, const vector& world_pos, const matrix& orient, vector& extents, vector& center); // 0x0059AA60 (:1048)
bool bbox_intersect(const vector& min1, const vector& max1, const vector& min2, const vector& max2); // 0x00563860 (.h:69)
