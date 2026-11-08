// Original: volition/render_lib/code/rl_hdr_state.cpp
//
// rl_hdr_state: the HDR/bloom parameters of a scene, normally set from the time of day. The RL_hdr_* globals are the
// debug console overrides (development builds); update_overridden_values copies them in when RL_hdr_override is set,
// and always copies the bloom-new flag and the brightpass values.

#include "volition/render_lib/code/rl_hdr_state.h"

// The override values (.data initial values).
bool RL_hdr_override = true; // 0x014FA28B
bool RL_hdr_enable = true; // 0x014FA2B4
bool RL_hdr_bloom_soft = true; // 0x014FA2B5
bool RL_hdr_bloom_alternate = true; // 0x014FA2B6
bool RL_hdr_tone_mapped_bloom = true; // 0x014FA2B7
float RL_hdr_lum_range = 3.0f; // 0x014FA2B8
float RL_hdr_lum_offset = 10.0f; // 0x014FA2BC
float RL_hdr_level = 0.6f; // 0x014FA2C0
float RL_hdr_iris_rate = 3.0f; // 0x014FA2C4
float RL_hdr_luminance_max = 4.0f; // 0x014FA2C8
float RL_hdr_luminance_min = 0.001f; // 0x014FA2CC
float RL_hdr_luminance_mask_max = 4.0f; // 0x014FA2D0
float RL_hdr_brightpass_threshold = 3.0f; // 0x014FA2D4
float RL_hdr_brightpass_offset = 6.0f; // 0x014FA2D8
bool RL_hdr_use_hdr_level = true; // 0x014FA2DC
bool RL_hdr_bloom_new = true; // 0x014FA2DD
float RL_hdr_eye_adaption_base = 0.2f; // 0x014FA2E0
float RL_hdr_eye_adaption_amount = 0.5f; // 0x014FA2E4
float RL_hdr_eye_fade_min = 1.0f; // 0x014FA2E8
float RL_hdr_eye_fade_max = 1.0f; // 0x014FA2EC
float RL_hdr_bloom_amount = 2.5f; // 0x014FA2F0
float RL_hdr_bloom_theta = 1.2f; // 0x014FA2F4
float RL_hdr_bloom_slope_B = 0.08f; // 0x014FA2F8
// Magic numbers: the Rec. 709 luminance weights.
vector RL_hdr_luminance_conversion(0.2125f, 0.7154f, 0.0721f); // 0x01514B98
bool RL_hdr_bloom_super_soft = false; // 0x01B7BCA6
bool RL_hdr_eye_override = false; // 0x01B7BCA7
float RL_hdr_bloom_slope_A = 0.0f; // 0x01B7BCC4 (.bss, never written)

// 0x004C1F80 (:60)
// Notes (decompiled behaviour, kept as is): m_sun_shafts_position is left uninitialized.
rl_hdr_state::rl_hdr_state()
{
    m_lum_offset = 10.0f;
    m_bloom_exposure = 0.5f;
    m_lum_range = 3.0f;
    m_iris_rate = 3.0f;
    m_luminance_max = 4.0f;
    m_luminance_conversion.x = 0.2125f;
    m_luminance_conversion.y = 0.7154f;
    m_luminance_min = 0.001f;
    m_luminance_mask_max = 1.0f;
    m_brightpass_threshold = 1.4f;
    m_eye_fade_min = 1.0f;
    m_eye_fade_max = 1.0f;
    m_bloom_amount = 1.0f;
    m_brightpass_offset = 50.0f;
    m_bloom_theta = 1.15f;
    m_eye_adaption_base = 0.2f;
    m_bloom_slope_A = 0.01f;
    m_enable = true;
    m_luminance_conversion.z = 0.0721f;
    // One dword store of 0x01010101 sets all four flags.
    m_bloom_soft = true;
    m_bloom_alternate = true;
    m_tone_mapped_bloom = true;
    m_use_hdr_level = true;
    m_eye_adaption_amount = 0.3f;
    m_bloom_new = true;
    m_bloom_slope_B = 0.1f;
}

// 0x004C7550 (compiler-generated)
rl_hdr_state::rl_hdr_state(const rl_hdr_state& __that) = default;

// 0x004C2090 (:88)
rl_hdr_state::~rl_hdr_state()
{
}

// 0x004C20A0 (:93)
void rl_hdr_state::update_overridden_values()
{
    if (RL_hdr_override)
    {
        m_bloom_exposure = RL_hdr_level;
        m_luminance_conversion = RL_hdr_luminance_conversion;
        m_iris_rate = RL_hdr_iris_rate;
        m_luminance_max = RL_hdr_luminance_max;
        m_luminance_min = RL_hdr_luminance_min;
        m_luminance_mask_max = RL_hdr_luminance_mask_max;
        m_brightpass_threshold = RL_hdr_brightpass_threshold;
        m_brightpass_offset = RL_hdr_brightpass_offset;
        m_eye_fade_min = RL_hdr_eye_fade_min;
        m_eye_fade_max = RL_hdr_eye_fade_max;
        m_bloom_amount = RL_hdr_bloom_amount;
        m_bloom_soft = RL_hdr_bloom_soft;
        m_bloom_alternate = RL_hdr_bloom_alternate;
        m_bloom_theta = RL_hdr_bloom_theta;
        m_bloom_slope_A = RL_hdr_bloom_slope_A;
        m_tone_mapped_bloom = RL_hdr_tone_mapped_bloom;
        m_use_hdr_level = RL_hdr_use_hdr_level;
        m_bloom_slope_B = RL_hdr_bloom_slope_B;
    }
    m_bloom_new = RL_hdr_bloom_new;
    m_brightpass_threshold = RL_hdr_brightpass_threshold;
    m_brightpass_offset = RL_hdr_brightpass_offset;
    if (RL_hdr_eye_override)
    {
        m_eye_adaption_base = RL_hdr_eye_adaption_base;
        m_eye_adaption_amount = RL_hdr_eye_adaption_amount;
        m_lum_range = RL_hdr_lum_range;
        m_lum_offset = RL_hdr_lum_offset;
    }
}
