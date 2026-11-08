// Original: volition/vlib/code/bmpman/bmpman.cpp
//
// Bitmap manager (decompiled). A bitmap handle is an index into Bm_bitmaps. The handles of the shipped bitmaps come
// from the bitmaps file ("bitmaps_pc": a string_hash_table of names, then one bitmap_entry_serialized per handle);
// names that are not in it get handles from bm_add_bitmap (Extra_bm_filename_hash_table) while free entries are left.
//
// A peg (.cpeg_pc header and entries, .gpeg_pc pixels) is registered under its base name (Bm_pegs). bm_register_peg
// links each peg entry into the entry list of its bitmap (sorted by width, largest first) and calls the texture
// register callback (rl_texture_register, which creates the GPU texture); the bitmap shows the head of its list. A
// bitmap whose peg is not loaded shows the bogus/static bitmap ("misc-static.tga" from the startup peg).
//
// Partial: the multiframe peg loading (bm_load_peg_multiframe_*), bm_get_frame_data, bm_copy_frame_data,
// bm_has_alpha(const char*) and bm_get_gpu_filename are not implemented. bm_load_peg_header and the saving
// (bm_save_peg, calc_bitmap_sizes) are (stub_impl.md S3, for the peg assembler).
//
// Notes (decompiled behaviour, kept as is):
// - bm_add_peg returns without unlocking Bm_add_lock when the name is already registered (and when it cannot be added
//   to Bm_pegs, the table is full). The lock is recursive, so the thread that loaded the peg twice goes on; any other
//   thread that loads a peg then waits forever (orig_main.md OM4: a peg left registered by a missing unload deadlocked
//   the stream2 loader against a level load job).
// - bm_register_peg advances by num_frames entries per registered bitmap; a found bitmap with num_frames 0 would never
//   advance. bm_unregister_peg likewise never advances past a peg entry whose name has no handle. Both loop forever
//   in those cases (the pegs this code loads never have them).
// - bm_register_peg adds a new name with bm_add_bitmap(peg_entry*) and then advances by one entry even for an animated
//   bitmap: the following frame entries are then found by name and linked to the first frame's handle.
// - bm_load_peg(filename, pool, peg, ...) writes the 'g' of the GPU extension into a string literal ("") when the
//   name has no extension.
// - The frame of an animated bitmap (bm_get_entry with auto_animate) follows Frametime_total_milliseconds.

#include "volition/vlib/code/bmpman/bmpman.h"

#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/cfile/cfile_internal.h"
#include "volition/vlib/code/memory/dynamic_mempool.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/memory/split_mempool.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/util/string_pool.h"

#include "keen/base/trace.hpp"

#include <stdio.h>
#include <string.h>

// size 0x14 (layout from the PDB)
struct multiframe_peg_info
{
    peg_file* peg_file_ptr; // 0x00
    unsigned char* cur_data_ptr; // 0x04
    unsigned int cpu_file_data_size; // 0x08
    int next_peg_entry; // 0x0C
    unsigned char header_read : 1; // 0x10:0
    unsigned char peg_entries_read : 1; // 0x10:1
    unsigned char peg_filenames_valid : 1; // 0x10:2
    unsigned char pixels_are_being_read : 1; // 0x10:3
};

static const int BITMAPS_FILE_VERSION = 5;
static const int PEG_MIN_VERSION = 10; // older pegs are refused by bm_fixup_peg_header
static const int BM_NAME_MAX = 0x40; // peg names are cut to 63 characters without their extension

const char* Peg_platform_ext[8] = {".cpeg_pc", ".cpeg_pc", ".cpeg_pc", ".cpeg_pc", ".cpeg_pc", ".cpeg_pc", ".cpeg_pc", ".cpeg_pc"}; // 0x01518940
const char* Peg_gpu_platform_ext[8] = {".gpeg_pc", ".gpeg_pc", ".gpeg_pc", ".gpeg_pc", ".gpeg_pc", ".gpeg_pc", ".gpeg_pc", ".gpeg_pc"}; // 0x01518960
const char* Vbm_platform_ext[8] = {".cvbm_pc", ".cvbm_pc", ".cvbm_pc", ".cvbm_pc", ".cvbm_pc", ".cvbm_pc", ".cvbm_pc", ".cvbm_pc"}; // 0x01518980
const char* Vbm_gpu_platform_ext[8] = {".gvbm_pc", ".gvbm_pc", ".gvbm_pc", ".gvbm_pc", ".gvbm_pc", ".gvbm_pc", ".gvbm_pc", ".gvbm_pc"}; // 0x015189A0
const char* Bitmaps_filename[8] = {"bitmaps_pc", "bitmaps_pc", "bitmaps_pc", "bitmaps_pc", "bitmaps_pc", "bitmaps_pc", "bitmaps_pc", "bitmaps_pc"}; // 0x015189C0
bitmap_entry* Bm_bitmaps; // 0x01CD294C
int Bm_bitmap_count; // 0x01CDAE4C
int Bm_entry_count; // 0x01CDD43C
str_data_hash_table<peg_file,2048,64,unsigned short> Bm_pegs; // 0x01D0E8C8
int Last_peg_approx_cpu_data_size; // 0x01CE863C
int Last_peg_approx_gpu_data_size; // 0x01CE8640

