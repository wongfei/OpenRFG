// Original: volition/render_lib/code/rl_mesh_instance.cpp
//
// rl_mesh_instance: an instance of one sub mesh of an rl_mesh with its material map. It is batchable: add_to_batch puts
// the instance's transform into the pass's instance stream and fills one rl_batched_instance per render block in the
// slots the pass allocated per material (from the counts of add_material_stats_to_batch). An instance with an
// alpha-blended material goes to the deferred (sorted) list instead, unless it is a far "disable deferred" one.

#include "volition/render_lib/code/rl_mesh_instance.h"

#include "volition/render_lib/code/rl_batch_defines.h"
#include "volition/render_lib/code/rl_batched_pass.h"
#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_decal.h"
#include "volition/render_lib/code/rl_decal_internal.h"
#include "volition/render_lib/code/rl_decal_parameters.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/render_lib/code/rl_instance_stream.h"
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
#include "volition/render_lib/code/rl_scene.h"
#include "volition/render_lib/code/rl_scene_renderer.h"
#include "volition/render_lib/code/rl_visibility_manager.h"

#include <math.h>
#include <vector>

// <rl_mesh_instance::factory_init> 0x00511B70 (:36)
// <rl_mesh_instance::create> 0x0054BD80 (:36)
RL_FACTORY_CLASS_CORE(rl_mesh_instance, 0xD1AB011C, 256)

// Debug switches that skip the batched / deferred-batched mesh instances (off in retail).
bool g_hideBatchMeshes = false; // 0x01B7BCD2
bool g_hideDefferedBatchMeshes = false; // 0x01B7BCD3

extern bool RL_alpha_dist_enable;

// Decal entries are copied into the pass in chunks of this many.
static const int MESH_INSTANCE_DECAL_CHUNK = 16;

// The sub mesh of the instance (rl_mesh::get_mesh_data, inlined: the mesh data is assumed set).
static const rl_submesh_data& submesh_of(const rl_renderable* renderable_p, unsigned int submesh_index)
{
    return ((const rl_mesh*)renderable_p)->get_mesh_data()->sub_meshes[submesh_index];
}

// The distance along the camera's view direction from the camera to a point, and the point's view depth.
static float view_depth(const rl_camera* camera, const vector& point)
{
    const vector& fvec = camera->get_rotation().fvec;
    return (point.y * fvec.y + point.x * fvec.x) + point.z * fvec.z;
}

static float view_distance(const rl_camera* camera, float point_depth)
{
    const vector& fvec = camera->get_rotation().fvec;
    const vector& eye = camera->get_position();
    return fabsf(point_depth - (eye.y * fvec.y + eye.x * fvec.x + eye.z * fvec.z));
}

// 0x0053F6D0 (:36)
// Deferred while render_lib is locked. Notes (decompiled behaviour, kept as is): the object goes back to the pool
// without checking that it came from the factory mempool.
void rl_mesh_instance::release()
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
    this->~rl_mesh_instance();
}

// 0x007C1F00 (.h:31)
void rl_mesh_instance::set_objects_max(int objects_max)
{
    ms_class_factory_info.m_objects_max = objects_max;
}

// 0x0054A850 (.h:132)
// Batchable, alpha cache invalid.
rl_mesh_instance::rl_mesh_instance()
{
    m_mat_map = nullptr;
    m_submesh_index = 0;
    m_flags = (m_flags & ~FLAGS_ALPHA_CACHE_VALID) | FLAGS_IS_BATCHABLE;
}

// 0x0054A8A0 (.h:142)
rl_mesh_instance::~rl_mesh_instance()
{
}

// 0x004E94E0 (:49)
// The local box of the sub mesh (rl_scene_entity::set_local_aabb, inlined: its dirty/partition step appears three
// times), then the partition update.
void rl_mesh_instance::calculate_local_aabb()
{
    const rl_submesh_data& submesh = submesh_of(m_renderable_p, m_submesh_index);
    set_local_aabb(rl_aabb(submesh.bmin, submesh.bmax));
    update_partition_system();
}

