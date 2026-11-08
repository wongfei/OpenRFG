// Original: volition/render_lib/code/rl_particle_renderer.h
// Implementation: volition/render_lib/code/rl_particle_renderer.cpp
//
// rl_particle_renderer, fills particle vertex buffers (billboard, drop, oriented, radial, ribbon fillers) and draws
// them, also deferred; the bright-pass, distortion and framebuffer states. Placement of the states inferred.
#pragma once

#include "keen/base/types.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/rendergeometry_d3d11.hpp"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_device.h"
#include "volition/render_lib/code/rl_particle.h"
#include "volition/render_lib/code/rl_render_texture.h"
#include "volition/render_lib/code/rl_sampling_offsets.h"
#include "volition/render_lib/code/rl_shader.h"
#include "volition/render_lib/code/rl_singleton.h"
#include "volition/render_lib/code/shader/particleshaderdata.hpp"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/math/vector4.h"
#include "volition/vlib/code/util/varray.h"

class matrix43;
class mempool;
class rl_batched_pass;
class rl_camera;
class rl_class_dynamic_info;
class rl_compositor;
class rl_corona_instance;
struct rl_level_of_detail;
struct rl_particle_brightpass_state;
struct rl_particle_distortion_state;
struct rl_particle_framebuffer_state;
class rl_particle_lighting_state;
struct rl_particle_material_data;
class rl_renderable_instance;
class rl_renderable_particles;
class rl_renderable_particles_instance;
class rl_renderer;

namespace keen
{
    struct DynamicConstantBuffer;
}

// size 0xB520
class rl_particle_renderer : public rl_base_object, public rl_singleton_phoenix<rl_particle_renderer>
{
    // Access inferred: rl_render_lib and rl_renderer read the private members directly.
    friend class rl_render_lib;
    friend class rl_renderer;
    // rl_renderable_particles_instance::alloc_*_particles allocate from the particle pools directly (friends are not
    // recorded in rfg.pdb).
    friend class rl_renderable_particles_instance;
    friend class rl_particle_iterative;
public:
    // size 0xC
    struct corona_render_params
    {
        float m_intensity; // 0x00
        float m_opacity; // 0x04
        float m_visibility; // 0x08
    };

    // size 0x10
    struct particle_shader_techniques
    {
        particle_shader_techniques(); // 0x004C3DA0
        ~particle_shader_techniques(); // 0x004C3DC0

        rl_shader_technique_ref m_technique_refs[4]; // 0x00
    };

    // size 0x8
    struct particle_vertex_info
    {
        rl_dev_vertex_format format; // 0x00
        int stride; // 0x04
    };

    // size 0x34
    struct parametric_vertex
    {
        parametric_vertex(); // 0x004C3BF0

        vector position; // 0x00
        vector velocity; // 0x0C
        float rotation_angle; // 0x18
        float time_created; // 0x1C
        keen::float4 random; // 0x20
        float lifetime; // 0x30
    };

    // size 0x30
    struct billboard_vertex
    {
        billboard_vertex(); // 0x004C3BE0

        vector pos; // 0x00
        unsigned int rgba; // 0x0C
        float saturation; // 0x10
        vector2 uvs; // 0x14
        float rotation; // 0x1C
        float current_width; // 0x20
        float current_length; // 0x24
        float additive_key; // 0x28
        float anim_time; // 0x2C
    };

    // size 0x34
    struct radial_vertex
    {
        radial_vertex(); // 0x004C3C20

        vector pos; // 0x00
        vector emission_pos; // 0x0C
        unsigned int rgba; // 0x18
        float saturation; // 0x1C
        vector2 uvs; // 0x20
        float current_size; // 0x28
        float additive_key; // 0x2C
        float anim_time; // 0x30
    };

    // size 0x38
    struct drop_vertex
    {
        drop_vertex(); // 0x004C3C00

        vector pos; // 0x00
        vector vel; // 0x0C
        unsigned int rgba; // 0x18
        float saturation; // 0x1C
        vector2 uvs; // 0x20
        vector2 v_trans; // 0x28
        float additive_key; // 0x30
        float anim_time; // 0x34
    };