// ---- file-static globals
static int Bm_bogus_static_bitmap = -1; // 0x015189E0
static int Bm_peg_align[8] = {256, 16, 128, 4096, 4096, 4096, 4096, 4096}; // 0x015189E4
static string_hash_table Bm_filename_hash_table("Bm_filename_hash_table"); // 0x01519C00 (pool name inferred)
static string_hash_table Extra_bm_filename_hash_table("Extra_bm_filename_hash_table"); // 0x01519CE0 (pool name inferred)
static multiframe_peg_info Bm_multiframe_peg_loading_info[1]; // 0x01CE2564
static unsigned char* Bm_bitmaps_data; // 0x01CE8490
static int Num_bitmaps_from_bitmaps_file; // 0x01CE8644
static void (*Tex_register_callback)(bitmap_entry*); // 0x01CE8648
static void (*Tex_unregister_callback)(bitmap_entry*); // 0x01CE864C
static void (*Bm_discovery_callback)(bitmap_entry*); // 0x01CE8650
static peg_entry* Bogus_static_bitmap_peg_entry; // 0x01CE8654
static bitmap_entry* Bogus_static_bitmap_entry; // 0x01CE8658
static cs_wrapper Bm_add_lock(0); // 0x01D08EE0

// The peg entries follow the header.
static peg_entry* peg_get_entry(peg_header* header, int index)
{
    return (peg_entry*)(header + 1) + index;
}

// The name without its extension, cut to 63 characters (inlined at each use).
static void bm_base_name(char* out, const char* filename)
{
    const char* ext = strrchr(filename, '.');
    if (ext == nullptr)
    {
        strncpy(out, filename, BM_NAME_MAX);
        out[BM_NAME_MAX - 1] = 0;
        return;
    }
    unsigned int len = (unsigned int)(ext - filename);
    if (len > BM_NAME_MAX - 1)
    {
        len = BM_NAME_MAX - 1;
    }
    strncpy(out, filename, len);
    out[len] = 0;
}

// 0x00563B80 (.h:226)
void bitmap_entry::copy_cache_data(const bitmap_entry* src)
{
    current_peg_entry = src->current_peg_entry;
    frame_number = src->frame_number;
    user_data = src->user_data;
    current_peg_entry->cache[0] = src->current_peg_entry->cache[0];
    current_peg_entry->cache[1] = src->current_peg_entry->cache[1];
}

// 0x005B4FB0 (.h:235)
void bitmap_entry::fill(const bitmap_entry_serialized* src)
{
    filename_ptr = src->filename_ptr.to_pointer();
    current_peg_entry = src->current_peg_entry.to_pointer();
    frame_number = src->frame_number;
    user_data = src->user_data;
}

// 0x005C7130 (:208)
// Unlinks pe from the entry list of be. When pe is shown, the next entry (or the bogus bitmap) takes its place.
static void remove_from_entry_list(bitmap_entry* be, peg_entry* pe)
{
    if (be->current_peg_entry != pe)
    {
        peg_entry* next = pe->next.to_pointer();
        peg_entry* prev = pe->prev.to_pointer();
        if (next != nullptr)
        {
            next->prev.from_pointer(prev);
        }
        if (prev != nullptr)
        {
            prev->next.from_pointer(next);
        }
        if (Tex_unregister_callback != nullptr)
        {
            peg_entry* shown = be->current_peg_entry;
            be->current_peg_entry = pe;
            Tex_unregister_callback(be);
            be->current_peg_entry = shown;
        }
        return;
    }
    if (Tex_unregister_callback != nullptr)
    {
        Tex_unregister_callback(be);
    }
    peg_entry* next = pe->next.to_pointer();
    if (next != nullptr)
    {
        be->current_peg_entry = next;
        next->prev.set_offset(-1);
        return;
    }
    be->current_peg_entry = Bogus_static_bitmap_entry->current_peg_entry;
    be->frame_number = Bogus_static_bitmap_entry->frame_number;
}

// 0x005C7250 (:258)
// LTCG: be in EDI, pe in EDX.
// Links pe into the entry list of be (by width, largest first) and registers its texture.
static void add_to_entry_list(bitmap_entry* be, peg_entry* pe)
{
    pe->next.set_offset(-1);
    pe->prev.set_offset(-1);
    peg_entry* shown = be->current_peg_entry;
    if (shown == nullptr || shown == Bogus_static_bitmap_peg_entry)
    {
        if (pe == Bogus_static_bitmap_peg_entry)
        {
            be->copy_cache_data(Bogus_static_bitmap_entry);
            return;
        }
        be->current_peg_entry = pe;
        if (Tex_register_callback != nullptr)
        {
            Tex_register_callback(be);
        }
        return;
    }
    peg_entry* prev = nullptr;
    peg_entry* entry = shown;
    while (entry != pe)
    {
        if (entry->width < pe->width)
        {
            if (prev != nullptr)
            {
                prev->next.from_pointer(pe);
                pe->prev.from_pointer(prev);
            }
            pe->next.from_pointer(entry);
            entry->prev.from_pointer(pe);
            if (prev == nullptr)
            {
                be->current_peg_entry = pe;
            }
            break;
        }
        peg_entry* next = entry->next.to_pointer();
        prev = entry;
        if (next == nullptr)
        {
            entry->next.from_pointer(pe);
            pe->prev.from_pointer(entry);
            break;
        }
        entry = next;
    }
    if (entry == pe)
    {
        return; // already linked
    }
    if (Tex_register_callback != nullptr)
    {
        peg_entry* current = be->current_peg_entry;
        be->current_peg_entry = pe;
        Tex_register_callback(be);
        be->current_peg_entry = current;
    }
}

