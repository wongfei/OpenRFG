// Original: volition/rfg/code/geom/character_mesh_shared.cpp (render half)
//
// The render half of character_mesh_shared.cpp (player_minimal.md P3), compiled in rfg_render: the render mesh,
// materials and material map of a character mesh, and their release. The CPU half (character_mesh_load) is in rfg;
// headless programs use stand-ins (headless_stubs_render.cpp).

#include "volition/rfg/code/geom/character_mesh_shared.h"

#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_data.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_data.h"

#include "keen/base/string.hpp"

// The loader's patch of a stored offset (from base) into a pointer; an offset of 0 or -1 is null (inlined for each
// pointer of the header).
template<typename T>
static void patch_pointer(et_sized_native_pointer<T>& pointer, void* base)
{
    const int offset = (int)(intptr_t)pointer.to_pointer();
    pointer.from_pointer(offset == -1 || offset == 0 ? nullptr : (T*)((char*)base + offset));
}

// 0x008024D0 (:18, LTCG: cmesh in ESI, pFilename in ECX, gpu_data in EDX; approx_gpu_size is not passed)
// The render mesh (named after the file), then the materials (each registered with the library and entered into the
// map data), then the material map. A failure releases what was made.
bool character_load_rl_data(character_mesh* cmesh, unsigned char* gpu_data, int& approx_gpu_size, const char* pFilename)
{
    mesh_header_shared& header = cmesh->m_shared_header;
    patch_pointer(header.m_mesh, cmesh);
    int cpu_offset = 0;
    LoadedMeshInfo mesh_info(nullptr, nullptr);
    mesh_info.name[0] = '\0';
    keen::copyString(mesh_info.name, pFilename, sizeof(mesh_info.name));
    int gpu_offset = 0;
    rl_mesh_data* mesh_data = rl_mesh_data_load_validated((unsigned char*)header.m_mesh.to_pointer(), gpu_data, cpu_offset, gpu_offset, &mesh_info);
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
    mesh->init(mesh_data, mesh_info, false);
    patch_pointer(header.m_material_map, cmesh);
    int offset = 0;
    rl_material_map_data* map_data = rl_material_map_data_load((unsigned char*)header.m_material_map.to_pointer(), offset);
    patch_pointer(header.m_materials, cmesh);
    patch_pointer(header.m_texture_names, cmesh);
    et_sized_native_pointer<rl_material>* materials = header.m_materials.to_pointer();
    for (unsigned int i = 0; i < header.m_num_materials; i++)
    {
        patch_pointer(materials[i], cmesh);
        offset = 0;
        rl_material_data* mat_data = rl_material_data_load((unsigned char*)materials[i].to_pointer(), offset, header.m_texture_names.to_pointer(), true);
        rl_material* material = mat_data != nullptr ? rl_material::create() : nullptr;
        if (mat_data != nullptr)
        {
            materials[i].from_pointer(material);
        }
        if (material == nullptr)
        {
            for (unsigned int k = 0; k < i; k++)
            {
                rl_material_library_unregister(materials[k].to_pointer());
                materials[k].to_pointer()->release();
            }
            header.m_mesh.to_pointer()->release();
            return false;
        }
        material->set_material_data(mat_data);
        map_data->materials[i] = rl_material_library_register(material);
    }
    rl_material_map* material_map = rl_material_map::create();
    header.m_material_map.from_pointer(material_map);
    if (material_map != nullptr)
    {
        material_map->set_map_data(map_data);
        return true;
    }
    for (unsigned int i = 0; i < header.m_num_materials; i++)
    {
        rl_material_library_unregister(materials[i].to_pointer());
        materials[i].to_pointer()->release();
    }
    header.m_mesh.to_pointer()->release();
    return false;
}

// 0x007FBB90 (:162)
void character_mesh_unload(character_mesh* cmesh)
{
    if (cmesh == nullptr)
    {
        return;
    }
    rl_mesh* mesh = cmesh->m_shared_header.m_mesh.to_pointer();
    if (mesh != nullptr)
    {
        mesh->release();
        cmesh->m_shared_header.m_mesh.from_pointer(nullptr);
    }
    rl_material_map* material_map = cmesh->m_shared_header.m_material_map.to_pointer();
    if (material_map != nullptr)
    {
        material_map->release();
        cmesh->m_shared_header.m_material_map.from_pointer(nullptr);
    }
    et_sized_native_pointer<rl_material>* materials = cmesh->m_shared_header.m_materials.to_pointer();
    for (unsigned int i = 0; i < cmesh->m_shared_header.m_num_materials; i++)
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
