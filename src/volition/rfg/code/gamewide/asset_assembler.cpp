// Original: volition/rfg/code/gamewide/asset_assembler.cpp
//
// The asset assembler (header comment in asset_assembler.h). Two modes per system:
// - load from preload (the game with packfiles): assasm_start reads <system>.asm_pc, which registers every container
//   of the system; container_start finds the container by name, container_add finds its primitive (and peg) in it,
//   container_finish preloads it when the widget is a preload one.
// - build (no packfiles, and the systems the game always assembles at run time, such as the foliage containers): the
//   containers and their primitives are created through stream2, with the .slw3/.tlist bitmap lists and the pegs
//   next to each item; a preload widget's pegs go into the peg assembler (peg_asm_*), whose result assasm_finish hands
//   back in peg_data.

#include "volition/rfg/code/gamewide/asset_assembler.h"

#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/util/always_loaded_shared.h"
#include "volition/stream2/code/stream2.h"
#include "volition/stream2/code/stream2_container.h"
#include "volition/stream2/code/stream2_prim.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/bmpman/peg_assemble.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/split_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/vlib_core/vlib_core.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// ---- file-static globals
static bool Validate = true; // 0x0165C941
static char Asset_name[64]; // 0x0227AEE8
// Placement: rfg.pdb puts these two in game_save_generic.cpp; only the asset assembler uses them.
static mempool_base* ass_Tmp_mempool; // 0x025337A0
static unsigned int ass_Tmp_mempool_cpu_mark; // 0x025337A4
static unsigned int ass_Tmp_mempool_gpu_mark; // 0x025337A8
static mempool_base* Container_pool; // 0x025337AC
static bool Widget_preload; // 0x025337B0
static bool Building_peg; // 0x025337B1
static bool Loading_from_preload_file; // 0x025337B2
// Never written in rfg.exe: always PLATFORM_PC (0).
static vlib_platform Asm_platform; // 0x025337B4
static unsigned int Current_srid; // 0x025337B8

// The peg assembler's last error text. Inferred: rfg.exe formats into a thread-local buffer (TLS block offset 0x38, the
// variable is not identified); the size here is ours.
static thread_local char Assasm_peg_asm_error[512];

// 0x007B9A70 (:62)
// The primitive of the current container with this name.
static stream2_prim* find_prim(const char* prim_name)
{
    stream2_container* container = stream2_get_container(Current_srid);
    if (container == nullptr)
    {
        return nullptr;
    }
    for (int i = 0; i < container->num_entries; i++)
    {
        if (v_stricmp(container->entries[i].prim->name, prim_name) == 0)
        {
            return container->entries[i].prim;
        }
    }
    return nullptr;
}

// 0x007D7DE0 (:56)
// One of the always-loaded files (bitmaps that stay resident).
bool assasm_is_preload_file(const char* filename)
{
    return always_loaded_get_tables_combined(Asm_platform)->exists(filename);
}

// 0x007B9AE0 (:141)
// Starts a system: marks the scratch pool (assasm_finish rewinds it), reads <system>.asm_pc when loading from the
// preload file, and starts the peg assembler (with the always-loaded bitmaps excluded) when building a peg.
bool assasm_start(const char* system_name, mempool_base* tmp_pool, mempool_base* container_pool, bool load_from_preload, mempool_base* peg_pool, bool build_peg, bool is_temp, bool validate, bool patch_data)
{
    stream2_reset_allocated_stats();
    Loading_from_preload_file = load_from_preload;
    ass_Tmp_mempool = tmp_pool;
    if ((tmp_pool->get_flags() & 0x40) == 0)
    {
        ass_Tmp_mempool_cpu_mark = tmp_pool->mark();
    }
    else
    {
        ass_Tmp_mempool_cpu_mark = static_cast<split_mempool*>(tmp_pool)->mark_cpu();
        ass_Tmp_mempool_gpu_mark = static_cast<split_mempool*>(ass_Tmp_mempool)->mark_gpu();
    }
    strcpy(Asset_name, system_name);
    Building_peg = build_peg;
    Container_pool = container_pool;
    Validate = validate;
    if (Loading_from_preload_file)
    {
        char filename[64];
        cf_add_ext(filename, sizeof(filename), Asset_name, ".asm_pc", false);
        stream2_read_container_data(filename, tmp_pool, container_pool, false, is_temp, patch_data);
    }
    if (Building_peg && !Loading_from_preload_file)
    {
        if (peg_pool == nullptr)
        {
            return false;
        }
        peg_asm_start(Asm_platform, tmp_pool, peg_pool);
        peg_asm_add_exclusions(always_loaded_get_tables_combined(Asm_platform));
    }
    return true;
}

