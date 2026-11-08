// Original: volition/vlib/code/math/bbox2d.h
// Implementation: volition/vlib/code/math/bbox2d.cpp
//
// bbox2d, a 2D bounding box, and its construction/test helpers.
#pragma once

#include "volition/vlib/code/math/vector2.h"

// size 0x10
struct bbox2d
{
    bbox2d(); // 0x0045E350

    vector2 min; // 0x00
    vector2 max; // 0x08
};

bool bbox2d_contains(const bbox2d& box, const vector2& position); // 0x00883820 (.h:53)
void bbox2d_create_from_points(bbox2d& box, const vector2& pt0, const vector2& pt1); // 0x0056B520 (:17)
void bbox2d_create_from_points(bbox2d& box, float pt0_x, float pt0_y, float pt1_x, float pt1_y); // 0x0056B580 (:44)
void bbox2d_create_from_circle(bbox2d& box, const vector2 pt, const float radius); // 0x0056B5E0 (:69)
void bbox2d_expand(bbox2d& box, const bbox2d& box2); // 0x0056B650 (:83)
void bbox2d_expand(bbox2d& box, const vector2& pt); // 0x0056B6F0 (:97)
void bbox2d_grow(bbox2d& box, const vector2& dir); // 0x0056B790 (:110)
void bbox2d_inflate(bbox2d& box, const vector2& dir); // 0x0056B810 (:130)
