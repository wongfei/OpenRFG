// Original: volition/vlib/code/cfile/packfile.cpp
//
// Packfiles (.vpp_pc): registration, lookup and condensed reads. See packfile.h for the layout.
//
// Notes (decompiled behaviour, kept as is):
// - The packfile hash table uses linear probing from hash % (size - 103) up to the end of the table, without
//   wrapping. A lookup returns the last entry with the same name before the first empty slot. Removing an entry
//   empties its slot, which can cut off entries probed past it.
// - packfile_add counts the packfile even when reading its header fails.
// - packfile_temp_find_data returns the entry's data from the last temp packfile, not from the one the entry was found
//   in (the same when there is only one).
// - v_packfile_read_and_callback leaves its temp packfile open and stops at the first name without an extension.
// - packfile_read_condensed (by name) closes the file again after a failed header read already closed it; the
//   second close fails harmlessly.

#include "volition/vlib/code/cfile/packfile.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/cfile/cfile_internal.h"
#include "volition/vlib/code/cfile/pc_cfile.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include "keen/base/string.hpp"

#include <stdio.h>
#include <string.h>

// One registered file: the hash of its name and its directory entry. Packfile_file_info_hash chains them by
// get_mod_hash(hash) with linear probing (Max_skips). Placement inferred.
// size 0x8
struct packfile_file_info
{
    unsigned int hash; // 0x00
    v_packfile_entry* packfile_entry; // 0x04
};

// Pool marks of one permanent packfile, so packfile_remove can release its directory. Placement inferred.
// size 0xC
struct packfile_runtime_info
{
    unsigned int mempool_mark; // 0x00
    unsigned int mempool_end_mark; // 0x04
    cfile* fp; // 0x08
};

bool Packfiles_enabled; // 0x01CE8673

// ---- file-static globals (arrays allocated with new[] in packfile_init from packfile_config)
static v_packfile* Packfiles; // 0x01CE2768, [max_packfiles]
static packfile_runtime_info* Packfile_runtime_info; // 0x01CE25C0, [max_packfiles]
static int Packfile_count; // 0x01CE867C
static int Max_packfiles; // 0x01CE8680
static v_packfile* Temp_packfile; // 0x01CDB90C, [max_temp_packfiles]
static int* Temp_packfile_file_count_mark; // 0x01CE276C, Packfile_file_count before each temp packfile
static int Temp_packfile_count; // 0x01CE8684
static int Max_temp_packfiles; // 0x01CE8688
static packfile_file_info* Packfile_file_infos; // 0x01CE7FCC, [max_files]
static packfile_file_info** Packfile_file_info_hash; // 0x01CE7FD0, [Packfile_hash_size]
static int Packfile_file_count; // 0x01CE868C
static int Max_packfile_files; // 0x01CE8690
static int Packfile_hash_size; // 0x01CE8694, max_files * 2 + 100
static int Max_skips; // 0x01CE869C, longest probe run seen by add_to_packfile_hash_table
static v_packfile* Priority_packfile; // 0x01CE8698, packfile_set_priority
static mempool Packfile_mempool; // 0x015DAA70, "packfile_pool", directories of permanent packfiles
static unsigned char* Packfile_mempool_buffer; // 0x01CE8460
static mempool Temp_packfile_mempool; // 0x015DAAC0, "temp packfile"
static unsigned char* Temp_packfile_mempool_buffer; // 0x01CDB0EC

// 0x005D1A00 (.h:242)
packfile_config::packfile_config()
    : max_files(500)
    , max_packfiles(20)
    , pool_size(0x7800)
    , max_temp_packfiles(1)
    , temp_pool_size(0x7800)
{
}

// 0x005B5920 (.h:150)
void v_packfile_entry::fill_without_parent(const v_packfile_entry_serialized& in_serialized)
{
    name = in_serialized.name.to_pointer();
    sector = in_serialized.sector;
    start = in_serialized.start;
    hash = in_serialized.hash;
    size = in_serialized.size;
    compressed_size = in_serialized.compressed_size;
}

