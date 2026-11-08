// Original: volition/render_lib/code/rl_scene_renderer.cpp
//
// rl_scene_renderer: renders an rl_scene from a camera. render() (part 1, on the calling thread) runs the visibility
// pass, sorts the visible renderable instances into the frame's rl_batched_pass (batchable opaque ones) and the
// deferred sort lists (sorted and transparent ones), then signals part 2 and renders the depth pre-pass, the GPU
// visibility queries and the CLB pass into the main scene depth buffer. Part 2 (render_scene_part2, on the
// "sceneRenderPart2" thread, recording into the deferred renderer's command buffer) renders the base pass into the HDR
// main scene target, the sky, the secondary lights and the deferred (sorted) instances, the particles, and composites
// the HDR target into the destination (rl_hdr_compositor), with the post effects. render() then executes the recorded
// command buffer on the main renderer's context. The lighting passes are in rl_scene_renderer_lighting.cpp, the shadow
// and CLB passes in rl_scene_shadows.cpp.

#include "volition/render_lib/code/rl_scene_renderer.h"

#include "volition/render_lib/code/gaussblur.hpp"
#include "volition/render_lib/code/rl_batched_pass.h"
#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_color_correct_state.h"
#include "volition/render_lib/code/rl_compositor.h"
#include "library/modules/runtime/graphics/sources/d3d11/graphicscommandbuffer_d3d11.hpp"
#include "volition/render_lib/code/rl_deformable_mesh_instance.h"
#include "volition/render_lib/code/rl_dyn_texture.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_gpu_visibility.h"
#include "volition/render_lib/code/rl_hdr_compositor.h"
#include "volition/render_lib/code/rl_hdr_state.h"
#include "volition/render_lib/code/rl_light.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_metrics.h"
#include "volition/render_lib/code/rl_outline_compositor.h"
#include "volition/render_lib/code/rl_particle_renderer.h"
#include "volition/render_lib/code/rl_partition_system.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_renderer_metrics.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/render_lib/code/rl_ssao.h"
#include "volition/render_lib/code/rl_state_manager.h"
#include "volition/render_lib/code/rl_terrain_renderer.h"
#include "volition/render_lib/code/rl_visibility_manager.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "keen/base/memoryallocator.hpp"
#include "keen/graphics/graphicscommandwriter.hpp"
#include "keen/graphics/graphicssystem.hpp"
#include "keen/graphics/postprocessing_fxaa.hpp"
#include "keen/graphics/rendertarget.hpp"
#include "keen/graphics/texturedata.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/rendertarget_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp"

#include <algorithm>
#include <intrin.h>
#include <math.h>
#include <string.h>

// <rl_scene_renderer::release> 0x004C7510 (:79)
RL_DYNAMIC_CLASS_IMPLEMENTATION(rl_scene_renderer)

// Capacities of the visible lists (the game sets 12000 renderable instances in game_render_system_init).
int rl_scene_renderer::ms_visible_renderable_instances_max = 5000; // 0x014FA45C
int rl_scene_renderer::ms_visible_distortion_instances_max = 50; // 0x014FA460
int rl_scene_renderer::ms_visible_lights_max = 100; // 0x014FA464
int rl_scene_renderer::ms_visible_lowlod_max = 1000; // 0x014FA468

// The scene processing switches of render() (all on in retail), and the debug locks (off).
bool RL_enable_scene_processing = true; // 0x014FA3D9
bool RL_enable_gpu_vis_prerender = true; // 0x014FA3DA
bool RL_enable_depth_prepass = true; // 0x014FA3DB
bool RL_enable_gpu_vis_render = true; // 0x014FA452
bool RL_enable_clb = true; // 0x014FA453
bool RL_enable_clear = true; // 0x014FA46C
bool RL_enable_base_pass = true; // 0x014FA46D
bool RL_enable_skybox = true; // 0x014FA46E
bool RL_enable_secondary_lights = true; // 0x014FA46F
bool RL_enable_render_deferred = true; // 0x014FA470
bool RL_enable_particle_rendering = true; // 0x014FA471
bool RL_enable_post_effects = true; // 0x014FA472
bool RL_lock_scene_processing = false; // 0x01B7BD18
bool RL_only_scene_processing = false; // 0x01B7BD19
bool RL_enable_scene_profiling = false; // 0x01B7BD1A
bool RL_scene_processing_locked = false; // 0x01B7BD1B
bool RL_scene_processing_onlied = false; // 0x01B7BD1C
bool RL_enable_msaa_prepass = false; // 0x01B7BD1D
// Which visibility passes the base pass draws (only the base pass in retail).
bool RL_vis_base = true; // 0x014FA473
bool RL_vis_clb_near = false; // 0x01B7BD1F
bool RL_vis_clb_mid = false; // 0x01B7BD20
bool RL_vis_clb_far = false; // 0x01B7BD21
bool RL_vis_clb_terrain = false; // 0x01B7BD22
bool RL_vis_clb_drop = false; // 0x01B7BD23
// The deferred opacity fade distances when RL_alpha_dist_override replaces the scene's.
bool RL_alpha_dist_override = false; // 0x01B7BD0B
float RL_alpha_dist_start = 65.0f; // 0x014FA474
float RL_alpha_dist_end = 75.0f; // 0x014FA478
bool g_fxaaEnabled = true; // 0x014FA47C
bool g_ssaoVisEnabled = false; // 0x01B7BD1E
bool Rl_landmarks_enabled = true; // 0x014FA47D
bool Rl_blur_landmarks = false; // 0x01B7BD42
bool Rl_sun_shafts_enabled = true; // 0x014FA288
// The share of the depth range kept for the landmarks (behind everything else).
float RL_reserved_landmark_depth_range = 0.0078125f; // 0x0150A618
// The CLB shadow distances.
float g_shadowMapMaxDist = 100.0f; // 0x014FA510
float g_shadowMapFadePct = 0.8f; // 0x014FA514
float g_shadowPercent = 1.0f; // 0x014FA540

// The scene renderer and render pass of the batch being built (read by instances while they add themselves).
rl_scene_renderer* Current_scene_renderer = nullptr; // 0x01B7BD10
rl_render_pass* Current_render_pass = nullptr; // 0x01B7BD14

// The deferred ids an instance adds while it is batched (1024 entries).
static const unsigned int DEFERRED_ITEMS_MAX = 0x400;
static rl_deferred_batch_item deferred_items_array[DEFERRED_ITEMS_MAX]; // 0x01CCF6F0

// The shadow map size per shadow LOD (off, low, medium, high).
static const int shadow_map_resolutions[4] = { 0, 512, 1024, 2048 }; // 0x012C1BB4

// Deferred sort list capacities (0x2400, 200, 200) and the transparent shadow casters (100).
static const int DEFERRED_SORT_MAX = 0x2400;
static const int DEFERRED_SORT_FIRST_MAX = 200;
static const int DEFERRED_SORT_LAST_MAX = 200;
static const unsigned int TRANSPARENT_SHADOW_CASTERS_MAX = 100;
static const unsigned int UNBATCHED_RENDERABLES_MAX = 0x1000;

const int rl_scene_renderer::VISIBLE_RENDERABLE_INSTANCES_MAX_DEFAULT = 5000; // 0x012BECF4
const int rl_scene_renderer::VISIBLE_DISTORTION_INSTANCES_MAX_DEFAULT = 50; // 0x012BECF8
const int rl_scene_renderer::VISIBLE_LIGHTS_MAX_DEFAULT = 100; // 0x012BECFC
const int rl_scene_renderer::TRANSPARENT_SHADOW_MAX = 100; // 0x012BED00
const int rl_scene_renderer::VISIBLE_LOWLOD_DEFAULT = 1000; // 0x012BED04
const int rl_scene_renderer::DEFERRED_MAX = DEFERRED_SORT_MAX; // 0x012BED08
const int rl_scene_renderer::DEFERRED_FIRST_MAX = DEFERRED_SORT_FIRST_MAX; // 0x012BED0C
const int rl_scene_renderer::DEFERRED_LAST_MAX = DEFERRED_SORT_LAST_MAX; // 0x012BED10
// m_flags bits.
const unsigned int rl_scene_renderer::kFramebufferLockInitiated = 1; // 0x012BED14
const unsigned int rl_scene_renderer::kFramebufferLockCompleted = 2; // 0x012BED18
const unsigned int rl_scene_renderer::kFramebufferResolveEnabled = 4; // 0x012BED1C
const int rl_scene_renderer::kDownsampleTechnique = 0; // 0x012BED20

// A packed macro state the original builds as a constant (the bit layout is rl_primitive_state's).
static rl_primitive_state macro_state_bits(unsigned int bits)
{
    rl_primitive_state state;
    state.set_bits(bits);
    return state;
}

// The render offset the camera position is taken relative to, and the eye constants (rl_camera::render_refresh
// without the projection, inlined).
static void set_eye_from_camera(rl_renderer& renderer, const rl_camera& camera)
{
    const vector& offset = renderer.get_render_offset();
    const vector& position = camera.get_position();
    renderer.set_eye_position(vector(position.x - offset.x, position.y - offset.y, position.z - offset.z));
}

// 0x004D63B0
rl_scene_renderer::saved_texture::saved_texture()
{
    m_saved_texture.pData = nullptr;
}

// 0x005173E0 (:145)
rl_scene_renderer::rl_scene_renderer()
{
    m_scene_p = nullptr;
    m_terrain_renderer_p = nullptr;
    m_metrics_p = rl_metrics::create();
    m_flags = 0;
    m_base_batched_pass = nullptr;
    m_compositor_p = nullptr;
    m_hdr_compositor_p = nullptr;
    m_outline_compositor_p = nullptr;
}

// 0x0052DE50 (:172)
// Releases the compositors and the terrain renderer, stops the part 2 thread, and destroys the targets.
rl_scene_renderer::~rl_scene_renderer()
{
    if (m_metrics_p != nullptr)
    {
        m_metrics_p->release();
    }
    if (m_locked_framebuffer_texture.pData != nullptr)
    {
        keen::graphics::freeTextureData(m_locked_framebuffer_texture.pData, Init_params.m_pKeenGraphicsSystem, nullptr);
        delete m_locked_framebuffer_texture.pData;
        m_locked_framebuffer_texture.pData = nullptr;
    }
    if (m_main_scene_depth_stencil_target_msaa.pData != nullptr)
    {
        keen::graphics::destroyRenderTarget(Init_params.m_pKeenGraphicsSystem, m_main_scene_depth_stencil_target_msaa.pData);
    }
    m_main_scene_depth_stencil_target_msaa.pData = nullptr;
    if (m_main_scene_depth_stencil_target.pData != nullptr)
    {
        keen::graphics::destroyRenderTarget(Init_params.m_pKeenGraphicsSystem, m_main_scene_depth_stencil_target.pData);
    }
    m_main_scene_depth_stencil_target.pData = nullptr;
    if (m_outline_compositor_p != nullptr)
    {
        m_outline_compositor_p->shutdown();
        m_outline_compositor_p->release();
    }
    if (m_hdr_compositor_p != nullptr)
    {
        m_hdr_compositor_p->shutdown();
        m_hdr_compositor_p->release();
    }
    if (m_compositor_p != nullptr)
    {
        m_compositor_p->release();
    }
    if (m_terrain_renderer_p != nullptr)
    {
        m_terrain_renderer_p->release();
    }
    m_part2Thread.requestQuit();
    m_startPart2Event.signal();
    m_part2Thread.join(0xFFFFFFFF);
    m_part2Thread.destroy();
    m_gpuVisMutex.destroy();
    m_startPart2Event.destroy();
    m_endPart2Event.destroy();
    // The render textures' destructors (rl_render_texture::destroy, then the dynamic texture released) run for the
    // members in reverse order; the visible lists free their dynamic storage.
}

