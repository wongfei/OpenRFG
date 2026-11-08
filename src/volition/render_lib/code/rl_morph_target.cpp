// Original: volition/render_lib/code/rl_morph_target.cpp
//
// rl_morph_container: the morph targets of a mesh (stub_impl.md S3). Format 0 (vbuffer) targets are one vertex buffer
// bound as stream 1 while a morphed mesh renders; format 1 (compressed) targets are applied on the CPU by
// rl_dynamic_morph_instance.

#include "volition/render_lib/code/rl_morph_target.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_morph_data.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "keen/graphics/rendergeometry.hpp"
#include "keen/graphics/vertexattribute.hpp"
#include "keen/graphics/vertexformat.hpp"

#include <new>

// The class factory (:7): class id 0x3052C073, 100 containers.
// <rl_morph_container::factory_init> 0x00513390 (:7)
// <rl_morph_container::release> 0x00513490 (:7)
// <rl_morph_container::create> 0x00529480 (:7)
RL_FACTORY_CLASS_IMPLEMENTATION(rl_morph_container, 0x3052C073, 100)

// 0x007B99A0 (.h:58)
// game_render_system_init sets 0x107 (rfg.exe writes the maximum inline).
void rl_morph_container::set_objects_max(int objects_max)
{
    ms_class_factory_info.m_objects_max = objects_max;
}

// The first target of a format 0 container's data.
static rl_morph_target_vbuffer* rl_morph_vbuffer_targets(rl_morph_data* data)
{
    return (rl_morph_target_vbuffer*)(unsigned char*)data->targets;
}

// The first target of a format 1 container's data.
static rl_morph_target_compressed* rl_morph_compressed_targets(rl_morph_data* data)
{
    return (rl_morph_target_compressed*)(unsigned char*)data->targets;
}

// 0x00513510 (:11)
// The vbuffer targets' vertex buffer: per vertex a TexCoord4 x16y16z16w16_snorm (the position delta) and a BiTangent
// x8y8z8w8_unorm (the normal delta).
void rl_morph_container::create_render_data()
{
    keen::GraphicsSystem* graphics_system = Init_params.m_pKeenGraphicsSystem;
    const rl_morph_target_vbuffer* target = rl_morph_vbuffer_targets(m_morph_data);
    keen::VertexAttributeDescription attributes[2];
    attributes[0].id = keen::VertexAttributeId_TexCoord4;
    attributes[0].format = keen::VertexAttributeFormat_x16y16z16w16_snorm;
    attributes[0].inputStreamIndex = 0;
    attributes[0].instanceStepRate = 0;
    attributes[1].id = keen::VertexAttributeId_BiTangent;
    attributes[1].format = keen::VertexAttributeFormat_x8y8z8w8_unorm;
    attributes[1].inputStreamIndex = 0;
    attributes[1].instanceStepRate = 0;
    keen::RenderGeometryInitializationData init_data;
    init_data.pIndexData = nullptr;
    init_data.pSkinningData = nullptr;
    init_data.pPlatformData = nullptr;
    init_data.pMorphingData = nullptr;
    init_data.pVertexData = (void*)(uintptr_t)target->verts; // the vertex data loaded with the container (gpu data)
    keen::RenderGeometryDescription description;
    description.cpuAccessMode = 0;
    description.vertexCount = target->num_morph_verts;
    description.pVertexFormat = keen::graphics::createVertexFormat(graphics_system, attributes, 2);
    description.indexCount = 0;
    description.indexFormat = keen::IndexFormat_Invalid;
    description.primitiveType = keen::PrimitiveType_TriangleList;
    description.streamIndex = 0;
    keen::graphics::initializeRenderGeometry(&m_renderGeometry, graphics_system, description, init_data, "MorphContainer");
    m_vertexBufferData.offset = 0;
    m_vertexBufferData.pBuffer = m_renderGeometry.pVertexBuffer;
    m_vertexBufferData.vertexCount = m_renderGeometry.vertexCount;
    m_renderGeometryValid = true;
}