// 0x005C3550 (:96)
static bool packfile_read_block_buffer(void* buffer, int bytes_to_read, cfile* in)
{
    // Reads whole sectors only: the last partial sector goes through a sector buffer, so the file stays aligned.
    unsigned char sector_buffer[0x800];
    const unsigned int num_whole_bytes = (unsigned int)(bytes_to_read / 0x800 * 0x800);
    if (cf_read(buffer, num_whole_bytes, in, false) != num_whole_bytes)
    {
        return false;
    }
    const unsigned int num_remaining_bytes = (unsigned int)bytes_to_read - num_whole_bytes;
    if (num_remaining_bytes != 0)
    {
        if (cf_read(sector_buffer, sizeof(sector_buffer), in, false) != sizeof(sector_buffer))
        {
            return false;
        }
        cf_wait_async(in);
        memcpy((unsigned char*)buffer + num_whole_bytes, sector_buffer, num_remaining_bytes);
    }
    return true;
}

// 0x005983C0 (:134)
static void fixup_filenames(v_packfile* packfile)
{
    // Serialized names are offsets into the name block; make them self-relative (-1 means no name).
    for (unsigned int i = 0; i < packfile->num_files; ++i)
    {
        et_ptr_offset<char, 0>& name = packfile->dir_serialized[i].name;
        const int offset = name.get_offset();
        if (offset != -1)
        {
            name.set_offset((int)(packfile->filenames + offset - (char*)&name));
        }
    }
}

// 0x00565E10 (:149)
static int get_first_data_sector_offset(v_packfile* p)
{
    return (int)(((p->filename_size + 0x7FF) >> 11) + 1 + ((p->dir_size + 0x7FF) >> 11));
}

// 0x00565E40 (:166)
static void v_packfile_set_sectors(v_packfile* p)
{
    // Entries follow each other on sector boundaries (an empty entry still takes one sector). Not for condensed
    // packfiles, whose data block is one stream.
    if ((p->flags & 2) != 0)
    {
        return;
    }
    unsigned int sector = (unsigned int)get_first_data_sector_offset(p);
    for (unsigned int i = 0; i < p->num_files; ++i)
    {
        v_packfile_entry_serialized& entry = p->dir_serialized[i];
        entry.sector = sector;
        const unsigned int size = (p->flags & 1) == 0 ? entry.size : entry.compressed_size;
        sector += size == 0 ? 1 : (size + 0x7FF) >> 11;
    }
}

// 0x00565ED0 (:202)
static unsigned int get_mod_hash(unsigned int hash)
{
    return hash % (unsigned int)(Packfile_hash_size - 0x67);
}

// 0x00565EF0 (:208)
static void add_to_packfile_hash_table(packfile_file_info* info)
{
    const unsigned int bucket = get_mod_hash(info->hash);
    const int num_slots = Packfile_hash_size - (int)bucket;
    for (int i = 0; i < num_slots; ++i)
    {
        if (Packfile_file_info_hash[bucket + i] == nullptr)
        {
            Packfile_file_info_hash[bucket + i] = info;
            return;
        }
        if (Max_skips < i)
        {
            Max_skips = i;
        }
    }
}

// 0x00598400 (:228)
static packfile_file_info* get_from_packfile_hash_table(const char* filename)
{
    const unsigned int hash = str_hash_string(filename);
    const unsigned int bucket = get_mod_hash(hash);
    packfile_file_info* found = nullptr;
    const int num_slots = Packfile_hash_size - (int)bucket;
    for (int i = 0; i < num_slots; ++i)
    {
        packfile_file_info* info = Packfile_file_info_hash[bucket + i];
        if (info == nullptr)
        {
            return found;
        }
        if (info->hash == hash)
        {
            if (info->packfile_entry->name == nullptr)
            {
                return nullptr;
            }
            if (keen::compareStringNoCase(filename, info->packfile_entry->name) == 0)
            {
                found = info;
            }
        }
    }
    return nullptr;
}

