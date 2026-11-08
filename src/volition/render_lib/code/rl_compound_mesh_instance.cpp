// Original: volition/render_lib/code/rl_compound_mesh_instance.cpp
//
// rl_compound_mesh_instance: one renderable instance made of sub instances of one mesh (each a sub mesh with its own
// compressed transform relative to the instance), with a fixed point kd tree over the sub instance boxes (decal
// queries). It is batchable: add_to_batch puts every visible sub instance into the scene's rl_batched_pass (one
// transform in the pass's instance stream, one rl_batched_instance per render block in the slots allocated per
// material by add_material_stats_to_batch), except alpha-blended blocks, which go to the deferred (sorted) list.
// rl_destroyable_model_instance derives from it. The header-inline members of rl_compound_mesh_instance.h are defined
// here as well.

#include "volition/render_lib/code/rl_compound_mesh_instance.h"

#include "volition/render_lib/code/rl_batch_defines.h"
#include "volition/render_lib/code/rl_batched_pass.h"
#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_decal.h"
#include "volition/render_lib/code/rl_decal_internal.h"
#include "volition/render_lib/code/rl_decal_parameters.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_fp_kdtree.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/render_lib/code/rl_instance_stream.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/render_lib/code/rl_mesh_instance.h"
#include "volition/render_lib/code/rl_metrics.h"
#include "volition/render_lib/code/rl_planar_decal.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_renderer_metrics.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/render_lib/code/rl_scene_renderer.h"
#include "volition/render_lib/code/rl_state_manager.h"
#include "volition/render_lib/code/rl_visibility_manager.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/memory/vmemory.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

// <rl_compound_mesh_instance::factory_init> 0x0050AE70 (:40)
// <rl_compound_mesh_instance::create> 0x005491B0 (:40)
RL_FACTORY_CLASS_CORE(rl_compound_mesh_instance, 0x3F2D6C63, 2)

// Alpha-blended sub instances farther than the scene's opacity distance end are drawn batched (in retail on).
bool RL_alpha_dist_enable = true; // 0x014FA103


// The mesh data of the renderable (rl_mesh::get_mesh_data, inlined): null until the mesh data is set.
static const rl_mesh_data_runtime* mesh_data_of(rl_renderable* renderable_p)
{
    return ((const rl_mesh*)renderable_p)->get_mesh_data();
}

// The local transform of a sub instance as a matrix (rl_compressed_transform::get).
static matrix43 sub_instance_transform(const rl_sub_instance& sub_instance)
{
    matrix43 transform;
    sub_instance.m_transform.get(transform);
    return transform;
}

// The world position of a sub instance: its position in the instance's frame.
static vector sub_instance_world_position(const matrix43& world, const vector& local)
{
    return vector(world.m_translation.x + world.m_rotation.rvec.x * local.x + world.m_rotation.uvec.x * local.y + world.m_rotation.fvec.x * local.z, world.m_translation.y + world.m_rotation.rvec.y * local.x + world.m_rotation.uvec.y * local.y + world.m_rotation.fvec.y * local.z,
                  world.m_translation.z + world.m_rotation.rvec.z * local.x + world.m_rotation.uvec.z * local.y + world.m_rotation.fvec.z * local.z);
}

// The distance along the camera's view direction from the camera to a point.
static float view_distance(const rl_camera* camera, const vector& point)
{
    const vector& fvec = camera->get_rotation().fvec;
    const vector& eye = camera->get_position();
    return fabsf(point.x * fvec.x + point.y * fvec.y + point.z * fvec.z - (eye.y * fvec.y + eye.x * fvec.x + eye.z * fvec.z));
}

// 0x005389A0 (:40)
// Deferred while render_lib is locked. Notes (decompiled behaviour, kept as is): the object goes back to the pool
// without checking that it came from the factory mempool.
void rl_compound_mesh_instance::release()
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
    this->~rl_compound_mesh_instance();
}

// 0x007C0B00 (.h:71)
void rl_compound_mesh_instance::set_objects_max(int objects_max)
{
    ms_class_factory_info.m_objects_max = objects_max;
}

// 0x005490B0 (.h:217)
// Batchable by default.
rl_compound_mesh_instance::rl_compound_mesh_instance()
{
    m_sub_instances = nullptr;
    m_num_sub_instances = 0;
    m_num_renderable_sub_instances = 0;
    prev = nullptr;
    next = nullptr;
    m_kdtree = nullptr;
    m_kdtree_leaves = nullptr;
    m_kdtree_nodes = nullptr;
    m_mat_map = nullptr;
    m_flags |= FLAGS_IS_BATCHABLE;
}

// 0x00549120 (.h:233)
rl_compound_mesh_instance::~rl_compound_mesh_instance()
{
    if (m_sub_instances != nullptr)
    {
        delete[] m_sub_instances;
        m_sub_instances = nullptr;
    }
    if (m_kdtree_nodes != nullptr)
    {
        delete[] m_kdtree_nodes;
        m_kdtree_nodes = nullptr;
    }
    if (m_kdtree_leaves != nullptr)
    {
        delete[] m_kdtree_leaves;
        m_kdtree_leaves = nullptr;
    }
    if (m_kdtree != nullptr)
    {
        m_kdtree->release();
        m_kdtree = nullptr;
    }
    m_num_sub_instances = 0;
    m_num_renderable_sub_instances = 0;
}

