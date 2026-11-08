// Original: volition/render_lib/code/rl_compiled_render_group.cpp
//
// rl_compiled_render_group: one render group (instances of one material on one mesh) compiled into instanced draws.
// build sorts the instances by render block and copies their transforms into one instancing batch; each run of one
// block becomes an rl_compiled_render_call (from the per-frame Call_pool). render issues one instanced draw per call.
// render_immediate draws a single instance with its transform in the instance constants (materials that cannot
// combine draw calls).

#include "volition/render_lib/code/rl_compiled_render_group.h"

#include "volition/render_lib/code/rl_batch_defines.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_gpu_visibility.h"
#include "volition/render_lib/code/rl_instance_stream.h"
#include "volition/render_lib/code/rl_metrics.h"
#include "volition/render_lib/code/rl_render_group.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_metrics.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "library/modules/runtime/graphics/sources/d3d11/rendergeometry_d3d11.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/zoneallocator.hpp"
#include "keen/graphics/graphicscommandwriter.hpp"
#include "keen/graphics/rendergeometry.hpp"
#include "keen/math/matrix43.hpp"

#include <algorithm>
#include <string.h>

// <rl_compiled_render_group::factory_init> 0x0050AB40 (:112)
// <rl_compiled_render_group::release> 0x0050AC40 (:112)
// <rl_compiled_render_group::create> 0x00522070 (:112)
RL_FACTORY_CLASS_IMPLEMENTATION(rl_compiled_render_group, 0x28BA6A06, 1024)

// Sort the instances of a group by render block, so that one block's instances share a draw (in retail on).
bool RL_sort_compiled_render_group_instances = true; // 0x014FA102

// The per-frame render call memory (0x60000 bytes, 0x4000 calls of 0x18 bytes).
static keen::MemoryBlock Call_pool_memBlock; // 0x01B69A48
static keen::ZoneAllocator Call_pool; // 0x01B7BD58
static const unsigned int CALL_POOL_SIZE = 0x60000;
static const int CALL_POOL_MAX_CALLS = 0x4000;
static volatile unsigned int Render_call_watermark = 0; // 0x01B7BC54
static unsigned long long Rl_compiled_render_group_total_instances = 0; // 0x01B7BC58
static unsigned long long Rl_compiled_render_group_total_calls = 0; // 0x01B7BC60

// The thread's instance stream for render_immediate (TLS slot 0x24, between render_immediate_begin and _end).
static __declspec(thread) const rl_gpu_mesh_instance* Render_immediate_instances = nullptr;

// 0x004BA740
rl_render_group::rl_render_group()
{
}

// 0x004BC7E0 (:100)
bool sort_by_block_id::operator()(const rl_batched_instance& pA, const rl_batched_instance& pB) const
{
    return pA.block_id < pB.block_id;
}

// 0x004BC800 (:115)
rl_compiled_render_group::rl_compiled_render_group()
{
    m_is_built = false;
    m_render_calls = nullptr;
}

// 0x004BC820 (:124)
rl_compiled_render_group::~rl_compiled_render_group()
{
}

// 0x005559B0 (:145)
// Notes (decompiled behaviour, kept as is): a full call pool returns (metrics warning 0x800) with the instancing
// batch still open and the group not built; an instance whose transform did not fit the stream (index 0xFFFF) is
// copied from past the stream's end.
void rl_compiled_render_group::build(rl_renderer& renderer, rl_render_group* group, const rl_mesh_instance_stream* inst_stream)
{
    mempool_mark_scope scope(Vlib_scratch_pool);
    m_vbuffer_info = &group->vbuffer_info;
    m_ibuffer_info = &group->ibuffer_info;
    m_geometry_p = group->geometry_p;
    if (RL_sort_compiled_render_group_instances)
    {
        std::sort(group->instances, group->instances + group->num_instances, sort_by_block_id());
    }
    const rl_gpu_mesh_instance* stream_instances = inst_stream->get_instances();
    if (stream_instances == nullptr)
    {
        m_is_built = false;
        return;
    }
    m_instance_offset = 0;
    unsigned char* batch_p = (unsigned char*)renderer.begin_instancing_batch(&m_instancing_batch, group->num_instances, sizeof(rl_gpu_mesh_instance));
    if (batch_p != nullptr)
    {
        rl_compiled_render_call* call_p = nullptr;
        unsigned int block_id = 0xFFFFFFFF;
        int offset = 0;
        for (unsigned int i = 0; i < group->num_instances; i++)
        {
            const rl_batched_instance& inst = group->instances[i];
            if (inst.block_id != block_id || call_p == nullptr)
            {
                call_p = (rl_compiled_render_call*)Call_pool.allocate(sizeof(rl_compiled_render_call), 8);
                _InterlockedIncrement((volatile long*)&Render_call_watermark);
                if (call_p == nullptr)
                {
                    rl_render_lib::get_instance()->m_metrics_p->set_warning_flag(0x800);
                    return;
                }
                block_id = inst.block_id;
                call_p->m_first_index = inst.first_index;
                call_p->m_instance_offset = offset;
                call_p->m_num_indices = inst.num_indices;
                call_p->m_num_instances = 0;
                if (m_render_calls == nullptr)
                {
                    call_p->next = call_p;
                    call_p->prev = call_p;
                    m_render_calls = call_p;
                }
                else
                {
                    call_p->prev = m_render_calls->prev;
                    call_p->next = m_render_calls;
                    m_render_calls->prev->next = call_p;
                    m_render_calls->prev = call_p;
                }
            }
            memcpy(batch_p + offset, &stream_instances[inst.transform_index], sizeof(rl_gpu_mesh_instance));
            call_p->m_num_instances++;
            offset += sizeof(rl_gpu_mesh_instance);
        }
        renderer.end_instancing_batch();
    }
    m_is_built = true;
}