// 0x00565F50 (:270)
static packfile_file_info* remove_from_packfile_hash_table(packfile_file_info* info)
{
    const unsigned int bucket = get_mod_hash(info->hash);
    const int num_slots = Packfile_hash_size - (int)bucket;
    for (int i = 0; i < num_slots; ++i)
    {
        if (Packfile_file_info_hash[bucket + i] == info)
        {
            Packfile_file_info_hash[bucket + i] = nullptr;
            return info;
        }
    }
    return nullptr;
}

// 0x005984D0 (:332)
static void vpackfile_process_directory_part_1(v_packfile* p)
{
    if ((int)(p->num_files + Packfile_file_count) > Max_packfile_files)
    {
        char message[256];
        sprintf(message, "Out of packfile infos! Need to increase maximum from %d!!\n", Max_packfile_files);
        keen::debug::nativePrintArgs(message);
        return;
    }
    fixup_filenames(p);
    if ((p->flags & 2) == 0)
    {
        v_packfile_set_sectors(p);
    }
}

// 0x00598530 (:354)
static void vpackfile_process_directory_part_2(v_packfile* p, bool register_file)
{
    for (int i = 0; i < (int)p->num_files; ++i)
    {
        v_packfile_entry* entry = &p->dir[i];
        entry->parent = p;
        if (register_file)
        {
            packfile_file_info* info = &Packfile_file_infos[Packfile_file_count++];
            info->hash = str_hash_string(entry->name);
            info->packfile_entry = entry;
            add_to_packfile_hash_table(info);
        }
    }
}

// 0x00565FA0 (:387)
static bool v_packfile_allocate_dir(v_packfile* p, bool perm_packfile, mempool_base* alloc_pool)
{
    if (!alloc_pool->can_alloc(p->dir_size + p->filename_size + 0x10, 0x10))
    {
        char message[256];
        sprintf(message, "Out of space in %s. size: %zu\n", alloc_pool->get_name(), (size_t)alloc_pool->space_max());
        keen::debug::nativePrintArgs(message);
        return false;
    }
    if (perm_packfile)
    {
        Packfile_runtime_info[Packfile_count - 1].mempool_mark = alloc_pool->mark();
    }
    p->dir_serialized = (v_packfile_entry_serialized*)alloc_pool->alloc(p->dir_size, 0x10);
    p->filenames = (char*)alloc_pool->alloc(p->filename_size, 0x10);
    if (perm_packfile)
    {
        Packfile_runtime_info[Packfile_count - 1].mempool_end_mark = alloc_pool->mark();
    }
    return true;
}

// 0x00566080 (:426)
static bool v_packfile_process_header(v_packfile* p, const unsigned char* data)
{
    const unsigned int sector = p->sector;
    const unsigned int flags = p->flags;
    char short_name[65];
    char pathname[256];
    strncpy(short_name, p->short_name, sizeof(short_name));
    strncpy(pathname, p->pathname, sizeof(pathname));
    memcpy(p, data, sizeof(v_packfile));
    if (p->descriptor != 0x51890ACE || p->version <= 2)
    {
        return false;
    }
    p->flags |= flags;
    strncpy(p->short_name, short_name, sizeof(p->short_name));
    strncpy(p->pathname, pathname, sizeof(p->pathname));
    if ((flags & 0x10) != 0)
    {
        p->sector = sector;
    }
    p->dir = nullptr;
    p->dir_serialized = nullptr;
    p->filenames = nullptr;
    p->data = nullptr;
    return true;
}