// 0x00527F10 (:64)
// The world transform (sub mesh offset * world), the instance parameters, and every pass of every render block's
// material (only the passes with material_flags, unless it is -1).
void rl_mesh_instance::render_internal(rl_renderer& renderer, unsigned int material_flags)
{
    rl_mesh* mesh_p = (rl_mesh*)m_renderable_p;
    const rl_submesh_data& submesh = submesh_of(m_renderable_p, m_submesh_index);
    matrix43 transform(submesh.offset_transform, IDENTITY_MATRIX);
    matrix43 world_transform;
    world_transform.multiply_two(transform, get_world_transform());
    renderer.set_world_transform(world_transform);
    renderer.set_object_instance_parameters(m_instance_parameters);
    const rl_render_block* blocks = submesh.render_blocks.to_pointer();
    for (int b = 0; b < submesh.num_render_blocks; b++)
    {
        const unsigned int material_index = blocks[b].material_map_idx;
        if (material_index >= (unsigned int)m_mat_map->get_num_materials())
        {
            continue;
        }
        const unsigned int mat_handle = m_mat_map->get_material(material_index);
        set_shader_types(renderer);
        const bool depth_pass = renderer.get_depth_only();
        const int num_passes = rl_material_library_get_num_passes(mat_handle, depth_pass);
        for (int pass = 0; pass < num_passes; pass++)
        {
            if (material_flags == 0xFFFFFFFF || rl_material_library_material_has_flags(mat_handle, pass, material_flags, depth_pass))
            {
                rl_material_library_set_render_material(renderer, mat_handle, pass);
                renderer.render(mesh_p, m_submesh_index, b);
            }
        }
    }
}

// 0x00528100 (:134)
// The tint color: the instance tint with its opacity (rl_renderer::set_tint_color, inlined).
void rl_mesh_instance::prerender(rl_renderer& renderer)
{
    renderer.set_tint_color(rl_color_float(m_tint.x, m_tint.y, m_tint.z, m_opacity));
}

// 0x004C3080 (:141)
void rl_mesh_instance::postrender(rl_renderer&)
{
}

// 0x00511C70 (:145)
void rl_mesh_instance::set_shader_types(rl_renderer& renderer)
{
    renderer.set_shader_type(shader_type_standard);
    renderer.set_shader_configuration(0);
}

