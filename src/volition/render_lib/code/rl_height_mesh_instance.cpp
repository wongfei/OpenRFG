// Original: volition/render_lib/code/rl_height_mesh_instance.cpp
//
// rl_height_mesh_instance: one height mesh patch of a high LOD terrain subzone (a sub mesh of the subzone's mesh with
// the terrain material map). The terrain renderer draws the visible patches itself (rl_terrain_renderer::render_pass,
// heightMeshInstanceFillChunks); render() is the old per-instance path (g_terrainUseNewRenderer off). The patch
// vertices are relative to the subzone (m_target_position); the shaders get the subzone's 511 m grid cell as
// Terrain_subzone_offset.

#include "volition/render_lib/code/rl_height_mesh_instance.h"

#include "volition/render_lib/code/rl_decal.h"
#include "volition/render_lib/code/rl_decal_internal.h"
#include "volition/render_lib/code/rl_decal_parameters.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/render_lib/code/rl_metrics.h"
#include "volition/render_lib/code/rl_planar_decal.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_renderer_metrics.h"
#include "volition/vlib/code/memory/vmemory.h"

#include <math.h>

// <rl_height_mesh_instance::factory_init> 0x005101C0 (:26)
// <rl_height_mesh_instance::create> 0x0054F3A0 (:26)
RL_FACTORY_CLASS_CORE(rl_height_mesh_instance, 0x52DD6C14, 3000)

// Not in the binary: the terrain subzone grid cell of a position (inlined in every terrain draw): floor(p / 511 + 8)
// per axis. Magic numbers: 511 m subzone cells, offset by 8 cells.
vector rl_terrain_subzone_offset(const vector& position)
{
    return vector(floorf((float)(position.x / 511.0) + 8.0f), floorf((float)(position.y / 511.0) + 8.0f), floorf((float)(position.z / 511.0) + 8.0f));
}

// 0x0053E310 (:26)
// Deferred while render_lib is locked. Notes (decompiled behaviour, kept as is): the object goes back to the pool
// without checking that it came from the factory mempool.
void rl_height_mesh_instance::release()
{
    if (rl_is_locked())
    {
        rl_add_deferred_op(RLDO_RELEASE_OBJECT, this);
        return;
    }
    if (ms_object_pool.size() < 1 && ms_class_factory_info.get_display_exhausted_factories_warning())
    {
        rl_class_factory_info::decrement_num_exhausted_factories();
    }
    ms_object_pool.add(this);
    this->~rl_height_mesh_instance();
}

// 0x0054BA90 (:29)
// Not batchable.
rl_height_mesh_instance::rl_height_mesh_instance()
{
    set_visibility_list_index(0);
    m_sub_mesh_index = 0;
    m_target_position = ZERO_VECTOR;
    m_flags &= ~FLAGS_IS_BATCHABLE;
    m_material_map_p = nullptr;
}

// 0x0054BB00 (:39)
rl_height_mesh_instance::~rl_height_mesh_instance()
{
}

// 0x00525CB0 (:43)
// Material slots 0-2 are the LOD materials: LOD 0 uses slot 0, the others slot 1. At LOD 2 the material draws with
// its user technique 0.
void rl_height_mesh_instance::set_material(rl_renderer& renderer, int material_idx)
{
    const int lod_level = m_lod_level;
    if (material_idx < 3)
    {
        material_idx = lod_level > 0 ? 1 : lod_level;
    }
    const unsigned int mat_h = m_material_map_p->get_material(material_idx);
    rl_material* material_p = (rl_material*)rl_material_library_get_material(mat_h);
    if (material_p == nullptr)
    {
        return;
    }
    if (lod_level == 2)
    {
        material_p->set_override_technique(rl_shader_technique_user, 0);
    }
    rl_material_library_set_render_material(renderer, mat_h, 0);
    if (lod_level == 2)
    {
        material_p->set_override_technique(rl_shader_technique_none, -1);
    }
}

