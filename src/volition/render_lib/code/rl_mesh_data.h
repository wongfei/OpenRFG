// Original: volition/render_lib/code/rl_mesh_data.h
// Implementation: volition/render_lib/code/rl_mesh_data.cpp
//
// Serialized mesh data (rl_mesh_data: vertex/index buffer descriptions and submeshes) and the runtime copies
// (rl_*_runtime) created on load. LoadedMeshInfo describes a mesh in a loaded file.
#pragma once

#include "volition/render_lib/code/rl_batch_defines.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/endian_type.h"

// size 0x48
struct LoadedMeshInfo
{
    LoadedMeshInfo(const char* pName, const char* pPostfix); // 0x004B6C80 (.h:82)

    char name[64]; // 0x00
    const void* pVertexBuffer; // 0x40
    const void* pIndexBuffer; // 0x44
};

// size 0xC
struct rl_vertex_buffer_data
{
    rl_vertex_buffer_data(); // 0x004E5050

    int num_verts; // 0x00
    unsigned char vert_stride_0; // 0x04
    unsigned char vertex_format; // 0x05
    unsigned char num_uv_channels; // 0x06
    unsigned char vert_stride_1; // 0x07
    et_ptr_offset<unsigned char,0> verts; // 0x08
};

// size 0xC
struct rl_index_buffer_data
{
    rl_index_buffer_data(); // 0x004E5060

    unsigned int num_indices; // 0x00
    et_ptr_offset<unsigned char,0> indices; // 0x04
    unsigned char index_size; // 0x08
    unsigned char prim_type; // 0x09
    unsigned short num_blocks; // 0x0A
};

// size 0x2C
struct rl_submesh_data
{
    int get_num_faces(unsigned char prim_type); // 0x00511B10 (:111)
    rl_submesh_data(); // 0x004E5040

    int num_render_blocks; // 0x00
    vector offset_transform; // 0x04
    vector bmin; // 0x10
    vector bmax; // 0x1C
    et_ptr_offset<rl_render_block,0> render_blocks; // 0x28
};

// size 0x20
struct rl_mesh_data
{
    rl_mesh_data(); // 0x0050E130

    int num_sub_meshes; // 0x00
    et_ptr_offset<rl_submesh_data,0> sub_meshes; // 0x04
    rl_vertex_buffer_data vertex_buffer; // 0x08
    rl_index_buffer_data index_buffer; // 0x14
};

// size 0x2C
struct rl_submesh_data_runtime
{
    int get_num_faces(unsigned char prim_type); // 0x004C2E60 (:127)
    rl_submesh_data_runtime(const rl_submesh_data&);
    rl_submesh_data_runtime();
    void fill(const rl_submesh_data&);

    int num_render_blocks; // 0x00
    vector offset_transform; // 0x04
    vector bmin; // 0x10
    vector bmax; // 0x1C
    rl_render_block* render_blocks; // 0x28
};

// size 0xC
struct rl_vertex_buffer_data_runtime
{
    rl_vertex_buffer_data_runtime(const rl_vertex_buffer_data&);
    rl_vertex_buffer_data_runtime(); // 0x004B6C90 (.h:115)
    void fill(const rl_vertex_buffer_data&);

    int num_verts; // 0x00
    unsigned char vert_stride_0; // 0x04
    unsigned char vertex_format; // 0x05
    unsigned char num_uv_channels; // 0x06
    unsigned char vert_stride_1; // 0x07
    unsigned char* verts; // 0x08
};

// size 0xC
struct rl_index_buffer_data_runtime
{
    rl_index_buffer_data_runtime(const rl_index_buffer_data&);
    rl_index_buffer_data_runtime(); // 0x004B6CA0 (.h:162)
    void fill(const rl_index_buffer_data&);

    unsigned int num_indices; // 0x00
    unsigned char* indices; // 0x04
    unsigned char index_size; // 0x08
    unsigned char prim_type; // 0x09
    unsigned short num_blocks; // 0x0A
};

// size 0x20
struct rl_mesh_data_runtime
{
    rl_mesh_data_runtime(const rl_mesh_data& data, const LoadedMeshInfo& meshInfo); // 0x00520900 (.h:268)
    rl_mesh_data_runtime(); // 0x004B6CB0 (.h:264)
    void fill(const rl_mesh_data& data, const LoadedMeshInfo& meshInfo); // 0x00508D30 (.h:274)

    int num_sub_meshes; // 0x00
    rl_submesh_data* sub_meshes; // 0x04
    rl_vertex_buffer_data_runtime vertex_buffer; // 0x08
    rl_index_buffer_data_runtime index_buffer; // 0x14
};

// ---- inline members (the et_ptr_offset members start at -1)

// 0x004E5050
inline rl_vertex_buffer_data::rl_vertex_buffer_data()
{
}

// 0x004E5060
inline rl_index_buffer_data::rl_index_buffer_data()
{
}

// 0x004E5040
inline rl_submesh_data::rl_submesh_data()
{
}

// 0x0050E130
inline rl_mesh_data::rl_mesh_data()
{
}

// 0x004B6C90 (.h:115)
// Uninitialized.
inline rl_vertex_buffer_data_runtime::rl_vertex_buffer_data_runtime()
{
}

// 0x004B6CA0 (.h:162)
// Uninitialized.
inline rl_index_buffer_data_runtime::rl_index_buffer_data_runtime()
{
}

// 0x004B6CB0 (.h:264)
// Uninitialized.
inline rl_mesh_data_runtime::rl_mesh_data_runtime()
{
}

rl_mesh_data* rl_mesh_data_load_validated(unsigned char* cpu_buf, unsigned char* gpu_buf, int& cpu_offset, int& gpu_offset, LoadedMeshInfo* pMeshInfo); // 0x0053F450 (:616)
rl_mesh_data* rl_mesh_data_load_unsplit_validated(unsigned char* buf, int& offset, LoadedMeshInfo* pMeshInfo); // 0x0053F4F0 (:769)
unsigned int rl_mesh_data_calc_crc(rl_mesh_data* data_p); // 0x004C3060 (:863)
