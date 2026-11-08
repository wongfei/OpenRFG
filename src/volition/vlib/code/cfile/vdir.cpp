// Original: volition/vlib/code/cfile/vdir.cpp
//
// The loose-file directory (see vdir.h). Names are looked up lower-case; the full name of an entry is
// "<directory>\\<name>", or just the name when the entry has no directory (dir_name == -1).
//
// Notes (decompiled behaviour, kept as is):
// - vdir_restore fails for a version 3+ header without extra header bytes (it reads them with cf_read and treats 0
//   bytes read as an error).
// - vdir_find_ext_path compares the directory of every candidate, also for entries without a directory.

#include "volition/vlib/code/cfile/vdir.h"
#include "volition/vlib/code/cfile/vdir_internal.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/cfile/cfile_internal.h"
#include "volition/vlib/code/cfile/keen_cfile.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include "keen/base/filesystem.hpp"
#include "keen/base/filesystemtypes.hpp"

#include <ctype.h>
#include <string.h>

const char* Vdir_filenames[8] = { "vdir_pc", "vdir_pc", "vdir_pc", "vdir_pc", "vdir_pc", "vdir_pc", "vdir_pc", "vdir_pc" }; // 0x01518A0C

// ---- file-static globals
static mempool Vdir_mempool; // 0x0163B198, "VDIR mempool", vdir_config::pool_size bytes
static unsigned char* Vdir_pool; // 0x01CE86A0, string pool followed by the file entries
static int Vdir_alloc_size = -1; // 0x01518A08, size of Vdir_pool (a refresh must fit in it)

// 0x005D19C0 (.h:25)
vdir_config::vdir_config()
    : pool_size(0x80000)
{
}

// Name of an entry: right after its vdir_file_entry record.
static inline const char* vdir_entry_name(const vdir_file_entry* entry)
{
    return (const char*)(entry + 1);
}

// Offset of the entry after the one at offset (records are padded to 4 bytes).
static inline int vdir_next_entry_offset(int offset)
{
    const char* name = vdir_entry_name((const vdir_file_entry*)(Vdir_file_entry_pool + offset));
    return (int)((offset + sizeof(vdir_file_entry) + strlen(name) + 1 + 3) & ~3u);
}

// 0x00566760 (:70)
static bool vdir_get_fullname(char* fullname, unsigned int fullname_array_size, const vdir_file_entry* found_entry)
{
    const char* name = vdir_entry_name(found_entry);
    const char* dir = "";
    const char* separator = "";
    size_t length = strlen(name) + 1;
    if (found_entry->dir_name != -1)
    {
        dir = Vdir_dir_string_pool + found_entry->dir_name;
        separator = "\\";
        length = strlen(dir) + strlen(name) + 2;
    }
    if (length > fullname_array_size)
    {
        VLIB_CRITICAL_FAILURE();
        return false;
    }
    fullname[0] = '\0';
    strcat(fullname, dir);
    strcat(fullname, separator);
    strcat(fullname, name);
    fullname[fullname_array_size - 1] = '\0';
    return true;
}

// 0x00599500 (:121)
bool vdir_system_init(vdir_config* config)
{
    unsigned char* buffer = new unsigned char[((config->pool_size >> 2) + 1) * 4];
    Vdir_mempool.init(buffer, config->pool_size, "VDIR mempool", 4, 4);
    vdir_hash_system_init(); // inlined in the original
    return true;
}