// 0x005CA850 (:480)
static bool v_packfile_read_header_and_directory(v_packfile* p, cfile* cfp, bool perm_packfile, bool register_file, mempool_base* alloc_mempool, mempool_base* preload_mempool, bool encrypted)
{
    // On failure the file is closed.
    strncpy(p->short_name, cfp->name, 0x3F);
    p->short_name[0x3F] = '\0';
    strncpy(p->pathname, cfp->full_pathname, 0xFE);
    p->pathname[0xFE] = '\0';

    unsigned char header[0x800];
    packfile_read_block_buffer(header, sizeof(header), cfp);
    cf_wait_async(cfp);
    if (!v_packfile_process_header(p, header) || !v_packfile_allocate_dir(p, perm_packfile, alloc_mempool))
    {
        cf_close(cfp);
        return false;
    }
    packfile_read_block_buffer(p->dir_serialized, (int)p->dir_size, cfp);
    packfile_read_block_buffer(p->filenames, (int)p->filename_size, cfp);
    vpackfile_process_directory_part_1(p);
    p->dir = (v_packfile_entry*)alloc_mempool->alloc(p->num_files * sizeof(v_packfile_entry), 0x10);
    for (unsigned int i = 0; i < p->num_files; ++i)
    {
        p->dir[i].fill_without_parent(p->dir_serialized[i]);
    }
    if (perm_packfile)
    {
        Packfile_runtime_info[Packfile_count - 1].mempool_end_mark = alloc_mempool->mark();
    }
    vpackfile_process_directory_part_2(p, register_file);

    if (preload_mempool == nullptr)
    {
        switch (cfp->m_media_type)
        {
        case CF_IO_MEDIA_HDD:
            p->flags |= 0x8;
            break;
        case CF_IO_MEDIA_DVD:
            p->flags |= 0x10;
            break;
        case CF_IO_MEDIA_HOST:
            p->flags |= 0x4;
            break;
        case CF_IO_MEDIA_MEMORY:
            p->flags |= 0x20;
            break;
        default:
            VLIB_CRITICAL_FAILURE();
            break;
        }
    }
    else
    {
        // Preloaded: the data block is read into the pool and the entries are served from memory.
        const unsigned int data_size = (p->flags & 1) == 0 ? p->data_size : p->compressed_data_size;
        p->data = (unsigned char*)preload_mempool->alloc(data_size, 1);
        const unsigned int num_read = cf_read(p->data, data_size, cfp, false);
        cf_wait_async(cfp);
        if (num_read != data_size)
        {
            cf_close(cfp);
            return false;
        }
        p->flags |= 0x20;
    }
    if (encrypted)
    {
        p->flags |= 0x80;
    }
    return true;
}

// 0x005B6B10 (:575)
void v_packfile_shutdown()
{
    Packfile_mempool.clear();
    Temp_packfile_mempool.clear();
    delete[] Packfiles;
    Packfiles = nullptr;
    delete[] Packfile_runtime_info;
    Packfile_runtime_info = nullptr;
    delete[] Temp_packfile;
    Temp_packfile = nullptr;
    delete[] Temp_packfile_file_count_mark;
    Temp_packfile_file_count_mark = nullptr;
    delete[] Packfile_file_infos;
    Packfile_file_infos = nullptr;
    delete[] Packfile_file_info_hash;
    Packfile_file_info_hash = nullptr;
    delete[] Packfile_mempool_buffer;
    Packfile_mempool_buffer = nullptr;
    delete[] Temp_packfile_mempool_buffer;
    Temp_packfile_mempool_buffer = nullptr;
}

// 0x005CAB20 (:636)
static bool packfile_temp_read_all_common(const char* packfile_name, v_packfile** p, cfile** cfp, mempool_base* preload, bool installed_file, bool encrypted)
{
    // An already loaded temp packfile counts as success (*cfp is not set then).
    if (packfile_temp_find_packfile(packfile_name) != nullptr)
    {
        return true;
    }
    if (Temp_packfile_count >= Max_temp_packfiles)
    {
        return false;
    }
    *cfp = cf_open(packfile_name, "rb", PLATFORM_PC, true);
    if (*cfp == nullptr)
    {
        return false;
    }
    Temp_packfile_file_count_mark[Temp_packfile_count] = Packfile_file_count;
    *p = &Temp_packfile[Temp_packfile_count++];
    if (v_packfile_read_header_and_directory(*p, *cfp, false, true, &Temp_packfile_mempool, preload, encrypted))
    {
        return true;
    }
    cf_close(*cfp);
    return false;
}

