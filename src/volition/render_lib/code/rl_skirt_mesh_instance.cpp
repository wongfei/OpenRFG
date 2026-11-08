// Original: volition/render_lib/code/rl_skirt_mesh_instance.cpp
//
// rl_skirt_mesh_instance: a skirt that hides the seam between a stitch piece and the terrain
// (game_render_terrain_create_stitched_piece). A "blend" skirt (blending enabled, visibility list 7) is a sub mesh of
// the stitch piece's chunk mesh drawn both as a normal mesh and blended with the terrain; a "generated" skirt
// (visibility list 5) is a sub mesh of the subzone's stitch mesh drawn with the terrain material of the instance's LOD.
// The terrain renderer draws the visible skirts itself (heightMeshSkirtInstanceRender) when g_terrainUseNewRenderer is
// on; render() is the old path.

#include "volition/render_lib/code/rl_skirt_mesh_instance.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_height_mesh_instance.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/vlib/code/memory/vmemory.h"

#include <float.h>

// <rl_skirt_mesh_instance::factory_init> 0x00518DD0 (:21)
// <rl_skirt_mesh_instance::create> 0x00550E60 (:21)
RL_FACTORY_CLASS_CORE(rl_skirt_mesh_instance, 0x64BA3EF1, 1000)

// Debug switch: the skirts are drawn (on in retail).
bool Rl_terrain_skirt_rendering_enable = true; // 0x014FA54D

// 0x00530DF0 (:21)
// Deferred while render_lib is locked. Notes (decompiled behaviour, kept as is): the object goes back to the pool
// without checking that it came from the factory mempool.
void rl_skirt_mesh_instance::release()
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
    this->~rl_skirt_mesh_instance();
}

// 0x0054E100 (:24)
// Not batchable, in the skirt visibility list.
rl_skirt_mesh_instance::rl_skirt_mesh_instance()
{
    m_terrain_material_map = nullptr;
    m_flags &= ~FLAGS_IS_BATCHABLE;
    m_blending_enabled = false;
    m_pRootMeshInstance = nullptr;
    // Magic number: the skirt visibility list (7 with blending, set_blending_enabled).
    set_visibility_list_index(5);
}

// 0x0054E150 (:32)
rl_skirt_mesh_instance::~rl_skirt_mesh_instance()
{
}

// 0x004FCD70 (:36)
// Terrain shaders, configuration 2 (skirt).
void rl_skirt_mesh_instance::set_shader_types_internal(rl_renderer& renderer)
{
    renderer.set_shader_type(shader_type_terrain);
    // Magic number: the terrain skirt shader configuration.
    renderer.set_shader_configuration(2);
}

// 0x00530E70 (:43)
// The skirt's sub mesh with the terrain material of its LOD (clamped to the map), alpha mode 2 (5 in the base pass)
// while blending.
void rl_skirt_mesh_instance::render_terrain_skirt(rl_renderer& renderer)
{
    const int lod_level = m_lod_level;
    const int last_material = m_terrain_material_map->get_num_materials() - 1;
    const unsigned int mat_h = m_terrain_material_map->get_material(lod_level < last_material ? lod_level : last_material);
    const rl_mesh* mesh_p = (const rl_mesh*)m_renderable_p;
    const rl_mesh_data_runtime* mesh_data_p = mesh_p->get_mesh_data();
    renderer.set_tint_color(rl_color_float(m_tint.x, m_tint.y, m_tint.z, m_opacity));
    renderer.set_terrain_subzone_offset(rl_terrain_subzone_offset(get_position()));
    const rl_primitive_state saved_state = renderer.get_macro_state();
    if (m_blending_enabled)
    {
        // Magic numbers: the alpha mode (state bits 0-4).
        rl_primitive_state state;
        state.set_bits((saved_state.get_bits() & ~0x1Fu) | (renderer.get_is_base_pass() ? 5 : 2));
        renderer.set_macro_state(state, false);
    }
    if ((unsigned int)m_submesh_index < (unsigned int)mesh_data_p->num_sub_meshes)
    {
        const rl_submesh_data& submesh = mesh_data_p->sub_meshes[m_submesh_index];
        const matrix43 offset(submesh.offset_transform, IDENTITY_MATRIX);
        const matrix43 world(get_position(), get_rotation());
        matrix43 transform;
        transform.multiply_two(offset, world);
        renderer.set_world_transform(transform);
        for (int b = 0; b < submesh.num_render_blocks; b++)
        {
            set_shader_types_internal(renderer);
            rl_material_library_set_render_material(renderer, mat_h, 0);
            renderer.render(mesh_p, m_submesh_index, b);
        }
    }
    if (m_blending_enabled)
    {
        renderer.set_macro_state(saved_state, false);
    }
}