// 0x005C9950 (:349)
// LTCG: p in ESI.
// A new handle (one per frame) for a peg entry whose name has none.
static void bm_add_bitmap(peg_entry* p)
{
    const unsigned short num_frames = p->num_frames;
    if (num_frames + Bm_bitmap_count >= Bm_entry_count)
    {
        return;
    }
    Bm_add_lock.enter();
    bitmap_entry* be = &Bm_bitmaps[Bm_bitmap_count];
    const short handle = (short)Bm_bitmap_count;
    Bm_bitmap_count++;
    be->filename_ptr = Extra_bm_filename_hash_table.add_string(p->filename.to_pointer(), handle);
    be->frame_number = 0;
    be->current_peg_entry = p;
    if (Bm_discovery_callback != nullptr)
    {
        Bm_discovery_callback(be);
    }
    if (Tex_register_callback != nullptr)
    {
        Tex_register_callback(be);
    }
    if (num_frames > 1)
    {
        be->frame_number = 0;
        // The frame handles follow (their filename stays unset; Bm_bitmap_count is not advanced past them).
        for (int i = 1; i < (int)p->num_frames; i++)
        {
            be[i].filename_ptr = nullptr;
            be[i].current_peg_entry = p;
            be[i].frame_number = (unsigned short)(i & 0xFF);
        }
    }
    Bm_add_lock.leave();
}

// 0x005C9A30 (:404)
// Replaces the bitmaps file: reserve_count empty handles and no shipped names.
void bm_reserve_bitmaps_hack(int reserve_count, mempool_base* pool)
{
    Bm_entry_count = reserve_count;
    Bm_bitmap_count = 0;
    Bm_filename_hash_table.string_pool::clear(); // inlined
    Bm_bitmaps = (bitmap_entry*)pool->alloc(Bm_entry_count * sizeof(bitmap_entry), 0x10);
    memset(Bm_bitmaps, 0, Bm_entry_count * sizeof(bitmap_entry));
    const int extra_entries = Bm_entry_count - Bm_bitmap_count;
    if (extra_entries == 0)
    {
        // Inlined create_empty with the defaults: 100 buckets and a 0x32000-byte string pool.
        Extra_bm_filename_hash_table.create_empty(100, pool, 0x32000);
    }
    else
    {
        // Magic number: 0x18 bytes of string pool per name.
        Extra_bm_filename_hash_table.create_empty(extra_entries, pool, extra_entries * 0x18);
    }
    Num_bitmaps_from_bitmaps_file = Bm_bitmap_count;
}

// 0x005C9C00 (:441)
// A handle for a bitmap name that no loaded peg has: it shows the bogus bitmap until a peg brings the name.
int bm_add_bitmap(const char* filename)
{
    if (Bm_entry_count <= Bm_bitmap_count + 1)
    {
        return -1;
    }
    Bm_add_lock.enter();
    const int handle = Bm_bitmap_count;
    bitmap_entry* be = &Bm_bitmaps[Bm_bitmap_count];
    Bm_bitmap_count++;
    be->filename_ptr = Extra_bm_filename_hash_table.add_string(filename, (short)handle);
    be->frame_number = 1;
    if (Bm_discovery_callback != nullptr)
    {
        Bm_discovery_callback(be);
    }
    if (Bogus_static_bitmap_entry != nullptr)
    {
        // The whole bogus entry is copied, its filename too (so the new name is not kept in the entry).
        *be = *Bogus_static_bitmap_entry;
    }
    Bm_add_lock.leave();
    return handle;
}

// 0x005C1E00 (:478)
// LTCG: filename in ECX.
static peg_file* bm_add_peg(const char* filename)
{
    char name[BM_NAME_MAX];
    Bm_add_lock.enter();
    if (filename != nullptr)
    {
        bm_base_name(name, filename);
    }
    if (Bm_pegs.get_ptr(name) != nullptr)
    {
        return nullptr; // already registered (Bm_add_lock stays locked, see the file comment)
    }
    peg_file file;
    file.data_size = 0;
    Bm_pegs.insert_key_data(name, file);
    peg_file* p = Bm_pegs.get_ptr(name);
    if (p == nullptr)
    {
        return nullptr;
    }
    Bm_pegs.get_key(p, p->filename); // the table's copy of the name
    p->header = nullptr;
    p->data_size = 0;
    Bm_add_lock.leave();
    return p;
}

// 0x005C1F50 (:514)
// LTCG: filename in ESI.
static int bm_find(const char* filename)
{
    short handle = Bm_filename_hash_table.find_string(filename);
    if (handle == -1)
    {
        handle = Extra_bm_filename_hash_table.find_string(filename);
    }
    return handle;
}

// 0x005B5070 (:528)
// LTCG: peg_filename in ESI.
static peg_file* bm_find_peg(const char* peg_filename)
{
    char name[BM_NAME_MAX];
    if (peg_filename != nullptr)
    {
        bm_base_name(name, peg_filename);
    }
    return Bm_pegs.get_ptr(name);
}

// 0x00597550 (:537)
static peg_entry* bm_get_peg_entry(int bitmap_handle)
{
    bitmap_entry* be = bm_get_entry(bitmap_handle, true);
    if (be == nullptr)
    {
        return nullptr;
    }
    return be->current_peg_entry;
}

