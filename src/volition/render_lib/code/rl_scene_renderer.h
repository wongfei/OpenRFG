// Original: volition/render_lib/code/rl_scene_renderer.h
// Implementation: volition/render_lib/code/rl_scene_renderer.cpp
//
// rl_scene_renderer, renders an rl_scene with a camera: visibility, the base/lighting/shadow/glow/distortion/overlay
// passes, deferred sorting (deferred_sort_entry) and post-processing compositors. rl_scene_renderer_lighting.cpp and
// rl_scene_shadows.cpp hold more of its methods.
#pragma once

#include "keen/base/memory.hpp"
#include "keen/base/win32/event_win32.hpp"
#include "keen/base/win32/mutex_win32.hpp"
#include "keen/base/win32/thread_win32.hpp"
#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_depth_of_field.h"
#include "volition/render_lib/code/rl_device.h"
#include "volition/render_lib/code/rl_distortion.h"
#include "volition/render_lib/code/rl_motion_blur.h"
#include "volition/render_lib/code/rl_primitive_defines.h"
#include "volition/render_lib/code/rl_render_texture.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/render_lib/code/rl_sampling_offsets.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/render_lib/code/rl_shadow_system.h"
#include "volition/render_lib/code/rl_terrain_renderer.h"
#include "volition/vlib/code/util/varray.h"

struct GaussBlurContext;
struct SsaoContext;
class rl_batched_pass;
class rl_camera;
class rl_class_dynamic_info;
class rl_color_correct_state;
class rl_compositor;
class rl_hdr_compositor;
class rl_hdr_state;
class rl_light;
class rl_metrics;
class rl_outline_compositor;
struct rl_outline_state;
class rl_rect;
class rl_render_pass;
class rl_renderer;
class rl_scene;
class rl_visibility_manager;
struct rl_xray_render_state;

namespace keen
{
    struct FxaaContext;
    class GraphicsCommandWriter;
    struct RecordedCommandBuffer;
    struct RenderTarget;
    struct TextureData;
}

// size 0x4BBE8
class rl_scene_renderer : public rl_base_object
{
    // Access inferred: rl_render_lib and rl_renderer read the private members directly.
    friend class rl_render_lib;
    friend class rl_renderer;
    friend class rl_scene;
    // rfg's ui_character.cpp sets flag 4 (the main scene into its temp target) and shows that target (friends are
    // not recorded in rfg.pdb).
    friend void ui_character_shutdown_internal();
    friend void ui_character_init_resources();
    friend void ui_character_set_frame_visible(bool visible);
public:
    // size 0x20
    struct deferred_sort_entry
    {
        static bool btf_predicate(const deferred_sort_entry& lhs, const deferred_sort_entry& rhs); // 0x004BCED0 (.h:478)
        static bool ftb_predicate(const deferred_sort_entry&, const deferred_sort_entry&);
        static bool duplicate_priority_sort_predicate(const deferred_sort_entry&, const deferred_sort_entry&);

        rl_renderable_instance* renderable_instance_p; // 0x00
        rl_renderable_instance::sort_method sort_method; // 0x04
        unsigned int generic_id; // 0x08
        int sort_priority; // 0x0C
        float dist_to_camera_sq; // 0x10
        rl_render_pass* render_pass_p; // 0x14
        int secondary_sort_priority; // 0x18
        bool opacity_fade; // 0x1C
    };

    // size 0x10
    struct saved_texture
    {
        saved_texture(); // 0x004D63B0

        rl_render_texture* m_render_texture_p; // 0x00
        rl_dev_texture m_saved_texture; // 0x04
        unsigned int m_width; // 0x08
        unsigned int m_height; // 0x0C
    };

