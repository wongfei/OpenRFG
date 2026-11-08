// Original: volition/render_lib/code/rl_scene_shadows.cpp
//
// The shadow passes of rl_scene_renderer: clb_render clears the CLB (cascaded light buffer) target to white and, with
// a shadow LOD, renders the near/mid/far (and extended terrain) shadow maps and projects them into the CLB target,
// with the cloud shadows on top.
//
// Partial: the drop shadow pass (g_dropShadowEnabled, off in retail) is not ported.

#include "volition/render_lib/code/rl_scene_renderer.h"

#include "volition/render_lib/code/rl_batched_pass.h"
#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_clb_projector.h"
#include "volition/render_lib/code/rl_dyn_texture.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_render_texture.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_shadow_system.h"
#include "volition/render_lib/code/rl_terrain_renderer.h"
#include "volition/render_lib/code/rl_visibility_manager.h"
#include "volition/vlib/code/os/scheduler.h"

#include "library/modules/runtime/graphics/sources/d3d11/graphicscommandbuffer_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp"
#include "keen/graphics/graphicscommandwriter.hpp"

#include <math.h>

extern event_trigger* Morph_process_finished_trigger; // 0x01B7BCE8 (rl_deformable_mesh_instance.cpp)

// The shadow map projection (the Rl_shadow_map_type console command sets it; uniform in retail).
rl_shadow_map_type Rl_shadow_map_type = SMT_UNIFORM; // 0x01B7BD24

// Cloud shadow projection (on in retail).
bool g_cloudShadowEnabled = true; // 0x014FA47E
float g_cloudShadowScale = 0.5f; // 0x014FA480
float g_cloudShadowIntensityScale = 4.5f; // 0x014FA484
float g_cloudShadowIntensityBias = -0.5f; // 0x014FA488
// The terrain shadow distance of the projection-only pass.
float g_terrainShadowsMaxDist = 240.0f; // 0x014FA550

float wrapMax(float x, float max); // 0x004C79C0 (:247)
float wrapMinMax(float x, float min, float max); // 0x004C7A00 (:252)
void buildShadowParamsProjectionTexture(rl_shadow_render_parameters* pParams, rl_camera* camera, keen::TextureData* pTexture, float scrollX, float scrollY); // 0x004C7A60 (:303)