// 0x005CDC60 (:554)
// Layout: version (5), the handle count, the name table (string_hash_table image), pad to 16, one
// bitmap_entry_serialized per handle, pad to 16. The file stays loaded (Bm_bitmaps_data); the name table points into it.
static int bm_load_bitmaps_file(mempool_base* pool)
{
    cfile* file = cf_open(Bitmaps_filename[0], "rb", PLATFORM_PC, false);
    const unsigned int size = file != nullptr ? cf_size(file) : 0; // the size field, inlined
    Bm_bitmaps_data = (unsigned char*)pool->alloc(size, 0x10);
    if (Bm_bitmaps_data == nullptr)
    {
        return 0;
    }
    cf_read(Bm_bitmaps_data, size, file, false);
    cf_close(file);
    unsigned char* data = Bm_bitmaps_data;
    if (*(int*)data != BITMAPS_FILE_VERSION)
    {
        keen::debug::nativePrintArgs("Bitmap list file header version is not current!  Run newest prep.");
        return 0;
    }
    Bm_bitmap_count = *(int*)(data + 4);
    Bm_entry_count = Bm_bitmap_count;
    int offset = 8;
    Bm_filename_hash_table.load(data, offset);
    if ((offset & 0xF) != 0)
    {
        offset += 0x10 - (offset & 0xF);
    }
    bitmap_entry_serialized* serialized = (bitmap_entry_serialized*)(data + offset);
    offset += Bm_entry_count * (int)sizeof(bitmap_entry_serialized);
    if ((offset & 0xF) != 0)
    {
        offset += 0x10 - (offset & 0xF);
    }
    Bm_bitmaps = (bitmap_entry*)pool->alloc(Bm_entry_count * sizeof(bitmap_entry), 0x10);
    unsigned char* const name_pool = (unsigned char*)Bm_filename_hash_table.strings();
    for (int i = 0; i < Bm_entry_count; i++)
    {
        bitmap_entry_serialized& src = serialized[i];
        // The file stores the name as an offset into the name pool; make it self-relative.
        if (src.filename_ptr.get_offset() != -1)
        {
            src.filename_ptr.set_offset((int)(name_pool + src.filename_ptr.get_offset() - (unsigned char*)&src.filename_ptr));
        }
        // The handle count ends where the named entries end (an unnamed entry before it, or a named one after it).
        const bool named = src.filename_ptr.to_pointer() != nullptr;
        if (Bm_bitmap_count < i)
        {
            if (named)
            {
                Bm_bitmap_count = i;
            }
        }
        else if (!named)
        {
            Bm_bitmap_count = i;
        }
        Bm_bitmaps[i].fill(&src);
    }
    const int extra_entries = Bm_entry_count - Bm_bitmap_count;
    if (extra_entries == 0)
    {
        // Inlined create_empty with the defaults: 100 buckets and a 0x32000-byte string pool.
        Extra_bm_filename_hash_table.create_empty(100, pool, 0x32000);
    }
    else
    {
        // Magic number: 0x18 bytes of string pool per name.
        Extra_bm_filename_hash_table.create_empty(extra_entries, pool, extra_entries * 0x18);
    }
    Num_bitmaps_from_bitmaps_file = Bm_bitmap_count;
    return 1;
}

// 0x005C9C90 (:663)
// Links every entry of the peg to its bitmap handle (see the file comment for the advance).
static bool bm_register_peg(peg_file* p)
{
    int i = 0;
    while (i < p->header->total_entries)
    {
        peg_entry* pe = peg_get_entry(p->header, i);
        int num_frames = pe->num_frames;
        int handle = bm_find(pe->filename.to_pointer());
        if (handle == -1)
        {
            bm_add_bitmap(pe);
            i++;
            continue;
        }
        if (num_frames > 1 && handle >= Num_bitmaps_from_bitmaps_file)
        {
            num_frames = 1;
        }
        else if (num_frames <= 0)
        {
            continue;
        }
        i += num_frames;
        for (; num_frames != 0; num_frames--)
        {
            if (handle != Bm_bogus_static_bitmap || Bm_bitmaps[handle].current_peg_entry == nullptr)
            {
                add_to_entry_list(&Bm_bitmaps[handle], pe);
            }
            handle++;
            pe++;
        }
    }
    return true;
}

// 0x005C7360 (:723)
// LTCG: entries_to_remove in ECX (-1 = all).
static void bm_unregister_peg(peg_file* p, int entries_to_remove)
{
    if (p->header == nullptr)
    {
        return;
    }
    if (entries_to_remove == -1)
    {
        entries_to_remove = p->header->total_entries;
    }
    int i = 0;
    while (i < entries_to_remove)
    {
        peg_entry* pe = peg_get_entry(p->header, i);
        const int handle = bm_find(pe->filename.to_pointer());
        if (handle < 0)
        {
            continue; // never advances (see the file comment)
        }
        const int num_frames = pe->num_frames;
        if (num_frames <= 0)
        {
            continue;
        }
        i += num_frames;
        for (int k = 0; k < num_frames; k++)
        {
            remove_from_entry_list(&Bm_bitmaps[handle + k], pe + k);
        }
    }
    p->data_size = 0;
    p->header = nullptr;
}

// 0x005C7470 (:772)
// LTCG: fname in ECX, peg_data (the peg header) in EDX; vram_size is not passed (unused).
// Turns the file offsets of the peg entries into pointers: each entry's name (the names follow the entries), and with
// has_data its pixels (cache[0]; data becomes 0). The pixels are relative to vram_data when the CPU file is only the
// directory (dir_block_size is 0 or the whole peg_size), else to the header.
static bool bm_fixup_peg_header(const char* fname, unsigned char* const peg_data, const unsigned int peg_size, unsigned char* const vram_data, const unsigned int vram_size, bool has_data)
{
    peg_header* header = (peg_header*)peg_data;
    unsigned char* pixel_base = peg_data;
    if (vram_data != nullptr && (header->dir_block_size == 0 || (unsigned int)header->dir_block_size == peg_size))
    {
        pixel_base = vram_data;
    }
    if (header->version < PEG_MIN_VERSION)
    {
        char message[256];
        sprintf(message, "Incompatible PEG version (%s)\n", fname);
        keen::debug::nativePrintArgs(message);
        return false;
    }
    char* name = (char*)peg_get_entry(header, header->total_entries);
    int i = 0;
    while (i < header->total_entries)
    {
        peg_entry* pe = peg_get_entry(header, i);
        pe->filename.from_pointer(name);
        char* name_end = name;
        while (*name_end != 0)
        {
            name_end++;
        }
        const unsigned int num_frames = pe->num_frames;
        pe->cache[1] = 0;
        pe->cache[0] = 0;
        i += num_frames; // a num_frames of 0 never advances
        // Every frame entry gets the name; only the first entry's cache was cleared.
        for (unsigned int frame = 0; frame < num_frames; frame++, pe++)
        {
            if (!has_data)
            {
                pe->data.from_pointer(nullptr); // offset -pe: to_pointer gives address 0
            }
            else
            {
                if (pe->cache[0] == 0 && pe->cache[1] == 0)
                {
                    pe->cache[0] = (unsigned int)(pixel_base + pe->data.get_offset());
                }
                pe->data.set_offset(0);
            }
            pe->filename.from_pointer(name);
        }
        name = name_end + 1;
    }
    return true;
}

