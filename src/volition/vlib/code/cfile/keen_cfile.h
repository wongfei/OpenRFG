// Original: volition/vlib/code/cfile/keen_cfile.h
// Implementation: volition/vlib/code/cfile/keen_cfile.cpp
//
// cfile's standard file layer on top of the keen file system (keen/base/filesystem.hpp). Volition hands the file system
// over with keen_cf_set_filesystem (g_pFileSystem). Open files come from Cfile_keen_open_files (256, file static);
// packfile entries share their packfile's keen file through keen_cf_packfile_handle (Cfile_keen_open_packfiles, 256).
// cf_keen_open_file::m_flags: 0x8 abort requested (keen_cf_abort_async_read). m_handle is the keen file handle,
// 0xFFFFFFFF when closed (keen_cf_initialize).
#pragma once

#include <stdint.h>
#include "volition/vlib/code/cfile/cfile_common.h"

namespace keen
{
    struct FileSystem;
}

// size 0x1C
struct cf_keen_open_file
{
    void reset(); // 0x005649B0 (.h:76)

    unsigned int m_handle; // 0x00
    keen::FileSystem* m_pFileSystem; // 0x04
    unsigned int m_flags; // 0x08
    unsigned int m_async_num_bytes_to_transfer; // 0x0C
    unsigned int m_async_pad_bytes; // 0x10
    unsigned int m_async_num_pad_bytes_to_transfer; // 0x14
    unsigned int m_async_num_bytes_read; // 0x18
};

// size 0x8
struct keen_cf_packfile_handle
{
    void reset(); // 0x005649D0 (.h:100)

    cf_keen_open_file* file_handle; // 0x00
    v_packfile_entry* packfile_entry; // 0x04
};

extern keen::FileSystem* g_pFileSystem; // 0x01CDBA14

void keen_cf_set_filesystem(keen::FileSystem* pFileSystem); // 0x00565B30 (:73)
bool keen_cf_getNativeFilePath(char* pBuffer, unsigned int bufferCapacity, const char* pFileName); // 0x00565B40 (:78)
bool keen_cf_initialize(); // 0x005982B0 (:109)
void keen_cf_abort_async_read(cf_keen_open_file* cfp); // 0x00565B60 (:126)
void keen_cf_abort_async_read(keen_cf_packfile_handle* cfp); // 0x00565B70 (:132)
void keen_cf_reset_async_abort(cf_keen_open_file* cfp); // 0x00565B80 (:142)
void keen_cf_reset_async_abort(keen_cf_packfile_handle* cfp); // 0x00565B90 (:149)
unsigned int keen_cf_read_compressed_chain(unsigned char** dest_buf, unsigned int total_compressed_bytes_to_read, unsigned int* num_bytes_to_read, cf_keen_open_file* cfp, cf_io_media_types media_type, uint64_t cur_file_pos); // 0x005B64A0 (:165)
unsigned int keen_cf_read_compressed(void* dest_buf, unsigned int total_compressed_bytes_to_read, cf_keen_open_file* cfp, uint64_t cur_file_pos); // 0x00565BA0 (:359)
bool keen_cf_find_file_io_standard(cf_found_info* found_info, const char* filename, cf_io_media_types media_type); // 0x00565BB0 (:381)
cf_keen_open_file* keen_cf_open_file(const char* filename, const char* mode, cf_io_media_types media_type, bool async); // 0x005B68A0 (:417)
keen_cf_packfile_handle* keen_cf_open_packfile_file(const cf_found_info* found_info, const char* mode, bool async); // 0x005B69C0 (:504)
bool keen_cf_close_packfile(keen_cf_packfile_handle* pfp, cf_io_media_types media_type); // 0x005C7F20 (:567)
unsigned int keen_cf_read_packfile(void* dest_buf, keen_cf_packfile_handle* pfp, cf_io_media_types media_type, unsigned int num_bytes_to_read, uint64_t cur_file_pos); // 0x005B6A80 (:602)
bool keen_cf_close_file(cf_keen_open_file* fp, cf_io_media_types media_type); // 0x005C3500 (:636)
unsigned int keen_cf_read(void* dest_buf, unsigned int num_bytes_to_read, cf_keen_open_file* fp, cf_io_media_types media_type, uint64_t cur_file_pos, bool abortable); // 0x005982F0 (:670)
bool keen_cf_seek(cf_keen_open_file* fp, int64_t offset, cf_seek_mode seek_mode, cf_io_media_types media_type); // 0x00565C60 (:839)
bool keen_cf_seek_packfile(keen_cf_packfile_handle* pfp, int offset, cf_seek_mode seek_mode, cf_io_media_types media_type); // 0x00598350 (:876)
int keen_cf_sync(cf_keen_open_file*, cf_io_media_types); // 0x00565D90 (:900)
void keen_cf_wait_async(cf_keen_open_file* fp, bool); // 0x00565DA0 (:962)
void keen_packfile_init(); // 0x00565DB0 (:1005)