// 0x005CB1D0 (:155)
bool vdir_restore(vlib_platform platform, bool refresh)
{
    // Reads Vdir_filenames[platform] (with cf_open_lock lifted for the open) into Vdir_pool and rebuilds the hash
    // table. A refresh reuses Vdir_pool and fails when the new directory is larger.
    const bool locked = cf_is_locked();
    if (locked)
    {
        cf_open_lock(false);
    }
    cfile* fp = cf_open(Vdir_filenames[platform], "rb", platform, false);
    if (locked)
    {
        cf_open_lock(true);
    }
    if (fp == nullptr)
    {
        return false;
    }
    vdir_header header;
    cf_read(&header, sizeof(header), fp, false);
    if (fp->m_error_code != CF_ERROR_NONE)
    {
        cf_close(fp);
        VLIB_CRITICAL_FAILURE();
        return false;
    }
    if (header.version >= 3)
    {
        unsigned char extra_header[1020];
        if (header.min_compatible_version > 2 || header.header_size - sizeof(vdir_header) > 0xFF)
        {
            cf_close(fp);
            return false;
        }
        if (cf_read(extra_header, header.header_size - sizeof(vdir_header), fp, false) == 0)
        {
            cf_close(fp);
            VLIB_CRITICAL_FAILURE();
            return false;
        }
    }
    else if (header.version < 2)
    {
        cf_close(fp);
        return false;
    }
    const unsigned int size = header.file_entry_size + header.directory_size;
    if (refresh)
    {
        if (size > (unsigned int)Vdir_alloc_size)
        {
            cf_close(fp);
            return false;
        }
    }
    else
    {
        Vdir_pool = (unsigned char*)Vdir_mempool.alloc(size, 0);
        Vdir_alloc_size = (int)size;
    }
    cf_read(Vdir_pool, size, fp, false);
    if (fp->m_error_code != CF_ERROR_NONE)
    {
        cf_close(fp);
        VLIB_CRITICAL_FAILURE();
        return false;
    }
    Vdir_dir_string_pool = (char*)Vdir_pool;
    Vdir_file_entry_pool = Vdir_pool + header.directory_size;
    Vdir_file_entry_pool_size = header.file_entry_size;
    vdir_clear_hash_table();
    cf_close(fp);
    Vdir_file_entry_pool_used = header.file_entry_size;
    const bool swap = Vlib_endianness[platform] != Vlib_endianness[PLATFORM_PC];
    for (unsigned int offset = 0; offset < header.file_entry_size; offset = (unsigned int)vdir_next_entry_offset((int)offset))
    {
        vdir_file_entry* entry = (vdir_file_entry*)(Vdir_file_entry_pool + offset);
        if (swap)
        {
            unsigned int* fields = (unsigned int*)entry;
            for (int i = 0; i < 3; ++i)
            {
                const unsigned int value = fields[i];
                fields[i] = ((value & 0xFF0000) | (value >> 16)) >> 8 | ((value << 16) | (value & 0xFF00)) << 8;
            }
        }
        // next is rebuilt as the hash chain.
        const unsigned int hash = vdir_hash(vdir_entry_name(entry));
        entry->next = Vdir_hash_table[hash];
        Vdir_hash_table[hash] = (int)offset;
    }
    return true;
}

// 0x005B7560 (:351)
bool vdir_find(char* fullname, unsigned int fullname_array_size, const char* name, unsigned int* size)
{
    char lower_name[256];
    str_copy_to_lower(lower_name, sizeof(lower_name), name);
    vdir_file_entry* entry = vdir_internal_find(lower_name);
    if (entry == nullptr)
    {
        return false;
    }
    if (size != nullptr)
    {
        *size = entry->size;
    }
    if (fullname != nullptr && !vdir_get_fullname(fullname, fullname_array_size, entry))
    {
        return false;
    }
    return true;
}

// 0x00566890 (:389)
const char* vdir_find_ext(int& offset, const char* ext)
{
    // Iterates the entries; offset is the byte offset of the next entry, -1 once done. ext includes the dot and is
    // compared case-sensitively.
    if (offset < 0)
    {
        return nullptr;
    }
    while (offset < (int)Vdir_file_entry_pool_used)
    {
        const char* name = vdir_entry_name((const vdir_file_entry*)(Vdir_file_entry_pool + offset));
        const char* dot = strrchr(name, '.');
        offset = vdir_next_entry_offset(offset);
        if (dot != nullptr && strcmp(dot, ext) == 0)
        {
            return name;
        }
    }
    offset = -1;
    return nullptr;
}

