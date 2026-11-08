// Original: volition/rfg/code/interface/ui_memory_manager.cpp
//
// UI.md UI3: whole. The UI memory manager: the documents' stream requests (one container pair at a time: the "<doc>"
// container with the document and its scripts, then the "<doc>2" peg container), the stream2 callbacks that hand the
// loaded data to the documents, the interface allocators, the game's bitmap sheets and external allocations.

#include "volition/rfg/code/interface/ui_memory_manager.h"

#include "keen/base/trace.hpp"
#include "volition/interface_lib/code/bitmap_sheets.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <malloc.h>
#include <string.h>

// ---- file-static globals
static bool Can_stream_next = true; // 0x01664A4B
game_sheet_info Sheets[10]; // 0x02C02CD0
// Used by vint_rfg.cpp too (a static of the interface unity build in rfg.exe): external here.
vint_sound_callbacks vint_sound_cb; // 0x02C02E10
ui_menu_item_char_list Menu_item_char_list[16]; // 0x02C40170 (file static in the binary; external since UI4, ui_menu.cpp)

farray<vint_doc_stream_info*,16> Stream_requests; // 0x01667648
stream2_mempool_allocator Stream_interface_allocator; // 0x01667614
stream2_system_allocator Stream_interface_stack_allocator; // 0x01667620
stream2_slot_allocator Stream_interface_slot_allocator; // 0x01667624
unsigned int Deferred_stream_release; // 0x02C08198
unsigned int Num_sheets; // 0x02C0819C
farray<umm_allocation_data,3> External_alloc_marks; // 0x02C3D000

// 0x008A0020 (:46)
umm_allocation_data::umm_allocation_data()
{
    name[0] = '\0';
    type = DOC_RESOURCE_TYPE_INVALID;
    cpu_data = nullptr;
    gpu_data = nullptr;
    cpu_size = 0;
    gpu_size = 0;
}

// 0x008A0110 (:134)
static unsigned char* interface_slot_allocator_cpu_cb(int size, int align)
{
    return (unsigned char*)Interface_mempool.alloc(size, align);
}

// 0x008A0130 (:139)
// The GPU part when the interface pool is split.
static unsigned char* interface_slot_allocator_gpu_cb(int size, int align)
{
    // Magic number: 0x40, the split pool's flag for separate CPU and GPU parts.
    if ((Interface_mempool.get_flags() & 0x40) != 0)
    {
        return (unsigned char*)Interface_mempool.alloc_gpu(size, align);
    }
    return nullptr;
}

// The document whose container pair has the srid (inlined by rfg.exe in every callback).
static vint_doc_stream_info* find_doc_stream_info(unsigned int srid)
{
    for (unsigned int i = 0; i < Num_vint_doc_stream_data; i++)
    {
        if (Vint_doc_stream_data[i].srid == srid || Vint_doc_stream_data[i].srid2 == srid)
        {
            return &Vint_doc_stream_data[i];
        }
    }
    return nullptr;
}

// 0x008A0160 (:150)
vint_doc_stream_info* mem_manager_find_doc_stream_info(unsigned int srid)
{
    return find_doc_stream_info(srid);
}

// 0x008A01A0 (:162)
// The last document of the name.
vint_doc_stream_info* mem_manager_find_doc_stream_info(const char* name)
{
    for (int i = (int)Num_vint_doc_stream_data - 1; i >= 0; i--)
    {
        if (v_stricmp(Vint_doc_stream_data[i].name, name) == 0)
        {
            return &Vint_doc_stream_data[i];
        }
    }
    return nullptr;
}

// 0x008A01F0 (:177)
critical_resource_data* mem_manager_find_meta_data_entry(const char* name, unsigned int srid)
{
    vint_doc_stream_info* info = find_doc_stream_info(srid);
    if (info == nullptr)
    {
        VLIB_CRITICAL_FAILURE();
        return nullptr;
    }
    for (unsigned int i = 0; i < info->num_meta_data_entries; i++)
    {
        if (v_stricmp(info->meta_data_entries[i].name, name) == 0)
        {
            return &info->meta_data_entries[i];
        }
    }
    return nullptr;
}

