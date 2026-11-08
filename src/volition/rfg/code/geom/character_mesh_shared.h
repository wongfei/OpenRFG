// Original: volition/rfg/code/geom/character_mesh_shared.h
// Implementation: volition/rfg/code/geom/character_mesh_shared.cpp
//
// Character (skinned) meshes (.ccmesh_pc / .gcmesh_pc), shared with the tools: character_mesh_load builds a
// character_mesh (render mesh, materials, tags, bone mapping). Placement of character_mesh inferred.
#pragma once

#include "volition/rfg/code/geom/mesh_shared.h"
#include "volition/rfg/code/geom/mesh_tags.h"
#include "volition/vlib/code/math/vector3.h"

class rl_mesh;
class rl_material;
class rl_material_map;
struct rl_mesh_data;

// The body part of a character collision shape (the hit locations). Placement inferred.
enum character_body_part_id
{
    CBP_UNDEFINED = -1,
    CBP_TORSO = 0,
    CBP_UPPER_LEFT_LEG = 1,
    CBP_LOWER_LEFT_LEG = 2,
    CBP_UPPER_RIGHT_LEG = 3,
    CBP_LOWER_RIGHT_LEG = 4,
    CBP_UPPER_LEFT_ARM = 5,
    CBP_LOWER_LEFT_ARM = 6,
    CBP_UPPER_RIGHT_ARM = 7,
    CBP_LOWER_RIGHT_ARM = 8,
    CBP_PELVIS = 9,
    CBP_HEAD = 10,
    NUM_CHARACTER_BODY_PARTS = 11,
};

// A collision sphere of a character mesh (radius scaled to meters at load).
// size 0x18
struct character_mesh_csphere
{
    unsigned int body_part_id; // 0x00
    int parent_index; // 0x04
    vector pos; // 0x08
    float radius; // 0x14
};

// A collision cylinder of a character mesh (radius and height scaled to meters at load).
// size 0x28
struct character_mesh_ccylinder
{
    unsigned int body_part_id; // 0x00
    int parent_index; // 0x04
    vector axis; // 0x08
    vector pos; // 0x14
    float radius; // 0x20
    float height; // 0x24
};

// size 0x68
struct character_mesh
{
    mesh_header_shared m_shared_header; // 0x00
    unsigned int m_num_lods; // 0x30
    et_sized_native_pointer<int> m_lod_submesh_id; // 0x38
    int m_num_bones; // 0x40
    et_sized_native_pointer<unsigned int> m_bone_indices; // 0x48
    short m_num_cspheres; // 0x50
    short m_num_ccylinders; // 0x52
    et_sized_native_pointer<character_mesh_csphere> m_cspheres; // 0x58
    et_sized_native_pointer<character_mesh_ccylinder> m_ccylinders; // 0x60
};

character_mesh* character_mesh_load(unsigned char* cpu_data, int cpu_size, unsigned char* gpu_data, int& approx_gpu_size, const char* pFilename); // 0x00802760 (:114)
void character_mesh_unload(character_mesh* cmesh); // 0x007FBB90 (:162)

// The render half (character_mesh_shared_render.cpp in rfg_render; a static function of the original file): the render
// mesh, materials and material map of a character mesh.
bool character_load_rl_data(character_mesh* cmesh, unsigned char* gpu_data, int& approx_gpu_size, const char* pFilename); // 0x008024D0 (:18)