// 0x00525D50 (:79)
// Every render block of the sub mesh, at the subzone position.
void rl_height_mesh_instance::render_internal(rl_renderer& renderer, bool rendering_decal)
{
    const rl_mesh* mesh_p = (const rl_mesh*)get_renderable();
    const rl_submesh_data& submesh = mesh_p->get_mesh_data()->sub_meshes[m_sub_mesh_index];
    renderer.set_world_transform(matrix43(m_target_position, IDENTITY_MATRIX));
    renderer.set_terrain_subzone_offset(rl_terrain_subzone_offset(get_position()));
    const rl_render_block* blocks = submesh.render_blocks.to_pointer();
    for (int b = 0; b < submesh.num_render_blocks; b++)
    {
        set_material(renderer, blocks[b].material_map_idx);
        renderer.render(mesh_p, m_sub_mesh_index, b);
    }
}

// 0x00525F90 (:107)
// The patch with the terrain shaders, alpha mode 2 (5 in the base pass, 0 depth only), then its decals unless flags
// bit 1, at LOD 2 or depth only.
void rl_height_mesh_instance::render(rl_renderer& renderer, rl_camera&, unsigned int flags)
{
    renderer.set_shader_type(shader_type_terrain);
    renderer.set_shader_configuration(0);
    const bool depth_only = renderer.get_depth_only();
    renderer.set_tint_color(rl_color_float(m_tint.x, m_tint.y, m_tint.z, m_opacity));
    const rl_primitive_state saved_state = renderer.get_macro_state();
    // Magic numbers: the alpha mode (state bits 0-4).
    const unsigned int alpha_mode = depth_only ? 0 : (renderer.get_is_base_pass() ? 5 : 2);
    rl_primitive_state state;
    state.set_bits((saved_state.get_bits() & ~0x1Fu) | alpha_mode);
    renderer.set_macro_state(state, false);
    render_internal(renderer, false);
    renderer.set_macro_state(saved_state, false);
    if ((flags & 2) == 0 && m_lod_level != 2 && !depth_only)
    {
        rl_decal* decals[256];
        const int num_decals = build_sorted_decal_list(decals, 256);
        if (num_decals > 0)
        {
            render_decals(renderer, num_decals, decals);
        }
    }
}

// 0x005102C0 (:154)
void rl_height_mesh_instance::set_shader_types(rl_renderer& renderer)
{
    renderer.set_shader_type(shader_type_terrain);
    renderer.set_shader_configuration(0);
}

// 0x0053E390 (:162)
// A planar decal at the subzone position, registered under reserved_handle.
unsigned int rl_height_mesh_instance::add_decal(const rl_decal_parameters* decal_params, unsigned int reserved_handle)
{
    if (rl_decal_maybe_defer_creation(reserved_handle, decal_params, this))
    {
        return reserved_handle;
    }
    if (decal_params != nullptr)
    {
        if (decal_params->m_type == RLDT_PLANAR)
        {
            rl_decal* decal_p = rl_planar_decal_create(decal_params, m_target_position, IDENTITY_MATRIX);
            if (decal_p != nullptr)
            {
                append_decal(decal_p);
                decal_p->set_owner(this);
                return rl_decal_register(decal_p, reserved_handle);
            }
        }
        else
        {
            VLIB_CRITICAL_FAILURE();
        }
    }
    // Magic number: metrics warning 0x10, a decal could not be created.
    rl_render_lib::get_instance()->m_metrics_p->set_warning_flag(0x10);
    return 0xFFFFFFFF;
}

