// Original: volition/rfg/code/objects/object_render_distance.h
// Implementation: volition/rfg/code/objects/object_render_distance.cpp
//
// Distance used for render LOD decisions of humans, vehicles and items: the distance to the camera scaled by the
// camera's zoom, cached per frame (last_frame_processed).
#pragma once


class vector;

// size 0x8
class object_render_distance
{
public:
    object_render_distance(); // 0x00A05670 (.h:20)
    float get_apparent_distance(const vector& pos) const; // 0x00A2C5C0 (.h:22)
protected:
    void update(const vector& pos); // 0x00A1B0D0 (:16)

    float apparent_distance; // 0x00
    int last_frame_processed; // 0x04
};
