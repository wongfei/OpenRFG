// Original: volition/rfg/code/geom/static_mesh_shared.cpp
//
// Loading a static mesh (.csmesh_pc, signature 0xC0FFEE11, version 5) in place: the header's pointers are stored as
// offsets from the header and patched to pointers; the render mesh, the material map and the materials are created
// and registered, the tags are read, and the collision models handed to the Havok collision model manager.

#include "volition/rfg/code/geom/static_mesh_shared.h"

#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_data.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/rfg/code/havok/havok_cm_manager.h"

static const unsigned int STATIC_MESH_SIGNATURE = 0xC0FFEE11;
static const unsigned int STATIC_MESH_VERSION = 5;

// The loader's patch of a stored offset (from base) into a pointer; an offset of 0 or -1 is null (inlined for each
// pointer of the header).
template<typename T>
static void patch_pointer(et_sized_native_pointer<T>& pointer, void* base)
{
    const int offset = (int)(intptr_t)pointer.to_pointer();
    pointer.from_pointer(offset == -1 || offset == 0 ? nullptr : (T*)((char*)base + offset));
}

// 0x00809490 (:23, LTCG: smesh in ESI, gpu_data in ECX): the render mesh, then the materials (each registered with
// the library and entered into the map data), then the material map. A failure releases what was made.
static bool static_mesh_load_rl_data(static_mesh* smesh, unsigned char* gpu_data, const char* pName)
{
    mesh_header_shared& header = smesh->m_shared_header;
    patch_pointer(header.m_mesh, smesh);
    int cpu_offset = 0;
    int gpu_offset = 0;
    LoadedMeshInfo mesh_info(nullptr, nullptr);
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
    patch_pointer(header.m_material_map, smesh);
    int offset = 0;
    rl_material_map_data* map_data = rl_material_map_data_load((unsigned char*)header.m_material_map.to_pointer(), offset);
    patch_pointer(header.m_materials, smesh);
    patch_pointer(header.m_texture_names, smesh);
    et_sized_native_pointer<rl_material>* materials = header.m_materials.to_pointer();
    for (unsigned int i = 0; i < header.m_num_materials; i++)
    {
        patch_pointer(materials[i], smesh);
        offset = 0;
        rl_material_data* mat_data = rl_material_data_load((unsigned char*)materials[i].to_pointer(), offset, header.m_texture_names.to_pointer(), true);
        rl_material* material = mat_data != nullptr ? rl_material::create() : nullptr;
        if (material != nullptr)
        {
            materials[i].from_pointer(material);
        }
        if (mat_data == nullptr || material == nullptr)
        {
            if (mat_data != nullptr)
            {
                materials[i].from_pointer(nullptr);
            }
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

// 0x00809710 (:126)
static_mesh* static_mesh_load(const char* filename, unsigned char* cpu_data, int cpu_size, unsigned char* gpu_data, int gpu_size)
{
    static_mesh* smesh = (static_mesh*)cpu_data;
    if (smesh->m_shared_header.m_signature != STATIC_MESH_SIGNATURE || gpu_data == nullptr || smesh->m_shared_header.m_version != STATIC_MESH_VERSION)
    {
        return nullptr;
    }
    if (!static_mesh_load_rl_data(smesh, gpu_data, filename))
    {
        return nullptr;
    }
    patch_pointer(smesh->m_lod_submesh_id, smesh);
    patch_pointer(smesh->m_tags_offset, smesh);
    int tag_offset = 0;
    smesh->m_tags.read_tag_data((unsigned char*)smesh->m_tags_offset.to_pointer(), tag_offset);
    if (smesh->m_cm_index != 0xFFFFFFFF)
    {
        // m_cm_index holds the offset of the collision models until they are loaded; then it is the first model's
        // index in the collision model manager.
        unsigned int cm_chunk_size;
        unsigned int cm_index;
        unsigned int num_models;
        if (havok_cm_manager_load_all(&cm_chunk_size, cpu_data + smesh->m_cm_index, &cm_index, 1, &num_models, filename))
        {
            smesh->m_cm_index = cm_index;
        }
    }
    return smesh;
}

// 0x007FBFD0 (:164)
void static_mesh_unload(static_mesh* smp)
{
    rl_mesh* mesh = smp->m_shared_header.m_mesh.to_pointer();
    if (mesh != nullptr)
    {
        mesh->release();
        smp->m_shared_header.m_mesh.from_pointer(nullptr);
    }
    rl_material_map* material_map = smp->m_shared_header.m_material_map.to_pointer();
    if (material_map != nullptr)
    {
        material_map->release();
        smp->m_shared_header.m_material_map.from_pointer(nullptr);
    }
    et_sized_native_pointer<rl_material>* materials = smp->m_shared_header.m_materials.to_pointer();
    for (unsigned int i = 0; i < smp->m_shared_header.m_num_materials; i++)
    {
        rl_material* material = materials[i].to_pointer();
        if (material != nullptr)
        {
            rl_material_library_unregister(material);
            material->release();
            materials[i].from_pointer(nullptr);
        }
    }
    if (smp->m_cm_index != 0xFFFFFFFF)
    {
        havok_cm_manager_remove_from_list(smp->m_cm_index);
        smp->m_cm_index = 0xFFFFFFFF;
    }
}
