// Original: volition/render_lib/code/rl_morph_instance.cpp
//
// Morph instances: rl_morph_instance holds per vertex weights (vehicle dents), rl_dynamic_morph_instance the character
// face morphs.

#include "volition/render_lib/code/rl_morph_instance.h"

#include "volition/render_lib/code/rl_morph_data.h"
#include "volition/render_lib/code/rl_morph_target.h"
#include "volition/render_lib/code/rl_morph_weights.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/vlib/code/memory/page_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/scheduler.h"
#include "keen/graphics/graphicscommandwriter.hpp"

#include <new>
#include <string.h>

extern event_trigger* Morph_process_finished_trigger; // 0x01B7BCE8 (rl_deformable_mesh_instance.cpp)

// Magic number: rfg.exe's factory holds 100.
// <rl_morph_instance::factory_init> 0x00512F60 (:182)
// <rl_morph_instance::release> 0x00513060 (:182)
// <rl_morph_instance::create> 0x005292D0 (:182)
RL_FACTORY_CLASS_IMPLEMENTATION(rl_morph_instance, 0xBFFE1337, 100)

// 0x004C3750 (:185)
rl_morph_instance::rl_morph_instance()
    : m_ref_count(0), m_mode(invalid_morph_mode), m_morph_container_p(nullptr), m_morph_weights_p(nullptr), m_dynamicVertexDataValid(false), m_last_build_frame(0xFFFFFFFF)
{
}

// 0x004E9C00 (:196)
rl_morph_instance::~rl_morph_instance()
{
    if (m_morph_weights_p != nullptr)
    {
        rl_morph_weights_allocator().release_bytes(m_morph_weights_p);
        m_morph_weights_p = nullptr;
        m_num_weights = 0;
    }
}

// 0x004C3780 (:211)
void rl_morph_instance::init(rl_morph_mode mode, rl_morph_container* container)
{
    m_mode = mode;
    // Original: Int3 critical function here for any mode but per_vertex_weight.
    m_morph_container_p = container;
}

// 0x004C37C0 (:256)
unsigned char* rl_morph_instance::get_morph_weights()
{
    return m_morph_weights_p;
}

// 0x004C37D0 (:265)
rl_morph_container* rl_morph_instance::get_morph_container()
{
    return m_morph_container_p;
}

// 0x004BDC40 (.h:101)
int rl_morph_instance::increment_ref_count()
{
    return ++m_ref_count;
}

// 0x004BDC50 (.h:106)
int rl_morph_instance::decrement_ref_count()
{
    return --m_ref_count;
}

// 0x00513130 (:228)
// One weight byte per vertex of the container's first target, zeroed, from the morph system's weight allocator.
bool rl_morph_instance::alloc_morph_weights()
{
    if (m_morph_weights_p != nullptr)
    {
        return true;
    }
    if (m_morph_container_p == nullptr)
    {
        return false;
    }
    // both target formats keep the vertex count at +4 of the first target
    const unsigned int size = (unsigned int)m_morph_container_p->get_num_morph_verts(0);
    unsigned char* weights = (unsigned char*)rl_morph_weights_allocator().alloc(size, 0);
    if (weights != nullptr)
    {
        memset(weights, 0, size);
    }
    m_morph_weights_p = weights;
    if (weights == nullptr)
    {
        return false;
    }
    m_num_weights = (int)size;
    return true;
}

// 0x004E9C40 (:270)
// Once per render lib frame: the weights into the frame's weight buffer (valid when there are weights and they fit).
void rl_morph_instance::build_frame_weights(rl_renderer& renderer)
{
    rl_render_lib* render_lib = rl_render_lib::get_instance();
    if (render_lib->m_current_frame_count == m_last_build_frame)
    {
        return;
    }
    m_last_build_frame = render_lib->m_current_frame_count;
    m_dynamicVertexDataValid = m_morph_weights_p != nullptr && rl_morph_weights_fillDynamicVertexBufferData(renderer.m_pCommandWriter, &m_dynamicVertexData, m_morph_weights_p, (unsigned int)m_num_weights);
}

