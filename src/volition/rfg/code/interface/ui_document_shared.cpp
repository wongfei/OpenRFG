// Original: volition/rfg/code/interface/ui_document_shared.cpp
//
// UI.md UI3: whole. The documents' stream containers, built at run time with the asset assembler: for each document
// of the project its critical resources and meta data come from the .vint_doc file's header (parse), then one
// container "<doc>" (the document and its Lua scripts) and one "<doc>2" (its pegs, without the project's include pegs).

#include "volition/rfg/code/interface/ui_document_shared.h"

#include "keen/base/trace.hpp"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_project.h"
#include "volition/rfg/code/gamewide/asset_assembler.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/interface/vint/vint_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_badge.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2.h"
#include "volition/stream2/code/stream2_container.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <stdio.h>
#include <string.h>

// ---- file-static globals
static const char* UI_STRING_POOL_NAME = {"UI String Pool"}; // 0x01664EE8
// A static of the interface unity build in rfg.exe (ui_init allocates its arrays, ui_init_variables points
// gVint_settings at it): external here.
Vint_settings My_vint_project_settings; // 0x01667174
static char* UI_string_pool_data; // 0x02C08140
static string_pool UI_string_pool(UI_STRING_POOL_NAME); // 0x02C3CB00

vint_doc_stream_info Vint_doc_stream_data[64]; // 0x02C40C60
unsigned int Num_vint_doc_stream_data; // 0x02C08194

// 0x008B4060
Vint_settings::Vint_settings()
{
}

// 0x008B4070
Vint_settings::~Vint_settings()
{
}

// Magic numbers: the .vint_doc ID (0x3027) and the limits of a document's stream info.
static const unsigned int Vint_doc_file_id = 0x3027;
static const int Max_doc_critical_resources = 4;
static const int Max_doc_meta_data_entries = 2;
static const int Ui_document_max_files = 64;

// 0x00888310 (.h:62)
critical_resource_data::critical_resource_data()
{
    name = nullptr;
    type = DOC_RESOURCE_TYPE_INVALID;
    cpu_data = nullptr;
    gpu_data = nullptr;
    cpu_size = 0;
    gpu_size = 0;
}

// 0x00888330 (.h:83)
// Original: name is left as it is.
vint_doc_stream_info::vint_doc_stream_info()
{
    num_meta_data_entries = 0;
    num_critical_resources = 0;
    srid = 0;
    srid2 = 0;
}

// 0x008B67E0 (:131)
// Reads the document's header and string table: its pegs (critical resources of type 0, with the platform's peg
// extension) and its Lua scripts (meta data "lua_script_file", with ".lua"), names kept in the UI string pool.
bool ui_document_parse_doc_file(const char* name, const char* filename, mempool_base* temp_pool, vint_doc_stream_info& stream_data, vlib_platform plat)
{
    mempool_mark_scope mark(temp_pool);
    cfile* file = cf_open(filename, "rb", PLATFORM_PC, false);
    if (file == nullptr)
    {
        return false;
    }
    // Original bug kept: the file stays open when it is empty or cannot be read.
    unsigned int size = cf_size(file);
    if (size == 0)
    {
        return false;
    }
    unsigned char* data = (unsigned char*)temp_pool->alloc(size + 1, 0);
    if (data == nullptr)
    {
        return false;
    }
    size = cf_read(data, size, file, false);
    if (size == 0)
    {
        return false;
    }
    cf_close(file);
    vint_bin_file_header header;
    memset(&header, 0, sizeof(header));
    vint_load_buffer buffer(data, size);
    buffer.get_data(header.ID);
    buffer.get_data(header.doc_name);
    buffer.get_data(header.version);
    buffer.get_data(header.doc_time_index);
    buffer.get_data(header.num_metadata_entries);
    buffer.get_data(header.num_crit_resources);
    buffer.get_data(header.strings_offset);
    buffer.get_data(header.num_root_element_children);
    buffer.get_data(header.num_root_object_children);
    if (header.ID != Vint_doc_file_id)
    {
        return false;
    }
    vint_bin_file_strings strings;
    vint_load_buffer strings_buffer(buffer);
    strings_buffer.set_pos(header.strings_offset);
    if (!strings.load_from_buffer(strings_buffer, temp_pool))
    {
        return false;
    }
    stream_data.vint_doc.name = UI_string_pool.add_unique(filename);
    stream_data.name = UI_string_pool.add_unique(name);
    int num_crit = (int)header.num_crit_resources;
    if (num_crit > Max_doc_critical_resources)
    {
        num_crit = Max_doc_critical_resources;
    }
    int num_meta = (int)header.num_metadata_entries;
    if (num_meta > Max_doc_meta_data_entries)
    {
        num_meta = Max_doc_meta_data_entries;
    }
    char resource_name[64];
    stream_data.num_critical_resources = 0;
    for (int i = 0; i < num_crit; i++)
    {
        unsigned char type = 0;
        buffer.get_data(type);
        unsigned int string_handle = 0;
        buffer.get_data(string_handle);
        if (type == 0)
        {
            critical_resource_data& resource = stream_data.critical_resources[stream_data.num_critical_resources];
            resource.type = DOC_RESOURCE_TYPE_PEG;
            strncpy_safe(resource_name, strings.get_string(string_handle), sizeof(resource_name));
            strncat(resource_name, Peg_platform_ext[plat], sizeof(resource_name));
            resource.name = UI_string_pool.add_unique(resource_name);
            stream_data.num_critical_resources++;
        }
        else
        {
            VLIB_CRITICAL_FAILURE();
        }
    }
    stream_data.num_meta_data_entries = 0;
    for (int i = 0; i < num_meta; i++)
    {
        // Original: the key and value are uninitialized (registers) when the buffer runs out; zero here.
        unsigned int key = 0;
        unsigned int value = 0;
        buffer.get_data(key);
        buffer.get_data(value);
        if (v_stricmp(strings.get_string(key), "lua_script_file") == 0)
        {
            critical_resource_data& entry = stream_data.meta_data_entries[stream_data.num_meta_data_entries];
            entry.type = DOC_RESOURCE_TYPE_LUA_SCRIPT;
            strncpy_safe(resource_name, strings.get_string(value), sizeof(resource_name));
            strncat(resource_name, ".lua", sizeof(resource_name));
            entry.name = UI_string_pool.add_unique(resource_name);
            stream_data.num_meta_data_entries++;
        }
        else
        {
            VLIB_CRITICAL_FAILURE();
        }
    }
    return true;
}

