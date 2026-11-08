// Original: volition/render_lib/code/rl_batched_pass.cpp
//
// rl_batched_pass: the per-frame batch of a scene pass. Visible instances first register how many render blocks they
// draw per (material, mesh, visibility flags) (add_material_stats, keyed in a lock-free hash); begin_construction
// sorts these material infos and gives each a range of the shared instance buffer; the instances then fill their
// slots (allocate_instances) and transforms (add_transform) from the visibility threads. finalize_construction turns
// each material info into an rl_render_group (compiled into instanced draws when the material allows), and render
// draws the groups of the requested visibility passes, then the unbatched instances. The pass object itself is
// created and released by the scene renderer every frame; the buffers are file statics shared by all passes.

#include "volition/render_lib/code/rl_batched_pass.h"

#include "volition/render_lib/code/rl_batch_defines.h"
#include "volition/render_lib/code/rl_compiled_render_group.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_instance_stream.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/render_lib/code/rl_metrics.h"
#include "volition/render_lib/code/rl_render_group.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_metrics.h"
#include "volition/vlib/code/math/matrix.h"

#include <algorithm>
#include <intrin.h>
#include <string.h>

// <rl_batched_pass::factory_init> 0x00509AE0 (:47)
// <rl_batched_pass::release> 0x00509BE0 (:47)
// <rl_batched_pass::create> 0x00548DD0 (:47)
RL_FACTORY_CLASS_IMPLEMENTATION(rl_batched_pass, 0x11235813, 2)

// Magic numbers: the capacities of the shared buffers.
static const unsigned int BATCHED_INSTANCES_MAX = 0x6000;
static const unsigned int UNBATCHED_RENDERABLES_MAX = 0x1000;
static const unsigned int BATCHED_DECALS_MAX = 0x200;
static const unsigned int BATCHED_MATERIALS_MAX = 0x400;
static const unsigned int RENDER_GROUPS_MAX = 0x1000;

static rl_batched_instance Batched_instance_buffer[BATCHED_INSTANCES_MAX]; // 0x01B09A40
static rl_renderable_instance* Unbatched_renderable_buffer[UNBATCHED_RENDERABLES_MAX]; // 0x01B6EBF8
static rl_batched_decal Batched_decal_buffer[BATCHED_DECALS_MAX]; // 0x01B6AAF8
static rl_batched_material_info Batched_material_info_buffer[BATCHED_MATERIALS_MAX]; // 0x01B76C08
static rl_lf_static_hash<uint64_t, rl_batched_material_info, &rl_hash_uint64, 2048> Batched_material_hash; // 0x0150A720
static rl_render_group Render_group_buffer[RENDER_GROUPS_MAX]; // 0x01C74A10
static int Batched_instance_watermark = 0; // 0x01B7BC3C
static int Unbatched_instance_watermark = 0; // 0x01B7BC40
static int Batched_decal_watermark = 0; // 0x01B7BC44
static int Batched_material_watermark = 0; // 0x01B7BC48
static int Batched_group_watermark = 0; // 0x01B7BC4C

// The material hash key: visibility flags in the high half, mesh uid << 16 | material handle in the low half.
static uint64_t material_key(unsigned int mat_handle, const rl_mesh* mesh, unsigned int vis_flags)
{
    const unsigned int low = (unsigned int)(unsigned short)mesh->get_uid() << 16 | (mat_handle & 0xFFFF);
    return (uint64_t)vis_flags << 32 | low;
}

// The renderer state after a batched draw: identity world transform (rl_renderer::set_world_transform, inlined).
static void reset_world_transform(rl_renderer& renderer)
{
    renderer.set_world_transform(IDENTITY_MATRIX43);
}

// 0x00536DA0 (:55)
// Notes (decompiled behaviour, kept as is): the decal count and the hash are only set up when the transform stream
// could be created.
rl_batched_pass::rl_batched_pass()
{
    m_num_instances = 0;
    m_num_unbatched_renderables = 0;
    m_num_batched_materials = 0;
    m_transform_stream = rl_mesh_instance_stream::create();
    if (m_transform_stream != nullptr)
    {
        m_instances = Batched_instance_buffer;
        m_unbatched_renderables = Unbatched_renderable_buffer;
        m_decals = Batched_decal_buffer;
        m_batched_materials = Batched_material_info_buffer;
        m_batched_material_hash = &Batched_material_hash;
        Batched_material_hash.clear();
        m_render_groups = Render_group_buffer;
        m_num_render_groups = 0;
    }
}

// 0x004BA750 (:74)
// Releases the compiled groups and the transform stream.
rl_batched_pass::~rl_batched_pass()
{
    for (unsigned int i = 0; i < m_num_render_groups; i++)
    {
        rl_render_group& group = m_render_groups[i];
        if (group.compiled_group != nullptr)
        {
            group.compiled_group->release();
            group.compiled_group = nullptr;
        }
    }
    m_transform_stream->release();
}

