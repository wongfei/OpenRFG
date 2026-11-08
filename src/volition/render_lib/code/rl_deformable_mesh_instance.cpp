// Original: volition/render_lib/code/rl_deformable_mesh_instance.cpp
//
// A compound mesh instance deformed by spheres (vehicle dents): queued per instance, processed into the morph weights
// once per frame.

#include "volition/render_lib/code/rl_deformable_mesh_instance.h"

#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/render_lib/code/rl_morph_instance.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/vmemory.h"

#include <math.h>
#include <string.h>

class event_trigger;

// The morph job's completion trigger (rl_d3d_morph_wait_for_complete, rl_scene_shadows); nothing sets it on PC.
event_trigger* Morph_process_finished_trigger; // 0x01B7BCE8
static rl_deformable_mesh_instance* Pending_deform_list; // 0x01B7BC6C
static object_pool<rl_deformation_sphere, 500, 1> Deform_sphere_pool(nullptr, nullptr, 0); // 0x01C32130

// Magic number: rfg.exe's factory holds 1000 instances.
// <rl_deformable_mesh_instance::factory_init> 0x0050C7D0 (:226)
// <rl_deformable_mesh_instance::create> 0x0054E6C0 (:226)
RL_FACTORY_CLASS_CORE(rl_deformable_mesh_instance, 0x6A55B736, 1000)

// 0x00539730 (:226): like a compound mesh instance, deferred while render_lib is locked.
void rl_deformable_mesh_instance::release()
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
    this->~rl_deformable_mesh_instance();
}

// 0x00549860 (:230)
rl_deformable_mesh_instance::rl_deformable_mesh_instance()
    : m_morph_instance_p(nullptr), m_pending_deformation_spheres(nullptr), pending_prev(nullptr), pending_next(nullptr)
{
    // Magic number: bit 0x80, no morph weights on this instance yet.
    m_flags |= 0x80;
}

// 0x005498D0 (:241)
rl_deformable_mesh_instance::~rl_deformable_mesh_instance()
{
    if (m_morph_instance_p != nullptr)
    {
        if (m_morph_instance_p->decrement_ref_count() == 0)
        {
            m_morph_instance_p->release();
        }
        m_morph_instance_p = nullptr;
    }
    // Original: the next sphere is read after the free.
    rl_deformation_sphere* sphere_p = m_pending_deformation_spheres;
    while (sphere_p != nullptr)
    {
        Deform_sphere_pool.free(sphere_p);
        sphere_p = sphere_p->next;
        if (sphere_p == m_pending_deformation_spheres)
        {
            break;
        }
    }
    m_pending_deformation_spheres = nullptr;
    if (pending_prev != nullptr || pending_next != nullptr)
    {
        if (Pending_deform_list == this && (Pending_deform_list = pending_next) == this)
        {
            Pending_deform_list = nullptr;
        }
        else
        {
            pending_prev->pending_next = pending_next;
            pending_next->pending_prev = pending_prev;
        }
        pending_prev = nullptr;
        pending_next = nullptr;
    }
}

// Inlined in render and render_for_deferred: morph streams 1 and 2 unbound again after the draw (only the uncompressed
// target format binds them).
static void rl_deform_end_render(rl_morph_instance* morph, rl_renderer& renderer)
{
    if (morph != nullptr)
    {
        morph->render_teardown(renderer);
    }
}

// 0x005397B0 (:272)
void rl_deformable_mesh_instance::render(rl_renderer& renderer, rl_camera& camera, unsigned int flags)
{
    if (m_morph_instance_p != nullptr)
    {
        m_morph_instance_p->build_frame_weights(renderer); // inlined
        m_morph_instance_p->render_setup(renderer);
    }
    rl_compound_mesh_instance::render(renderer, camera, flags);
    rl_deform_end_render(m_morph_instance_p, renderer);
}

