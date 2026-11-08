// Original: volition/rfg/code/geom/cloth_simulation_shared_spu.h
// Implementation: volition/rfg/code/geom/cloth_simulation_shared_spu.cpp
//
// The cloth solver (written to run as a PS3 SPU job, here on the job scheduler): closest point helpers and the collision
// record of a node against a primitive (cloth_simulation::simulate is defined in this file). Placement of cs_collision
// inferred.
#pragma once

#include "volition/vlib/code/math/vector3.h"

// size 0x10
struct cs_collision
{
    cs_collision(); // 0x007F2E90

    vector hit_point; // 0x00
    float fraction; // 0x0C
};

float cs_closest_point_on_line_segment(vector& ix, const vector& pnt, const vector& p0, const vector& p1); // 0x007F2720 (:36)
bool cs_closest_line_line(cs_collision& col_a, cs_collision& col_b, const vector& p1, const vector& p2, const vector& p3, const vector& p4); // 0x007F29A0 (:67)