// 0x008B6C50 (:332)
// The two containers of every document of the project (or of the DLC's documents, which follow the base ones; a
// patch starts the list again).
void ui_document_create_doc_containers(mempool_base* tmp_pool, mempool* pool, bool load_from_preload, vlib_platform plat, bool patch, int dlc_id)
{
    bool is_dlc = dlc_id != -1;
    const char* system;
    if (patch)
    {
        system = "patch_vint_doc_containers";
    }
    else if (dlc_id != -1)
    {
        system = "dlc_vint_doc_containers";
    }
    else
    {
        system = "vint_doc_containers";
    }
    char system_name[64];
    strncpy(system_name, system, sizeof(system_name));
    assasm_start(system_name, tmp_pool, (mempool_base*)pool, load_from_preload, nullptr, false, is_dlc, true, !patch);
    int num_docs = is_dlc ? vint_project_get_num_dlc_filenames() : vint_project_get_num_filenames();
    if (patch)
    {
        Num_vint_doc_stream_data = 0;
    }
    if (num_docs > Ui_document_max_files)
    {
        // Original: the message is formatted and never shown.
        char message[512];
        sprintf(message, "Too many VINT docs found! Need to increase UI_DOCUMENT_MAX_FILES above %d\n", Ui_document_max_files);
        num_docs = Ui_document_max_files;
    }
    int first = 0;
    if (is_dlc)
    {
        UI_string_pool.set_thread_ownership();
        UI_string_pool.restore_to_mark(0);
        if (load_from_preload || dlc_allocate_temp_container_list(Ui_document_max_files, (mempool_base*)pool))
        {
            // The DLC's documents are the last ones of the project.
            first = vint_project_get_num_filenames() - num_docs;
        }
        else
        {
            is_dlc = false;
        }
    }
    const int end = num_docs + first;
    for (int i = first; i < end; i++)
    {
        const char* filename = vint_project_get_doc_filename(i);
        if (filename == nullptr)
        {
            continue;
        }
        const char* name = vint_project_get_doc_name(i);
        if (name == nullptr)
        {
            continue;
        }
        vint_doc_stream_info& info = Vint_doc_stream_data[Num_vint_doc_stream_data];
        if (!ui_document_parse_doc_file(name, filename, tmp_pool, info, plat))
        {
            continue;
        }
        // Magic numbers: container type 0x0D, primitive types 0x12 (vint doc) and 0x13 (meta data).
        assasm_container_start(filename, 0x0D, false, false);
        assasm_container_add(info.vint_doc.name, 0x12, "UI VINT doc", false, false, false, nullptr, 0);
        for (unsigned int m = 0; m < info.num_meta_data_entries; m++)
        {
            if (info.meta_data_entries[m].type == DOC_RESOURCE_TYPE_LUA_SCRIPT)
            {
                assasm_container_add(info.meta_data_entries[m].name, 0x13, "UI meta data", false, false, false, nullptr, 0);
            }
        }
        info.srid = assasm_container_finish(false, is_dlc, false);
        char peg_container[64];
        strncpy(peg_container, filename, sizeof(peg_container));
        strncat(peg_container, "2", sizeof(peg_container));
        // Magic numbers: container type 0x21, primitive type 0x01 (peg) in allocator 0x17.
        assasm_container_start(peg_container, 0x21, false, false);
        for (unsigned int c = 0; c < info.num_critical_resources; c++)
        {
            if (info.critical_resources[c].type != DOC_RESOURCE_TYPE_PEG)
            {
                continue;
            }
            char peg_name[64];
            cf_drop_ext(peg_name, sizeof(peg_name), info.critical_resources[c].name);
            bool included = false;
            for (int p = 0; p < gVint_settings->include_bmp_pegs.size(); p++)
            {
                if (v_stricmp(gVint_settings->include_bmp_pegs[p], peg_name) == 0)
                {
                    included = true;
                    break;
                }
            }
            if (!included)
            {
                assasm_container_add(info.critical_resources[c].name, 0x01, "UI crit rsc", false, false, false, nullptr, 0x17);
            }
        }
        info.srid2 = assasm_container_finish(false, is_dlc, false);
        Num_vint_doc_stream_data++;
    }
    assasm_finish(nullptr, false, true, false);
    if (!is_dlc)
    {
        UI_string_pool.mark();
    }
}

