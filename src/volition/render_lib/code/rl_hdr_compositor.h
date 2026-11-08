// Original: volition/render_lib/code/rl_hdr_compositor.h
// Implementation: volition/render_lib/code/rl_hdr_compositor.cpp
//
// rl_hdr_compositor, HDR tone mapping and bloom.
#pragma once

#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_device.h"
#include "volition/render_lib/code/rl_render_texture.h"
#include "volition/render_lib/code/rl_sampling_offsets.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/render_lib/code/rl_shader.h"

class rl_camera;
class rl_class_dynamic_info;
class rl_color_correct_state;
class rl_color_float;
class rl_compositor;
class rl_hdr_state;
class rl_renderer;
class rl_scene_renderer;

namespace keen
{
    class GraphicsCommandWriter;
}

// size 0xC40
class rl_hdr_compositor : public rl_base_object
{
    // Access inferred: the scene renderer reads and writes these members directly.
    friend class rl_scene_renderer;
    // Access inferred from rl_renderer::renew_all_resources.
    friend class rl_renderer;
protected:
    rl_hdr_compositor(); // 0x005253F0 (:44)
    virtual ~rl_hdr_compositor(); // vtbl+0x00 0x00525750 (:49)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_hdr_compositor* create(); // 0x0053B940 (:36)
    static bool can_create(); // 0x004C1EF0 (:36)
    virtual void release(); // vtbl+0x04 0x004C1F00 (:36)
    void initialize(const unsigned int width, const unsigned int height, const rl_level_of_detail& lod, keen::GraphicsCommandWriter* pCommandWriter); // 0x0054E820 (:64)
    void create_render_textures(keen::GraphicsCommandWriter* pCommandWriter); // 0x0054B200 (:634)
    void shutdown(); // 0x00525970 (:165)
    void calculate_particle_fog_color(rl_renderer& renderer, rl_compositor& compositor, const rl_color_float& fog_color, const rl_dev_render_target& fog_color_target, const int source_tex, const rl_hdr_state& hdr_state, const rl_color_correct_state& color_correct_state); // 0x0053D4C0 (:562)
    void composite(rl_renderer& renderer, rl_compositor& compositor, rl_camera& camera, const int particle_texture_handle, const int particle_brightpass_texture_handle, const rl_dev_texture& sunShaftsMaskTex, const rl_hdr_state& hdr_state, const rl_color_correct_state& color_correct_state, const int source_tex, const rl_dev_render_target& dest_rt); // 0x0054A9F0 (:177)
    void render_sun_shafts(rl_renderer& renderer, rl_compositor& compositor, rl_camera& camera, const int source_tex, const int src_width, const int src_height, const rl_dev_texture& sunShaftsMaskTex, const rl_hdr_state& hdr_state, rl_render_texture& final_bloom_stage); // 0x0053D7A0 (:716)
    void release_resources(); // 0x00510000 (:596)
    void renew_resources(keen::GraphicsCommandWriter* pCommandWriter); // 0x0054F390 (:618)
    void resize(int w, int h); // 0x004E83E0 (:681)
    void set_level_of_detail(const rl_level_of_detail& lod); // 0x004C1F50 (:711)
    void save_textures(rl_scene_renderer& scene_renderer); // 0x004C1F70 (:791)
private:
    // Not in the binary: the luminance target setup create_render_textures inlines.
    static void init_luminance_target(rl_render_texture& target, unsigned int width, unsigned int height, const char* pDebugName);
    static int get_downsampled_width(int width); // 0x004C1F20 (:54)
    static int get_downsampled_height(int height); // 0x004C1F30 (:59)
    void compute_sampling_offsets(); // 0x004E7DB0 (:291)
    void compute_sampling_offsets_3(); // 0x0050FD30 (:305)
    void upload_tone_mapping_constantants(rl_renderer& renderer, const rl_hdr_state& hdr_state, const rl_color_correct_state& color_correct_state); // 0x005259E0 (:254)
    void measure_luminance(rl_renderer& renderer, rl_compositor& compositor, const rl_hdr_state& hdr_state, const rl_color_correct_state& color_correct_state, const rl_color_float& particle_fog_color, const rl_dev_render_target& fog_color_target); // 0x0053B9A0 (:322)
    void upload_sampling_offsets(rl_renderer& renderer, const rl_sampling_offsets& offsets); // 0x004E8080 (:533)
    void bright_pass_filter(rl_renderer& renderer, rl_compositor& compositor, const int particle_brightpass_texture_handle, const rl_hdr_state& hdr_state, const rl_color_correct_state& color_correct_state); // 0x0053C720 (:391)
    void create_bloom_texture(rl_renderer& renderer, rl_compositor& compositor, const int src_texture, int src_width, int src_height, const rl_dev_render_target& dest_texture, int dest_width, int dest_height); // 0x0053CAF0 (:422)
    void blur_bloom_texture(rl_renderer& renderer, rl_compositor& compositor, rl_render_texture* tex, int width, int height, const rl_sampling_offsets& kernel); // 0x0053CB30 (:429)
    void render_bloom(rl_renderer& renderer, rl_compositor& compositor, const rl_hdr_state& hdr_state); // 0x0053CBE0 (:439)
    void render_bloom_3(rl_renderer& renderer, rl_compositor& compositor, const rl_hdr_state& hdr_state); // 0x0053CE50 (:476)
    void bloom_composite(rl_renderer& renderer, rl_compositor& compositor, const rl_dev_render_target& dest_rt, const unsigned int width, const unsigned int height, bool sun_shafts_enabled); // 0x0053D090 (:511)
    void set_current_adapted_luminance_target_index(const int idx); // 0x004C1F40 (:557)
    int get_current_adapted_luminance_target_index(); // 0x004E80E0 (:546)
    int get_current_adapted_luminance_source_index(); // 0x004E8100 (:552)

public:
    static rl_class_dynamic_info ms_class_dynamic_info; // 0x01514B80
private:
    static const int kDownsampleTechnique; // 0x012BEDF4
    static const int kDownsampleMaskTechnique; // 0x012BEDF8
    static const int LUM_STAGE_0_WIDTH; // 0x012BEDFC
    static const int LUM_STAGE_0_HEIGHT; // 0x012BEE00
    static const int LUM_DOWN_SAMPLE_WIDTH; // 0x012BEE04
    static const int LUM_DOWN_SAMPLE_HEIGHT; // 0x012BEE08
    static const int LUM_STAGE_1_WIDTH; // 0x012BEE0C
    static const int LUM_STAGE_1_HEIGHT; // 0x012BEE10
    static const int LUM_ADAPT_WIDTH; // 0x012BEE14
    static const int LUM_ADAPT_HEIGHT; // 0x012BEE18
    int m_bloom_stage_4_width; // 0x08
    int m_bloom_stage_4_height; // 0x0C
    int m_bloom_stage_3_width; // 0x10
    int m_bloom_stage_3_height; // 0x14
    int m_bloom_stage_2_width; // 0x18
    int m_bloom_stage_2_height; // 0x1C
    int m_bloom_stage_1_width; // 0x20
    int m_bloom_stage_1_height; // 0x24
    int m_bloom_stage_0_width; // 0x28
    int m_bloom_stage_0_height; // 0x2C
    int m_bloom_stage_0_opt_width; // 0x30
    int m_bloom_stage_0_opt_height; // 0x34
    rl_downscale_sampling_offsets<9> m_downscale_sampling_lum_stage_0; // 0x38
    rl_downscale_sampling_offsets<16> m_downscale_sampling_lum_stage_1; // 0xD0
    rl_downscale_sampling_offsets<16> m_downscale_sampling_fullscreen; // 0x1D8
    rl_gaussian5x5_blur_kernel m_bloom_blur_stage_0_opt; // 0x2E0
    rl_gaussian5x5_blur_kernel m_bloom_blur_stage_0; // 0x3F8
    rl_gaussian5x5_blur_kernel m_bloom_blur_stage_1; // 0x510
    rl_gaussian5x5_blur_kernel m_bloom_blur_stage_2; // 0x628
    rl_gaussian5x5_blur_kernel m_bloom_blur_stage_3; // 0x740
    rl_gaussian5x5_blur_kernel m_bloom_blur_stage_4; // 0x858
    int m_downsampled_width; // 0x970
    int m_downsampled_height; // 0x974
    rl_dev_texture m_sun_mask_texture; // 0x978
    rl_render_texture m_downsampled_source; // 0x97C
    rl_render_texture m_luminance_stage_0; // 0x990
    rl_render_texture m_downsampled_lum_0; // 0x9A4
    rl_render_texture m_downsampled_lum_1; // 0x9B8
    rl_render_texture m_luminance_stage_1; // 0x9CC
    rl_render_texture* m_adapted_lum_p; // 0x9E0
    rl_render_texture m_brightpass[2]; // 0x9E4
    rl_render_texture m_bloom_stage_1[2]; // 0xA0C
    rl_render_texture m_bloom_stage_2[2]; // 0xA34
    rl_render_texture m_bloom_stage_3[2]; // 0xA5C
    rl_render_texture m_bloom_stage_4[2]; // 0xA84
    rl_render_texture m_downsampled_sun_shafts; // 0xAAC
    rl_render_texture m_downsampled_sun_shafts_int; // 0xAC0
    unsigned int m_hdr_shader_handle; // 0xAD4
    rl_shader_technique_ref m_luminance_stage_0_tech; // 0xAD8
    rl_shader_technique_ref m_luminance_stage_1_tech; // 0xADC
    rl_shader_technique_ref m_luminance_adapt_tech; // 0xAE0
    rl_shader_technique_ref m_brightpass_tech; // 0xAE4
    rl_shader_technique_ref m_brightpass_tech_alt; // 0xAE8
    rl_shader_technique_ref m_brightpass_tech_alt2; // 0xAEC
    rl_shader_technique_ref m_bloom_composite_tech; // 0xAF0
    rl_shader_technique_ref m_bloom_composite_3_tech; // 0xAF4
    rl_shader_technique_ref m_tech_final_composite_tonemapped_bloom; // 0xAF8
    rl_shader_technique_ref m_tech_final_composite_bloom_after_tonemapping; // 0xAFC
    rl_shader_technique_ref m_tech_final_composite_no_tonemapping; // 0xB00
    rl_shader_technique_ref m_tech_final_composite_tonemapping_no_bloom; // 0xB04
    rl_shader_technique_ref m_tech_final_composite_debug; // 0xB08
    rl_shader_technique_ref m_tech_compute_particle_fog_color; // 0xB0C
    rl_shader_technique_ref m_light_shafts_render_tech; // 0xB10
    rl_shader_technique_ref m_light_shafts_mask_tech; // 0xB14
    rl_shader_const_ref m_sampling_offsets_const; // 0xB18
    rl_shader_const_ref m_adapt_time_const; // 0xB1C
    rl_shader_const_ref m_hdr_level_const; // 0xB20
    rl_shader_const_ref m_luminance_conversion_const; // 0xB24
    rl_shader_const_ref m_luminance_max; // 0xB28
    rl_shader_const_ref m_luminance_min; // 0xB2C
    rl_shader_const_ref m_luminance_mask_max; // 0xB30
    rl_shader_const_ref m_luminance_whitepoint; // 0xB34
    rl_shader_const_ref m_brightpass_threshold; // 0xB38
    rl_shader_const_ref m_brightpass_offset; // 0xB3C
    rl_shader_const_ref m_tone_mapped_bloom_const; // 0xB40
    rl_shader_const_ref m_use_hdr_level_const; // 0xB44
    rl_shader_const_ref m_eye_adaption_base; // 0xB48
    rl_shader_const_ref m_eye_adaption_amount; // 0xB4C
    rl_shader_const_ref m_eye_fade_min_const; // 0xB50
    rl_shader_const_ref m_eye_fade_max_const; // 0xB54
    rl_shader_const_ref m_bloom_amount; // 0xB58
    rl_shader_const_ref m_bloom_curve_values_const; // 0xB5C
    rl_shader_const_ref m_debug_bloom_buffer; // 0xB60
    rl_shader_const_ref m_debug_lummap; // 0xB64
    rl_shader_const_ref m_debug_avg_lum; // 0xB68
    rl_shader_const_ref m_lum_range; // 0xB6C
    rl_shader_const_ref m_lum_offset; // 0xB70
    rl_shader_const_ref m_base_sampler; // 0xB74
    rl_shader_const_ref m_mask_sampler; // 0xB78
    rl_shader_const_ref m_sun_sampler; // 0xB7C
    rl_shader_const_ref m_particle_brightpass_sampler; // 0xB80
    rl_shader_const_ref m_particle_sampler; // 0xB84
    rl_shader_const_ref m_lum_adapt_sampler; // 0xB88
    rl_shader_const_ref m_bloom_stage_0_sampler; // 0xB8C
    rl_shader_const_ref m_bloom_stage_1_sampler; // 0xB90
    rl_shader_const_ref m_bloom_stage_2_sampler; // 0xB94
    rl_shader_const_ref m_bloom_stage_3_sampler; // 0xB98
    rl_shader_const_ref m_bloom_stage_4_sampler; // 0xB9C
    rl_shader_const_ref m_light_shafts_brightpass_sampler; // 0xBA0
    int m_target_adapted_lum_buffer; // 0xBA4
    unsigned int m_last_luminance_calc; // 0xBA8
    rl_level_of_detail m_level_of_detail; // 0xBAC
};