// 0x005CAC20 (:694)
static bool packfile_read_condensed(cfile* fp, mempool_base* header_pool, mempool_base* dest_pool, v_packfile* p, bool encrypted)
{
    // Reads the directory into header_pool and the whole (inflated) data block of a condensed packfile into
    // dest_pool.
    if (!v_packfile_read_header_and_directory(p, fp, false, false, header_pool, nullptr, encrypted) || (p->flags & 2) == 0)
    {
        return false;
    }
    p->data = (unsigned char*)dest_pool->alloc(p->data_size, 0);
    if (p->data == nullptr)
    {
        return false;
    }
    if ((p->flags & 1) == 0)
    {
        const unsigned int num_read = cf_read(p->data, p->data_size, fp, false);
        cf_wait_async(fp);
        return num_read == p->data_size;
    }
    // The original passes the addresses of two locals as one-entry chains; cf_decompress_chain looks one entry
    // ahead, so the chains here get an empty second entry.
    unsigned char* chain_dest_buf[2] = { p->data, nullptr };
    unsigned int chain_num_bytes[2] = { p->data_size, 0 };
    if (cf_read_compressed_chain(chain_dest_buf, p->compressed_data_size, chain_num_bytes, 1, fp) != p->data_size)
    {
        VLIB_CRITICAL_FAILURE();
        return false;
    }
    cf_wait_async(fp);
    return true;
}

// 0x005985F0 (:743)
void packfile_init(packfile_config* config)
{
    Max_temp_packfiles = config->max_temp_packfiles;
    Max_packfile_files = config->max_files;
    Max_packfiles = config->max_packfiles;
    Packfile_count = 0;
    Temp_packfile_count = 0;
    Packfile_file_count = 0;
    Packfiles = new v_packfile[Max_packfiles];
    memset(Packfiles, 0, config->max_packfiles * sizeof(v_packfile));
    Packfile_runtime_info = new packfile_runtime_info[config->max_packfiles];
    memset(Packfile_runtime_info, 0, config->max_packfiles * sizeof(packfile_runtime_info));
    Temp_packfile = new v_packfile[config->max_temp_packfiles];
    memset(Temp_packfile, 0, config->max_temp_packfiles * sizeof(v_packfile));
    Temp_packfile_file_count_mark = new int[config->max_temp_packfiles];
    memset(Temp_packfile_file_count_mark, 0, config->max_temp_packfiles * sizeof(int));
    Packfile_file_infos = new packfile_file_info[config->max_files];
    memset(Packfile_file_infos, 0, config->max_files * sizeof(packfile_file_info));
    Packfile_hash_size = config->max_files * 2 + 100;
    Packfile_file_info_hash = new packfile_file_info*[Packfile_hash_size];
    memset(Packfile_file_info_hash, 0, Packfile_hash_size * sizeof(packfile_file_info*));
    Packfile_mempool_buffer = new unsigned char[config->pool_size];
    Packfile_mempool.init(Packfile_mempool_buffer, config->pool_size, "packfile_pool", 0x10, 8);
    Temp_packfile_mempool_buffer = new unsigned char[config->temp_pool_size];
    Temp_packfile_mempool.init(Temp_packfile_mempool_buffer, config->temp_pool_size, "temp packfile", 0x10, 8);
    pc_packfile_init(); // inlined in the original (clears Cfile_pc_open_packfiles)
    Packfiles_enabled = true;
}

// 0x00566170 (:799)
void packfile_set_priority(v_packfile* p)
{
    Priority_packfile = p;
}

// 0x00598880 (:810)
v_packfile_entry* packfile_find(const char* filename, unsigned int packfile_media_flag)
{
    // Looks up the name without its path: the priority packfile first, then the hash table. An entry is only
    // returned when its packfile has the media flag asked for.
    char name[64];
    cf_drop_path(name, sizeof(name), filename);
    if (Priority_packfile != nullptr)
    {
        for (int i = 0; i < (int)Priority_packfile->num_files; ++i)
        {
            if (keen::compareStringNoCase(name, Priority_packfile->dir[i].name) == 0)
            {
                return &Priority_packfile->dir[i];
            }
        }
    }
    packfile_file_info* info = get_from_packfile_hash_table(name);
    if (info == nullptr)
    {
        return nullptr;
    }
    return (info->packfile_entry->parent->flags & packfile_media_flag) != 0 ? info->packfile_entry : nullptr;
}

