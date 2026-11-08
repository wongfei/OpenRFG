// Original: volition/vlib/code/math/bbox2d.cpp
//
// 2D bounding boxes (UI.md UI2: the vint elements' bounds and picking). The comparisons are rfg.exe's, including the
// differences of floats tested against zero. bbox2d_grow and bbox2d_inflate are not used by the ported code yet.

#include "volition/vlib/code/math/bbox2d.h"

// 0x0045E350
bbox2d::bbox2d()
{
}

// 0x00883820 (.h:53)
bool bbox2d_contains(const bbox2d& box, const vector2& position)
{
    // the four tests summed as floats
    float out_min_x = box.min.x <= position.x ? 0.0f : 1.0f;
    float out_min_y = box.min.y <= position.y ? 0.0f : 1.0f;
    float out_max_x = position.x <= box.max.x ? 0.0f : 1.0f;
    float out_max_y = position.y <= box.max.y ? 0.0f : 1.0f;
    return out_max_y + out_max_x + out_min_y + out_min_x == 0.0f;
}

// 0x0056B520 (:17)
void bbox2d_create_from_points(bbox2d& box, const vector2& pt0, const vector2& pt1)
{
    if (pt1.x <= pt0.x)
    {
        box.min.x = pt1.x;
        box.max.x = pt0.x;
    }
    else
    {
        box.min.x = pt0.x;
        box.max.x = pt1.x;
    }
    if (pt0.y < pt1.y)
    {
        box.min.y = pt0.y;
        box.max.y = pt1.y;
        return;
    }
    box.min.y = pt1.y;
    box.max.y = pt0.y;
}

// 0x0056B580 (:44)
void bbox2d_create_from_points(bbox2d& box, float pt0_x, float pt0_y, float pt1_x, float pt1_y)
{
    if (pt1_x <= pt0_x)
    {
        box.min.x = pt1_x;
        box.max.x = pt0_x;
    }
    else
    {
        box.min.x = pt0_x;
        box.max.x = pt1_x;
    }
    if (pt0_y < pt1_y)
    {
        box.min.y = pt0_y;
        box.max.y = pt1_y;
        return;
    }
    box.min.y = pt1_y;
    box.max.y = pt0_y;
}

// 0x0056B5E0 (:69)
void bbox2d_create_from_circle(bbox2d& box, const vector2 pt, const float radius)
{
    box.min.x = pt.x - radius;
    box.min.y = pt.y - radius;
    box.max.x = pt.x + radius;
    box.max.y = pt.y + radius;
}

// 0x0056B650 (:83)
void bbox2d_expand(bbox2d& box, const bbox2d& box2)
{
    if (box.min.x - box2.min.x >= 0.0f)
    {
        box.min.x = box2.min.x;
    }
    if (box.min.y - box2.min.y >= 0.0f)
    {
        box.min.y = box2.min.y;
    }
    if (box.max.x - box2.max.x < 0.0f)
    {
        box.max.x = box2.max.x;
    }
    if (!(box.max.y - box2.max.y >= 0.0f))
    {
        box.max.y = box2.max.y;
    }
}

// 0x0056B6F0 (:97)
void bbox2d_expand(bbox2d& box, const vector2& pt)
{
    if (box.min.x - pt.x >= 0.0f)
    {
        box.min.x = pt.x;
    }
    if (box.max.x - pt.x < 0.0f)
    {
        box.max.x = pt.x;
    }
    if (box.min.y - pt.y >= 0.0f)
    {
        box.min.y = pt.y;
    }
    if (!(box.max.y - pt.y >= 0.0f))
    {
        box.max.y = pt.y;
    }
}
