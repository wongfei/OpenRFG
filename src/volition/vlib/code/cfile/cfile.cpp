// Original: volition/vlib/code/cfile/cfile.cpp
//
// The cf_* file API over the keen file system (keen_cfile), packfiles and memory buffers.
//
// Notes (decompiled behaviour, kept as is):
// - Only memory files can be written; writes to any other file fail with CF_ERROR_WRITE.
// - cf_open_lock only records the flag (cf_is_locked); no open path checks it in this build.
// - cf_find_file_io_vdir and the CF_SEARCH_VDIR branch of cf_find_file print "This should not be called...".
// - cf_read_float/cf_write_float byte-swap the value rounded to an integer, not its bits.
// - cf_write_string writes the 16-bit length without byte-swapping it.
// - cf_find_ext returns the number of names found (the original returns a stale register); unlike cf_find_ext_path
//   and cf_find_pattern it does not take Cfile_search_cs.
// - cf_open_internal leaks the cfile when a search system it does not know reports a hit.

#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/cfile/cfile_internal.h"
#include "volition/vlib/code/cfile/keen_cfile.h"
#include "volition/vlib/code/cfile/packfile.h"
#include "volition/vlib/code/cfile/vdir.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/zlib_vlib.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include "keen/base/filesystem.hpp"
#include "keen/base/filesystemtypes.hpp"
#include "keen/base/string.hpp"

#include <ctype.h>
#include <math.h>
#include <new>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const unsigned int* const CRC_table; // 0x01518B80 (crc.cpp)

// ---- file-static globals
static object_pool<cfile, 256, 1> Cfile_list; // 0x01CE9BA0
static cs_wrapper Cfile_list_cs(0); // 0x01D500A8
static object_pool<cf_memory_handle, 256, 1> Cfile_memory_handle_list; // 0x01D0B580
static cs_wrapper Cfile_memory_handle_list_cs(0); // 0x01D08F38
static cf_registered_search Cfile_registered_searches[16]; // 0x01CDA5C8, search order (cf_add_search)
static unsigned int Cfile_num_registered_searches; // 0x01CE8674
static cs_wrapper Cfile_search_cs(0); // 0x01D0D080
static zlib_stream Cfile_decompress[2] = { "Cfile_decompress", "Cfile_decompress" }; // 0x01519D60, name not recovered (unused)
static volatile bool Cfile_decompress_in_use[2]; // 0x01CE8678
static cs_wrapper Cfile_decompress_cs(0); // 0x01D3E030
static char Cfile_downloadable_content_drive_base_name[64]; // 0x01CE2650
static bool Cfile_initialized; // 0x01CE8670
// File statics of vlib's unity build in rfg.exe that vconsole.cpp uses too (orig_main.md OM5); external here.
bool Cfile_vdir_enabled; // 0x01CE8671, set when a CF_SEARCH_VDIR search is added
bool Cf_open_locked; // 0x01CE8672, cf_open_lock

// 0x005649E0 (:183)
void cfile::reset()
{
    name[0] = '\0';
    full_pathname[0] = '\0';
    m_searched_system = CF_SEARCH_NONE;
    m_media_type = CF_IO_MEDIA_NONE;
    mode = CF_MODE_NONE;
    m_error_code = CF_ERROR_NONE;
    pos = 0;
    size = 0;
    max_write_size = 0xFFFFFFFF;
    m_flags = 0;
    m_standard_fp = nullptr;
    m_access_type = CF_IO_ACCESS_NONE;
}

// 0x005B5970 (:268)
static cfile* cf_get_free_cfile()
{
    Cfile_list_cs.enter();
    cfile* cfp = Cfile_list.alloc(0);
    Cfile_list_cs.leave();
    return cfp;
}

// 0x005B59A0 (:274)
static cf_memory_handle* cf_alloc_memory_handle()
{
    Cfile_memory_handle_list_cs.enter();
    cf_memory_handle* fp = Cfile_memory_handle_list.alloc(0);
    Cfile_memory_handle_list_cs.leave();
    return fp;
}

// 0x005C20E0 (:280)
static void cf_release_memory_handle(cf_memory_handle* fp)
{
    Cfile_memory_handle_list_cs.enter();
    Cfile_memory_handle_list.free(fp);
    Cfile_memory_handle_list_cs.leave();
}

// 0x005B59D0 (:293)
static bool cf_find_file_io_packfile(cf_found_info* found_info, const char* filename, cf_io_media_types media_type)
{
    unsigned int packfile_media_flag;
    switch (media_type)
    {
    case CF_IO_MEDIA_HDD:
        packfile_media_flag = 0x8;
        break;
    case CF_IO_MEDIA_DVD:
        packfile_media_flag = 0x10;
        break;
    case CF_IO_MEDIA_HOST:
        packfile_media_flag = 0x4;
        break;
    case CF_IO_MEDIA_MEMORY:
        packfile_media_flag = 0x20;
        break;
    default:
        VLIB_CRITICAL_FAILURE();
        packfile_media_flag = 0;
        break;
    }
    v_packfile_entry* entry = packfile_find(filename, packfile_media_flag);
    if (entry == nullptr)
    {
        return false;
    }
    found_info->vpe = entry;
    found_info->m_media_type = media_type;
    found_info->m_searched_system = CF_SEARCH_PACKFILE;
    strncpy(found_info->m_full_name, entry->name, cf_found_info::CFFI_FULL_NAME_ARRAY_SIZE);
    found_info->m_full_name[cf_found_info::CFFI_FULL_NAME_ARRAY_SIZE - 1] = '\0';
    found_info->m_size = entry->size;
    return true;
}

// 0x005C2110 (:335)
static bool cf_find_file_io_vdir(cf_found_info* found_info, const char* filename, cf_io_media_types media_type)
{
    char fullname[256];
    if (!vdir_find(fullname, sizeof(fullname), filename, &found_info->m_size))
    {
        return false;
    }
    keen::debug::nativePrintArgs("This should not be called...\n");
    strncpy(found_info->m_full_name, fullname, cf_found_info::CFFI_FULL_NAME_ARRAY_SIZE);
    found_info->m_media_type = media_type;
    found_info->m_searched_system = CF_SEARCH_VDIR;
    return true;
}

// 0x005B5A90 (:380)
static cf_memory_handle* cf_open_memory(unsigned char* buf, unsigned int buf_size, const char* open_mode, bool compressed, unsigned int compressed_size)
{
    cf_memory_handle* fp = cf_alloc_memory_handle();
    if (fp == nullptr)
    {
        return nullptr;
    }
    fp->buf = buf;
    fp->compressed_size = compressed_size;
    fp->size = buf_size;
    fp->compressed = compressed;
    return fp;
}

// 0x005C21B0 (:395)
static void cf_close_memory(cf_memory_handle* fp)
{
    cf_release_memory_handle(fp);
}

// 0x005B5AE0 (:400)
static unsigned int cf_read_compressed_chain_memory(unsigned int num_compressed_bytes_to_read, unsigned char** dest_buf, unsigned int* num_bytes_to_read, unsigned char* data)
{
    unsigned char* dest_buf_pos = dest_buf[0];
    unsigned int dest_buf_read_remaining = num_bytes_to_read[0];
    unsigned int dest_buf_index = 0;
    zlib_stream* decompressor = cf_alloc_decompressor();

    virtual_mempool pool;
    pool.init(Vlib_temp_page_pool, "cf_read_compressed_chain_memory_pool", 0x40000, false, 0x1000);
    unsigned char* buffer_space = (unsigned char*)pool.alloc(0x40000, 0);

    decompressor->decompress_init();
    const unsigned int result = cf_decompress_chain(num_compressed_bytes_to_read, data, decompressor, dest_buf, num_bytes_to_read, dest_buf_pos, dest_buf_index, dest_buf_read_remaining, buffer_space, 0x40000);
    decompressor->decompress_finish();
    cf_release_decompressor(decompressor);
    return result;
}

// 0x005B5C70 (:425)
static unsigned int cf_read_memory(void* dest_buf, cfile* p_cfile, unsigned int num_bytes_to_read)
{
    cf_memory_handle* fp = p_cfile->_m_memory_fp;
    if (fp->compressed)
    {
        return cf_read_compressed_chain_memory(fp->compressed_size, (unsigned char**)&dest_buf, &num_bytes_to_read, fp->buf + p_cfile->pos);
    }
    memcpy(dest_buf, fp->buf + p_cfile->pos, num_bytes_to_read);
    return num_bytes_to_read;
}