// 0x00562EA0 (:227)
int rl_scene_renderer::startSceneRenderPart2(const keen::Thread& thread)
{
    return ((rl_scene_renderer*)thread.getArgument())->runSceneRenderPart2(thread);
}

// 0x00562EB0 (:232)
// The visible lists, the terrain renderer, the level of detail, the targets (resize then create_render_textures), the
// compositors and post effect contexts, the blit shader and the part 2 thread.
void rl_scene_renderer::init(keen::GraphicsCommandWriter* pCommandWriter, int w, int h, const rl_level_of_detail& lod, keen::MemoryBlock deferredCommandBufferMemory)
{
    m_pMainSceneDepthTarget = nullptr;
    m_pSsaoBlurTarget[0] = nullptr;
    m_pSsaoBlurTarget[1] = nullptr;
    m_pClbTarget = nullptr;
    m_pMainSceneDepthTexture = nullptr;
    m_pSsaoBlurTexture[0] = nullptr;
    m_pSsaoBlurTexture[1] = nullptr;
    m_pClbTexture = nullptr;
    m_pGaussBlurContext = nullptr;
    m_visible_renderable_instances.init_dynamic(ms_visible_renderable_instances_max);
    m_visible_lights.init_dynamic(ms_visible_lights_max);
    rl_terrain_renderer* terrain_renderer_p = rl_terrain_renderer::create();
    m_terrain_renderer_p = terrain_renderer_p;
    terrain_renderer_p->set_scene_renderer(this);
    m_new_level_of_detail = lod;
    m_level_of_detail = lod;
    resize(w, h, false);
    m_commandBufferMemory = deferredCommandBufferMemory;
    create_render_textures(pCommandWriter);
    m_pGaussBlurContext = createGaussBlurContext(Init_params.m_pDefaultAllocator, Init_params.m_pKeenGraphicsSystem);
    m_compositor_p = rl_compositor::create();
    m_compositor_p->intialize(m_main_scene_target_width, m_main_scene_target_height);
    m_hdr_compositor_p = rl_hdr_compositor::create();
    m_hdr_compositor_p->initialize(m_main_scene_target_width, m_main_scene_target_height, lod, pCommandWriter);
    m_outline_compositor_p = rl_outline_compositor::create();
    m_outline_compositor_p->initialize(m_main_scene_target_width, m_main_scene_target_height);
    m_pSsaoContext = createSsaoContext(Init_params.m_pDefaultAllocator, Init_params.m_pKeenGraphicsSystem);
    m_pFxaaContext = keen::createFxaaContext(Init_params.m_pDefaultAllocator, Init_params.m_pKeenGraphicsSystem);
    m_depth_of_field.initialize(m_main_scene_target_width, m_main_scene_target_height);
    m_distortion.initialize();
    m_motion_blur.initialize();
    m_blit_shader = rl_shader_library::get_instance()->lookup(crc_stri("rl_blit", 0));
    m_startPart2Event.create("startPart2", false);
    m_endPart2Event.create("endPart2", false);
    // Magic numbers: thread priority 100, 64 KB stack.
    m_part2Thread.create("sceneRenderPart2", startSceneRenderPart2, 100, 0x10000, 0);
    m_gpuVisMutex.create("gpuVisMutex");
    m_part2Thread.start(this);
}

// 0x00517770 (:302)
// LTCG: renderer in ESI, scene in EDI. The fog, ambient, render offset and time constants of the scene (time: the
// scene time, its fraction, the time times pi, the time of day).
static void initialize_scene_constants(rl_renderer& renderer, const rl_scene& scene)
{
    const rl_fog_state& fog = scene.get_fog_state();
    renderer.set_fog_parameters(fog.get_height_density(), fog.get_atmosphere_scale(), fog.get_density(), fog.get_density_offset());
    renderer.set_fog_color(fog.get_color());
    renderer.set_ambient_color(scene.get_ambient_color());
    renderer.set_back_ambient_color(scene.get_back_ambient_color());
    vector render_offset;
    ((rl_scene&)scene).get_render_offset(render_offset);
    renderer.set_render_offset(render_offset);
    const float time_val = scene.get_time_value();
    // Magic number: pi (0x400921FB60000000 as a double).
    renderer.set_time_value(vector4(time_val, (float)fmod(time_val, 1.0), (float)(time_val * 3.1415927410125732), scene.get_time_of_day_value()));
}

// 0x004C7530 (:344)
void rl_scene_renderer::initialize_deferred()
{
    m_deferred_ids_to_add.init(deferred_items_array, DEFERRED_ITEMS_MAX);
}