// 0x00539870 (:288)
void rl_deformable_mesh_instance::render_for_deferred(rl_renderer& renderer, rl_camera& camera, unsigned int def_id, unsigned int flags)
{
    if (m_morph_instance_p != nullptr)
    {
        m_morph_instance_p->build_frame_weights(renderer); // inlined
        m_morph_instance_p->render_setup(renderer);
    }
    rl_compound_mesh_instance::render_for_deferred(renderer, camera, def_id, flags);
    rl_deform_end_render(m_morph_instance_p, renderer);
}

// 0x0050C8D0 (:348)
void rl_deformable_mesh_instance::set_shader_types(rl_renderer& renderer)
{
    renderer.set_shader_type(shader_type_standard);
    // Magic number: shader configuration 2 is the morphed one.
    renderer.set_shader_configuration(m_morph_instance_p != nullptr && m_morph_instance_p->is_valid_for_rendering() ? 2 : 0);
}

// 0x004E40A0 (:362)
void rl_deformable_mesh_instance::set_morph_instance(rl_morph_instance* instance_p)
{
    if (instance_p != nullptr && instance_p == m_morph_instance_p)
    {
        return;
    }
    if (m_morph_instance_p != nullptr && m_morph_instance_p->decrement_ref_count() == 0)
    {
        m_morph_instance_p->release();
    }
    m_morph_instance_p = instance_p;
    if (instance_p == nullptr)
    {
        m_flags |= 0x80;
        return;
    }
    instance_p->increment_ref_count();
    if (instance_p->get_morph_weights() != nullptr)
    {
        m_flags &= 0xFF7F;
    }
}

// 0x004BDC70 (:401)
rl_morph_instance* rl_deformable_mesh_instance::get_morph_instance()
{
    return m_morph_instance_p;
}

// Clamps to [0, 1] the way the deformation does (each test on a float difference computed in double).
static float rl_deform_clamp01(float v)
{
    if ((float)((double)v - 0.0) < 0.0f)
    {
        v = 0.0f;
    }
    if (!((float)((double)v - 1.0) < 0.0f))
    {
        v = 1.0f;
    }
    return v;
}

