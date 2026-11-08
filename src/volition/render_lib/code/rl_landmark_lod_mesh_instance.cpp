// Original: volition/render_lib/code/rl_landmark_lod_mesh_instance.cpp
//
// rl_landmark_lod_mesh_instance: a distant landmark mesh (streamed by game_render_terrain_landmark_lod_stream). It
// lives in the scene's landmark LOD partition system; rl_terrain_renderer::render_landmarks draws the visible ones in
// the reserved far end of the depth range.

#include "volition/render_lib/code/rl_landmark_lod_mesh_instance.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_handle.h"
#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/vlib/code/memory/vmemory.h"

// <rl_landmark_lod_mesh_instance::factory_init> 0x00510470 (:15)
// <rl_landmark_lod_mesh_instance::create> 0x0054BB10 (:15)
RL_FACTORY_CLASS_CORE(rl_landmark_lod_mesh_instance, 0x12323434, 100)

// The landmark material technique: 1 user technique 0, 2 user technique 1, otherwise the material's own passes and
// then user technique 1.
int RL_landmark_render_mode = 1; // 0x014FA2FC

// 0x0053E470 (:15)
// Deferred while render_lib is locked. Notes (decompiled behaviour, kept as is): the object goes back to the pool
// without checking that it came from the factory mempool.
void rl_landmark_lod_mesh_instance::release()
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
    this->~rl_landmark_lod_mesh_instance();
}

// 0x0053E4F0 (:18)
// Added to the scene's landmark LOD partition system (rl_scene::add_landmark_lod, inlined; the scene is not checked),
// in visibility list 4.
rl_landmark_lod_mesh_instance::rl_landmark_lod_mesh_instance()
{
    get_scene()->add_landmark_lod(this);
    // Magic number: the landmark LOD visibility list.
    set_visibility_list_index(4);
    m_material_map_p = nullptr;
}

// 0x0053E870 (:26)
// Removed from the landmark LOD partition system (rl_scene::remove_landmark_lod, inlined).
rl_landmark_lod_mesh_instance::~rl_landmark_lod_mesh_instance()
{
    if (get_scene() != nullptr)
    {
        get_scene()->remove_landmark_lod(this);
    }
}

// 0x00526980 (:39)
// The first material of the map with the RL_landmark_render_mode technique.
// Notes (decompiled behaviour, kept as is): material_idx is ignored, and the override technique stays set on the
// material afterwards.
void rl_landmark_lod_mesh_instance::set_material(rl_renderer& renderer, int material_idx)
{
    const unsigned int mat_h = m_material_map_p->get_material(0);
    rl_material* material_p = (rl_material*)rl_material_library_get_material(mat_h);
    if (material_p == nullptr)
    {
        return;
    }
    int user_technique = 0;
    if (RL_landmark_render_mode != 1)
    {
        if (RL_landmark_render_mode != 2)
        {
            material_p->set_override_technique(rl_shader_technique_none, -1);
            rl_material_library_set_render_material(renderer, mat_h, 0);
        }
        user_technique = 1;
    }
    material_p->set_override_technique(rl_shader_technique_user, user_technique);
    rl_material_library_set_render_material(renderer, mat_h, 0);
}

// 0x00526A20 (:67)
// Every render block of the sub mesh at the instance position.
void rl_landmark_lod_mesh_instance::render_internal(rl_renderer& renderer)
{
    const rl_mesh* mesh_p = (const rl_mesh*)get_renderable();
    const rl_submesh_data& submesh = mesh_p->get_mesh_data()->sub_meshes[m_submesh_index];
    renderer.set_world_transform(matrix43(get_position(), IDENTITY_MATRIX));
    const rl_render_block* blocks = submesh.render_blocks.to_pointer();
    for (int b = 0; b < submesh.num_render_blocks; b++)
    {
        set_material(renderer, blocks[b].material_map_idx);
        renderer.render(mesh_p, m_submesh_index, b);
    }
}

// 0x00526AC0 (:90)
// The landmark with the terrain shaders, untinted, with the material library override off.
void rl_landmark_lod_mesh_instance::render(rl_renderer& renderer, unsigned int flags)
{
    renderer.set_shader_type(shader_type_terrain);
    renderer.set_shader_configuration(0);
    renderer.set_tint_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    const unsigned int override_handle = rl_material_library_get_override();
    rl_material_library_set_override(0xFFFFFFFF);
    render_internal(renderer);
    rl_material_library_set_override(override_handle);
}

// 0x00510570 (:109)
void rl_landmark_lod_mesh_instance::set_shader_types(rl_renderer& renderer)
{
    renderer.set_shader_type(shader_type_terrain);
    renderer.set_shader_configuration(0);
}

// 0x004E89D0 (:124)
// The landmark at the origin with no rotation; the position and box are set by the caller.
bool rl_landmark_lod_mesh_instance::init(rl_mesh* mesh_p, const rl_material_map* mat_map_p, unsigned int submesh_id)
{
    m_submesh_index = submesh_id;
    m_renderable_p = mesh_p;
    m_material_map_p = mat_map_p;
    set_position(ZERO_VECTOR);
    set_rotation(IDENTITY_MATRIX);
    update_partition_system();
    return true;
}

// 0x004C22D0 (.h:15) and 0x004C22E0 (.h:15) are the factory macro's get_object_pool and get_objects_max.

// (.h:15) The factory macro's count of the instances in use (inlined away in the binary; the viewer's census reads it).
int rl_landmark_lod_mesh_instance::get_objects_used()
{
    return ms_class_factory_info.m_objects_max - ms_object_pool.size();
}

// 0x0053E650 (.h:61)
// The instance takes the renderable's local box (rl_scene_entity::set_local_aabb, inlined).
void rl_landmark_lod_mesh_instance::set_renderable(rl_renderable* renderable_p)
{
    m_renderable_p = renderable_p;
    rl_aabb aabb;
    renderable_p->get_local_aabb(aabb);
    set_local_aabb(aabb);
}

// 0x0053E560 (.h:70)
rl_renderable* rl_landmark_lod_mesh_instance::get_renderable() const
{
    return m_renderable_p;
}
