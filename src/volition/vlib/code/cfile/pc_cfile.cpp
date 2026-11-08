// Original: volition/vlib/code/cfile/pc_cfile.cpp
//
// The Win32 file layer. cfile does not use it for reading in this build (keen_cfile does); packfile_init clears its
// packfile handles and the save code uses cf_create_or_verify_dirs_for_path.
//
// Partial: the file I/O functions (pc_cf_read_uncompressed, pc_cf_read_compressed_chain, pc_cf_read_compressed,
// pc_cf_find_file_io_standard, pc_cf_open_file, pc_cf_open_packfile_file, pc_cf_close_packfile, pc_cf_read_packfile,
// pc_cf_close_file, pc_cf_read, pc_cf_write, pc_cf_seek, pc_cf_seek_packfile, pc_cf_sync, pc_cf_wait_async) and
// cf_create_or_verify_dirs_for_path(_wide) are not implemented yet.

#include "volition/vlib/code/cfile/pc_cfile.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/cs_wrapper.h"

#include <string.h>

// ---- file-static globals
static object_pool<cf_pc_open_file, 256, 1> Cfile_pc_open_files; // 0x01D38960
static cs_wrapper Cfile_pc_open_files_cs(0); // 0x01CE9940
static pc_cf_packfile_handle Cfile_pc_open_packfiles[256]; // 0x01CD2B20, cleared by packfile_init and pc_packfile_init
static cs_wrapper Cfile_pc_open_packfiles_cs(0); // 0x01D3BEA0
static unsigned int Cfile_pc_disk_bytes_per_sector = 0x200; // 0x01518A04, pc_cf_get_async_align
static void (*CFile_pc_open_file_callback)(const char*, bool); // 0x01CDD7CC, pc_cf_set_open_callback

// 0x00565DD0 (.h:75)
void cf_pc_open_file::reset()
{
    m_file_handle = INVALID_HANDLE_VALUE;
    m_attrs_flags = 0;
    m_flags = 0;
    memset(&m_overlapped, 0, sizeof(m_overlapped));
    m_async_num_bytes_to_transfer = 0;
    m_async_pad_bytes = 0;
    m_async_num_pad_bytes_to_transfer = 0;
    m_async_num_bytes_read = 0;
}

// 0x00565E00 (.h:105)
void pc_cf_packfile_handle::reset()
{
    file_handle = nullptr;
    packfile_entry = nullptr;
}

// 0x00566460 (:92)
int64_t pc_file_seek(void* hf, int64_t distance, unsigned long MoveMethod)
{
    LONG high = (LONG)(distance >> 32);
    const DWORD low = SetFilePointer(hf, (LONG)distance, &high, MoveMethod);
    if (low == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
    {
        return -1;
    }
    return ((int64_t)high << 32) | low;
}

// 0x00598DC0 (:133)
bool pc_cf_initialize()
{
    for (int i = 0; i < 256; ++i)
    {
        Cfile_pc_open_files.get_entry(i)->reset();
    }
    return true;
}

// 0x005664B0 (:149)
bool pc_cf_get_os_full_name(char* fullname, unsigned int fullname_array_size, const char* filename, cf_io_media_types media_type)
{
    // Terminates at 255 whatever fullname_array_size is.
    strncpy(fullname, filename, fullname_array_size);
    fullname[0xFF] = '\0';
    return true;
}

// 0x005664E0 (:166)
void pc_cf_abort_async_read(cf_pc_open_file* cfp)
{
    cfp->m_flags |= 8;
}

// 0x005664F0 (:175)
void pc_cf_abort_async_read(pc_cf_packfile_handle* cfp)
{
    cfp->file_handle->m_flags |= 8;
}

// 0x00566500 (:185)
void pc_cf_reset_async_abort(cf_pc_open_file* cfp)
{
    cfp->m_flags &= ~8u;
}

// 0x00566510 (:194)
void pc_cf_reset_async_abort(pc_cf_packfile_handle* cfp)
{
    cfp->file_handle->m_flags &= ~8u;
}

// 0x00566530 (:752)
void pc_cf_trigger_open_callback(const char* filename, bool write)
{
    if (CFile_pc_open_file_callback != nullptr)
    {
        CFile_pc_open_file_callback(filename, write);
    }
}

// 0x00566720 (:1353)
void pc_packfile_init()
{
    for (int i = 0; i < 256; ++i)
    {
        Cfile_pc_open_packfiles[i].reset();
    }
}

// 0x00566740 (:1471)
unsigned int pc_cf_get_async_align()
{
    return Cfile_pc_disk_bytes_per_sector;
}

// 0x00566750 (:1476)
void pc_cf_set_open_callback(void (*callback)(const char*, bool))
{
    CFile_pc_open_file_callback = callback;
}
