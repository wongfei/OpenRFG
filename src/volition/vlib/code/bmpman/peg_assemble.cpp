// Original: volition/vlib/code/bmpman/peg_assemble.cpp
//
// The peg assembler (stub_impl.md S3): the bitmaps of several pegs (or of an .slw rule file's sources) merged into one
// peg. peg_asm_start takes a temporary pool (the hash table of the names added, the entry list, the source pegs) and
// the pool of the result; peg_asm_add registers each bitmap of a peg once (by name without extension, skipping the
// exclusion table's names, with the platform's .cvbm extension); peg_asm_finish lays out the header, the entries and
// the names, then copies each bitmap's pixels from its .cvbm_pc/.gvbm_pc pair (the GPU data in the result pool's GPU
// sub-pool when it is split), and gives the temporary pool back. The asset assembler builds pegs this way only without
// packfiles (assasm_start with build_peg and not load_from_preload).
//
// Notes (decompiled behaviour, kept as is):
// - copy_bitmap_data_with_interleave does not check the .cvbm_pc load: a missing file reads the entries at address
//   0x18. For an entry with interleave flags (0x30) it clears flag 0x40 and steps by one entry, not by num_frames.
// - peg_asm_add_from_slw restores the temporary pool before it loads the sources' headers (the names were copied to
//   the stack).

#include "volition/vlib/code/bmpman/peg_assemble.h"

#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/bmpman/bmpman_defs.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/split_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/varray.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include "keen/base/string.hpp"

#include <malloc.h>
#include <new>
#include <string.h>

typedef str_data_hash_table<int, 2048, 64, unsigned short> peg_asm_hash_table;
static_assert(sizeof(peg_asm_hash_table) == 0x26008, "peg_asm_start allocates 0x26008 bytes for the hash table");

// rfg.exe reads bmpman.cpp's Bm_peg_align (0x015189E4) directly; here through bm_get_peg_align_value.
static base_array<peg_entry*> Peg_entries; // 0x01519D50 (over the temporary pool's memory, peg_asm_start)
static int Temp_pool_gpu_mark; // 0x01CD2AD0
static unsigned int Temp_pool_mark; // 0x01CDB900
static peg_asm_hash_table* Peg_hash_table; // 0x01CDD7C4
static vlib_platform Platform; // 0x01CDD7C8
static mempool_base* Temp_pool; // 0x01CE865C
static mempool_base* Final_pool; // 0x01CE8660
static str_data_hash_table<unsigned char,1024,64,unsigned short>* Peg_exclusions_hash_table; // 0x01CE8664
static void (*Peg_asm_error_cb)(const char*, ...); // 0x01CE8668

// 0x00563E00 (.h:31)
pasm_data::pasm_data()
    : cpu_data(nullptr)
    , cpu_size(0)
    , gpu_data(nullptr)
    , gpu_size(0)
{
}

// 0x005C76B0
// Compiler-generated (placed here: vlib, which every program links; bmpman.cpp is in rfg_render): member-wise, so the
// self-relative offsets are rebased on the copy (et_ptr_offset's copy).
peg_entry::peg_entry(const peg_entry& __that) = default;

// 0x005C7890
peg_entry& peg_entry::operator=(const peg_entry& __that) = default;

// Not in the binary: the padding that brings value up to a multiple of align (inlined everywhere).
static int peg_asm_pad(int value, int align)
{
    const int rem = value % align;
    return rem == 0 ? 0 : align - rem;
}

// Not in the binary: the temporary pool (and its GPU half) back to the marks of peg_asm_start (inlined in
// peg_asm_finish and peg_asm_abort).
static void peg_asm_release_temp_pool()
{
    Peg_hash_table->~peg_asm_hash_table();
    if ((Temp_pool->get_flags() & 0x40) != 0)
    {
        static_cast<split_mempool*>(Temp_pool)->restore_to_mark_cpu(Temp_pool_mark);
        static_cast<split_mempool*>(Temp_pool)->restore_to_mark_gpu(Temp_pool_gpu_mark);
    }
    else
    {
        Temp_pool->restore_to_mark(Temp_pool_mark);
    }
    Peg_hash_table = nullptr;
}