// 0x00517D60 (:78)
// One shadow map: depth only into depth_target (cleared to 1, the viewport z range 0..1), with the light's shadow
// map matrix as the view and an identity projection, the shadow macro state
// and the terrain, the batched casters and the transparent casters of the visibility pass. The renderer's camera
// transforms, viewport, z range and macro state are restored afterwards.
// Notes (decompiled behaviour, kept as is): the pixel and vertex shader resources are unbound directly on the D3D
// context (bypassing the command writer's cache), and the base pass flag is left as base_pass.
void rl_scene_renderer::render_shadow_pass(rl_renderer& renderer, rl_camera& camera, rl_light& light, const rl_dev_render_target& depth_target, int width, int height, rl_shadow_creation_parameters& shadow_creation_params, rl_batched_pass& batch_to_use, const unsigned int vis_flags, rl_renderable_instance** transparent_casters, unsigned int num_transparent_casters, const bool base_pass)
{
    // Magic number: 16, every pixel and vertex shader resource slot.
    ID3D11ShaderResourceView* null_views[16] = {};
    ID3D11DeviceContext* context_p = renderer.m_pCommandWriter->getBuffer()->pContext;
    context_p->PSSetShaderResources(0, 16, null_views);
    context_p->VSSetShaderResources(0, 16, null_views);
    const rl_rect saved_vp = renderer.m_viewPort;
    renderer.set_is_base_pass(base_pass);
    renderer.set_depth_only(true);
    renderer.set_world_transform(IDENTITY_MATRIX43);
    matrix44 saved_world_view;
    matrix44 saved_projection;
    renderer.get_3d_transforms(saved_world_view, saved_projection);
    rl_dev_depth_stencil_target no_ds;
    no_ds.pData = nullptr;
    renderer.begin_pass(depth_target, no_ds);
    rl_rect shadow_vp;
    shadow_vp.m_left = 0;
    shadow_vp.m_top = 0;
    shadow_vp.m_right = width;
    shadow_vp.m_bottom = height;
    renderer.set_viewport(shadow_vp);
    const float saved_z_near = renderer.m_z_near;
    const float saved_z_far = renderer.m_z_far;
    renderer.m_z_near = 0.0f;
    renderer.m_z_far = 1.0f;
    renderer.set_viewport(renderer.m_viewPort);
    // Magic number: clear flags 3 (color and depth).
    renderer.clear(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f), 3, 1.0f, 0);
    const rl_primitive_state saved_state = renderer.get_macro_state();
    rl_primitive_state state = saved_state;
    // (bits & 0xFFACA120) | 0x20A100: no blending, full depth, no stencil, culling, no color writes, the shadow bias.
    state.set_alpha_mode(RLPAM_NONE);
    state.set_zbuf_mode(RLPZM_FULL);
    state.set_stencil_mode(RLPSM_NONE);
    state.set_cull_mode(RLPCM_CULL);
    state.set_color_write_mode(RLPCW_NONE);
    state.set_zbias_mode(RLPZB_SHADOW);
    renderer.set_macro_state(state, false);
    renderer.set_3d_transforms(shadow_creation_params.shadow_map_matrix, IDENTITY_MATRIX44);
    renderer.set_world_transform(IDENTITY_MATRIX43);
    if (m_terrain_renderer_p != nullptr)
    {
        m_terrain_renderer_p->render_depth(renderer, camera, &light, vis_flags, true);
    }
    batch_to_use.render(renderer, camera, vis_flags, true);
    if (num_transparent_casters != 0)
    {
        for (unsigned int i = 0; i < num_transparent_casters; i++)
        {
            rl_renderable_instance* caster_p = transparent_casters[i];
            if ((caster_p->get_vis_pass_flags() & vis_flags) != 0)
            {
                // Magic number: render flags 3 (no decals).
                caster_p->render(renderer, camera, 3);
            }
        }
        renderer.set_world_transform(IDENTITY_MATRIX43);
    }
    renderer.set_world_transform(IDENTITY_MATRIX43);
    rl_material_library_clear_render_material(renderer);
    renderer.set_macro_state(saved_state, false);
    renderer.set_3d_transforms(saved_world_view, saved_projection);
    renderer.set_depth_only(false);
    renderer.m_z_near = saved_z_near;
    renderer.m_z_far = saved_z_far;
    renderer.set_viewport(renderer.m_viewPort);
    renderer.set_viewport(saved_vp);
    renderer.end_pass();
}

// The projector settings of a shadow range: the render offset, the z range and the range's fade parameters.
static void clb_settings_for_range(ClbProjectorRenderSettings& settings, rl_renderer& renderer, rl_camera& camera, rl_shadow_map_range shadow_map_range)
{
    settings.renderOffset = renderer.get_render_offset();
    renderer.get_viewport_z_range(settings.minZ, settings.maxZ);
    rl_shadow_system::get_instance()->get_shadow_range_fade_params(settings.shadowRangeFadeParams, shadow_map_range, &camera);
}

// 0x0052E7B0 (:166)
// A shadow range: its shadow map, then projected into the CLB target.
void rl_scene_renderer::render_clb_pass(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_shadow_creation_parameters& creation_params, rl_shadow_render_parameters& render_params, rl_batched_pass& batch_to_use, unsigned int pass_vis_flags, rl_shadow_map_range shadow_map_range, rl_renderable_instance** transparent_casters, int num_transparent_casters, rl_shadow_map_pass shadow_map_pass)
{
    if (m_shadow_map_target.m_msaa_texture.pData != nullptr)
    {
        m_shadow_map_target.m_flags |= rl_render_texture::kRequiresResolve;
    }
    render_shadow_pass(renderer, camera, light, m_shadow_map_target.m_render_target, m_shadow_map_width, m_shadow_map_height, creation_params, batch_to_use, pass_vis_flags, transparent_casters, num_transparent_casters, true);
    const rl_primitive_state saved_state = renderer.get_macro_state();
    ClbProjectorRenderSettings settings;
    clb_settings_for_range(settings, renderer, camera, shadow_map_range);
    rl_clb_projector_system_render_DepthTexture(renderer, settings, m_pClbTarget, camera, m_pMainSceneDepthTexture, m_shadow_map_target.m_dyn_texture_p->get_texture_data().pData, render_params, shadow_map_pass, shadow_map_range);
    renderer.set_macro_state(saved_state, false);
}