// 0x00561D20 (:350)
// LTCG: the decompiler shows mainRenderer and scene in registers for initialize_scene_constants. See the file header.
// Notes (decompiled behaviour, kept as is): with the framebuffer resolve flag the scene's fog state is overwritten
// (color green); the camera is copied into a heap rl_camera for part 2; the render offset is written to the main
// renderer's state twice (directly and through the state manager pointer).
void rl_scene_renderer::render(rl_renderer& mainRenderer, rl_renderer& deferredRenderer, rl_scene& scene, rl_camera& camera, const rl_hdr_state& base_hdr_state, const rl_color_correct_state& color_correct_state)
{
    const rl_rect main_viewport(0, 0, m_main_scene_target_width, m_main_scene_target_height);
    mainRenderer.set_viewport(main_viewport);
    rl_hdr_state hdr_state(base_hdr_state);
    if ((m_flags >> 2 & 1) != 0)
    {
        rl_fog_state& fog = scene.m_fog_state;
        fog.set_color(rl_color_float(0.0f, 1.0f, 0.0f, 0.0f));
        hdr_state.m_enable = false;
    }
    keen::GraphicsCommandBuffer* pCommandBuffer = keen::graphics::beginCommandBuffer(Init_params.m_pKeenGraphicsSystem, m_pRecordedCommandBuffer);
    keen::GraphicsCommandWriter commandWriter;
    commandWriter.beginWriting(pCommandBuffer, Init_params.m_pKeenGraphicsSystem);
    rl_camera* part2_camera = rl_camera::create();
    *part2_camera = camera;
    deferredRenderer.begin(&commandWriter);
    rl_rect viewport;
    mainRenderer.get_viewport(viewport);
    deferredRenderer.set_viewport(viewport);
    float z_near;
    float z_far;
    mainRenderer.get_viewport_z_range(z_near, z_far);
    deferredRenderer.set_viewport_z_range(z_near, z_far);
    deferredRenderer.copy(mainRenderer);
    deferredRenderer.set_scissor_region(main_viewport);
    rl_renderer_metrics* metrics_p = rl_render_lib::get_instance()->m_metrics_p;
    metrics_p->clear_warning_flags();
    metrics_p->reset_average_draw_prims_per_group();
    metrics_p->set_particle_batches(0);
    metrics_p->set_particle_batch_average(0);
    metrics_p->reset_num_draw_calls();
    metrics_p->set_num_dynamic_indices(0);
    metrics_p->set_num_dynamic_vert_bytes(0);
    metrics_p->set_num_dynamic_inst_bytes(0);
    metrics_p->reset_num_clone_mesh_widgets();
    metrics_p->reset_num_clone_mesh_vertices();
    metrics_p->reset_num_gpu_vis_entries();
    // Magic numbers: the 16 pixel shader texture slots unbound.
    for (unsigned int slot = 0; slot < 16; slot++)
    {
        mainRenderer.m_pCommandWriter->writeSetFragmentShaderTextureCommand(slot, nullptr);
    }
    if (!RL_enable_scene_processing)
    {
        return;
    }
    if (!RL_scene_processing_onlied)
    {
        initialize_scene_constants(mainRenderer, scene);
        initialize_scene_constants(deferredRenderer, scene);
    }
    if (!RL_scene_processing_locked)
    {
        if (scene.m_partition_system_p != nullptr)
        {
            scene.m_partition_system_p->clean();
        }
        rl_deformation_process_pending_morphs();
    }
    if (RL_enable_gpu_vis_prerender && !RL_scene_processing_onlied)
    {
        rl_gpu_vis_prerender(camera);
    }
    if (Rl_landmarks_enabled && !Rl_blur_landmarks)
    {
        const float landmark_z_far = 1.0f - RL_reserved_landmark_depth_range;
        mainRenderer.set_viewport_z_range(0.0f, landmark_z_far);
        deferredRenderer.set_viewport_z_range(0.0f, landmark_z_far);
    }
    if ((m_flags >> 1 & 1) == 0)
    {
        if (!RL_scene_processing_locked)
        {
            m_base_batched_pass = rl_batched_pass::create();
            rl_visibility_manager* visibility_manager = scene.m_visibility_manager_p;
            m_terrain_renderer_p->reset();
            m_visible_renderable_instances.clear();
            m_visible_lights.clear();
            base_array<pool_list<rl_scene_entity*>*>& lists = visibility_manager->m_visibility_lists;
            lists[0] = (pool_list<rl_scene_entity*>*)&m_terrain_renderer_p->get_visible_renderable_instances();
            lists[1] = (pool_list<rl_scene_entity*>*)&m_visible_renderable_instances;
            lists[2] = (pool_list<rl_scene_entity*>*)&m_visible_lights;
            lists[3] = &m_terrain_renderer_p->get_visible_low_lod_instances();
            lists[4] = &m_terrain_renderer_p->get_visible_landmark_lod_instances();
            lists[5] = &m_terrain_renderer_p->get_visible_skirt_instances();
            lists[6] = &m_terrain_renderer_p->get_visible_road_instances();
            lists[7] = &m_terrain_renderer_p->get_visible_alpha_skirt_instances();
            visibility_manager->m_visible_light_list = &m_visible_lights;
            visibility_manager->m_view_frustum = camera.m_high_lod_frustum;
            visibility_manager->m_low_lod_view_frustum = camera.m_frustum;
            // The landmark frustum: the camera frustum with its far plane (plane 4) moved out to 10 km.
            rl_view_frustum landmark_frustum(camera.m_frustum);
            rl_frustum_base::plane_info& far_plane = landmark_frustum.m_planes[rl_view_frustum::FAR_PLANE];
            // Magic number: -10000 (0xC61C4000), the far plane distance.
            far_plane.m_plane.w = -10000.0f;
            const vector far_normal(far_plane.m_plane.x, far_plane.m_plane.y, far_plane.m_plane.z);
            bbox_find_np_vertices(far_plane.m_neg_verts, far_plane.m_pos_verts, far_normal);
            landmark_frustum.m_cache.m_dirty = true;
            visibility_manager->m_landmark_lod_view_frustum = landmark_frustum;
            visibility_manager->m_viewer_p = &camera;
            visibility_manager->perform_visibility_pass(mainRenderer, camera, &scene);
            m_num_transparent_shadow_objects = 0;
            m_terrain_renderer_p->prerender(&camera, mainRenderer);
            const vector& render_offset = m_scene_p->m_render_offset;
            mainRenderer.set_render_offset(render_offset);
            initialize_deferred();
            reset_deferred();
            mempool_mark_scope scope(Vlib_scratch_pool);
            Current_render_pass = nullptr;
            Current_scene_renderer = this;
            m_base_batched_pass->begin_construction();
            const unsigned int non_shadow_vis_flags = m_scene_p->m_visibility_manager_p->m_non_shadow_vis_flags;
            for (int i = 0; i < m_visible_renderable_instances.size(); i++)
            {
                rl_renderable_instance* renderable_instance_p = m_visible_renderable_instances[i];
                const rl_renderable_instance::sort_method sort_method = renderable_instance_p->get_sort_method();
                if (sort_method == rl_renderable_instance::SORT_METHOD_NONE && !renderable_instance_p->get_is_transparent())
                {
                    if (renderable_instance_p->get_is_batchable())
                    {
                        renderable_instance_p->add_to_batch(m_base_batched_pass, &camera, &m_deferred_ids_to_add);
                    }
                    else
                    {
                        renderable_instance_p->add_to_deferred(&camera, &m_deferred_ids_to_add);
                        // rl_batched_pass::add_unbatched_renderable_instance, inlined.
                        m_base_batched_pass->add_unbatched_renderable_instance(renderable_instance_p);
                    }
                    // The instance's alpha-blended parts, sorted back to front.
                    const unsigned int num_ids = m_deferred_ids_to_add.size();
                    for (unsigned int id = 0; id < num_ids; id++)
                    {
                        const rl_deferred_batch_item& item = m_deferred_ids_to_add.m_elem[id];
                        const int sort_priority = renderable_instance_p->get_sort_priority();
                        const unsigned int index = (unsigned int)_InterlockedExchangeAdd((volatile long*)&m_num_deferred_sort_entries, 1);
                        if ((int)(index + 1) < DEFERRED_SORT_MAX)
                        {
                            deferred_sort_entry& entry = m_deferred_sort_entries[index];
                            entry.sort_priority = sort_priority;
                            entry.sort_method = rl_renderable_instance::SORT_METHOD_BACK_TO_FRONT;
                            entry.render_pass_p = nullptr;
                            entry.renderable_instance_p = renderable_instance_p;
                            entry.dist_to_camera_sq = item.dist_sq;
                            entry.generic_id = item.def_id;
                            entry.secondary_sort_priority = -1;
                            entry.opacity_fade = item.opacity_fade;
                        }
                    }
                    m_deferred_ids_to_add.clear();
                }
                else
                {
                    if ((renderable_instance_p->get_vis_pass_flags() & non_shadow_vis_flags) != 0)
                    {
                        const vector& position = renderable_instance_p->get_position();
                        const vector& fvec = camera.get_rotation().fvec;
                        const float dist = position.y * fvec.y + position.x * fvec.x + position.z * fvec.z;
                        renderable_instance_p->set_camera_distance_squared(dist);
                        add_deferred_sort(renderable_instance_p, nullptr, dist, sort_method, 0xFFFFFFFF, false, -1, -1);
                    }
                    if (renderable_instance_p->get_casts_transparent_shadows() && m_num_transparent_shadow_objects < TRANSPARENT_SHADOW_CASTERS_MAX)
                    {
                        m_transparent_shadow_objects[m_num_transparent_shadow_objects] = renderable_instance_p;
                        m_num_transparent_shadow_objects++;
                    }
                }
            }
            m_base_batched_pass->finalize_construction(mainRenderer);
            Current_scene_renderer = nullptr;
            Current_render_pass = nullptr;
        }
        if (!RL_scene_processing_onlied)
        {
            rl_dev_render_target original_rt = mainRenderer.m_render_target;
            rl_dev_depth_stencil_target original_ds = mainRenderer.m_depth_stencil_target;
            rl_rect original_vp;
            mainRenderer.get_viewport(original_vp);
            mainRenderer.set_viewport(main_viewport);
            const bool msaa_enabled = m_level_of_detail.m_aa_lod != RL_AA_LOD_OFF;
            rl_shadow_render_parameters shadow_render_params;
            shadow_render_params.shadow_texture_matrix = IDENTITY_MATRIX44;
            shadow_render_params.shadow_max_dist = 1000.0f;
            shadow_render_params.shadow_fade_start = 1.0f;
            shadow_render_params.shadow_percent = 1.0f;
            shadow_render_params.shadow_aa_enabled = 0;
            shadow_render_params.shadow_percent_bias = 0.0f;
            shadow_render_params.shadows_enabled = 0;
            shadow_render_params.use_clb_shadows = 0;
            // rl_shadow_system::get_instance() (created on first use) and its CLB render parameters, inlined.
            rl_shadow_system::get_instance();
            shadow_render_params.shadow_fade_start = g_shadowMapFadePct;
            shadow_render_params.shadow_max_dist = g_shadowMapMaxDist;
            shadow_render_params.shadows_enabled = 1;
            shadow_render_params.use_clb_shadows = 1;
            shadow_render_params.shadow_percent = g_shadowPercent;
            m_gpuVisMutex.lock(0);
            m_part2_params.p_camera = part2_camera;
            m_part2_params.p_deferredRenderer = &deferredRenderer;
            m_part2_params.msaa_enabled = msaa_enabled;
            m_part2_params.p_shadow_render_params = &shadow_render_params;
            m_part2_params.p_original_rt = &original_rt;
            m_part2_params.p_hdr_state = &hdr_state;
            m_part2_params.p_original_ds = &original_ds;
            m_part2_params.p_scene = &scene;
            m_part2_params.p_color_correct_state = &color_correct_state;
            m_part2_params.p_original_vp = &original_vp;
            m_startPart2Event.signal();
            mainRenderer.m_render_target.pData = nullptr;
            mainRenderer.m_depth_stencil_target.pData = m_main_scene_depth_stencil_target.pData;
            mainRenderer.begin_pass(m_main_scene_depth_stencil_target.pData);
            rl_color_float clear_color(1.0f, 1.0f, 1.0f, 1.0f);
            if (RL_enable_depth_prepass)
            {
                render_depth_pre_pass(mainRenderer, camera, scene, false, &clear_color, false);
                const keen::TextureData* pDepthBuffer = keen::graphics::getRenderTargetDepthBuffer(m_main_scene_depth_stencil_target.pData);
                mainRenderer.m_pCommandWriter->writeCopyDepthBufferCommand(m_pMainSceneDepthTarget, pDepthBuffer);
                rl_rect current_vp;
                mainRenderer.get_viewport(current_vp);
                mainRenderer.set_viewport(current_vp);
            }
            if (Rl_landmarks_enabled && m_terrain_renderer_p != nullptr)
            {
                m_terrain_renderer_p->prerender_landmarks(mainRenderer, camera);
            }
            if (RL_enable_gpu_vis_render)
            {
                rl_gpu_vis_render(mainRenderer, camera);
            }
            mainRenderer.end_pass();
            m_gpuVisMutex.unlock();
            if (RL_enable_clb)
            {
                rl_light* light = scene.m_largest_light_p != nullptr ? scene.m_largest_light_p : scene.m_largest_light_default_p;
                clb_render(mainRenderer, camera, *light, *scene.m_visibility_manager_p, *m_base_batched_pass, m_transparent_shadow_objects, m_num_transparent_shadow_objects);
            }
            m_endPart2Event.sleepUntilSignaled();
        }
        const bool lock = RL_lock_scene_processing;
        if (!lock)
        {
            m_base_batched_pass->release();
            m_base_batched_pass = nullptr;
        }
        RL_scene_processing_onlied = RL_only_scene_processing;
        RL_scene_processing_locked = lock;
    }
    if (Rl_landmarks_enabled && !Rl_blur_landmarks)
    {
        mainRenderer.set_viewport_z_range(0.0f, 1.0f);
    }
    if ((m_flags >> 1 & 1) != 0 && mainRenderer.m_render_target.pData != nullptr)
    {
        blit_locked_framebuffer(mainRenderer);
    }
    deferredRenderer.apply_macro_state(deferredRenderer.get_macro_state(), deferredRenderer.m_state_p->get_set_sampler_macro_states());
    deferredRenderer.commit_rasterizer_state();
    deferredRenderer.end_pass();
    deferredRenderer.m_pCommandWriter = nullptr;
    keen::graphics::endCommandBuffer(Init_params.m_pKeenGraphicsSystem, pCommandBuffer);
    keen::GraphicsCommandWriter* pMainWriter = mainRenderer.m_pCommandWriter;
    mainRenderer.apply_macro_state(mainRenderer.get_macro_state(), mainRenderer.m_state_p->get_set_sampler_macro_states());
    mainRenderer.commit_rasterizer_state();
    mainRenderer.end_pass();
    mainRenderer.m_pCommandWriter = nullptr;
    keen::graphics::callCommandBuffer(pMainWriter->getBuffer(), m_pRecordedCommandBuffer);
    mainRenderer.begin(pMainWriter);
    part2_camera->release();
}

// The locked framebuffer (complete_lock_frame_buffer's copy) drawn into the main renderer's target with the blit
// shader (part of render, inlined).
void rl_scene_renderer::blit_locked_framebuffer(rl_renderer& mainRenderer)
{
    const rl_dev_render_target target = mainRenderer.m_render_target;
    rl_rect saved_vp;
    mainRenderer.get_viewport(saved_vp);
    const rl_primitive_state saved_state = mainRenderer.get_macro_state();
    mainRenderer.set_shader_type(shader_type_generic);
    mainRenderer.set_shader_configuration(0);
    mainRenderer.set_active_shader(m_blit_shader, rl_shader_technique_user, 0);
    mainRenderer.m_pCommandWriter->writeSetFragmentShaderTextureCommand(0, m_locked_framebuffer_texture.pData);
    const keen::RenderTarget* pRenderTarget = mainRenderer.m_pCommandWriter->getCurrentRenderTarget();
    const unsigned int height = keen::graphics::getRenderTargetHeight(pRenderTarget);
    const unsigned int width = keen::graphics::getRenderTargetWidth(pRenderTarget);
    rl_rect current_vp;
    mainRenderer.get_viewport(current_vp);
    float z_near;
    float z_far;
    mainRenderer.get_viewport_z_range(z_near, z_far);
    mainRenderer.m_render_target = target;
    mainRenderer.m_depth_stencil_target.pData = nullptr;
    mainRenderer.begin_pass(target.pData);
    const rl_primitive_state pass_state = mainRenderer.get_macro_state();
    // Magic numbers: the blit state keeps the zbias, const alpha and valid bits and sets clamp (0x20).
    const rl_primitive_state blit_state = macro_state_bits(pass_state.get_bits() & 0xFFFF8020 | 0x20);
    mainRenderer.set_viewport(rl_rect(0, 0, width, height));
    mainRenderer.set_viewport_z_range(0.0f, 1.0f);
    mainRenderer.set_target_dimensions(width, height);
    mainRenderer.set_macro_state(blit_state, false);
    // Magic number: clear flags 8.
    mainRenderer.clear(rl_color_float(0.0f, 0.0f, 0.0f, 0.0f), 8, 1.0f, 0);
    mainRenderer.set_macro_state(blit_state, false);
    mainRenderer.set_vertex_format(RLVF_COMPOSITOR);
    mainRenderer.draw_primitive(RL_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, 2, vertex_data, sizeof(rl_compositor_vertex));
    mainRenderer.set_macro_state(pass_state, false);
    mainRenderer.end_pass();
    mainRenderer.set_viewport(current_vp);
    mainRenderer.set_viewport_z_range(z_near, z_far);
    mainRenderer.set_macro_state(saved_state, false);
    mainRenderer.set_viewport(saved_vp);
}

