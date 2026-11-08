// Original: volition/vlib/code/cfile/keen_cfile.cpp
//
// cfile's standard file layer over the keen file system (g_pFileSystem). Only CF_IO_MEDIA_HDD is served and files open
// read-only ("Not supported" otherwise).
//
// Notes (decompiled behaviour, kept as is):
// - Files are read sequentially; the position is only set before a read when the file was opened async (m_flags 0x1).
// - keen_cf_open_file does not return its pool entry when the keen open fails.
// - keen_cf_seek with CF_SEEK_END seeks to size - offset; keen_cf_seek_packfile ignores the seek mode (offset from the
//   start of the entry).
// - keen_cf_read_compressed_chain reads the compressed data in blocks (half the data rounded up to 2 KB below 256 KB,
//   128 KB above) into two alternating buffers and inflates the previous block after reading the next one. With an
//   async file the last read is padded to 2 KB.

#include "volition/vlib/code/cfile/keen_cfile.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/cfile/packfile.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/util/zlib_vlib.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include "keen/base/filesystem.hpp"
#include "keen/base/filesystemtypes.hpp"

#include <stdlib.h>
#include <string.h>

keen::FileSystem* g_pFileSystem; // 0x01CDBA14

// ---- file-static globals
static object_pool<cf_keen_open_file, 256, 1> Cfile_keen_open_files; // 0x01D3BEE0
static cs_wrapper Cfile_keen_open_files_cs(0); // 0x01D0CB2C
static keen_cf_packfile_handle Cfile_keen_open_packfiles[256]; // 0x01CDA648, cleared by keen_packfile_init
static cs_wrapper Cfile_keen_open_packfiles_cs(0); // 0x01D3E06C

// 0x005649B0 (.h:76)
void cf_keen_open_file::reset()
{
    // m_pFileSystem is left as is.
    m_handle = 0xFFFFFFFF;
    m_flags = 0;
    m_async_num_bytes_to_transfer = 0;
    m_async_num_pad_bytes_to_transfer = 0;
    m_async_num_bytes_read = 0;
    m_async_pad_bytes = 0;
}

// 0x005649D0 (.h:100)
void keen_cf_packfile_handle::reset()
{
    file_handle = nullptr;
    packfile_entry = nullptr;
}

// 0x00565B30 (:73)
void keen_cf_set_filesystem(keen::FileSystem* pFileSystem)
{
    g_pFileSystem = pFileSystem;
}

// 0x00565B40 (:78)
bool keen_cf_getNativeFilePath(char* pBuffer, unsigned int bufferCapacity, const char* pFileName)
{
    return keen::file::getNativeFilePath(pBuffer, bufferCapacity, g_pFileSystem, pFileName);
}

// 0x005B6450 (:92)
static unsigned int keen_cf_read_uncompressed(void* dest_buf, unsigned int num_bytes_to_read, cf_keen_open_file* fp, uint64_t cur_file_pos)
{
    if ((fp->m_flags & 1) != 0)
    {
        keen::file::setFilePosition(fp->m_pFileSystem, fp->m_handle, cur_file_pos);
    }
    const keen::IoResult<uint32> result = keen::file::read(fp->m_pFileSystem, fp->m_handle, dest_buf, num_bytes_to_read);
    return result.error == keen::IoError_Ok ? result.result : 0;
}

// 0x005982B0 (:109)
bool keen_cf_initialize()
{
    for (int i = 0; i < 256; ++i)
    {
        Cfile_keen_open_files.get_entry(i)->reset();
    }
    return true;
}

// 0x00565B60 (:126)
void keen_cf_abort_async_read(cf_keen_open_file* cfp)
{
    cfp->m_flags |= 8;
}

// 0x00565B70 (:132)
void keen_cf_abort_async_read(keen_cf_packfile_handle* cfp)
{
    cfp->file_handle->m_flags |= 8;
}

// 0x00565B80 (:142)
void keen_cf_reset_async_abort(cf_keen_open_file* cfp)
{
    cfp->m_flags &= ~8u;
}

// 0x00565B90 (:149)
void keen_cf_reset_async_abort(keen_cf_packfile_handle* cfp)
{
    cfp->file_handle->m_flags &= ~8u;
}