    // size 0x50
    struct ribbon_parametric_vertex
    {
        ribbon_parametric_vertex(); // 0x004C3C30

        vector4 pos_1; // 0x00
        vector4 pos_2; // 0x10
        vector2 uvs; // 0x20
        vector2 time_created; // 0x28
        keen::float4 random_1; // 0x30
        keen::float4 random_2; // 0x40
    };

    // size 0x44
    struct ribbon_vertex
    {
        ribbon_vertex(); // 0x004C3C40

        vector pos; // 0x00
        unsigned int rgba; // 0x0C
        float saturation; // 0x10
        vector2 uvs; // 0x14
        vector2 v_trans; // 0x1C
        vector vel1; // 0x24
        vector vel2; // 0x30
        float additive_key; // 0x3C
        float anim_time; // 0x40
    };

    // size 0x38
    struct oriented_vertex
    {
        oriented_vertex(); // 0x004C3C10

        vector pos; // 0x00
        unsigned int rgba; // 0x0C
        float saturation; // 0x10
        vector2 uvs; // 0x14
        float current_width; // 0x1C
        float current_length; // 0x20
        vector velocity; // 0x24
        float additive_key; // 0x30
        float anim_time; // 0x34
    };

    // size 0xB0
    struct corona_vertex
    {
        corona_vertex(); // 0x004C3F30