// 0x00562DF0 (:757)
// Waits for part 1's signal, renders part 2 with its parameters and signals the end, until the thread is asked to quit.
int rl_scene_renderer::runSceneRenderPart2(const keen::Thread& thread)
{
    while (!thread.isQuitRequested())
    {
        m_startPart2Event.sleepUntilSignaled();
        if (thread.isQuitRequested())
        {
            break;
        }
        const rl_rect original_vp = *m_part2_params.p_original_vp;
        render_scene_part2(*m_part2_params.p_deferredRenderer, *m_part2_params.p_camera, m_part2_params.msaa_enabled, *m_part2_params.p_scene, *m_part2_params.p_original_rt, *m_part2_params.p_shadow_render_params, *m_part2_params.p_hdr_state, *m_part2_params.p_color_correct_state, *m_part2_params.p_original_ds, original_vp);
        m_endPart2Event.signal();
    }
    return 0;
}

// 0x005601B0 (:782)
// See the file header. The main scene target is cleared to the fog color (no alpha) and the base pass draws the base
// pass visibility (and the debug CLB passes) at equal depth against part 1's pre-pass. Sun shafts need the camera to
// face the sun (the HDR state's sun position). The particle path downsamples the depth and calculates the fog color
// for the particle composite. The post effects composite into the first temp target (or the destination), in the
// order HDR composite, depth of field and distortion (on the blurred downsample), FXAA, then the x-ray effect.
void rl_scene_renderer::render_scene_part2(rl_renderer& deferredRenderer, rl_camera& camera, const bool msaa_enabled, rl_scene& scene, rl_dev_render_target original_rt, rl_shadow_render_parameters& shadow_render_params, const rl_hdr_state& hdr_state, const rl_color_correct_state& color_correct_state, rl_dev_depth_stencil_target original_ds, rl_rect original_vp)
{
    bool ssao_enabled = false;
    if (m_level_of_detail.m_ssao_enabled)
    {
        SsaoRenderSettings ssao_settings;
        ssao_settings.minZ = deferredRenderer.m_z_near;
        ssao_settings.maxZ = deferredRenderer.m_z_far;
        ssao_enabled = true;
        renderSsaoEffect(m_pSsaoContext, deferredRenderer.m_pCommandWriter, ssao_settings, camera, m_pMainSceneDepthTexture, m_pClbTarget, m_pSsaoBlurTarget[0], m_pSsaoBlurTarget[1]);
        deferredRenderer.set_macro_state(deferredRenderer.get_macro_state(), true);
    }
    const rl_dev_depth_stencil_target& scene_ds = msaa_enabled ? m_main_scene_depth_stencil_target_msaa : m_main_scene_depth_stencil_target;
    const rl_dev_render_target& scene_rt = m_main_scene_render_target.get_dev_render_target();
    deferredRenderer.begin_pass(scene_rt, scene_ds);
    const rl_color_float& fog_color = scene.get_fog_state().get_color();
    rl_color_float clear_color(fog_color.red, fog_color.green, fog_color.blue, 0.0f);
    if (msaa_enabled)
    {
        if (RL_enable_msaa_prepass)
        {
            render_depth_pre_pass(deferredRenderer, camera, scene, false, &clear_color, true);
        }
        else
        {
            deferredRenderer.clear(clear_color, 0xFFFFFFFF, 1.0f, 0);
        }
    }
    else
    {
        // Magic number: clear flags 1, color only (the depth is part 1's pre-pass).
        deferredRenderer.clear(clear_color, 1, 1.0f, 0);
    }
    deferredRenderer.end_pass();
    if (ssao_enabled && g_ssaoVisEnabled)
    {
        renderSsaoDebug(m_pSsaoContext, deferredRenderer.m_pCommandWriter, original_rt.pData, m_pClbTexture);
        deferredRenderer.set_macro_state(deferredRenderer.get_macro_state(), true);
    }
    else
    {
        render_scene_part2_passes(deferredRenderer, camera, msaa_enabled, scene, original_rt, shadow_render_params, hdr_state, color_correct_state, original_ds);
    }
    if ((m_flags & kFramebufferLockInitiated) != 0)
    {
        complete_lock_frame_buffer(deferredRenderer);
    }
    deferredRenderer.set_viewport(original_vp);
}

// The scene passes, particles and post effects of render_scene_part2 (inlined there).
void rl_scene_renderer::render_scene_part2_passes(rl_renderer& deferredRenderer, rl_camera& camera, const bool msaa_enabled, rl_scene& scene, rl_dev_render_target original_rt, rl_shadow_render_parameters& shadow_render_params, const rl_hdr_state& hdr_state, const rl_color_correct_state& color_correct_state, rl_dev_depth_stencil_target original_ds)
{
    const rl_dev_depth_stencil_target& scene_ds = msaa_enabled ? m_main_scene_depth_stencil_target_msaa : m_main_scene_depth_stencil_target;
    deferredRenderer.begin_pass(m_main_scene_render_target.get_dev_render_target(), scene_ds);
    rl_visibility_manager* visibility_manager = scene.m_visibility_manager_p;
    rl_light* light = scene.m_largest_light_p != nullptr ? scene.m_largest_light_p : scene.m_largest_light_default_p;
    if (RL_enable_base_pass)
    {
        const unsigned int base_flags = RL_vis_base ? 1u << (visibility_manager->m_base_pass_vis_index & 0x1F) : 0;
        const unsigned int clb_near_flags = RL_vis_clb_near ? 1u << (visibility_manager->m_clb_base_vis_index & 0x1F) : 0;
        const unsigned int clb_mid_flags = RL_vis_clb_mid ? 1u << (visibility_manager->m_clb_base_vis_index + 1 & 0x1F) : 0;
        const unsigned int clb_far_flags = RL_vis_clb_far ? 1u << (visibility_manager->m_clb_base_vis_index + 2 & 0x1F) : 0;
        const unsigned int clb_terrain_flags = RL_vis_clb_terrain ? 1u << (visibility_manager->m_clb_base_vis_index + 3 & 0x1F) : 0;
        const unsigned int clb_drop_flags = RL_vis_clb_drop ? 1u << (visibility_manager->m_drop_shadow_vis_index & 0x1F) : 0;
        render_base_pass(deferredRenderer, camera, *light, shadow_render_params, m_terrain_renderer_p, *m_base_batched_pass, base_flags | clb_near_flags | clb_mid_flags | clb_far_flags | clb_terrain_flags | clb_drop_flags, false, false, RLPCW_RGBA, msaa_enabled, true, RL_TERRAIN_ALL);
    }
    vector view_direction = camera.get_rotation().fvec;
    const vector& camera_position = camera.get_position();
    vector to_sun(hdr_state.m_sun_shafts_position.x - camera_position.x, hdr_state.m_sun_shafts_position.y - camera_position.y, hdr_state.m_sun_shafts_position.z - camera_position.z);
    view_direction.normalize();
    to_sun.normalize();
    bool sun_shafts_enabled = false;
    if (Rl_sun_shafts_enabled && m_level_of_detail.m_sun_shafts_enabled)
    {
        sun_shafts_enabled = to_sun.x * view_direction.x + to_sun.y * view_direction.y + to_sun.z * view_direction.z > 0.0f;
    }
    rl_renderable_instance* skybox_p = m_scene_p->m_skybox_p;
    if (RL_enable_skybox && skybox_p != nullptr)
    {
        const rl_primitive_state saved_state = deferredRenderer.get_macro_state();
        rl_rect saved_vp;
        deferredRenderer.get_viewport(saved_vp);
        // The sky state: the saved state with the stencil mode (bits 9-12) cleared, stencil write (bit 9) when the
        // sun shafts mask the sky, and msaa (bit 19).
        const rl_primitive_state sky_state = macro_state_bits(((unsigned int)msaa_enabled << 10 | (unsigned int)sun_shafts_enabled) << 9 | saved_state.get_bits() & 0xFFF7E1FF);
        deferredRenderer.set_macro_state(sky_state, false);
        if (sun_shafts_enabled)
        {
            // Magic number: clear flags 4, stencil.
            deferredRenderer.clear(rl_color(0), 4, 1.0f, 0);
        }
        skybox_p->render(deferredRenderer, camera, 0);
        deferredRenderer.set_macro_state(sky_state, false);
        deferredRenderer.set_viewport(saved_vp);
    }
    if (Rl_landmarks_enabled && m_terrain_renderer_p != nullptr)
    {
        const rl_primitive_state saved_state = deferredRenderer.get_macro_state();
        deferredRenderer.set_macro_state(saved_state, false);
        m_terrain_renderer_p->render_landmarks(deferredRenderer, camera);
        deferredRenderer.set_macro_state(saved_state, false);
    }
    if (sun_shafts_enabled)
    {
        const rl_primitive_state saved_state = deferredRenderer.get_macro_state();
        // The saved state with msaa (bit 19).
        deferredRenderer.set_macro_state(macro_state_bits(saved_state.get_bits() & 0xFFF7FFFF | (unsigned int)msaa_enabled << 19), false);
        // The sky's stencil mask goes into the first SSAO blur target (0x33C in rfg.exe), which the HDR compositor then
        // reads as the sun shafts mask; m_sun_shafts_skybox_mask_target is never created in rfg.exe (its null target
        // would make begin_pass fall back to the main scene target and overwrite the scene with the mask).
        m_compositor_p->mask_stencil(deferredRenderer, rl_dev_render_target(m_pSsaoBlurTarget[0]), deferredRenderer.m_depth_stencil_target, m_sun_shafts_skybox_mask_width, m_sun_shafts_skybox_mask_height);
        deferredRenderer.clear(rl_color::BLACK, 4, 1.0f, 0);
        deferredRenderer.set_macro_state(saved_state, false);
    }
    if (RL_enable_secondary_lights)
    {
        render_lighting_passes(deferredRenderer, camera, *light, *m_base_batched_pass, m_transparent_shadow_objects, m_num_transparent_shadow_objects, msaa_enabled);
    }
    if (RL_enable_render_deferred)
    {
        float fade_end = scene.m_opacity_distance_end;
        float fade_start = scene.m_opacity_distance_start;
        if (RL_alpha_dist_override)
        {
            fade_end = RL_alpha_dist_end;
            fade_start = RL_alpha_dist_start;
        }
        render_deferred(deferredRenderer, camera, *light, shadow_render_params, 1u << (visibility_manager->m_base_pass_vis_index & 0x1F), fade_end - fade_start, false, true, msaa_enabled);
    }
    deferredRenderer.end_pass();
    const rl_primitive_state saved_state = deferredRenderer.get_macro_state();
    deferredRenderer.set_macro_state(saved_state, false);
    m_gpuVisMutex.lock(0);
    const rl_dev_texture& scene_depth_texture = m_main_scene_render_target.get_texture_data();
    rl_outline_state outline_state = scene.m_outline_state;
    if (RL_enable_particle_rendering)
    {
        render_particles(deferredRenderer, camera, scene, outline_state, scene_depth_texture, hdr_state, color_correct_state);
    }
    m_gpuVisMutex.unlock();
    if (RL_enable_post_effects)
    {
        render_post_effects(deferredRenderer, camera, scene, original_rt, shadow_render_params, hdr_state, color_correct_state, original_ds, sun_shafts_enabled);
    }
}

