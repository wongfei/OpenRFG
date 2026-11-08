// Original: volition/rfg/code/level/rfg_terrainx_shared.cpp
//
// Partial (stub_impl.md S3): the terrain file header and the asset assembler's name lists (terrain_load_filenames_SLOW,
// the build path without preload files). restricted_area_2d_point_in_polygon too.

#include "volition/rfg/code/level/rfg_terrainx_shared.h"

#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/util/string_pool.h"

#include "keen/base/trace.hpp"

#include <stdio.h>

static const unsigned int Terrain_signature = 0x52524554; // magic number: "TERR"
static const unsigned int Terrain_version = 0x1F; // the only version this code reads

// The thread's message buffer (TLS +0x38 in rfg.exe).
static __declspec(thread) char Terrain_message[1024];

// offset rounded up to a multiple of 4.
static int terrain_align4(int offset)
{
    return (offset & 3) == 0 ? offset : offset + (4 - (offset & 3));
}

// 0x0091B390 (:21)
// Reads the header at data + offset and points the name lists and stitch infos into data, advancing offset past them
// (each list 4-byte aligned). False with a message for an old or newer version, silently for a bad signature.
bool terrain_process_header(const char* filename, unsigned char* data, int size, int& offset, terrain_header* header, char*& texture_filenames, char*& stitch_filenames, char*& fmesh_filenames, terrain_stitch_info*& stitch_infos)
{
    *header = *(const terrain_header*)(data + offset);
    if (header->signature != Terrain_signature)
    {
        return false;
    }
    if (header->version < Terrain_version)
    {
        sprintf(Terrain_message, "Terrain '%s' is too old and needs to be re-saved", filename);
        keen::debug::nativePrintArgs(Terrain_message);
        return false;
    }
    if (header->version > Terrain_version)
    {
        sprintf(Terrain_message, "Terrain '%s' is newer than the current version, which means you need to sync your code", filename);
        keen::debug::nativePrintArgs(Terrain_message);
        return false;
    }
    offset += sizeof(terrain_header);
    offset = terrain_align4(offset);
    texture_filenames = (char*)(data + offset);
    offset += header->texture_names_size;
    offset = terrain_align4(offset);
    stitch_filenames = (char*)(data + offset);
    offset += header->stitch_piece_names_size;
    // original: the stitch infos are not aligned
    stitch_infos = (terrain_stitch_info*)(data + offset);
    offset += header->num_stitch_pieces * sizeof(terrain_stitch_info);
    offset = terrain_align4(offset);
    fmesh_filenames = (char*)(data + offset);
    offset += header->fmesh_names_size;
    offset = terrain_align4(offset);
    return true;
}

// 0x009314E0 (:76)
// Reads the terrain file (at most data_size bytes) and adds its textures (with the platform's bitmap extension) and
// foliage meshes (.fmeshx) to the unique lists; the stitch infos' names are made relative to the stitch name list.
bool terrain_load_filenames_SLOW(const char* filename, unsigned char* data, int data_size, str_data_hash_table<unsigned char,1024,64,unsigned short>* texture_list_unique, str_data_hash_table<unsigned char,1024,64,unsigned short>* fmesh_list_unique, terrain_stitch_info*& stitch_infos, int& num_stitch_infos, vlib_platform plat)
{
    cfile* file = cf_open(filename, "rb", PLATFORM_PC, false);
    if (file == nullptr)
    {
        return false;
    }
    unsigned int size = cf_size(file);
    if ((unsigned int)data_size < size)
    {
        size = data_size;
    }
    cf_read(data, size, file, false);
    cf_close(file);
    int offset = 0;
    terrain_header header;
    char* texture_filenames;
    char* stitch_filenames;
    char* fmesh_filenames;
    if (!terrain_process_header(filename, data, size, offset, &header, texture_filenames, stitch_filenames, fmesh_filenames, stitch_infos))
    {
        return false;
    }
    string_pool names(nullptr);
    const unsigned char data_value = 0;
    char name[64];
    if (texture_list_unique != nullptr)
    {
        names.restore_from_mem(texture_filenames, header.texture_names_size, header.num_texture_name_strings, "");
        const char* texture = names.get_first();
        for (int i = 0; i < (int)header.num_texture_name_strings; i++)
        {
            cf_add_ext(name, sizeof(name), texture, Vbm_platform_ext[plat], false);
            texture_list_unique->insert_key_data(name, data_value);
            texture = names.get_next(texture);
        }
    }
    for (int i = 0; i < (int)header.num_stitch_pieces; i++)
    {
        et_ptr_offset<char,0>& field = stitch_infos[i].m_filename;
        const int name_offset = (int)(field.to_pointer() - (char*)&field);
        field.from_pointer(stitch_filenames + name_offset);
    }
    num_stitch_infos = header.num_stitch_pieces;
    // original: fmesh_list_unique is not checked
    names.restore_from_mem(fmesh_filenames, header.fmesh_names_size, header.num_fmesh_names, "");
    const char* fmesh = names.get_first();
    for (int i = 0; i < (int)header.num_fmesh_names; i++)
    {
        cf_add_ext(name, sizeof(name), fmesh, ".fmeshx", false);
        fmesh_list_unique->insert_key_data(name, data_value);
        fmesh = names.get_next(fmesh);
    }
    return true;
}

// 0x0091B4D0 (:173): an odd number of edges crossing the ray from the point (the winding of the side test decides which
// way an edge counts).
bool restricted_area_2d_point_in_polygon(const vector2& pt, const vector2* polygon, int num_pts)
{
    unsigned int crossings = 0;
    const vector2* prev = polygon + num_pts - 1;
    for (int i = 0; i < num_pts; i++)
    {
        const vector2* cur = polygon + i;
        if (pt.y < prev->y)
        {
            if (cur->y <= pt.y && vec2_which_side_of_2d_line(pt, *prev, *cur) < 0.0f)
            {
                crossings++;
            }
        }
        else if (pt.y < cur->y && 0.0f < vec2_which_side_of_2d_line(pt, *prev, *cur))
        {
            crossings++;
        }
        prev = cur;
    }
    return (crossings & 1) != 0;
}
