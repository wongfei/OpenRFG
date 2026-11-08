// Original: volition/vlib/code/cfile/pc_cfile.h
// Implementation: volition/vlib/code/cfile/pc_cfile.cpp
//
// The Win32 file layer (CreateFile + overlapped I/O). In this build cfile uses keen_cfile for standard files; the PC
// layer is still linked (pc_cf_initialize, pc_packfile_init from packfile_init) and cf_create_or_verify_dirs_for_path
// is used for save paths. "<CSIDL_PERSONAL>" in a name expands to the user's documents folder (decompiled).
// cf_pc_open_file::m_flags: 0x8 abort requested, as in cf_keen_open_file.
#pragma once

#include <windows.h>
#include <stdint.h>
#include "volition/vlib/code/cfile/cfile_common.h"

// size 0x30
struct cf_pc_open_file
{
    void reset(); // 0x00565DD0 (.h:75)

    void* m_file_handle; // 0x00
    unsigned int m_attrs_flags; // 0x04
    unsigned int m_flags; // 0x08
    _OVERLAPPED m_overlapped; // 0x0C
    unsigned int m_async_num_bytes_to_transfer; // 0x20
    unsigned int m_async_pad_bytes; // 0x24
    unsigned int m_async_num_pad_bytes_to_transfer; // 0x28
    unsigned int m_async_num_bytes_read; // 0x2C
};

// size 0x8
struct pc_cf_packfile_handle
{
    void reset(); // 0x00565E00 (.h:105)

    cf_pc_open_file* file_handle; // 0x00
    v_packfile_entry* packfile_entry; // 0x04
};

int64_t pc_file_seek(void* hf, int64_t distance, unsigned long MoveMethod); // 0x00566460 (:92)
bool pc_cf_initialize(); // 0x00598DC0 (:133)
bool pc_cf_get_os_full_name(char* fullname, unsigned int fullname_array_size, const char* filename, cf_io_media_types media_type); // 0x005664B0 (:149)
void pc_cf_abort_async_read(cf_pc_open_file* cfp); // 0x005664E0 (:166)
void pc_cf_abort_async_read(pc_cf_packfile_handle* cfp); // 0x005664F0 (:175)
void pc_cf_reset_async_abort(cf_pc_open_file* cfp); // 0x00566500 (:185)
void pc_cf_reset_async_abort(pc_cf_packfile_handle* cfp); // 0x00566510 (:194)
unsigned int pc_cf_read_compressed_chain(unsigned char** dest_buf, unsigned int total_compressed_bytes_to_read, unsigned int* num_bytes_to_read, cf_pc_open_file* cfp, cf_io_media_types media_type, uint64_t cur_file_pos); // 0x005B6DA0 (:210)
unsigned int pc_cf_read_compressed(void* dest_buf, unsigned int total_compressed_bytes_to_read, cf_pc_open_file* cfp, uint64_t cur_file_pos); // 0x00566520 (:403)
bool pc_cf_find_file_io_standard(cf_found_info* found_info, const char* filename, cf_io_media_types media_type); // 0x00598E20 (:564)
cf_pc_open_file* pc_cf_open_file(const char* filename, const char* mode, cf_io_media_types media_type, bool async); // 0x005B71F0 (:641)
void pc_cf_trigger_open_callback(const char* filename, bool write); // 0x00566530 (:752)
pc_cf_packfile_handle* pc_cf_open_packfile_file(const cf_found_info* found_info, const char* mode, bool async); // 0x005B73E0 (:767)
bool pc_cf_close_packfile(pc_cf_packfile_handle* pfp, cf_io_media_types media_type); // 0x005C7F70 (:830)
unsigned int pc_cf_read_packfile(void* dest_buf, pc_cf_packfile_handle* pfp, cf_io_media_types media_type, unsigned int num_bytes_to_read, uint64_t cur_file_pos); // 0x005B74C0 (:865)
bool pc_cf_close_file(cf_pc_open_file* fp, cf_io_media_types media_type); // 0x005C3600 (:899)
unsigned int pc_cf_read(void* dest_buf, unsigned int num_bytes_to_read, cf_pc_open_file* fp, cf_io_media_types media_type, uint64_t cur_file_pos, bool abortable); // 0x00598F90 (:947)
unsigned int pc_cf_write(const void* source_buf, unsigned int num_bytes_to_write, cf_pc_open_file* fp, unsigned int curr_file_pos); // 0x00599200 (:1110)
bool pc_cf_seek(cf_pc_open_file* fp, int64_t offset, cf_seek_mode seek_mode, cf_io_media_types media_type); // 0x00566540 (:1191)
bool pc_cf_seek_packfile(pc_cf_packfile_handle* pfp, int offset, cf_seek_mode seek_mode, cf_io_media_types media_type); // 0x005665E0 (:1226)
int pc_cf_sync(cf_pc_open_file* fp, cf_io_media_types media_type); // 0x00566680 (:1248)
void pc_cf_wait_async(cf_pc_open_file* fp, bool check_transfer_size); // 0x005666E0 (:1309)
void pc_packfile_init(); // 0x00566720 (:1353)
bool cf_create_or_verify_dirs_for_path(const char* full_path); // 0x005992A0 (:1363)
bool cf_create_or_verify_dirs_for_path_wide(const char* full_path); // 0x00599390 (:1405)
unsigned int pc_cf_get_async_align(); // 0x00566740 (:1471)
void pc_cf_set_open_callback(void (*callback)(const char*, bool)); // 0x00566750 (:1476)