// 0x005CE4E0 (:79)
static peg_header* load_bitmap_as_vbm(const char* filename)
{
    char vbm_name[64];
    cf_add_ext(vbm_name, sizeof(vbm_name), filename, ".cvbm_pc", false);
    peg_header* peg;
    return bm_load_peg(vbm_name, Temp_pool, peg, false) ? peg : nullptr;
}

// 0x005CE540 (:97)
// The entries' data pointers laid out from offset in data (each frame aligned), then each bitmap's frames copied from
// its .cvbm_pc (load_bitmap_as_vbm inlined). LTCG: dst_peg in EDX, offset in ECX.
static void copy_bitmap_data_with_interleave(peg_header* dst_peg, unsigned char* data, int& offset)
{
    const int align = bm_get_peg_align_value(Platform);
    peg_entry* dst_entries = (peg_entry*)(dst_peg + 1);
    for (int i = 0; i < dst_peg->total_entries; i++)
    {
        dst_entries[i].data.from_pointer(data + offset);
        offset += dst_entries[i].frame_size;
        offset += peg_asm_pad(offset, align);
    }
    int i = 0;
    while (i < Peg_entries.size())
    {
        peg_entry* entry = Peg_entries[i];
        if ((entry->flags & 0x30) != 0)
        {
            i++;
            entry->flags &= 0xFFBF;
            continue;
        }
        mempool_mark_scope scope(Temp_pool);
        char vbm_name[64];
        cf_add_ext(vbm_name, sizeof(vbm_name), entry->filename.to_pointer(), ".cvbm_pc", false);
        peg_header* src_peg;
        const bool loaded = bm_load_peg(vbm_name, Temp_pool, src_peg, false);
        // Original: not checked (a failed load reads the entries at address 0x18).
        peg_entry* src_entries = (peg_entry*)(loaded ? src_peg + 1 : (peg_header*)nullptr + 1);
        for (int frame = 0; frame < entry->num_frames; frame++)
        {
            memcpy(dst_entries[i].data.to_pointer(), src_entries[frame].data.to_pointer(), src_entries[frame].frame_size);
            i++;
        }
    }
}

// 0x005B53F0 (:175)
// The directory size (header, entries and names), the data size (each frame aligned) and the bitmap count; without
// interleaving the interleave flags (0x30) are dropped and the frame size reset to the plain size (cache[0]). LTCG:
// total_gpu_size in ESI.
static bool calc_interleaving_and_data_size(int& total_cpu_size, int& total_gpu_size, int& num_bitmaps, int& align_value, bool apply_interleaving)
{
    align_value = bm_get_peg_align_value(Platform);
    total_gpu_size = 0;
    num_bitmaps = 0;
    total_cpu_size = sizeof(peg_header);
    int i = 0;
    while (i < Peg_entries.size())
    {
        peg_entry* entry = Peg_entries[i];
        total_cpu_size += (int)strlen(entry->filename.to_pointer()) + 1;
        total_cpu_size += entry->num_frames * (int)sizeof(peg_entry);
        num_bitmaps++;
        for (int frame = 0; frame < entry->num_frames; frame++)
        {
            total_gpu_size += Peg_entries[i]->frame_size;
            total_gpu_size += peg_asm_pad(total_gpu_size, align_value);
            i++;
        }
    }
    if (!apply_interleaving)
    {
        for (int j = 0; j < Peg_entries.size(); j++)
        {
            peg_entry* entry = Peg_entries[j];
            if ((entry->flags & 0x30) != 0)
            {
                entry->flags &= 0xFFCF;
                entry->frame_size = entry->cache[0];
            }
        }
    }
    return true;
}