// 0x005281E0 (:151)
// The instance (prerender inlined), then its decals unless flags bit 1.
void rl_mesh_instance::render(rl_renderer& renderer, rl_camera&, unsigned int flags)
{
    prerender(renderer);
    render_internal(renderer, 0xFFFFFFFF);
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

// 0x00511CA0 (:181)
// Refreshes the alpha cache (FLAGS_ALPHA_CACHE: a render block has an alpha-blended material), then counts the render
// blocks per material when the instance is batched: opaque, or "disable deferred" and at least the scene's opacity
// distance end away along the view direction.
// Notes (decompiled behaviour, kept as is): FLAGS_ALPHA_CACHE_VALID is never set here (only cleared by
// set_material_map), so the cache is recomputed on every call; the material is read without checking the map.
void rl_mesh_instance::get_material_block_counts(int* buffer, const rl_camera* pCamera)
{
    const rl_submesh_data& submesh = submesh_of(m_renderable_p, m_submesh_index);
    const rl_render_block* blocks = submesh.render_blocks.to_pointer();
    if ((m_flags & FLAGS_ALPHA_CACHE_VALID) == 0)
    {
        m_flags &= ~FLAGS_ALPHA_CACHE;
        for (int b = 0; b < submesh.num_render_blocks; b++)
        {
            if (rl_material_library_material_is_transparent(m_mat_map->get_material(blocks[b].material_map_idx)))
            {
                m_flags |= FLAGS_ALPHA_CACHE;
                break;
            }
        }
    }
    bool batched = (m_flags & FLAGS_ALPHA_CACHE) == 0;
    if (!batched && (m_flags & FLAGS_DISABLE_DEFERRED) != 0 && RL_alpha_dist_enable)
    {
        const vector& position = get_position();
        const vector point(position.x + submesh.offset_transform.x, submesh.offset_transform.y + position.y, submesh.offset_transform.z + position.z);
        batched = get_scene()->get_opacity_distance_end() <= view_distance(pCamera, view_depth(pCamera, point));
    }
    if (!batched)
    {
        return;
    }
    for (int b = 0; b < submesh.num_render_blocks; b++)
    {
        buffer[blocks[b].material_map_idx]++;
    }
}

// 0x00511EC0 (:221)
// Registers the block counts per material with the pass (the pass allocates the instance slots from these).
void rl_mesh_instance::add_material_stats_to_batch(rl_batched_pass* pass_p, const rl_camera* pCamera)
{
    const int num_materials = m_mat_map->get_num_materials();
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

// 0x00528310 (:241)
// Ages the decals, and a batchable opaque instance registers its block counts with the scene's base pass.
void rl_mesh_instance::on_visible_for_frame(rl_renderer& renderer)
{
    rl_decal_process_decals(&m_decal_list);
    if (get_sort_method() == SORT_METHOD_NONE && !get_is_transparent() && get_is_batchable())
    {
        add_material_stats_to_batch(get_scene()->get_scene_renderer()->get_scene_batch(), renderer.get_active_camera());
    }
}

// 0x00511F90 (:261)
// An alpha-blended instance (unless far and "disable deferred") goes to def_ids with its view depth; otherwise its
// transform (sub mesh offset * world relative to the render offset) goes into the pass's instance stream and each
// render block fills one of the slots allocated per material. Then the decals, when the instance is in a non-shadow
// pass.
// Notes (decompiled behaviour, kept as is): the sort position is the world position plus the unrotated sub mesh
// offset; unlike rl_compound_mesh_instance, the deferred entry is added whatever the pass.
void rl_mesh_instance::add_to_batch(rl_batched_pass* pass_p, const rl_camera* pCamera, rl_deferred_batch_info* def_ids)
{
    const unsigned int vis_flags = get_vis_pass_flags();
    const rl_submesh_data& submesh = submesh_of(m_renderable_p, m_submesh_index);
    rl_scene* scene_p = get_scene();
    const unsigned int non_shadow_flags = scene_p->get_visibility_manager()->get_non_shadow_vis_flags() & vis_flags;
    const int num_materials = m_mat_map->get_num_materials();
    std::vector<int> counts(num_materials > 0 ? num_materials : 0, 0);
    get_material_block_counts(counts.data(), pCamera);
    vector render_offset;
    scene_p->get_render_offset(render_offset);
    bool deferred = false;
    if ((m_flags & FLAGS_ALPHA_CACHE) != 0 && def_ids != nullptr)
    {
        const vector& position = get_position();
        const vector point(position.x + submesh.offset_transform.x, submesh.offset_transform.y + position.y, submesh.offset_transform.z + position.z);
        const float depth = view_depth(pCamera, point);
        const float distance = view_distance(pCamera, depth);
        if ((m_flags & FLAGS_DISABLE_DEFERRED) == 0 || !RL_alpha_dist_enable || distance < scene_p->get_opacity_distance_end())
        {
            if (g_hideDefferedBatchMeshes)
            {
                return;
            }
            def_ids->add_def_id(0, depth, scene_p->get_opacity_distance_start() < distance);
            deferred = true;
        }
    }
    if (!deferred)
    {
        if (g_hideBatchMeshes)
        {
            return;
        }
        const matrix43 offset(submesh.offset_transform, IDENTITY_MATRIX);
        matrix43 world(get_world_transform());
        world.m_translation.x -= render_offset.x;
        world.m_translation.y -= render_offset.y;
        world.m_translation.z -= render_offset.z;
        rl_mesh_instance_info info;
        info.transform.multiply_two(offset, world);
        info.instance_params = m_instance_parameters;
        int transform_index = pass_p->add_transform(info);
        if (transform_index == -1)
        {
            transform_index = 0xFFFF;
        }
        const rl_mesh* mesh_p = (const rl_mesh*)m_renderable_p;
        std::vector<rl_batched_instance*> slots(num_materials > 0 ? num_materials : 0, nullptr);
        std::vector<int> used(num_materials > 0 ? num_materials : 0, 0);
        for (int i = 0; i < m_mat_map->get_num_materials(); i++)
        {
            if (counts[i] != 0)
            {
                slots[i] = pass_p->allocate_instances(m_mat_map->get_material(i), mesh_p, vis_flags, counts[i]);
            }
        }
        const rl_render_block* blocks = submesh.render_blocks.to_pointer();
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
    if (non_shadow_flags == 0 || m_decal_list == nullptr)
    {
        return;
    }
    rl_batched_decal decals[MESH_INSTANCE_DECAL_CHUNK];
    int count = 0;
    rl_decal* decal = m_decal_list;
    do
    {
        if (count == MESH_INSTANCE_DECAL_CHUNK)
        {
            pass_p->add_decals(decals, count);
            count = 0;
        }
        decals[count].decal = decal;
        decals[count].owner = this;
        decals[count].material_handle = decal->get_material();
        count++;
        decal = decal->next;
    } while (decal != m_decal_list && decal != nullptr);
    if (count != 0)
    {
        pass_p->add_decals(decals, count);
    }
}

// 0x00528370 (:464)
// render() with the render material cleared before the decals.
void rl_mesh_instance::render_for_deferred(rl_renderer& renderer, rl_camera&, unsigned int, unsigned int flags)
{
    prerender(renderer);
    render_internal(renderer, 0xFFFFFFFF);
    rl_material_library_clear_render_material(renderer);
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

// 0x005125D0 (:523)
// The indices of one render block (sub_id: the block).
int rl_mesh_instance::get_indices_for_batch(const unsigned char** indices, unsigned int& first_index, unsigned int& block_id, unsigned int sub_id) const
{
    const rl_mesh* mesh_p = (const rl_mesh*)m_renderable_p;
    const rl_mesh_data_runtime* mesh_data = mesh_p->get_mesh_data();
    const rl_render_block& block = mesh_data->sub_meshes[m_submesh_index].render_blocks.to_pointer()[sub_id];
    block_id = (unsigned int)(&block - mesh_p->get_base_render_block());
    first_index = block.start_index;
    *indices = mesh_data->index_buffer.indices + mesh_data->index_buffer.index_size * block.start_index;
    return block.num_indices;
}

// 0x00512660 (:544)
void rl_mesh_instance::get_index_range(int& index_min, int& index_max, unsigned int sub_id) const
{
    const rl_render_block& block = submesh_of(m_renderable_p, m_submesh_index).render_blocks.to_pointer()[sub_id];
    index_min = block.minimum_index;
    index_max = block.maximum_index;
}

// 0x004C3090 (:564)
// Notes (decompiled behaviour, kept as is): m_render_instance_index is never written by rl_mesh_instance.
int rl_mesh_instance::get_instance_stream_index(unsigned int sub_id) const
{
    return m_render_instance_index;
}

// 0x004E9740 (:578)
// The mesh, sub mesh and material map, at the origin (the position and orientation sets inlined).
bool rl_mesh_instance::init(rl_mesh* mesh_p, const rl_material_map* mat_map_p, unsigned int submesh_id)
{
    m_submesh_index = submesh_id;
    m_renderable_p = (rl_renderable*)mesh_p;
    m_mat_map = mat_map_p;
    rl_scene_entity::set_position(ZERO_VECTOR);
    rl_scene_entity::set_rotation(IDENTITY_MATRIX);
    update_partition_system();
    return true;
}

// 0x0053F750 (:593)
// A planar decal on the sub mesh (in the sub mesh's world frame), registered under reserved_handle.
unsigned int rl_mesh_instance::add_decal(const rl_decal_parameters* decal_params, unsigned int reserved_handle)
{
    if (rl_decal_maybe_defer_creation(reserved_handle, decal_params, this))
    {
        return reserved_handle;
    }
    const rl_submesh_data& submesh = submesh_of(m_renderable_p, m_submesh_index);
    const matrix43 offset(submesh.offset_transform, IDENTITY_MATRIX);
    matrix43 transform;
    transform.multiply_two(offset, get_world_transform());
    if (decal_params != nullptr)
    {
        if (decal_params->m_type == RLDT_PLANAR)
        {
            rl_decal* decal_p = rl_planar_decal_create(decal_params, transform.m_translation, transform.m_rotation);
            if (decal_p != nullptr)
            {
                append_decal(decal_p);
                decal_p->set_owner(this);
                decal_p->set_generic_ri_handle(0);
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

// 0x004C30A0 (:627)
int rl_mesh_instance::get_decal_count()
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

// 0x005284A0 (:639)
// The decals of the instance (VFX.md FX7), not in a depth-only pass: the sub mesh is drawn again per decal with the
// decal's material (the shader projects the decal), scissored to the decal box's screen rectangle; decals off screen
// are skipped. The material is set when it changes, the decal constants and pass state (double sided: no culling)
// when the decal handle changes. The material override and the pass state are restored after.
void rl_mesh_instance::render_decals(rl_renderer& renderer, int num_decals, rl_decal** decal_list_p)
{
    if (renderer.get_depth_only())
    {
        return;
    }
    const rl_mesh* mesh_p = (const rl_mesh*)m_renderable_p;
    const rl_submesh_data& submesh = submesh_of(m_renderable_p, m_submesh_index);
    rl_rect saved_scissor;
    renderer.get_scissor_region(saved_scissor);
    const rl_primitive_state saved_state = renderer.get_macro_state();
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
    const matrix43 offset_transform(submesh.offset_transform, IDENTITY_MATRIX);
    matrix43 world_transform;
    world_transform.multiply_two(offset_transform, get_world_transform());
    unsigned int last_material = 0xFFFFFFFF;
    unsigned int last_handle = 0xFFFFFFFF;
    for (int i = 0; i < num_decals; i++)
    {
        rl_planar_decal* decal_p = (rl_planar_decal*)decal_list_p[i];
        vector box_min;
        vector box_max;
        rl_decal_world_box(box_min, box_max, world_transform, decal_p);
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
        renderer.set_world_transform(world_transform);
        renderer.set_object_instance_parameters(m_instance_parameters);
        for (int b = 0; b < submesh.num_render_blocks; b++)
        {
            renderer.render(mesh_p, m_submesh_index, b);
        }
        renderer.set_scissor_region(saved_scissor);
    }
    rl_material_library_set_override(0xFFFFFFFF);
    renderer.set_macro_state(saved_state, false);
}

// 0x004C30C0 (:770)
// Invalidates the alpha cache.
void rl_mesh_instance::set_material_map(const rl_material_map* mat_p)
{
    if (mat_p != nullptr)
    {
        m_mat_map = mat_p;
        m_flags &= ~FLAGS_ALPHA_CACHE_VALID;
    }
}

// 0x004BEBE0 (.h:31) and 0x004BEBF0 (.h:31) are the factory macro's get_object_pool and get_objects_max.

// 0x007F20B0 (.h:73)
void rl_mesh_instance::set_submesh_index(unsigned int submesh_id)
{
    m_submesh_index = submesh_id;
}

// 0x004BEC00 (.h:74)
unsigned int rl_mesh_instance::get_submesh_index() const
{
    return m_submesh_index;
}

// 0x0054A890 (.h:82)
const rl_material_map* rl_mesh_instance::get_material_map()
{
    return m_mat_map;
}

// 0x006E36A0 (.h:91)
void rl_mesh_instance::set_disable_deferred(bool disable_deferred)
{
    if (disable_deferred)
    {
        m_flags |= FLAGS_DISABLE_DEFERRED;
        return;
    }
    m_flags &= ~FLAGS_DISABLE_DEFERRED;
}

// 0x004BCF60
// Uninitialized.
rl_mesh_instance_info::rl_mesh_instance_info()
{
}