// 0x004BC830
// The vertex format of a mesh's vertex buffer info byte (the rl_dev_vertex_format values in another numbering).
rl_dev_vertex_format get_vertex_format(const unsigned char val)
{
    switch (val)
    {
    case 4:
        return RLVF_UNLIT;
    case 5:
        return RLVF_PARTICLE_POINTSPRITE;
    case 6:
        return RLVF_PARTICLE_BILLBOARD;
    case 7:
        return RLVF_PARTICLE_RADIAL;
    case 8:
        return RLVF_PARTICLE_DROP;
    case 9:
        return RLVF_PARTICLE_RIBBON;
    case 10:
        return RLVF_PARTICLE_ORIENTED;
    case 0xB:
        return RLVF_PRIMITIVE_3D;
    case 0xC:
        return RLVF_PRIMITIVE_2D;
    case 0xD:
        return RLVF_SG_MESH;
    case 0xE:
        return RLVF_HEIGHT_MESH;
    case 0xF:
        return RLVF_HEIGHT_MESH_LOW_LOD;
    case 0x10:
        return RLVF_PARTICLE_PARAMETRIC;
    case 0x11:
        return RLVF_COMPOSITOR;
    case 0x13:
        return RLVF_CLONE_NMAP;
    case 0x14:
        return RLVF_CLONE_CLR;
    case 0x15:
        return RLVF_2D_SPLINE;
    case 0x16:
        return RLVF_PARTICLE_CORONA;
    case 0x17:
        return RLVF_PARTICLE_RIBBON_PARAMETRIC;
    case 0x18:
        return RLVF_CONDITIONAL_BBOX;
    case 0x19:
        return RLVF_TERRAIN_ROAD;
    case 0x1F:
        return RLVF_PIXLIT_1UV;
    case 0x20:
        return RLVF_PIXLIT_1UV_CA;
    case 0x21:
        return RLVF_PIXLIT_1UV_NMAP;
    case 0x22:
        return RLVF_PIXLIT_1UV_NMAP_CA;
    case 0x23:
        return RLVF_PIXLIT_2UV;
    case 0x24:
        return RLVF_PIXLIT_2UV_CA;
    case 0x25:
        return RLVF_PIXLIT_2UV_NMAP;
    case 0x26:
        return RLVF_PIXLIT_2UV_NMAP_CA;
    case 0x27:
        return RLVF_PIXLIT_3UV;
    case 0x28:
        return RLVF_PIXLIT_3UV_CA;
    case 0x29:
        return RLVF_PIXLIT_3UV_NMAP;
    case 0x2A:
        return RLVF_PIXLIT_3UV_NMAP_CA;
    case 0x2B:
        return RLVF_PIXLIT_4UV;
    case 0x2C:
        return RLVF_PIXLIT_4UV_CA;
    case 0x2D:
        return RLVF_PIXLIT_4UV_NMAP;
    case 0x2E:
        return RLVF_PIXLIT_4UV_NMAP_CA;
    case 0x2F:
        return RLVF_CLONE_1UV_UVS;
    case 0x30:
        return RLVF_CLONE_2UV_UVS;
    default:
        VLIB_CRITICAL_FAILURE();
        return RLVF_INVALID;
    }
}

// 0x005220E0 (:593)
// One instanced draw per call: the geometry's vertex buffer in stream 0 (streams 1-3 empty) and the call's range of
// the instancing batch.
void rl_compiled_render_group::render(rl_renderer& renderer)
{
    if (!m_is_built)
    {
        return;
    }
    renderer.set_vertex_format(get_vertex_format(m_vbuffer_info->vertex_format));
    rl_compiled_render_call* call_p = m_render_calls;
    while (call_p != nullptr)
    {
        Rl_compiled_render_group_total_instances += call_p->m_num_instances;
        Rl_compiled_render_group_total_calls++;
        keen::VertexBufferData vertex_buffers[4];
        memset(vertex_buffers, 0, sizeof(vertex_buffers));
        const keen::RenderGeometry* geometry_p = m_geometry_p;
        vertex_buffers[0].pBuffer = geometry_p->pVertexBuffer;
        keen::IndexBufferData index_buffer;
        index_buffer.pBuffer = geometry_p->pIndexBuffer;
        index_buffer.indexCount = geometry_p->indexCount;
        index_buffer.offset = m_ibuffer_info->index_size * call_p->m_first_index;
        index_buffer.indexFormat = geometry_p->indexFormat;
        keen::PrimitiveData primitive;
        primitive.primitiveTopology = geometry_p->primitiveTopology;
        primitive.vertexDrawCount = call_p->m_num_indices;
        if (primitive.vertexDrawCount == 0)
        {
            primitive.vertexDrawCount = geometry_p->pIndexBuffer == nullptr ? geometry_p->vertexCount : geometry_p->indexCount;
        }
        keen::GeometryInstancingBatch batch;
        keen::graphics::fillGeometryInstancingBatch(&batch, &m_instancing_batch, call_p->m_instance_offset, call_p->m_num_instances);
        renderer.flush_states();
        renderer.m_pCommandWriter->writeDrawInstancedVertexBuffersCommand(vertex_buffers, 4, &index_buffer, &primitive, &batch);
        call_p = call_p->next;
        if (call_p == m_render_calls)
        {
            break;
        }
    }
}