// 0x005B5530 (:239)
// One block: the directory padded to the alignment, then the data. Inlined in peg_asm_finish. LTCG: total_size in EDI,
// num_bitmaps in ECX, apply_interleaving in EAX.
static bool calc_interleaving_and_data_size(int& total_size, int& num_bitmaps, int& align_value, bool apply_interleaving)
{
    int data_size;
    if (!calc_interleaving_and_data_size(total_size, data_size, num_bitmaps, align_value, apply_interleaving))
    {
        return false;
    }
    total_size += peg_asm_pad(total_size, align_value) + data_size;
    return true;
}

// 0x005C77A0 (:258)
bool peg_asm_start(vlib_platform plat, mempool_base* temp_pool, mempool_base* final_pool)
{
    Temp_pool = temp_pool;
    Temp_pool_mark = temp_pool->mark();
    Temp_pool_gpu_mark = 0;
    if ((Temp_pool->get_flags() & 0x40) != 0)
    {
        Temp_pool_gpu_mark = static_cast<split_mempool*>(Temp_pool)->mark_gpu();
    }
    Final_pool = final_pool;
    void* table = temp_pool->alloc(sizeof(peg_asm_hash_table), 0);
    Peg_hash_table = (peg_asm_hash_table*)table;
    if (table != nullptr)
    {
        new (table) peg_asm_hash_table();
    }
    // Magic number: room for 0x2000 entries (0x8000 bytes of pointers).
    Peg_entries.init((peg_entry**)temp_pool->alloc(0x8000, 0), 0x2000, 0);
    Peg_exclusions_hash_table = nullptr;
    Platform = plat;
    return true;
}

// 0x00563E20 (:292)
void peg_asm_add_exclusions(str_data_hash_table<unsigned char,1024,64,unsigned short>* const exclusions)
{
    Peg_exclusions_hash_table = exclusions;
}

// 0x00563E30 (:297)
void peg_asm_set_error_callback(void (*error_cb)(const char*, ...))
{
    Peg_asm_error_cb = error_cb;
}

// 0x005B5580 (:302)
// Each bitmap not added yet (by name without extension) and not excluded (by name with the platform's .cvbm
// extension): its frame entries listed, its name in the hash table with the peg's size.
bool peg_asm_add(peg_header* peg)
{
    peg_entry* entries = (peg_entry*)(peg + 1);
    int i = 0;
    while (i < peg->total_entries)
    {
        peg_entry* entry = &entries[i];
        // Deviation from rfg.exe: name and vbm_name are left uninitialised there for an entry without a name
        // (cf_drop_ext/cf_add_ext return early); the port zeroes them.
        char name[64] = {};
        char vbm_name[64] = {};
        cf_drop_ext(name, sizeof(name), entry->filename.to_pointer());
        cf_add_ext(vbm_name, sizeof(vbm_name), entry->filename.to_pointer(), Vbm_platform_ext[Platform], false);
        if (Peg_hash_table->exists(name) || (Peg_exclusions_hash_table != nullptr && Peg_exclusions_hash_table->exists(vbm_name)))
        {
            i += entry->num_frames;
            continue;
        }
        for (int frame = 0; frame < entry->num_frames; frame++)
        {
            Peg_entries.add(&entry[frame]);
            i++;
        }
        const int size = peg->data_block_size + peg->dir_block_size;
        Peg_hash_table->insert_key_data(name, size);
    }
    return true;
}

// 0x005CE7E0 (:366)
bool peg_asm_add(const char* filename)
{
    peg_header* peg = bm_load_peg_header(filename, Temp_pool);
    if (peg == nullptr)
    {
        return false;
    }
    return peg_asm_add(peg);
}