// 0x00564A50 (:441)
static unsigned int cf_write_memory(const void* src_buf, unsigned int num_bytes, cfile* p_cfile)
{
    cf_memory_handle* fp = p_cfile->_m_memory_fp;
    if (fp->compressed)
    {
        return 0;
    }
    if (p_cfile->max_write_size < p_cfile->pos + num_bytes)
    {
        VLIB_CRITICAL_FAILURE();
        p_cfile->m_error_code = CF_ERROR_WRITE;
    }
    if (p_cfile->m_error_code != CF_ERROR_NONE)
    {
        return 0;
    }
    memcpy(fp->buf + p_cfile->pos, src_buf, num_bytes);
    return num_bytes;
}

// 0x00564AC0 (:526)
void cf_set_downloadable_content_base_name(const char* name)
{
    strcpy(Cfile_downloadable_content_drive_base_name, name);
}

// 0x005C21E0 (:547)
bool cf_find_file(cf_found_info* found_info, const char* filename, bool encrypted)
{
    Cfile_search_cs.enter();
    found_info->m_searched_system = CF_SEARCH_NONE;
    found_info->m_media_type = CF_IO_MEDIA_NONE;
    found_info->m_full_name[0] = '\0';
    found_info->m_size = 0;
    found_info->vpe = nullptr;
    for (unsigned int i = 0; i < Cfile_num_registered_searches; ++i)
    {
        const cf_registered_search& search = Cfile_registered_searches[i];
        bool found;
        if (search.m_search_type == CF_SEARCH_STANDARD)
        {
            found = cf_find_file_io_standard(found_info, filename, search.m_media_type, encrypted);
        }
        else if (search.m_search_type == CF_SEARCH_PACKFILE)
        {
            found = cf_find_file_io_packfile(found_info, filename, search.m_media_type);
        }
        else if (search.m_search_type == CF_SEARCH_VDIR)
        {
            found = cf_find_file_io_vdir(found_info, filename, search.m_media_type);
        }
        else
        {
            VLIB_CRITICAL_FAILURE();
            break;
        }
        if (found)
        {
            Cfile_search_cs.leave();
            return true;
        }
    }
    Cfile_search_cs.leave();
    return false;
}