// The particle part of render_scene_part2 (inlined there): the half resolution depth (unless very high particle LOD
// without outlines and bloom), the outline edge mask (bloom), the particle fog color, the deferred particles, and the
// outlines added to the bloom.
void rl_scene_renderer::render_particles(rl_renderer& deferredRenderer, rl_camera& camera, rl_scene& scene, rl_outline_state& outline_state, const rl_dev_texture& scene_depth_texture, const rl_hdr_state& hdr_state, const rl_color_correct_state& color_correct_state)
{
    rl_particle_renderer* particle_renderer = rl_particle_renderer::get_instance();
    bool downsample = true;
    if (m_level_of_detail.m_particle_lod == RL_PARTICLE_LOD_VERY_HIGH)
    {
        downsample = false;
        for (int layer = 0; layer < 4; layer++)
        {
            if (outline_state.layer[layer] != RLOL_DISABLED)
            {
                downsample = m_level_of_detail.m_bloom_enabled;
                break;
            }
        }
    }
    if (downsample)
    {
        particle_renderer->downsample_depth(deferredRenderer, *m_compositor_p, scene_depth_texture, m_main_scene_target_width, m_main_scene_target_height, m_half_depth_target.get_dev_render_target(), m_half_depth_stencil_target, m_half_target_width, m_half_target_height);
        if (m_level_of_detail.m_bloom_enabled)
        {
            const keen::TextureData* pHalfDepthTexture = m_half_depth_target.get_texture_data().pData;
            rl_light* light = scene.m_largest_light_p != nullptr ? scene.m_largest_light_p : scene.m_largest_light_default_p;
            m_outline_compositor_p->render_outline_edge_mask(deferredRenderer, *this, *m_compositor_p, camera, *light, m_outline_target.get_dev_render_target(), m_outline_width, m_outline_height, outline_state, *m_base_batched_pass, *scene.m_visibility_manager_p, pHalfDepthTexture);
        }
    }
    const int scene_texture = m_main_scene_render_target.get_texture_handle();
    m_hdr_compositor_p->calculate_particle_fog_color(deferredRenderer, *m_compositor_p, scene.get_fog_state().get_color(), m_particle_fog_color_target.get_dev_render_target(), scene_texture, hdr_state, color_correct_state);
    rl_particle_framebuffer_state fullscreen;
    fullscreen.m_render_target_p = &m_full_res_particle_target;
    fullscreen.m_depth_stencil = m_main_scene_depth_stencil_target;
    fullscreen.m_depth_texture.pData = (keen::TextureData*)m_pMainSceneDepthTexture;
    fullscreen.m_width = m_main_scene_target_width;
    fullscreen.m_height = m_main_scene_target_height;
    rl_particle_framebuffer_state halfscreen;
    halfscreen.m_render_target_p = &m_half_render_target;
    halfscreen.m_depth_stencil = m_half_depth_stencil_target;
    halfscreen.m_depth_texture = m_half_depth_target.get_texture_data();
    halfscreen.m_width = m_half_target_width;
    halfscreen.m_height = m_half_target_height;
    rl_particle_brightpass_state brightpass;
    brightpass.m_render_target = m_particle_brightpass_target.get_dev_render_target();
    brightpass.m_width = m_particle_brightpass_width;
    brightpass.m_height = m_particle_brightpass_height;
    brightpass.m_enabled = m_level_of_detail.m_bloom_enabled;
    rl_particle_distortion_state distortion;
    distortion.m_render_target = m_distortion_target.get_dev_render_target();
    distortion.m_width = m_distortion_width;
    distortion.m_height = m_distortion_height;
    distortion.m_material = scene.m_distortion_effect_material;
    distortion.m_batch_p = m_base_batched_pass;
    distortion.m_vis_flags = 1u << (scene.m_visibility_manager_p->m_distortion_vis_index & 0x1F);
    distortion.m_enabled = m_level_of_detail.m_distortion_enabled;
    distortion.m_distortion_only_entries_p = &m_distortion_only_entries;
    // Magic numbers: the particle tint (1.6, 1.6, 1.6, 1).
    const rl_color_float particle_tint(1.6f, 1.6f, 1.6f, 1.0f);
    const int fog_color_texture = m_particle_fog_color_target.get_texture_handle();
    particle_renderer->render_deferred(deferredRenderer, *m_compositor_p, camera, fullscreen, halfscreen, brightpass, distortion, scene.m_particle_lighting_state, fog_color_texture, particle_tint);
    if (m_level_of_detail.m_bloom_enabled)
    {
        const int outline_texture = m_outline_target.get_texture_handle();
        m_outline_compositor_p->add_outline_to_buffer(deferredRenderer, *m_compositor_p, m_particle_brightpass_target.get_dev_render_target(), m_particle_brightpass_width, m_particle_brightpass_height, outline_state, outline_texture);
    }
}

// The post effect part of render_scene_part2 (inlined there). The effects ping-pong between the two temp targets;
// the last enabled one writes the destination (the first temp target while the framebuffer is locked).
// Notes (decompiled behaviour, kept as is): the x-ray effect runs with the shadows switched off in the shared shadow
// parameters, and renders into the destination with the original depth stencil.
void rl_scene_renderer::render_post_effects(rl_renderer& deferredRenderer, rl_camera& camera, rl_scene& scene, rl_dev_render_target original_rt, rl_shadow_render_parameters& shadow_render_params, const rl_hdr_state& hdr_state, const rl_color_correct_state& color_correct_state, rl_dev_depth_stencil_target original_ds, bool sun_shafts_enabled)
{
    rl_dev_render_target destination = original_rt;
    if ((m_flags & kFramebufferLockInitiated) != 0)
    {
        destination = m_main_scene_temp_target[0].get_dev_render_target();
    }
    const bool fxaa = g_fxaaEnabled;
    const bool distortion = m_level_of_detail.m_distortion_enabled;
    const bool depth_of_field = (camera.m_depth_of_field_enabled || camera.m_frame_override_blur != 0.0f) && m_level_of_detail.m_depth_of_field_enabled;
    const keen::TextureData* pFxaaInput = m_main_scene_temp_target[1].get_texture_data().pData;
    const rl_dev_render_target temp0_rt = m_main_scene_temp_target[0].get_dev_render_target();
    // The composite target and the source/target of each later effect.
    unsigned int read_index = fxaa ? 0 : 1;
    unsigned int write_index = fxaa ? 1 : 0;
    const int distortion_source = m_main_scene_temp_target[read_index].get_texture_handle();
    rl_dev_render_target distortion_target = m_main_scene_temp_target[write_index].get_dev_render_target();
    if (distortion)
    {
        read_index ^= 1;
        write_index ^= 1;
    }
    const int dof_source = m_main_scene_temp_target[read_index].get_texture_handle();
    rl_dev_render_target dof_target = m_main_scene_temp_target[write_index].get_dev_render_target();
    if (depth_of_field)
    {
        write_index ^= 1;
    }
    rl_dev_render_target composite_target = m_main_scene_temp_target[write_index].get_dev_render_target();
    const int downsample_source = m_main_scene_temp_target[write_index].get_texture_handle();
    rl_dev_render_target fxaa_target = destination;
    if (!fxaa)
    {
        fxaa_target = temp0_rt;
        if (!distortion)
        {
            if (!depth_of_field)
            {
                composite_target = destination;
            }
            else
            {
                dof_target = destination;
            }
        }
        else
        {
            distortion_target = destination;
        }
    }
    // The sky mask written by mask_stencil above, only on frames that draw sun shafts (0x00560FD4 tests the same flag
    // as the sky stencil and mask_stencil). A mask without sun shafts would send the HDR compositor into
    // m_downsampled_sun_shafts(_int), which are not created when the sun shafts level of detail is off.
    rl_dev_texture sun_shafts_mask;
    if (sun_shafts_enabled)
    {
        sun_shafts_mask.pData = (keen::TextureData*)keen::graphics::getRenderTargetColorBuffer(m_pSsaoBlurTarget[0], 0);
    }
    const int scene_texture = m_main_scene_render_target.get_texture_handle();
    const int particle_brightpass_texture = m_particle_brightpass_target.get_texture_handle();
    const int particle_texture = m_full_res_particle_target.get_texture_handle();
    m_hdr_compositor_p->composite(deferredRenderer, *m_compositor_p, camera, particle_texture, particle_brightpass_texture, sun_shafts_mask, hdr_state, color_correct_state, scene_texture, composite_target);
    if (depth_of_field || distortion)
    {
        rl_dev_texture no_mask;
        m_compositor_p->downsample(deferredRenderer, downsample_source, m_main_scene_target_width, m_main_scene_target_height, m_main_scene_downsample_target.get_dev_render_target(), m_main_scene_downsample_width, m_main_scene_downsample_height, 0, no_mask);
        m_blur_kernel.init_simple(m_main_scene_downsample_width, m_main_scene_downsample_height);
        m_compositor_p->simple_blur(deferredRenderer, m_main_scene_downsample_target.get_texture_handle(), m_main_scene_blur_target.get_dev_render_target(), m_main_scene_downsample_width, m_main_scene_downsample_height, m_blur_kernel);
        if (depth_of_field)
        {
            rl_dev_texture depth_texture((keen::TextureData*)m_pMainSceneDepthTexture);
            m_depth_of_field.composite(deferredRenderer, *m_compositor_p, camera, dof_source, depth_texture, m_main_scene_blur_target.get_texture_handle(), dof_target, m_main_scene_target_width, m_main_scene_target_height);
        }
        if (distortion)
        {
            const bool diffraction_enabled = scene.m_diffraction_enabled;
            const int blur_texture = m_main_scene_blur_target.get_texture_handle();
            const int distortion_texture = m_distortion_target.get_texture_handle();
            m_distortion.composite(deferredRenderer, *m_compositor_p, camera, distortion_source, distortion_texture, blur_texture, distortion_target, m_main_scene_target_width, m_main_scene_target_height, diffraction_enabled);
        }
    }
    if (fxaa)
    {
        keen::renderFxaaEffect(m_pFxaaContext, deferredRenderer.m_pCommandWriter, pFxaaInput, fxaa_target.pData, 0.0f);
    }
    const rl_xray_render_state xray_state = scene.m_xray_render_state;
    shadow_render_params.shadows_enabled = 0;
    shadow_render_params.use_clb_shadows = 0;
    rl_light* light = scene.m_largest_light_p != nullptr ? scene.m_largest_light_p : scene.m_largest_light_default_p;
    render_xray_effect(deferredRenderer, camera, *light, *scene.m_visibility_manager_p, shadow_render_params, *m_terrain_renderer_p, xray_state, destination, original_ds, *m_base_batched_pass, m_visible_renderable_instances);
}