// 0x005CE810 (:387)
// The sources of the rule's entry for the platform, but the .slw's own extension and the GPU files: each source's peg
// header added.
bool peg_asm_add_from_slw(const char* slw_filename, vlib_platform plat)
{
    const char* slw_ext = cf_get_ext(slw_filename);
    // Magic number: up to 512 sources.
    const char* sources[512];
    int num_sources = 0;
    {
        mempool_mark_scope scope(Temp_pool);
        xml_element* root = xml_parse(slw_filename, Temp_pool);
        if (root == nullptr)
        {
            return false;
        }
        xml_element* rule = root->elements;
        while (rule != nullptr && keen::compareStringNoCase(rule->name, "rule") != 0)
        {
            rule = rule->next;
        }
        if (rule == nullptr)
        {
            return false;
        }
        xml_element* platform = xml_find(rule, "platform");
        if (platform == nullptr)
        {
            return false;
        }
        while (keen::compareStringNoCase(platform->text, Vlib_platforms[plat]) != 0)
        {
            do
            {
                platform = platform->next;
                if (platform == nullptr)
                {
                    return false;
                }
            } while (keen::compareStringNoCase(platform->name, "platform") != 0);
        }
        xml_element* source = platform->elements;
        while (source != nullptr && keen::compareStringNoCase(source->name, "source") != 0)
        {
            source = source->next;
        }
        while (source != nullptr)
        {
            const char* ext = cf_get_ext(source->text);
            if (keen::compareStringNoCase(ext, slw_ext) != 0 && keen::compareStringNoCase(ext, Peg_gpu_platform_ext[plat]) != 0 && keen::compareStringNoCase(ext, Vbm_gpu_platform_ext[plat]) != 0)
            {
                char* copy = (char*)_alloca(strlen(source->text) + 1);
                strcpy(copy, source->text);
                sources[num_sources++] = copy;
            }
            do
            {
                source = source->next;
            } while (source != nullptr && keen::compareStringNoCase(source->name, "source") != 0);
        }
    }
    for (int i = 0; i < num_sources; i++)
    {
        peg_header* peg = bm_load_peg_header(sources[i], Temp_pool);
        if (peg != nullptr)
        {
            peg_asm_add(peg);
        }
    }
    return true;
}

// 0x005B5870 (:464)
bool peg_asm_find_bitmap(const char* filename)
{
    // Deviation from rfg.exe: left uninitialised there for a null name; the port zeroes it.
    char name[64] = {};
    cf_drop_ext(name, sizeof(name), filename);
    return Peg_hash_table->exists(name);
}

// 0x005CFFB0 (:489)
peg_header* peg_asm_finish(bool pack_data_pointers)
{
    pasm_data data;
    return peg_asm_finish(&data, pack_data_pointers) ? (peg_header*)data.cpu_data : nullptr;
}

