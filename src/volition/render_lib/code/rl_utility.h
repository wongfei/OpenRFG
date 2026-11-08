// Original: volition/render_lib/code/rl_utility.h
// Implementation: volition/render_lib/code/rl_utility.cpp
//
// Miscellaneous render_lib helpers, and line (a point and a direction, used to unproject screen positions; placement
// inferred, its constructor is compiled in render_lib).
#pragma once

#include "volition/vlib/code/math/vector3.h"

class matrix;

// size 0x18
struct line
{
    line(); // 0x004B96D0

    vector point; // 0x00
    vector dir; // 0x0C
};

// template instance: void rl_swap<vector>(vector& v1, vector& v2); // 0x006E57D0 (.h:18)
// template instance: void rl_swap<float>(float& v1, float& v2); // 0x006E59C0 (.h:18)
void rl_util_massage_tgn_filename_in_place(char* filename); // 0x004D4530 (:14)
void rl_create_color_correction_matrix(float H, float S, float V, matrix& color_correction_matrix); // 0x004D4560 (:24)

// 0x004B96D0: nothing to set
inline line::line()
{
}