// 0x004C7660 (:1205)
// Into the sorted, first or last list by sort method (none counts as back to front); dropped when the list is full.
void rl_scene_renderer::add_deferred_sort(rl_renderable_instance* renderable_instance_p, rl_render_pass* render_pass_p, float dist_to_cam_sq, rl_renderable_instance::sort_method sort_method, unsigned int generic_id, bool opacity_fade, int sort_priority, int secondary_sort_priority)
{
    if (sort_priority == -1)
    {
        sort_priority = renderable_instance_p->get_sort_priority();
    }
    if (sort_method == rl_renderable_instance::SORT_METHOD_NONE)
    {
        sort_method = rl_renderable_instance::SORT_METHOD_BACK_TO_FRONT;
    }
    deferred_sort_entry* entry = nullptr;
    if (sort_method == rl_renderable_instance::SORT_METHOD_BACK_TO_FRONT || sort_method == rl_renderable_instance::SORT_METHOD_FRONT_TO_BACK)
    {
        const unsigned int index = (unsigned int)_InterlockedExchangeAdd((volatile long*)&m_num_deferred_sort_entries, 1);
        if ((int)(index + 1) >= DEFERRED_SORT_MAX)
        {
            return;
        }
        entry = &m_deferred_sort_entries[index];
    }
    if (sort_method == rl_renderable_instance::SORT_METHOD_FIRST)
    {
        const unsigned int index = (unsigned int)_InterlockedExchangeAdd((volatile long*)&m_num_deferred_sort_entries_first, 1);
        if ((int)(index + 1) >= DEFERRED_SORT_FIRST_MAX)
        {
            return;
        }
        entry = &m_deferred_sort_entries_first[index];
    }
    else if (sort_method == rl_renderable_instance::SORT_METHOD_LAST)
    {
        const unsigned int index = (unsigned int)_InterlockedExchangeAdd((volatile long*)&m_num_deferred_sort_entries_last, 1);
        if ((int)(index + 1) >= DEFERRED_SORT_LAST_MAX)
        {
            return;
        }
        entry = &m_deferred_sort_entries_last[index];
    }
    entry->sort_method = sort_method;
    entry->render_pass_p = render_pass_p;
    entry->generic_id = generic_id;
    entry->renderable_instance_p = renderable_instance_p;
    entry->dist_to_camera_sq = dist_to_cam_sq;
    entry->sort_priority = sort_priority;
    entry->secondary_sort_priority = secondary_sort_priority;
    entry->opacity_fade = opacity_fade;
}

// 0x00517A90 (:1243)
void rl_scene_renderer::reset_deferred()
{
    m_num_deferred_sort_entries = 0;
    m_num_deferred_sort_entries_first = 0;
    m_num_deferred_sort_entries_last = 0;
    m_distortion_only_entries.clear();
}

// 0x00544770 (:1251)
// The terrain (depth or base pass), then the batch of the passes and its decals (unless only the terrain is drawn),
// then the terrain's alpha skirts.
void rl_scene_renderer::render_base_pass(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_shadow_render_parameters& shadow_render_params, rl_iterrain_renderer* terrain_renderer_p, rl_batched_pass& batched_pass, const unsigned int vis_flags, const bool skip_decals, const bool alpha_blend, const rl_prim_color_write_mode color_write_mode, const bool msaa_enabled, bool isMainPass, rl_terrain_mode terrain_mode)
{
    const rl_primitive_state saved_state = renderer.get_macro_state();
    begin_base_pass(renderer, camera, light, shadow_render_params, alpha_blend, color_write_mode, msaa_enabled, isMainPass);
    if (terrain_renderer_p != nullptr)
    {
        if (renderer.get_depth_only())
        {
            terrain_renderer_p->render_depth(renderer, camera, &light, vis_flags, false);
        }
        else
        {
            terrain_renderer_p->render_base_pass(renderer, camera, &light, vis_flags, terrain_mode, false);
        }
    }
    if (terrain_mode != RL_TERRAIN_ONLY_LOW_LOD)
    {
        batched_pass.render(renderer, camera, vis_flags, skip_decals);
        if (!skip_decals)
        {
            batched_pass.render_decals(renderer);
        }
    }
    if (terrain_renderer_p != nullptr && !renderer.get_depth_only())
    {
        terrain_renderer_p->render_alpha_skirts(renderer, camera, &light, vis_flags);
    }
    rl_material_library_clear_render_material(renderer);
    camera.render_refresh(renderer);
    renderer.set_macro_state(saved_state, false);
}

// 0x00544870 (:1286)
// Each entry of the passes: distortion-only instances are collected for the distortion pass; the others draw their
// deferred part (render_for_deferred with the entry's id), additive ones with the additive alpha mode. An opacity fade
// entry fades its instance's opacity over fade_range from RL_alpha_dist_start (the opacity is restored after).
// Notes (decompiled behaviour, kept as is): the fade moves the opacity towards 2 (t * (2 - opacity) + opacity), and
// the saved tint is restored with its blue in the red and green positions' places as read (red, green, blue, alpha
// from the state).
void rl_scene_renderer::render_deferred_list(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_shadow_render_parameters& shadow_render_params, const unsigned int vis_flags, const float fade_range, const bool skip_decals, const bool alpha_blend, const deferred_sort_entry* list, unsigned int num_entries, const bool msaa_enabled)
{
    begin_base_pass(renderer, camera, light, shadow_render_params, alpha_blend, RLPCW_RGBA, msaa_enabled, false);
    const rl_color_float saved_tint = renderer.get_tint_color();
    const unsigned int render_flags = skip_decals ? 2 : 0;
    for (unsigned int i = 0; i < num_entries; i++)
    {
        const deferred_sort_entry& entry = list[i];
        rl_renderable_instance* inst = entry.renderable_instance_p;
        if (inst->get_distortion_only())
        {
            m_distortion_only_entries.add(inst);
            continue;
        }
        if ((vis_flags & inst->get_vis_pass_flags()) == 0)
        {
            continue;
        }
        const float saved_opacity = inst->get_opacity();
        if (entry.opacity_fade)
        {
            const vector& fvec = camera.get_rotation().fvec;
            const vector& eye = camera.get_position();
            const float eye_depth = (float)((double)eye.y * fvec.y + (double)fvec.x * eye.x + (double)eye.z * fvec.z);
            const float t = (float)(((double)(float)fabs((double)entry.dist_to_camera_sq - eye_depth) - RL_alpha_dist_start) / ((double)fade_range + 1e-07));
            // Magic number: 2.0 (see the note).
            inst->set_opacity((float)((double)t * (2.0 - (double)saved_opacity) + (double)saved_opacity));
        }
        const rl_primitive_state state = renderer.get_macro_state();
        if (inst->get_is_additive())
        {
            rl_primitive_state additive_state = state;
            additive_state.set_alpha_mode(RLPAM_ADDITIVE);
            renderer.set_macro_state(additive_state, false);
        }
        inst->render_for_deferred(renderer, camera, entry.generic_id, render_flags);
        if (inst->get_is_additive())
        {
            renderer.set_macro_state(state, false);
        }
        if (entry.opacity_fade)
        {
            inst->set_opacity(saved_opacity);
        }
    }
    renderer.set_tint_color(saved_tint);
    rl_material_library_clear_render_material(renderer);
    camera.render_refresh(renderer);
}

// 0x0055D140 (:1344)
// The particle renderer's deferred list reset, the sorted list back to front, then the first, sorted and last lists.
void rl_scene_renderer::render_deferred(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_shadow_render_parameters& shadow_render_params, const unsigned int vis_flags, const float fade_range, const bool skip_decals, const bool alpha_blend, const bool msaa_enabled)
{
    rl_particle_renderer::get_instance()->reset_deferred();
    std::sort(m_deferred_sort_entries, m_deferred_sort_entries + m_num_deferred_sort_entries, deferred_sort_entry::btf_predicate);
    render_deferred_list(renderer, camera, light, shadow_render_params, vis_flags, fade_range, skip_decals, alpha_blend, m_deferred_sort_entries_first, m_num_deferred_sort_entries_first, msaa_enabled);
    render_deferred_list(renderer, camera, light, shadow_render_params, vis_flags, fade_range, skip_decals, alpha_blend, m_deferred_sort_entries, m_num_deferred_sort_entries, msaa_enabled);
    render_deferred_list(renderer, camera, light, shadow_render_params, vis_flags, fade_range, skip_decals, alpha_blend, m_deferred_sort_entries_last, m_num_deferred_sort_entries_last, msaa_enabled);
}

// 0x0052E2D0 (:1358)
// The main light, the base pass state and the shadow states. Depth test: read-equal for the opaque main pass (the
// pre-pass wrote the depth), full for other opaque passes, read for alpha-blended ones (which also use the filter
// alpha mode); back face culling (0x2000).
void rl_scene_renderer::begin_base_pass(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_shadow_render_parameters& shadow_render_params, const bool alpha_blend, const rl_prim_color_write_mode color_write_mode, const bool msaa_enabled, bool isMainPass)
{
    rl_material_library_clear_render_material(renderer);
    renderer.set_active_camera(&camera);
    renderer.set_is_base_pass(true);
    renderer.set_light(light, true);
    rl_prim_zbuf_mode zbuf_mode;
    if (!isMainPass)
    {
        zbuf_mode = alpha_blend ? RLPZM_READ : RLPZM_FULL;
    }
    else
    {
        zbuf_mode = alpha_blend ? RLPZM_READ : RLPZM_READ_EQUAL;
    }
    // Magic numbers: the packed macro state (color write bits 15-17, msaa bit 19, filter alpha 5, zbuf bits 6-8, cull
    // bit 13).
    const unsigned int bits = ((unsigned int)msaa_enabled << 4 | ((unsigned int)color_write_mode & RLPCW_GA)) << 15 | (alpha_blend ? 5u : 0u) | ((unsigned int)zbuf_mode & 7) << 6 | 0x2000;
    renderer.set_macro_state(macro_state_bits(bits), false);
    set_shadow_states(shadow_render_params, renderer);
    renderer.set_world_transform(IDENTITY_MATRIX43);
    camera.render_refresh(renderer);
}

// 0x0052E3A0 (:1389)
void rl_scene_renderer::end_base_pass(rl_camera& camera, rl_renderer& renderer)
{
    rl_material_library_clear_render_material(renderer);
    camera.render_refresh(renderer);
}

// 0x00517AD0 (:1399)
// Destroys the targets, the compositors' targets and the recorded command buffer (resolution or LOD change).
void rl_scene_renderer::release_resources()
{
    m_main_scene_render_target.destroy();
    m_half_render_target.destroy();
    m_half_depth_target.destroy();
    m_full_res_particle_target.destroy();
    m_particle_brightpass_target.destroy();
    m_particle_fog_color_target.destroy();
    m_distortion_target.destroy();
    if (m_main_scene_depth_stencil_target_msaa.pData != nullptr)
    {
        keen::graphics::destroyRenderTarget(Init_params.m_pKeenGraphicsSystem, m_main_scene_depth_stencil_target_msaa.pData);
    }
    m_main_scene_depth_stencil_target_msaa.pData = nullptr;
    if (m_main_scene_depth_stencil_target.pData != nullptr)
    {
        keen::graphics::destroyRenderTarget(Init_params.m_pKeenGraphicsSystem, m_main_scene_depth_stencil_target.pData);
    }
    m_main_scene_depth_stencil_target.pData = nullptr;
    if (m_half_depth_stencil_target.pData != nullptr)
    {
        keen::graphics::destroyRenderTarget(Init_params.m_pKeenGraphicsSystem, m_half_depth_stencil_target.pData);
    }
    m_half_depth_stencil_target.pData = nullptr;
    m_main_scene_downsample_target.destroy();
    m_main_scene_blur_target.destroy();
    m_main_scene_temp_target[0].destroy();
    m_main_scene_temp_target[1].destroy();
    m_shadow_map_target.destroy();
    m_outline_target.destroy();
    for (int i = 0; i < 2; i++)
    {
        keen::graphics::destroyRenderTarget(Init_params.m_pKeenGraphicsSystem, m_pSsaoBlurTarget[i]);
        m_pSsaoBlurTarget[i] = nullptr;
        keen::graphics::freeTextureData(m_pSsaoBlurTexture[i], Init_params.m_pKeenGraphicsSystem, g_pFramebufferAllocator);
        delete m_pSsaoBlurTexture[i];
        m_pSsaoBlurTexture[i] = nullptr;
    }
    keen::graphics::destroyRenderTarget(Init_params.m_pKeenGraphicsSystem, m_pMainSceneDepthTarget);
    m_pMainSceneDepthTarget = nullptr;
    keen::graphics::freeTextureData((keen::TextureData*)m_pMainSceneDepthTexture, Init_params.m_pKeenGraphicsSystem, g_pFramebufferAllocator);
    delete m_pMainSceneDepthTexture;
    m_pMainSceneDepthTexture = nullptr;
    keen::graphics::destroyRenderTarget(Init_params.m_pKeenGraphicsSystem, m_pClbTarget);
    m_pClbTarget = nullptr;
    keen::graphics::freeTextureData(m_pClbTexture, Init_params.m_pKeenGraphicsSystem, g_pFramebufferAllocator);
    delete m_pClbTexture;
    m_pClbTexture = nullptr;
    if (m_hdr_compositor_p != nullptr)
    {
        m_hdr_compositor_p->release_resources();
    }
    if (m_outline_compositor_p != nullptr)
    {
        m_outline_compositor_p->release_resources();
    }
    keen::graphics::destroyCommandBuffer(Init_params.m_pKeenGraphicsSystem, m_pRecordedCommandBuffer);
    m_pRecordedCommandBuffer = nullptr;
    if (m_locked_framebuffer_texture.pData != nullptr)
    {
        keen::graphics::freeTextureData(m_locked_framebuffer_texture.pData, Init_params.m_pKeenGraphicsSystem, g_pFramebufferAllocator);
        delete m_locked_framebuffer_texture.pData;
        m_locked_framebuffer_texture.pData = nullptr;
    }
}