// 0x00563C20 (:881), 0x00563C80 (:902), 0x005B5130 (:907): multiframe peg loading, not implemented.

// 0x005CDF50 (:1081)
static bool bm_slurp_file(const char* const filename, unsigned char*& buffer_ptr, unsigned int& buffer_size, mempool_base* const pool, const unsigned int align_size)
{
    buffer_ptr = nullptr;
    buffer_size = 0;
    cfile* file = cf_open(filename, "rb", PLATFORM_PC, false);
    if (file == nullptr)
    {
        return false;
    }
    const unsigned int size = cf_size(file); // the size field, inlined
    if (size == 0)
    {
        cf_close(file);
        return true;
    }
    if (pool != nullptr)
    {
        pool->pad_to_page(align_size);
    }
    unsigned char* data = (unsigned char*)pool->alloc(size, 0);
    if (data == nullptr)
    {
        keen::debug::nativePrintArgs("[vlib] Not enough memory to load peg %s, needed %d\n", filename, size);
        cf_close(file);
        return false;
    }
    if (cf_read(data, size, file, false) == 0)
    {
        cf_close(file);
        return false;
    }
    cf_close(file);
    buffer_ptr = data;
    buffer_size = size;
    return true;
}

// 0x005D05D0 (:1176)
// Loads the bitmaps file and the startup peg, and resolves the bogus/static bitmap, which every handle without a
// loaded peg then shows.
void bm_init(const char* startup_peg, const char* static_bitmap_name, mempool_base* pool, void (*texture_register_callback)(bitmap_entry*), void (*texture_unregister_callback)(bitmap_entry*), void (*bm_discovery_callback)(bitmap_entry*), void (*post_bitmaps_file_load_callback)(mempool_base*))
{
    Tex_register_callback = texture_register_callback;
    Tex_unregister_callback = texture_unregister_callback;
    Bm_discovery_callback = bm_discovery_callback;
    if (bm_load_bitmaps_file(&Vlib_mempool) == 0)
    {
        keen::debug::nativePrintArgs("bmpman failed to start because the bitmaps binary failed to load");
    }
    if (post_bitmaps_file_load_callback != nullptr)
    {
        post_bitmaps_file_load_callback(pool);
    }
    Bm_multiframe_peg_loading_info[0].peg_file_ptr = nullptr;

    // The startup peg's name with the platform extension.
    char base_name[BM_NAME_MAX];
    char peg_name[BM_NAME_MAX];
    if (startup_peg != nullptr)
    {
        bm_base_name(base_name, startup_peg);
    }
    bm_base_name(peg_name, base_name);
    const char* ext = Peg_platform_ext[0];
    if (strlen(peg_name) + strlen(ext) + (ext[0] != '.' ? 1 : 0) < BM_NAME_MAX)
    {
        if (ext[0] != '.')
        {
            strcat(peg_name, ".");
        }
        strcat(peg_name, ext);
    }
    if (peg_name[0] == 0)
    {
        return;
    }
    peg_header* startup_header;
    if (!bm_load_peg(peg_name, pool, startup_header, true))
    {
        keen::debug::nativePrintArgs("Could not load startup peg!");
    }
    int handle = bm_find(static_bitmap_name);
    if (handle == -1)
    {
        handle = bm_add_bitmap(static_bitmap_name);
    }
    Bm_bogus_static_bitmap = handle;
    if (handle < 0)
    {
        keen::debug::nativePrintArgs("Could no load bogus/static bitmap!");
    }
    bitmap_entry* bogus = bm_get_entry(Bm_bogus_static_bitmap, true);
    Bogus_static_bitmap_peg_entry = bogus->current_peg_entry;
    Bogus_static_bitmap_entry = bogus;
    for (int i = 0; i < Bm_entry_count; i++)
    {
        if (Bm_bitmaps[i].current_peg_entry == nullptr)
        {
            Bm_bitmaps[i].copy_cache_data(bogus);
        }
    }
}

// 0x005B5270 (:1146)
// The directory size (header, entries, names) and the data size (each frame padded to the peg's alignment).
// Original: a bitmap entry with num_frames 0 is never stepped past.
static bool calc_bitmap_sizes(peg_header* pheader, int& header_size, int& data_size)
{
    if (pheader == nullptr)
    {
        return false;
    }
    peg_entry* entries = (peg_entry*)(pheader + 1);
    header_size = pheader->total_entries * (int)sizeof(peg_entry) + (int)sizeof(peg_header);
    data_size = 0;
    int i = 0;
    while (i < pheader->total_entries)
    {
        const peg_entry& entry = entries[i];
        header_size += (int)strlen(entry.filename.to_pointer()) + 1;
        for (int frame = 0; frame < entry.num_frames; frame++)
        {
            data_size += entries[i].frame_size;
            const int rem = data_size % pheader->align_value;
            data_size += rem == 0 ? 0 : pheader->align_value - rem;
            i++;
        }
    }
    return true;
}

// 0x00563C90 (:1238)
void bm_shutdown()
{
}

// 0x005CFE50 (:1259)
bool bm_load_peg(const char* filename, mempool_base* pool, bool register_with_bmpman)
{
    peg_header* peg;
    return bm_load_peg(filename, pool, peg, register_with_bmpman);
}

