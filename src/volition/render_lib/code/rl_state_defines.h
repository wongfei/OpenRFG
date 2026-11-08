// Original: volition/render_lib/code/rl_state_defines.h
//
// Rasterizer mode enums of render_lib (rl_cull_mode, rl_fill_mode). Placement inferred.
#pragma once

enum rl_cull_mode
{
    RL_CULL_CCW = 0,
    RL_CULL_CW = 1,
    RL_CULL_NONE = 2,
    RL_CULL_SHADER_DRIVEN = 3,
    NUM_RL_CULL_MODE_VALS = 4,
};

enum rl_fill_mode
{
    RL_FILL_POINT = 0,
    RL_FILL_LINE = 1,
    RL_FILL_SOLID = 2,
    NUM_RL_FILL_MODE_VALS = 3,
};
