// Original: volition/render_lib/code/rl_hdr_state.h
// Implementation: volition/render_lib/code/rl_hdr_state.cpp
//
// rl_hdr_state, HDR parameters (exposure, bloom) blended by time of day.
#pragma once

#include "volition/vlib/code/math/vector3.h"

// size 0x68
class rl_hdr_state
{
public:
    rl_hdr_state(const rl_hdr_state& __that); // 0x004C7550
    rl_hdr_state(); // 0x004C1F80 (:60)
    ~rl_hdr_state(); // 0x004C2090 (:88)
    void update_overridden_values(); // 0x004C20A0 (:93)

    bool m_enable; // 0x00
    float m_lum_range; // 0x04
    float m_lum_offset; // 0x08
    float m_bloom_exposure; // 0x0C
    vector m_luminance_conversion; // 0x10
    float m_iris_rate; // 0x1C
    float m_luminance_max; // 0x20
    float m_luminance_min; // 0x24
    float m_luminance_mask_max; // 0x28
    float m_brightpass_threshold; // 0x2C
    float m_brightpass_offset; // 0x30
    bool m_bloom_soft; // 0x34
    bool m_bloom_alternate; // 0x35
    bool m_tone_mapped_bloom; // 0x36
    bool m_use_hdr_level; // 0x37
    float m_eye_adaption_base; // 0x38
    float m_eye_adaption_amount; // 0x3C
    float m_eye_fade_min; // 0x40
    float m_eye_fade_max; // 0x44
    float m_bloom_amount; // 0x48
    bool m_bloom_new; // 0x4C
    float m_bloom_theta; // 0x50
    float m_bloom_slope_A; // 0x54
    float m_bloom_slope_B; // 0x58
    vector m_sun_shafts_position; // 0x5C
};