// 0x005CE030 (:1265)
// Loads the CPU file and the GPU file (same name, 'g' for the extension's first letter: .cpeg_pc -> .gpeg_pc). The
// GPU file goes to the pool's GPU sub-pool when it has one.
bool bm_load_peg(const char* filename, mempool_base* pool, peg_header*& peg, bool register_with_bmpman)
{
    mempool_base* gpu_pool = nullptr;
    unsigned char* cpu_data = nullptr;
    unsigned int cpu_size = 0;
    if ((pool->get_flags() & 0x40) != 0)
    {
        gpu_pool = ((split_mempool*)pool)->get_gpu_pool();
    }
    char gpu_name[256];
    strncpy(gpu_name, filename, 0x100);
    gpu_name[0xFF] = 0;
    char* ext = strrchr(gpu_name, '.');
    if (ext == nullptr || strchr(ext, '/') != nullptr || strchr(ext, '\\') != nullptr)
    {
        // The original points at a string literal here and writes into it (see the file comment).
        static char no_extension[2] = "";
        ext = no_extension;
    }
    ext[1] = 'g';
    if (!bm_slurp_file(filename, cpu_data, cpu_size, pool, 4))
    {
        return false;
    }
    unsigned char* gpu_data = nullptr;
    unsigned int gpu_size = 0;
    if (gpu_pool != nullptr)
    {
        pool = gpu_pool;
    }
    if (!bm_slurp_file(gpu_name, gpu_data, gpu_size, pool, Bm_peg_align[0]))
    {
        char message[356];
        sprintf(message, "Unable to load peg GPU file %s", gpu_name);
        keen::debug::nativePrintArgs(message);
        return false;
    }
    if (!bm_load_peg(filename, cpu_data, cpu_size, gpu_data, gpu_size, register_with_bmpman))
    {
        return false;
    }
    peg = (peg_header*)cpu_data;
    return true;
}

// 0x005CE190 (:1312)
// A peg loaded as one block: the header block, padded to the peg's alignment, then the pixels.
bool bm_load_peg(const char* filename, unsigned char* const preloaded, bool register_with_bmpman)
{
    peg_header* header = (peg_header*)preloaded;
    Last_peg_approx_cpu_data_size = header->dir_block_size;
    const unsigned int align = (unsigned int)(int)header->align_value;
    const unsigned int rest = (unsigned int)Last_peg_approx_cpu_data_size % align;
    const unsigned int padding = rest == 0 ? 0 : align - rest;
    Last_peg_approx_gpu_data_size = header->data_block_size;
    const unsigned int total_size = Last_peg_approx_cpu_data_size + header->data_block_size + padding;
    peg_file* p = nullptr;
    if (register_with_bmpman)
    {
        p = bm_add_peg(filename);
        if (p == nullptr)
        {
            return false;
        }
    }
    if (!bm_fixup_peg_header(filename, preloaded, total_size, nullptr, 0, true))
    {
        return false;
    }
    if (!register_with_bmpman)
    {
        return true;
    }
    p->header = header;
    p->data_size = header->dir_block_size + header->data_block_size;
    return bm_register_peg(p);
}

// 0x005C9D80 (:1321)
// The stream2 Pegs path (peg_load_wrapper): both files are already in memory.
bool bm_load_peg(const char* filename, unsigned char* const peg_data, const unsigned int peg_size, unsigned char* const vram_data, const unsigned int vram_size, bool register_with_bmpman)
{
    peg_header* header = (peg_header*)peg_data;
    if (vram_size == 0)
    {
        Last_peg_approx_cpu_data_size = header->dir_block_size;
        Last_peg_approx_gpu_data_size = header->data_block_size;
    }
    else
    {
        Last_peg_approx_cpu_data_size = peg_size;
        Last_peg_approx_gpu_data_size = vram_size;
    }
    peg_file* p = nullptr;
    if (register_with_bmpman)
    {
        p = bm_add_peg(filename);
        if (p == nullptr)
        {
            return false;
        }
    }
    if (!bm_fixup_peg_header(filename, peg_data, peg_size, vram_data, vram_size, true))
    {
        return false;
    }
    if (!register_with_bmpman)
    {
        return true;
    }
    p->header = header;
    p->data_size = header->data_block_size + header->dir_block_size;
    return bm_register_peg(p);
}

// 0x005CE230 (:1362)
// The directory of a peg (header, entries and names, fixed up without the pixels) read into pool: from the name with
// ".c" before the extension when that file exists, else from filename itself. cf_open inlined.
peg_header* bm_load_peg_header(const char* filename, mempool_base* pool)
{
    const char* ext = cf_get_ext(filename);
    // Deviation from rfg.exe: for a name without extension it reads one past the "" literal and appends what follows
    // it; the port appends an empty string.
    const char* ext_tail = ext[0] != '\0' ? ext + 1 : "";
    char cpu_name[256];
    if (filename != nullptr)
    {
        cf_drop_ext(cpu_name, sizeof(cpu_name), filename);
    }
    strncat(cpu_name, ".c", 0x100);
    strncat(cpu_name, ext_tail, 0x100);
    cf_found_info found_info;
    cfile* cfp = cf_open(cf_find_file(&found_info, cpu_name, false) ? cpu_name : filename, "rb", PLATFORM_PC, false);
    if (cfp == nullptr)
    {
        return nullptr;
    }
    peg_header header;
    if (cf_read(&header, sizeof(header), cfp, false) != sizeof(header))
    {
        cf_close(cfp);
        return nullptr;
    }
    cf_seek(cfp, 0, CF_SEEK_SET);
    pool->pad_to_page(Bm_peg_align[0]);
    unsigned char* peg_data = (unsigned char*)pool->alloc(header.dir_block_size, 0);
    if (peg_data == nullptr)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        cf_close(cfp);
        return nullptr;
    }
    if (cf_read(peg_data, header.dir_block_size, cfp, false) != (unsigned int)header.dir_block_size)
    {
        cf_close(cfp);
        return nullptr;
    }
    cf_close(cfp);
    return bm_fixup_peg_header(filename, peg_data, header.dir_block_size, nullptr, 0, false) ? (peg_header*)peg_data : nullptr;
}