// 0x0089E330 (:564)
// Compiled out: finds nothing.
int findStreamedFiles(char** pResultBuffer, const char** pFiles, unsigned int fileCount, const char* pPreFix)
{
    return 0;
}

// 0x0089E340 (:627)
// The streamed image containers: nothing is added in this build (findStreamedFiles finds nothing), but the
// multiplayer badges sheet ("interface-badges") is its own preloaded container.
void ui_document_create_stream_image_containers(mempool_base* tmp_pool, mempool* pool, bool load_from_preload, vlib_platform plat, int dlc_id)
{
    const bool is_dlc = dlc_id != -1;
    assasm_start(is_dlc ? "dlc_ui_images" : "ui_images", tmp_pool, (mempool_base*)pool, load_from_preload, nullptr, false, is_dlc, false, false);
    // Original: the name list's buffer (0x1200 bytes) and the prefixes are made for findStreamedFiles and not used.
    tmp_pool->alloc(0x1200, 0);
    char name[256];
    if (is_dlc)
    {
        sprintf(name, "dlc%d_sp:", dlc_id);
        sprintf(name, "dlc%d_mp:", dlc_id);
    }
    assasm_finish(nullptr, false, true, false);
    if (is_dlc)
    {
        return;
    }
    assasm_start("ui_mp_preload", tmp_pool, (mempool_base*)pool, load_from_preload, nullptr, false, false, true, false);
    assasm_container_start("badges_bitmap_sheet", 0x21, false, false);
    sprintf(name, "%s%s", "interface-badges", Vbm_platform_ext[plat]);
    // Magic number: allocator 0x19.
    assasm_container_add(name, 0x01, "UI MP preload", false, false, false, nullptr, 0x19);
    Multi_badges_bitmap_sheet_srid = assasm_container_finish(true, false, false);
    stream2_container* container = stream2_get_container(Multi_badges_bitmap_sheet_srid);
    if (container != nullptr)
    {
        // Original: the requirements are computed and not used.
        int cpu_bytes = 0;
        int cpu_align;
        int gpu_bytes = 0;
        int gpu_align;
        container->calc_container_requirements(cpu_bytes, cpu_align, gpu_bytes, gpu_align);
    }
    assasm_finish(nullptr, false, true, false);
}

// 0x008B7020 (:805)
// The UI string pool (10000 bytes of the permanent pool, once), then the document and image containers.
void ui_document_load_shared(mempool_base* tmp_pool, mempool* pool, bool load_from_preload, vlib_platform plat, bool patch, int dlc_id)
{
    mempool_mark_scope mark(tmp_pool);
    if (UI_string_pool_data == nullptr)
    {
        // Magic number: 10000 bytes.
        UI_string_pool_data = (char*)Permanent_mempool.alloc(10000, 0);
        if (UI_string_pool_data == nullptr)
        {
            keen::debug::nativePrintArgs("Insufficient memory to allocate the UI string table");
        }
        UI_string_pool.init(UI_string_pool_data, 10000, UI_STRING_POOL_NAME, 0, 0);
        UI_string_pool.clear();
    }
    ui_document_create_doc_containers(tmp_pool, pool, load_from_preload, plat, patch, dlc_id);
    if (!patch)
    {
        ui_document_create_stream_image_containers(tmp_pool, pool, load_from_preload, plat, dlc_id);
    }
}