// 0x00529350 (:314)
// With this frame's weights and a container of target format 0: the container's targets as vertex stream 1 and the
// weights as stream 2.
bool rl_morph_instance::render_setup(rl_renderer& renderer)
{
    if (!m_dynamicVertexDataValid || m_morph_container_p->m_morph_data->target_format != 0)
    {
        return false;
    }
    renderer.m_vertexBufferData[1] = m_morph_container_p->m_vertexBufferData;
    renderer.m_vertexBufferData[2] = m_dynamicVertexData;
    return true;
}

// 0x005293D0 (:353)
// Unbinds the two morph streams render_setup bound.
bool rl_morph_instance::render_teardown(rl_renderer& renderer)
{
    if (!m_dynamicVertexDataValid || m_morph_container_p->m_morph_data->target_format != 0)
    {
        return false;
    }
    renderer.m_vertexBufferData[1].pBuffer = nullptr;
    renderer.m_vertexBufferData[1].vertexCount = 0;
    renderer.m_vertexBufferData[1].offset = 0;
    renderer.m_vertexBufferData[2].pBuffer = nullptr;
    renderer.m_vertexBufferData[2].vertexCount = 0;
    renderer.m_vertexBufferData[2].offset = 0;
    return true;
}

// 0x004C37E0 (:379)
bool rl_morph_instance::is_valid_for_rendering()
{
    return m_dynamicVertexDataValid && m_morph_container_p != nullptr;
}

// ---- rl_dynamic_morph_instance (stub_impl.md S3): compressed targets summed on the CPU into this frame's morph buffer,
// bound as vertex stream 1 of the skinned mesh.

// The active morphs of every dynamic morph instance.
object_pool<rl_active_morph, 256, 1> Active_morph_pool(nullptr, nullptr, 0); // 0x01CCE9B0
static unsigned int Rl_dyn_morph_largest_allocation; // 0x01B7BCE4, never read

// 0x00512CF0 (:57)
// One compressed target scaled by blend_wt into the destination vertices (only those inside m_num_morph_verts). The
// position deltas are shorts scaled by the target's maxima (fixed point, 8192 per unit, 32767 per short); the normal
// deltas move the destination's normal bytes around 0x7F and clamp them.
bool rl_dynamic_morph_instance::add_morph_contribution(rl_morph_vert_uncompressed* morph_dest_p, rl_morph_target_compressed* morph_target_p, float blend_wt)
{
    if (morph_target_p == nullptr)
    {
        return false;
    }
    // the scales in double from the float maxima and weight, truncated (magic numbers: 8192 and 255 per unit)
    const int sx = (int)((double)morph_target_p->x_max * (double)blend_wt * 8192.0);
    const int sy = (int)((double)morph_target_p->y_max * (double)blend_wt * 8192.0);
    const int sz = (int)((double)morph_target_p->z_max * (double)blend_wt * 8192.0);
    const int snx = (int)((double)morph_target_p->nx_max * (double)blend_wt * 255.0);
    const int sny = (int)((double)morph_target_p->ny_max * (double)blend_wt * 255.0);
    const int snz = (int)((double)morph_target_p->nz_max * (double)blend_wt * 255.0);
    const rl_morph_vert_compressed* verts = (rl_morph_vert_compressed*)morph_target_p->verts;
    for (int i = 0; i < (int)morph_target_p->num_morph_verts; i++)
    {
        const rl_morph_vert_compressed& src = verts[i];
        if ((int)src.vb_mapping >= (int)m_num_morph_verts)
        {
            continue;
        }
        rl_morph_vert_uncompressed& dst = morph_dest_p[src.vb_mapping];
        dst.delta_pos[1] = (short)(dst.delta_pos[1] + (short)(((int)src.delta_pos[1] * sy) / 0x7FFF));
        dst.delta_pos[0] = (short)(dst.delta_pos[0] + (short)(((int)src.delta_pos[0] * sx) / 0x7FFF));
        dst.delta_pos[2] = (short)(dst.delta_pos[2] + (short)(((int)src.delta_pos[2] * sz) / 0x7FFF));
        const unsigned int src_normal = src.delta_normal.data;
        const unsigned int dst_normal = dst.normal.data;
        int nx = ((int)(src_normal & 0xFF) - 0x7F) * snx / 0xFF + (int)(dst_normal & 0xFF);
        int ny = ((int)((src_normal >> 8) & 0xFF) - 0x7F) * sny / 0xFF + (int)((dst_normal >> 8) & 0xFF);
        int nz = ((int)((src_normal >> 16) & 0xFF) - 0x7F) * snz / 0xFF + (int)((dst_normal >> 16) & 0xFF);
        nx = nx < 0 ? 0 : (nx > 0xFF ? 0xFF : nx);
        ny = ny < 0 ? 0 : (ny > 0xFF ? 0xFF : ny);
        nz = nz < 0 ? 0 : (nz > 0xFF ? 0xFF : nz);
        // original: the w byte of the normal is cleared
        dst.normal.data = ((unsigned int)nz << 8 | (unsigned int)ny) << 8 | (unsigned int)nx;
    }
    return true;
}