// 0x0050ACC0 (:666)
void rl_compiled_render_group::render_immediate_begin(rl_renderer& renderer, rl_render_group* group, const rl_mesh_instance_stream* inst_stream)
{
    renderer.set_vertex_format((rl_dev_vertex_format)group->vbuffer_info.vertex_format);
    Render_immediate_instances = inst_stream->get_instances();
}

// 0x00522330 (:693)
// One instance: its transform rows and parameters in the instance vertex constants, then the block's draw.
// Notes (decompiled behaviour, kept as is): the parameter bytes are multiplied by 255 instead of divided.
void rl_compiled_render_group::render_immediate(rl_renderer& renderer, rl_render_group* grp_p, rl_batched_instance* inst_p)
{
    const rl_gpu_mesh_instance& instance = Render_immediate_instances[inst_p->transform_index];
    const unsigned char* params = (const unsigned char*)&instance.instance_params;
    memcpy(&renderer.m_instanceVertexData.objTM, instance.transform, sizeof(instance.transform));
    renderer.m_constantDataDirtyMask |= rl_renderer::ConstantDataMask_InstanceVertexData;
    // Magic number: 255 (see the note).
    renderer.m_instanceVertexData.instanceData.x = (float)(params[2] * 255.0);
    renderer.m_instanceVertexData.instanceData.z = (float)(params[0] * 255.0);
    renderer.m_instanceVertexData.instanceData.y = (float)(params[1] * 255.0);
    renderer.m_instanceVertexData.instanceData.w = (float)(params[3] * 255.0);
    renderer.m_constantDataDirtyMask |= rl_renderer::ConstantDataMask_InstanceVertexData;
    const keen::RenderGeometry* geometry_p = grp_p->geometry_p;
    keen::VertexBufferData vertex_buffer;
    vertex_buffer.pBuffer = geometry_p->pVertexBuffer;
    vertex_buffer.vertexCount = geometry_p->vertexCount;
    vertex_buffer.offset = 0;
    keen::IndexBufferData index_buffer;
    index_buffer.pBuffer = geometry_p->pIndexBuffer;
    index_buffer.indexCount = geometry_p->indexCount;
    // DXGI_FORMAT_R32_UINT (0x2A): 4-byte indices, else 2.
    index_buffer.offset = (geometry_p->indexFormat == 0x2A ? 4 : 2) * inst_p->first_index;
    index_buffer.indexFormat = geometry_p->indexFormat;
    keen::PrimitiveData primitive;
    primitive.primitiveTopology = geometry_p->primitiveTopology;
    primitive.vertexDrawCount = inst_p->num_indices;
    if (primitive.vertexDrawCount == 0)
    {
        primitive.vertexDrawCount = geometry_p->pIndexBuffer == nullptr ? geometry_p->vertexCount : geometry_p->indexCount;
    }
    renderer.flush_states();
    renderer.m_pCommandWriter->writeDrawVertexBuffersCommand(keen::Matrix43(), &vertex_buffer, 1, &index_buffer, &primitive);
}

// 0x004BCA00 (:736)
void rl_compiled_render_group::render_immediate_end(rl_renderer&)
{
    Render_immediate_instances = nullptr;
}

// 0x004BA720 (.h:40) and 0x004BA730 (.h:40) are the factory macro's get_object_pool and get_objects_max.

// 0x004BCA20 (:752)
void rl_compiled_render_group_get_call_stats(int& high_watermark, int& max_capacity)
{
    high_watermark = Render_call_watermark;
    max_capacity = CALL_POOL_MAX_CALLS;
}

// 0x004BCA40 (:758)
void rl_compiled_render_group_reset_call_stats()
{
    Render_call_watermark = 0;
}

// 0x004E3690 (:763)
void rl_compiled_render_group_init(keen::MemoryAllocator* pAllocator)
{
    Call_pool_memBlock.size = CALL_POOL_SIZE;
    Call_pool_memBlock.pStart = (uint8*)pAllocator->allocate(CALL_POOL_SIZE, 0x10, 0, nullptr);
    if (Call_pool_memBlock.pStart != nullptr)
    {
        Call_pool.create(Call_pool_memBlock);
    }
}

// 0x004BCA50 (:770)
void rl_compiled_render_group_reset()
{
    Call_pool.clear();
}