// 0x00597A30 (:625)
bool cf_find_file_io_standard(cf_found_info* found_info, const char* filename, cf_io_media_types media_type, bool encrypted)
{
    if (media_type != CF_IO_MEDIA_NONE && media_type != CF_IO_MEDIA_HDD)
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

// 0x005C2380 (:668)
static bool cf_open_file(cfile* cfp, cf_found_info* found_info, const char* mode)
{
    const bool async = (cfp->m_flags & 1) != 0;
    switch (found_info->m_searched_system)
    {
    case CF_SEARCH_PACKFILE:
        if (found_info->m_media_type == CF_IO_MEDIA_MEMORY)
        {
            // v_packfile::data holds the data block only; entry sectors count from the start of the file.
            v_packfile_entry* entry = found_info->vpe;
            v_packfile* packfile = entry->parent;
            const unsigned int header_sectors = 1 + ((packfile->dir_size + 0x7FF) >> 11) + ((packfile->filename_size + 0x7FF) >> 11);
            unsigned char* data = packfile->data + (entry->sector - header_sectors) * 0x800;
            cfp->_m_memory_fp = cf_open_memory(data, entry->size, mode, (packfile->flags & 1) != 0, entry->compressed_size);
            return true;
        }
        cfp->m_packfile_fp = keen_cf_open_packfile_file(found_info, mode, async);
        return cfp->m_packfile_fp != nullptr;
    case CF_SEARCH_STANDARD:
    case CF_SEARCH_VDIR:
        cfp->m_standard_fp = keen_cf_open_file(found_info->m_full_name, mode, found_info->m_media_type, async);
        return cfp->m_standard_fp != nullptr;
    default:
        VLIB_CRITICAL_FAILURE();
        return false;
    }
}

// 0x00564AE0 (:745)
static bool cf_verify_mode(const char* mode)
{
    return strlen(mode) == 2 && (mode[0] == 'r' || mode[0] == 'w' || mode[0] == 'a') && (mode[1] == 't' || mode[1] == 'b');
}

// 0x005C24A0 (:777)
static cfile* cf_open_internal(const char* filename_in, const char* mode, vlib_platform disk_platform, bool async, cf_io_media_types media)
{
    if (Cfile_num_registered_searches == 0)
    {
        VLIB_CRITICAL_FAILURE();
        return nullptr;
    }
    if (!cf_verify_mode(mode))
    {
        VLIB_CRITICAL_FAILURE();
        return nullptr;
    }
    char filename[256];
    strncpy(filename, filename_in, sizeof(filename));
    filename[sizeof(filename) - 1] = '\0';

    cfile* cfp = cf_get_free_cfile();
    if (cfp == nullptr)
    {
        VLIB_CRITICAL_FAILURE();
        return nullptr;
    }
    const bool encrypted = strchr(mode, 'e') != nullptr;
    cf_found_info found_info;
    found_info.vpe = nullptr;
    if (strchr(mode, 'r') == nullptr)
    {
        // Write and append always go to the standard system; append keeps the existing size.
        cfp->mode = CF_MODE_WRITE;
        bool found = false;
        if (strchr(mode, 'a') != nullptr)
        {
            found = cf_find_file_io_standard(&found_info, filename, media, encrypted);
        }
        found_info.m_media_type = media;
        if (!found)
        {
            cf_get_os_full_name(found_info.m_full_name, sizeof(found_info.m_full_name), filename, media);
            found_info.m_searched_system = CF_SEARCH_STANDARD;
            found_info.m_size = 0;
        }
    }
    else
    {
        cfp->mode = CF_MODE_READ;
        if (!cf_find_file(&found_info, filename, encrypted))
        {
            cfp->m_access_type = CF_IO_ACCESS_NONE;
            Cfile_list.free(cfp);
            return nullptr;
        }
    }
    cfp->m_flags = async ? 1u : 0u;
    if (!cf_open_file(cfp, &found_info, mode))
    {
        cfp->m_access_type = CF_IO_ACCESS_NONE;
        Cfile_list.free(cfp);
        return nullptr;
    }
    cfp->m_searched_system = found_info.m_searched_system;
    if (found_info.m_media_type == CF_IO_MEDIA_MEMORY)
    {
        cfp->m_access_type = CF_IO_ACCESS_MEMORY;
    }
    else if (found_info.m_searched_system == CF_SEARCH_PACKFILE)
    {
        cfp->m_access_type = CF_IO_ACCESS_PACKFILE;
    }
    else if (found_info.m_searched_system == CF_SEARCH_STANDARD || found_info.m_searched_system == CF_SEARCH_VDIR)
    {
        cfp->m_access_type = CF_IO_ACCESS_STANDARD;
    }
    else
    {
        keen::debug::nativePrintArgs("Bad times about to happen in cfile");
        return nullptr;
    }
    cfp->m_media_type = found_info.m_media_type;
    cfp->size = found_info.m_size;
    cfp->pos = cfp->mode == CF_MODE_WRITE ? found_info.m_size : 0;
    cf_drop_path(cfp->name, sizeof(cfp->name), filename);
    strcpy(cfp->full_pathname, found_info.m_full_name);
    if (Vlib_endianness[PLATFORM_PC] != Vlib_endianness[disk_platform])
    {
        cfp->m_flags |= 4;
    }
    return cfp;
}

// 0x005CF560 (:955)
bool cfile_system_init(vdir_config* vd_config, packfile_config* pf_config, bool)
{
    if (Cfile_initialized)
    {
        return true;
    }
    for (int i = 0; i < 256; ++i)
    {
        Cfile_list.get_entry(i)->reset();
    }
    if (vd_config != nullptr)
    {
        if (!vdir_system_init(vd_config))
        {
            return false;
        }
        // vdir_restore reads the directory file through the standard system.
        cf_add_search(CF_SEARCH_STANDARD, CF_IO_MEDIA_HDD, false);
        vdir_restore(PLATFORM_PC, false);
        cf_remove_search(CF_SEARCH_STANDARD, CF_IO_MEDIA_HDD);
    }
    if (pf_config != nullptr)
    {
        packfile_init(pf_config);
    }
    Cfile_downloadable_content_drive_base_name[0] = '\0';
    Cfile_initialized = true;
    return true;
}

// 0x005C27E0 (:1026)
void cfile_system_shutdown()
{
    v_packfile_shutdown();
}

// 0x00597AF0 (:1035)
void cf_abort_async_read(cfile* file)
{
    file->m_flags |= 8;
    switch (file->m_access_type)
    {
    case CF_IO_ACCESS_STANDARD:
        keen_cf_abort_async_read(file->m_standard_fp);
        break;
    case CF_IO_ACCESS_PACKFILE:
        keen_cf_abort_async_read(file->m_packfile_fp);
        break;
    case CF_IO_ACCESS_MEMORY:
        break;
    default:
        VLIB_CRITICAL_FAILURE();
        break;
    }
}

// 0x00597B40 (:1074)
void cf_reset_async_abort(cfile* file)
{
    file->m_flags &= ~8u;
    if (file->m_access_type == CF_IO_ACCESS_STANDARD)
    {
        keen_cf_reset_async_abort(file->m_standard_fp);
    }
    else if (file->m_access_type == CF_IO_ACCESS_PACKFILE)
    {
        keen_cf_reset_async_abort(file->m_packfile_fp);
    }
    else
    {
        VLIB_CRITICAL_FAILURE();
    }
}

// 0x00597B90 (:1108)
bool cf_add_search(cf_search_types search_type, cf_io_media_types media_type, bool use_first)
{
    Cfile_search_cs.enter();
    for (unsigned int i = 0; i < Cfile_num_registered_searches; ++i)
    {
        if (Cfile_registered_searches[i].m_search_type == search_type && Cfile_registered_searches[i].m_media_type == media_type)
        {
            // Already registered: only moved to the front when asked.
            if (use_first && i != 0)
            {
                for (; i != 0; --i)
                {
                    Cfile_registered_searches[i] = Cfile_registered_searches[i - 1];
                }
                Cfile_registered_searches[0].m_search_type = search_type;
                Cfile_registered_searches[0].m_media_type = media_type;
            }
            Cfile_search_cs.leave();
            return true;
        }
    }
    if (Cfile_num_registered_searches >= 16)
    {
        VLIB_CRITICAL_FAILURE();
        Cfile_search_cs.leave();
        return false;
    }
    cf_registered_search* search;
    if (use_first)
    {
        for (int i = (int)Cfile_num_registered_searches - 1; i >= 0; --i)
        {
            Cfile_registered_searches[i + 1] = Cfile_registered_searches[i];
        }
        search = &Cfile_registered_searches[0];
    }
    else
    {
        search = &Cfile_registered_searches[Cfile_num_registered_searches];
    }
    ++Cfile_num_registered_searches;
    search->m_search_type = search_type;
    search->m_media_type = media_type;
    if (search_type == CF_SEARCH_VDIR)
    {
        Cfile_vdir_enabled = true;
    }
    Cfile_search_cs.leave();
    return true;
}

// 0x00597CA0 (:1164)
void cf_remove_search(cf_search_types search_type, cf_io_media_types media_type)
{
    Cfile_search_cs.enter();
    for (unsigned int i = 0; i < Cfile_num_registered_searches; ++i)
    {
        if (Cfile_registered_searches[i].m_search_type == search_type && Cfile_registered_searches[i].m_media_type == media_type)
        {
            // Moves one entry more than needed (the one past the end), as the original does.
            memmove(&Cfile_registered_searches[i], &Cfile_registered_searches[i + 1], (Cfile_num_registered_searches - i) * sizeof(cf_registered_search));
            --Cfile_num_registered_searches;
            break;
        }
    }
    Cfile_search_cs.leave();
}

// 0x005C27F0 (:1204)
cfile* cf_open(const char* filename_in, const char* mode, vlib_platform disk_platform, bool async)
{
    return cf_open_internal(filename_in, mode, disk_platform, async, CF_IO_MEDIA_HDD);
}

// 0x005B5CC0 (:1229)
cfile* cf_open(unsigned char* buf, unsigned int buf_size, const char* open_mode, vlib_platform disk_platform)
{
    cfile* cfp = cf_get_free_cfile();
    if (cfp == nullptr)
    {
        VLIB_CRITICAL_FAILURE();
        return nullptr;
    }
    cfp->m_searched_system = CF_SEARCH_NONE;
    cfp->m_access_type = CF_IO_ACCESS_MEMORY;
    cfp->m_media_type = CF_IO_MEDIA_MEMORY;
    cfp->m_flags = 0x10; // the buffer belongs to the caller (cf_close does not free it)
    cfp->_m_memory_fp = cf_open_memory(buf, buf_size, open_mode, false, 0);
    cfp->mode = buf != nullptr && strchr(open_mode, 'r') != nullptr ? CF_MODE_READ : CF_MODE_WRITE;
    cfp->pos = 0;
    if (cfp->mode == CF_MODE_WRITE)
    {
        cfp->size = 0;
        cfp->max_write_size = buf_size;
    }
    else
    {
        cfp->size = buf_size;
    }
    if (Vlib_endianness[PLATFORM_PC] != Vlib_endianness[disk_platform])
    {
        cfp->m_flags |= 4;
    }
    strncpy(cfp->name, "mem mapped file", sizeof(cfp->name));
    strncpy(cfp->full_pathname, cfp->name, sizeof(cfp->full_pathname));
    return cfp;
}

// 0x005CF6A0 (:1310)
cfile* cf_open_buffered(const char* filename, mempool_base* pool, const char* open_mode, vlib_platform disk_platform)
{
    if (open_mode == nullptr || open_mode[0] != 'r')
    {
        return nullptr;
    }
    cfile* cfp = cf_open_internal(filename, open_mode, PLATFORM_PC, false, CF_IO_MEDIA_HDD);
    if (cfp == nullptr)
    {
        return nullptr;
    }
    const unsigned int size = cfp->size;
    unsigned char* buf = (unsigned char*)pool->alloc(size, 0);
    if (buf != nullptr && cf_read(buf, size, cfp, false) == size)
    {
        cf_close(cfp);
        return cf_open(buf, size, open_mode, disk_platform);
    }
    VLIB_CRITICAL_FAILURE();
    cf_close(cfp);
    return nullptr;
}

// 0x005CA590 (:1342)
bool cf_close(cfile* cfp)
{
    if (cfp == nullptr)
    {
        return false;
    }
    bool result;
    switch (cfp->m_access_type)
    {
    case CF_IO_ACCESS_STANDARD:
        result = keen_cf_close_file(cfp->m_standard_fp, cfp->m_media_type);
        break;
    case CF_IO_ACCESS_PACKFILE:
        result = keen_cf_close_packfile(cfp->m_packfile_fp, cfp->m_media_type);
        break;
    case CF_IO_ACCESS_MEMORY:
        if (cfp->mode == CF_MODE_WRITE && cfp->_m_memory_fp != nullptr && cfp->_m_memory_fp->buf != nullptr && (cfp->m_flags & 0x10) == 0)
        {
            free(cfp->_m_memory_fp->buf);
        }
        cf_close_memory(cfp->_m_memory_fp);
        cfp->_m_memory_fp = nullptr;
        result = true;
        break;
    default:
        VLIB_CRITICAL_FAILURE();
        return false;
    }
    cfp->reset();
    Cfile_list.free(cfp);
    return result;
}

// 0x005C2810 (:1398)
bool cf_exist(const char* filename_in)
{
    cf_found_info found_info;
    return cf_find_file(&found_info, filename_in, false);
}

// 0x00564B20 (:1410)
unsigned int cf_size(const cfile* p_cfile)
{
    if (p_cfile == nullptr)
    {
        return 0;
    }
    return p_cfile->size;
}

// 0x005C2850 (:1436)
bool cf_get_file_size(unsigned int* filesize, const char* filename, bool encrypted)
{
    cf_found_info found_info;
    if (!cf_find_file(&found_info, filename, encrypted))
    {
        return false;
    }
    *filesize = found_info.m_size;
    return true;
}

// 0x005C28D0 (:1471)
unsigned int cf_read(void* dest_buf, unsigned int num_bytes_to_read, cfile* p_cfile, bool abortable)
{
    if (p_cfile->m_error_code != CF_ERROR_NONE)
    {
        return 0;
    }
    if (p_cfile->mode != CF_MODE_READ)
    {
        p_cfile->m_error_code = CF_ERROR_READ;
        return 0;
    }
    if (p_cfile->pos >= p_cfile->size)
    {
        p_cfile->m_error_code = CF_ERROR_READ;
        return 0;
    }
    if (num_bytes_to_read == 0)
    {
        return 0;
    }
    if (p_cfile->pos + num_bytes_to_read > p_cfile->size)
    {
        num_bytes_to_read = p_cfile->size - p_cfile->pos;
        p_cfile->m_error_code = CF_ERROR_READ;
    }
    unsigned int num_read;
    switch (p_cfile->m_access_type)
    {
    case CF_IO_ACCESS_STANDARD:
        num_read = keen_cf_read(dest_buf, num_bytes_to_read, p_cfile->m_standard_fp, p_cfile->m_media_type, p_cfile->pos, abortable);
        break;
    case CF_IO_ACCESS_PACKFILE:
    {
        // Entries of condensed packfiles cannot be read; compressed entries only whole.
        keen_cf_packfile_handle* pfp = p_cfile->m_packfile_fp;
        const v_packfile* packfile = pfp->packfile_entry->parent;
        if ((packfile->flags & 2) != 0)
        {
            num_read = 0;
        }
        else if ((packfile->flags & 1) == 0)
        {
            num_read = keen_cf_read_packfile(dest_buf, pfp, p_cfile->m_media_type, num_bytes_to_read, p_cfile->pos);
        }
        else if (num_bytes_to_read == p_cfile->size)
        {
            num_read = keen_cf_read_packfile(dest_buf, pfp, p_cfile->m_media_type, pfp->packfile_entry->compressed_size, p_cfile->pos);
        }
        else
        {
            num_read = 0;
        }
        break;
    }
    case CF_IO_ACCESS_MEMORY:
        num_read = cf_read_memory(dest_buf, p_cfile, num_bytes_to_read);
        break;
    default:
        VLIB_CRITICAL_FAILURE();
        p_cfile->m_error_code = CF_ERROR_READ;
        return 0;
    }
    p_cfile->pos += num_read;
    if (num_read != num_bytes_to_read)
    {
        if ((p_cfile->m_flags & 8) != 0)
        {
            cf_reset_async_abort(p_cfile);
            p_cfile->m_error_code = CF_ERROR_ABORT;
            return num_read;
        }
        p_cfile->m_error_code = CF_ERROR_READ;
    }
    return num_read;
}

// 0x005C7EB0 (:1598)
unsigned int cf_read_compressed(void* dest_buf, unsigned int num_compressed_bytes_to_read, cfile* p_cfile, unsigned int expected_num_uncompressed_bytes_read)
{
    // The original passes the addresses of its two arguments as one-entry chains. cf_decompress_chain looks at the
    // entry after a filled buffer, so the chains here get an empty second entry instead of whatever follows on the
    // stack.
    unsigned char* chain_dest_buf[2] = { (unsigned char*)dest_buf, nullptr };
    unsigned int chain_num_bytes[2] = { expected_num_uncompressed_bytes_read, 0 };
    return cf_read_compressed_chain(chain_dest_buf, num_compressed_bytes_to_read, chain_num_bytes, 1, p_cfile);
}

// 0x005C2A30 (:1604)
unsigned int cf_read_compressed_chain(unsigned char** chain_dest_buf, unsigned int total_compressed_bytes_to_read, unsigned int* chain_num_bytes, unsigned int chain_num_reads, cfile* p_cfile)
{
    if (p_cfile->m_error_code != CF_ERROR_NONE)
    {
        return 0;
    }
    if (p_cfile->mode != CF_MODE_READ)
    {
        VLIB_CRITICAL_FAILURE();
        p_cfile->m_error_code = CF_ERROR_READ;
        return 0;
    }
    if (p_cfile->pos >= p_cfile->size)
    {
        VLIB_CRITICAL_FAILURE();
        p_cfile->m_error_code = CF_ERROR_READ;
        return 0;
    }
    if (total_compressed_bytes_to_read == 0)
    {
        return 0;
    }
    if (p_cfile->pos + total_compressed_bytes_to_read > p_cfile->size)
    {
        p_cfile->m_error_code = CF_ERROR_READ;
        return 0;
    }
    unsigned int num_read;
    switch (p_cfile->m_access_type)
    {
    case CF_IO_ACCESS_STANDARD:
        num_read = keen_cf_read_compressed_chain(chain_dest_buf, total_compressed_bytes_to_read, chain_num_bytes, p_cfile->m_standard_fp, p_cfile->m_media_type, p_cfile->pos);
        break;
    case CF_IO_ACCESS_PACKFILE:
    {
        keen_cf_packfile_handle* pfp = p_cfile->m_packfile_fp;
        const uint64_t file_pos = (uint64_t)pfp->packfile_entry->sector * 0x800 + p_cfile->pos;
        num_read = keen_cf_read_compressed_chain(chain_dest_buf, total_compressed_bytes_to_read, chain_num_bytes, pfp->file_handle, p_cfile->m_media_type, file_pos);
        break;
    }
    case CF_IO_ACCESS_MEMORY:
        num_read = cf_read_compressed_chain_memory(total_compressed_bytes_to_read, chain_dest_buf, chain_num_bytes, p_cfile->_m_memory_fp->buf + p_cfile->pos);
        break;
    default:
        VLIB_CRITICAL_FAILURE();
        return 0;
    }
    p_cfile->pos += num_read;
    unsigned int expected = 0;
    for (unsigned int i = 0; i < chain_num_reads; ++i)
    {
        expected += chain_num_bytes[i];
    }
    if (num_read != expected)
    {
        if ((p_cfile->m_flags & 8) != 0)
        {
            return 0;
        }
        p_cfile->m_error_code = CF_ERROR_READ;
    }
    return num_read;
}

// 0x00564B30 (:1714)
unsigned int cf_query_num_async_bytes_read(cfile* p_cfile)
{
    return 0;
}

// 0x00564B50 (:1752)
unsigned int cf_write(const void* source_buf, unsigned int num_bytes, cfile* p_cfile)
{
    if (num_bytes == 0)
    {
        return 0;
    }
    if (p_cfile->m_error_code != CF_ERROR_NONE)
    {
        return 0;
    }
    if (p_cfile->mode != CF_MODE_WRITE)
    {
        p_cfile->m_error_code = CF_ERROR_WRITE;
        return 0;
    }
    const unsigned int num_written = p_cfile->m_access_type == CF_IO_ACCESS_MEMORY ? cf_write_memory(source_buf, num_bytes, p_cfile) : 0;
    if (num_written != num_bytes)
    {
        p_cfile->m_error_code = CF_ERROR_WRITE;
    }
    p_cfile->pos += num_written;
    if (p_cfile->pos > p_cfile->size)
    {
        p_cfile->size = p_cfile->pos;
    }
    return num_written;
}

// 0x00564BD0 (:1805)
unsigned int cf_write(cfile* p_source_file, cfile* p_cfile)
{
    if (p_source_file->m_access_type == CF_IO_ACCESS_MEMORY && p_source_file->mode == CF_MODE_WRITE)
    {
        return cf_write(p_source_file->_m_memory_fp->buf, p_source_file->pos, p_cfile);
    }
    VLIB_CRITICAL_FAILURE();
    p_cfile->m_error_code = CF_ERROR_WRITE;
    return 0;
}

// 0x005C2BC0 (:1829)
bool cf_pad_align_read(unsigned int align, cfile* p_cfile)
{
    const unsigned int misalignment = p_cfile->pos % align;
    return cf_seek(p_cfile, misalignment == 0 ? 0 : (int)(align - misalignment), CF_SEEK_CUR);
}

// 0x00597D30 (:1845)
unsigned int cf_pad_align_write(unsigned int align, cfile* p_cfile)
{
    const unsigned int misalignment = p_cfile->pos % align;
    const unsigned int pad_size = misalignment == 0 ? 0 : align - misalignment;
    unsigned char zeros[64];
    memset(zeros, 0, sizeof(zeros));
    for (unsigned int written = 0; written < pad_size;)
    {
        unsigned int chunk = pad_size - written;
        if (chunk > sizeof(zeros))
        {
            chunk = sizeof(zeros);
        }
        cf_write(zeros, chunk, p_cfile);
        written += chunk;
    }
    return pad_size;
}

// 0x005B5E10 (:1873)
bool cf_seek(cfile* p_cfile, int relative_offset, cf_seek_mode seek_mode)
{
    unsigned int new_pos;
    switch (seek_mode)
    {
    case CF_SEEK_SET:
        new_pos = (unsigned int)relative_offset;
        break;
    case CF_SEEK_CUR:
        new_pos = p_cfile->pos + relative_offset;
        break;
    case CF_SEEK_END:
        new_pos = p_cfile->size + relative_offset;
        break;
    default:
        VLIB_CRITICAL_FAILURE();
        new_pos = 0;
        break;
    }
    if (seek_mode <= CF_SEEK_END && (int)new_pos < 0)
    {
        return false;
    }
    if (p_cfile->mode == CF_MODE_READ && new_pos > p_cfile->size)
    {
        return false;
    }
    if (p_cfile->pos == new_pos)
    {
        return true;
    }
    switch (p_cfile->m_access_type)
    {
    case CF_IO_ACCESS_STANDARD:
        if (!keen_cf_seek(p_cfile->m_standard_fp, relative_offset, seek_mode, p_cfile->m_media_type))
        {
            return false;
        }
        break;
    case CF_IO_ACCESS_PACKFILE:
    {
        const unsigned int flags = p_cfile->m_packfile_fp->packfile_entry->parent->flags;
        if ((flags & 1) != 0 || (flags & 2) != 0)
        {
            return false;
        }
        if (!keen_cf_seek_packfile(p_cfile->m_packfile_fp, relative_offset, seek_mode, p_cfile->m_media_type))
        {
            return false;
        }
        break;
    }
    case CF_IO_ACCESS_MEMORY:
        break;
    default:
        VLIB_CRITICAL_FAILURE();
        return false;
    }
    p_cfile->pos = new_pos;
    return true;
}

// 0x00564C30 (:1969)
unsigned int cf_tell(const cfile* p_cfile)
{
    return p_cfile->pos;
}

// 0x00597E00 (:1981)
int cf_sync(const cfile* p_cfile)
{
    if (p_cfile != nullptr)
    {
        switch (p_cfile->m_access_type)
        {
        case CF_IO_ACCESS_STANDARD:
        case CF_IO_ACCESS_MEMORY:
            return 1;
        case CF_IO_ACCESS_PACKFILE:
            break;
        default:
            VLIB_CRITICAL_FAILURE();
            break;
        }
    }
    return -1;
}

// 0x00597E40 (:2013)
void cf_wait_async(const cfile* p_cfile)
{
    const cf_io_access_types access_type = p_cfile->m_access_type;
    if (access_type != CF_IO_ACCESS_STANDARD && access_type != CF_IO_ACCESS_PACKFILE && access_type != CF_IO_ACCESS_MEMORY)
    {
        VLIB_CRITICAL_FAILURE();
    }
}

// 0x00564C40 (:2054)
bool cf_eof(const cfile* p_cfile)
{
    return p_cfile->pos >= p_cfile->size;
}

// 0x00564C60 (:2062)
void* cf_memory_fp(const cfile* p_cfile)
{
    if (p_cfile->m_access_type != CF_IO_ACCESS_MEMORY)
    {
        VLIB_CRITICAL_FAILURE();
        return nullptr;
    }
    return p_cfile->_m_memory_fp->buf;
}

// 0x00564C90 (:2077)
void* cf_cur_memory_fp(const cfile* p_cfile)
{
    if (p_cfile->m_access_type != CF_IO_ACCESS_MEMORY)
    {
        VLIB_CRITICAL_FAILURE();
        return nullptr;
    }
    return p_cfile->_m_memory_fp->buf + p_cfile->pos;
}

// 0x00564CD0 (:2098)
cf_error_codes cf_error(const cfile* p_cfile)
{
    return p_cfile->m_error_code;
}

// 0x00564CE0 (:2109)
void cf_clear_error(cfile* p_cfile)
{
    p_cfile->m_error_code = CF_ERROR_NONE;
}

static inline unsigned short cf_swap16(unsigned short value)
{
    return (unsigned short)((value << 8) | (value >> 8));
}

static inline unsigned int cf_swap32(unsigned int value)
{
    return ((value & 0xFF0000) | (value >> 16)) >> 8 | ((value & 0xFF00) | (value << 16)) << 8;
}

// Byte-swaps the value converted to an integer (decompiled; this mangles every non-integer float).
static inline float cf_swap_float(float value)
{
    const unsigned int swapped = cf_swap32((unsigned int)llrintf(value));
    return (float)swapped;
}

// 0x005C2C00 (:2119)
char cf_read_char(cfile* p_cfile)
{
    char value;
    cf_read(&value, 1, p_cfile, false);
    return value;
}

// 0x005C2C20 (:2120)
int8_t cf_read_int8(cfile* p_cfile)
{
    int8_t value;
    cf_read(&value, 1, p_cfile, false);
    return value;
}

// 0x005C2C40 (:2121)
unsigned char cf_read_uint8(cfile* p_cfile)
{
    unsigned char value;
    cf_read(&value, 1, p_cfile, false);
    return value;
}

// 0x005C2C60 (:2122)
short cf_read_int16(cfile* p_cfile)
{
    unsigned short value;
    cf_read(&value, 2, p_cfile, false);
    if ((p_cfile->m_flags & 4) != 0)
    {
        value = cf_swap16(value);
    }
    return (short)value;
}

// 0x005C2CA0 (:2123)
unsigned short cf_read_uint16(cfile* p_cfile)
{
    unsigned short value;
    cf_read(&value, 2, p_cfile, false);
    if ((p_cfile->m_flags & 4) != 0)
    {
        value = cf_swap16(value);
    }
    return value;
}

// 0x005C2CE0 (:2124)
int cf_read_int32(cfile* p_cfile)
{
    unsigned int value;
    cf_read(&value, 4, p_cfile, false);
    if ((p_cfile->m_flags & 4) != 0)
    {
        value = cf_swap32(value);
    }
    return (int)value;
}

// 0x005C2D30 (:2125)
unsigned int cf_read_uint32(cfile* p_cfile)
{
    unsigned int value;
    cf_read(&value, 4, p_cfile, false);
    if ((p_cfile->m_flags & 4) != 0)
    {
        value = cf_swap32(value);
    }
    return value;
}

// 0x005C2D80 (:2126)
float cf_read_float(cfile* p_cfile)
{
    float value;
    cf_read(&value, 4, p_cfile, false);
    if ((p_cfile->m_flags & 4) != 0)
    {
        value = cf_swap_float(value);
    }
    return value;
}

// 0x005C2E10 (:2133)
bool cf_read_bool(cfile* p_cfile)
{
    unsigned char value;
    cf_read(&value, 1, p_cfile, false);
    return value != 0;
}

// 0x005C7ED0 (:2139)
void cf_read_bbox(bbox& bb, cfile* p_cfile)
{
    bb.min.x = cf_read_float(p_cfile);
    bb.min.y = cf_read_float(p_cfile);
    bb.min.z = cf_read_float(p_cfile);
    bb.max.x = cf_read_float(p_cfile);
    bb.max.y = cf_read_float(p_cfile);
    bb.max.z = cf_read_float(p_cfile);
}

// 0x005C2E30 (:2150)
void cf_read_vector(vector& v, cfile* p_cfile)
{
    v.x = cf_read_float(p_cfile);
    v.y = cf_read_float(p_cfile);
    v.z = cf_read_float(p_cfile);
}

// 0x005C2E60 (:2162)
void cf_read_quaternion(quaternion& quat, cfile* p_cfile)
{
    quat.x = cf_read_float(p_cfile);
    quat.y = cf_read_float(p_cfile);
    quat.z = cf_read_float(p_cfile);
    quat.w = cf_read_float(p_cfile);
}

// 0x005C2EA0 (:2175)
void cf_read_matrix(matrix& mat, cfile* p_file)
{
    mat.rvec.x = cf_read_float(p_file);
    mat.rvec.y = cf_read_float(p_file);
    mat.rvec.z = cf_read_float(p_file);
    mat.uvec.x = cf_read_float(p_file);
    mat.uvec.y = cf_read_float(p_file);
    mat.uvec.z = cf_read_float(p_file);
    mat.fvec.x = cf_read_float(p_file);
    mat.fvec.y = cf_read_float(p_file);
    mat.fvec.z = cf_read_float(p_file);
}

// 0x005C2F00 (:2184)
bool cf_read_line(char* dest_buf, unsigned int dest_buf_size, cfile* p_cfile)
{
    // The '\n' is replaced by the terminator.
    if (dest_buf == nullptr || p_cfile == nullptr || dest_buf_size == 0)
    {
        return false;
    }
    if (p_cfile->pos < p_cfile->size)
    {
        bool line_end = false;
        while (--dest_buf_size != 0)
        {
            *dest_buf = cf_read_char(p_cfile);
            if (*dest_buf == '\n')
            {
                line_end = true;
                break;
            }
            ++dest_buf;
            if (p_cfile->pos >= p_cfile->size)
            {
                *dest_buf = '\0';
                return p_cfile->m_error_code == CF_ERROR_NONE;
            }
        }
        if (!line_end)
        {
            VLIB_CRITICAL_FAILURE();
        }
    }
    *dest_buf = '\0';
    return p_cfile->m_error_code == CF_ERROR_NONE;
}

// 0x005C2FA0 (:2239)
bool cf_read_string(char* dest_buf, unsigned int dest_buf_size, cfile* p_cfile)
{
    // 16-bit length, then the characters. What does not fit is skipped.
    if (p_cfile->pos >= p_cfile->size)
    {
        *dest_buf = '\0';
        return false;
    }
    unsigned short length;
    cf_read(&length, 2, p_cfile, false);
    if ((p_cfile->m_flags & 4) != 0)
    {
        length = cf_swap16(length);
    }
    const unsigned int num_bytes_to_read = length < dest_buf_size ? length : dest_buf_size - 1;
    dest_buf[cf_read(dest_buf, num_bytes_to_read, p_cfile, false)] = '\0';
    for (unsigned int i = num_bytes_to_read; i < length; ++i)
    {
        char skipped;
        cf_read(&skipped, 1, p_cfile, false);
    }
    return p_cfile->m_error_code == CF_ERROR_NONE;
}

// 0x005C3050 (:2277)
unsigned int cf_read_chksum_string(char* dest_buf, unsigned int dest_buf_size, unsigned int& chksum, cfile* p_cfile)
{
    chksum = 0;
    if (p_cfile->pos >= p_cfile->size)
    {
        *dest_buf = '\0';
        return 0;
    }
    unsigned short length;
    cf_read(&length, 2, p_cfile, false);
    if ((p_cfile->m_flags & 4) != 0)
    {
        length = cf_swap16(length);
    }
    const unsigned int num_bytes_to_read = length < dest_buf_size ? length : dest_buf_size - 1;
    const unsigned int num_read = cf_read(dest_buf, num_bytes_to_read, p_cfile, false);
    dest_buf[num_read] = '\0';
    chksum = crc_str(dest_buf, 0);
    for (unsigned int i = num_bytes_to_read; i < length; ++i)
    {
        char skipped;
        cf_read(&skipped, 1, p_cfile, false);
    }
    return num_read;
}

// 0x005C3110 (:2333)
bool cf_read_string_null_term(char* dest_buf, unsigned int dest_buf_size, cfile* p_cfile)
{
    // Characters that do not fit are skipped up to the terminator. Running into the end of the file returns false.
    unsigned int length = 0;
    if (p_cfile->pos >= p_cfile->size)
    {
        *dest_buf = '\0';
        return false;
    }
    char c;
    cf_read(&c, 1, p_cfile, false);
    while (c != '\0' && p_cfile->m_error_code == CF_ERROR_NONE)
    {
        if (length < dest_buf_size - 1)
        {
            dest_buf[length++] = c;
        }
        if (p_cfile->pos >= p_cfile->size)
        {
            dest_buf[length] = '\0';
            return false;
        }
        cf_read(&c, 1, p_cfile, false);
    }
    dest_buf[length] = '\0';
    return p_cfile->m_error_code == CF_ERROR_NONE;
}

// 0x005C31C0 (:2381)
unsigned int cf_read_chksum_string_null_term(char* dest_buf, unsigned int dest_buf_size, unsigned int& chksum, cfile* p_cfile)
{
    // The checksum is a CRC32 of the stored characters (as crc_str), updated per character.
    unsigned int length = 0;
    chksum = 0;
    if (p_cfile->pos >= p_cfile->size)
    {
        *dest_buf = '\0';
        return 0;
    }
    unsigned char c;
    cf_read(&c, 1, p_cfile, false);
    while (c != 0 && p_cfile->m_error_code == CF_ERROR_NONE)
    {
        if (length < dest_buf_size - 1)
        {
            dest_buf[length++] = (char)c;
            chksum = (chksum >> 8) ^ CRC_table[(c ^ chksum) & 0xFF];
        }
        if (p_cfile->pos >= p_cfile->size)
        {
            break;
        }
        cf_read(&c, 1, p_cfile, false);
    }
    dest_buf[length] = '\0';
    return length;
}

// 0x00564CF0 (:2427)
bool cf_write_char(char c, cfile* p_cfile)
{
    cf_write(&c, 1, p_cfile);
    return p_cfile->m_error_code == CF_ERROR_NONE;
}

// 0x00564D80 (:2428)
bool cf_write_int8(int8_t v, cfile* p_cfile)
{
    cf_write(&v, 1, p_cfile);
    return p_cfile->m_error_code == CF_ERROR_NONE;
}

// 0x00564E10 (:2429)
bool cf_write_uint8(unsigned char v, cfile* p_cfile)
{
    cf_write(&v, 1, p_cfile);
    return p_cfile->m_error_code == CF_ERROR_NONE;
}

// 0x00564EA0 (:2430)
bool cf_write_int16(short v, cfile* p_cfile)
{
    unsigned short value = (unsigned short)v;
    if ((p_cfile->m_flags & 4) != 0)
    {
        value = cf_swap16(value);
    }
    cf_write(&value, 2, p_cfile);
    return p_cfile->m_error_code == CF_ERROR_NONE;
}

// 0x00564F50 (:2431)
bool cf_write_uint16(unsigned short v, cfile* p_cfile)
{
    if ((p_cfile->m_flags & 4) != 0)
    {
        v = cf_swap16(v);
    }
    cf_write(&v, 2, p_cfile);
    return p_cfile->m_error_code == CF_ERROR_NONE;
}

// 0x00564FF0 (:2432)
bool cf_write_int32(int v, cfile* p_cfile)
{
    unsigned int value = (unsigned int)v;
    if ((p_cfile->m_flags & 4) != 0)
    {
        value = cf_swap32(value);
    }
    cf_write(&value, 4, p_cfile);
    return p_cfile->m_error_code == CF_ERROR_NONE;
}

// 0x005650B0 (:2433)
bool cf_write_uint32(unsigned int v, cfile* p_cfile)
{
    if ((p_cfile->m_flags & 4) != 0)
    {
        v = cf_swap32(v);
    }
    cf_write(&v, 4, p_cfile);
    return p_cfile->m_error_code == CF_ERROR_NONE;
}

// 0x00565170 (:2434)
bool cf_write_float(float v, cfile* p_cfile)
{
    if ((p_cfile->m_flags & 4) != 0)
    {
        v = cf_swap_float(v);
    }
    cf_write(&v, 4, p_cfile);
    return p_cfile->m_error_code == CF_ERROR_NONE;
}

// 0x00565270 (:2439)
bool cf_write_bool(bool v, cfile* p_cfile)
{
    cf_write(&v, 1, p_cfile);
    return p_cfile->m_error_code == CF_ERROR_NONE;
}

// 0x00565300 (:2446)
bool cf_write_vector(const vector& v, cfile* p_cfile)
{
    return cf_write_float(v.x, p_cfile) && cf_write_float(v.y, p_cfile) && cf_write_float(v.z, p_cfile);
}

// 0x00565360 (:2450)
bool cf_write_matrix(const matrix& m, cfile* p_cfile)
{
    return cf_write_float(m.rvec.x, p_cfile) && cf_write_float(m.rvec.y, p_cfile) && cf_write_float(m.rvec.z, p_cfile) && cf_write_float(m.uvec.x, p_cfile) && cf_write_float(m.uvec.y, p_cfile) && cf_write_float(m.uvec.z, p_cfile) && cf_write_vector(m.fvec, p_cfile);
}

// 0x00565420 (:2454)
bool cf_write_quaternion(const quaternion& q, cfile* p_cfile)
{
    return cf_write_float(q.x, p_cfile) && cf_write_float(q.y, p_cfile) && cf_write_float(q.z, p_cfile) && cf_write_float(q.w, p_cfile);
}

// 0x005654A0 (:2459)
bool cf_write_bbox(const bbox& b, cfile* p_cfile)
{
    return cf_write_float(b.min.x, p_cfile) && cf_write_float(b.min.y, p_cfile) && cf_write_float(b.min.z, p_cfile) && cf_write_float(b.max.x, p_cfile) && cf_write_float(b.max.y, p_cfile) && cf_write_float(b.max.z, p_cfile);
}

// 0x00565550 (:2472)
bool cf_write_string(const char* str, cfile* p_cfile)
{
    const unsigned short length = (unsigned short)strlen(str);
    if (p_cfile->m_error_code != CF_ERROR_NONE)
    {
        return false;
    }
    cf_write(&length, 2, p_cfile);
    if (p_cfile->m_error_code != CF_ERROR_NONE)
    {
        return false;
    }
    cf_write(str, length, p_cfile);
    return p_cfile->m_error_code == CF_ERROR_NONE;
}

// 0x00565600 (:2503)
bool cf_write_string_null_term(const char* str, cfile* p_cfile)
{
    cf_write(str, (unsigned int)strlen(str) + 1, p_cfile);
    return p_cfile->m_error_code == CF_ERROR_NONE;
}

// 0x00565640 (:2525)
bool cf_write_text(const char* text, cfile* p_cfile)
{
    cf_write(text, (unsigned int)strlen(text), p_cfile);
    return p_cfile->m_error_code == CF_ERROR_NONE;
}

// 0x00565680 (:2541)
bool cf_printf(cfile* p_cfile, const char* fmt, ...)
{
    char buffer[4096];
    if (p_cfile == nullptr)
    {
        return false;
    }
    va_list args;
    va_start(args, fmt);
    vsprintf(buffer, fmt, args);
    va_end(args);
    cf_write(buffer, (unsigned int)strlen(buffer), p_cfile);
    return p_cfile->m_error_code == CF_ERROR_NONE;
}

typedef str_hash_table_cached<32768, unsigned short> cf_found_name_table;

// The name tables of cf_find_ext/_path/_pattern live in the temp pool (0x40008 bytes) for the duration of the call.
static cf_found_name_table* cf_create_found_name_table()
{
    void* memory = Vlib_temp_mempool->alloc(sizeof(cf_found_name_table), 0);
    return memory != nullptr ? new (memory) cf_found_name_table() : nullptr;
}

// Adds name to files[] unless an earlier search already returned it.
static void cf_add_found_name(const char** files, unsigned int& num_files, cf_found_name_table* found_names, const char* name)
{
    if (!found_names->exists(name))
    {
        files[num_files++] = name;
        found_names->insert(name);
    }
}

// 0x005C3280 (:2566)
unsigned int cf_find_ext(const char** files, unsigned int max_files, const char* ext)
{
    const unsigned int mark = Vlib_temp_mempool->mark();
    cf_found_name_table* found_names = cf_create_found_name_table();
    unsigned int num_files = 0;
    for (unsigned int i = 0; i < Cfile_num_registered_searches; ++i)
    {
        int offset = 0;
        if (Cfile_registered_searches[i].m_search_type == CF_SEARCH_PACKFILE)
        {
            for (const char* name = packfile_find_ext(offset, ext); offset != -1 && num_files < max_files; name = packfile_find_ext(offset, ext))
            {
                cf_add_found_name(files, num_files, found_names, name);
            }
        }
        else if (Cfile_registered_searches[i].m_search_type == CF_SEARCH_VDIR)
        {
            for (const char* name = vdir_find_ext(offset, ext); offset != -1 && num_files < max_files; name = vdir_find_ext(offset, ext))
            {
                cf_add_found_name(files, num_files, found_names, name);
            }
        }
    }
    found_names->~cf_found_name_table();
    Vlib_temp_mempool->restore_to_mark(mark);
    return num_files;
}

// 0x005B5FA0 (:2632)
unsigned int cf_find_ext_path(const char** files, unsigned int max_files, const char* ext, const char* path)
{
    // The path only applies to vdir; packfile names have no directories.
    Cfile_search_cs.enter();
    const unsigned int mark = Vlib_temp_mempool->mark();
    cf_found_name_table* found_names = cf_create_found_name_table();
    unsigned int num_files = 0;
    for (unsigned int i = 0; i < Cfile_num_registered_searches; ++i)
    {
        int offset = 0;
        if (Cfile_registered_searches[i].m_search_type == CF_SEARCH_PACKFILE)
        {
            for (const char* name = packfile_find_ext(offset, ext); offset != -1 && num_files < max_files; name = packfile_find_ext(offset, ext))
            {
                cf_add_found_name(files, num_files, found_names, name);
            }
        }
        else if (Cfile_registered_searches[i].m_search_type == CF_SEARCH_VDIR)
        {
            for (const char* name = vdir_find_ext_path(offset, ext, path); offset != -1 && num_files < max_files; name = vdir_find_ext_path(offset, ext, path))
            {
                cf_add_found_name(files, num_files, found_names, name);
            }
        }
    }
    found_names->~cf_found_name_table();
    Vlib_temp_mempool->restore_to_mark(mark);
    Cfile_search_cs.leave();
    return num_files;
}

// 0x005B61F0 (:2700)
unsigned int cf_find_pattern(const char** files, unsigned int max_files, const char* pattern)
{
    Cfile_search_cs.enter();
    const unsigned int mark = Vlib_temp_mempool->mark();
    cf_found_name_table* found_names = cf_create_found_name_table();
    unsigned int num_files = 0;
    for (unsigned int i = 0; i < Cfile_num_registered_searches; ++i)
    {
        int offset = 0;
        if (Cfile_registered_searches[i].m_search_type == CF_SEARCH_PACKFILE)
        {
            for (const char* name = packfile_find_pattern(offset, pattern); offset != -1 && num_files < max_files; name = packfile_find_pattern(offset, pattern))
            {
                cf_add_found_name(files, num_files, found_names, name);
            }
        }
        else if (Cfile_registered_searches[i].m_search_type == CF_SEARCH_VDIR)
        {
            for (const char* name = vdir_find_pattern(offset, pattern); offset != -1 && num_files < max_files; name = vdir_find_pattern(offset, pattern))
            {
                cf_add_found_name(files, num_files, found_names, name);
            }
        }
    }
    found_names->~cf_found_name_table();
    Vlib_temp_mempool->restore_to_mark(mark);
    Cfile_search_cs.leave();
    return num_files;
}

// 0x00565720 (:2770)
const char* cf_filename(const cfile* p_cfile)
{
    return p_cfile->name;
}

// 0x00565730 (:2782)
const char* cf_full_pathname(const cfile* p_cfile)
{
    return p_cfile->full_pathname;
}

// 0x00565740 (:2794)
cf_io_media_types cf_media_type(const cfile* p_cfile)
{
    return p_cfile->m_media_type;
}

// Last '\\' or '/' in filename, nullptr when there is none.
static const char* cf_find_last_separator(const char* filename)
{
    const char* backslash = strrchr(filename, '\\');
    const char* slash = strrchr(filename, '/');
    if (backslash == nullptr)
    {
        return slash;
    }
    return slash == nullptr || slash < backslash ? backslash : slash;
}

// 0x00565750 (:2805)
void cf_drop_path(char* new_filename, unsigned int new_filename_array_size, const char* filename)
{
    const char* separator = cf_find_last_separator(filename);
    if (separator != nullptr)
    {
        filename = separator + 1;
    }
    strncpy(new_filename, filename, new_filename_array_size);
    new_filename[new_filename_array_size - 1] = '\0';
}

// Characters of path from start on, counting a doubled backslash once.
static unsigned int cf_normalized_path_length(const char* path, unsigned int start, unsigned int length)
{
    unsigned int normalized_length = 0;
    for (unsigned int i = start; i < length; ++i)
    {
        if (i == 0 || path[i] != '\\' || path[i - 1] != '\\')
        {
            ++normalized_length;
        }
    }
    return normalized_length;
}

// 0x00597E70 (:2864)
bool cf_subtract_path(char* dst, unsigned int dest_len, const char* full_path, const char* base_path)
{
    // The original also builds normalized copies of both paths ('/' -> '\\', doubled '\\' collapsed) in a scratch
    // buffer, but only their lengths are used; the comparison and the copy work on the paths as given.
    const unsigned int base_length = (unsigned int)strlen(base_path);
    if (base_length == 0)
    {
        strncpy(dst, full_path, dest_len);
        return true;
    }
    const unsigned int full_length = (unsigned int)strlen(full_path);
    unsigned int full_start = 0;
    unsigned int base_start = 0;
    if (full_path[1] == ':')
    {
        if (base_path[1] != ':')
        {
            full_start = 2;
        }
    }
    else if (base_path[1] == ':')
    {
        base_start = 2;
    }
    const unsigned int full_normalized_length = cf_normalized_path_length(full_path, full_start, full_length);
    unsigned int base_normalized_length = cf_normalized_path_length(base_path, base_start, base_length);
    if (base_normalized_length > full_normalized_length || keen::compareStringNoCase(full_path, base_path, base_normalized_length) != 0)
    {
        return false;
    }
    if (full_path[base_normalized_length] == '\\')
    {
        ++base_normalized_length;
    }
    unsigned int count = full_normalized_length - base_normalized_length + 1;
    if (count > dest_len)
    {
        count = dest_len;
    }
    strncpy(dst, full_path + base_normalized_length, count);
    return true;
}

// 0x005657B0 (:2946)
bool cf_get_path(char* dest_path, unsigned int dest_path_size, const char* filename)
{
    // The path keeps its trailing separator. dest_path_size is not checked.
    const char* separator = cf_find_last_separator(filename);
    if (separator == nullptr)
    {
        return false;
    }
    const size_t length = separator + 1 - filename;
    strncpy(dest_path, filename, length);
    dest_path[length] = '\0';
    return true;
}

// 0x00565800 (:2983)
bool cf_get_os_full_name(char* fullname, unsigned int fullname_array_size, const char* filename, cf_io_media_types media_type)
{
    keen::debug::nativePrintArgs("This should not be called...\n");
    strncpy(fullname, filename, fullname_array_size);
    return false;
}

// 0x00565830 (:3007)
void cf_drop_ext(char* new_filename, unsigned int new_filename_array_size, const char* filename)
{
    if (filename == nullptr)
    {
        return;
    }
    const char* dot = strrchr(filename, '.');
    if (dot == nullptr)
    {
        strncpy(new_filename, filename, new_filename_array_size);
        new_filename[new_filename_array_size - 1] = '\0';
        return;
    }
    size_t count = dot - filename;
    if (count > new_filename_array_size - 1)
    {
        count = new_filename_array_size - 1;
    }
    strncpy(new_filename, filename, count);
    new_filename[count] = '\0';
}

// 0x00598010 (:3052)
bool cf_add_ext(char* new_filename, unsigned int new_filename_array_size, const char* filename, const char* ext, bool dont_replace)
{
    if (dont_replace)
    {
        strncpy(new_filename, filename, new_filename_array_size);
        new_filename[new_filename_array_size - 1] = '\0';
    }
    else
    {
        cf_drop_ext(new_filename, new_filename_array_size, filename);
    }
    const size_t ext_length = strlen(ext) + (ext[0] != '.' ? 1 : 0);
    if (strlen(new_filename) + ext_length > new_filename_array_size - 1)
    {
        return false;
    }
    if (ext[0] != '.')
    {
        strcat(new_filename, ".");
    }
    strcat(new_filename, ext);
    return true;
}

// 0x00565890 (:3097)
const char* cf_get_ext(const char* filename)
{
    const char* dot = strrchr(filename, '.');
    if (dot != nullptr && strchr(dot, '/') == nullptr && strchr(dot, '\\') == nullptr)
    {
        return dot;
    }
    return "";
}

// 0x005658D0 (:3121)
bool cf_add_suffix(char* new_filename, unsigned int new_filename_array_size, const char* filename, const char* suffix)
{
    // "name.ext" + "suffix" -> "name_suffix.ext"
    char ext[40];
    strcpy(ext, cf_get_ext(filename));
    if (strlen(filename) + strlen(suffix) + 2 > new_filename_array_size)
    {
        return false;
    }
    cf_drop_ext(new_filename, new_filename_array_size, filename);
    strcat(new_filename, "_");
    strcat(new_filename, suffix);
    strcat(new_filename, ext);
    return true;
}

// 0x005C34C0 (:3158)
unsigned int cf_refresh_file_size(const char* filename)
{
    if (!Cfile_vdir_enabled)
    {
        return 0;
    }
    return vdir_refresh_file_size(filename);
}

// 0x005C34E0 (:3168)
void cf_refresh_file_size_ext(const char* ext)
{
    if (Cfile_vdir_enabled)
    {
        vdir_refresh_file_size_ext(ext);
    }
}

// 0x005C34F0 (:3178)
void cf_refresh_all_sizes()
{
    if (Cfile_vdir_enabled)
    {
        vdir_refresh_all_sizes();
    }
}

// 0x005CF750 (:3188)
bool cf_copy_file(const char* dst_full_path, const char* src_filename)
{
    // The source stays open when the destination cannot be opened (as in the original).
    cfile* in = cf_open_internal(src_filename, "rb", PLATFORM_PC, false, CF_IO_MEDIA_HDD);
    if (in == nullptr)
    {
        return false;
    }
    cfile* out = cf_open_internal(dst_full_path, "wb", PLATFORM_PC, false, CF_IO_MEDIA_HDD);
    if (out == nullptr)
    {
        return false;
    }
    return cf_copy_file(in, out);
}

// 0x005CA6C0 (:3201)
bool cf_copy_file(cfile* in, cfile* out)
{
    // Copies in 128 KB blocks and closes both files.
    unsigned int remaining = in != nullptr ? in->size : 0;
    virtual_mempool pool;
    pool.init(Vlib_temp_page_pool, "cf_copy_pool", 0x20000, false, 0x1000);
    void* buffer = pool.alloc(0x20000, 0);
    while (remaining != 0)
    {
        const unsigned int block_size = remaining < 0x20000 ? remaining : 0x20000;
        const unsigned int num_read = cf_read(buffer, block_size, in, false);
        if (num_read == 0)
        {
            cf_close(in);
            cf_close(out);
            return false;
        }
        remaining -= block_size;
        if (cf_write(buffer, num_read, out) != num_read)
        {
            cf_close(in);
            cf_close(out);
            return false;
        }
    }
    cf_close(in);
    cf_close(out);
    return true;
}

// 0x005980D0 (:3247)
unsigned int cf_decompress_chain(unsigned int compressed_bytes_read, unsigned char* compressed_buf, zlib_stream* decompressor, unsigned char** dest_buf, unsigned int* num_bytes_to_read, unsigned char*& dest_buf_pos, unsigned int& dest_buf_index, unsigned int& dest_buf_read_remaining, unsigned char* buffer_space, unsigned int buffer_size)
{
    // Inflates compressed_buf through buffer_space and spreads the output over the chain of destination buffers
    // (dest_buf[i] receives num_bytes_to_read[i] bytes; a null buffer drops its share). The chain position is kept
    // across calls. Returns the number of bytes inflated, 0xFFFFFFFF on a zlib error.
    unsigned int total_written = 0;
    for (;;)
    {
        unsigned int num_written = 0;
        unsigned int num_read = 0;
        const zlib_result result = decompressor->decompress(buffer_space, buffer_size, &num_written, compressed_buf, compressed_bytes_read, &num_read);
        compressed_buf += num_read;
        compressed_bytes_read -= num_read;
        if (result == ZR_ERROR)
        {
            return 0xFFFFFFFF;
        }
        total_written += num_written;
        unsigned char* source = buffer_space;
        for (unsigned int remaining = num_written; remaining != 0;)
        {
            if (remaining < dest_buf_read_remaining)
            {
                if (dest_buf_pos != nullptr)
                {
                    memcpy(dest_buf_pos, source, remaining);
                    dest_buf_pos += remaining;
                }
                dest_buf_read_remaining -= remaining;
                break;
            }
            // This buffer is full: move on to the next one in the chain.
            if (dest_buf_pos != nullptr)
            {
                memcpy(dest_buf_pos, source, dest_buf_read_remaining);
            }
            const unsigned int num_copied = dest_buf_read_remaining;
            ++dest_buf_index;
            source += num_copied;
            dest_buf_pos = dest_buf[dest_buf_index];
            dest_buf_read_remaining = num_bytes_to_read[dest_buf_index];
            remaining -= num_copied;
        }
        if (result != ZR_MORE_MEM)
        {
            return total_written;
        }
    }
}

// 0x005981D0 (:3330)
zlib_stream* cf_alloc_decompressor()
{
    Cfile_decompress_cs.enter();
    int index = -1;
    for (int i = 0; i < 2; ++i)
    {
        if (!Cfile_decompress_in_use[i])
        {
            Cfile_decompress_in_use[i] = true;
            index = i;
            break;
        }
    }
    Cfile_decompress_cs.leave();
    return index != -1 ? &Cfile_decompress[index] : nullptr;
}

// 0x00598220 (:3354)
void cf_release_decompressor(zlib_stream* decom_to_free)
{
    Cfile_decompress_cs.enter();
    for (int i = 0; i < 2; ++i)
    {
        if (&Cfile_decompress[i] == decom_to_free)
        {
            Cfile_decompress_in_use[i] = false;
        }
    }
    Cfile_decompress_cs.leave();
}

// 0x00565A10 (:3367)
bool cf_running_on_disc()
{
    return false;
}

// 0x00565A20 (:3376)
void cf_open_lock(bool lock)
{
    Cf_open_locked = lock;
}

// 0x00565A30 (:3381)
bool cf_is_locked()
{
    return Cf_open_locked;
}

// 0x00565A40 (:3387)
unsigned int cf_get_async_align()
{
    return 4;
}