// 0x00526140 (:183)
// The decals of the patch (VFX.md FX7), not in a depth-only pass: the patch is drawn again per decal with the terrain
// shaders at the subzone position, scissored to the decal box's screen rectangle inside the current scissor region
// (rl_project_get_bbox_2d_rect_clipped_ex). A new decal material becomes the material library override, applied
// through the patch's own set_material (slot 0); the decal constants are set when the handle changes. The pass state
// (alpha mode 2, 5 in the base pass, state bit 18) is set once; double sided decals make no difference here.
void rl_height_mesh_instance::render_decals(rl_renderer& renderer, int num_decals, rl_decal** decal_list_p)
{
    if (renderer.get_depth_only())
    {
        return;
    }
    const rl_mesh* mesh_p = (const rl_mesh*)get_renderable();
    const rl_submesh_data& submesh = mesh_p->get_mesh_data()->sub_meshes[m_sub_mesh_index];
    const rl_primitive_state saved_state = renderer.get_macro_state();
    rl_rect saved_scissor;
    renderer.get_scissor_region(saved_scissor);
    rl_primitive_state state;
    // Magic numbers: alpha mode 2 (5 in the base pass, bits 0-4) and state bit 18 (meaning inferred).
    if (!renderer.get_is_base_pass())
    {
        state.set_bits(saved_state.get_bits() & 0xFFFFFFE2 | 0x40002);
    }
    else
    {
        state.set_bits(saved_state.get_bits() & 0xFFFFFFE5 | 0x40005);
    }
    const rl_screen_info* screen = rl_render_lib::get_instance()->get_screen_info();
    const int screen_width = screen->get_screen_width();
    const int screen_height = screen->get_screen_height();
    const rl_rect screen_rect(0, 0, screen_width, screen_height);
    const matrix44 clip_screen = rl_decal_scissor_clip_screen(screen_width, screen_height);
    renderer.set_macro_state(state, false);
    renderer.set_shader_type(shader_type_terrain);
    renderer.set_shader_configuration(0);
    const matrix43 world_transform(m_target_position, IDENTITY_MATRIX);
    renderer.set_world_transform(world_transform);
    renderer.set_terrain_subzone_offset(rl_terrain_subzone_offset(get_position()));
    unsigned int last_material = 0xFFFFFFFF;
    unsigned int last_handle = 0xFFFFFFFF;
    for (int i = 0; i < num_decals; i++)
    {
        rl_planar_decal* decal_p = (rl_planar_decal*)decal_list_p[i];
        vector box_min;
        vector box_max;
        rl_decal_world_box(box_min, box_max, world_transform, decal_p);
        rl_rect scissor;
        rl_project_get_bbox_2d_rect_clipped_ex(scissor, box_min, box_max, screen_rect, clip_screen, renderer);
        if (scissor.m_right - scissor.m_left <= 0 || scissor.m_bottom - scissor.m_top <= 0)
        {
            continue;
        }
        const unsigned int material = decal_p->get_material();
        const unsigned int handle = decal_p->get_decal_handle();
        if (material != last_material)
        {
            rl_material_library_set_override(material);
            set_material(renderer, 0);
            last_material = material;
        }
        if (handle != last_handle)
        {
            rl_decal_setup_decal_for_render(renderer, decal_p);
            last_handle = handle;
        }
        renderer.set_scissor_region(scissor);
        rl_decal_setup_decal_instance_for_render(renderer, decal_p);
        for (int b = 0; b < submesh.num_render_blocks; b++)
        {
            renderer.render(mesh_p, m_sub_mesh_index, b);
        }
        renderer.set_scissor_region(saved_scissor);
    }
    rl_material_library_set_override(0xFFFFFFFF);
    renderer.set_shader_type(shader_type_standard);
    renderer.set_macro_state(saved_state, false);
}

// 0x004C2200 (:322)
void rl_height_mesh_instance::set_material_map(const rl_material_map* mat_p)
{
    if (mat_p != nullptr)
    {
        m_material_map_p = mat_p;
    }
}

// 0x004C1DE0 (.h:24) and 0x004C1DF0 (.h:24) are the factory macro's get_object_pool and get_objects_max.

// 0x004C1E00 (.h:65)
int rl_height_mesh_instance::get_sub_mesh_index() const
{
    return m_sub_mesh_index;
}

// 0x005D0C30 (.h:70)
void rl_height_mesh_instance::set_sub_mesh_index(int sub_mesh_index)
{
    m_sub_mesh_index = sub_mesh_index;
}

// 0x004C1E10 (.h:75)
const vector& rl_height_mesh_instance::get_target_position() const
{
    return m_target_position;
}

// 0x005D0C40 (.h:80)
void rl_height_mesh_instance::set_target_position(const vector& target_position)
{
    m_target_position = target_position;
}

// 0x0054BAE0 (.h:85)
void rl_height_mesh_instance::set_renderable(rl_renderable* renderable_p)
{
    m_renderable_p = renderable_p;
}

// 0x0054BAF0 (.h:90)
const rl_material_map* rl_height_mesh_instance::get_material_map()
{
    return m_material_map_p;
}
