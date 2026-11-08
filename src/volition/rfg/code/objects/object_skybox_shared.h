// Original: volition/rfg/code/objects/object_skybox_shared.h
// Implementation: volition/rfg/code/objects/object_skybox_shared.cpp
//
// Sky box data shared with the tools (horizon mountain colors and fog). Placement of horizon_mountain_parameters
// inferred.
#pragma once

#include "volition/render_lib/code/rl_color.h"
#include "volition/vlib/code/vlib_core/vlib_core.h"

class mempool;
class mempool_base;

// size 0x38
struct horizon_mountain_parameters
{
    horizon_mountain_parameters(); // 0x00A1C380

    rl_color_float color_front; // 0x00
    rl_color_float color_back; // 0x10
    rl_color_float fog_color; // 0x20
    float fog_density; // 0x30
    bool enabled; // 0x34
};

void object_skybox_load_shared(mempool_base* tmp_pool, mempool* pool, bool load_from_preload, vlib_platform plat); // 0x00A1C440 (:6)