// 0x004BA7B0 (:97)
// By material handle, then mesh, then visibility flags.
bool sort_material_infos(const rl_batched_material_info& lhs, const rl_batched_material_info& rhs)
{
    if (rhs.material_handle != lhs.material_handle)
    {
        return rhs.material_handle < lhs.material_handle;
    }
    if (rhs.base_mesh != lhs.base_mesh)
    {
        return rhs.base_mesh < lhs.base_mesh;
    }
    return rhs.visibility_flags < lhs.visibility_flags;
}

// 0x00555670 (:113)
// Sorts the material infos and gives each its range of the instance buffer (counts reset for allocate_instances);
// materials that no longer fit are dropped (metrics warning 4). The hash is rebuilt over the sorted infos.
void rl_batched_pass::partition_batched_instances()
{
    std::sort(m_batched_materials, m_batched_materials + m_num_batched_materials, sort_material_infos);
    m_batched_material_hash->clear();
    unsigned int first_instance = 0;
    for (unsigned int i = 0; i < m_num_batched_materials; i++)
    {
        rl_batched_material_info& info = m_batched_materials[i];
        if (info.material_handle == 0xFFFFFFFF)
        {
            continue;
        }
        if (info.num_instances + first_instance <= BATCHED_INSTANCES_MAX)
        {
            info.first_instance = first_instance;
            first_instance += info.num_instances;
            info.num_instances = 0;
            m_batched_material_hash->find_or_insert(material_key(info.material_handle, info.base_mesh, info.visibility_flags), &info);
        }
        else
        {
            info.material_handle = 0xFFFFFFFF;
            rl_render_lib::get_instance()->m_metrics_p->set_warning_flag(4);
        }
    }
    m_num_instances = first_instance;
}

// 0x004BA7E0 (:154)
// By material handle, then owner.
bool sort_decals(const rl_batched_decal& lhs, const rl_batched_decal& rhs)
{
    if (rhs.material_handle != lhs.material_handle)
    {
        return rhs.material_handle < lhs.material_handle;
    }
    return rhs.owner < lhs.owner;
}

// 0x00555790 (:171)
// rl_mesh_instance_stream::init, inlined.
void rl_batched_pass::begin_construction()
{
    m_transform_stream->init();
    m_num_instances = 0;
    m_num_unbatched_renderables = 0;
    m_num_decals = 0;
    partition_batched_instances();
    m_num_render_groups = 0;
    rl_compiled_render_group_reset_call_stats();
}

// 0x00555800 (:185)
// One render group per material info with instances (compiled when the material can combine draw calls; at most
// 4096, metrics warning 8), then the decals sorted by material and owner. The counts are clamped to the buffers and
// recorded as watermarks.
// Notes (decompiled behaviour, kept as is): the group count is incremented before and after each group is written,
// so the groups sit at odd indices and the even ones keep their zeroed contents (no visibility flags, never drawn);
// rl_compiled_render_group::create is not checked for an exhausted pool.
void rl_batched_pass::finalize_construction(rl_renderer& renderer)
{
    rl_compiled_render_group_reset();
    Batched_instance_watermark = m_num_instances;
    Unbatched_instance_watermark = m_num_unbatched_renderables;
    Batched_decal_watermark = m_num_decals;
    Batched_material_watermark = m_num_batched_materials;
    if (m_num_unbatched_renderables >= UNBATCHED_RENDERABLES_MAX)
    {
        m_num_unbatched_renderables = UNBATCHED_RENDERABLES_MAX;
    }
    if (m_num_decals >= BATCHED_DECALS_MAX)
    {
        m_num_decals = BATCHED_DECALS_MAX;
    }
    if (m_num_batched_materials >= BATCHED_MATERIALS_MAX)
    {
        m_num_batched_materials = BATCHED_MATERIALS_MAX;
    }
    for (unsigned int i = 0; i < m_num_batched_materials; i++)
    {
        const rl_batched_material_info& info = m_batched_materials[i];
        if (info.material_handle == 0xFFFFFFFF || info.num_instances == 0)
        {
            continue;
        }
        if (m_num_render_groups >= RENDER_GROUPS_MAX)
        {
            rl_render_lib::get_instance()->m_metrics_p->set_warning_flag(8);
            break;
        }
        m_num_render_groups++;
        rl_render_group& group = m_render_groups[m_num_render_groups];
        m_num_render_groups++;
        group.material_handle = info.material_handle;
        group.visibility_flags = info.visibility_flags;
        group.instances = m_instances + info.first_instance;
        group.num_instances = info.num_instances;
        const rl_mesh_data_runtime* mesh_data = info.base_mesh->get_mesh_data();
        group.vbuffer_info = mesh_data->vertex_buffer;
        group.ibuffer_info = mesh_data->index_buffer;
        group.geometry_p = &info.base_mesh->getRenderGeometry();
        if (rl_material_library_can_combine_drawcalls(group.material_handle))
        {
            group.compiled_group = rl_compiled_render_group::create();
            group.compiled_group->build(renderer, &group, m_transform_stream);
        }
        else
        {
            group.compiled_group = nullptr;
        }
    }
    if (m_num_decals != 0)
    {
        std::sort(m_decals, m_decals + m_num_decals, sort_decals);
    }
    Batched_group_watermark = m_num_render_groups;
}