// 0x00598990 (:851)
bool packfile_exists_in_priority(const char* filename)
{
    char name[64];
    cf_drop_path(name, sizeof(name), filename);
    if (Priority_packfile != nullptr)
    {
        for (int i = 0; i < (int)Priority_packfile->num_files; ++i)
        {
            if (keen::compareStringNoCase(name, Priority_packfile->dir[i].name) == 0)
            {
                return true;
            }
        }
    }
    return false;
}

// 0x00598A60 (:878)
v_packfile* packfile_find_packfile(const char* packfile_name)
{
    for (int i = 0; i < Packfile_count; ++i)
    {
        if (keen::compareStringNoCase(packfile_name, Packfiles[i].short_name) == 0)
        {
            return &Packfiles[i];
        }
    }
    return nullptr;
}

// 0x00566180 (:893)
const char* packfile_get_last_packfile_name()
{
    if (Packfile_count != 0)
    {
        return Packfiles[Packfile_count - 1].short_name;
    }
    return nullptr;
}

// 0x005661A0 (:908)
const char* packfile_find_ext(int& offset, const char* ext)
{
    // Iterates the registered files; offset is the next file index, -1 once done. ext includes the dot and is
    // compared case-sensitively.
    if (offset < 0)
    {
        return nullptr;
    }
    while (offset < Packfile_file_count)
    {
        const char* name = Packfile_file_infos[offset].packfile_entry->name;
        ++offset;
        if (strcmp(cf_get_ext(name), ext) == 0)
        {
            return name;
        }
    }
    offset = -1;
    return nullptr;
}

// 0x00598AC0 (:952)
const char* packfile_find_pattern(int& offset, const char* pattern)
{
    if (offset < 0)
    {
        return nullptr;
    }
    while (offset < Packfile_file_count)
    {
        const char* name = Packfile_file_infos[offset].packfile_entry->name;
        ++offset;
        if (str_matchi(name, pattern)) // str_match_internal(name, pattern, true) in the original
        {
            return name;
        }
    }
    offset = -1;
    return nullptr;
}

// 0x00598B20 (:974)
bool packfile_contains_file(const v_packfile* p, const char* filename)
{
    if (p == nullptr)
    {
        return false;
    }
    for (int i = 0; i < (int)p->num_files; ++i)
    {
        if (keen::compareStringNoCase(p->dir[i].name, filename) == 0)
        {
            return true;
        }
    }
    return false;
}

// 0x00598B80 (:989)
void packfile_set_thread_ownership()
{
}

// 0x00566260 (:1002)
bool packfile_validate_packfile_name(const char* packfile_pathname)
{
    // The name is cut to 63 characters before the length check, so only null fails.
    if (packfile_pathname == nullptr)
    {
        return false;
    }
    char name[64];
    cf_drop_path(name, sizeof(name), packfile_pathname);
    return strlen(name) <= 0x40;
}

// 0x005CAD00 (:1029)
bool packfile_verify_packfile(const char* packfile_pathname)
{
    // Checks the header and that the file is at least as large as header + directory + data.
    if (!packfile_validate_packfile_name(packfile_pathname))
    {
        return false;
    }
    cfile* in = cf_open(packfile_pathname, "rb", PLATFORM_PC, true);
    if (in == nullptr)
    {
        return false;
    }
    const unsigned int file_size = in->size;
    if (file_size < sizeof(v_packfile))
    {
        cf_close(in);
        return false;
    }
    v_packfile header;
    packfile_read_block_buffer(&header, sizeof(header), in);
    cf_wait_async(in);
    cf_close(in);
    if (header.descriptor != 0x51890ACE || header.version <= 2)
    {
        return false;
    }
    const unsigned int data_size = (header.flags & 1) == 0 ? header.data_size : header.compressed_data_size;
    return data_size + sizeof(v_packfile) + header.dir_size <= file_size;
}

// 0x005CAE00 (:1084)
bool packfile_get_header_info(cfile* f, v_packfile* p, mempool_base* dir_pool)
{
    return v_packfile_read_header_and_directory(p, f, false, false, dir_pool, nullptr, false);
}