// 0x005C75C0 (:1428)
void bm_unload_peg(const char* filename)
{
    peg_file* p = bm_find_peg(filename);
    if (p == nullptr)
    {
        return;
    }
    bm_unregister_peg(p, -1);
    char name[BM_NAME_MAX];
    cf_drop_ext(name, BM_NAME_MAX, filename);
    Bm_pegs.remove(name);
}

// 0x005CFE70 (:1739)
// The peg to filename and its GPU file (same name, 'g' for the extension's first letter). Original: a name without
// extension writes the 'g' into a string literal, as bm_load_peg does.
int bm_save_peg(const char* filename, peg_header* peg)
{
    char gpu_name[256];
    strncpy(gpu_name, filename, 0x100);
    gpu_name[0xFF] = 0;
    char* ext = strrchr(gpu_name, '.');
    if (ext == nullptr || strchr(ext, '/') != nullptr || strchr(ext, '\\') != nullptr)
    {
        // Deviation from rfg.exe: it writes the 'g' past the end of a "" literal; the port writes into this buffer.
        static char no_extension[2] = "";
        ext = no_extension;
    }
    ext[1] = 'g';
    return bm_save_peg(filename, gpu_name, peg);
}

// 0x005CE440 (:1747)
// Both files written (one file when gpu_filename is null; cf_open inlined). A GPU file left empty gets 4 bytes.
// Original: with one file, the closed CPU file is checked, written and closed again as the GPU file.
int bm_save_peg(const char* cpu_filename, const char* gpu_filename, peg_header* peg)
{
    cfile* peg_cpu_file = cf_open(cpu_filename, "wb", PLATFORM_PC, false);
    if (peg_cpu_file == nullptr)
    {
        return 0;
    }
    cfile* peg_gpu_file = peg_cpu_file;
    if (gpu_filename != nullptr)
    {
        peg_gpu_file = cf_open(gpu_filename, "wb", PLATFORM_PC, false);
        if (peg_gpu_file == nullptr)
        {
            cf_close(peg_cpu_file);
            return 0;
        }
    }
    const int result = bm_save_peg(peg_cpu_file, peg_gpu_file, peg);
    cf_close(peg_cpu_file);
    if (peg_gpu_file == nullptr || peg_gpu_file->size == 0)
    {
        cf_write("abcd", 4, peg_gpu_file); // magic number: a placeholder so the GPU file is never empty
    }
    cf_close(peg_gpu_file);
    return result;
}

// 0x005CA100 (:1780)
// The header (with the sizes of calc_bitmap_sizes), the entries (data offsets from the start of the data: after the
// aligned directory in one file, from 0 in a separate GPU file), the names, then each frame's data aligned. 1 when the
// files have the expected sizes, else -1. The entries are written as copies, so their name/next/prev offsets point
// relative to the copy (decompiled behaviour, kept as is). Original: a bitmap entry with num_frames 0 is never stepped
// past when the names are written.
int bm_save_peg(cfile* peg_cpu_file, cfile* peg_gpu_file, peg_header* peg)
{
    calc_bitmap_sizes(peg, peg->dir_block_size, peg->data_block_size);
    if (peg_cpu_file == peg_gpu_file)
    {
        const int rem = peg->dir_block_size % peg->align_value;
        peg->dir_block_size += rem == 0 ? 0 : peg->align_value - rem;
    }
    cf_write(peg, sizeof(peg_header), peg_cpu_file);
    int offset = peg->dir_block_size;
    if (peg_gpu_file != nullptr && peg_cpu_file != peg_gpu_file)
    {
        offset = 0;
    }
    peg_entry* entries = (peg_entry*)(peg + 1);
    for (int i = 0; i < peg->total_entries; i++)
    {
        peg_entry entry(entries[i]);
        int next = offset + (int)entry.frame_size;
        const int rem = next % peg->align_value;
        next += rem == 0 ? 0 : peg->align_value - rem;
        entry.data.set_offset(offset);
        cf_write(&entry, sizeof(peg_entry), peg_cpu_file);
        offset = next;
    }
    int i = 0;
    while (i < peg->total_entries)
    {
        const char* name = entries[i].filename.to_pointer();
        cf_write(name, (unsigned int)strlen(name) + 1, peg_cpu_file);
        if (entries[i].num_frames != 0)
        {
            i += entries[i].num_frames;
        }
    }
    if (peg_cpu_file == peg_gpu_file)
    {
        cf_pad_align_write(peg->align_value, peg_cpu_file);
    }
    for (int j = 0; j < peg->total_entries; j++)
    {
        if (entries[j].frame_size != 0)
        {
            cf_write(entries[j].data.to_pointer(), entries[j].frame_size, peg_gpu_file);
        }
        cf_pad_align_write(peg->align_value, peg_gpu_file);
    }
    if (peg_cpu_file == peg_gpu_file)
    {
        if (peg_cpu_file->pos == (unsigned int)(peg->dir_block_size + peg->data_block_size))
        {
            return 1;
        }
    }
    else if (peg_cpu_file->pos == (unsigned int)peg->dir_block_size && peg_gpu_file->pos == (unsigned int)peg->data_block_size)
    {
        return 1;
    }
    return -1;
}

