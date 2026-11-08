// Original: volition/render_lib/code/rl_xray_material_manager.cpp
//
// The x-ray materials (up to 32) by index; the index is what the x-ray pass draws with (stub_impl.md S3 completed it).

#include "volition/render_lib/code/rl_xray_material_manager.h"

// Magic number: 32 x-ray materials.
static unsigned int Xray_material_manager_materials[32]; // 0x01B69A50
static int Num_registered_xray_materials; // 0x01B7BD54

// 0x004D4C60 (:65)
// The first free index (0xFF when all 32 are taken).
unsigned char rl_xray_material_manager_register_material(unsigned int mat_handle)
{
    for (int i = 0; i < 32; i++)
    {
        if (Xray_material_manager_materials[i] == 0xFFFFFFFF)
        {
            Num_registered_xray_materials++;
            Xray_material_manager_materials[i] = mat_handle;
            return (unsigned char)i;
        }
    }
    return 0xFF;
}

// 0x004D4C90 (:78)
// Every index holding the material is freed.
void rl_xray_material_manager_unregister_material(unsigned int mat_handle)
{
    for (int i = 0; i < 32; i++)
    {
        if (Xray_material_manager_materials[i] == mat_handle)
        {
            Xray_material_manager_materials[i] = 0xFFFFFFFF;
            Num_registered_xray_materials--;
        }
    }
}

// 0x004D4CC0 (:89)
void rl_xray_material_manager_init()
{
    for (int i = 0; i < 32; i++)
    {
        Xray_material_manager_materials[i] = 0xFFFFFFFF;
    }
    Num_registered_xray_materials = 0;
}

// 0x004D4D70 (:98)
void rl_xray_material_manager_shutdown()
{
    for (int i = 0; i < 32; i++)
    {
        Xray_material_manager_materials[i] = 0xFFFFFFFF;
    }
    Num_registered_xray_materials = 0;
}

// 0x004D4E20 (:107)
unsigned int rl_xray_material_manager_lookup(unsigned char xray_material)
{
    if (xray_material > 31)
    {
        return 0xFFFFFFFF;
    }
    return Xray_material_manager_materials[xray_material];
}