// 0x004E9D00 (:49)
void rl_morph_container::release_render_data()
{
    if (!m_renderGeometryValid)
    {
        return;
    }
    const keen::VertexFormat* format = m_renderGeometry.pVertexFormat;
    m_renderGeometryValid = false;
    keen::GraphicsSystem* graphics_system = Init_params.m_pKeenGraphicsSystem;
    keen::graphics::destroyVertexFormat(graphics_system, format);
    keen::graphics::freeRenderGeometry(&m_renderGeometry, graphics_system, nullptr);
}

// 0x004C3870 (:65)
rl_morph_container::rl_morph_container()
{
    m_morph_data = nullptr;
    m_renderGeometryValid = false;
    m_debug_signature = 0x600DF00D; // magic number: the live object signature
    m_max_vertex_index = 0;
}

// 0x004E9D30 (:76)
rl_morph_container::~rl_morph_container()
{
    release_render_data(); // inlined
    m_debug_signature = 0x0BADF00D; // magic number: the released object signature
    m_max_vertex_index = 0;
}

// 0x00513640 (:89)
// Format 0: the targets' vertex buffer. Format 1: the largest vertex index any compressed target maps to.
void rl_morph_container::set_morph_data(rl_morph_data* morph_data)
{
    m_morph_data = morph_data;
    m_max_vertex_index = 0;
    if (morph_data->target_format == RL_MTF_VBUFFER)
    {
        create_render_data();
        return;
    }
    rl_morph_target_compressed* targets = rl_morph_compressed_targets(morph_data);
    for (int t = 0; t < m_morph_data->num_targets; t++)
    {
        const rl_morph_vert_compressed* verts = (rl_morph_vert_compressed*)targets[t].verts;
        for (unsigned int i = 0; i < targets[t].num_morph_verts; i++)
        {
            if (m_max_vertex_index <= (int)verts[i].vb_mapping)
            {
                m_max_vertex_index = verts[i].vb_mapping;
            }
        }
    }
}

// 0x004E9D80 (:112)
// The index of the target whose name checksum matches; -1 when none does.
int rl_morph_container::find_morph_target(unsigned int name_checksum)
{
    rl_morph_data* data = m_morph_data;
    for (int i = 0; i < data->num_targets; i++)
    {
        const unsigned int name = data->target_format == RL_MTF_VBUFFER ? rl_morph_vbuffer_targets(data)[i].name_chksum : rl_morph_compressed_targets(data)[i].name_chksum;
        if (name == name_checksum)
        {
            return i;
        }
    }
    return -1;
}

// 0x004E9DF0 (:133)
unsigned char* rl_morph_container::get_morph_target(int index)
{
    if (index < 0 || index >= m_morph_data->num_targets)
    {
        return nullptr;
    }
    if (m_morph_data->target_format == RL_MTF_VBUFFER)
    {
        return (unsigned char*)&rl_morph_vbuffer_targets(m_morph_data)[index];
    }
    return (unsigned char*)&rl_morph_compressed_targets(m_morph_data)[index];
}

// 0x004E9E30 (:156)
int rl_morph_container::get_num_morph_verts(int target)
{
    if (m_morph_data->target_format == RL_MTF_VBUFFER)
    {
        return (int)rl_morph_vbuffer_targets(m_morph_data)[target].num_morph_verts;
    }
    return (int)rl_morph_compressed_targets(m_morph_data)[target].num_morph_verts;
}

// 0x005136E0 (:173)
// Format 0 only: the targets as vertex stream 1.
bool rl_morph_container::render_setup(rl_renderer& renderer)
{
    if (m_morph_data->target_format != RL_MTF_VBUFFER)
    {
        return false;
    }
    renderer.m_vertexBufferData[1] = m_vertexBufferData;
    return true;
}

// 0x00513730 (:205)
bool rl_morph_container::render_teardown(rl_renderer& renderer)
{
    if (m_morph_data->target_format != RL_MTF_VBUFFER)
    {
        return false;
    }
    renderer.m_vertexBufferData[1].pBuffer = nullptr;
    renderer.m_vertexBufferData[1].vertexCount = 0;
    renderer.m_vertexBufferData[1].offset = 0;
    return true;
}

// 0x004C3890 (:216)
int rl_morph_container::get_max_vertex_index()
{
    return m_max_vertex_index;
}