// 0x00509C60 (:280)
int rl_batched_pass::add_transform(const rl_mesh_instance_info& new_inst)
{
    return m_transform_stream->add_instance(new_inst);
}

// 0x004DF880 (:292)
// Metrics warning 0x20 when the decals do not fit (the count stays raised; finalize_construction clamps it).
bool rl_batched_pass::add_decals(rl_batched_decal* decals, int num_decals)
{
    const unsigned int first = (unsigned int)_InterlockedExchangeAdd((volatile long*)&m_num_decals, num_decals);
    if (first + num_decals >= BATCHED_DECALS_MAX)
    {
        rl_render_lib::get_instance()->m_metrics_p->set_warning_flag(0x20);
        return false;
    }
    memcpy(m_decals + first, decals, num_decals * sizeof(rl_batched_decal));
    return true;
}

// 0x004DF8D0 (:312)
// Metrics warning 0x200 when the list is full.
bool rl_batched_pass::add_unbatched_renderable_instance(rl_renderable_instance* new_inst)
{
    const unsigned int index = (unsigned int)_InterlockedExchangeAdd((volatile long*)&m_num_unbatched_renderables, 1);
    if (index + 1 > UNBATCHED_RENDERABLES_MAX)
    {
        rl_render_lib::get_instance()->m_metrics_p->set_warning_flag(0x200);
        return false;
    }
    m_unbatched_renderables[index] = new_inst;
    return true;
}

// 0x004DF910 (:331)
// Adds num_instances to the (material, mesh, visibility flags) info, creating it on first use (metrics warning 0x400
// when the infos are full). Two threads creating the same info: the loser marks its own entry unused.
void rl_batched_pass::add_material_stats(unsigned int mat_handle, const rl_mesh* mesh, unsigned int vis_flags, int num_instances)
{
    if (mat_handle == 0xFFFFFFFF)
    {
        return;
    }
    const uint64_t key = material_key(mat_handle, mesh, vis_flags);
    rl_batched_material_info* info = m_batched_material_hash->find(key);
    if (info == nullptr)
    {
        const unsigned int index = (unsigned int)_InterlockedExchangeAdd((volatile long*)&m_num_batched_materials, 1);
        if (index + 1 > BATCHED_MATERIALS_MAX)
        {
            rl_render_lib::get_instance()->m_metrics_p->set_warning_flag(0x400);
            return;
        }
        rl_batched_material_info* value = m_batched_materials + index;
        value->material_handle = mat_handle;
        value->num_instances = 0;
        value->first_instance = 0;
        value->base_mesh = mesh;
        value->visibility_flags = vis_flags;
        info = m_batched_material_hash->find_or_insert(key, value);
        if (value != info)
        {
            value->material_handle = 0xFFFFFFFF;
            if (info == nullptr)
            {
                return;
            }
        }
    }
    _InterlockedExchangeAdd((volatile long*)&info->num_instances, num_instances);
}