// 0x0052E8D0 (:195)
// A shadow range with a projection texture (the cloud shadows) applied in the same projector pass.
void rl_scene_renderer::render_clb_pass(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_shadow_creation_parameters& creation_params, rl_shadow_render_parameters& depthMapRenderParams, keen::TextureData* pProjectionTexture, rl_shadow_render_parameters* pProjectionTextureRenderParams, rl_batched_pass& batch_to_use, unsigned int pass_vis_flags, rl_shadow_map_range shadow_map_range, rl_renderable_instance** transparent_casters, int num_transparent_casters, rl_shadow_map_pass shadow_map_pass)
{
    if (m_shadow_map_target.m_msaa_texture.pData != nullptr)
    {
        m_shadow_map_target.m_flags |= rl_render_texture::kRequiresResolve;
    }
    render_shadow_pass(renderer, camera, light, m_shadow_map_target.m_render_target, m_shadow_map_width, m_shadow_map_height, creation_params, batch_to_use, pass_vis_flags, transparent_casters, num_transparent_casters, true);
    const rl_primitive_state saved_state = renderer.get_macro_state();
    ClbProjectorRenderSettings settings;
    clb_settings_for_range(settings, renderer, camera, shadow_map_range);
    rl_clb_projector_system_render_DepthAndProjectionTexture(renderer, settings, m_pClbTarget, camera, m_pMainSceneDepthTexture, m_shadow_map_target.m_dyn_texture_p->get_texture_data().pData, depthMapRenderParams, pProjectionTexture, pProjectionTextureRenderParams, shadow_map_pass, shadow_map_range);
    renderer.set_macro_state(saved_state, false);
}

// 0x0052E9F0 (:224)
// The projection texture alone (the cloud shadows) past g_terrainShadowsMaxDist: it fades in over 6 m when the far
// clip is beyond the terrain shadow distance (at once otherwise) and never fades out.
void rl_scene_renderer::render_clb_pass(rl_renderer& renderer, rl_camera& camera, rl_shadow_render_parameters& render_params, keen::TextureData* pProjectionTexture)
{
    ClbProjectorRenderSettings settings;
    settings.renderOffset = renderer.get_render_offset();
    renderer.get_viewport_z_range(settings.minZ, settings.maxZ);
    const rl_primitive_state saved_state = renderer.get_macro_state();
    rl_shadow_system::get_instance();
    settings.shadowRangeFadeParams[1] = camera.get_far_clip();
    // Magic numbers: 1.1754944e-38 (FLT_MIN) and 6.0, the fade-in width.
    settings.shadowRangeFadeParams[2] = 1.1754944e-38f;
    if (g_terrainShadowsMaxDist < settings.shadowRangeFadeParams[1])
    {
        settings.shadowRangeFadeParams[2] = 6.0f;
    }
    settings.shadowRangeFadeParams[2] = 1.0f / settings.shadowRangeFadeParams[2];
    settings.shadowRangeFadeParams[0] = g_terrainShadowsMaxDist;
    settings.shadowRangeFadeParams[3] = 0.0f;
    rl_clb_projector_system_render_ProjectionTexture(renderer, settings, m_pClbTarget, camera, m_pMainSceneDepthTexture, pProjectionTexture, render_params);
    renderer.set_macro_state(saved_state, false);
}

