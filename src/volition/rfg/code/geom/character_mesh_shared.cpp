// Original: volition/rfg/code/geom/character_mesh_shared.cpp
//
// Loading a character mesh (.ccmesh_pc, signature 0xFAC351A9, version 4) in place (player_minimal.md P3): the render
// data first (character_load_rl_data, in the render half character_mesh_shared_render.cpp since it creates render
// objects), then the header's offsets are patched to pointers and the collision shapes scaled from inches to meters.
// character_mesh_unload is in the render half too. Compiled in rfg_geom_unity.cpp.

#include "volition/rfg/code/geom/character_mesh_shared.h"

// Magic numbers: the signature and version of .ccmesh_pc files.
static const unsigned int CHARACTER_MESH_SIGNATURE = 0xFAC351A9;
static const unsigned int CHARACTER_MESH_VERSION = 4;
// Magic number: inches to meters (the float constant the binary widens to double).
static const double CHARACTER_MESH_INCHES_TO_METERS = (double)0.0254f;

// The loader's patch of a stored offset (from base) into a pointer; an offset of 0 or -1 is null (inlined for each
// pointer of the header).
template<typename T>
static void patch_pointer(et_sized_native_pointer<T>& pointer, void* base)
{
    const int offset = (int)(intptr_t)pointer.to_pointer();
    pointer.from_pointer(offset == -1 || offset == 0 ? nullptr : (T*)((char*)base + offset));
}

// 0x00802760 (:114, LTCG: gpu_data in EDX)
character_mesh* character_mesh_load(unsigned char* cpu_data, int cpu_size, unsigned char* gpu_data, int& approx_gpu_size, const char* pFilename)
{
    if (gpu_data == nullptr)
    {
        return nullptr;
    }
    character_mesh* cmesh = (character_mesh*)cpu_data;
    if (cmesh->m_shared_header.m_signature != CHARACTER_MESH_SIGNATURE || cmesh->m_shared_header.m_version != CHARACTER_MESH_VERSION ||
        !character_load_rl_data(cmesh, gpu_data, approx_gpu_size, pFilename))
    {
        return nullptr;
    }
    patch_pointer(cmesh->m_lod_submesh_id, cmesh);
    patch_pointer(cmesh->m_bone_indices, cmesh);
    patch_pointer(cmesh->m_cspheres, cmesh);
    character_mesh_csphere* spheres = cmesh->m_cspheres.to_pointer();
    for (int i = 0; i < cmesh->m_num_cspheres; i++)
    {
        spheres[i].radius = (float)((double)spheres[i].radius * CHARACTER_MESH_INCHES_TO_METERS);
    }
    patch_pointer(cmesh->m_ccylinders, cmesh);
    character_mesh_ccylinder* cylinders = cmesh->m_ccylinders.to_pointer();
    for (int i = 0; i < cmesh->m_num_ccylinders; i++)
    {
        cylinders[i].height = (float)((double)cylinders[i].height * CHARACTER_MESH_INCHES_TO_METERS);
        cylinders[i].radius = (float)((double)cylinders[i].radius * CHARACTER_MESH_INCHES_TO_METERS);
    }
    return cmesh;
}
