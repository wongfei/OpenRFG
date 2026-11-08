// Original: volition/render_lib/code/rl_low_lod_mesh_instance.cpp
//
// rl_low_lod_mesh_instance: one low LOD terrain patch (a 3x3 grid per territory cell, game_render_terrain). It lives in
// the scene's low LOD partition system; the terrain renderer draws the visible patches whose high LOD subzone is not
// loaded (or outside the camera's high LOD distance) with the first material of its map.

#include "volition/render_lib/code/rl_low_lod_mesh_instance.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_handle.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/vlib/code/memory/vmemory.h"

// <rl_low_lod_mesh_instance::factory_init> 0x00510C20 (:14)
// <rl_low_lod_mesh_instance::create> 0x0054BC60 (:14)
RL_FACTORY_CLASS_CORE(rl_low_lod_mesh_instance, 0x236D1D4A, 82)

// 0x0053EAA0 (:14)
// Deferred while render_lib is locked. Notes (decompiled behaviour, kept as is): the object goes back to the pool
// without checking that it came from the factory mempool.
void rl_low_lod_mesh_instance::release()
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
    this->~rl_low_lod_mesh_instance();
}

// 0x0053EB20 (:17)
// Added to the scene's low LOD partition system (rl_scene::add_low_lod, inlined; the scene is not checked), casting
// terrain shadows, in visibility list 3.
rl_low_lod_mesh_instance::rl_low_lod_mesh_instance()
{
    m_has_high_lod_loaded = false;
    get_scene()->add_low_lod(this);
    // Magic number: scene entity flag 0x400, casts terrain shadows (rl_renderable_instance::FLAGS_CASTS_TERRAIN_SHADOWS).
    m_flags |= 0x400;
    // Magic number: the low LOD visibility list.
    set_visibility_list_index(3);
    m_material_map_p = nullptr;
}

// 0x0053EDD0 (:31)
// Removed from the low LOD partition system (rl_scene::remove_low_lod, inlined).
rl_low_lod_mesh_instance::~rl_low_lod_mesh_instance()
{
    if (get_scene() != nullptr)
    {
        get_scene()->remove_low_lod(this);
    }
}

// 0x00527230 (:40)
// Notes (decompiled behaviour, kept as is): material_idx is ignored; the first material of the map is used.
void rl_low_lod_mesh_instance::set_material(rl_renderer& renderer, int material_idx)
{
    rl_material_library_set_render_material(renderer, m_material_map_p->get_material(0), 0);
}

// 0x00527250 (:51)
// Every render block of the sub mesh at the patch position, all with the first material of the map.
void rl_low_lod_mesh_instance::render_internal(rl_renderer& renderer, bool rendering_decal)
{
    const rl_mesh* mesh_p = (const rl_mesh*)get_renderable();
    const rl_submesh_data& submesh = mesh_p->get_mesh_data()->sub_meshes[m_submesh_index];
    renderer.set_world_transform(matrix43(get_position(), IDENTITY_MATRIX));
    for (int b = 0; b < submesh.num_render_blocks; b++)
    {
        rl_material_library_set_render_material(renderer, m_material_map_p->get_material(0), 0);
        renderer.render(mesh_p, m_submesh_index, b);
    }
}

// 0x005272E0 (:76)
// The patch with the terrain shaders, untinted, with the material library override off.
void rl_low_lod_mesh_instance::render(rl_renderer& renderer, rl_camera&, unsigned int flags)
{
    renderer.set_shader_type(shader_type_terrain);
    renderer.set_shader_configuration(0);
    renderer.set_tint_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    const unsigned int override_handle = rl_material_library_get_override();
    rl_material_library_set_override(0xFFFFFFFF);
    render_internal(renderer, false);
    rl_material_library_set_override(override_handle);
}

// 0x00510D20 (:95)
void rl_low_lod_mesh_instance::set_shader_types(rl_renderer& renderer)
{
    renderer.set_shader_type(shader_type_terrain);
    renderer.set_shader_configuration(0);
}

// 0x004C29F0 (:103)
unsigned int rl_low_lod_mesh_instance::add_decal(const rl_decal_parameters*, unsigned int)
{
    keen::debug::nativePrintArgs("Decals cannot be added to low lod renderable instances");
    return 0xFFFFFFFF;
}

// 0x004C2A10 (:110)
void rl_low_lod_mesh_instance::render_decals(int, rl_decal**)
{
}

// 0x004E8B90 (:126)
// The patch at the origin with no rotation; the position and box are set by the caller
// (game_render_terrain_create_low_lod_mesh).
bool rl_low_lod_mesh_instance::init(rl_mesh* mesh_p, const rl_material_map* mat_map_p, unsigned int submesh_id)
{
    m_submesh_index = submesh_id;
    m_renderable_p = mesh_p;
    m_material_map_p = mat_map_p;
    set_position(ZERO_VECTOR);
    set_rotation(IDENTITY_MATRIX);
    update_partition_system();
    return true;
}

// 0x004C29A0 (.h:22) and 0x004C29B0 (.h:22) are the factory macro's get_object_pool and get_objects_max.

// 0x004C29D0 (.h:71)
const rl_material_map* rl_low_lod_mesh_instance::get_material_map()
{
    return m_material_map_p;
}

// 0x004C29C0 (.h:39)
int rl_low_lod_mesh_instance::get_sub_mesh_index() const
{
    return m_submesh_index;
}

// 0x0091B1D0 (.h:90)
void rl_low_lod_mesh_instance::set_has_high_lod_loaded(bool is_loaded)
{
    m_has_high_lod_loaded = is_loaded;
}

// 0x004C29E0 (.h:95)
bool rl_low_lod_mesh_instance::get_has_high_lod_loaded() const
{
    return m_has_high_lod_loaded;
}

// 0x0053EBB0 (.h:76)
// The instance takes the renderable's local box (rl_scene_entity::set_local_aabb, inlined).
void rl_low_lod_mesh_instance::set_renderable(rl_renderable* renderable_p)
{
    m_renderable_p = renderable_p;
    rl_aabb aabb;
    renderable_p->get_local_aabb(aabb);
    set_local_aabb(aabb);
}

// 0x0053EBA0 (.h:85)
rl_renderable* rl_low_lod_mesh_instance::get_renderable() const
{
    return m_renderable_p;
}
