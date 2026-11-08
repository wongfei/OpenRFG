// Original: volition/render_lib/code/rl_fog_volume.cpp
//
// Partial: the shared box mesh (system_init/system_shutdown) and the factory's registration and pool
// (final_push.md step 5): rl_init reserves the pool in the factory mempool as rfg.exe does. The instances are not
// ported: nothing in rfg.exe creates a fog volume (create, the constructor and the virtuals are dead), so the factory
// is written out without RL_FACTORY_CLASS_CORE, whose create() would need them.

#include "volition/render_lib/code/rl_fog_volume.h"

#include "volition/render_lib/code/rl_device.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "keen/base/memoryallocator.hpp"

static_assert(sizeof(rl_fog_volume) == 0x180, "rl_fog_volume is 0x180 bytes in rfg.exe (its factory pool stride)");

// :11, the factory class expansion. The class id, name, size and the default maximum (256; game_render_system_init
// sets 1) are rfg.exe's constant-initialized rl_class_factory_info at 0x01514AB0.
const unsigned int rl_fog_volume::m___class_id = 0xF047073E; // 0x012BEDE0
pool_list<rl_fog_volume*> rl_fog_volume::ms_object_pool; // 0x01514AE0
rl_class_factory_info rl_fog_volume::ms_class_factory_info("rl_fog_volume", 0xF047073E, &rl_fog_volume::factory_init, sizeof(rl_fog_volume), (const pool_list<rl_base_object*>*)&rl_fog_volume::ms_object_pool, 256); // 0x01514AB0

// 0x004BEC10 (.h:23)
const pool_list<rl_fog_volume*>& rl_fog_volume::get_object_pool()
{
    return ms_object_pool;
}

// 0x007C1F20 (.h:23)
void rl_fog_volume::set_objects_max(int objects_max)
{
    ms_class_factory_info.m_objects_max = objects_max;
}

// 0x004BEC20 (.h:23)
int rl_fog_volume::get_objects_max()
{
    return ms_class_factory_info.m_objects_max;
}

// 0x0050E030 (:11)
// The pool's element array, then the objects (16-byte aligned, not constructed), from the factory mempool.
void rl_fog_volume::factory_init(mempool& factory_mempool)
{
    ms_class_factory_info.m_factory_mempool_p = &factory_mempool;
    rl_fog_volume** elements = (rl_fog_volume**)factory_mempool.alloc(ms_class_factory_info.m_objects_max * sizeof(rl_fog_volume*), 4);
    ms_object_pool.init(elements, ms_class_factory_info.m_objects_max, 0);
    // Magic number: objects are 16-byte aligned in the factory mempool.
    rl_fog_volume* objects = (rl_fog_volume*)factory_mempool.alloc(ms_class_factory_info.m_objects_max * sizeof(rl_fog_volume), 16);
    for (int i = 0; i < ms_class_factory_info.m_objects_max; i++)
    {
        ms_object_pool.add(&objects[i]);
    }
    ms_class_factory_info.m_object_data_base = (unsigned char*)objects;
    ms_class_factory_info.m_object_data_size = ms_class_factory_info.m_objects_max * sizeof(rl_fog_volume);
}

const int rl_fog_volume::BOX_POSITIONS_COUNT = 8; // 0x012BEDE4
const int rl_fog_volume::BOX_INDEX_COUNT = 36; // 0x012BEDE8
rl_fog_volume::BoxVertex* rl_fog_volume::ms_box_positions = nullptr; // 0x01B7BC9C
unsigned short* rl_fog_volume::ms_box_indices = nullptr; // 0x01B7BC98
rl_mesh* rl_fog_volume::ms_box_mesh_p = nullptr; // 0x01B7BC88
rl_mesh_data* rl_fog_volume::ms_mesh_data = nullptr; // 0x01B7BC8C
rl_submesh_data* rl_fog_volume::ms_submesh = nullptr; // 0x01B7BC90
rl_render_block* rl_fog_volume::ms_renderblock = nullptr; // 0x01B7BC94
bool rl_fog_volume::ms_initialized = false; // 0x01B7BC84

