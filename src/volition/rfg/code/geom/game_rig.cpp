// Original: volition/rfg/code/geom/game_rig.cpp
//
// The rigs: every .rig_pc file is loaded into Rig_mempool at init (game_rig_system_init) and registered with
// anim_lib and in Anim_rig_hash by file name. Part of rfg_geom_unity.cpp (player_minimal.md P2).
// The external interface is declared in game_rig.h.

#include "volition/rfg/code/geom/game_rig.h"

#include "keen/base/trace.hpp"
#include "volition/anim_lib/code/anim_lib.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/util/hash_table.h"

#include <stdio.h>

// ---- file-static globals
static str_data_hash_table<anim_rig*, 290, 64, unsigned short> Anim_rig_hash; // 0x025F09B8

// 0x00808440
anim_rig* anim_rig_find(const char* filename)
{
    anim_rig* rig = nullptr;
    if (Anim_rig_hash.get(filename, rig)) {
        return rig;
    }
    return nullptr;
}

// Reads a rig file into Rig_mempool and creates its anim_lib rig (null when the file is missing).
static anim_rig* load_rig_file(const char* rig_name)
{
    char filename[128];
    cf_add_ext(filename, sizeof(filename), rig_name, ".rig_pc", false);
    cfile* fp = cf_open(filename, "rb", PLATFORM_PC, false);
    if (fp == nullptr) {
        char message[256];
        sprintf(message, "Rig file %s not found", filename);
        keen::debug::nativePrintArgs(message);
        return nullptr;
    }
    const unsigned int size = cf_size(fp);
    unsigned char* data = (unsigned char*)Rig_mempool.alloc(size, 16);
    cf_read(data, size, fp, false);
    cf_close(fp);
    return animlib_new_anim_rig(rig_name, data, size, true);
}

// 0x0080BF60. All rigs of the file system (at most 290).
void game_rig_system_init()
{
    const unsigned int mark = Common_mempool.mark();
    const char** files = (const char**)Common_mempool.alloc(0x8000, 0);
    const int num_files = (int)cf_find_ext(files, 0x2000, ".rig_pc");
    if (num_files > 290) {
        char message[512];
        sprintf(message, "Found too many animation rigs to preload (max : %d)", num_files);
        keen::debug::nativePrintArgs(message);
    }
    for (int i = 0; i < num_files; i++) {
        const char* name = files[i];
        if (animlib_get_rig_index(name) >= 0) {
            continue;
        }
        anim_rig* rig = load_rig_file(name);
        if (rig != nullptr) {
            Anim_rig_hash.insert_key_data(name, rig);
        }
    }
    Common_mempool.restore_to_mark(mark);
}

// 0x0080B2B0. A rig not loaded at init.
void page_in_animlib_rig(const char* rig_name)
{
    if (animlib_get_rig_index(rig_name) >= 0) {
        return;
    }
    anim_rig* rig = load_rig_file(rig_name);
    if (rig != nullptr) {
        Anim_rig_hash.insert_key_data(rig_name, rig);
    }
}
