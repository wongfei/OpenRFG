// Original: volition/render_lib/code/rl_decal_internal.h
//
// rl_decal_deferred_info, decals queued for creation on the render thread. VFX.md FX7 adds the steps every
// render_decals inlines (not in the binary as functions): the screen mapping of the scissor projection, the world box
// of a decal on its owner, and the scissor rectangle of its projection.
#pragma once

#include "volition/render_lib/code/rl_decal.h"
#include "volition/render_lib/code/rl_decal_defines.h"
#include "volition/render_lib/code/rl_decal_parameters.h"
#include "volition/render_lib/code/rl_rect.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/matrix.h"

class rl_renderable_instance;

// size 0x88
class rl_decal_deferred_info
{
public:
    rl_decal_deferred_info(); // 0x004BCC80 (.h:30)
    ~rl_decal_deferred_info();
    rl_decal_type get_type();

    rl_decal_parameters m_params; // 0x00
    rl_renderable_instance* m_inst; // 0x80
    unsigned int m_reserved_decal_handle; // 0x84
};

// Decal flags (rl_decal::m_decal_flags; names inferred).
static const unsigned int RLDF_DOUBLE_SIDED = 0x8;
static const unsigned int RLDF_FADING_OUT = 0x10;

// The decal pass states of the mesh and compound render_decals (rl_primitive_state bits, meanings inferred): the
// current state with alpha mode 2 (5 in the base pass, bits 0-4), depth mode 1 (read, bits 6-8), bits 21-22 cleared
// and 0x140000 set (bits 18-22); stencil, culling and the rest kept. A double sided decal clears the culling bits
// (0x6000).
static const unsigned int Decal_state_keep_mask = 0xFF9FFE62;
static const unsigned int Decal_state_keep_mask_base_pass = 0xFF9FFE65;
static const unsigned int Decal_state_bits = 0x140042;
static const unsigned int Decal_state_bits_base_pass = 0x140045;
static const unsigned int Decal_state_cull_bits = 0x6000;

// Not in the binary: the clip-to-screen matrix of the scissor projection for a screen of width x height (x to
// [0, width], y flipped to [height, 0]).
inline matrix44 rl_decal_scissor_clip_screen(int width, int height)
{
    const float half_w = (float)width * 0.5f;
    const float half_h = (float)height;
    // Magic numbers: the half screen scales and offsets.
    matrix44 clip_screen;
    clip_screen.set(half_w, 0.0f, 0.0f, 0.0f, 0.0f, half_h * -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, half_w, half_h * 0.5f, 0.0f, 1.0f);
    return clip_screen;
}

// Not in the binary: the decal's box in world space on an owner whose (sub mesh) transform is base: the decal's
// relative position through base, its relative orientation times base's.
inline void rl_decal_world_box(vector& out_min, vector& out_max, const matrix43& base, const rl_decal* decal_p)
{
    const vector& p = decal_p->m_relative_pos;
    const vector pos(base.m_translation.x + p.x * base.m_rotation.rvec.x + base.m_rotation.uvec.x * p.y + base.m_rotation.fvec.x * p.z,
                     base.m_translation.y + p.x * base.m_rotation.rvec.y + base.m_rotation.uvec.y * p.y + base.m_rotation.fvec.y * p.z,
                     base.m_translation.z + p.x * base.m_rotation.rvec.z + base.m_rotation.uvec.z * p.y + base.m_rotation.fvec.z * p.z);
    const matrix orient = decal_p->m_relative_orient * base.m_rotation;
    const rl_aabb& box = decal_p->get_aabb();
    bbox_instance(out_min, out_max, orient, pos, box.min, box.max);
}

// Not in the binary: the scissor rectangle of a projected rectangle, each edge clamped to the screen (v - 0 < 0 gives
// 0, then 0 <= v - limit gives limit, as rfg.exe compares).
inline float rl_decal_clamp_edge(float v, float limit)
{
    if (v - 0.0f < 0.0f)
    {
        v = 0.0f;
    }
    if (0.0f <= v - limit)
    {
        v = limit;
    }
    return v;
}

inline rl_rect rl_decal_scissor_rect(float x, float y, float w, float h, float screen_w, float screen_h)
{
    return rl_rect((int)rl_decal_clamp_edge(x, screen_w), (int)rl_decal_clamp_edge(y, screen_h), (int)rl_decal_clamp_edge(x + w, screen_w), (int)rl_decal_clamp_edge(y + h, screen_h));
}
