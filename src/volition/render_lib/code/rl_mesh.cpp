// Original: volition/render_lib/code/rl_mesh.cpp
//
// rl_mesh: a static mesh, its runtime mesh data (submeshes and render blocks) and the keen RenderGeometry built from
// the vertex and index data.

#include "volition/render_lib/code/rl_mesh.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "keen/graphics/rendergeometry.hpp"

#include <math.h>
#include <string.h>
#include <windows.h>

// The uid of the last mesh created.
static volatile LONG Next_uid; // 0x01B7BCCC

// :7
// <rl_mesh::factory_init> 0x00511610 (:7)
// <rl_mesh::release> 0x00511830 (:7)
// <rl_mesh::create> 0x00527E20 (:7)
RL_FACTORY_CLASS_IMPLEMENTATION(rl_mesh, 0x1771352C, 2048)

// 0x004E9390 (:11)
rl_mesh::rl_mesh()
{
    m_uid = (unsigned int)InterlockedIncrement(&Next_uid);
    m_max_material_index = 0;
}

// 0x004E94A0 (:17)
rl_mesh::~rl_mesh()
{
    keen::graphics::freeRenderGeometry(&m_geometry, Init_params.m_pKeenGraphicsSystem, nullptr);
}

// 0x005118F0 (:33)
// The bounding volume is reset to a unit box (rl_renderable's default) and the data copied.
void rl_mesh::set_mesh_data(rl_mesh_data_runtime* new_data)
{
    m_local_bvolume.m_half_vec.set(1.0f, 1.0f, 1.0f);
    m_local_bvolume.m_sphere.m_position.set(m_local_bvolume.m_half_vec.x - 1.0f, m_local_bvolume.m_half_vec.y - 1.0f, m_local_bvolume.m_half_vec.z - 1.0f);
    m_local_bvolume.m_sphere.m_radius = sqrtf(m_local_bvolume.m_half_vec.y * m_local_bvolume.m_half_vec.y + m_local_bvolume.m_half_vec.x * m_local_bvolume.m_half_vec.x + m_local_bvolume.m_half_vec.z * m_local_bvolume.m_half_vec.z);
    m_data = *new_data;
    m_data_set = true;
    if (m_data.num_sub_meshes > 0)
    {
        m_base_render_block = m_data.sub_meshes[0].render_blocks.to_pointer();
        return;
    }
    m_base_render_block = nullptr;
}

// 0x004C2DA0 (:55)
// The bounds of a submesh, or a unit box.
void rl_mesh::get_subpiece_local_bb(int index, vector& bmin, vector& bmax) const
{
    if (m_data_set && index < m_data.num_sub_meshes)
    {
        bmin = m_data.sub_meshes[index].bmin;
        bmax = m_data.sub_meshes[index].bmax;
        return;
    }
    bmin.set(-0.5f, -0.5f, -0.5f);
    bmax.set(0.5f, 0.5f, 0.5f);
}

// 0x00527E80 (:66)
// The vertex and index data come from the loaded file.
void rl_mesh::init(rl_mesh_data* new_data, const LoadedMeshInfo& meshInfo, bool)
{
    rl_mesh_data_runtime data;
    data.num_sub_meshes = new_data->num_sub_meshes;
    data.sub_meshes = new_data->sub_meshes.to_pointer();
    data.vertex_buffer.num_verts = new_data->vertex_buffer.num_verts;
    data.vertex_buffer.vert_stride_0 = new_data->vertex_buffer.vert_stride_0;
    data.vertex_buffer.vertex_format = new_data->vertex_buffer.vertex_format;
    data.vertex_buffer.num_uv_channels = new_data->vertex_buffer.num_uv_channels;
    data.vertex_buffer.vert_stride_1 = new_data->vertex_buffer.vert_stride_1;
    data.vertex_buffer.verts = (unsigned char*)meshInfo.pVertexBuffer;
    data.index_buffer.num_indices = new_data->index_buffer.num_indices;
    data.index_buffer.index_size = new_data->index_buffer.index_size;
    data.index_buffer.prim_type = new_data->index_buffer.prim_type;
    data.index_buffer.num_blocks = new_data->index_buffer.num_blocks;
    data.index_buffer.indices = (unsigned char*)meshInfo.pIndexBuffer;
    init(&data, meshInfo.name, false);
}