// 0x005B64A0 (:165)
unsigned int keen_cf_read_compressed_chain(unsigned char** dest_buf, unsigned int total_compressed_bytes_to_read, unsigned int* num_bytes_to_read, cf_keen_open_file* cfp, cf_io_media_types media_type, uint64_t cur_file_pos)
{
    if (media_type != CF_IO_MEDIA_HDD && media_type != CF_IO_MEDIA_DVD)
    {
        return 0;
    }
    virtual_mempool pool;
    pool.init(Vlib_temp_page_pool, "cf_read_pool", 0xC0000, false, 0x1000);
    unsigned char* read_buffers[2];
    read_buffers[0] = (unsigned char*)pool.alloc(0x20000, 0);
    read_buffers[1] = (unsigned char*)pool.alloc(0x20000, 0);
    unsigned char* inflate_buffer = (unsigned char*)pool.alloc(0x80000, 0);

    unsigned int total_read = 0;
    zlib_stream* decompressor = cf_alloc_decompressor();
    if (decompressor == nullptr)
    {
        keen::debug::nativePrintArgs("Unable to allocate a free decompressor, we are attempting to decompress from too many threads at once.");
        return 0;
    }
    unsigned int dest_buf_read_remaining = num_bytes_to_read[0];
    unsigned char* dest_buf_pos = dest_buf[0];
    unsigned int dest_buf_index = 0;
    decompressor->decompress_init();

    if (total_compressed_bytes_to_read < 0x1000)
    {
        keen_cf_read_uncompressed(read_buffers[0], total_compressed_bytes_to_read, cfp, cur_file_pos);
        total_read = cf_decompress_chain(total_compressed_bytes_to_read, read_buffers[0], decompressor, dest_buf, num_bytes_to_read, dest_buf_pos, dest_buf_index, dest_buf_read_remaining, inflate_buffer, 0x80000);
        cfp->m_async_num_bytes_read += total_read;
        decompressor->decompress_finish();
        cf_release_decompressor(decompressor);
        return total_read;
    }

    unsigned int block_size;
    if (total_compressed_bytes_to_read < 0x40000)
    {
        block_size = total_compressed_bytes_to_read >> 1;
        if ((block_size & 0x7FF) != 0)
        {
            block_size += 0x800 - (block_size & 0x7FF);
        }
    }
    else
    {
        block_size = 0x20000;
    }

    bool aborted = (cfp->m_flags & 8) != 0;
    if (!aborted)
    {
        unsigned int read_size = block_size <= total_compressed_bytes_to_read ? block_size : total_compressed_bytes_to_read;
        unsigned int last_size = 0xFFFFFFFF;
        keen_cf_read_uncompressed(read_buffers[0], read_size, cfp, cur_file_pos);
        aborted = (cfp->m_flags & 8) != 0;
        if (!aborted)
        {
            cur_file_pos += read_size;
            unsigned int remaining = total_compressed_bytes_to_read - read_size;
            int previous = 0;
            int current = 1;
            do
            {
                const unsigned int previous_size = read_size;
                read_size = block_size <= remaining ? block_size : remaining;
                remaining -= read_size;
                if (remaining == 0)
                {
                    last_size = read_size;
                    if ((cfp->m_flags & 1) != 0)
                    {
                        const unsigned int pad = (read_size & 0x7FF) == 0 ? 0 : 0x800 - (read_size & 0x7FF);
                        read_size += pad;
                        cfp->m_async_pad_bytes = pad;
                    }
                }
                if ((cfp->m_flags & 8) != 0)
                {
                    aborted = true;
                    break;
                }
                keen_cf_read_uncompressed(read_buffers[current], read_size, cfp, cur_file_pos);
                if ((cfp->m_flags & 8) != 0)
                {
                    aborted = true;
                    break;
                }
                const unsigned int num_inflated = cf_decompress_chain(previous_size, read_buffers[previous], decompressor, dest_buf, num_bytes_to_read, dest_buf_pos, dest_buf_index, dest_buf_read_remaining, inflate_buffer, 0x80000);
                total_read += num_inflated;
                cfp->m_async_num_bytes_read += num_inflated;
                cur_file_pos += read_size;
                previous = current;
                current = 1 - current;
            }
            while (remaining != 0);
            if (!aborted)
            {
                const unsigned int num_inflated = cf_decompress_chain(last_size, read_buffers[previous], decompressor, dest_buf, num_bytes_to_read, dest_buf_pos, dest_buf_index, dest_buf_read_remaining, inflate_buffer, 0x80000);
                total_read += num_inflated;
                cfp->m_async_num_bytes_read += num_inflated;
            }
        }
    }
    if (aborted)
    {
        total_read = 0;
    }
    decompressor->decompress_finish();
    cf_release_decompressor(decompressor);
    return total_read;
}

// 0x00565BA0 (:359)
unsigned int keen_cf_read_compressed(void* dest_buf, unsigned int total_compressed_bytes_to_read, cf_keen_open_file* cfp, uint64_t cur_file_pos)
{
    exit(1);
}