// 0x00566930 (:439)
const char* vdir_find_ext_path(int& offset, const char* ext, const char* path)
{
    if (offset < 0)
    {
        return nullptr;
    }
    while (offset < (int)Vdir_file_entry_pool_used)
    {
        const vdir_file_entry* entry = (const vdir_file_entry*)(Vdir_file_entry_pool + offset);
        const char* name = vdir_entry_name(entry);
        const char* dot = strrchr(name, '.');
        offset = vdir_next_entry_offset(offset);
        if (dot != nullptr && strcmp(dot, ext) == 0 && strcmp(path, Vdir_dir_string_pool + entry->dir_name) == 0)
        {
            return name;
        }
    }
    offset = -1;
    return nullptr;
}

// 0x005995C0 (:490)
const char* vdir_find_pattern(int& offset, const char* pattern)
{
    if (offset < 0)
    {
        return nullptr;
    }
    while (offset < (int)Vdir_file_entry_pool_used)
    {
        const char* name = vdir_entry_name((const vdir_file_entry*)(Vdir_file_entry_pool + offset));
        offset = vdir_next_entry_offset(offset);
        if (str_matchi(name, pattern)) // str_match_internal(name, pattern, true) in the original
        {
            return name;
        }
    }
    offset = -1;
    return nullptr;
}

// 0x00566A20 (:519)
const char* vdir_get_filename_entry(int entry_index)
{
    return vdir_entry_name((const vdir_file_entry*)(Vdir_file_entry_pool + entry_index));
}

// 0x005B7610 (:527)
unsigned int vdir_refresh_file_size(const char* filename)
{
    // Reads the size of the file from the file system into its entry.
    char lower_name[256];
    str_copy_to_lower(lower_name, sizeof(lower_name), filename);
    vdir_file_entry* entry = vdir_internal_find(lower_name);
    if (entry == nullptr)
    {
        return 0;
    }
    char fullname[256];
    if (!vdir_get_fullname(fullname, sizeof(fullname), entry))
    {
        return 0;
    }
    keen::FileStatus status;
    if (keen::file::getFileStatusByName(&status, g_pFileSystem, fullname, keen::FileStatus::ValueMask_Size | keen::FileStatus::ValueMask_Exists) != keen::IoError_Ok)
    {
        return 0;
    }
    entry->size = (unsigned int)status.size;
    return (unsigned int)status.size;
}

// 0x005B76F0 (:566)
void vdir_refresh_file_size_ext(const char* ext)
{
    int offset = 0;
    for (const char* filename = vdir_find_ext(offset, ext); offset != -1; filename = vdir_find_ext(offset, ext))
    {
        vdir_refresh_file_size(filename);
    }
}

// 0x005B7740 (:577)
void vdir_refresh_all_sizes()
{
    for (int offset = 0; offset < (int)Vdir_file_entry_pool_used;)
    {
        const char* name = vdir_entry_name((const vdir_file_entry*)(Vdir_file_entry_pool + offset));
        offset = vdir_next_entry_offset(offset);
        char lower_name[256];
        size_t length = strlen(name);
        if (length > 0xFF)
        {
            length = 0xFF;
        }
        for (size_t i = 0; i < length; ++i)
        {
            lower_name[i] = (char)tolower(name[i]);
        }
        lower_name[length] = '\0';
        vdir_file_entry* entry = vdir_internal_find(lower_name);
        char fullname[256];
        keen::FileStatus status;
        if (entry != nullptr && vdir_get_fullname(fullname, sizeof(fullname), entry) && keen::file::getFileStatusByName(&status, g_pFileSystem, fullname, keen::FileStatus::ValueMask_Size | keen::FileStatus::ValueMask_Exists) == keen::IoError_Ok)
        {
            entry->size = (unsigned int)status.size;
        }
    }
}
