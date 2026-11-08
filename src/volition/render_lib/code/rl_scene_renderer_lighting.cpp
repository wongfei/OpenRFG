// Original: volition/render_lib/code/rl_scene_renderer_lighting.cpp
//
// The secondary light passes of rl_scene_renderer: each visible light except the primary one draws the terrain and
// the batch of its own visibility pass additively (no shadows), nearest light first.

#include "volition/render_lib/code/rl_scene_renderer.h"

#include "volition/render_lib/code/rl_batched_pass.h"
#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_light.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_shadow_system.h"
#include "volition/render_lib/code/rl_terrain_renderer.h"

// The position the lights are sorted by distance to (the camera).
static vector Reference_pos; // 0x01C20A54

// 0x004C7770 (:9)
bool sort_lights_by_distance(rl_light* const& l1, rl_light* const& l2)
{
    const vector& p1 = l1->get_position();
    const vector& p2 = l2->get_position();
    const float dx1 = p1.x - Reference_pos.x;
    const float dy1 = p1.y - Reference_pos.y;
    const float dz1 = p1.z - Reference_pos.z;
    const float dx2 = p2.x - Reference_pos.x;
    const float dy2 = p2.y - Reference_pos.y;
    const float dz2 = p2.z - Reference_pos.z;
    return dz1 * dz1 + dy1 * dy1 + dx1 * dx1 <= dz2 * dz2 + dy2 * dy2 + dx2 * dx2;
}

// 0x0054DC00 (:19)
// Per light (sorted by distance to the camera): the shadow map cleared to white (when there is one), the light set as
// a secondary light, the additive light pass state (begin_lighting_pass, inlined), the terrain and the batch of the
// light's visibility pass (all passes when it has none), then the macro state back.
void rl_scene_renderer::render_lighting_passes(rl_renderer& renderer, rl_camera& camera, rl_light& primary_light, rl_batched_pass& batch_to_use, rl_renderable_instance** transparent_casters, unsigned int num_transparent_casters, const bool msaa_enabled)
{
    Reference_pos = camera.get_position();
    m_visible_lights.sort(sort_lights_by_distance);
    for (int i = 0; i < m_visible_lights.size(); i++)
    {
        rl_light* light = m_visible_lights[i];
        if (light == &primary_light)
        {
            continue;
        }
        rl_shadow_render_parameters shadow_params;
        const rl_dev_render_target& shadow_rt = m_shadow_map_target.get_dev_render_target();
        if (shadow_rt.pData != nullptr)
        {
            const rl_dev_depth_stencil_target no_ds;
            renderer.begin_pass(m_shadow_map_target.get_dev_render_target(), no_ds);
            // Magic number: clear flags 1, color.
            renderer.clear(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f), 1, 1.0f, 0);
            renderer.end_pass();
        }
        unsigned int vis_flags = 0xFFFFFFFF;
        if (light->m_vis_pass_index != 0xFFFF)
        {
            vis_flags = 1u << (light->m_vis_pass_index & 0x1F);
        }
        rl_iterrain_renderer* terrain_renderer_p = m_terrain_renderer_p;
        const rl_primitive_state saved_state = renderer.get_macro_state();
        renderer.set_active_camera(&camera);
        renderer.set_is_base_pass(false);
        renderer.set_light(*light, false);
        // Magic numbers: the packed light pass state (additive alpha, read depth, back face culling, RGBA writes and
        // msaa bit 19).
        rl_primitive_state light_state;
        light_state.set_bits((unsigned int)msaa_enabled << 19 | 0x2A081);
        renderer.set_macro_state(light_state, false);
        set_shadow_states(shadow_params, renderer);
        renderer.set_world_transform(IDENTITY_MATRIX43);
        camera.render_refresh(renderer);
        renderer.set_shadow_texture_transform(shadow_params.shadow_texture_matrix);
        if (terrain_renderer_p != nullptr)
        {
            terrain_renderer_p->render_lighting_pass(renderer, camera, light, vis_flags, false);
        }
        batch_to_use.render(renderer, camera, vis_flags, false);
        batch_to_use.render_decals(renderer);
        rl_material_library_clear_render_material(renderer);
        camera.render_refresh(renderer);
        renderer.set_macro_state(saved_state, false);
    }
}

// 0x00545C00 (:56)
// One light pass: the terrain and the batch of the passes (and its decals unless skip_decals).
void rl_scene_renderer::render_lighting_pass(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_shadow_render_parameters& shadow_render_params, rl_iterrain_renderer* terrain_renderer_p, rl_batched_pass& batched_pass, const unsigned int vis_flags, const bool skip_decals, const bool alpha_blend, const rl_prim_color_write_mode color_write_mode, const bool msaa_enabled)
{
    const rl_primitive_state saved_state = renderer.get_macro_state();
    begin_lighting_pass(renderer, camera, light, shadow_render_params, alpha_blend, color_write_mode, msaa_enabled);
    if (terrain_renderer_p != nullptr)
    {
        terrain_renderer_p->render_lighting_pass(renderer, camera, &light, vis_flags, skip_decals);
    }
    batched_pass.render(renderer, camera, vis_flags, skip_decals);
    if (!skip_decals)
    {
        batched_pass.render_decals(renderer);
    }
    rl_material_library_clear_render_material(renderer);
    camera.render_refresh(renderer);
    renderer.set_macro_state(saved_state, false);
}

// 0x0052E6E0 (:76)
// The light as a secondary light and the additive light pass state (alpha mode 1 or 2 with alpha_blend, read depth,
// back face culling), the shadow states and the shadow texture transform.
void rl_scene_renderer::begin_lighting_pass(rl_renderer& renderer, rl_camera& camera, rl_light& light, rl_shadow_render_parameters& shadow_render_params, const bool alpha_blend, const rl_prim_color_write_mode color_write_mode, const bool msaa_enabled)
{
    renderer.set_active_camera(&camera);
    renderer.set_is_base_pass(false);
    renderer.set_light(light, false);
    // Magic numbers: the packed state (color write bits 15-17, msaa bit 19, alpha mode alpha_blend + 1, read depth
    // 0x80, back face culling 0x2000).
    rl_primitive_state state;
    state.set_bits(((unsigned int)msaa_enabled << 4 | ((unsigned int)color_write_mode & RLPCW_GA)) << 15 | ((unsigned int)alpha_blend + 1) | 0x2080);
    renderer.set_macro_state(state, false);
    set_shadow_states(shadow_render_params, renderer);
    renderer.set_world_transform(IDENTITY_MATRIX43);
    camera.render_refresh(renderer);
    renderer.set_shadow_texture_transform(shadow_render_params.shadow_texture_matrix);
}

// 0x0052E790 (:103)
void rl_scene_renderer::end_lighting_pass(rl_camera& camera, rl_renderer& renderer)
{
    rl_material_library_clear_render_material(renderer);
    camera.render_refresh(renderer);
}