// 0x008A0290 (:197)
critical_resource_data* mem_manager_find_crit_resource(const char* name, unsigned int srid)
{
    vint_doc_stream_info* info = find_doc_stream_info(srid);
    if (info == nullptr)
    {
        VLIB_CRITICAL_FAILURE();
        return nullptr;
    }
    for (unsigned int i = 0; i < info->num_critical_resources; i++)
    {
        if (v_stricmp(info->critical_resources[i].name, name) == 0)
        {
            return &info->critical_resources[i];
        }
    }
    return nullptr;
}

// 0x008A0330 (:220)
static void game_sheet_info_initialize(game_sheet_info* gsi)
{
    if (gsi != nullptr)
    {
        memset(gsi->m_name, 0, sizeof(gsi->m_name));
    }
}

// 0x008A0350 (:229)
static void game_sheet_info_clear(game_sheet_info* gsi)
{
    if (gsi != nullptr)
    {
        memset(gsi->m_name, 0, sizeof(gsi->m_name));
    }
}

// The sheet's table file: the name with ".xtbl" when it has no extension.
static void game_sheet_table_name(char* table, unsigned int size, const char* name)
{
    _snprintf_safe(table, size, name);
    if (strstr(name, ".") == nullptr)
    {
        strcat(table, ".xtbl");
    }
}

// 0x008A0370 (:238)
// LTCG: gsi in ESI, name in EDI. Parses the sheet table into the permanent interface pool and keeps the name.
bool game_sheet_info_set(game_sheet_info* gsi, const char* name)
{
    if (gsi == nullptr || name == nullptr)
    {
        return false;
    }
    char table[128];
    game_sheet_table_name(table, sizeof(table), name);
    const unsigned int pos = Common_mempool.mark();
    bitmap_sheets_parse_table(table, &Interface_perm_mempool, &Common_mempool);
    Common_mempool.restore_to_mark(pos);
    _snprintf_safe(gsi->m_name, sizeof(gsi->m_name), name);
    return true;
}

// 0x008A0430 (:274)
// Parses the sheet table again, into the interface pool.
void game_sheet_info_reload(game_sheet_info* gsi)
{
    if (gsi != nullptr)
    {
        char table[128];
        game_sheet_table_name(table, sizeof(table), gsi->m_name);
        const unsigned int pos = Common_mempool.mark();
        bitmap_sheets_parse_table(table, &Interface_mempool, &Common_mempool);
        Common_mempool.restore_to_mark(pos);
    }
}

// 0x008A04E0 (:296)
bool mem_manager_initialize()
{
    for (int i = 0; i < 10; i++)
    {
        game_sheet_info_initialize(&Sheets[i]);
    }
    return true;
}

// 0x008A0510 (:307)
// One slot bracket of 0x1B580 CPU and 0x200000 GPU bytes for the documents' pegs; the scripts and documents come from
// the permanent interface pool.
bool mem_manager_initialize_allocators()
{
    // Magic numbers: the bracket {cpu bytes, gpu bytes, slots}.
    int bracket_info[3] = {0x1B580, 0x200000, 1};
    const bool result = Stream_interface_slot_allocator.init(interface_slot_allocator_cpu_cb, 4, interface_slot_allocator_gpu_cb, 4,
                                                             bracket_info, 1, "UI slot allocator");
    Stream_interface_allocator.init(&Interface_perm_mempool);
    return result;
}

// 0x008A0580 (:326)
void mem_manager_shutdown()
{
    Stream_interface_slot_allocator.destroy();
    for (int i = 0; i < 10; i++)
    {
        game_sheet_info_clear(&Sheets[i]);
    }
}

