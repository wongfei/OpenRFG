// Original: volition/vlib/code/cfile/packfile.h
// Implementation: volition/vlib/code/cfile/packfile.cpp
//
// Packfiles (.vpp_pc), decompiled from packfile.cpp:
// - Layout: one 2048-byte header sector whose first 0x17C bytes are v_packfile (descriptor 0x51890ACE, version >= 3),
//   then the directory (num_files v_packfile_entry_serialized, dir_size bytes), the file names (filename_size bytes)
//   and the data, each block starting on a 2048-byte boundary. Entry offsets (start) are relative to the data block.
// - v_packfile::flags: 0x1 compressed (each entry deflated, compressed_size), 0x2 condensed (the whole data block is
//   one stream; read with packfile_read_condensed, not through cfile), 0x4/0x8/0x10 media HOST/HDD/DVD, 0x20 data
//   preloaded in memory (v_packfile::data), 0x80 encrypted. packfile_find only returns an entry whose packfile has
//   the media flag asked for.
// - packfile_add opens the file, reads header and directory into Packfile_mempool, converts the serialized entries
//   (self-relative et_ptr_offset names) into v_packfile_entry, and registers each file in the packfile hash table
//   (filename hash -> packfile_file_info). With a preload pool the data block is read too and the packfile is served
//   from memory (flag 0x20). Packfile_runtime_info keeps the pool marks so packfile_remove can roll back.
// - packfile_set_priority makes one packfile win every lookup (searched linearly before the hash table).
// - Temp packfiles (packfile_temp_add, packfile_temp_find_data) live in Temp_packfile_mempool and are dropped with
//   packfile_clear_last_temp_packfile / packfile_clear_temp_packfiles.
// - packfile_read_condensed reads header and directory into header_pool and the data block into dest_pool
//   (xml_enable_precache loads the table packfile this way).
// packfile_config defaults (constructor): 500 files, 20 packfiles, 0x7800-byte pool, 1 temp packfile, 0x7800-byte
// temp pool.
#pragma once

#include "volition/vlib/code/util/endian_type.h"

struct cfile;
class mempool_base;
struct v_packfile_entry;
struct v_packfile_entry_serialized;

// size 0x17C
struct v_packfile
{
    unsigned int descriptor; // 0x00
    unsigned int version; // 0x04
    char short_name[65]; // 0x08
    char pathname[256]; // 0x49
    unsigned int flags; // 0x14C
    unsigned int sector; // 0x150
    unsigned int num_files; // 0x154
    unsigned int file_size; // 0x158
    unsigned int dir_size; // 0x15C
    unsigned int filename_size; // 0x160
    unsigned int data_size; // 0x164
    unsigned int compressed_data_size; // 0x168
    v_packfile_entry* dir; // 0x16C
    v_packfile_entry_serialized* dir_serialized; // 0x170
    char* filenames; // 0x174
    unsigned char* data; // 0x178
};

// size 0x1C
struct v_packfile_entry
{
    char* get_name() const;
    void fill_without_parent(const v_packfile_entry_serialized& in_serialized); // 0x005B5920 (.h:150)

    char* name; // 0x00
    unsigned int sector; // 0x04
    unsigned int start; // 0x08
    unsigned int hash; // 0x0C
    unsigned int size; // 0x10
    unsigned int compressed_size; // 0x14
    v_packfile* parent; // 0x18
};

// size 0x1C
struct v_packfile_entry_serialized
{
    et_ptr_offset<char,0> name; // 0x00
    unsigned int sector; // 0x04
    unsigned int start; // 0x08
    unsigned int hash; // 0x0C
    unsigned int size; // 0x10
    unsigned int compressed_size; // 0x14
    et_ptr_offset<v_packfile,0> parent; // 0x18
};

// size 0x14
struct packfile_config
{
    packfile_config(); // 0x005D1A00 (.h:242)

    int max_files; // 0x00
    int max_packfiles; // 0x04
    int pool_size; // 0x08
    int max_temp_packfiles; // 0x0C
    int temp_pool_size; // 0x10
};

// size 0x8
struct v_packfile_callback
{
    char ext[4]; // 0x00
    void (*pfnReader)(cfile*, v_packfile_entry*); // 0x04
};

extern bool Packfiles_enabled; // 0x01CE8673

void v_packfile_shutdown(); // 0x005B6B10 (:575)
void packfile_init(packfile_config* config); // 0x005985F0 (:743)
void packfile_set_priority(v_packfile* p); // 0x00566170 (:799)
v_packfile_entry* packfile_find(const char* filename, unsigned int packfile_media_flag); // 0x00598880 (:810)
bool packfile_exists_in_priority(const char* filename); // 0x00598990 (:851)
v_packfile* packfile_find_packfile(const char* packfile_name); // 0x00598A60 (:878)
const char* packfile_get_last_packfile_name(); // 0x00566180 (:893)
const char* packfile_find_ext(int& offset, const char* ext); // 0x005661A0 (:908)
const char* packfile_find_pattern(int& offset, const char* pattern); // 0x00598AC0 (:952)
bool packfile_contains_file(const v_packfile* p, const char* filename); // 0x00598B20 (:974)
void packfile_set_thread_ownership(); // 0x00598B80 (:989)
bool packfile_validate_packfile_name(const char* packfile_pathname); // 0x00566260 (:1002)
bool packfile_verify_packfile(const char* packfile_pathname); // 0x005CAD00 (:1029)
bool packfile_get_header_info(cfile* f, v_packfile* p, mempool_base* dir_pool); // 0x005CAE00 (:1084)
bool packfile_get_header_info(const char* packfile_name, v_packfile* p, mempool_base* dir_pool); // 0x005CAE20 (:1090)
bool remove_packfiles_from_hash_table(v_packfile* p); // 0x00566300 (:1112)
bool packfile_remove(const char* packfile_name); // 0x005B6C30 (:1135)
bool packfile_add(const char* packfile_name, mempool_base* preload, bool installed_packfile, bool encrypted); // 0x005CAE80 (:1188)
bool packfile_temp_add(const char* packfile_name, mempool_base* preload, bool installed_file, bool encrypted); // 0x005CAF80 (:1259)
v_packfile* packfile_temp_find_packfile(const char* packfile_name); // 0x00598B90 (:1279)
bool packfile_temp_find_data(const char* filename, unsigned char** buf, unsigned int& size); // 0x00598BF0 (:1302)
void packfile_clear_last_temp_packfile(); // 0x00566390 (:1345)
void packfile_clear_temp_packfiles(); // 0x00598D20 (:1369)
bool v_packfile_read_and_callback(const char* packfile_name, int num_callbacks, v_packfile_callback* callback_infos); // 0x005CAFC0 (:1408)
bool packfile_read_condensed(const char* packfile_name, mempool_base* header_pool, mempool_base* dest_pool, v_packfile* p, bool encrypted); // 0x005CB0B0 (:1500)
bool packfile_read_condensed(unsigned char* packfile_loc, unsigned int packfile_size, mempool_base* dest_pool, v_packfile* p); // 0x005CB170 (:1546)
uint64_t packfile_get_absolute_offset(v_packfile_entry* pe); // 0x005663E0 (:1574)
uint64_t packfile_get_absolute_data_offset(v_packfile* pf); // 0x00566400 (:1579)
const char* packfile_get_filename_entry(int entry_index); // 0x00566430 (:1584)
