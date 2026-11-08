// Original: volition/render_lib/code/rl_primitive_state.h
// Implementation: volition/render_lib/code/rl_primitive_state.cpp
//
// rl_primitive_state, a packed macro state (the rl_prim_* modes) selecting precreated device states.
#pragma once

#include "volition/render_lib/code/rl_primitive_defines.h"

// size 0x4
class rl_primitive_state
{
public:
    rl_primitive_state(rl_prim_alpha_mode am, rl_prim_clamp_mode cm, rl_prim_zbuf_mode zm, rl_prim_stencil_mode stm, unsigned int const_alpha, rl_prim_cull_mode tclm, bool scissor_enabled, bool msaa_enabled, rl_prim_zbias_mode zbm, rl_prim_color_write_mode cwm); // 0x004B71F0 (.h:173)
    rl_primitive_state(); // 0x004B71E0 (.h:167)
    void invalidate(); // 0x004B7280 (.h:202)
    bool valid() const;
    bool operator==(const rl_primitive_state&) const;
    rl_prim_alpha_mode get_alpha_mode() const; // 0x004B7290 (.h:218)
    rl_prim_clamp_mode get_clamp_mode() const; // 0x004B72A0 (.h:219)
    rl_prim_zbuf_mode get_zbuf_mode() const; // 0x004B72B0 (.h:220)
    rl_prim_stencil_mode get_stencil_mode() const; // 0x004B72C0 (.h:221)
    rl_prim_cull_mode get_cull_mode() const; // 0x004B72D0 (.h:222)
    unsigned int get_const_alpha() const; // 0x004B72E0 (.h:223)
    rl_prim_color_write_mode get_color_write_mode() const; // 0x004B72F0 (.h:225)
    bool get_scissor_enabled() const; // 0x004B7300 (.h:226)
    bool get_msaa_enabled() const; // 0x004B7310 (.h:227)
    rl_prim_zbias_mode get_zbias_mode() const; // 0x004B7320 (.h:228)
    void set_alpha_mode(rl_prim_alpha_mode am); // 0x004B7330 (.h:231)
    void set_clamp_mode(rl_prim_clamp_mode cm); // 0x004B7340 (.h:232)
    void set_zbuf_mode(rl_prim_zbuf_mode zm); // 0x004B7360 (.h:233)
    void set_stencil_mode(rl_prim_stencil_mode stm); // 0x004B7380 (.h:234)
    void set_cull_mode(rl_prim_cull_mode tclm); // 0x004B73A0 (.h:235)
    void set_const_alpha(unsigned int alpha); // 0x004B73C0 (.h:236)
    void set_color_write_mode(rl_prim_color_write_mode cwm); // 0x004B73E0 (.h:238)
    void set_scissor_enabled(bool enabled); // 0x004B7400 (.h:239)
    void set_msaa_enabled(bool enabled); // 0x004B7420 (.h:240)
    void set_zbias_mode(rl_prim_zbias_mode zm); // 0x004B7440 (.h:241)
    // Not in the binary: the packed bits, for the states the original builds as bit constants.
    unsigned int get_bits() const;
    void set_bits(unsigned int bits);

private:
    static const unsigned int kAlphaBits; // 0x012B8018
    static const unsigned int kClampBits; // 0x012B801C
    static const unsigned int kZBufBits; // 0x012B8020
    static const unsigned int kStencilBits; // 0x012B8024
    static const unsigned int kCullBits; // 0x012B8028
    static const unsigned int kColorWriteBits; // 0x012B802C
    static const unsigned int kAlphaRefBits; // 0x012B8030
    static const unsigned int kMsaaBits; // 0x012B8034
    static const unsigned int kScissorBits; // 0x012B8038
    static const unsigned int kDepthBiasBits; // 0x012B803C
    union
    {
        unsigned int m_bits; // 0x00
        struct
        {
            unsigned int m_alpha_mode : 5; // 0x00:0
            unsigned int m_clamp_mode : 1; // 0x00:5
            unsigned int m_zbuf_mode : 3; // 0x00:6
            unsigned int m_stencil_mode : 4; // 0x00:9
            unsigned int m_cull_mode : 2; // 0x00:13
            unsigned int m_color_write_mode : 3; // 0x00:15
            unsigned int m_scissor : 1; // 0x00:18
            unsigned int m_msaa : 1; // 0x00:19
            unsigned int m_zbias_mode : 3; // 0x00:20
            unsigned int m_const_alpha : 8; // 0x00:23
            unsigned int m_valid : 1; // 0x00:31
        } v; // 0x00
    } d; // 0x00
public:
    static const rl_primitive_state NO_ALPHA; // 0x01515028
    static const rl_primitive_state NO_ALPHA_Z; // 0x0151502C
    static const rl_primitive_state FILTER; // 0x01515030
    static const rl_primitive_state FILTER_SCREEN; // 0x01515048
    static const rl_primitive_state FILTER_Z; // 0x01515034
    static const rl_primitive_state FILTER_Z_READ; // 0x01515038
    static const rl_primitive_state ADDITIVE; // 0x0151503C
    static const rl_primitive_state ADDITIVE_ALPHA; // 0x01515040
    static const rl_primitive_state ADDITIVE_Z; // 0x01515044
    static const rl_primitive_state STENCIL_WRITE; // 0x0151504C
    static const rl_primitive_state STENCIL_TEST; // 0x01515050
    static const rl_primitive_state ALPHA_TEST; // 0x01515054
    static const rl_primitive_state ALPHA_TEST_Z; // 0x01515058
    static const rl_primitive_state TEXT; // 0x01C230D8
    static const rl_primitive_state DEFAULT; // 0x01B7BD7C
};

