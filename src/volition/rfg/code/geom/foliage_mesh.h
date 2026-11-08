// Original: volition/rfg/code/geom/foliage_mesh.h
// Implementation: volition/rfg/code/geom/foliage_mesh.cpp
//
// Foliage meshes (the undergrowth models, the in-world GPS chevron): the streamed mesh with its LOD descriptions, the
// stream2 load callbacks registered by game_stream2_register_types and the lookup by file name.
#pragma once

#include "volition/render_lib/code/rl_clone_mesh_instance.h"
#include "volition/rfg/code/geom/mesh_shared.h"
#include "volition/vlib/code/util/endian_type.h"

// size 0x40
struct foliage_mesh
{
    mesh_header_shared m_shared_header; // 0x00
    unsigned int m_num_lods; // 0x30
    et_sized_native_pointer<rl_clone_mesh_lod_desc> m_lod_descs; // 0x38
};

bool foliage_mesh_stream_load(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size); // 0x0080B210 (:52)
void foliage_mesh_stream_unload(const char* filename, unsigned int srid); // 0x00808150 (:73)
void foliage_mesh_system_init(); // 0x007F2F30 (:91)
void foliage_mesh_system_shutdown(); // 0x007F2F50 (:100)
foliage_mesh* foliage_mesh_find(const char* filename); // 0x008081A0 (:109)
void foliage_mesh_level_shutdown(); // 0x007F2F70 (:120)