// Not in the binary: the body process_deform_sphere and process_deform_sphere_for_submesh both inline. Every vertex the
// sub instance's render blocks index (once each, in the scratch pool's visited array) inside the sphere gains weight
// by (1 - distance / radius) * intensity, up to the sphere's maximum deformation. The steps run in double and round
// where the source assigns a float.
static void rl_deform_sub_instance(const rl_deformation_sphere* sphere_p, const rl_sub_instance& sub, const rl_mesh_data_runtime* data, unsigned char* weights, unsigned short& entity_flags)
{
    const float radius_sq = (float)((double)sphere_p->m_radius * (double)sphere_p->m_radius);
    const float inv_radius = (float)(1.0 / (double)sphere_p->m_radius);
    const double unit = (double)0.003921569f; // magic number: 1/255 as a float
    const float max_weight = rl_deform_clamp01((float)((double)(float)sphere_p->m_max_deformation * unit));
    matrix orient;
    orient.set(sub.m_transform.pitch, sub.m_transform.bank, sub.m_transform.heading);
    if (!sub.m_is_active || (int)sub.m_submesh_id >= data->num_sub_meshes)
    {
        return;
    }
    const rl_submesh_data* submesh = (const rl_submesh_data*)((const unsigned char*)data->sub_meshes + sub.m_submesh_id * sizeof(rl_submesh_data));
    mempool_base* scratch = Vlib_scratch_pool; // TLS +0x30
    unsigned char* visited = (unsigned char*)scratch->alloc((unsigned int)data->vertex_buffer.num_verts, 0);
    memset(visited, 0, (size_t)data->vertex_buffer.num_verts);
    vector local_pos;
    local_pos.x = (float)((double)sphere_p->m_position.x - (double)sub.m_transform.pos.x);
    local_pos.y = (float)((double)sphere_p->m_position.y - (double)sub.m_transform.pos.y);
    local_pos.z = (float)((double)sphere_p->m_position.z - (double)sub.m_transform.pos.z);
    local_pos.rotate(orient);
    const rl_render_block* blocks = (const rl_render_block*)submesh->render_blocks;
    for (int b = 0; b < submesh->num_render_blocks; b++)
    {
        const rl_render_block& block = blocks[b];
        for (unsigned int i = block.start_index; i < block.start_index + block.num_indices; i++)
        {
            unsigned int vertex;
            if (data->index_buffer.index_size == 2)
            {
                vertex = ((const unsigned short*)data->index_buffer.indices)[i];
                if (vertex == 0xFFFF) // strip restart
                {
                    continue;
                }
            }
            else
            {
                vertex = ((const unsigned int*)data->index_buffer.indices)[i];
                if (vertex == 0xFFFFFF) // magic number: the 32-bit restart index (24 bits)
                {
                    continue;
                }
            }
            if (visited[vertex] != 0)
            {
                continue;
            }
            visited[vertex] = 1;
            if (weights[vertex] >= sphere_p->m_max_deformation)
            {
                continue;
            }
            const vector& pos = *(const vector*)(data->vertex_buffer.verts + vertex * data->vertex_buffer.vert_stride_0);
            const float dist_sq = vec_dist_squared(local_pos, pos);
            if (!(radius_sq > dist_sq))
            {
                continue;
            }
            const float t = rl_deform_clamp01((float)((double)(float)sqrt((double)dist_sq) * (double)inv_radius));
            const float gain = (float)((1.0 - (double)t) * (double)sphere_p->m_intensity);
            float w = rl_deform_clamp01((float)((double)(float)weights[vertex] * unit));
            w = (float)((double)w + (double)gain);
            if (!((float)((double)w - (double)max_weight) < 0.0f))
            {
                w = max_weight;
            }
            w = rl_deform_clamp01(w);
            weights[vertex] = (unsigned char)fl_uround((float)((double)w * 255.0));
        }
    }
    entity_flags &= 0xFF7F; // magic number: clears bit 0x80 (the cached state of the instance)
}

// 0x00539230 (:52)
// A sphere on every sub instance (subpiece -1) or on one.
void rl_deformable_mesh_instance::process_deform_sphere(rl_deformation_sphere* sphere_p)
{
    if (m_morph_instance_p == nullptr)
    {
        return;
    }
    if (sphere_p->m_subpiece != -1)
    {
        process_deform_sphere_for_submesh(sphere_p, sphere_p->m_subpiece);
        return;
    }
    for (int i = 0; i < m_num_sub_instances; i++)
    {
        mempool_mark_scope scope(Vlib_scratch_pool);
        if (m_morph_instance_p == nullptr)
        {
            continue;
        }
        if (m_morph_instance_p->m_morph_weights_p == nullptr && !m_morph_instance_p->alloc_morph_weights())
        {
            continue;
        }
        const rl_mesh_data_runtime* data = ((rl_mesh*)m_renderable_p)->m_data_set ? &((rl_mesh*)m_renderable_p)->m_data : nullptr;
        rl_deform_sub_instance(sphere_p, m_sub_instances[i], data, m_morph_instance_p->m_morph_weights_p, m_flags);
    }
}

// 0x00523800 (:76)
void rl_deformable_mesh_instance::process_deform_sphere_for_submesh(rl_deformation_sphere* sphere_p, int submesh_idx)
{
    mempool_mark_scope scope(Vlib_scratch_pool);
    if (m_morph_instance_p == nullptr)
    {
        return;
    }
    if (m_morph_instance_p->m_morph_weights_p == nullptr && !m_morph_instance_p->alloc_morph_weights())
    {
        return;
    }
    const rl_mesh_data_runtime* data = ((rl_mesh*)m_renderable_p)->m_data_set ? &((rl_mesh*)m_renderable_p)->m_data : nullptr;
    rl_deform_sub_instance(sphere_p, m_sub_instances[submesh_idx], data, m_morph_instance_p->m_morph_weights_p, m_flags);
}