// 0x00565BB0 (:381)
bool keen_cf_find_file_io_standard(cf_found_info* found_info, const char* filename, cf_io_media_types media_type)
{
    if (media_type != CF_IO_MEDIA_HDD)
    {
        return false;
    }
    keen::FileStatus status;
    if (keen::file::getFileStatusByName(&status, g_pFileSystem, filename, keen::FileStatus::ValueMask_Size | keen::FileStatus::ValueMask_Exists) != keen::IoError_Ok)
    {
        return false;
    }
    strncpy(found_info->m_full_name, filename, cf_found_info::CFFI_FULL_NAME_ARRAY_SIZE);
    found_info->m_size = (unsigned int)status.size;
    found_info->m_full_name[cf_found_info::CFFI_FULL_NAME_ARRAY_SIZE - 1] = '\0';
    found_info->m_searched_system = CF_SEARCH_STANDARD;
    found_info->m_media_type = CF_IO_MEDIA_HDD;
    return true;
}

// 0x005B68A0 (:417)
cf_keen_open_file* keen_cf_open_file(const char* filename, const char* mode, cf_io_media_types media_type, bool async)
{
    if (media_type != CF_IO_MEDIA_HDD)
    {
        return nullptr;
    }
    if (mode[0] != 'r')
    {
        keen::debug::nativePrintArgs("Not supported\n");
        return nullptr;
    }
    Cfile_keen_open_files_cs.enter();
    cf_keen_open_file* fp = Cfile_keen_open_files.alloc(0);
    if (fp == nullptr)
    {
        Cfile_keen_open_files_cs.leave();
        return nullptr;
    }
    fp->reset();
    if (async)
    {
        fp->m_flags = 1;
    }
    const keen::IoResult<uint32> file = keen::file::openFile(g_pFileSystem, filename, keen::FileAccessMode_Read);
    if (file.error != keen::IoError_Ok)
    {
        Cfile_keen_open_files_cs.leave();
        return nullptr;
    }
    fp->m_handle = file.result;
    fp->m_pFileSystem = g_pFileSystem;
    if (mode[0] == 'a')
    {
        // Unreachable: only 'r' gets here.
        const keen::IoResult<uint64> size = keen::file::getFileSize(fp->m_pFileSystem, fp->m_handle);
        keen::file::setFilePosition(fp->m_pFileSystem, fp->m_handle, size.result);
    }
    Cfile_keen_open_files_cs.leave();
    return fp;
}

// 0x005B69C0 (:504)
keen_cf_packfile_handle* keen_cf_open_packfile_file(const cf_found_info* found_info, const char* mode, bool async)
{
    // Every open entry gets its own keen file on the packfile, positioned at the start of the entry.
    if (found_info->m_media_type > CF_IO_MEDIA_DVD)
    {
        return nullptr;
    }
    Cfile_keen_open_packfiles_cs.enter();
    for (int i = 0; i < 256; ++i)
    {
        keen_cf_packfile_handle* pfp = &Cfile_keen_open_packfiles[i];
        if (pfp->file_handle != nullptr)
        {
            continue;
        }
        v_packfile_entry* entry = found_info->vpe;
        pfp->file_handle = keen_cf_open_file(entry->parent->pathname, mode, CF_IO_MEDIA_HDD, async);
        if (pfp->file_handle == nullptr)
        {
            Cfile_keen_open_packfiles_cs.leave();
            return nullptr;
        }
        pfp->packfile_entry = entry;
        keen::file::setFilePosition(pfp->file_handle->m_pFileSystem, pfp->file_handle->m_handle, (uint64_t)entry->sector * 0x800);
        Cfile_keen_open_packfiles_cs.leave();
        return pfp;
    }
    Cfile_keen_open_packfiles_cs.leave();
    return nullptr;
}

// 0x005C7F20 (:567)
bool keen_cf_close_packfile(keen_cf_packfile_handle* pfp, cf_io_media_types media_type)
{
    if (media_type != CF_IO_MEDIA_HDD && media_type != CF_IO_MEDIA_DVD)
    {
        return false;
    }
    const bool result = keen_cf_close_file(pfp->file_handle, media_type);
    Cfile_keen_open_packfiles_cs.enter();
    pfp->reset();
    Cfile_keen_open_packfiles_cs.leave();
    return result;
}

