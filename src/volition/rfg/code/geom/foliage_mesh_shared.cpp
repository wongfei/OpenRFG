// Original: volition/rfg/code/geom/foliage_mesh_shared.cpp
//
// Loading a foliage mesh (.cfmesh_pc, signature 0x0FF1C1A1, version 5; lod.md L2) in place. The file is CPU only: the
// mesh is an unsplit mesh (rl_mesh_data_load_unsplit_validated, both buffers in the file). The header's pointers are
// stored as offsets from the header and patched to pointers; the render mesh, the materials (registered with the
// material library and entered into the map data) and the material map are created; then the LOD descriptions
// (rl_clone_mesh_lod_desc, the undergrowth's distance LODs) are patched.
//
// Notes (decompiled behaviour, kept as is): the render objects' creation is not checked except the mesh's; a failed
// material releases the materials before it and the mesh, but not the map data; the in-place header keeps the
// patched pointers after a failure.

#include "volition/rfg/code/geom/foliage_mesh_shared.h"

#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_data.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/rfg/code/geom/foliage_mesh.h"

#include "keen/base/string.hpp"

// Magic numbers: the foliage mesh header's signature and version.
static const unsigned int FOLIAGE_MESH_SIGNATURE = 0x0FF1C1A1;
static const unsigned int FOLIAGE_MESH_VERSION = 5;

// The loader's patch of a stored offset (from base) into a pointer; an offset of 0 or -1 is null (inlined for each
// pointer of the header).
template<typename T>
static void patch_pointer(et_sized_native_pointer<T>& pointer, void* base)
{
    const int offset = (int)(intptr_t)pointer.to_pointer();
    pointer.from_pointer(offset == -1 || offset == 0 ? nullptr : (T*)((char*)base + offset));
}

// 0x00808200 (:27, LTCG: fmesh in ESI, pFilename in ECX)
// The render mesh (named after the file), the materials, then the material map.
static bool foliage_mesh_load_rl_data(foliage_mesh* fmesh, const char* pFilename)
{
    mesh_header_shared& header = fmesh->m_shared_header;
    patch_pointer(header.m_mesh, fmesh);
    int offset = 0;
    LoadedMeshInfo mesh_info(nullptr, nullptr);
    // Magic number: the size of LoadedMeshInfo::name.
    keen::copyString(mesh_info.name, pFilename, 64);
    rl_mesh_data* mesh_data = rl_mesh_data_load_unsplit_validated((unsigned char*)header.m_mesh.to_pointer(), offset, &mesh_info);
    if (mesh_data == nullptr)
    {
        return false;
    }
    rl_mesh* mesh = rl_mesh::create();
    header.m_mesh.from_pointer(mesh);
    if (mesh == nullptr)
    {
        return false;
    }
    mesh->init(mesh_data, mesh_info, true);
    patch_pointer(header.m_material_map, fmesh);
    offset = 0;
    rl_material_map_data* map_data = rl_material_map_data_load((unsigned char*)header.m_material_map.to_pointer(), offset);
    patch_pointer(header.m_materials, fmesh);
    patch_pointer(header.m_texture_names, fmesh);
    et_sized_native_pointer<rl_material>* materials = header.m_materials.to_pointer();
    for (unsigned int i = 0; i < header.m_num_materials; i++)
    {
        patch_pointer(materials[i], fmesh);
        offset = 0;
        rl_material_data* mat_data = rl_material_data_load((unsigned char*)materials[i].to_pointer(), offset, header.m_texture_names.to_pointer(), true);
        if (mat_data == nullptr)
        {
            for (unsigned int k = 0; k < i; k++)
            {
                rl_material_library_unregister(materials[k].to_pointer());
                materials[k].to_pointer()->release();
            }
            header.m_mesh.to_pointer()->release();
            return false;
        }
        materials[i].from_pointer(rl_material::create());
        materials[i].to_pointer()->set_material_data(mat_data);
        map_data->materials[i] = rl_material_library_register(materials[i].to_pointer());
    }
    rl_material_map* material_map = rl_material_map::create();
    header.m_material_map.from_pointer(material_map);
    material_map->set_map_data(map_data);
    return true;
}

// 0x008083E0 (:121)
// The mesh in place in cpu_data, or null when the header is wrong or the render data fails.
foliage_mesh* foliage_mesh_load(const char* filename, unsigned char* cpu_data, int cpu_size, unsigned char* gpu_data, int gpu_size)
{
    foliage_mesh* fmesh = (foliage_mesh*)cpu_data;
    if (fmesh->m_shared_header.m_signature != FOLIAGE_MESH_SIGNATURE || fmesh->m_shared_header.m_version != FOLIAGE_MESH_VERSION)
    {
        return nullptr;
    }
    if (!foliage_mesh_load_rl_data(fmesh, filename))
    {
        return nullptr;
    }
    patch_pointer(fmesh->m_lod_descs, fmesh);
    return fmesh;
}

// 0x007FBEC0 (:145)
// Releases the mesh, the material map and the materials (unregistered from the library).
void foliage_mesh_unload(foliage_mesh* fmp)
{
    rl_mesh* mesh = fmp->m_shared_header.m_mesh.to_pointer();
    if (mesh != nullptr)
    {
        mesh->release();
        fmp->m_shared_header.m_mesh.from_pointer(nullptr);
    }
    rl_material_map* material_map = fmp->m_shared_header.m_material_map.to_pointer();
    if (material_map != nullptr)
    {
        material_map->release();
        fmp->m_shared_header.m_material_map.from_pointer(nullptr);
    }
    et_sized_native_pointer<rl_material>* materials = fmp->m_shared_header.m_materials.to_pointer();
    for (unsigned int i = 0; i < fmp->m_shared_header.m_num_materials; i++)
    {
        rl_material* material = materials[i].to_pointer();
        if (material != nullptr)
        {
            rl_material_library_unregister(material);
            material->release();
            materials[i].from_pointer(nullptr);
        }
    }
}
