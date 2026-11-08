// Original: volition/vlib/code/math/plane.h
// Implementation: volition/vlib/code/math/plane.cpp
//
// Plane as normal + offset (normal . p = offset for points on the plane; inferred from point() and
// distance_to_point). Used by the xml readers (read_xml_plane) and later by collision code.
#pragma once

#include "volition/vlib/code/math/vector3.h"

class matrix;
class vector4;

// size 0x10
class plane
{
public:
    plane(const plane& __that); // 0x00578F30
    plane(const vector& a, const vector& b, const vector& c); // 0x004B50E0 (.h:54)
    plane(const vector& norm, const vector& point); // 0x004B50C0 (.h:47)
    plane(const vector4& v4); // 0x004B50A0 (.h:38)
    plane(float a, float b, float c, float d); // 0x00B629E0 (.h:30)
    plane(); // 0x004B5090 (.h:27)
    void set(const vector& a, const vector& b, const vector& c); // 0x005A4EF0 (:36)
    void set(const vector& norm, const vector& point); // 0x00578EB0 (:17)
    void reverse(); // 0x00579050 (:159)
    vector point() const; // 0x004B5100 (.h:69)
    float distance_to_point(const vector& v) const; // 0x004B5160 (.h:77)
    float distance_to_point_swizzled(const vector&) const;
    void rotate(const matrix& orient, const vector& pos); // 0x005A5110 (:48)
    plane copy_rotate(const matrix& orient, const vector& pos) const; // 0x005A5290 (:64)
    void unrotate(const matrix& orient, const vector& pos); // 0x005A5440 (:79)
    plane copy_unrotate(const matrix& orient, const vector& pos) const; // 0x005A56D0 (:101)
    void normalize(); // 0x005A59A0 (:113)
    void normalize_safe(); // 0x005A5A50 (:121)
    plane copy_normalize() const; // 0x005A5B70 (:133)
    plane copy_normalize_safe() const; // 0x005A5C20 (:141)
    vector find_closest_point(const vector& v) const; // 0x00578F50 (:152)

    vector normal; // 0x00
    float offset; // 0x0C
};

// 0x004B5160 (.h:77)
inline float plane::distance_to_point(const vector& v) const
{
    return normal.x * v.x + normal.y * v.y + normal.z * v.z + offset;
}