// 0x0053AB50 (:276)
// A unit box (corners at +-0.5) mesh in the compositor vertex format, 12 triangles.
// Notes (decompiled behaviour, kept as is): the mesh draws 37 indices of the 36 in the index buffer, the uvs are
// uninitialized, and the mesh data objects are not freed by system_shutdown.
void rl_fog_volume::system_init()
{
    keen::MemoryAllocator* allocator_p = Init_params.m_pBufferAllocator;
    const unsigned short indices[36] = {0, 1, 2, 2, 1, 3, 1, 5, 3, 3, 5, 7, 2, 4, 0, 6, 4, 2, 6, 5, 4, 7, 5, 6, 0, 4, 5, 0, 5, 1, 7, 6, 2, 3, 7, 2};
    ms_box_positions = (BoxVertex*)allocator_p->allocate(8 * sizeof(BoxVertex), 16, 0, nullptr);
    const float corners[8][3] = {{-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {-0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}};
    for (int i = 0; i < 8; i++)
    {
        ms_box_positions[i].pos.x = corners[i][0];
        ms_box_positions[i].pos.y = corners[i][1];
        ms_box_positions[i].pos.z = corners[i][2];
        ms_box_positions[i].pos.w = 1.0f;
    }
    ms_box_indices = (unsigned short*)allocator_p->allocate(sizeof(indices), 16, 0, nullptr);
    for (int i = 0; i < 36; i++)
    {
        ms_box_indices[i] = indices[i];
    }
    ms_box_mesh_p = rl_mesh::create();
    ms_mesh_data = new rl_mesh_data();
    ms_submesh = new rl_submesh_data();
    ms_renderblock = new rl_render_block;
    ms_mesh_data->index_buffer.index_size = 2;
    ms_mesh_data->index_buffer.num_blocks = 1;
    // Magic number: 37 indices (one more than the box has).
    ms_mesh_data->index_buffer.num_indices = 0x25;
    ms_mesh_data->index_buffer.prim_type = 1;
    ms_mesh_data->num_sub_meshes = 1;
    ms_mesh_data->vertex_buffer.num_uv_channels = 0;
    ms_mesh_data->vertex_buffer.num_verts = 8;
    ms_mesh_data->vertex_buffer.vert_stride_0 = sizeof(BoxVertex);
    ms_mesh_data->vertex_buffer.vert_stride_1 = 0;
    ms_mesh_data->vertex_buffer.vertex_format = RLVF_COMPOSITOR;
    ms_submesh->bmin.set(-0.5f, -0.5f, -0.5f);
    ms_submesh->bmax.set(0.5f, 0.5f, 0.5f);
    ms_submesh->num_render_blocks = 1;
    ms_submesh->offset_transform = ZERO_VECTOR;
    ms_submesh->render_blocks.from_pointer(ms_renderblock);
    ms_renderblock->material_map_idx = 0;
    ms_renderblock->maximum_index = 8;
    ms_renderblock->minimum_index = 0;
    ms_renderblock->num_indices = 0x25;
    ms_renderblock->start_index = 0;
    ms_mesh_data->sub_meshes.from_pointer(ms_submesh);
    rl_mesh_data_runtime data;
    data.num_sub_meshes = ms_mesh_data->num_sub_meshes;
    data.sub_meshes = ms_mesh_data->sub_meshes.to_pointer();
    data.vertex_buffer.num_verts = ms_mesh_data->vertex_buffer.num_verts;
    data.vertex_buffer.vert_stride_0 = ms_mesh_data->vertex_buffer.vert_stride_0;
    data.vertex_buffer.vertex_format = ms_mesh_data->vertex_buffer.vertex_format;
    data.vertex_buffer.num_uv_channels = ms_mesh_data->vertex_buffer.num_uv_channels;
    data.vertex_buffer.vert_stride_1 = ms_mesh_data->vertex_buffer.vert_stride_1;
    data.vertex_buffer.verts = (unsigned char*)ms_box_positions;
    data.index_buffer.num_indices = ms_mesh_data->index_buffer.num_indices;
    data.index_buffer.index_size = ms_mesh_data->index_buffer.index_size;
    data.index_buffer.prim_type = ms_mesh_data->index_buffer.prim_type;
    data.index_buffer.num_blocks = ms_mesh_data->index_buffer.num_blocks;
    data.index_buffer.indices = (unsigned char*)ms_box_indices;
    ms_box_mesh_p->init(&data, "FogVolume", false);
    ms_initialized = true;
}

// 0x004E5070 (:444)
void rl_fog_volume::system_shutdown()
{
    keen::MemoryAllocator* allocator_p = Init_params.m_pBufferAllocator;
    allocator_p->free(ms_box_positions);
    allocator_p->free(ms_box_indices);
    ms_box_mesh_p->release();
    ms_initialized = false;
}
