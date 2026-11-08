// Original: volition/rfg/code/geom/character_mesh.cpp
//
// Character mesh streaming (player_minimal.md P3): the GSP_CHARACTER_MESH callbacks load and unload the meshes
// (character_mesh_shared.cpp) and keep them by (file name, srid) for character_mesh_find. Compiled in
// rfg_geom_unity.cpp, with character_mesh_modify_rimlight_colors (the multiplayer team colors of the render materials,
// stub_impl.md S3).
// The external interface is declared in character_mesh.h.

#include "volition/rfg/code/geom/character_mesh.h"

#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/rfg/code/geom/character_mesh_shared.h"
#include "volition/vlib/code/math/crc.h"

#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/cfile.h"

#include "keen/base/trace.hpp"

#include <stdio.h>

// ---- globals and file statics
farray<character_mesh*,32> Character_mesh_modified_characters; // 0x0165F3C0
static stream2_data_hash<character_mesh*,512,unsigned short> Character_mesh_hash; // 0x0260BEB0

// ---- functions
// 0x0080A8A0 (:32)
// Original: a mesh of a container without a srid is loaded but not kept (reported as a failure).
bool character_mesh_load_cb(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size)
{
    character_mesh* cmesh = character_mesh_load(cpu_data, cpu_data_size, gpu_data, gpu_data_size, filename);
    if (cmesh == nullptr)
    {
        // The original formats into a thread-local buffer.
        char message[256];
        sprintf(message, "Failed to load rl data for %s", filename);
        keen::debug::nativePrintArgs(message);
        return false;
    }
    return srid != 0 && Character_mesh_hash.insert(filename, srid, cmesh);
}

// 0x0080A960 (:61)
void character_mesh_unload_cb(const char* filename, unsigned int srid)
{
    character_mesh* cmesh = nullptr;
    Character_mesh_hash.get(filename, srid, cmesh);
    if (cmesh == nullptr)
    {
        return;
    }
    // No longer a mesh with modified (rim light) materials.
    for (int i = 0; i < Character_mesh_modified_characters.size(); i++)
    {
        if (Character_mesh_modified_characters[i] == cmesh)
        {
            Character_mesh_modified_characters.remove(i);
            break;
        }
    }
    if (srid != 0)
    {
        Character_mesh_hash.remove(filename, srid);
    }
    character_mesh_unload(cmesh);
}

// 0x008023A0 (:82)
// The multiplayer team colour: every material of the mesh with an "MP_rim_color" constant gets the colour (taken out
// of the material library, changed, registered again and set in the mesh's material map). A mesh is changed once
// (Character_mesh_modified_characters, 32 at most); false only when that list is full.
bool character_mesh_modify_rimlight_colors(character_mesh* cmesh, const rl_color_float& rimlight_color)
{
    if (Character_mesh_modified_characters.full())
    {
        return false;
    }
    if (Character_mesh_modified_characters.find(cmesh, false) != -1)
    {
        return true;
    }
    static unsigned int rimcolor_name_crc = crc_stri("MP_rim_color", 0); // a function static, set on the first call
    bool modified = false;
    mesh_header_shared& header = cmesh->m_shared_header;
    for (int i = 0; i < (int)header.m_num_materials; i++)
    {
        rl_material* material = header.m_materials.to_pointer()[i].to_pointer();
        const unsigned int loc = (unsigned int)material->get_constant_loc(rimcolor_name_crc);
        if (loc == 0xFFFFFFFF)
        {
            continue;
        }
        rl_material_library_unregister(material);
        material->set_constant_data(loc, (float*)&rimlight_color.red, 4);
        material->invalidate_hash_cache();
        const unsigned int handle = rl_material_library_register(material);
        header.m_material_map.to_pointer()->set_material(i, handle);
        modified = true;
    }
    if (modified)
    {
        Character_mesh_modified_characters.add(cmesh);
    }
    return true;
}

// 0x007F2500 (:147)
void character_mesh_system_init()
{
}

// 0x007F2510 (:151)
void character_mesh_system_shutdown()
{
}

// 0x0080AA20 (:155): by file name (with or without the .ccmesh_pc extension).
character_mesh* character_mesh_find(const char* filename, unsigned int srid)
{
    char name[64];
    cf_add_ext(name, sizeof(name), filename, ".ccmesh_pc", false);
    character_mesh* cmesh = nullptr;
    Character_mesh_hash.get(name, srid, cmesh);
    return cmesh;
}