// 0x00531270 (:124)
void rl_skirt_mesh_instance::render_normal_skirt(rl_renderer& renderer, rl_camera& camera)
{
    if (m_blending_enabled)
    {
        rl_mesh_instance::render(renderer, camera, 0);
    }
}

// 0x00531290 (:135)
// A blend skirt also draws as a normal mesh first; then the terrain skirt and its decals unless flags bit 1.
void rl_skirt_mesh_instance::render(rl_renderer& renderer, rl_camera& camera, unsigned int flags)
{
    if (!Rl_terrain_skirt_rendering_enable)
    {
        return;
    }
    if (m_blending_enabled)
    {
        rl_mesh_instance::render(renderer, camera, 0);
    }
    render_terrain_skirt(renderer);
    if ((flags & 2) == 0)
    {
        rl_decal* decals[256];
        const int num_decals = build_sorted_decal_list(decals, 256);
        if (num_decals > 0)
        {
            render_decals(renderer, num_decals, decals);
        }
    }
}

// 0x00531310 (:162)
void rl_skirt_mesh_instance::render_for_deferred(rl_renderer& renderer, rl_camera&, unsigned int def_id, unsigned int flags)
{
    render_terrain_skirt(renderer);
    if ((flags & 2) == 0)
    {
        rl_decal* decals[256];
        const int num_decals = build_sorted_decal_list(decals, 256);
        if (num_decals > 0)
        {
            render_decals(renderer, num_decals, decals);
        }
    }
}

// 0x004D1670 (:178)
// Blending skirts go to visibility list 7.
void rl_skirt_mesh_instance::set_blending_enabled(bool blending_enabled)
{
    m_blending_enabled = blending_enabled;
    set_visibility_list_index(blending_enabled * 2 + 5);
}

// 0x00518ED0 (:184)
// The mesh part batches like a mesh instance (without its decals); the terrain skirt goes to the deferred list as id 0
// at the far end.
void rl_skirt_mesh_instance::add_to_batch(rl_batched_pass* pass_p, const rl_camera* pCamera, rl_deferred_batch_info* def_ids)
{
    if (!Rl_terrain_skirt_rendering_enable)
    {
        return;
    }
    rl_decal* decal_list = m_decal_list;
    m_decal_list = nullptr;
    rl_mesh_instance::add_to_batch(pass_p, pCamera, def_ids);
    m_decal_list = decal_list;
    def_ids->add_def_id(0, FLT_MAX, false);
}

// 0x004D1690 (:203)
void rl_skirt_mesh_instance::add_to_deferred(const rl_camera* pCamera, rl_deferred_batch_info* def_ids)
{
}

// 0x004D1640 (.h:22) and 0x004D1650 (.h:22) are the factory macro's get_object_pool and get_objects_max.

// 0x005D0C00 (.h:38)
rl_mesh_instance* rl_skirt_mesh_instance::getRootMeshInstance() const
{
    return m_pRootMeshInstance;
}

// 0x005D0C10 (.h:39)
void rl_skirt_mesh_instance::setRootMeshInstance(rl_mesh_instance* pMesh)
{
    m_pRootMeshInstance = pMesh;
}

// 0x004D1660 (.h:60)
rl_material_map* rl_skirt_mesh_instance::get_terrain_material_map() const
{
    return m_terrain_material_map;
}

// 0x005D0C20 (.h:65)
void rl_skirt_mesh_instance::set_terrain_material_map(rl_material_map* map_p)
{
    m_terrain_material_map = map_p;
}