// 0x004DF9E0 (:378)
// num_instances slots of the material info's range (after partition_batched_instances), or null.
rl_batched_instance* rl_batched_pass::allocate_instances(unsigned int mat_handle, const rl_mesh* mesh, unsigned int vis_flags, int num_instances)
{
    if (mat_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    rl_batched_material_info* info = m_batched_material_hash->find(material_key(mat_handle, mesh, vis_flags));
    if (info == nullptr)
    {
        return nullptr;
    }
    const unsigned int used = (unsigned int)_InterlockedExchangeAdd((volatile long*)&info->num_instances, num_instances);
    return m_instances + info->first_instance + used;
}

// 0x004BA800 (:397)
void rl_batched_pass_get_instance_stats(int& high_watermark, int& max_capacity)
{
    high_watermark = Batched_instance_watermark;
    max_capacity = BATCHED_INSTANCES_MAX;
}

// 0x004BA820 (:403)
void rl_batched_pass_get_unbatched_renderable_stats(int& high_watermark, int& max_capacity)
{
    high_watermark = Unbatched_instance_watermark;
    max_capacity = UNBATCHED_RENDERABLES_MAX;
}

// 0x004BA840 (:409)
void rl_batched_pass_get_decal_stats(int& high_watermark, int& max_capacity)
{
    high_watermark = Batched_decal_watermark;
    max_capacity = BATCHED_DECALS_MAX;
}

// 0x004BA860 (:415)
void rl_batched_pass_get_material_stats(int& high_watermark, int& max_capacity)
{
    high_watermark = Batched_material_watermark;
    max_capacity = BATCHED_MATERIALS_MAX;
}

// 0x00536E20 (:421)
// White tint, instanced shader configuration (1); per run of groups with one material that any of vis_pass_flags
// draws: materials that combine draw calls draw their compiled groups per pass, the others each instance per pass
// immediately (shader configuration 0). Then the unbatched instances of the passes (render flags 1, plus 2 to skip
// their decals), and the identity world transform.
void rl_batched_pass::render(rl_renderer& renderer, rl_camera& camera, unsigned int vis_pass_flags, bool skip_non_batched_decals)
{
    renderer.set_world_transform(IDENTITY_MATRIX43);
    renderer.set_tint_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    renderer.set_shader_type(shader_type_standard);
    renderer.set_shader_configuration(1);
    unsigned int start = 0;
    while (start < m_num_render_groups)
    {
        bool any_visible = false;
        unsigned int end = start;
        while (end < m_num_render_groups && m_render_groups[end].material_handle == m_render_groups[start].material_handle)
        {
            if ((m_render_groups[end].visibility_flags & vis_pass_flags) != 0)
            {
                any_visible = true;
            }
            end++;
        }
        if (any_visible)
        {
            const unsigned int mat_handle = m_render_groups[start].material_handle;
            const int num_passes = rl_material_library_get_num_passes(mat_handle, renderer.get_depth_only());
            if (!rl_material_library_can_combine_drawcalls(mat_handle))
            {
                renderer.set_shader_type(shader_type_standard);
                renderer.set_shader_configuration(0);
                for (unsigned int g = start; g < end; g++)
                {
                    rl_render_group& group = m_render_groups[g];
                    if ((group.visibility_flags & vis_pass_flags) == 0)
                    {
                        continue;
                    }
                    rl_compiled_render_group::render_immediate_begin(renderer, &group, m_transform_stream);
                    for (unsigned int i = 0; i < group.num_instances; i++)
                    {
                        for (int pass = 0; pass < num_passes; pass++)
                        {
                            rl_material_library_set_render_material(renderer, group.material_handle, pass);
                            rl_compiled_render_group::render_immediate(renderer, &group, &group.instances[i]);
                        }
                    }
                    rl_compiled_render_group::render_immediate_end(renderer);
                }
                renderer.set_shader_type(shader_type_standard);
                renderer.set_shader_configuration(1);
                reset_world_transform(renderer);
            }
            else
            {
                for (int pass = 0; pass < num_passes; pass++)
                {
                    rl_material_library_set_render_material(renderer, mat_handle, pass);
                    for (unsigned int g = start; g < end; g++)
                    {
                        if ((m_render_groups[g].visibility_flags & vis_pass_flags) != 0)
                        {
                            m_render_groups[g].compiled_group->render(renderer);
                        }
                    }
                }
            }
        }
        start = end;
    }
    for (unsigned int i = 0; i < m_num_unbatched_renderables; i++)
    {
        rl_renderable_instance* inst = m_unbatched_renderables[i];
        if ((inst->get_vis_pass_flags() & vis_pass_flags) != 0)
        {
            inst->render(renderer, camera, skip_non_batched_decals ? 3 : 1);
        }
    }
    renderer.set_tint_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    reset_world_transform(renderer);
}

// 0x00520AF0 (:531)
// The decals in runs of one material and owner (at most 256): each pass of the material, then the owner draws them.
void rl_batched_pass::render_decals(rl_renderer& renderer)
{
    unsigned int start = 0;
    while (start < m_num_decals)
    {
        rl_decal* decals[256];
        int count = 0;
        unsigned int end = start;
        while (end < m_num_decals && count != 256 && m_decals[end].material_handle == m_decals[start].material_handle && m_decals[end].owner == m_decals[start].owner)
        {
            decals[count++] = m_decals[end].decal;
            end++;
        }
        const int num_passes = rl_material_library_get_num_passes(m_decals[start].material_handle, renderer.get_depth_only());
        for (int pass = 0; pass < num_passes; pass++)
        {
            rl_material_library_set_render_material(renderer, m_decals[start].material_handle, pass);
            m_decals[start].owner->render_decals(renderer, count, decals);
        }
        start = end;
    }
}

// 0x004BA230 (.h:83) and 0x004BA240 (.h:83) are the factory macro's get_object_pool and get_objects_max.