// 0x00550CD0 (:1480)
// The new level of detail takes effect (also in the HDR compositor), and the targets are created again.
void rl_scene_renderer::renew_resources(keen::GraphicsCommandWriter* pCommandWriter)
{
    m_level_of_detail = m_new_level_of_detail;
    m_hdr_compositor_p->m_level_of_detail = m_level_of_detail;
    create_render_textures(pCommandWriter);
    if (m_hdr_compositor_p != nullptr)
    {
        m_hdr_compositor_p->create_render_textures(pCommandWriter);
    }
    if (m_outline_compositor_p != nullptr)
    {
        m_outline_compositor_p->renew_resources();
    }
}

// A texture of the framebuffer allocator (keen::graphics::allocateTextureData of a new TextureData, inlined).
static keen::TextureData* create_framebuffer_texture(const keen::TextureDescription& description, const char* pDebugName)
{
    keen::TextureData* pTexture = new keen::TextureData();
    keen::graphics::allocateTextureData(pTexture, Init_params.m_pKeenGraphicsSystem, g_pFramebufferAllocator, description, nullptr, 0, pDebugName);
    return pTexture;
}

// A 2D render target texture description (one level, GPU access, render target flags 3).
static keen::TextureDescription render_target_description(unsigned int width, unsigned int height, uint8 format)
{
    keen::TextureDescription description;
    description.width = (uint16)width;
    description.height = (uint16)height;
    description.depth = 0;
    description.flags = 3;
    description.type = 1;
    description.format = format;
    description.lutFormat = 0;
    description.multiSampleType = 0;
    description.levelCount = 1;
    description.cpuAccessMode = 0;
    description.gpuAccessMode = 1;
    description.addressModeU = 0;
    description.addressModeV = 0;
    description.addressModeW = 0;
    return description;
}

// 0x00544CA0 (:1496)
// The main scene depth stencil and HDR target, the depth copy, the particle and half resolution targets, the MSAA depth
// stencil, the bloom targets, the 1x1 particle fog color target (cleared through the command writer), the distortion
// and temp targets, the CLB and shadow map targets, the SSAO blur targets and the recorded command buffer.
void rl_scene_renderer::create_render_textures(keen::GraphicsCommandWriter* pCommandWriter)
{
    keen::debug::nativePrintArgs("Creating main scene rendertargets %dx%d\n", m_main_scene_target_width, m_main_scene_target_height);
    rl_render_lib* render_lib = rl_render_lib::get_instance();
    // Magic numbers: the depth stencil creation flags (3: shader readable, 0: not).
    m_main_scene_depth_stencil_target = render_lib->create_depth_stencil_target(m_main_scene_target_width, m_main_scene_target_height, 0, "mainSceneDS", 3);
    m_main_scene_render_target.init(m_main_scene_target_width, m_main_scene_target_height, RL_FORMAT_HDR, m_level_of_detail.m_aa_lod, "mainScene", 1);
    // The main depth stencil buffer, also the CLB target's depth (the projector's stencil layers test the scene depth).
    const keen::RenderTargetBuffer main_depth_buffer = m_main_scene_depth_stencil_target.pData->depthBuffer;
    const rl_dev_depth_stencil_target depth_copy = render_lib->create_depth_stencil_target(m_main_scene_target_width, m_main_scene_target_height, 0, "copyMainSceneDS", 0);
    m_pMainSceneDepthTarget = depth_copy.pData;
    m_pMainSceneDepthTexture = keen::graphics::getRenderTargetDepthBuffer(m_pMainSceneDepthTarget);
    m_full_res_particle_target.init(m_main_scene_target_width, m_main_scene_target_height, RL_FORMAT_BLEND_LDR, 0, "fullResParticles", 1);
    m_half_render_target.init(m_half_target_width, m_half_target_height, RL_FORMAT_BLEND_LDR, 0, "HalfRenderTarget", 0);
    m_half_depth_target.init(m_half_target_width, m_half_target_height, RL_FORMAT_DEPTH_BLEND_FLOAT, 0, "HalfDepthTarget", 0);
    m_half_depth_stencil_target = render_lib->create_depth_stencil_target(m_half_target_width, m_half_target_height, 0, "halfDepthDS", 0);
    if (m_level_of_detail.m_aa_lod != RL_AA_LOD_OFF)
    {
        m_main_scene_depth_stencil_target_msaa = render_lib->create_depth_stencil_target(m_main_scene_target_width, m_main_scene_target_height, m_level_of_detail.m_aa_lod, "DepthStencil", 0);
    }
    if (m_level_of_detail.m_bloom_enabled)
    {
        m_particle_brightpass_target.init(m_particle_brightpass_width, m_particle_brightpass_height, RL_FORMAT_HDR, 0, "BrightPassTarget", 0);
        m_outline_target.init(m_outline_width, m_outline_height, RL_FORMAT_LDR, 0, "OutlineTarget", 0);
    }
    // Magic number: 0x2A, the 1x1 fog color texture's pixel format.
    // The 1x1 fog color target: a dynamic texture over a framebuffer texture, its render target, cleared to black once
    // (rl_render_texture::init's steps for an external texture, and a command writer render pass, inlined).
    rl_render_texture& fog_target = m_particle_fog_color_target;
    fog_target.m_flags = 0;
    if (fog_target.m_dyn_texture_p == nullptr)
    {
        fog_target.m_dyn_texture_p = rl_dyn_texture::create();
    }
    // Magic number: 0x2A, the fog color texture's pixel format.
    fog_target.m_dyn_texture_p->m_dev_texture.pData = create_framebuffer_texture(render_target_description(1, 1, 0x2A), "ParticleFogColorTarget");
    keen::RenderTargetBuffer fog_buffer;
    fog_buffer.pDataBuffer = fog_target.m_dyn_texture_p->get_texture_data().pData;
    fog_buffer.format = (keen::PixelFormat)fog_buffer.pDataBuffer->description.format;
    // Magic numbers: pixel formats 0x2D and 0x37 are the depth formats (a depth buffer instead of a color buffer).
    const bool fog_is_depth = fog_buffer.format == 0x2D || fog_buffer.format == 0x37;
    fog_target.m_render_target.pData = keen::graphics::createRenderTarget(Init_params.m_pKeenGraphicsSystem, fog_buffer.pDataBuffer->description.width, fog_buffer.pDataBuffer->description.height, fog_is_depth ? nullptr : &fog_buffer, fog_is_depth ? 0 : 1, fog_is_depth ? &fog_buffer : nullptr, keen::TilingMode_None);
    // Magic number: clear mask 2, color.
    const keen::ClearSettings fog_clear(0, 1.0f, 0, 2);
    pCommandWriter->writeBeginRenderPassCommand(fog_target.get_dev_render_target().pData, (keen::RenderTargetPartFlagMask)0, &fog_clear);
    pCommandWriter->writeEndRenderPassCommand((keen::RenderTargetPartFlagMask)0);
    if (m_level_of_detail.m_distortion_enabled)
    {
        m_distortion_target.init(m_distortion_width, m_distortion_height, RL_FORMAT_R8G8B8A8_UNORM, 0, "DistortionTarget", 0);
    }
    m_main_scene_temp_target[0].init(m_main_scene_target_width, m_main_scene_target_height, RL_FORMAT_LDR, 0, "MainSceneTempTarget[0]", 0);
    if (m_level_of_detail.m_distortion_enabled || m_level_of_detail.m_motion_blur_enabled || m_level_of_detail.m_depth_of_field_enabled)
    {
        m_main_scene_temp_target[1].init(m_main_scene_target_width, m_main_scene_target_height, RL_FORMAT_LDR, 0, "MainSceneTempTarget[1]", 0);
        m_main_scene_downsample_target.init(m_main_scene_downsample_width, m_main_scene_downsample_height, RL_FORMAT_LDR, 0, "MainSceneDownsampleTarget", 0);
        m_main_scene_blur_target.init(m_main_scene_downsample_width, m_main_scene_downsample_height, RL_FORMAT_LDR, 0, "MainSceneBlurTarget", 0);
    }
    // Magic number: 0x17, the CLB texture's pixel format.
    m_pClbTexture = create_framebuffer_texture(render_target_description(m_clb_width, m_clb_height, 0x17), "ClbTexture");
    keen::RenderTargetBuffer clb_buffer;
    clb_buffer.pDataBuffer = m_pClbTexture;
    clb_buffer.format = keen::PixelFormat_None;
    m_pClbTarget = keen::graphics::createRenderTarget(Init_params.m_pKeenGraphicsSystem, m_clb_width, m_clb_height, &clb_buffer, 1, &main_depth_buffer, keen::TilingMode_None);
    const rl_shadow_lod shadow_lod = m_level_of_detail.m_shadow_lod;
    m_shadow_map_width = shadow_map_resolutions[shadow_lod];
    m_shadow_map_height = shadow_map_resolutions[shadow_lod];
    if (shadow_lod != RL_SHADOW_LOD_OFF)
    {
        m_shadow_map_target.init(m_shadow_map_width, m_shadow_map_height, RL_FORMAT_SHADOW_MAP, 0, "shadowMap", 2);
    }
    // Magic number: 0x27, the SSAO blur textures' pixel format.
    m_pSsaoBlurTexture[0] = create_framebuffer_texture(render_target_description(m_ssao_width, m_ssao_height, 0x27), "SSaoBlurTarget0");
    m_pSsaoBlurTexture[1] = create_framebuffer_texture(render_target_description(m_ssao_width, m_ssao_height, 0x27), "SSaoBlurTarget1");
    for (int i = 0; i < 2; i++)
    {
        keen::RenderTargetBuffer buffer;
        buffer.pDataBuffer = m_pSsaoBlurTexture[i];
        buffer.format = keen::PixelFormat_None;
        m_pSsaoBlurTarget[i] = keen::graphics::createRenderTarget(Init_params.m_pKeenGraphicsSystem, m_ssao_width, m_ssao_height, &buffer, 1, nullptr, keen::TilingMode_None);
    }
    m_pRecordedCommandBuffer = keen::graphics::createCommandBuffer(Init_params.m_pKeenGraphicsSystem, m_commandBufferMemory);
}

