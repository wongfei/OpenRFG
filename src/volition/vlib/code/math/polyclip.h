// Original: volition/vlib/code/math/polyclip.h
// Implementation: volition/vlib/code/math/polyclip.cpp
//
// Polygon clipping against planes in 2D and 3D, and sphere/polygon inside tests.
#pragma once

#include <stdint.h>

class plane;
class vector;
class vector2;

int64_t polyclip_2d(int n1, vector2* poly, int n2, vector2* clipping_poly, vector2* inter, vector2*& which); // 0x00579150 (:79)
int polyclip_with_plane_3d(int nverts, const vector* inverts, vector* outverts, const plane& p); // 0x00579640 (:209)
int polyclip_with_plane_list_3d(int nverts, const vector* inverts, vector* outverts, const plane* plane_list, int plane_count); // 0x005799B0 (:271)
bool polyclip_test_poly_outside_plane(int nverts, const vector* inverts, const plane& p); // 0x00579AB0 (:327)
bool polyclip_test_poly_inside_plane_list(int nverts, const vector* inverts, const plane* plane_list, int plane_count, float accuracy); // 0x00579B60 (:347)
bool polyclip_test_sphere_inside_plane_list(const vector& center, float radius, const plane* plane_list, int plane_count, float accuracy); // 0x00579EC0 (:384)
bool polyclip_test_sphere_completely_inside_plane_list(const vector& center, float radius, const plane* plane_list, int plane_count, float accuracy); // 0x00579F80 (:404)