// ---- inline members

// 0x004B71E0 (.h:167)
// All zero: not valid.
inline rl_primitive_state::rl_primitive_state()
{
    d.m_bits = 0;
}

// 0x004B71F0 (.h:173)
// Packs the modes; the valid bit is set.
inline rl_primitive_state::rl_primitive_state(rl_prim_alpha_mode am, rl_prim_clamp_mode cm, rl_prim_zbuf_mode zm, rl_prim_stencil_mode stm, unsigned int const_alpha, rl_prim_cull_mode tclm, bool scissor_enabled, bool msaa_enabled, rl_prim_zbias_mode zbm, rl_prim_color_write_mode cwm)
{
    d.m_bits = 0;
    d.v.m_alpha_mode = am;
    d.v.m_clamp_mode = cm;
    d.v.m_zbuf_mode = zm;
    d.v.m_stencil_mode = stm;
    d.v.m_cull_mode = tclm;
    d.v.m_color_write_mode = cwm;
    d.v.m_scissor = scissor_enabled ? 1 : 0;
    d.v.m_msaa = msaa_enabled ? 1 : 0;
    d.v.m_zbias_mode = zbm;
    d.v.m_const_alpha = const_alpha;
    d.v.m_valid = 1;
}

// 0x004B7280 (.h:202)
inline void rl_primitive_state::invalidate()
{
    d.m_bits = 0;
}

// 0x004B7290 (.h:218)
inline rl_prim_alpha_mode rl_primitive_state::get_alpha_mode() const
{
    return (rl_prim_alpha_mode)d.v.m_alpha_mode;
}

// 0x004B72A0 (.h:219)
inline rl_prim_clamp_mode rl_primitive_state::get_clamp_mode() const
{
    return (rl_prim_clamp_mode)d.v.m_clamp_mode;
}

// 0x004B72B0 (.h:220)
inline rl_prim_zbuf_mode rl_primitive_state::get_zbuf_mode() const
{
    return (rl_prim_zbuf_mode)d.v.m_zbuf_mode;
}

// 0x004B72C0 (.h:221)
inline rl_prim_stencil_mode rl_primitive_state::get_stencil_mode() const
{
    return (rl_prim_stencil_mode)d.v.m_stencil_mode;
}

// 0x004B72D0 (.h:222)
inline rl_prim_cull_mode rl_primitive_state::get_cull_mode() const
{
    return (rl_prim_cull_mode)d.v.m_cull_mode;
}

// 0x004B72E0 (.h:223)
inline unsigned int rl_primitive_state::get_const_alpha() const
{
    return d.v.m_const_alpha;
}

// 0x004B72F0 (.h:225)
inline rl_prim_color_write_mode rl_primitive_state::get_color_write_mode() const
{
    return (rl_prim_color_write_mode)d.v.m_color_write_mode;
}

// 0x004B7300 (.h:226)
inline bool rl_primitive_state::get_scissor_enabled() const
{
    return d.v.m_scissor != 0;
}

// 0x004B7310 (.h:227)
inline bool rl_primitive_state::get_msaa_enabled() const
{
    return d.v.m_msaa != 0;
}

// 0x004B7320 (.h:228)
inline rl_prim_zbias_mode rl_primitive_state::get_zbias_mode() const
{
    return (rl_prim_zbias_mode)d.v.m_zbias_mode;
}

// 0x004B7330 (.h:231)
inline void rl_primitive_state::set_alpha_mode(rl_prim_alpha_mode am)
{
    d.v.m_alpha_mode = am;
}

// 0x004B7340 (.h:232)
inline void rl_primitive_state::set_clamp_mode(rl_prim_clamp_mode cm)
{
    d.v.m_clamp_mode = cm;
}

// 0x004B7360 (.h:233)
inline void rl_primitive_state::set_zbuf_mode(rl_prim_zbuf_mode zm)
{
    d.v.m_zbuf_mode = zm;
}

// 0x004B7380 (.h:234)
inline void rl_primitive_state::set_stencil_mode(rl_prim_stencil_mode stm)
{
    d.v.m_stencil_mode = stm;
}

// 0x004B73A0 (.h:235)
inline void rl_primitive_state::set_cull_mode(rl_prim_cull_mode tclm)
{
    d.v.m_cull_mode = tclm;
}

// 0x004B73C0 (.h:236)
inline void rl_primitive_state::set_const_alpha(unsigned int alpha)
{
    d.v.m_const_alpha = alpha;
}

// 0x004B73E0 (.h:238)
inline void rl_primitive_state::set_color_write_mode(rl_prim_color_write_mode cwm)
{
    d.v.m_color_write_mode = cwm;
}

// 0x004B7400 (.h:239)
inline void rl_primitive_state::set_scissor_enabled(bool enabled)
{
    d.v.m_scissor = enabled ? 1 : 0;
}

// 0x004B7420 (.h:240)
inline void rl_primitive_state::set_msaa_enabled(bool enabled)
{
    d.v.m_msaa = enabled ? 1 : 0;
}

// 0x004B7440 (.h:241)
inline void rl_primitive_state::set_zbias_mode(rl_prim_zbias_mode zm)
{
    d.v.m_zbias_mode = zm;
}