// 0x007B9C20 (:200)
// Builds a new container, or finds the preload file's (a missing one fails unless the system does not validate).
bool assasm_container_start(const char* widget_name, unsigned char container_type, bool preload, bool passive)
{
    Widget_preload = preload;
    if (!Loading_from_preload_file)
    {
        return stream2_container_start(widget_name, Container_pool, container_type, passive, false);
    }
    Current_srid = stream2_container_find(widget_name);
    if (!Validate)
    {
        return true;
    }
    return Current_srid != 0;
}

// 0x007B9C80 (:352)
static void assasm_peg_asm_error_cb(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    // OpenRFG deviation: bounded (rfg.exe uses vsprintf).
    _vsnprintf(Assasm_peg_asm_error, sizeof(Assasm_peg_asm_error) - 1, format, args);
    Assasm_peg_asm_error[sizeof(Assasm_peg_asm_error) - 1] = '\0';
    va_end(args);
}

// 0x007E8890 (:218)
// The item's primitive (added, or found in the preload file's container; null when the system does not validate),
// then, when building, its bitmap list (.slw3, else .tlist) and its peg: a preload widget's peg goes into the peg being
// built, any other becomes a peg primitive of the container (always-loaded ones flagged 8). Loading from the preload
// file only looks the peg up.
stream2_prim* assasm_container_add(const char* item_name, unsigned char prim_type, const char* category, bool add_peg, bool add_slw, bool report_errors, const char* peg_category, unsigned char allocator)
{
    stream2_prim* prim;
    if (Loading_from_preload_file)
    {
        if (!Validate)
        {
            return nullptr;
        }
        prim = find_prim(item_name);
    }
    else
    {
        prim = stream2_container_add_prim(item_name, category, prim_type, allocator, report_errors, 0, nullptr);
    }
    if (prim == nullptr)
    {
        return nullptr;
    }
    char filename[64];
    if (add_slw && !Loading_from_preload_file)
    {
        cf_add_ext(filename, sizeof(filename), item_name, "slw3", false);
        bool found = cf_exist(filename);
        if (!found)
        {
            cf_add_ext(filename, sizeof(filename), item_name, "tlist", false);
            found = cf_exist(filename);
        }
        if (found)
        {
            assasm_add_slw3(filename, peg_category != nullptr ? peg_category : category, report_errors);
        }
    }
    if (add_peg)
    {
        cf_add_ext(filename, sizeof(filename), item_name, Peg_platform_ext[Asm_platform], false);
        const bool exists = cf_exist(filename);
        if (!Widget_preload || !Building_peg)
        {
            if (exists)
            {
                if (!Loading_from_preload_file)
                {
                    if (peg_category == nullptr)
                    {
                        peg_category = category;
                    }
                    // Magic number: add flag 8 marks an always-loaded file.
                    stream2_container_add_prim(filename, peg_category, 1, allocator, report_errors, assasm_is_preload_file(filename) ? 8 : 0, nullptr);
                }
                else
                {
                    find_prim(filename);
                }
            }
        }
        else if (exists && !Loading_from_preload_file)
        {
            peg_asm_add(filename);
        }
    }
    return prim;
}

