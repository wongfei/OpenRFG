// Original: volition/rfg/code/geom/mesh_shared.h
//
// Common header of static and character mesh files (signature, version, offsets of the mesh and material data).
// Placement inferred.
#pragma once

#include "volition/vlib/code/util/endian_type.h"

class rl_mesh;
class rl_material;
class rl_material_map;

// size 0x30
struct mesh_header_shared
{
    unsigned int m_signature; // 0x00
    unsigned int m_version; // 0x04
    et_sized_native_pointer<rl_mesh> m_mesh; // 0x08
    et_sized_native_pointer<rl_material_map> m_material_map; // 0x10
    et_sized_native_pointer<et_sized_native_pointer<rl_material> > m_materials; // 0x18
    unsigned int m_num_materials; // 0x20
    et_sized_native_pointer<char const > m_texture_names; // 0x28
};