        vector pos; // 0x00
        float type; // 0x0C
        rl_color_float color_start; // 0x10
        float saturation; // 0x20
        float intensity; // 0x24
        vector2 uvs; // 0x28
        float rotation; // 0x30
        float width_scale; // 0x34
        float additive_key; // 0x38
        float anim_time; // 0x3C
        float intensity_exp_decay; // 0x40
        float opacity_exp_decay; // 0x44
        float rotation_offset; // 0x48
        float visibility; // 0x4C
        float opacity_visibility_decay; // 0x50
        float radius_visibility_decay; // 0x54
        float color_distance_start; // 0x58
        float color_distance_end; // 0x5C
        float radius_start; // 0x60
        float radius_end; // 0x64
        float radius_distance_start; // 0x68
        float radius_distance_end; // 0x6C
        rl_color_float color_end; // 0x70
        float intensity_view_fade_scale; // 0x80
        float radius_view_fade_scale; // 0x84
        float opacity_view_fade_scale; // 0x88
        float view_fade_decay; // 0x8C
        vector fvec; // 0x90
        float visibility_radius; // 0x9C
        float position_offset; // 0xA0
        vector pad0; // 0xA4
    };

protected:
    rl_particle_renderer(); // 0x00529820 (:205)
    virtual ~rl_particle_renderer(); // vtbl+0x00 0x00529AA0 (:242)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_particle_renderer* create(); // 0x00542110 (:43)
    static bool can_create(); // 0x004C3D70 (:43)
    virtual void release(); // vtbl+0x04 0x004C3D80 (:43)
    void initialize(const int width, const int height, const rl_level_of_detail& lod); // 0x0054FBE0 (:273)
    void downsample_depth(rl_renderer& renderer, rl_compositor& compositor, const rl_dev_texture& src_depth, const int src_width, const int src_height, const rl_dev_render_target& dst_depth, const rl_dev_depth_stencil_target& dst_ds_surface, const int dst_width, const int dst_height); // 0x00542170 (rl_particle_renderer_deferred.cpp:61)
    void create_render_targets(); // 0x0054C6C0 (:1255)
    void render_deferred(rl_renderer& renderer, rl_compositor& compositor, rl_camera& camera, const rl_particle_framebuffer_state& fullscreen, const rl_particle_framebuffer_state& halfscreen, const rl_particle_brightpass_state& brightpass, const rl_particle_distortion_state& distortion, const rl_particle_lighting_state& lighting_state, const int fog_color_texture_handle, const rl_color_float& particle_tint); // 0x0054C750 (rl_particle_renderer_deferred.cpp:15)
    void render(rl_renderer& renderer, const rl_camera& camera, rl_corona_instance* instance_p); // 0x0052B280 (:1140)
    void render(rl_renderer& renderer, const rl_camera& camera, rl_renderable_particles_instance* instance_p, const rl_particle_lighting_state& lighting_state, const bool is_distortion_pass); // 0x00529F80 (:582)
    void render_for_deferred(rl_corona_instance* instance_p); // 0x00513D60 (:577)
    void render_for_deferred(rl_renderable_particles_instance* instance_p); // 0x00513CD0 (:562)
    void add_half_res(rl_renderable_particles_instance* instance_p); // 0x00513A80 (:527)
    void add_quarter_res(rl_renderable_particles_instance* instance_p); // 0x00513AD0 (:532)
    void add_distortion(rl_renderable_particles_instance* instance_p); // 0x00513B20 (:537)
    void add_full_res(rl_renderable_particles_instance* instance_p); // 0x00513B70 (:542)
    void add_corona(rl_corona_instance* instance_p); // 0x00513BC0 (:547)
    void reset_deferred(); // 0x00513C10 (:552)
    void particle_pool_reset(); // 0x004C3DD0 (:349)
    rl_particle_iterative* alloc_iterative_particles(int count); // 0x004C3E50 (:373)
    rl_particle_base::tail_data* alloc_particle_tails(int count); // 0x004C3E90 (:387)
    rl_particle_parametric* alloc_parametric_particles(int count); // 0x004C3E10 (:359)
    static void set_particle_parametric_pool_size(int particle_parametric_pool_size); // 0x004C3EF0 (:413)
    static int get_particle_parametric_pool_size(); // 0x004C3F00 (:418)
    static void set_particle_iterative_pool_size(int particle_iterative_pool_size); // 0x004C3ED0 (:402)
    static int get_particle_iterative_pool_size(); // 0x004C3EE0 (:407)
    static void set_particle_tail_pool_size(int particle_tail_pool_size); // 0x004C3F10 (:423)
    static int get_particle_tail_pool_size(); // 0x004C3F20 (:428)
    void release_resources(); // 0x00513E80 (:1240)
    void renew_resources(); // 0x00550470 (:1250)
    void resize(int w, int h); // 0x004C4120 (:1270)
    void set_level_of_detail(const rl_level_of_detail& lod); // 0x004C4140 (:1276)
protected:
    void render_corona_internal(rl_renderer& renderer, const rl_camera& camera, rl_corona_instance* instance_p, int visibility_phase); // 0x0052A9F0 (:856)
    void render_mesh_distortion(rl_renderer& renderer, rl_camera& camera, rl_batched_pass& scene_batch, base_array<rl_renderable_instance*>& distortion_only_entries, unsigned int distortion_mat, const unsigned int vis_flags); // 0x00529BE0 (:460)
    void flushStates(rl_renderer& renderer); // 0x00529E40 (:511)
private:
    void create_buffers(); // 0x004F03C0 (:46)
    void draw_particles(rl_renderer& renderer, bool is_iterative, rl_particle_base::render_type current_render_type, unsigned int, unsigned int num_prims); // 0x004C3F40 (:1166)
    void set_deferred_render_states(rl_renderer& renderer, rl_camera& camera, const int depth_width, const int depth_height, const bool mod_alpha); // 0x0052B2C0 (rl_particle_renderer_deferred.cpp:107)
    void render_quarter_res_particles(rl_renderer& renderer, rl_camera& camera, const rl_particle_lighting_state& lighting_state, const int fog_color_texture_handle, const rl_color_float& particle_tint); // 0x0052B520 (rl_particle_renderer_deferred.cpp:143)
    void render_half_res_particles(rl_renderer& renderer, rl_camera& camera, const rl_dev_render_target& half_res_rt, const rl_dev_depth_stencil_target& half_ds_surface, const rl_dev_texture& half_depth_target, const int half_width, const int half_height, const rl_particle_lighting_state& lighting_state, const int fog_color_texture_handle, const rl_color_float& particle_tint); // 0x0052B880 (rl_particle_renderer_deferred.cpp:176)
    void composite_quarter_res_to_half_res(rl_renderer& renderer, rl_compositor& compositor, const rl_dev_render_target& half_res_rt, const int half_width, const int half_height); // 0x005424E0 (rl_particle_renderer_deferred.cpp:218)
    void stretch_half_res_to_full_res(rl_renderer& renderer, rl_compositor& compositor, const rl_dev_render_target& full_res_rt, const int full_res_width, const int full_res_height, const int half_res_rt); // 0x00542720 (rl_particle_renderer_deferred.cpp:236)
    void render_full_res_particles(rl_renderer& renderer, rl_camera& camera, const rl_dev_render_target& full_res_rt, const rl_dev_depth_stencil_target& ds_surface, const rl_dev_texture& full_res_depth_target, const int full_width, const int full_height, const rl_particle_lighting_state& lighting_state, const int fog_color_texture_handle, const rl_color_float& particle_tint); // 0x0052BC20 (rl_particle_renderer_deferred.cpp:254)
    void render_particle_brightpass(rl_renderer& renderer, rl_compositor& compositor, const rl_dev_render_target& target, const int width, const int height, const int full_res_tex); // 0x00542A90 (rl_particle_renderer_deferred.cpp:304)
    void render_distortion(rl_renderer& renderer, rl_camera& camera, const rl_dev_render_target& distortion_target, rl_dev_depth_stencil_target distortion_ds_surface, const rl_dev_texture& distortion_depth_texture, const int width, const int height, const unsigned int distortion_mat, rl_batched_pass& scene_batch, base_array<rl_renderable_instance*>& distortion_only_entries, const unsigned int vis_flags, const rl_particle_lighting_state& lighting_state, const int fog_color_texture_handle); // 0x0052C000 (rl_particle_renderer_deferred.cpp:330)
    void set_texture(rl_renderer& renderer, int stage, int ref); // 0x00513DB0 (:1148)
    void set_material(rl_renderer&, const rl_camera&, rl_renderable_particles*, rl_particle_material_data*, const bool, const bool, const matrix43&, const rl_particle_base::render_type, const bool);
    void filler_billboard(rl_renderable_particles_instance* instance_p, const bool is_iterative, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride); // 0x004EA510 (rl_particle_filler_billboard.cpp:115)
    void filler_radial(rl_renderable_particles_instance* instance_p, const bool is_iterative, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride); // 0x004EDFD0 (rl_particle_filler_radial.cpp:118)
    void filler_drop(rl_renderable_particles_instance* instance_p, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride); // 0x004EC190 (rl_particle_filler_drop.cpp:203)
    void filler_ribbon(rl_renderable_particles_instance* instance_p, const bool is_iterative, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride); // 0x004EFB30 (rl_particle_filler_ribbon.cpp:239)
    void filler_oriented(rl_renderable_particles_instance* instance_p, const bool is_iterative, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride); // 0x004EC8B0 (rl_particle_filler_oriented.cpp:74)
    void billboard_add_particle(const rl_particle_iterative& particle, float rotation_angle, const vector& position_in, const rl_color_float& current_color, float current_width, float current_length, bool rotation_enabled, bool local_space, rl_renderable_particles_instance* instance_p, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride); // 0x004E9F90 (rl_particle_filler_billboard.cpp:19)
    void billboard_add_particle_parametric(const rl_particle_parametric& particle, const rl_renderable_particles_instance* emitter_inst_p, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride); // 0x004EA410 (rl_particle_filler_billboard.cpp:78)
    void drop_add_particle(const rl_particle_iterative& particle, float rotation_angle, const vector& position_in, const rl_color_float& current_color, float current_size, bool rotation_enabled, bool local_space, int tail_length, rl_renderable_particles_instance* instance_p, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride); // 0x004EB160 (rl_particle_filler_drop.cpp:20)
    void oriented_add_particle(const rl_particle_iterative& particle, const vector& position_in, const rl_color_float& current_color, float current_width, float current_length, const vector& velocity_in, bool local_space, rl_renderable_particles_instance* instance_p, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride); // 0x004EC270 (rl_particle_filler_oriented.cpp:19)
    void radial_add_particle(const rl_particle_iterative& particle, float rotation_angle, const vector& position_in, const rl_color_float& current_color, float current_size, bool rotation_enabled, bool local_space, rl_renderable_particles_instance* instance_p, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride); // 0x004ED950 (rl_particle_filler_radial.cpp:20)
    void filler_ribbon_iterative(rl_renderable_particles_instance* instance_p, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride); // 0x004EF0F0 (rl_particle_filler_ribbon.cpp:110)
    void filler_ribbon_parametric(rl_renderable_particles_instance* instance_p, unsigned char*& vertex_p, unsigned int& num_verts, unsigned int& num_prims, const unsigned int current_vertex_stride); // 0x004EE880 (rl_particle_filler_ribbon.cpp:8)
    static float get_uv_anim_time(float elapsed_time, float random, float anim_rate); // 0x004BD710 (.h:434)
    static float get_uv_anim_time(rl_renderable_particles_instance* instance_p, const rl_particle_iterative& particle); // 0x004E3EA0 (.h:423)
    static void uvs_animate(vector2& uvs, const rl_particle_iterative& particle); // 0x004BD730 (.h:440)

public:
    static rl_class_dynamic_info ms_class_dynamic_info; // 0x01514F94
    static const int PARTICLE_ITERATIVE_POOL_SIZE_DEFAULT; // 0x012BED3C
    static const int PARTICLE_PARAMETRIC_POOL_SIZE_DEFAULT; // 0x012BED40
    static const int PARTICLE_TAIL_POOL_SIZE_DEFAULT; // 0x012BED44
protected:
    keen::ParticleVertexShaderConstants m_vertexData; // 0x10
    keen::ParticleFragmentShaderConstants m_fragmentData; // 0xA0
    keen::DynamicConstantBuffer* m_pVertexConstants; // 0x140
    keen::DynamicConstantBuffer* m_pFragmentConstants; // 0x144
    static const int INV_PROJ_MATRIX_CONST; // 0x012BED48
    static const int Z_DIMENSIONS_CONST; // 0x012BED4C
    static const int COLOR_CORRECTION_MATRIX_CONST; // 0x012BED50
    static const int UV_ANIM_TILING_CONST; // 0x012BED54
    static const int EFFECT_OPACITY_CONST; // 0x012BED58
    static const int EFFECT_AMBIENT_COLOR_CONST; // 0x012BED5C
    static const int EFFECT_SELF_ILLUM_CONST; // 0x012BED60
    static const int EFFECT_DIFFUSE_COLOR_CONST; // 0x012BED64
    static const int EFFECT_MATERIAL_TINT_CONST; // 0x012BED68
    static const int EFFECT_LIGHT_DIR_CONST; // 0x012BED6C
private:
    static int ms_particle_iterative_pool_size; // 0x014FA314
    static int ms_particle_parametric_pool_size; // 0x014FA318
    static int ms_particle_tail_pool_size; // 0x014FA31C
    static mempool ms_particle_parametric_pool; // 0x01C209F8
    static mempool ms_particle_iterative_pool; // 0x01C1A9A0
    static mempool ms_particle_tail_pool; // 0x01C273B0
    static const int PARTICLES_PER_INSTANCE_MAX; // 0x012BED70
    static const int BILLBOARD_INDEX_COUNT; // 0x012BED74
    static const int DROP_INDEX_COUNT; // 0x012BED78
    static const int RIBBON_INDEX_COUNT; // 0x012BED7C
    static const int HALF_RES_MAX; // 0x012BED80
    static const int QUARTER_RES_MAX; // 0x012BED84
    static const int FULL_RES_MAX; // 0x012BED88
    static const int CORONA_MAX; // 0x012BED8C
    static const int PARAMETRIC_VERTEX_SIZE; // 0x012BED90
    static const int PARAMETRIC_RIBBON_VERTEX_SIZE; // 0x012BED94
    static const int BILLBOARD_VERTEX_SIZE; // 0x012BED98
    static const int RADIAL_VERTEX_SIZE; // 0x012BED9C
    static const int DROP_VERTEX_SIZE; // 0x012BEDA0
    static const int RIBBON_VERTEX_SIZE; // 0x012BEDA4
    static const int ORIENTED_VERTEX_SIZE; // 0x012BEDA8
    static const int CORONA_VERTEX_SIZE; // 0x012BEDAC
    farray<rl_renderable_particles_instance*,4096> m_distortion_entries; // 0x148
    farray<rl_renderable_particles_instance*,4096> m_half_res_entries; // 0x4158
    farray<rl_renderable_particles_instance*,1024> m_quarter_res_entries; // 0x8168
    farray<rl_renderable_particles_instance*,1024> m_full_res_entries; // 0x9178
    farray<rl_corona_instance*,1024> m_corona_entries; // 0xA188
    particle_vertex_info m_iterative_vertex_info[10]; // 0xB198
    particle_vertex_info m_parametric_vertex_info[10]; // 0xB1E8
    unsigned int m_particle_type_shaders[10]; // 0xB238
    particle_shader_techniques m_particle_shader_techniques[10]; // 0xB260
    keen::RenderGeometry m_billboard; // 0xB300
    keen::RenderGeometry m_drop; // 0xB32C
    keen::RenderGeometry m_ribbon; // 0xB358
    keen::RenderGeometry m_uv; // 0xB384
    rl_render_texture m_quarter_res_rt; // 0xB3B0
    rl_render_texture m_quarter_depth_target; // 0xB3C4
    rl_dev_depth_stencil_target m_quarter_ds_surface; // 0xB3D8
    int m_quarter_width; // 0xB3DC
    int m_quarter_height; // 0xB3E0
    unsigned int m_downsample_depth_effect; // 0xB3E4
    rl_shader_const_ref m_downscale_sampling_offsets_const; // 0xB3E8
    rl_shader_const_ref m_downscale_depth_sampler_const; // 0xB3EC
    rl_shader_const_ref m_downscale_z_min_and_range_const; // 0xB3F0
    unsigned int m_up_res_effect; // 0xB3F4
    rl_shader_const_ref m_up_res_sampler_const; // 0xB3F8
    unsigned int m_particle_brightpass_effect; // 0xB3FC
    rl_downscale_sampling_offsets<16> m_brightpass_downscale_sampling; // 0xB400
    rl_shader_const_ref m_brightpass_downscale_sampling_offsets_const; // 0xB508
    rl_shader_const_ref m_brightpass_sampler; // 0xB50C
    rl_particle_lod m_level_of_detail; // 0xB510
};

