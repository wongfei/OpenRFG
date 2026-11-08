// Original: volition/vlib/code/cfile/vdir.h
// Implementation: volition/vlib/code/cfile/vdir.cpp
//
// Loose-file directory ("vdir"): a prebuilt index of the files on disk (Vdir_filenames[platform]) used when the game
// runs without packfiles (CF_SEARCH_VDIR). vdir_restore (decompiled) reads a vdir_header, then the directory string pool
// and the file entries into Vdir_mempool, byte-swaps them for big-endian platforms and rebuilds Vdir_hash_table.
// vdir_config::pool_size defaults to 0x80000. Every Vdir_filenames entry is "vdir_pc".
#pragma once

#include "volition/vlib/code/vlib_core/vlib_core.h"

// size 0x4
struct vdir_config
{
    vdir_config(); // 0x005D19C0 (.h:25)

    unsigned int pool_size; // 0x00
};

extern const char* Vdir_filenames[8]; // 0x01518A0C

bool vdir_system_init(vdir_config* config); // 0x00599500 (:121)
bool vdir_restore(vlib_platform platform, bool refresh); // 0x005CB1D0 (:155)
bool vdir_find(char* fullname, unsigned int fullname_array_size, const char* name, unsigned int* size); // 0x005B7560 (:351)
const char* vdir_find_ext(int& offset, const char* ext); // 0x00566890 (:389)
const char* vdir_find_ext_path(int& offset, const char* ext, const char* path); // 0x00566930 (:439)
const char* vdir_find_pattern(int& offset, const char* pattern); // 0x005995C0 (:490)
const char* vdir_get_filename_entry(int entry_index); // 0x00566A20 (:519)
unsigned int vdir_refresh_file_size(const char* filename); // 0x005B7610 (:527)
void vdir_refresh_file_size_ext(const char* ext); // 0x005B76F0 (:566)
void vdir_refresh_all_sizes(); // 0x005B7740 (:577)