// 0x008B72B0 (:341)
// One step a frame: a deferred release of a loaded "<doc>" container, else the next request's container pair.
void mem_manager_process()
{
    if (Deferred_stream_release != 0)
    {
        if (stream2_get_status(Deferred_stream_release) != ST_UNLOADED)
        {
            stream2_release(Deferred_stream_release);
        }
        Deferred_stream_release = 0;
        Can_stream_next = true;
        return;
    }
    if (Can_stream_next && Stream_requests.size() > 0)
    {
        if (!stream2_request(Stream_requests[0]->srid))
        {
            VLIB_CRITICAL_FAILURE();
        }
        if (!stream2_request(Stream_requests[0]->srid2))
        {
            VLIB_CRITICAL_FAILURE();
        }
        Stream_requests.remove(0);
        Can_stream_next = false;
    }
}

// 0x008B7380 (:390)
// Preloads every request at once.
void mem_manager_force_synchronous_process()
{
    while (Stream_requests.size() > 0)
    {
        if (Deferred_stream_release != 0)
        {
            if (stream2_get_status(Deferred_stream_release) != ST_UNLOADED)
            {
                stream2_release(Deferred_stream_release);
            }
            Deferred_stream_release = 0;
        }
        if (!stream2_preload(Stream_requests[0]->srid))
        {
            VLIB_CRITICAL_FAILURE();
        }
        if (!stream2_preload(Stream_requests[0]->srid2))
        {
            VLIB_CRITICAL_FAILURE();
        }
        Stream_requests.remove(0);
    }
    if (Deferred_stream_release != 0)
    {
        if (stream2_get_status(Deferred_stream_release) != ST_UNLOADED)
        {
            stream2_release(Deferred_stream_release);
        }
        Deferred_stream_release = 0;
    }
    Can_stream_next = true;
}

// 0x008A05C0 (:429)
// Loads a peg (the platform's peg extension added when the name has none) into the permanent interface pool.
bool mem_manager_add_preloaded_peg(const char* name)
{
    char filename[128];
    _snprintf_safe(filename, sizeof(filename), name);
    if (strstr(name, ".") == nullptr)
    {
        // Original: the PC peg extension (Peg_platform_ext's first entry).
        strcat(filename, Peg_platform_ext[0]);
    }
    return bm_load_peg(filename, &Interface_perm_mempool, true);
}

// 0x008B74A0 (:443)
// Adds a bitmap sheet (at most 10, each once).
bool mem_manager_add_sheet(const char* name)
{
    if (mem_manager_has_sheet(name) != -1)
    {
        return false;
    }
    if (Num_sheets < 10)
    {
        game_sheet_info_set(&Sheets[Num_sheets], name);
        Num_sheets++;
        return true;
    }
    return false;
}