// size 0x10
struct rl_particle_brightpass_state
{
    rl_dev_render_target m_render_target; // 0x00
    int m_width; // 0x04
    int m_height; // 0x08
    bool m_enabled; // 0x0C
};

// size 0x20
struct rl_particle_distortion_state
{
    rl_dev_render_target m_render_target; // 0x00
    int m_width; // 0x04
    int m_height; // 0x08
    unsigned int m_material; // 0x0C
    rl_batched_pass* m_batch_p; // 0x10
    base_array<rl_renderable_instance*>* m_distortion_only_entries_p; // 0x14
    unsigned int m_vis_flags; // 0x18
    bool m_enabled; // 0x1C
};

// size 0x14
struct rl_particle_framebuffer_state
{
    rl_render_texture* m_render_target_p; // 0x00
    rl_dev_depth_stencil_target m_depth_stencil; // 0x04
    rl_dev_texture m_depth_texture; // 0x08
    int m_width; // 0x0C
    int m_height; // 0x10
};

void update_corona(rl_renderer& renderer, rl_corona_instance* instance_p); // 0x00513990 (:433)
// template instance: const rl_color_float interpolate_two_values<rl_color_float>(const rl_color_float& _start, const rl_color_float& _end, float _ratio); // 0x004D9200 (:455)