// 0x005CAE20 (:1090)
bool packfile_get_header_info(const char* packfile_name, v_packfile* p, mempool_base* dir_pool)
{
    if (!packfile_validate_packfile_name(packfile_name))
    {
        return false;
    }
    cfile* cfp = cf_open(packfile_name, "rb", PLATFORM_PC, false);
    if (cfp == nullptr)
    {
        return false;
    }
    const bool result = v_packfile_read_header_and_directory(p, cfp, false, false, dir_pool, nullptr, false);
    cf_close(cfp);
    return result;
}

// 0x00566300 (:1112)
bool remove_packfiles_from_hash_table(v_packfile* p)
{
    // Removes the last p->num_files registered files (p must be the last packfile added).
    for (unsigned int i = 0; i < p->num_files; ++i)
    {
        remove_from_packfile_hash_table(&Packfile_file_infos[Packfile_file_count - 1 - (int)i]);
    }
    return true;
}

// 0x005B6C30 (:1135)
bool packfile_remove(const char* packfile_name)
{
    // Only the last permanent packfile can be removed, and only while nothing was allocated after its directory.
    if (!packfile_validate_packfile_name(packfile_name))
    {
        return false;
    }
    char name[64];
    cf_drop_path(name, sizeof(name), packfile_name);
    v_packfile* p = packfile_find_packfile(name);
    if (p == nullptr || p != &Packfiles[Packfile_count - 1] || Packfile_runtime_info[Packfile_count - 1].mempool_end_mark != Packfile_mempool.mark())
    {
        return false;
    }
    remove_packfiles_from_hash_table(p);
    Packfile_file_count -= p->num_files;
    --Packfile_count;
    Packfile_mempool.restore_to_mark(Packfile_runtime_info[Packfile_count].mempool_mark);
    return true;
}

// 0x005CAE80 (:1188)
bool packfile_add(const char* packfile_name, mempool_base* preload, bool installed_packfile, bool encrypted)
{
    if (!packfile_validate_packfile_name(packfile_name) || Packfile_count >= Max_packfiles)
    {
        return false;
    }
    char name[65];
    cf_drop_path(name, sizeof(name), packfile_name);
    if (packfile_find_packfile(name) != nullptr)
    {
        return true;
    }
    cfile* cfp = cf_open(packfile_name, "rb", PLATFORM_PC, false);
    if (cfp == nullptr)
    {
        return false;
    }
    v_packfile* p = &Packfiles[Packfile_count++];
    const bool result = v_packfile_read_header_and_directory(p, cfp, true, true, &Packfile_mempool, preload, encrypted);
    if (result)
    {
        cf_close(cfp);
    }
    return result;
}

// 0x005CAF80 (:1259)
bool packfile_temp_add(const char* packfile_name, mempool_base* preload, bool installed_file, bool encrypted)
{
    v_packfile* p;
    cfile* cfp = nullptr; // not set when the packfile is already loaded (garbage is closed in the original)
    if (!packfile_temp_read_all_common(packfile_name, &p, &cfp, preload, installed_file, encrypted))
    {
        return false;
    }
    cf_close(cfp);
    return true;
}

// 0x00598B90 (:1279)
v_packfile* packfile_temp_find_packfile(const char* packfile_name)
{
    for (int i = 0; i < Temp_packfile_count; ++i)
    {
        if (keen::compareStringNoCase(packfile_name, Temp_packfile[i].short_name) == 0)
        {
            return &Temp_packfile[i];
        }
    }
    return nullptr;
}