// 0x004BCF40 (:61)
void rl_compound_mesh_instance::update_xform_cache()
{
}

// 0x004E39A0 (:96)
// The local box around all sub instances (a 2 m box when there are none), and FLAGS_INSTANCE_CACHES_DIRTY.
void rl_compound_mesh_instance::calculate_local_aabb()
{
    const rl_mesh_data_runtime* mesh_data = mesh_data_of(m_renderable_p);
    vector box_min(FLT_MAX, FLT_MAX, FLT_MAX);
    vector box_max(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    int count = 0;
    for (int i = 0; i < m_num_sub_instances; i++)
    {
        const rl_sub_instance& sub_instance = m_sub_instances[i];
        if (sub_instance.m_submesh_id < (unsigned int)mesh_data->num_sub_meshes)
        {
            const rl_submesh_data& submesh = mesh_data->sub_meshes[sub_instance.m_submesh_id];
            const matrix43 transform = sub_instance_transform(sub_instance);
            vector sub_min;
            vector sub_max;
            bbox_instance(sub_min, sub_max, transform.m_rotation, transform.m_translation, submesh.bmin, submesh.bmax);
            bbox_expand(box_min, box_max, sub_min, sub_max);
            count++;
        }
    }
    if (count == 0)
    {
        box_max = vector(1.0f, 1.0f, 1.0f);
        box_min = vector(-1.0f, -1.0f, -1.0f);
    }
    set_local_aabb(rl_aabb(box_min, box_max));
    m_flags |= FLAGS_INSTANCE_CACHES_DIRTY;
    update_partition_system();
}

// 0x00538A20 (:142)
// One sub instance with the instance's tint (prerender, inlined).
void rl_compound_mesh_instance::render_individual_sub_instance(rl_renderer& renderer, unsigned int sub_instance_idx, bool rendering_decal)
{
    prerender(renderer);
    render_sub_instance(renderer, sub_instance_idx, 0xFFFFFFFF, rendering_decal, false);
}

// 0x005228A0 (:198)
// The tint color: the instance tint with its opacity (rl_renderer::set_tint_color, inlined).
void rl_compound_mesh_instance::prerender(rl_renderer& renderer)
{
    renderer.set_tint_color(rl_color_float(m_tint.x, m_tint.y, m_tint.z, m_opacity));
}

// 0x004BCF50 (:211)
void rl_compound_mesh_instance::postrender()
{
}

// 0x0050AF70 (:216)
void rl_compound_mesh_instance::set_shader_types(rl_renderer& renderer)
{
    renderer.set_shader_type(shader_type_standard);
    renderer.set_shader_configuration(0);
}

// 0x00538B10 (:222)
// All renderable sub instances (flags bit 0: skip the alpha-blended ones), then the decals unless flags bit 1.
void rl_compound_mesh_instance::render(rl_renderer& renderer, rl_camera& camera, unsigned int flags)
{
    prerender(renderer);
    for (int i = 0; i < m_num_renderable_sub_instances; i++)
    {
        render_sub_instance(renderer, i, 0xFFFFFFFF, false, (flags & 1) != 0);
    }
    if ((flags & 2) == 0)
    {
        rl_material_library_clear_render_material(renderer);
        rl_decal* decals[256];
        const int num_decals = build_sorted_decal_list(decals, 256);
        if (num_decals > 0)
        {
            render_decals(renderer, num_decals, decals);
        }
    }
}

// The alpha-blended flag of each material of the map (rl_material_library_material_is_transparent, inlined).
static std::vector<unsigned char> material_alpha_flags(const rl_material_map* mat_map)
{
    const int num_materials = mat_map->get_num_materials();
    std::vector<unsigned char> alpha(num_materials > 0 ? num_materials : 0, 0);
    for (int i = 0; i < num_materials; i++)
    {
        alpha[i] = rl_material_library_material_is_transparent(mat_map->get_material(i)) ? 1 : 0;
    }
    return alpha;
}

// 0x0050AFA0 (:259)
// The number of render blocks per material that the batch will draw: those of the active sub instances, except the
// sub instances with an alpha-blended block (unless it is a far "disable deferred" one, which is batched).
void rl_compound_mesh_instance::get_material_block_counts(int* buffer, const rl_camera* pCamera)
{
    const rl_mesh_data_runtime* mesh_data = mesh_data_of(m_renderable_p);
    const std::vector<unsigned char> alpha = material_alpha_flags(m_mat_map);
    for (int i = 0; i < m_num_renderable_sub_instances; i++)
    {
        const rl_sub_instance& sub_instance = m_sub_instances[i];
        if (!sub_instance.m_is_visible || sub_instance.m_submesh_id >= (unsigned int)mesh_data->num_sub_meshes)
        {
            continue;
        }
        const rl_submesh_data& submesh = mesh_data->sub_meshes[sub_instance.m_submesh_id];
        const rl_render_block* blocks = submesh.render_blocks.to_pointer();
        bool deferred = false;
        for (int b = 0; b < submesh.num_render_blocks; b++)
        {
            if (sub_instance.m_disable_deferred && RL_alpha_dist_enable)
            {
                if (alpha[blocks[b].material_map_idx] != 0)
                {
                    const vector position = sub_instance_world_position(get_world_transform(), sub_instance.m_transform.pos);
                    if (view_distance(pCamera, position) < get_scene()->get_opacity_distance_end())
                    {
                        deferred = true;
                    }
                    break;
                }
            }
            else if (alpha[blocks[b].material_map_idx] != 0)
            {
                deferred = true;
                break;
            }
        }
        if (deferred)
        {
            continue;
        }
        for (int b = 0; b < submesh.num_render_blocks; b++)
        {
            buffer[blocks[b].material_map_idx]++;
        }
    }
}

// 0x0050B380 (:339)
// Registers the block counts per material with the pass (the pass allocates the instance slots from these).
void rl_compound_mesh_instance::add_material_stats_to_batch(rl_batched_pass* pass_p, const rl_camera* pCamera)
{
    const int num_materials = m_mat_map->get_num_materials();
    if (num_materials * 4 == 0)
    {
        keen::debug::nativePrintArgs("rl_compound_mesh_instance::add_material_stats_to_batch: size == 0\n");
    }
    std::vector<int> counts(num_materials > 0 ? num_materials : 0, 0);
    get_material_block_counts(counts.data(), pCamera);
    const unsigned int vis_flags = get_vis_pass_flags();
    for (int i = 0; i < m_mat_map->get_num_materials(); i++)
    {
        if (counts[i] > 0)
        {
            pass_p->add_material_stats(m_mat_map->get_material(i), (const rl_mesh*)m_renderable_p, vis_flags, counts[i]);
        }
    }
}

// 0x00522980 (:366)
// Ages the decals, and a batchable opaque instance registers its block counts with the scene's base pass.
void rl_compound_mesh_instance::on_visible_for_frame(rl_renderer& renderer)
{
    rl_decal_process_decals(&m_decal_list);
    if (get_sort_method() == SORT_METHOD_NONE && !get_is_transparent() && get_is_batchable())
    {
        add_material_stats_to_batch(get_scene()->get_scene_renderer()->get_scene_batch(), renderer.get_active_camera());
    }
}

// 0x0050B450 (:387)
// The batch entries: per material the instance slots reserved in the pass (by the counts of
// add_material_stats_to_batch); each visible sub instance adds its world transform (relative to the render offset) to
// the pass's transform stream and fills one slot per render block. Sub instances with a near alpha-blended block go to
// def_ids instead (when the instance is in a non-shadow pass).
void rl_compound_mesh_instance::add_to_batch(rl_batched_pass* pass_p, const rl_camera* pCamera, rl_deferred_batch_info* def_ids)
{
    rl_scene* scene_p = get_scene();
    const unsigned int non_shadow_flags = scene_p->get_visibility_manager()->get_non_shadow_vis_flags() & get_vis_pass_flags();
    vector render_offset;
    scene_p->get_render_offset(render_offset);
    const std::vector<unsigned char> alpha = material_alpha_flags(m_mat_map);
    const int num_materials = m_mat_map->get_num_materials();
    std::vector<int> counts(num_materials > 0 ? num_materials : 0, 0);
    get_material_block_counts(counts.data(), pCamera);
    std::vector<rl_batched_instance*> slots(num_materials > 0 ? num_materials : 0, nullptr);
    std::vector<int> used(num_materials > 0 ? num_materials : 0, 0);
    const rl_mesh* mesh_p = (const rl_mesh*)m_renderable_p;
    for (int i = 0; i < num_materials; i++)
    {
        const unsigned int mat_handle = m_mat_map->get_material(i);
        if (counts[i] == 0 || mat_handle == 0xFFFFFFFF)
        {
            slots[i] = nullptr;
            continue;
        }
        slots[i] = pass_p->allocate_instances(mat_handle, mesh_p, get_vis_pass_flags(), counts[i]);
    }
    // Notes (decompiled behaviour, kept as is): the camera position in the instance's frame is computed and not used.
    vector camera_local(pCamera->get_position().x - get_position().x, pCamera->get_position().y - get_position().y, pCamera->get_position().z - get_position().z);
    camera_local.rotate(get_rotation());
    const rl_mesh_data_runtime* mesh_data = mesh_p->get_mesh_data();
    for (int i = 0; i < m_num_renderable_sub_instances; i++)
    {
        const rl_sub_instance& sub_instance = m_sub_instances[i];
        if (!sub_instance.m_is_visible || sub_instance.m_submesh_id >= (unsigned int)mesh_data->num_sub_meshes)
        {
            continue;
        }
        const rl_submesh_data& submesh = mesh_data->sub_meshes[sub_instance.m_submesh_id];
        const rl_render_block* blocks = submesh.render_blocks.to_pointer();
        bool deferred = false;
        if (def_ids != nullptr)
        {
            for (int b = 0; b < submesh.num_render_blocks; b++)
            {
                if (alpha[blocks[b].material_map_idx] == 0)
                {
                    continue;
                }
                const vector position = sub_instance_world_position(get_world_transform(), sub_instance.m_transform.pos);
                const vector& fvec = pCamera->get_rotation().fvec;
                const float dist = position.y * fvec.y + position.x * fvec.x + position.z * fvec.z;
                const float view_dist = view_distance(pCamera, position);
                if (!sub_instance.m_disable_deferred || !RL_alpha_dist_enable || view_dist < scene_p->get_opacity_distance_end())
                {
                    if (non_shadow_flags != 0)
                    {
                        def_ids->add_def_id(i, dist, scene_p->get_opacity_distance_start() < view_dist);
                    }
                    deferred = true;
                }
                break;
            }
        }
        if (deferred)
        {
            continue;
        }
        // The world transform relative to the render offset: offset (sub mesh) * local (sub instance) * world.
        matrix43 world(get_world_transform());
        world.m_translation.x -= render_offset.x;
        world.m_translation.y -= render_offset.y;
        world.m_translation.z -= render_offset.z;
        const matrix43 local = sub_instance_transform(sub_instance);
        matrix43 transform(submesh.offset_transform, IDENTITY_MATRIX);
        transform.multiply_two_additional(local, world);
        rl_mesh_instance_info info;
        info.transform = transform;
        info.instance_params = m_instance_parameters;
        int transform_index = pass_p->add_transform(info);
        if (transform_index < 0)
        {
            transform_index = 0xFFFF;
        }
        for (int b = 0; b < submesh.num_render_blocks; b++)
        {
            const rl_render_block& block = blocks[b];
            rl_batched_instance* material_slots = slots[block.material_map_idx];
            if (material_slots == nullptr)
            {
                continue;
            }
            rl_batched_instance& slot = material_slots[used[block.material_map_idx]++];
            slot.first_index = block.start_index;
            slot.num_indices = block.num_indices;
            slot.block_id = (unsigned int)(&block - mesh_p->get_base_render_block());
            slot.transform_index = transform_index;
        }
    }
    add_decals_to_batch(pass_p, non_shadow_flags);
}

// 0x0050BCD0 (:588)
// The unbatched counterpart: each visible sub instance with an alpha-blended block goes to def_ids (when the instance
// is in a non-shadow pass), with its view depth as the sort distance.
void rl_compound_mesh_instance::add_to_deferred(const rl_camera* pCamera, rl_deferred_batch_info* def_ids)
{
    if (def_ids == nullptr || m_mat_map == nullptr)
    {
        return;
    }
    const unsigned int non_shadow_flags = get_scene()->get_visibility_manager()->get_non_shadow_vis_flags() & get_vis_pass_flags();
    const std::vector<unsigned char> alpha = material_alpha_flags(m_mat_map);
    // Notes (decompiled behaviour, kept as is): the camera position in the instance's frame is computed and not used.
    vector camera_local(pCamera->get_position().x - get_position().x, pCamera->get_position().y - get_position().y, pCamera->get_position().z - get_position().z);
    camera_local.rotate(get_rotation());
    const rl_mesh_data_runtime* mesh_data = mesh_data_of(m_renderable_p);
    for (int i = 0; i < m_num_renderable_sub_instances; i++)
    {
        const rl_sub_instance& sub_instance = m_sub_instances[i];
        if (!sub_instance.m_is_visible || sub_instance.m_submesh_id >= (unsigned int)mesh_data->num_sub_meshes)
        {
            continue;
        }
        const rl_submesh_data& submesh = mesh_data->sub_meshes[sub_instance.m_submesh_id];
        const rl_render_block* blocks = submesh.render_blocks.to_pointer();
        for (int b = 0; b < submesh.num_render_blocks; b++)
        {
            if (alpha[blocks[b].material_map_idx] == 0)
            {
                continue;
            }
            if (non_shadow_flags != 0)
            {
                const vector position = sub_instance_world_position(get_world_transform(), sub_instance.m_transform.pos);
                const vector& fvec = pCamera->get_rotation().fvec;
                def_ids->add_def_id(i, fvec.y * position.y + fvec.x * position.x + fvec.z * position.z, false);
            }
            break;
        }
    }
}

// 0x00538CA0 (:778)
// A deferred (sorted) entry: the whole instance (def_id -1) or one sub instance with its decals, with the alpha mode
// of the deferred draw (alpha additive outside the base pass, filter in it).
void rl_compound_mesh_instance::render_for_deferred(rl_renderer& renderer, rl_camera& camera, unsigned int def_id, unsigned int flags)
{
    const rl_primitive_state saved_state = renderer.get_macro_state();
    rl_primitive_state state = saved_state;
    // The alpha mode of the deferred draw (set_alpha_mode inlined: bits & 0xFFFFFFE2 | 2, bits & 0xFFFFFFE5 | 5).
    if (!renderer.get_is_base_pass())
    {
        state.set_alpha_mode(RLPAM_ALPHA_ADDITIVE);
    }
    else
    {
        state.set_alpha_mode(RLPAM_FILTER);
    }
    renderer.set_macro_state(state, false);
    if (def_id == 0xFFFFFFFF)
    {
        render(renderer, camera, flags);
    }
    else
    {
        render_individual_sub_instance(renderer, def_id, false);
        rl_material_library_clear_render_material(renderer);
        if ((flags & 2) == 0)
        {
            rl_decal* decals[256];
            int num_decals = 0;
            rl_decal* decal = m_decal_list;
            if (decal != nullptr)
            {
                while (num_decals != 256)
                {
                    if (decal->get_generic_ri_handle() == def_id)
                    {
                        decals[num_decals++] = decal;
                    }
                    decal = decal->next;
                    if (decal == m_decal_list || decal == nullptr)
                    {
                        break;
                    }
                }
                if (num_decals > 0)
                {
                    sort_decals(decals, num_decals);
                    render_decals(renderer, num_decals, decals);
                }
            }
        }
    }
    renderer.set_macro_state(saved_state, false);
}

// 0x0055EEB0 (:909)
// Stripped in retail (RL_show_kdtree is off).
void rl_compound_mesh_instance::render_debug()
{
}

// 0x0050C0B0 (:935)
// The indices of one render block (sub_id: sub instance << 16 | block).
int rl_compound_mesh_instance::get_indices_for_batch(const unsigned char** indices, unsigned int& first_index, unsigned int& block_id, unsigned int sub_id) const
{
    const rl_mesh* mesh_p = (const rl_mesh*)m_renderable_p;
    const rl_mesh_data_runtime* mesh_data = mesh_p->get_mesh_data();
    const unsigned int submesh_id = m_sub_instances[sub_id >> 16].m_submesh_id;
    if (submesh_id >= (unsigned int)mesh_data->num_sub_meshes)
    {
        return 0;
    }
    const rl_render_block& block = mesh_data->sub_meshes[submesh_id].render_blocks.to_pointer()[sub_id & 0xFFFF];
    block_id = (unsigned int)(&block - mesh_p->get_base_render_block());
    first_index = block.start_index;
    *indices = mesh_data->index_buffer.indices + mesh_data->index_buffer.index_size * block.start_index;
    return block.num_indices;
}

// 0x0050C150 (:961)
void rl_compound_mesh_instance::get_index_range(int& index_min, int& index_max, unsigned int sub_id) const
{
    index_min = 0;
    index_max = 0;
    const rl_mesh_data_runtime* mesh_data = mesh_data_of(m_renderable_p);
    const unsigned int submesh_id = m_sub_instances[sub_id >> 16].m_submesh_id;
    if (submesh_id < (unsigned int)mesh_data->num_sub_meshes)
    {
        const rl_render_block& block = mesh_data->sub_meshes[submesh_id].render_blocks.to_pointer()[sub_id & 0xFFFF];
        index_min = block.minimum_index;
        index_max = block.maximum_index;
    }
}

// 0x004BCF70 (:989)
int rl_compound_mesh_instance::get_instance_stream_index(unsigned int sub_id) const
{
    return m_sub_instances[sub_id >> 16].m_instance_index;
}

// 0x00549210 (:1004)
// The sub instances from their infos (transform compressed, submesh, active and visible flags, disable deferred), the
// mesh and material map, an identity transform at the origin, and the kd tree.
bool rl_compound_mesh_instance::init(rl_mesh* mesh_p, const rl_material_map* mat_map_p, rl_sub_instance_info* sub_inst_info_p, int num_sub_inst_infos)
{
    m_sub_instances = new rl_sub_instance[num_sub_inst_infos];
    m_kdtree_nodes = new rl_fp_kdtree_node[num_sub_inst_infos];
    m_kdtree_leaves = new rl_fp_kdtree_leaf[num_sub_inst_infos];
    for (int i = 0; i < num_sub_inst_infos; i++)
    {
        rl_sub_instance& sub_instance = m_sub_instances[i];
        const rl_sub_instance_info& info = sub_inst_info_p[i];
        sub_instance.m_transform.set(info.m_transform);
        sub_instance.m_instance_index = 0xFFFF;
        sub_instance.m_is_active = info.m_is_active;
        sub_instance.m_is_visible = info.m_is_active;
        sub_instance.m_submesh_id = (unsigned short)info.m_submesh_id;
        // Notes (decompiled behaviour, kept as is): every sub instance takes the first info's disable-deferred bit.
        sub_instance.m_disable_deferred = sub_inst_info_p[0].m_disable_deferred;
    }
    m_renderable_p = mesh_p;
    m_num_sub_instances = num_sub_inst_infos;
    m_num_renderable_sub_instances = num_sub_inst_infos;
    m_batching_complexity = (short)num_sub_inst_infos;
    m_mat_map = mat_map_p;
    set_position(ZERO_VECTOR);
    set_rotation(IDENTITY_MATRIX);
    if (build_rbb())
    {
        update_partition_system();
        return true;
    }
    delete[] m_sub_instances;
    m_sub_instances = nullptr;
    delete[] m_kdtree_nodes;
    m_kdtree_nodes = nullptr;
    delete[] m_kdtree_leaves;
    m_kdtree_leaves = nullptr;
    return false;
}

// 0x00549480 (:1060)
// New transforms and active flags for some sub instances, then the kd tree again.
void rl_compound_mesh_instance::update_instances(rl_sub_instance_update* updates_p, int num_updates)
{
    for (int i = 0; i < num_updates; i++)
    {
        const rl_sub_instance_update& update = updates_p[i];
        rl_sub_instance& sub_instance = m_sub_instances[update.m_base_index];
        sub_instance.m_transform.set(update.m_info.m_transform);
        sub_instance.m_is_active = update.m_info.m_is_active;
        sub_instance.m_is_visible = update.m_info.m_is_active;
    }
    build_rbb();
}

// 0x004BCFB0 (:1076)
void rl_compound_mesh_instance::get_sub_instance_info(rl_sub_instance_info& sub_info, int base_index) const
{
    const rl_sub_instance& sub_instance = m_sub_instances[base_index];
    sub_instance.m_transform.get(sub_info.m_transform);
    sub_info.m_is_active = sub_instance.m_is_visible;
    sub_info.m_submesh_id = sub_instance.m_submesh_id;
    sub_info.m_disable_deferred = sub_instance.m_disable_deferred;
}

// 0x004BD030 (:1097)
bool rl_compound_mesh_instance::process_internal_visibility(const rl_frustum& frustum, const linked_list_p<rl_occluder>& occluders)
{
    return true;
}

// 0x004BD040 (:1156)
// Notes (decompiled behaviour, kept as is): both the visible and the active bit take is_active.
void rl_compound_mesh_instance::set_sub_instance_is_active(short* sub_inst_indices, int num_sub_insts, bool is_active)
{
    for (int i = 0; i < num_sub_insts; i++)
    {
        const int index = sub_inst_indices[i];
        if (index >= 0 && index < m_num_sub_instances)
        {
            m_sub_instances[index].m_is_visible = is_active;
            m_sub_instances[index].m_is_active = is_active;
        }
    }
}

// 0x00538DB0 (:1174)
// The kd tree over the sub instance boxes (12.4 fixed point; leaf data = sub instance index).
bool rl_compound_mesh_instance::build_rbb()
{
    if (m_kdtree != nullptr)
    {
        m_kdtree->release();
    }
    const rl_mesh_data_runtime* mesh_data = mesh_data_of(m_renderable_p);
    for (int i = 0; i < m_num_sub_instances; i++)
    {
        const rl_sub_instance& sub_instance = m_sub_instances[i];
        const matrix43 transform = sub_instance_transform(sub_instance);
        if (sub_instance.m_submesh_id < (unsigned int)mesh_data->num_sub_meshes)
        {
            const rl_submesh_data& submesh = mesh_data->sub_meshes[sub_instance.m_submesh_id];
            vector sub_min;
            vector sub_max;
            bbox_instance(sub_min, sub_max, transform.m_rotation, transform.m_translation, submesh.bmin, submesh.bmax);
            m_kdtree_leaves[i].data = i;
            m_kdtree_leaves[i].aabb.set_min(sub_min);
            m_kdtree_leaves[i].aabb.set_max(sub_max);
        }
    }
    m_kdtree = rl_fp_kdtree::create();
    m_kdtree->init(m_kdtree_nodes, m_num_sub_instances, m_kdtree_leaves, m_num_sub_instances);
    return true;
}

// 0x0050C1C0 (:1216)
// The sub instances whose boxes meet the decal's box (in the instance's frame).
// Notes (decompiled behaviour, kept as is): the query result is dropped and 0 is returned (the copy into out_indices
// was optimized away as unreachable), so compound instances never get decals this way.
int rl_compound_mesh_instance::find_decal_sub_insts(int* out_indices, int max_out_indices, const rl_decal_parameters* decal_params)
{
    return 0;
}

// 0x00549500 (:1258)
// A planar decal on each sub instance the decal touches (find_decal_sub_insts, vtbl+0x5C), with the sub instance as
// its generic handle.
unsigned int rl_compound_mesh_instance::add_decal(const rl_decal_parameters* decal_params, unsigned int reserved_handle)
{
    if (rl_decal_maybe_defer_creation(reserved_handle, decal_params, this))
    {
        return reserved_handle;
    }
    const rl_mesh* mesh_p = (const rl_mesh*)m_renderable_p;
    std::vector<int> indices(m_num_sub_instances > 0 ? m_num_sub_instances : 1);
    const int count = find_decal_sub_insts(indices.data(), m_num_sub_instances, decal_params);
    for (int i = 0; i < count; i++)
    {
        const int sub_index = indices[i];
        const rl_sub_instance& sub_instance = m_sub_instances[sub_index];
        const rl_mesh_data_runtime* mesh_data = mesh_p->get_mesh_data();
        if (sub_instance.m_submesh_id >= (unsigned int)mesh_data->num_sub_meshes)
        {
            continue;
        }
        const rl_submesh_data& submesh = mesh_data->sub_meshes[sub_instance.m_submesh_id];
        const matrix43 local = sub_instance_transform(sub_instance);
        matrix43 transform(submesh.offset_transform, IDENTITY_MATRIX);
        transform.multiply_two_additional(local, get_world_transform());
        rl_decal* decal_p = nullptr;
        if (decal_params != nullptr)
        {
            if (decal_params->m_type == RLDT_PLANAR)
            {
                decal_p = rl_planar_decal_create(decal_params, transform.m_translation, transform.m_rotation);
            }
            else
            {
                VLIB_CRITICAL_FAILURE();
            }
        }
        if (decal_p == nullptr)
        {
            // Magic number: metrics warning 0x10, a decal could not be created.
            rl_render_lib::get_instance()->m_metrics_p->set_warning_flag(0x10);
            break;
        }
        append_decal(decal_p);
        decal_p->set_owner(this);
        decal_p->set_generic_ri_handle(sub_index);
        reserved_handle = rl_decal_register(decal_p, reserved_handle);
    }
    return reserved_handle;
}

// 0x004BD0A0 (:1311)
int rl_compound_mesh_instance::get_decal_count()
{
    int count = 0;
    rl_decal* decal = m_decal_list;
    while (decal != nullptr)
    {
        decal = decal->next;
        count++;
        if (decal == m_decal_list)
        {
            break;
        }
    }
    return count;
}

// 0x005229E0 (:1328)
// rl_mesh_instance::render_decals per sub instance (VFX.md FX7): each decal belongs to the sub instance of its generic
// instance handle and is drawn with that visible sub instance's sub mesh at its world transform (offset * local *
// world).
// Notes (decompiled behaviour, kept as is): the generic handle is used as a sub instance index without a range check.
void rl_compound_mesh_instance::render_decals(rl_renderer& renderer, int num_decals, rl_decal** decal_list_p)
{
    if (renderer.get_depth_only())
    {
        return;
    }
    const rl_primitive_state saved_state = renderer.get_macro_state();
    rl_rect saved_scissor;
    renderer.get_scissor_region(saved_scissor);
    rl_primitive_state base_state;
    if (!renderer.get_is_base_pass())
    {
        base_state.set_bits(saved_state.get_bits() & Decal_state_keep_mask | Decal_state_bits);
    }
    else
    {
        base_state.set_bits(saved_state.get_bits() & Decal_state_keep_mask_base_pass | Decal_state_bits_base_pass);
    }
    rl_primitive_state decal_state = base_state;
    const rl_screen_info* screen = rl_render_lib::get_instance()->get_screen_info();
    const int screen_width = screen->get_screen_width();
    const int screen_height = screen->get_screen_height();
    const float screen_w = (float)screen_width;
    const float screen_h = (float)screen_height;
    const rl_rect screen_rect(0, 0, screen_width, screen_height);
    const matrix44 clip_screen = rl_decal_scissor_clip_screen(screen_width, screen_height);
    unsigned int last_material = 0xFFFFFFFF;
    unsigned int last_handle = 0xFFFFFFFF;
    for (int i = 0; i < num_decals; i++)
    {
        rl_planar_decal* decal_p = (rl_planar_decal*)decal_list_p[i];
        const rl_sub_instance& sub_instance = m_sub_instances[decal_p->get_generic_ri_handle()];
        if (!sub_instance.m_is_visible)
        {
            continue;
        }
        const rl_mesh* mesh_p = (const rl_mesh*)m_renderable_p;
        const unsigned int submesh_id = sub_instance.m_submesh_id;
        const rl_mesh_data_runtime* mesh_data = mesh_p->get_mesh_data();
        if (submesh_id >= (unsigned int)mesh_data->num_sub_meshes)
        {
            continue;
        }
        const rl_submesh_data& submesh = mesh_data->sub_meshes[submesh_id];
        const matrix43 local = sub_instance_transform(sub_instance);
        matrix43 transform(submesh.offset_transform, IDENTITY_MATRIX);
        transform.multiply_two_additional(local, get_world_transform());
        vector box_min;
        vector box_max;
        rl_decal_world_box(box_min, box_max, transform, decal_p);
        float x;
        float y;
        float w;
        float h;
        rl_project_get_bbox_2d_rect_ex(box_min, box_max, x, y, w, h, screen_rect, clip_screen, renderer);
        const rl_rect scissor = rl_decal_scissor_rect(x, y, w, h, screen_w, screen_h);
        if (scissor.m_right - scissor.m_left <= 0 || scissor.m_bottom - scissor.m_top <= 0)
        {
            continue;
        }
        const unsigned int material = decal_p->get_material();
        const unsigned int handle = decal_p->get_decal_handle();
        if (material != last_material)
        {
            set_shader_types(renderer);
            rl_material_library_set_render_material(renderer, material, 0);
            last_material = material;
        }
        if (handle != last_handle)
        {
            rl_decal_setup_decal_for_render(renderer, decal_p);
            if (decal_p->check_flags(RLDF_DOUBLE_SIDED))
            {
                decal_state.set_bits(decal_state.get_bits() & ~Decal_state_cull_bits);
            }
            else
            {
                decal_state = base_state;
            }
            renderer.set_macro_state(decal_state, false);
            last_handle = handle;
        }
        renderer.set_scissor_region(scissor);
        rl_decal_setup_decal_instance_for_render(renderer, decal_p);
        renderer.set_world_transform(transform);
        renderer.set_object_instance_parameters(m_instance_parameters);
        for (int b = 0; b < submesh.num_render_blocks; b++)
        {
            renderer.render(mesh_p, submesh_id, b);
        }
        renderer.set_scissor_region(saved_scissor);
    }
    rl_material_library_set_override(0xFFFFFFFF);
    renderer.set_macro_state(saved_state, false);
}

// 0x004BD0C0 (:1475)
int rl_compound_mesh_instance::calculate_memory_cost(int num_sub_instances)
{
    return num_sub_instances * 0x40 + 0x1C;
}

// 0x005232C0 (:1489)
// The render blocks of one visible sub instance at its world transform (offset * local * world), each material pass
// that has material_flags (all with 0xFFFFFFFF); with skip_alpha nothing is drawn when a block is alpha-blended.
void rl_compound_mesh_instance::render_sub_instance(rl_renderer& renderer, unsigned int sub_instance_idx, unsigned int material_flags, bool rendering_decal, bool skip_alpha)
{
    rl_mesh* mesh_p = (rl_mesh*)m_renderable_p;
    const rl_mesh_data_runtime* mesh_data = mesh_p->get_mesh_data();
    const rl_sub_instance& sub_instance = m_sub_instances[sub_instance_idx];
    if (!sub_instance.m_is_visible || sub_instance.m_submesh_id >= (unsigned int)mesh_data->num_sub_meshes)
    {
        return;
    }
    const rl_submesh_data& submesh = mesh_data->sub_meshes[sub_instance.m_submesh_id];
    const rl_render_block* blocks = submesh.render_blocks.to_pointer();
    if (skip_alpha)
    {
        for (int b = 0; b < submesh.num_render_blocks; b++)
        {
            if (rl_material_library_material_is_transparent(m_mat_map->get_material(blocks[b].material_map_idx)))
            {
                return;
            }
        }
    }
    const matrix43 local = sub_instance_transform(sub_instance);
    matrix43 transform(submesh.offset_transform, IDENTITY_MATRIX);
    transform.multiply_two_additional(local, get_world_transform());
    renderer.set_world_transform(transform);
    renderer.set_object_instance_parameters(m_instance_parameters);
    const bool depth_pass = renderer.get_depth_only();
    for (int b = 0; b < submesh.num_render_blocks; b++)
    {
        const unsigned int mat_handle = m_mat_map->get_material(blocks[b].material_map_idx);
        set_shader_types(renderer);
        const int num_passes = rl_material_library_get_num_passes(mat_handle, depth_pass);
        for (int pass = 0; pass < num_passes; pass++)
        {
            if (material_flags == 0xFFFFFFFF || rl_material_library_material_has_flags(mat_handle, pass, material_flags, depth_pass))
            {
                rl_material_library_set_render_material(renderer, mat_handle, pass);
                renderer.render(mesh_p, sub_instance.m_submesh_id, b);
            }
        }
    }
}

// 0x004BD0D0 (:1575)
void rl_compound_mesh_instance::set_material_map(const rl_material_map* mat_p)
{
    if (mat_p != nullptr)
    {
        m_mat_map = mat_p;
    }
}

// 0x004E3D60 (:1591)
// The decals of the visible sub instances into the pass's decal list (in chunks of 256; metrics warning 0x20 when the
// pass's 512 entries are full).
void rl_compound_mesh_instance::add_decals_to_batch(rl_batched_pass* pass_p, unsigned int non_shadow_vis_flags)
{
    if (non_shadow_vis_flags == 0 || m_decal_list == nullptr)
    {
        return;
    }
    rl_batched_decal decals[256];
    int count = 0;
    rl_decal* decal = m_decal_list;
    do
    {
        if (m_sub_instances[decal->get_generic_ri_handle()].m_is_visible)
        {
            if (count == 256)
            {
                pass_p->add_decals(decals, count);
                count = 0;
            }
            decals[count].decal = decal;
            decals[count].owner = this;
            decals[count].material_handle = decal->get_material();
            count++;
        }
        decal = decal->next;
    } while (decal != m_decal_list && decal != nullptr);
    if (count != 0)
    {
        pass_p->add_decals(decals, count);
    }
}

// 0x004BCB20 (.h:71) and 0x004BCB30 (.h:71) are the factory macro's get_object_pool and get_objects_max.

// 0x00549110 (.h:153)
const rl_material_map* rl_compound_mesh_instance::get_material_map()
{
    return m_mat_map;
}

// 0x004E3860 (.h:261)
// rl_scene_entity::set_position, then FLAGS_INSTANCE_CACHES_DIRTY.
void rl_compound_mesh_instance::set_position(const vector& position)
{
    rl_scene_entity::set_position(position);
    m_flags |= FLAGS_INSTANCE_CACHES_DIRTY;
}

// 0x004E38D0 (.h:269)
void rl_compound_mesh_instance::set_rotation(const matrix& rotation)
{
    rl_scene_entity::set_rotation(rotation);
    m_flags |= FLAGS_INSTANCE_CACHES_DIRTY;
}

int rl_compound_mesh_instance::get_num_sub_instances()
{
    return m_num_sub_instances;
}

rl_sub_instance* rl_compound_mesh_instance::get_sub_instance(int index)
{
    return &m_sub_instances[index];
}

// 0x004E3D50 (compiler-generated)
rl_sub_instance::rl_sub_instance()
{
}