// 0x004F5E10 (:1612)
// The target sizes: half and quarter resolution targets, full or half resolution distortion by particle LOD; the
// compositor's 4x4 downscale offsets and the HDR and outline compositors follow.
void rl_scene_renderer::resize(int w, int h, bool highQuality)
{
    const int quarter_w = w >> 2;
    const int quarter_h = h >> 2;
    m_main_scene_target_width = w;
    m_main_scene_target_height = h;
    m_half_target_width = w >> 1;
    m_half_target_height = h >> 1;
    m_particle_brightpass_width = quarter_w;
    m_particle_brightpass_height = quarter_h;
    if ((int)m_level_of_detail.m_particle_lod < 2)
    {
        m_distortion_width = w >> 1;
        m_distortion_height = h >> 1;
    }
    else
    {
        m_distortion_width = w;
        m_distortion_height = h;
    }
    m_main_scene_downsample_width = quarter_w;
    m_main_scene_downsample_height = quarter_h;
    m_outline_width = quarter_w;
    m_outline_height = quarter_h;
    m_ssao_width = w;
    m_ssao_height = h;
    m_clb_width = w;
    m_clb_height = h;
    m_sun_shafts_skybox_mask_width = w;
    m_sun_shafts_skybox_mask_height = h;
    if (m_compositor_p != nullptr)
    {
        // rl_downscale_sampling_offsets<16>::init, inlined.
        m_compositor_p->m_downscale_sampling_fullscreen.init(w, h);
    }
    if (m_hdr_compositor_p != nullptr)
    {
        m_hdr_compositor_p->resize(w, h);
    }
    if (m_outline_compositor_p != nullptr)
    {
        m_outline_compositor_p->m_mask_render_target_width = quarter_w;
        m_outline_compositor_p->m_mask_render_target_height = quarter_h;
    }
}

// 0x005455A0 (:1711)
// The base pass visibility depth only (no shadows), after a clear (color only when clear_color is given... all
// buffers; depth and stencil with black otherwise).
void rl_scene_renderer::render_depth_pre_pass(rl_renderer& renderer, rl_camera& camera, rl_scene& scene, const bool depth_to_color, rl_color_float* clear_color, const bool msaa_enabled)
{
    rl_shadow_render_parameters shadow_params;
    shadow_params.shadow_texture_matrix = IDENTITY_MATRIX44;
    shadow_params.shadow_max_dist = 1000.0f;
    shadow_params.shadow_fade_start = 1.0f;
    shadow_params.shadow_aa_enabled = 0;
    shadow_params.shadow_percent = 1.0f;
    shadow_params.shadow_percent_bias = 0.0f;
    shadow_params.shadows_enabled = 0;
    shadow_params.use_clb_shadows = 0;
    rl_color_float black(0.0f, 0.0f, 0.0f, 0.0f);
    unsigned int clear_flags;
    if (clear_color == nullptr)
    {
        // Magic number: clear flags 6, depth and stencil.
        clear_flags = 6;
        clear_color = &black;
    }
    else
    {
        clear_flags = 0xFFFFFFFF;
    }
    renderer.clear(*clear_color, clear_flags, 1.0f, 0);
    renderer.set_depth_only(true);
    rl_light* light = scene.m_largest_light_p != nullptr ? scene.m_largest_light_p : scene.m_largest_light_default_p;
    const rl_prim_color_write_mode color_write = (rl_prim_color_write_mode)((unsigned int)depth_to_color * 4 + RLPCW_NONE);
    render_base_pass(renderer, camera, *light, shadow_params, m_terrain_renderer_p, *m_base_batched_pass, 1u << (scene.m_visibility_manager_p->m_base_pass_vis_index & 0x1F), true, false, color_write, msaa_enabled, false, RL_TERRAIN_ALL);
    renderer.set_depth_only(false);
}

// 0x005456D0 (:1735)
// The x-ray effect (only when enabled): the outline polygon written to the stencil, the terrain (when it occludes)
// depth only, then the x-ray pass instances faded by distance.
void rl_scene_renderer::render_xray_effect(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_visibility_manager& visibility_manager, rl_shadow_render_parameters& shadow_render_params, rl_iterrain_renderer& terrain_renderer, const rl_xray_render_state& xray_state, const rl_dev_render_target& target_rt, const rl_dev_depth_stencil_target& target_ds, rl_batched_pass& batched_pass, pool_list<rl_renderable_instance*>& renderable_list)
{
    if (!xray_state.m_xray_effect_enabled)
    {
        return;
    }
    keen::debug::nativePrintArgs("rl_scene_renderer::render_xray_effect is not ported (x-ray)\n");
}

// 0x0052E3C0 (:1838)
// Reached only from render_xray_effect.
void rl_scene_renderer::render_xray_pass(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_shadow_render_parameters& shadow_render_params, rl_iterrain_renderer* terrain_renderer_p, rl_batched_pass* batched_pass_p, pool_list<rl_renderable_instance*>* renderable_list_p, unsigned int vis_pass_flags)
{
    keen::debug::nativePrintArgs("rl_scene_renderer::render_xray_pass is not ported (x-ray)\n");
}

// 0x004F6010 (:1917)
// The shadow fade constants and the shadow texture (CLB target in slot 15, or the shadow map in slot 14) when shadows
// are enabled, and the CLB target in slot 13 always.
void rl_scene_renderer::set_shadow_states(rl_shadow_render_parameters& shadow_render_params, rl_renderer& renderer)
{
    if (shadow_render_params.shadows_enabled != 0)
    {
        const float max_dist = shadow_render_params.shadow_max_dist;
        const float fade_start = shadow_render_params.shadow_fade_start * max_dist;
        // Magic number: 0.008, the shadow fade w constant.
        renderer.set_shadow_fade_params(vector4(fade_start, max_dist, 1.0f / (max_dist - fade_start), 0.008f));
        unsigned int width;
        unsigned int height;
        if (shadow_render_params.use_clb_shadows == 0)
        {
            renderer.set_texture(14, m_shadow_map_target.get_texture_handle());
            width = m_shadow_map_width;
            height = m_shadow_map_height;
        }
        else
        {
            renderer.m_pCommandWriter->writeSetFragmentShaderTextureCommand(15, m_pClbTexture);
            width = m_main_scene_target_width;
            height = m_main_scene_target_height;
        }
        renderer.set_target_dimensions(width, height);
    }
    renderer.m_pCommandWriter->writeSetFragmentShaderTextureCommand(13, m_pClbTexture);
}

// 0x004C7750 (:1969)
// Takes effect with renew_resources.
void rl_scene_renderer::set_level_of_detail(const rl_level_of_detail& lod)
{
    m_new_level_of_detail = lod;
}

// 0x004BCE30 (.h:102)
bool rl_scene_renderer::get_visibility_locked() const
{
    return false;
}

// 0x004BCE40 (.h:129)
rl_batched_pass* rl_scene_renderer::get_scene_batch()
{
    return m_base_batched_pass;
}

// 0x004BCE50 (.h:155)
int rl_scene_renderer::get_mainscene_width() const
{
    return m_main_scene_target_width;
}

// 0x004BCE60 (.h:156)
int rl_scene_renderer::get_mainscene_height() const
{
    return m_main_scene_target_height;
}

// 0x007C0210 (.h:374)
void rl_scene_renderer::set_visible_renderable_instances_max(int visible_renderable_instances_max)
{
    ms_visible_renderable_instances_max = visible_renderable_instances_max;
}

// 0x004BCE70 (.h:394)
rl_scene* rl_scene_renderer::get_scene() const
{
    return m_scene_p;
}

// 0x004BCE80 (.h:399)
void rl_scene_renderer::set_scene(rl_scene* scene_p)
{
    m_scene_p = scene_p;
}

// 0x004BCE90 (.h:404)
rl_iterrain_renderer* rl_scene_renderer::get_terrain_renderer() const
{
    return m_terrain_renderer_p;
}

// 0x007CD390 (.h:414)
// Unlocking clears both lock flags and frees the copy.
void rl_scene_renderer::lock_framebuffer(bool lock)
{
    if (lock)
    {
        m_flags |= kFramebufferLockInitiated;
    }
    else
    {
        m_flags &= ~(kFramebufferLockInitiated | kFramebufferLockCompleted);
    }
    if (!lock && m_locked_framebuffer_texture.pData != nullptr)
    {
        rl_render_lib::get_instance()->destroy_render_texture(m_locked_framebuffer_texture);
        m_locked_framebuffer_texture.pData = nullptr;
    }
}

// 0x008AFE20 (.h:427)
bool rl_scene_renderer::get_framebuffer_is_locked()
{
    return (m_flags & kFramebufferLockInitiated) != 0;
}

// 0x004BCEA0 (.h:432)
bool rl_scene_renderer::get_framebuffer_lock_completed()
{
    return (m_flags >> 1 & 1) != 0;
}

// 0x004BCEB0 (.h:437)
bool rl_scene_renderer::get_framebuffer_lock_initiated()
{
    return (m_flags & kFramebufferLockInitiated) != 0;
}

// 0x0050AD90 (.h:442)
// Copies the first temp target into the locked framebuffer texture (created on the first lock).
void rl_scene_renderer::complete_lock_frame_buffer(rl_renderer& renderer)
{
    m_flags |= kFramebufferLockCompleted;
    if (m_locked_framebuffer_texture.pData == nullptr)
    {
        // Magic number: 0x28, the copy's pixel format.
        keen::TextureData* pTexture = new keen::TextureData();
        const keen::TextureDescription description = render_target_description(m_main_scene_target_width, m_main_scene_target_height, 0x28);
        keen::graphics::allocateTextureData(pTexture, Init_params.m_pKeenGraphicsSystem, g_pFramebufferAllocator, description, nullptr, 0, "RenderTarget");
        m_locked_framebuffer_texture.pData = pTexture;
    }
    else
    {
        keen::debug::nativePrintArgs("We should never complete a framebuffer lock twice before unlocking.\n");
    }
    renderer.copy_render_texture(m_main_scene_temp_target[0], m_locked_framebuffer_texture);
}

// 0x00893140 (.h:459)
void rl_scene_renderer::set_framebuffer_resolve_enabled(bool enable)
{
    if (enable)
    {
        m_flags |= kFramebufferResolveEnabled;
        return;
    }
    m_flags &= ~kFramebufferResolveEnabled;
}

// 0x004BCEC0 (.h:464)
bool rl_scene_renderer::get_framebuffer_resolve_enabled()
{
    return (m_flags >> 2 & 1) != 0;
}

// 0x00893160 (.h:469)
int rl_scene_renderer::get_framebuffer_resolve_texture()
{
    if ((m_flags >> 2 & 1) != 0)
    {
        return m_main_scene_temp_target[0].get_texture_handle();
    }
    return -1;
}

// 0x004BCED0 (.h:478)
// Farther first; equal distances (within 0.0001) by higher sort priority first.
bool rl_scene_renderer::deferred_sort_entry::btf_predicate(const deferred_sort_entry& lhs, const deferred_sort_entry& rhs)
{
    const float diff = lhs.dist_to_camera_sq - rhs.dist_to_camera_sq;
    // Magic number: 0.0001, the equal distance tolerance.
    if (diff < 0.0001f && -diff < 0.0001f)
    {
        return rhs.sort_priority < lhs.sort_priority;
    }
    return rhs.dist_to_camera_sq < lhs.dist_to_camera_sq;
}