// 0x008A0660 (:466)
int mem_manager_has_sheet(const char* name)
{
    if (name == nullptr)
    {
        return -1;
    }
    for (int i = 0; i < (int)Num_sheets; i++)
    {
        if (Sheets[i].m_name[0] != '\0' && v_stricmp(Sheets[i].m_name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

// 0x008A06B0 (:510)
unsigned int mem_manager_get_num_sheets()
{
    return Num_sheets;
}

// 0x008A06C0 (:530)
const char* mem_manager_get_sheet(int which)
{
    if (which < (int)Num_sheets)
    {
        return Sheets[which].m_name;
    }
    return nullptr;
}

// Deviation: rfg.exe calls _aligned_malloc directly, with alignment 0 from every caller (fog of war's materials, and
// always for the empty GPU block). Its MSVCR100 accepts that, as the caller uses the block unchecked; the UCRT raises
// an invalid parameter and aborts. The UCRT's minimum alignment is the pointer size, so that is what 0 becomes.
static void* umm_aligned_malloc(unsigned int size, unsigned int alignment)
{
    return _aligned_malloc(size, alignment != 0 ? alignment : sizeof(void*));
}

// 0x008CABA0 (:563)
// Up to 3 named external allocations, from the CRT's aligned heap; the sizes round up to the alignments.
void mem_manager_request_memory(unsigned int cpu_size, unsigned int gpu_size, const char* name, unsigned int cpu_alignment, unsigned int gpu_alignment, void*& cpu_mem, void*& gpu_mem)
{
    cpu_mem = nullptr;
    gpu_mem = nullptr;
    if (name == nullptr || cpu_size == 0)
    {
        return;
    }
    if (cpu_alignment != 0)
    {
        cpu_size += cpu_size % cpu_alignment == 0 ? 0 : cpu_alignment - cpu_size % cpu_alignment;
    }
    if (gpu_alignment != 0)
    {
        gpu_size += gpu_size % gpu_alignment == 0 ? 0 : gpu_alignment - gpu_size % gpu_alignment;
    }
    if (External_alloc_marks.size() < External_alloc_marks.capacity())
    {
        umm_allocation_data data;
        cpu_mem = umm_aligned_malloc(cpu_size, cpu_alignment);
        data.gpu_data = (unsigned char*)umm_aligned_malloc(gpu_size, gpu_alignment);
        gpu_mem = data.gpu_data;
        data.cpu_data = (unsigned char*)cpu_mem;
        // Original: the sizes are not recorded.
        strncpy(data.name, name, sizeof(data.name));
        External_alloc_marks.add(data);
    }
}

// 0x008CACB0 (:592)
void* mem_manager_request_memory(unsigned int size, const char* name, unsigned int alignment)
{
    void* cpu_mem;
    void* gpu_mem;
    mem_manager_request_memory(size, 0, name, alignment, 0, cpu_mem, gpu_mem);
    return cpu_mem;
}

// 0x008B74F0 (:601)
// Frees the last external allocation of the name.
bool mem_manager_free_requested_memory(const char* name)
{
    for (int i = External_alloc_marks.size() - 1; i >= 0; i--)
    {
        if (v_stricmp(External_alloc_marks[i].name, name) == 0)
        {
            unsigned char* gpu_data = External_alloc_marks[i].gpu_data;
            if (External_alloc_marks[i].cpu_data != nullptr)
            {
                _aligned_free(External_alloc_marks[i].cpu_data);
            }
            if (gpu_data != nullptr)
            {
                _aligned_free(gpu_data);
            }
            External_alloc_marks.remove(i);
            return true;
        }
    }
    return false;
}

// 0x008A06E0 (:616)
void mem_manager_get_cpu_gpu_peg_alignments(int& cpu_align, int& gpu_align)
{
    const int align = bm_get_peg_align_value();
    if (Stream_interface_allocator.using_split_mempools())
    {
        cpu_align = 4;
        gpu_align = align;
        return;
    }
    cpu_align = align;
    gpu_align = align;
}

// 0x008B75A0 (:624)
bool mem_manager_doc_in_loading_q(vint_doc_stream_info* stream_info)
{
    int count = 0;
    for (int i = 0; i < Stream_requests.size(); i++)
    {
        if (Stream_requests[i] == stream_info)
        {
            count++;
        }
    }
    return count != 0;
}

// 0x008A0720 (:630)
// The document's peg container srid (the one the document manager keeps).
unsigned int mem_manager_get_doc_srid(const char* name)
{
    vint_doc_stream_info* info = mem_manager_find_doc_stream_info(name);
    if (info == nullptr)
    {
        return 0;
    }
    return info->srid2;
}

// 0x008CACE0 (:640)
// Queues the document's containers (once) when the srid is unloaded.
bool mem_manager_load_doc(unsigned int srid)
{
    vint_doc_stream_info* info = find_doc_stream_info(srid);
    if (info == nullptr)
    {
        return false;
    }
    if (stream2_get_status(srid) != ST_UNLOADED)
    {
        VLIB_CRITICAL_FAILURE();
        return false;
    }
    if (Stream_requests.size() >= Stream_requests.capacity())
    {
        return false;
    }
    if (mem_manager_doc_in_loading_q(info))
    {
        return true;
    }
    Stream_requests.add(info);
    return true;
}

// 0x008CADA0 (:670)
// Drops a queued request; releases the peg container and the document container when loaded.
void mem_manager_unload_doc(unsigned int srid)
{
    vint_doc_stream_info* info = find_doc_stream_info(srid);
    if (info == nullptr)
    {
        return;
    }
    const bool queued = mem_manager_doc_in_loading_q(info);
    if (stream2_get_status(info->srid2) == ST_UNLOADED)
    {
        if (queued)
        {
            Stream_requests.remove_matching(info);
        }
        return;
    }
    if (queued)
    {
        Stream_requests.remove_matching(info);
    }
    stream2_release(info->srid2);
    if (stream2_get_status(info->srid) != ST_UNLOADED && Deferred_stream_release != info->srid)
    {
        stream2_release(info->srid);
        Can_stream_next = true;
    }
}

// 0x008EFA00 (:712)
// The peg container loaded: once the document container is loaded too, the document can be created; its container
// is released next frame (the document keeps its own copy).
void interface_pegs_loaded_callback(unsigned int srid)
{
    vint_doc_stream_info* info = find_doc_stream_info(srid);
    if (info != nullptr && stream2_get_status(info->srid) == ST_LOADED && Deferred_stream_release != info->srid)
    {
        ui_notify_doc_assets_loaded(info->srid2);
        Deferred_stream_release = info->srid;
    }
}

// 0x008A0740 (:729)
void interface_pegs_unloaded_callback(unsigned int srid)
{
}

// 0x008EFA80 (:735)
// The document container loaded: as interface_pegs_loaded_callback, the other way round. A container that is no
// document's is released next frame.
void interface_doc_loaded_callback(unsigned int srid)
{
    vint_doc_stream_info* info = find_doc_stream_info(srid);
    if (info == nullptr)
    {
        Deferred_stream_release = srid;
        return;
    }
    if (stream2_get_status(info->srid2) == ST_LOADED && Deferred_stream_release != info->srid)
    {
        ui_notify_doc_assets_loaded(info->srid2);
        Deferred_stream_release = info->srid;
    }
}

// 0x008A0750 (:753)
void interface_doc_unloaded_callback(unsigned int srid)
{
}

// 0x008A0760 (:759)
// The GSP_VDOC primitive: the document's data.
bool interface_vint_doc_loaded_callback(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size)
{
    vint_doc_stream_info* info = find_doc_stream_info(srid);
    if (info == nullptr)
    {
        return false;
    }
    info->vint_doc.cpu_data = cpu_data;
    info->vint_doc.gpu_data = gpu_data;
    info->vint_doc.cpu_size = cpu_data_size;
    info->vint_doc.gpu_size = gpu_data_size;
    return true;
}

// 0x008A07C0 (:775)
void interface_vint_doc_unloaded_callback(const char* filename, unsigned int srid)
{
    vint_doc_stream_info* info = find_doc_stream_info(srid);
    if (info != nullptr)
    {
        info->vint_doc.cpu_data = nullptr;
        info->vint_doc.gpu_data = nullptr;
        info->vint_doc.cpu_size = 0;
        info->vint_doc.gpu_size = 0;
    }
}

// 0x008A0820 (:790)
// A Lua script primitive of the document container.
bool interface_script_loaded_callback(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size)
{
    critical_resource_data* entry = mem_manager_find_meta_data_entry(filename, srid);
    if (entry == nullptr)
    {
        return false;
    }
    entry->cpu_data = cpu_data;
    entry->gpu_data = gpu_data;
    entry->cpu_size = cpu_data_size;
    entry->gpu_size = gpu_data_size;
    return true;
}

// 0x008A0860 (:805)
void interface_script_unloaded_callback(const char* filename, unsigned int srid)
{
    critical_resource_data* entry = mem_manager_find_meta_data_entry(filename, srid);
    if (entry != nullptr)
    {
        entry->cpu_data = nullptr;
        entry->gpu_data = nullptr;
        entry->cpu_size = 0;
        entry->gpu_size = 0;
    }
}

// 0x008A0890 (:817)
void ui_set_thread_ownership()
{
    Stream_interface_allocator.set_thread_ownership();
    Stream_interface_slot_allocator.set_thread_ownership();
}