// 0x00598BF0 (:1302)
bool packfile_temp_find_data(const char* filename, unsigned char** buf, unsigned int& size)
{
    // Finds an entry of a preloaded temp packfile by name (and the hash stored in the directory).
    const unsigned int hash = str_hash_string(filename);
    v_packfile_entry* found = nullptr;
    int last_index = 0;
    for (int i = 0; i < Temp_packfile_count; ++i)
    {
        last_index = i;
        v_packfile& packfile = Temp_packfile[i];
        for (unsigned int j = 0; j < packfile.num_files; ++j)
        {
            if (packfile.dir[j].hash == hash && keen::compareStringNoCase(filename, packfile.dir[j].name) == 0)
            {
                found = &packfile.dir[j];
                break;
            }
        }
    }
    if (found != nullptr)
    {
        const v_packfile& last = Temp_packfile[last_index];
        if (last.data == nullptr)
        {
            return false;
        }
        size = found->size;
        *buf = last.data + found->start;
        return true;
    }
    size = 0;
    *buf = nullptr;
    return false;
}

// 0x00566390 (:1345)
void packfile_clear_last_temp_packfile()
{
    if (Temp_packfile_count > 0)
    {
        v_packfile* p = &Temp_packfile[Temp_packfile_count - 1];
        remove_packfiles_from_hash_table(p);
        Packfile_file_count -= p->num_files;
        --Temp_packfile_count;
        p->flags = 0;
    }
}

// 0x00598D20 (:1369)
void packfile_clear_temp_packfiles()
{
    for (int i = Temp_packfile_count - 1; i >= 0; --i)
    {
        v_packfile* p = &Temp_packfile[i];
        remove_packfiles_from_hash_table(p);
        Packfile_file_count -= p->num_files;
        p->flags = 0;
    }
    Temp_packfile_count = 0;
    Temp_packfile_mempool.clear();
}

// 0x005CAFC0 (:1408)
bool v_packfile_read_and_callback(const char* packfile_name, int num_callbacks, v_packfile_callback* callback_infos)
{
    // Loads the packfile as a temp packfile and calls the reader registered for each entry's extension.
    v_packfile* p = nullptr;
    cfile* cfp = nullptr;
    if (!packfile_temp_read_all_common(packfile_name, &p, &cfp, nullptr, false, false))
    {
        return false;
    }
    if ((p->flags & 2) != 0)
    {
        cf_close(cfp);
        return false;
    }
    for (unsigned int i = 0; i < p->num_files; ++i)
    {
        const char* ext = strrchr(p->dir[i].name, '.');
        if (ext == nullptr)
        {
            return true;
        }
        for (int j = 0; j < num_callbacks; ++j)
        {
            if (keen::compareStringNoCase(ext + 1, callback_infos[j].ext) == 0)
            {
                callback_infos[j].pfnReader(cfp, &p->dir[i]);
                break;
            }
        }
    }
    return true;
}

// 0x005CB0B0 (:1500)
bool packfile_read_condensed(const char* packfile_name, mempool_base* header_pool, mempool_base* dest_pool, v_packfile* p, bool encrypted)
{
    memset(p, 0, sizeof(v_packfile));
    cfile* fp = cf_open(packfile_name, "rb", PLATFORM_PC, true);
    if (fp == nullptr)
    {
        return false;
    }
    const bool result = packfile_read_condensed(fp, header_pool, dest_pool, p, encrypted);
    cf_close(fp);
    return result;
}

// 0x005CB170 (:1546)
bool packfile_read_condensed(unsigned char* packfile_loc, unsigned int packfile_size, mempool_base* dest_pool, v_packfile* p)
{
    memset(p, 0, sizeof(v_packfile));
    cfile* fp = cf_open(packfile_loc, packfile_size, "rb", PLATFORM_PC);
    if (fp == nullptr)
    {
        return false;
    }
    const bool result = packfile_read_condensed(fp, dest_pool, dest_pool, p, false);
    cf_close(fp);
    return result;
}

// 0x005663E0 (:1574)
uint64_t packfile_get_absolute_offset(v_packfile_entry* pe)
{
    return (uint64_t)pe->sector * 0x800;
}

// 0x00566400 (:1579)
uint64_t packfile_get_absolute_data_offset(v_packfile* pf)
{
    return (uint64_t)(int64_t)(get_first_data_sector_offset(pf) * 0x800);
}

// 0x00566430 (:1584)
const char* packfile_get_filename_entry(int entry_index)
{
    return Packfile_file_infos[entry_index].packfile_entry->name;
}