// 0x007D7E20 (:277)
// A bitmap list: into the peg being built (preload widget), else each of its bitmaps for this platform becomes a
// bitmap primitive of the container (always-loaded ones flagged 8). Nothing to do when loading from the preload file.
bool assasm_add_slw3(const char* slw_file, const char* category, bool report_errors)
{
    if (Widget_preload && Building_peg)
    {
        if (!Loading_from_preload_file)
        {
            return peg_asm_add_from_slw(slw_file, Asm_platform);
        }
        return true;
    }
    if (Loading_from_preload_file)
    {
        return true;
    }
    mempool_mark_scope mark(ass_Tmp_mempool);
    xml_element* root = xml_parse(slw_file, ass_Tmp_mempool);
    if (root == nullptr)
    {
        return false;
    }
    xml_element* rule = xml_find(root, "rule");
    if (rule == nullptr)
    {
        return false;
    }
    for (xml_element* platform = xml_find(rule, "platform"); platform != nullptr; platform = xml_find_next(rule, platform, "platform"))
    {
        if (vlib_find_platform(platform->text) != Asm_platform)
        {
            continue;
        }
        for (xml_element* source = xml_find(platform, "source"); source != nullptr; source = xml_find_next(platform, source, "source"))
        {
            if (v_stricmp(cf_get_ext(source->text), Vbm_platform_ext[Asm_platform]) == 0)
            {
                const bool always_loaded = always_loaded_get_tables_combined(Asm_platform)->exists(source->text);
                // Magic number: add flag 8 marks an always-loaded file.
                stream2_container_add_prim(source->text, category, 1, 0, report_errors, always_loaded ? 8 : 0, nullptr);
            }
        }
        return true;
    }
    return false;
}

// 0x007CAFD0 (:330)
// The container: finished when building (a DLC temp container is recorded), the preload file's otherwise; a preload
// widget's is preloaded unless no_preload.
// Notes (decompiled behaviour, kept as is): stream2_container_finish receives is_temp as its first (skip allocator size
// verification) argument.
unsigned int assasm_container_finish(bool no_preload, bool is_temp, bool patch)
{
    unsigned int srid = Current_srid;
    if (!Loading_from_preload_file)
    {
        srid = stream2_container_finish(is_temp, patch);
        Current_srid = srid;
        if (is_temp && srid != 0)
        {
            dlc_store_temp_srid(srid);
        }
    }
    if (Widget_preload && srid != 0 && !no_preload)
    {
        stream2_preload(srid);
        return Current_srid;
    }
    return srid;
}

// 0x007B9CB0 (:365)
// Ends the system: the built peg (or an empty peg_data), then the scratch pool goes back to its mark.
// Notes (decompiled behaviour, kept as is): verify_only and force_no_data_generate are unused.
bool assasm_finish(pasm_data* peg_data, bool verify_only, bool pack_pointers, bool force_no_data_generate)
{
    const bool was_loading = Loading_from_preload_file;
    const bool was_building_peg = Building_peg;
    bool result = true;
    Current_srid = 0;
    Building_peg = false;
    Loading_from_preload_file = false;
    peg_asm_set_error_callback(assasm_peg_asm_error_cb);
    if (!was_building_peg || was_loading)
    {
        if (peg_data != nullptr)
        {
            peg_data->cpu_data = nullptr;
            peg_data->gpu_data = nullptr;
            peg_data->cpu_size = 0;
            peg_data->gpu_size = 0;
        }
    }
    else
    {
        result = peg_asm_finish(peg_data, pack_pointers);
    }
    peg_asm_set_error_callback(nullptr);
    Asset_name[0] = '\0';
    if ((ass_Tmp_mempool->get_flags() & 0x40) != 0)
    {
        static_cast<split_mempool*>(ass_Tmp_mempool)->restore_to_mark_cpu(ass_Tmp_mempool_cpu_mark);
        static_cast<split_mempool*>(ass_Tmp_mempool)->restore_to_mark_gpu(ass_Tmp_mempool_gpu_mark);
    }
    else
    {
        ass_Tmp_mempool->restore_to_mark(ass_Tmp_mempool_cpu_mark);
    }
    ass_Tmp_mempool = nullptr;
    Container_pool = nullptr;
    return result;
}