// 0x00511A10 (:72)
// Creates the geometry (16-bit indices for index size 2, else 32-bit; prim_type 1 is a triangle list, else a strip).
// A mesh with an unknown vertex format gets a cleared geometry.
void rl_mesh::init(rl_mesh_data_runtime* new_data, const char* pDebugName, bool)
{
    set_mesh_data(new_data);
    const unsigned int num_indices = new_data->index_buffer.num_indices;
    if (num_indices == 0 && new_data->vertex_buffer.num_verts == 0)
    {
        return;
    }
    keen::RenderGeometryDescription description;
    description.primitiveType = keen::PrimitiveType_Invalid;
    description.pVertexFormat = nullptr;
    description.indexFormat = keen::IndexFormat_Uint16;
    description.vertexCount = 0;
    description.indexCount = 0;
    description.cpuAccessMode = 0;
    description.streamIndex = 0;
    keen::RenderGeometryInitializationData init_data;
    init_data.pVertexData = nullptr;
    init_data.pIndexData = nullptr;
    init_data.pSkinningData = nullptr;
    init_data.pPlatformData = nullptr;
    init_data.pMorphingData = nullptr;
    if (new_data->vertex_buffer.num_verts != 0)
    {
        init_data.pVertexData = new_data->vertex_buffer.verts;
        description.vertexCount = new_data->vertex_buffer.num_verts;
        description.pVertexFormat = rl_render_lib::get_instance()->m_vertex_formats_p[new_data->vertex_buffer.vertex_format];
        if (description.pVertexFormat == nullptr)
        {
            memset(&m_geometry, 0, sizeof(m_geometry));
            return;
        }
    }
    if (num_indices != 0)
    {
        init_data.pIndexData = new_data->index_buffer.indices;
        description.indexFormat = new_data->index_buffer.index_size != 2 ? keen::IndexFormat_Uint32 : keen::IndexFormat_Uint16;
        description.indexCount = num_indices;
    }
    // Magic number: prim_type 1 is a triangle list; anything else a strip.
    description.primitiveType = new_data->index_buffer.prim_type != 1 ? keen::PrimitiveType_TriangleStrip : keen::PrimitiveType_TriangleList;
    if (pDebugName == nullptr)
    {
        pDebugName = "rl_mesh";
    }
    keen::graphics::initializeRenderGeometry(&m_geometry, Init_params.m_pKeenGraphicsSystem, description, init_data, pDebugName);
}

// 0x007B9980 (.h:30)
void rl_mesh::set_objects_max(int objects_max)
{
    ms_class_factory_info.m_objects_max = objects_max;
}

// 0x006E6C80 (.h:30)
int rl_mesh::get_objects_free()
{
    return ms_object_pool.size();
}

// 0x004BA4A0 (.h:37)
rl_mesh_data_runtime* rl_mesh::get_mesh_data()
{
    return m_data_set ? &m_data : nullptr;
}

// 0x004BA4B0 (.h:38)
const rl_mesh_data_runtime* rl_mesh::get_mesh_data() const
{
    return m_data_set ? &m_data : nullptr;
}

// 0x004BA4C0 (.h:44)
unsigned int rl_mesh::get_uid() const
{
    return m_uid;
}

// 0x004E9490 (.h:48)
void rl_mesh::serialize(rl_serial_block&, bool)
{
}

// 0x004BA4D0 (.h:50)
const keen::RenderGeometry& rl_mesh::getRenderGeometry() const
{
    return m_geometry;
}

// 0x004BA4E0 (.h:73)
rl_render_block* rl_mesh::get_base_render_block() const
{
    return m_base_render_block;
}