    // size 0x28
    struct Part2Params
    {
        rl_renderer* p_deferredRenderer; // 0x00
        rl_camera* p_camera; // 0x04
        bool msaa_enabled; // 0x08
        rl_scene* p_scene; // 0x0C
        rl_dev_render_target* p_original_rt; // 0x10
        rl_shadow_render_parameters* p_shadow_render_params; // 0x14
        const rl_hdr_state* p_hdr_state; // 0x18
        const rl_color_correct_state* p_color_correct_state; // 0x1C
        rl_dev_depth_stencil_target* p_original_ds; // 0x20
        rl_rect* p_original_vp; // 0x24
    };

protected:
    rl_scene_renderer(); // 0x005173E0 (:145)
    virtual ~rl_scene_renderer(); // vtbl+0x00 0x0052DE50 (:172)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_scene_renderer* create(); // 0x0052DE10 (:79)
    static bool can_create(); // 0x004C7500 (:79)
    virtual void release(); // vtbl+0x04 0x004C7510 (:79)
    static void set_visible_renderable_instances_max(int visible_renderable_instances_max); // 0x007C0210 (.h:374)
    static int get_visible_renderable_instances_max();
    static void set_visible_lights_max(int);
    static int get_visible_lights_max();
    virtual void init(keen::GraphicsCommandWriter* pCommandWriter, int w, int h, const rl_level_of_detail& lod, keen::MemoryBlock deferredCommandBufferMemory); // vtbl+0x08 0x00562EB0 (:232)
    void create_render_textures(keen::GraphicsCommandWriter* pCommandWriter); // 0x00544CA0 (:1496)
    virtual void render(rl_renderer& mainRenderer, rl_renderer& deferredRenderer, rl_scene& scene, rl_camera& camera, const rl_hdr_state& base_hdr_state, const rl_color_correct_state& color_correct_state); // vtbl+0x0C 0x00561D20 (:350)
    void render_scene_part2(rl_renderer& deferredRenderer, rl_camera& camera, const bool msaa_enabled, rl_scene& scene, rl_dev_render_target original_rt, rl_shadow_render_parameters& shadow_render_params, const rl_hdr_state& hdr_state, const rl_color_correct_state& color_correct_state, rl_dev_depth_stencil_target original_ds, rl_rect original_vp); // 0x005601B0 (:782)
    void render_depth_pre_pass(rl_renderer& renderer, rl_camera& camera, rl_scene& scene, const bool depth_to_color, rl_color_float* clear_color, const bool msaa_enabled); // 0x005455A0 (:1711)
    rl_scene* get_scene() const; // 0x004BCE70 (.h:394)
    void set_scene(rl_scene* scene_p); // 0x004BCE80 (.h:399)
    const rl_metrics* get_metrics() const;
    const pool_list<rl_renderable_instance*>& get_visible_renderable_instances() const;
    const pool_list<rl_light*>& get_visible_lights() const;
    rl_iterrain_renderer* get_terrain_renderer() const; // 0x004BCE90 (.h:404)
    void set_terrain_renderer(rl_iterrain_renderer*);
    bool get_visibility_locked() const; // 0x004BCE30 (.h:102)
    void set_visibility_locked(bool);
    bool get_color_correction_enabled() const;
    void set_color_correction_enabled(bool);
    void set_editor_mode(bool);
    void lock_framebuffer(bool lock); // 0x007CD390 (.h:414)
    bool get_framebuffer_is_locked(); // 0x008AFE20 (.h:427)
    void set_framebuffer_resolve_enabled(bool enable); // 0x00893140 (.h:459)
    bool get_framebuffer_resolve_enabled(); // 0x004BCEC0 (.h:464)
    int get_framebuffer_resolve_texture(); // 0x00893160 (.h:469)
    void add_deferred_sort(rl_renderable_instance* renderable_instance_p, rl_render_pass* render_pass_p, float dist_to_cam_sq, rl_renderable_instance::sort_method sort_method, unsigned int generic_id, bool opacity_fade, int sort_priority, int secondary_sort_priority); // 0x004C7660 (:1205)
    rl_batched_pass* get_scene_batch(); // 0x004BCE40 (.h:129)
    const base_array<rl_renderable_instance*>& get_distortion_only_entries();
    void release_resources(); // 0x00517AD0 (:1399)
    void renew_resources(keen::GraphicsCommandWriter* pCommandWriter); // 0x00550CD0 (:1480)
    void resize(int w, int h, bool highQuality); // 0x004F5E10 (:1612)
    void set_level_of_detail(const rl_level_of_detail& lod); // 0x004C7750 (:1969)
    void save_render_texture(rl_render_texture&, rl_dev_texture&, unsigned int, unsigned int);
    void save_textures();
    void restore_textures();
    void begin_exp_biasing();
    void end_exp_biasing();
    void render_lighting_passes(rl_renderer& renderer, rl_camera& camera, rl_light& primary_light, rl_batched_pass& batch_to_use, rl_renderable_instance** transparent_casters, unsigned int num_transparent_casters, const bool msaa_enabled); // 0x0054DC00 (rl_scene_renderer_lighting.cpp:19)
    void render_base_pass(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_shadow_render_parameters& shadow_render_params, rl_iterrain_renderer* terrain_renderer_p, rl_batched_pass& batched_pass, const unsigned int vis_flags, const bool skip_decals, const bool alpha_blend, const rl_prim_color_write_mode color_write_mode, const bool msaa_enabled, bool isMainPass, rl_terrain_mode terrain_mode); // 0x00544770 (:1251)
    void render_lighting_pass(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_shadow_render_parameters& shadow_render_params, rl_iterrain_renderer* terrain_renderer_p, rl_batched_pass& batched_pass, const unsigned int vis_flags, const bool skip_decals, const bool alpha_blend, const rl_prim_color_write_mode color_write_mode, const bool msaa_enabled); // 0x00545C00 (rl_scene_renderer_lighting.cpp:56)
    int get_mainscene_width() const; // 0x004BCE50 (.h:155)
    int get_mainscene_height() const; // 0x004BCE60 (.h:156)
private:
    bool get_framebuffer_lock_completed(); // 0x004BCEA0 (.h:432)
    bool get_framebuffer_lock_initiated(); // 0x004BCEB0 (.h:437)
    void complete_lock_frame_buffer(rl_renderer& renderer); // 0x0050AD90 (.h:442)
    void render_landmarks();
    void initialize_deferred(); // 0x004C7530 (:344)
    void reset_deferred(); // 0x00517A90 (:1243)
    void clb_render(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_visibility_manager& visibility_manager, rl_batched_pass& batch_to_use, rl_renderable_instance** transparent_casters, int num_transparent_casters); // 0x00545CB0 (rl_scene_shadows.cpp:297)
    void render_clb_pass(rl_renderer& renderer, rl_camera& camera, rl_shadow_render_parameters& render_params, keen::TextureData* pProjectionTexture); // 0x0052E9F0 (rl_scene_shadows.cpp:224)
    void render_clb_pass(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_shadow_creation_parameters& creation_params, rl_shadow_render_parameters& depthMapRenderParams, keen::TextureData* pProjectionTexture, rl_shadow_render_parameters* pProjectionTextureRenderParams, rl_batched_pass& batch_to_use, unsigned int pass_vis_flags, rl_shadow_map_range shadow_map_range, rl_renderable_instance** transparent_casters, int num_transparent_casters, rl_shadow_map_pass shadow_map_pass); // 0x0052E8D0 (rl_scene_shadows.cpp:195)
    void render_clb_pass(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_shadow_creation_parameters& creation_params, rl_shadow_render_parameters& render_params, rl_batched_pass& batch_to_use, unsigned int pass_vis_flags, rl_shadow_map_range shadow_map_range, rl_renderable_instance** transparent_casters, int num_transparent_casters, rl_shadow_map_pass shadow_map_pass); // 0x0052E7B0 (rl_scene_shadows.cpp:166)
    void render_shadow_pass(rl_renderer& renderer, rl_camera& camera, rl_light& light, const rl_dev_render_target& depth_target, int width, int height, rl_shadow_creation_parameters& shadow_creation_params, rl_batched_pass& batch_to_use, const unsigned int vis_flags, rl_renderable_instance** transparent_casters, unsigned int num_transparent_casters, const bool base_pass); // 0x00517D60 (rl_scene_shadows.cpp:78)
    void render_deferred(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_shadow_render_parameters& shadow_render_params, const unsigned int vis_flags, const float fade_range, const bool skip_decals, const bool alpha_blend, const bool msaa_enabled); // 0x0055D140 (:1344)
    void render_deferred_list(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_shadow_render_parameters& shadow_render_params, const unsigned int vis_flags, const float fade_range, const bool skip_decals, const bool alpha_blend, const deferred_sort_entry* list, unsigned int num_entries, const bool msaa_enabled); // 0x00544870 (:1286)
    void begin_base_pass(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_shadow_render_parameters& shadow_render_params, const bool alpha_blend, const rl_prim_color_write_mode color_write_mode, const bool msaa_enabled, bool isMainPass); // 0x0052E2D0 (:1358)
    void end_base_pass(rl_camera& camera, rl_renderer& renderer); // 0x0052E3A0 (:1389)
    void begin_lighting_pass(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_shadow_render_parameters& shadow_render_params, const bool alpha_blend, const rl_prim_color_write_mode color_write_mode, const bool msaa_enabled); // 0x0052E6E0 (rl_scene_renderer_lighting.cpp:76)
    void end_lighting_pass(rl_camera& camera, rl_renderer& renderer); // 0x0052E790 (rl_scene_renderer_lighting.cpp:103)
    void set_shadow_states(rl_shadow_render_parameters& shadow_render_params, rl_renderer& renderer); // 0x004F6010 (:1917)
    void render_xray_effect(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_visibility_manager& visibility_manager, rl_shadow_render_parameters& shadow_render_params, rl_iterrain_renderer& terrain_renderer, const rl_xray_render_state& xray_state, const rl_dev_render_target& target_rt, const rl_dev_depth_stencil_target& target_ds, rl_batched_pass& batched_pass, pool_list<rl_renderable_instance*>& renderable_list); // 0x005456D0 (:1735)
    void render_xray_pass(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_shadow_render_parameters& shadow_render_params, rl_iterrain_renderer* terrain_renderer_p, rl_batched_pass* batched_pass_p, pool_list<rl_renderable_instance*>* renderable_list_p, unsigned int vis_pass_flags); // 0x0052E3C0 (:1838)
    static int startSceneRenderPart2(const keen::Thread& thread); // 0x00562EA0 (:227)
    // Not in the binary: parts the original inlines in render and render_scene_part2 (rl_scene_renderer.cpp).
    void blit_locked_framebuffer(rl_renderer& mainRenderer);
    void render_scene_part2_passes(rl_renderer& deferredRenderer, rl_camera& camera, const bool msaa_enabled, rl_scene& scene, rl_dev_render_target original_rt, rl_shadow_render_parameters& shadow_render_params, const rl_hdr_state& hdr_state, const rl_color_correct_state& color_correct_state, rl_dev_depth_stencil_target original_ds);
    void render_particles(rl_renderer& deferredRenderer, rl_camera& camera, rl_scene& scene, rl_outline_state& outline_state, const rl_dev_texture& scene_depth_texture, const rl_hdr_state& hdr_state, const rl_color_correct_state& color_correct_state);
    void render_post_effects(rl_renderer& deferredRenderer, rl_camera& camera, rl_scene& scene, rl_dev_render_target original_rt, rl_shadow_render_parameters& shadow_render_params, const rl_hdr_state& hdr_state, const rl_color_correct_state& color_correct_state, rl_dev_depth_stencil_target original_ds, bool sun_shafts_enabled);
    int runSceneRenderPart2(const keen::Thread& thread); // 0x00562DF0 (:757)

public:
    static rl_class_dynamic_info ms_class_dynamic_info; // 0x01516230
    static const int VISIBLE_RENDERABLE_INSTANCES_MAX_DEFAULT; // 0x012BECF4
    static const int VISIBLE_DISTORTION_INSTANCES_MAX_DEFAULT; // 0x012BECF8
    static const int VISIBLE_LIGHTS_MAX_DEFAULT; // 0x012BECFC
    static const int TRANSPARENT_SHADOW_MAX; // 0x012BED00
    static const int VISIBLE_LOWLOD_DEFAULT; // 0x012BED04
private:
    static const int DEFERRED_MAX; // 0x012BED08
    static const int DEFERRED_FIRST_MAX; // 0x012BED0C
    static const int DEFERRED_LAST_MAX; // 0x012BED10
    static const unsigned int kFramebufferLockInitiated; // 0x012BED14
    static const unsigned int kFramebufferLockCompleted; // 0x012BED18
    static const unsigned int kFramebufferResolveEnabled; // 0x012BED1C
    rl_compositor* m_compositor_p; // 0x08
    rl_hdr_compositor* m_hdr_compositor_p; // 0x0C
    rl_outline_compositor* m_outline_compositor_p; // 0x10
    unsigned int m_flags; // 0x14
    rl_scene* m_scene_p; // 0x18
    rl_iterrain_renderer* m_terrain_renderer_p; // 0x1C
    pool_list<rl_renderable_instance*> m_visible_renderable_instances; // 0x20
    pool_list<rl_light*> m_visible_lights; // 0x38
    rl_batched_pass* m_base_batched_pass; // 0x50
    rl_deferred_batch_info m_deferred_ids_to_add; // 0x54
    rl_metrics* m_metrics_p; // 0x60
    unsigned int m_num_transparent_shadow_objects; // 0x64
    rl_renderable_instance* m_transparent_shadow_objects[100]; // 0x68
    int m_main_scene_target_width; // 0x1F8
    int m_main_scene_target_height; // 0x1FC
    int m_half_target_width; // 0x200
    int m_half_target_height; // 0x204
    int m_particle_brightpass_width; // 0x208
    int m_particle_brightpass_height; // 0x20C
    int m_distortion_width; // 0x210
    int m_distortion_height; // 0x214
    rl_render_texture m_main_scene_render_target; // 0x218
    rl_render_texture m_half_render_target; // 0x22C
    rl_render_texture m_half_depth_target; // 0x240
    rl_render_texture m_full_res_particle_target; // 0x254
    rl_render_texture m_particle_brightpass_target; // 0x268
    rl_render_texture m_particle_fog_color_target; // 0x27C
    rl_render_texture m_distortion_target; // 0x290
    rl_dev_depth_stencil_target m_main_scene_depth_stencil_target; // 0x2A4
    const keen::TextureData* m_pMainSceneDepthTexture; // 0x2A8
    rl_dev_depth_stencil_target m_half_depth_stencil_target; // 0x2AC
    const keen::RenderTarget* m_pMainSceneDepthTarget; // 0x2B0
    rl_dev_depth_stencil_target m_main_scene_depth_stencil_target_msaa; // 0x2B4
    int m_main_scene_downsample_width; // 0x2B8
    int m_main_scene_downsample_height; // 0x2BC
    rl_render_texture m_main_scene_downsample_target; // 0x2C0
    rl_render_texture m_main_scene_blur_target; // 0x2D4
    rl_render_texture m_main_scene_temp_target[2]; // 0x2E8
    int m_outline_width; // 0x310
    int m_outline_height; // 0x314
    rl_render_texture m_outline_target; // 0x318
    int m_ssao_width; // 0x32C
    int m_ssao_height; // 0x330
    keen::TextureData* m_pSsaoBlurTexture[2]; // 0x334
    const keen::RenderTarget* m_pSsaoBlurTarget[2]; // 0x33C
    int m_sun_shafts_skybox_mask_width; // 0x344
    int m_sun_shafts_skybox_mask_height; // 0x348
    rl_render_texture m_sun_shafts_skybox_mask_target; // 0x34C
    int m_clb_width; // 0x360
    int m_clb_height; // 0x364
    keen::TextureData* m_pClbTexture; // 0x368
    const keen::RenderTarget* m_pClbTarget; // 0x36C
    GaussBlurContext* m_pGaussBlurContext; // 0x370
    int m_shadow_map_width; // 0x374
    int m_shadow_map_height; // 0x378
    rl_render_texture m_shadow_map_target; // 0x37C
    rl_color_float m_particle_fog_color; // 0x390
    SsaoContext* m_pSsaoContext; // 0x3A0
    rl_depth_of_field m_depth_of_field; // 0x3A4
    rl_distortion m_distortion; // 0x460
    rl_motion_blur m_motion_blur; // 0x474
    keen::FxaaContext* m_pFxaaContext; // 0x4E0
    rl_level_of_detail m_level_of_detail; // 0x4E4
    rl_level_of_detail m_new_level_of_detail; // 0x578
    static const int kDownsampleTechnique; // 0x012BED20
    rl_gaussian5x5_blur_kernel m_blur_kernel; // 0x60C
    volatile unsigned int m_num_deferred_sort_entries; // 0x724
    deferred_sort_entry m_deferred_sort_entries[9216]; // 0x728
    volatile unsigned int m_num_deferred_sort_entries_first; // 0x48728
    deferred_sort_entry m_deferred_sort_entries_first[200]; // 0x4872C
    volatile unsigned int m_num_deferred_sort_entries_last; // 0x4A02C
    deferred_sort_entry m_deferred_sort_entries_last[200]; // 0x4A030
    farray<rl_renderable_instance*,50> m_distortion_only_entries; // 0x4B930
    farray<saved_texture,16> m_saved_textures; // 0x4BA08
    rl_dev_texture m_locked_framebuffer_texture; // 0x4BB18
    unsigned int m_blit_shader; // 0x4BB1C
    keen::RecordedCommandBuffer* m_pRecordedCommandBuffer; // 0x4BB20
    keen::Event m_startPart2Event; // 0x4BB24
    keen::Event m_endPart2Event; // 0x4BB28
    keen::Thread m_part2Thread; // 0x4BB2C
    keen::Mutex m_gpuVisMutex; // 0x4BB80
    Part2Params m_part2_params; // 0x4BBB8
    static int ms_visible_renderable_instances_max; // 0x014FA45C
    static int ms_visible_distortion_instances_max; // 0x014FA460
    static int ms_visible_lights_max; // 0x014FA464
    static int ms_visible_decals_max;
    static int ms_visible_lowlod_max; // 0x014FA468
    keen::MemoryBlock m_commandBufferMemory; // 0x4BBE0
};