// 0x004C79C0 (:247)
// x wrapped into [0, max).
float wrapMax(float x, float max)
{
    return (float)fmod(max + fmod(x, max), max);
}

// 0x004C7A00 (:252)
// x wrapped into [min, max).
float wrapMinMax(float x, float min, float max)
{
    return min + wrapMax(x - min, max - min);
}

// 0x00545CB0 (:297)
// Ends the morph processing job, then (CLB shadow mode) clears the CLB target to white; with a shadow LOD the near,
// mid and far shadow maps (and the extended terrain one) are rendered and projected, and the cloud shadows added.
void rl_scene_renderer::clb_render(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_visibility_manager& visibility_manager, rl_batched_pass& batch_to_use, rl_renderable_instance** transparent_casters, int num_transparent_casters)
{
    if (Morph_process_finished_trigger != nullptr)
    {
        scheduler_relinquish_cpu(Morph_process_finished_trigger, ccjt_d3d);
        Morph_process_finished_trigger = nullptr;
    }
    rl_rect saved_vp;
    renderer.get_viewport(saved_vp);
    const rl_primitive_state saved_state = renderer.get_macro_state();
    const bool cloud_shadows = g_cloudShadowEnabled && rl_shadow_system::m_pCloudTexture != nullptr;
    rl_shadow_render_parameters cloud_params;
    rl_shadow_render_parameters* cloud_params_p = nullptr;
    if (cloud_shadows)
    {
        buildShadowParamsProjectionTexture(&cloud_params, &camera, rl_shadow_system::m_pCloudTexture, rl_shadow_system::m_cloudUVScroll.x, rl_shadow_system::m_cloudUVScroll.y);
        cloud_params_p = &cloud_params;
    }
    if (rl_shadow_system::m_shadow_mode != RLSM_CLB)
    {
        return;
    }
    renderer.begin_pass(m_pClbTarget);
    // Magic numbers: white (0xFFFFFFFF), clear flags 5 (color and stencil).
    renderer.clear(rl_color(0xFFFFFFFF), 5, 1.0f, 0);
    renderer.end_pass();
    if (m_level_of_detail.m_shadow_lod != RL_SHADOW_LOD_OFF)
    {
        static const rl_shadow_map_pass passes[3] = { SMP_NEAR, SMP_MID, SMP_FAR };
        for (int i = 0; i < 3; i++)
        {
            const rl_shadow_map_pass pass = passes[i];
            unsigned int pass_vis_flags = 0;
            if (pass == SMP_NEAR)
            {
                pass_vis_flags = 1u << (visibility_manager.m_clb_base_vis_index & 0x1F);
            }
            else if (pass == SMP_MID)
            {
                pass_vis_flags = 1u << (visibility_manager.m_clb_base_vis_index + 1 & 0x1F);
            }
            else if (pass == SMP_FAR)
            {
                pass_vis_flags = 1u << (visibility_manager.m_clb_base_vis_index + 2 & 0x1F);
            }
            rl_shadow_creation_parameters creation_params;
            creation_params.shadow_map_matrix = IDENTITY_MATRIX44;
            creation_params.shadow_zbias = 0.0f;
            creation_params.shadow_slope_zbias = 0.0f;
            rl_shadow_render_parameters render_params;
            rl_aabb casters_aabb;
            rl_shadow_system::get_instance()->build_shadow_params_base(renderer, creation_params, render_params, Rl_shadow_map_type, &camera, &light, renderer.get_render_offset(), visibility_manager.m_shadow_min_y, visibility_manager.m_shadow_max_y, pass, nullptr, (rl_shadow_map_range)pass, &casters_aabb);
            render_clb_pass(renderer, camera, light, creation_params, render_params, rl_shadow_system::m_pCloudTexture, cloud_params_p, batch_to_use, pass_vis_flags, (rl_shadow_map_range)pass, transparent_casters, num_transparent_casters, pass);
        }
        if (rl_shadow_system::m_terrain_casts_shadows && rl_shadow_system::m_terrain_casts_shadows_extended)
        {
            rl_shadow_creation_parameters creation_params;
            creation_params.shadow_map_matrix = IDENTITY_MATRIX44;
            creation_params.shadow_zbias = 0.0f;
            creation_params.shadow_slope_zbias = 0.0f;
            rl_shadow_render_parameters render_params;
            rl_aabb casters_aabb;
            // Notes (decompiled behaviour, kept as is): the min and max y are passed swapped.
            rl_shadow_system::get_instance()->build_shadow_params_base(renderer, creation_params, render_params, Rl_shadow_map_type, &camera, &light, renderer.get_render_offset(), visibility_manager.m_shadow_max_y, visibility_manager.m_shadow_min_y, SMP_TERRAIN_EXTENDED, nullptr, SMR_TERRAIN_EXTENDED, &casters_aabb);
            render_clb_pass(renderer, camera, light, creation_params, render_params, rl_shadow_system::m_pCloudTexture, cloud_params_p, batch_to_use, 1u << (visibility_manager.m_clb_base_vis_index + 3 & 0x1F), SMR_TERRAIN_EXTENDED, transparent_casters, num_transparent_casters, SMP_TERRAIN_EXTENDED);
        }
        if (cloud_shadows)
        {
            render_clb_pass(renderer, camera, cloud_params, rl_shadow_system::m_pCloudTexture);
        }
    }
    renderer.set_macro_state(saved_state, false);
    renderer.set_viewport(saved_vp);
}

