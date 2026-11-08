// Original: volition/rfg/code/util/always_loaded_shared.cpp
//
// The bitmaps and files that stay loaded for the whole game: the textures of always_loaded.slw3,
// always_loaded_terrain.slw3 and startup.slw3 (read once), plus what the asset assembler adds while it builds.

#include "volition/rfg/code/util/always_loaded_shared.h"

#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/bmpman/bmpman_defs.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/util/rule_file.h"

static bool Loaded_loaded_shared; // 0x03758250
static str_data_hash_table<unsigned char,1024,64,unsigned short> Always_loaded_combined; // 0x0375D180

// 0x00B611B0 (:26)
str_data_hash_table<unsigned char,1024,64,unsigned short>* always_loaded_get_tables_combined(vlib_platform plat)
{
    if (!Loaded_loaded_shared)
    {
        slw_get_textures("always_loaded.slw3", Always_loaded_combined, plat, false);
        slw_get_textures("always_loaded_terrain.slw3", Always_loaded_combined, plat, false);
        slw_get_textures("startup.slw3", Always_loaded_combined, plat, false);
        Loaded_loaded_shared = true;
    }
    return &Always_loaded_combined;
}

// 0x00B62780 (:45)
// Each entry's bitmap name with the platform's bitmap extension (str_data_hash_table::insert_key_data inlined).
void always_loaded_add_peg(peg_header* header, vlib_platform plat)
{
    peg_entry* entries = (peg_entry*)(header + 1);
    for (int i = 0; i < header->total_entries; i++)
    {
        char name[64];
        cf_add_ext(name, sizeof(name), (const char*)entries[i].filename, Vbm_platform_ext[plat], false);
        const unsigned char data = 0;
        Always_loaded_combined.insert_key_data(name, data);
    }
}

// 0x00B628B0 (:57)
void always_loaded_add_file(const char* filename)
{
    const unsigned char data = 0;
    Always_loaded_combined.insert_key_data(filename, data); // hash_table::add and the key copy inlined
}
