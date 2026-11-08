// Original: volition/rfg/code/geom/static_mesh_shared.h
// Implementation: volition/rfg/code/geom/static_mesh_shared.cpp
//
// Static meshes (.csmesh_pc / .gsmesh_pc), shared with the tools: static_mesh_load builds a static_mesh from the
// CPU/GPU pair (render mesh, materials, tags). Placement of static_mesh inferred.
#pragma once

#include "volition/rfg/code/geom/mesh_shared.h"
#include "volition/rfg/code/geom/mesh_tags.h"

class rl_mesh;
class rl_material;
class rl_material_map;
struct rl_mesh_data;

// size 0x60
struct static_mesh
{
    mesh_header_shared m_shared_header; // 0x00
    unsigned int m_num_lods; // 0x30
    et_sized_native_pointer<int> m_lod_submesh_id; // 0x38
    et_sized_native_pointer<mesh_tags> m_tags_offset; // 0x40
    mesh_tags m_tags; // 0x48
    unsigned int m_cm_index; // 0x58
};

static_mesh* static_mesh_load(const char* filename, unsigned char* cpu_data, int cpu_size, unsigned char* gpu_data, int gpu_size); // 0x00809710 (:126)
void static_mesh_unload(static_mesh* smp); // 0x007FBFD0 (:164)
