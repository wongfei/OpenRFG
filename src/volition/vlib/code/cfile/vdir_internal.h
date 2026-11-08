// Original: volition/vlib/code/cfile/vdir_internal.h
// Implementation: volition/vlib/code/cfile/vdir_internal.cpp
//
// Storage of the loose-file directory (decompiled from vdir_restore): Vdir_file_entry_pool is a packed list of
// vdir_file_entry records, each followed by its NUL-terminated name and padded to 4 bytes; dir_name is an offset into
// Vdir_dir_string_pool. Vdir_hash_table (32768 buckets) maps vdir_hash(name) to the first entry offset; entries chain
// through vdir_file_entry::next. Placement of vdir_header inferred.
#pragma once

// size 0x14
struct vdir_header
{
    unsigned int version; // 0x00
    unsigned int header_size; // 0x04
    unsigned int min_compatible_version; // 0x08
    unsigned int directory_size; // 0x0C
    unsigned int file_entry_size; // 0x10
};

// size 0xC
struct vdir_file_entry
{
    int next; // 0x00
    unsigned int size; // 0x04
    int dir_name; // 0x08
};

extern unsigned char* Vdir_file_entry_pool; // 0x01CE86A4
extern unsigned int Vdir_file_entry_pool_size; // 0x01CE86A8
extern unsigned int Vdir_file_entry_pool_used; // 0x01CE86AC
extern char* Vdir_dir_string_pool; // 0x01CE86B0
extern int* Vdir_hash_table; // 0x01CE86B4

void vdir_hash_system_init(); // 0x00566A30 (:53)
void vdir_clear_hash_table(); // 0x00566A50 (:61)
unsigned int vdir_hash(const char* s); // 0x00566A80 (:76)
vdir_file_entry* vdir_internal_find(const char* filename); // 0x00566AB0 (:93)
