// Original: volition/rfg/code/geom/material_shared.cpp
//
// Standalone material loading shared with the tools (VFX.md FX4): the material's render_lib data (materials registered
// with the material library, then the material map) is made in place in the loaded file and released again. Compiled in
// rfg_effects (OPENRFG_EFFECTS) with the stream2 callbacks of rfg_material.cpp; the programs without it keep the
// stand-ins of tests/stubs/rfg/geom/geom.cpp. Placement of material_load_rl_data inferred (it sits between the two loaders).

#include "volition/rfg/code/geom/material_shared.h"
#include "volition/rfg/code/geom/material.h"

#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_data.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_material_map.h"

// 'b01dface' and the only version the loader takes.
static const unsigned int MATERIAL_SIGNATURE = 0xB01DFACE;
static const unsigned int MATERIAL_VERSION = 2;

// The loader's patch of a stored offset (from base) into a pointer; an offset of 0 or -1 is null (inlined for each
// pointer of the header).
template<typename T>
static void patch_pointer(et_sized_native_pointer<T>& pointer, void* base)
{
    const int offset = (int)(intptr_t)pointer.to_pointer();
    pointer.from_pointer(offset == -1 || offset == 0 ? nullptr : (T*)((char*)base + offset));
}

// 0x00808490 (LTCG: mat_p in ESI)
// The material map data, then the materials (each registered with the library and entered into the map data), then
// the material map. A failed material releases the ones made before it.
// Original: neither rl_material::create nor rl_material_map::create is checked, and the map data is loaded before the
// material and texture name pointers are patched (it does not read them).
static bool material_load_rl_data(material* mat_p)
{
    patch_pointer(mat_p->m_material_map, mat_p);
    int offset = 0;
    rl_material_map_data* map_data = rl_material_map_data_load((unsigned char*)mat_p->m_material_map.to_pointer(), offset);
    patch_pointer(mat_p->m_materials, mat_p);
    patch_pointer(mat_p->m_texture_names, mat_p);
    et_sized_native_pointer<rl_material>* materials = mat_p->m_materials.to_pointer();
    for (unsigned int i = 0; i < mat_p->m_num_materials; i++)
    {
        offset = 0;
        patch_pointer(materials[i], mat_p);
        rl_material_data* mat_data = rl_material_data_load((unsigned char*)materials[i].to_pointer(), offset, mat_p->m_texture_names.to_pointer(), true);
        if (mat_data == nullptr)
        {
            for (unsigned int k = 0; k < i; k++)
            {
                rl_material_library_unregister(materials[k].to_pointer());
                materials[k].to_pointer()->release();
            }
            return false;
        }
        materials[i].from_pointer(rl_material::create());
        materials[i].to_pointer()->set_material_data(mat_data);
        map_data->materials[i] = rl_material_library_register(materials[i].to_pointer());
    }
    mat_p->m_material_map.from_pointer(rl_material_map::create());
    mat_p->m_material_map.to_pointer()->set_map_data(map_data);
    return true;
}

// 0x008085D0 (:80): the material is the loaded data itself.
material* material_load(unsigned char* data, int size)
{
    material* mat_p = (material*)data;
    if (mat_p->m_signature != MATERIAL_SIGNATURE || mat_p->m_version != MATERIAL_VERSION)
    {
        return nullptr;
    }
    return material_load_rl_data(mat_p) ? mat_p : nullptr;
}

// 0x007FBF40 (:97)
void material_unload(material* smp)
{
    rl_material_map* material_map = smp->m_material_map.to_pointer();
    if (material_map != nullptr)
    {
        material_map->release();
        smp->m_material_map.from_pointer(nullptr);
    }
    et_sized_native_pointer<rl_material>* materials = smp->m_materials.to_pointer();
    for (unsigned int i = 0; i < smp->m_num_materials; i++)
    {
        rl_material* mat = materials[i].to_pointer();
        if (mat != nullptr)
        {
            rl_material_library_unregister(mat);
            materials[i].to_pointer()->release();
            materials[i].from_pointer(nullptr);
        }
    }
}