// 0x00539930 (:333)
// The compound mesh decals with the dents: the morph weights of the frame and the morph streams around them.
void rl_deformable_mesh_instance::render_decals(rl_renderer& renderer, int num_decals, rl_decal** decal_list_p)
{
    if (m_morph_instance_p != nullptr)
    {
        m_morph_instance_p->build_frame_weights(renderer); // inlined
        m_morph_instance_p->render_setup(renderer);
    }
    rl_compound_mesh_instance::render_decals(renderer, num_decals, decal_list_p);
    if (m_morph_instance_p != nullptr)
    {
        m_morph_instance_p->render_teardown(renderer); // inlined
    }
}

// 0x005399F0 (:408)
// Every queued sphere processed and back in the pool. Original: the next sphere is read after the free.
void rl_deformable_mesh_instance::process_pending_spheres()
{
    rl_deformation_sphere* sphere_p = m_pending_deformation_spheres;
    while (sphere_p != nullptr)
    {
        process_deform_sphere(sphere_p);
        Deform_sphere_pool.free(sphere_p);
        sphere_p = sphere_p->next;
        if (sphere_p == m_pending_deformation_spheres)
        {
            break;
        }
    }
    m_pending_deformation_spheres = nullptr;
}

// 0x0050C920 (:436)
// The sphere queued on the instance and the instance on the pending list; false when the 500 spheres are in use.
bool rl_deformable_mesh_instance::queue_deform_sphere(const vector& position, float radius, float intensity, unsigned char max_deformation, int subpiece)
{
    rl_deformation_sphere* sphere_p = Deform_sphere_pool.alloc(0);
    if (sphere_p == nullptr)
    {
        return false;
    }
    sphere_p->m_position = position;
    sphere_p->m_radius = radius;
    sphere_p->m_intensity = intensity;
    sphere_p->m_max_deformation = max_deformation;
    sphere_p->m_subpiece = subpiece;
    if (m_pending_deformation_spheres == nullptr)
    {
        sphere_p->next = sphere_p;
        sphere_p->prev = sphere_p;
        m_pending_deformation_spheres = sphere_p;
    }
    else
    {
        sphere_p->prev = m_pending_deformation_spheres->prev;
        sphere_p->next = m_pending_deformation_spheres;
        m_pending_deformation_spheres->prev->next = sphere_p;
        m_pending_deformation_spheres->prev = sphere_p;
    }
    if (pending_next == nullptr)
    {
        if (Pending_deform_list == nullptr)
        {
            pending_next = this;
            pending_prev = this;
            Pending_deform_list = this;
            return true;
        }
        pending_prev = Pending_deform_list->pending_prev;
        pending_next = Pending_deform_list;
        Pending_deform_list->pending_prev->pending_next = this;
        Pending_deform_list->pending_prev = this;
    }
    return true;
}

// 0x00539A30 (:467)
// Once per frame: every pending instance's spheres processed (process_pending_spheres inlined) and the instance taken
// off the pending list.
void rl_deformation_process_pending_morphs()
{
    rl_deformable_mesh_instance* instance = Pending_deform_list;
    while (instance != nullptr)
    {
        rl_deformable_mesh_instance* next = instance->pending_next != Pending_deform_list ? instance->pending_next : nullptr;
        instance->process_pending_spheres();
        if (Pending_deform_list == instance && (Pending_deform_list = instance->pending_next) == instance)
        {
            Pending_deform_list = nullptr;
        }
        else
        {
            instance->pending_prev->pending_next = instance->pending_next;
            instance->pending_next->pending_prev = instance->pending_prev;
        }
        instance->pending_prev = nullptr;
        instance->pending_next = nullptr;
        instance = next;
    }
}