// 0x005CA530 (:1875)
// The handle of a bitmap name; a new one (showing the bogus bitmap) when no peg has brought it yet.
int bm_load(const char* filename, bool force_dynamic)
{
    if (force_dynamic)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    int handle = bm_find(filename);
    if (handle == -1)
    {
        handle = bm_add_bitmap(filename);
    }
    return handle;
}

// 0x005C1FE0 (:1906)
bool bm_is_loaded(const char* filename)
{
    const int handle = bm_find(filename);
    if (handle == -1)
    {
        return false;
    }
    bitmap_entry* be = bm_get_entry(handle, true);
    bitmap_entry* be_again = bm_get_entry(handle, true);
    peg_entry* pe = be_again != nullptr ? be_again->current_peg_entry : nullptr;
    if (handle == Bm_bogus_static_bitmap || pe == Bogus_static_bitmap_peg_entry || be == Bogus_static_bitmap_entry)
    {
        return false;
    }
    return pe != nullptr;
}

// 0x00563CA0 (:1924)
void bm_unload(int handle, bool force_unload)
{
}

// 0x00563CB0 (:1975)
int bm_get_peg_align_value()
{
    return Bm_peg_align[0];
}

// 0x00563CC0 (:1980)
int bm_get_peg_align_value(vlib_platform platform)
{
    return Bm_peg_align[platform];
}

// 0x00597570 (:2028)
const char* bm_get_filename(int handle)
{
    bitmap_entry* be = bm_get_entry(handle, true);
    if (be == nullptr)
    {
        return nullptr;
    }
    return be->filename_ptr;
}

// The texture scale of a peg entry: the source size (low 15 bits) over the stored size, times 1 / (1 << reduction).
// The reduction level is packed into bit 15 of source_height (bit 0) and source_width (bit 1); the two
// bm_get_dimensions overloads extract it differently (the 5-argument one also ors in bit 14 of source_width).
static void peg_entry_scale(const peg_entry* pe, int w, int h, unsigned int reduction, float& scale_x, float& scale_y)
{
    const unsigned int source_width = pe->source_width.combined;
    const unsigned int source_height = pe->source_height.combined;
    const float reduction_scale = 1.0f / (float)(1 << reduction);
    const unsigned int sw = source_width & 0x7FFF;
    const unsigned int sh = source_height & 0x7FFF;
    scale_x = reduction_scale * (sw == 0 ? 1.0f : (float)sw / (float)w);
    scale_y = reduction_scale * (sh == 0 ? 1.0f : (float)sh / (float)h);
}

// 0x00597590 (:2041)
void bm_get_dimensions(int handle, int& w, int& h, float& scale_x, float& scale_y)
{
    bitmap_entry* be = bm_get_entry(handle, true);
    if (be == nullptr || be->current_peg_entry == nullptr)
    {
        return;
    }
    const peg_entry* pe = be->current_peg_entry;
    w = pe->width;
    h = pe->height;
    const unsigned int reduction = (((unsigned char)(pe->source_height.combined >> 9)) | ((unsigned char)(pe->source_width.combined >> 8))) >> 6;
    peg_entry_scale(pe, w, h, reduction, scale_x, scale_y);
}

// 0x00597690 (:2058)
void bm_get_dimensions(int handle, int& w, int& h, float& scale_x, float& scale_y, int& mip_levels)
{
    bitmap_entry* be = bm_get_entry(handle, true);
    if (be == nullptr || be->current_peg_entry == nullptr)
    {
        return;
    }
    const peg_entry* pe = be->current_peg_entry;
    w = pe->width;
    h = pe->height;
    mip_levels = pe->mip_levels;
    const unsigned int reduction = (((unsigned int)pe->source_height.combined >> 1) | ((unsigned int)pe->source_width.combined & 0x8000)) >> 14;
    peg_entry_scale(pe, w, h, reduction, scale_x, scale_y);
}

// 0x00563CE0 (:2161)
// With auto_animate, frame 0 of an animated bitmap gives the entry of the current frame (fps from the peg entry).
bitmap_entry* bm_get_entry(int handle, bool auto_animate)
{
    if (handle < 0 || handle >= Bm_bitmap_count)
    {
        return nullptr;
    }
    if (auto_animate)
    {
        const unsigned int num_frames = Bm_bitmaps[handle].current_peg_entry->num_frames;
        if (num_frames > 1 && Bm_bitmaps[handle].frame_number == 0)
        {
            // Magic number: fps * 0.001 frames per millisecond (rounded to float first).
            const float frames_per_ms = (float)(Bm_bitmaps[handle].current_peg_entry->fps * 0.001);
            const int frame = (int)(frames_per_ms * (float)Frametime_total_milliseconds);
            handle += frame % (int)num_frames;
        }
    }
    return &Bm_bitmaps[handle];
}

// 0x00563D70 (:2198)
bool bm_has_alpha(int handle)
{
    bitmap_entry* be = bm_get_entry(handle, true);
    if (be == nullptr)
    {
        return false;
    }
    return (be->current_peg_entry->flags & 1) != 0;
}

// 0x00563D90 (:2242)
bool bm_is_cube_map(int handle)
{
    bitmap_entry* be = bm_get_entry(handle, true);
    if (be == nullptr)
    {
        return false;
    }
    return (be->current_peg_entry->flags >> 3 & 1) != 0;
}

// 0x00563DC0 (:2258)
bool bm_is_bogus(int handle)
{
    bitmap_entry* be = bm_get_entry(handle, true);
    if (be == nullptr || be->current_peg_entry == nullptr)
    {
        return true;
    }
    return be->current_peg_entry == Bogus_static_bitmap_peg_entry;
}

// 0x00563DF0 (:2270)
int bm_get_bogus_bitmap_handle()
{
    return Bm_bogus_static_bitmap;
}
