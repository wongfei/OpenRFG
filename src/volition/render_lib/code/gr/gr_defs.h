// Original: volition/render_lib/code/gr/gr_defs.h
//
// gr state and mode enums (alpha, z-buffer, clamp, cull, stencil, transform/lighting modes) and gr_state. Placement of
// the enums inferred.
#pragma once

enum gr_alpha_mode
{
    ALPHA_BLEND_NONE = 0,
    ALPHA_BLEND_ADDITIVE = 1,
    ALPHA_BLEND_ALPHA_ADDITIVE = 2,
    ALPHA_BLEND_ALPHA_ADDITIVE_CONST = 3,
    ALPHA_BLEND_ALPHA_ADDITIVE_SCREEN = 4,
    ALPHA_BLEND_FILTER = 5,
    ALPHA_BLEND_FILTER_SCREEN = 6,
    ALPHA_BLEND_FILTER_SCREEN_INVERSE = 7,
    ALPHA_BLEND_FILTER_CONST = 8,
    ALPHA_BLEND_SUBTRACTIVE = 9,
    ALPHA_BLEND_ALPHA_SUBTRACTIVE = 10,
    ALPHA_BLEND_ALPHA_SUBTRACTIVE_CONST = 11,
    ALPHA_BLEND_ALPHA_MULTIPLY = 12,
    ALPHA_BLEND_FILTER_PREMULT_ALPHA = 13,
};

enum gr_clamp_mode
{
    CLAMP_MODE_WRAP = 0,
    CLAMP_MODE_CLAMP = 1,
};

enum gr_cull_mode
{
    CULL_MODE_NONE = 0,
    CULL_MODE_CULL = 1,
};

enum gr_stencil_mode
{
    STENCIL_NONE = 0,
    STENCIL_WRITE = 1,
    STENCIL_TEST = 2,
};

enum gr_tnl_clip_mode
{
    TNL_CLIP_MODE_NONE = 0,
    TNL_CLIP_MODE_GUARD_BAND = 1,
    TNL_CLIP_MODE_CLIP = 2,
};

enum gr_tnl_light_mode
{
    TNL_LIGHT_MODE_NONE = 0,
    TNL_LIGHT_MODE_LIGHT = 1,
};

enum gr_zbuf_mode
{
    ZBUF_NONE = 0,
    ZBUF_READ = 1,
    ZBUF_READ_EQUAL = 2,
    ZBUF_WRITE = 3,
    ZBUF_FULL = 4,
};