// 0x005B6A80 (:602)
unsigned int keen_cf_read_packfile(void* dest_buf, keen_cf_packfile_handle* pfp, cf_io_media_types media_type, unsigned int num_bytes_to_read, uint64_t cur_file_pos)
{
    if (media_type != CF_IO_MEDIA_HDD && media_type != CF_IO_MEDIA_DVD)
    {
        return 0;
    }
    v_packfile_entry* entry = pfp->packfile_entry;
    const uint64_t file_pos = cur_file_pos + (uint64_t)entry->sector * 0x800;
    if ((entry->parent->flags & 1) != 0)
    {
        // The whole entry is inflated into dest_buf. The original passes the addresses of dest_buf and of a local as
        // one-entry chains; the chains here get an empty second entry (cf_decompress_chain looks one entry ahead).
        unsigned char* chain_dest_buf[2] = { (unsigned char*)dest_buf, nullptr };
        unsigned int chain_num_bytes[2] = { entry->size, 0 };
        const unsigned int num_read = keen_cf_read_compressed_chain(chain_dest_buf, entry->compressed_size, chain_num_bytes, pfp->file_handle, media_type, file_pos);
        entry->size = chain_num_bytes[0];
        return num_read;
    }
    return keen_cf_read_uncompressed(dest_buf, num_bytes_to_read, pfp->file_handle, file_pos);
}

// 0x005C3500 (:636)
bool keen_cf_close_file(cf_keen_open_file* fp, cf_io_media_types media_type)
{
    if (media_type != CF_IO_MEDIA_HDD)
    {
        return false;
    }
    keen::file::closeFile(fp->m_pFileSystem, fp->m_handle);
    Cfile_keen_open_files_cs.enter();
    Cfile_keen_open_files.free(fp);
    Cfile_keen_open_files_cs.leave();
    return true;
}

// 0x005982F0 (:670)
unsigned int keen_cf_read(void* dest_buf, unsigned int num_bytes_to_read, cf_keen_open_file* fp, cf_io_media_types media_type, uint64_t cur_file_pos, bool abortable)
{
    if (media_type != CF_IO_MEDIA_HDD)
    {
        return 0;
    }
    if ((fp->m_flags & 1) != 0)
    {
        keen::file::setFilePosition(fp->m_pFileSystem, fp->m_handle, cur_file_pos);
    }
    const keen::IoResult<uint32> result = keen::file::read(fp->m_pFileSystem, fp->m_handle, dest_buf, num_bytes_to_read);
    return result.error == keen::IoError_Ok ? result.result : 0;
}

// 0x00565C60 (:839)
bool keen_cf_seek(cf_keen_open_file* fp, int64_t offset, cf_seek_mode seek_mode, cf_io_media_types media_type)
{
    if (media_type != CF_IO_MEDIA_HDD)
    {
        return false;
    }
    keen::IoError error;
    switch (seek_mode)
    {
    case CF_SEEK_SET:
        error = keen::file::setFilePosition(fp->m_pFileSystem, fp->m_handle, (uint64_t)offset);
        break;
    case CF_SEEK_CUR:
    {
        const keen::IoResult<uint64> position = keen::file::getFilePosition(fp->m_pFileSystem, fp->m_handle);
        error = keen::file::setFilePosition(fp->m_pFileSystem, fp->m_handle, position.result + offset);
        break;
    }
    case CF_SEEK_END:
    {
        const keen::IoResult<uint64> size = keen::file::getFileSize(fp->m_pFileSystem, fp->m_handle);
        error = keen::file::setFilePosition(fp->m_pFileSystem, fp->m_handle, size.result - offset);
        break;
    }
    default:
        VLIB_CRITICAL_FAILURE();
        return false;
    }
    return error == keen::IoError_Ok;
}

// 0x00598350 (:876)
bool keen_cf_seek_packfile(keen_cf_packfile_handle* pfp, int offset, cf_seek_mode seek_mode, cf_io_media_types media_type)
{
    if ((pfp->packfile_entry->parent->flags & 1) != 0)
    {
        VLIB_CRITICAL_FAILURE();
    }
    if (media_type != CF_IO_MEDIA_HDD)
    {
        return false;
    }
    const uint64_t position = (uint64_t)pfp->packfile_entry->sector * 0x800 + (int64_t)offset;
    return keen::file::setFilePosition(pfp->file_handle->m_pFileSystem, pfp->file_handle->m_handle, position) == keen::IoError_Ok;
}

// 0x00565D90 (:900)
int keen_cf_sync(cf_keen_open_file*, cf_io_media_types)
{
    return 1;
}

// 0x00565DA0 (:962)
void keen_cf_wait_async(cf_keen_open_file* fp, bool)
{
}

// 0x00565DB0 (:1005)
void keen_packfile_init()
{
    for (int i = 0; i < 256; ++i)
    {
        Cfile_keen_open_packfiles[i].reset();
    }
}