// 0x005CEBA0 (:498)
// The assembled peg: one block (header, entries, names, aligned data) in the result pool, or the directory there and
// the data in its GPU sub-pool when it is split. With pack_data_pointers the data pointers become offsets from the
// data's start. The temporary pool is given back and the error callback cleared.
bool peg_asm_finish(pasm_data* data, bool pack_data_pointers)
{
    int num_bitmaps;
    int align;
    const bool split = (Final_pool->get_flags() & 0x40) != 0;
    if (split)
    {
        if (!calc_interleaving_and_data_size(data->cpu_size, data->gpu_size, num_bitmaps, align, true))
        {
            return false;
        }
    }
    else
    {
        data->gpu_size = 0;
        if (!calc_interleaving_and_data_size(data->cpu_size, num_bitmaps, align, true))
        {
            if (Peg_asm_error_cb != nullptr)
            {
                Peg_asm_error_cb("error calculating interleave and data size\n");
            }
            return false;
        }
    }
    if (split)
    {
        split_mempool* pool = static_cast<split_mempool*>(Final_pool);
        const int gpu_size = data->gpu_size;
        const int gpu_align = bm_get_peg_align_value();
        if (!pool->can_alloc(data->cpu_size, 4) || (gpu_size != 0 && (pool->get_gpu_pool() == nullptr || !pool->get_gpu_pool()->can_alloc(gpu_size, gpu_align))))
        {
            if (Peg_asm_error_cb != nullptr)
            {
                const unsigned int gpu_free = pool->get_gpu_pool() != nullptr ? pool->get_gpu_pool()->space_free() : 0;
                const unsigned int cpu_free = pool->space_free_cpu();
                char* gpu_free_str = str_commas(gpu_free);
                char* cpu_free_str = str_commas(cpu_free);
                char* gpu_size_str = str_commas(data->gpu_size);
                char* cpu_size_str = str_commas(data->cpu_size);
                Peg_asm_error_cb("out of space. (requested %s / %s) (available %s / %s)\n", cpu_size_str, gpu_size_str, cpu_free_str, gpu_free_str);
            }
            return false;
        }
        data->cpu_data = (unsigned char*)pool->alloc(data->cpu_size, 4);
        data->gpu_data = (unsigned char*)pool->get_gpu_pool()->alloc(data->gpu_size, bm_get_peg_align_value());
    }
    else
    {
        if (!Final_pool->can_alloc(data->cpu_size, bm_get_peg_align_value()))
        {
            if (Peg_asm_error_cb != nullptr)
            {
                char* free_str = str_commas(Final_pool->space_free());
                char* size_str = str_commas(data->cpu_size);
                Peg_asm_error_cb("out of space. (requested %s) (available %s)\n", size_str, free_str);
            }
            return false;
        }
        data->cpu_data = (unsigned char*)Final_pool->alloc(data->cpu_size, bm_get_peg_align_value());
        data->gpu_data = nullptr;
    }
    memset(data->cpu_data, 0, data->cpu_size);
    if (data->gpu_data != nullptr)
    {
        memset(data->gpu_data, 0, data->gpu_size);
    }
    peg_header* header = (peg_header*)data->cpu_data;
    memset(header, 0, sizeof(peg_header));
    header->signature = 0x564B4547; // magic number: 'GEKV'
    header->version = 10; // magic number: the peg version
    header->platform = (short)Platform;
    header->flags = 0;
    header->total_entries = (short)Peg_entries.size();
    header->num_bitmaps = (short)num_bitmaps;
    header->align_value = (short)align;
    int offset = sizeof(peg_header);
    int i = 0;
    while (i < Peg_entries.size())
    {
        peg_entry* entry = Peg_entries[i];
        for (int frame = 0; frame < entry->num_frames; frame++)
        {
            *(peg_entry*)(data->cpu_data + offset) = *Peg_entries[i];
            offset += sizeof(peg_entry);
            i++;
        }
    }
    peg_entry* dst_entries = (peg_entry*)(header + 1);
    i = 0;
    while (i < Peg_entries.size())
    {
        peg_entry* entry = Peg_entries[i];
        peg_entry& first = dst_entries[i];
        first.filename.from_pointer((char*)data->cpu_data + offset);
        strcpy(first.filename.to_pointer(), entry->filename.to_pointer());
        offset += (int)strlen(entry->filename.to_pointer()) + 1;
        // Every frame shares the first frame's name.
        for (int frame = 0; frame < entry->num_frames; frame++)
        {
            dst_entries[i].filename.from_pointer(first.filename.to_pointer());
            i++;
        }
    }
    int gpu_offset = 0;
    if (data->gpu_data == nullptr)
    {
        offset += peg_asm_pad(offset, align);
        copy_bitmap_data_with_interleave(header, data->cpu_data, offset);
    }
    else
    {
        copy_bitmap_data_with_interleave(header, data->gpu_data, gpu_offset);
    }
    if (pack_data_pointers)
    {
        unsigned char* base = data->gpu_data != nullptr ? data->gpu_data : data->cpu_data;
        for (int j = 0; j < header->total_entries; j++)
        {
            et_ptr_offset<unsigned char,0>& ptr = dst_entries[j].data;
            if (ptr.get_offset() != -1)
            {
                ptr.set_offset((int)((unsigned char*)&ptr - base) + ptr.get_offset());
            }
        }
    }
    peg_asm_release_temp_pool();
    Peg_asm_error_cb = nullptr;
    return true;
}

// 0x005C2060 (:672)
void peg_asm_abort()
{
    peg_asm_release_temp_pool();
}
