// Original: volition/render_lib/code/rl_primitive_defines.h
//
// Primitive macro state enums (alpha, z-buffer, z-bias, clamp, cull, stencil, color write modes), primitive render
// types and the 2D/3D primitive vertex layouts. Placement inferred.
#pragma once

#include "volition/render_lib/code/rl_color.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"

enum rl_prim_alpha_mode
{
    RLPAM_NONE = 0,
    RLPAM_ADDITIVE = 1,
    RLPAM_ALPHA_ADDITIVE = 2,
    RLPAM_ALPHA_ADDITIVE_CONST = 3,
    RLPAM_ALPHA_ADDITIVE_SCREEN = 4,
    RLPAM_FILTER = 5,
    RLPAM_FILTER_SCREEN = 6,
    RLPAM_FILTER_SCREEN_INVERSE = 7,
    RLPAM_FILTER_CONST = 8,
    RLPAM_SUBTRACTIVE = 9,
    RLPAM_ALPHA_SUBTRACTIVE = 10,
    RLPAM_ALPHA_SUBTRACTIVE_CONST = 11,
    RLPAM_ALPHA_MULTIPLY = 12,
    RLPAM_FILTER_PREMULT_ALPHA = 13,
    RLPAM_MODULATE_ALPHA = 14,
    RLPAM_MODULATE_COLOR_MODULATE_ALPHA = 15,
    RLPAM_FILTER_MODULATE_ALPHA = 16,
    RLPAM_FILTER_MODULATE_PREMULT_ALPHA = 17,
    RLPAM_FILTER_MODULATE_INVERSE_ALPHA = 18,
    RLPAM_ALPHA_ADDITIVE_MODULATE_ALPHA = 19,
    RLPAM_ALPHA_SUBTRACTIVE_MODULATE_ALPHA = 20,
    RLPAM_ALPHA_TEST = 21,
    RLPAM_NUM_ALPHA_MODES = 22,
};

enum rl_prim_clamp_mode
{
    RLPCM_WRAP = 0,
    RLPCM_CLAMP = 1,
    RLPCM_NUM_CLAMP_MODES = 2,
};

enum rl_prim_color_write_mode
{
    RLPCW_UNDEFINED = 0,
    RLPCW_NONE = 1,
    RLPCW_R = 2,
    RLPCW_G = 3,
    RLPCW_RGB = 4,
    RLPCW_RGBA = 5,
    RLPCW_RB = 6,
    RLPCW_GA = 7,
    RLPCW_NUM_COLOR_WRITE_MODES = 8,
};

enum rl_prim_cull_mode
{
    RLPCM_NONE = 0,
    RLPCM_CULL = 1,
    RLPCM_FRONT = 2,
    RLPCM_BACK = 3,
    RLPCM_NUM_CULL_MODES = 4,
};

enum rl_prim_stencil_mode
{
    RLPSM_NONE = 0,
    RLPSM_WRITE = 1,
    RLPSM_TEST = 2,
    RLPSM_CLEAR = 3,
    RLPSM_WRITE_NOCOLOR = 4,
    RLPSM_CLEAR_NOCOLOR = 5,
    RLPSM_LAYERED_MATERIAL_MASKING = 6,
    RLPSM_LAYERED_MATERIAL_BASE = 7,
    RLPSM_LAYERED_MATERIAL_TEST_BASE_FINAL = 8,
    RLPSM_LAYERED_MATERIAL_TEST_BASE = 9,
    RLPSM_LAYERED_MATERIAL_TEST_MASKING_FINAL = 10,
    RLPSM_LAYERED_MATERIAL_TEST_MASKING = 11,
    RLPSM_LAYERED_MATERIAL_IMPLICIT_CLEAR = 12,
    RLPSM_NUM_STENCIL_MODES = 13,
};

enum rl_prim_zbias_mode
{
    RLPZB_NONE = 0,
    RLPZB_DECAL = 1,
    RLPZB_SHADOW = 2,
    RLPZB_DISTORTION = 3,
    RLPZB_TERRAIN_LOW_LOD = 4,
    RLPZB_NUM_ZBIAS_MODES = 5,
};

enum rl_prim_zbuf_mode
{
    RLPZM_NONE = 0,
    RLPZM_READ = 1,
    RLPZM_READ_EQUAL = 2,
    RLPZM_WRITE = 3,
    RLPZM_FULL = 4,
    RLPZM_NUM_ZBUF_MODES = 5,
};

enum rl_primitive_render_type
{
    RLPRT_INVALID = -1,
    RLPRT_LINELIST_2D = 0,
    RLPRT_TEXTURED_LINELIST_2D = 1,
    RLPRT_LINELIST_3D = 2,
    RLPRT_TEXTURED_LINELIST_3D = 3,
    RLPRT_TRISTRIP_2D = 4,
    RLPRT_TEXTURED_TRISTRIP_2D = 5,
    RLPRT_TRISTRIP_3D = 6,
    RLPRT_TEXTURED_TRISTRIP_3D = 7,
    RLPRT_BINKED_TRISTRIP_2D = 8,
    RLPRT_MATERIAL_TRISTRIP_2D = 9,
    RLPRT_MATERIAL_TRISTRIP_3D = 10,
    NUM_RL_PRIMITIVE_RENDER_TYPES = 11,
};

enum rl_primitive_renderer_mode
{
    RLPRM_INVALID = -1,
    RLPRM_IMMEDIATE = 0,
    RLPRM_DEFERRED = 1,
    NUM_RL_PRIMITIVE_RENDERER_MODES = 2,
};

// size 0x1C
struct rl_primitive_2d_vert
{
    rl_primitive_2d_vert(); // 0x004C7740

    vector2 pt; // 0x00
    vector2 unused_pad; // 0x08
    vector2 uvs; // 0x10
    rl_color color; // 0x18
};

// size 0x18
struct rl_primitive_3d_vert
{
    rl_primitive_3d_vert(); // 0x004BEB60

    vector pt; // 0x00
    vector2 uvs; // 0x0C
    rl_color color; // 0x14
};