// The class factory (:384): class id 0x64738000, 100 instances.
// <rl_dynamic_morph_instance::factory_init> 0x005131A0 (:384)
// <rl_dynamic_morph_instance::release> 0x00513290 (:384)
// <rl_dynamic_morph_instance::create> 0x0054F510 (:384)
RL_FACTORY_CLASS_IMPLEMENTATION(rl_dynamic_morph_instance, 0x64738000, 100)

// 0x007C1F30 (.h:57)
// game_render_system_init sets 0x20 (rfg.exe writes the maximum inline).
void rl_dynamic_morph_instance::set_objects_max(int objects_max)
{
    ms_class_factory_info.m_objects_max = objects_max;
}

// 0x0054BE20 (:387)
rl_dynamic_morph_instance::rl_dynamic_morph_instance()
{
    m_dynamic_morph_bucket.init(&Active_morph_pool);
    m_dynamic_morph_bucket.clear(0);
    m_dynamicVertexDataValid = false;
}

// 0x0054BE80 (:396)
rl_dynamic_morph_instance::~rl_dynamic_morph_instance()
{
    m_dynamic_morph_bucket.clear(0);
}

// 0x004C3800 (:401)
void rl_dynamic_morph_instance::init(int num_verts)
{
    m_num_morph_verts = num_verts;
    m_morph_data_size = num_verts * sizeof(rl_morph_vert_uncompressed); // 0xC per vertex
}

// 0x0053FD50 (:407)
void rl_dynamic_morph_instance::clear_active_morphs()
{
    m_dynamic_morph_bucket.clear(0);
}

// 0x00529410 (:412)
// False when the container maps vertices beyond this instance's vertex count. Original: the pool's alloc is not
// checked (256 active morphs across all instances).
bool rl_dynamic_morph_instance::add_active_morph(rl_morph_container* container_p, int target, float weight)
{
    if ((int)m_num_morph_verts <= container_p->get_max_vertex_index())
    {
        return false;
    }
    unsigned char* target_data = container_p->get_morph_target(target); // inlined
    rl_active_morph* morph = m_dynamic_morph_bucket.alloc(0);
    morph->m_morph_target_data_p = target_data;
    morph->blend_percent = weight;
    return true;
}

// 0x0054BED0 (:427)
void rl_dynamic_morph_instance::process_morph_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2)
{
    ((rl_dynamic_morph_instance*)arg1.pointer_val)->process_morphs_internal((rl_morph_vert_uncompressed*)arg2.pointer_val);
}