// 0x004C7A60 (:303)
// The cloud texture projected straight down: one texture tile covers (texture size / g_cloudShadowScale) meters, the
// projection is snapped to whole tiles around the camera and scrolled by the texture size times the scroll; the
// matrix is the downward view (make_uvec) times the orthographic scale (2 / tile size, depth 0.001). Intensity scale
// and bias from the globals.
// Notes (decompiled behaviour, kept as is): x scrolls by the width times scrollY and z by the height times -scrollX;
// the view position's y is 1 (the snapped position's y is never set).
void buildShadowParamsProjectionTexture(rl_shadow_render_parameters* pParams, rl_camera* camera, keen::TextureData* pTexture, float scrollX, float scrollY)
{
    const float width = (float)pTexture->description.width;
    const float height = (float)pTexture->description.height;
    const float tile_z = height / g_cloudShadowScale;
    const float tile_x = width / g_cloudShadowScale;
    const float scale_x = 2.0f / tile_x;
    const float scale_y = 2.0f / tile_z;
    // Magic number: the depth scale of the projection.
    const float scale_z = 0.001f;
    const vector& position = camera->get_position();
    const float center_x = (float)floor((double)(position.x / tile_x)) * tile_x + width * scrollY;
    const float center_z = (float)floor((double)(position.z / tile_z)) * tile_z - height * scrollX;
    const vector up(0.0f, 0.0f, 1.0f);
    const vector down(center_x - center_x, -1.0f, center_z - center_z);
    matrix view;
    view.make_uvec(down, up);
    const float tx = -(view.rvec.x * center_x + view.rvec.y + view.rvec.z * center_z);
    const float ty = -(view.uvec.x * center_x + view.uvec.y + view.uvec.z * center_z);
    const float tz = -(view.fvec.x * center_x + view.fvec.y + view.fvec.z * center_z);
    matrix44& m = pParams->shadow_texture_matrix;
    m.set(view.rvec.x * scale_x, view.uvec.x * scale_y, view.fvec.x * scale_z, 0.0f,
          view.rvec.y * scale_x, view.uvec.y * scale_y, view.fvec.y * scale_z, 0.0f,
          view.rvec.z * scale_x, view.uvec.z * scale_y, view.fvec.z * scale_z, 0.0f,
          tx * scale_x, ty * scale_y, tz * scale_z - 0.0f, 1.0f);
    pParams->shadow_percent = g_cloudShadowIntensityScale;
    pParams->shadows_enabled = 1;
    pParams->use_clb_shadows = 1;
    pParams->shadow_percent_bias = g_cloudShadowIntensityBias;
}