// 0x0053FD90 (:437)
// The active morphs summed in the thread's scratch pool, then copied to the mapped buffer. Without scratch memory the
// instance turns invalid and drops its morphs.
void rl_dynamic_morph_instance::process_morphs_internal(rl_morph_vert_uncompressed* destination_buffer_p)
{
    if (Rl_dyn_morph_largest_allocation <= m_morph_data_size)
    {
        Rl_dyn_morph_largest_allocation = m_morph_data_size;
    }
    mempool_base* scratch = Vlib_scratch_pool; // TLS +0x30
    mempool_mark_scope scope(scratch);
    rl_morph_vert_uncompressed* morph_dest_p = (rl_morph_vert_uncompressed*)scratch->alloc(m_morph_data_size, 0);
    if (morph_dest_p == nullptr)
    {
        m_dynamicVertexDataValid = false;
        m_dynamic_morph_bucket.clear(0);
        return;
    }
    rl_compressed_vector zero_normal;
    zero_normal.set(0.0f, 0.0f, 0.0f, 0.0f);
    for (unsigned int i = 0; i < m_num_morph_verts; i++)
    {
        morph_dest_p[i].delta_pos[0] = 0;
        morph_dest_p[i].delta_pos[1] = 0;
        morph_dest_p[i].delta_pos[2] = 0;
        morph_dest_p[i].delta_pos[3] = 0;
        morph_dest_p[i].normal.data = zero_normal.data;
    }
    m_dynamic_morph_bucket.thread_access.enter();
    for (rl_active_morph* morph = m_dynamic_morph_bucket.get_first(0); morph != nullptr; morph = m_dynamic_morph_bucket.get_next(morph))
    {
        if (0.0f < morph->blend_percent)
        {
            add_morph_contribution(morph_dest_p, (rl_morph_target_compressed*)morph->m_morph_target_data_p, morph->blend_percent);
        }
    }
    m_dynamic_morph_bucket.thread_access.leave();
    memcpy(destination_buffer_p, morph_dest_p, m_morph_data_size);
}

// 0x0053FF20 (:493)
// The frame's morphed vertices, computed on the calling thread (the job path is not used on PC).
void rl_dynamic_morph_instance::process_active_morphs(rl_renderer& renderer)
{
    m_dynamicVertexDataValid = false;
    if (m_morph_data_size == 0 || m_num_morph_verts == 0 || m_dynamic_morph_bucket.get_count(0) < 1)
    {
        return;
    }
    keen::GraphicsCommandWriter* command_writer = renderer.m_pCommandWriter;
    rl_morph_vert_uncompressed* destination = (rl_morph_vert_uncompressed*)rl_morph_system_beginDynamicVertexBufferData(command_writer, &m_dynamicVertexData, m_morph_data_size);
    if (destination != nullptr)
    {
        m_dynamicVertexDataValid = true;
        process_morphs_internal(destination);
        rl_morph_system_endDynamicVertexBufferData(command_writer);
    }
}

// 0x00513310 (:575)
bool rl_dynamic_morph_instance::render_setup(rl_renderer& renderer)
{
    if (!morph_valid()) // inlined
    {
        return false;
    }
    renderer.m_vertexBufferData[1] = m_dynamicVertexData;
    return true;
}

// 0x00513360 (:607)
bool rl_dynamic_morph_instance::render_teardown(rl_renderer& renderer)
{
    if (!morph_valid())
    {
        return false;
    }
    renderer.m_vertexBufferData[1].pBuffer = nullptr;
    renderer.m_vertexBufferData[1].vertexCount = 0;
    renderer.m_vertexBufferData[1].offset = 0;
    return true;
}

// 0x004C3820 (:618)
bool rl_dynamic_morph_instance::morph_valid()
{
    return m_dynamicVertexDataValid && m_morph_data_size != 0 && m_num_morph_verts != 0;
}

// 0x004C3840 (:623)
void rl_d3d_morph_run_jobs()
{
}

// 0x004C3850 (:641)
void rl_d3d_morph_wait_for_complete()
{
    if (Morph_process_finished_trigger != nullptr)
    {
        scheduler_relinquish_cpu(Morph_process_finished_trigger, ccjt_d3d);
        Morph_process_finished_trigger = nullptr;
    }
}
