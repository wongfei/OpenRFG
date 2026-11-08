// Original: volition/vfxlib/code/vfx_effect_manager.cpp
//
// The effect manager (VFX.md FX1): the effect free list (max_effects effects of 0xD0 bytes preallocated from the vfx
// mempool), the registered effects and the loaded templates (handle = index, removal swaps the last one in).

#include "volition/vfxlib/code/vfx_effect_manager.h"

#include "volition/vfxlib/code/vfx.h"
#include "volition/vfxlib/code/vfx_effect.h"
#include "volition/vfxlib/code/vfx_effect_template.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include "keen/base/trace.hpp"

#include <new>
#include <stdio.h>
#include <string.h>

// ---- statics
const int vfx_effect_manager::max_effects_default = 512; // 0x012C709C
const int vfx_effect_manager::max_effect_templates_default = 400; // 0x012C70A0
int vfx_effect_manager::max_effects = 512; // 0x016F0A04
int vfx_effect_manager::max_effect_templates = 400; // 0x016F0A08

// 0x00CAEB80
vfx_effect_manager::effect_list::effect_list()
{
}

// 0x00CAEBA0
vfx_effect_manager::effect_list::~effect_list()
{
}

// 0x00CAEBD0
vfx_effect_manager::effect_template_list::effect_template_list()
{
}

// 0x00CAEBF0
vfx_effect_manager::effect_template_list::~effect_template_list()
{
}

// 0x00CB4E00 (:17)
vfx_effect_manager::vfx_effect_manager()
{
}

// 0x00CAECA0 (:22)
vfx_effect_manager::~vfx_effect_manager()
{
}

// 0x00CB4E50 (:27): 0x48 bytes from the vfx mempool (no alignment).
vfx_effect_manager* vfx_effect_manager::create()
{
    void* data_p = vfx_system::get_mempool()->alloc(sizeof(vfx_effect_manager), 0);
    if (data_p == nullptr)
    {
        return nullptr;
    }
    return new (data_p) vfx_effect_manager();
}

// 0x00C9F030 (:37)
void vfx_effect_manager::release()
{
}

// 0x00CAED10 (:42): the effects are raw 0xD0-byte blocks (4-byte aligned, although vfx_effect is a 16-byte aligned
// class); vfx_effect::create constructs them in place.
void vfx_effect_manager::init()
{
    mempool* pool = vfx_system::get_mempool();
    effects_freelist.init((vfx_effect**)pool->alloc(max_effects * sizeof(vfx_effect*), 0), max_effects, 0);
    effects_freelist.resize(max_effects);
    for (int i = 0; i < max_effects; i++)
    {
        // Magic number: 0xD0 = sizeof(vfx_effect), 4 = the alignment rfg.exe asks for.
        effects_freelist[i] = (vfx_effect*)pool->alloc(sizeof(vfx_effect), 4);
    }
    effects_registered.init((vfx_effect**)pool->alloc(max_effects * sizeof(vfx_effect*), 0), max_effects, 0);
    effect_templates.init((vfx_effect_template**)pool->alloc(max_effect_templates * sizeof(vfx_effect_template*), 0), max_effect_templates, 0);
}

// 0x00CBA430 (:104): the template keeps its handle (the index in the template list, -1 when the list is full).
vfx_effect_template* vfx_effect_manager::load_template(const unsigned char* data_p, unsigned char* gpu_data, const char* peg_filename, int& approx_gpu_size)
{
    vfx_effect_template* template_p = vfx_effect_template::create();
    template_p->set_peg_filename(peg_filename);
    if (!template_p->serialize((vfx_effect_template::vfx_data*)data_p, gpu_data, true, approx_gpu_size))
    {
        template_p->release();
        return nullptr;
    }
    template_p->set_handle(effect_templates.add(template_p));
    return template_p;
}

// 0x00CBA4C0 (:120): a development path (rfg.exe flags it with a critical failure first). The peg goes next to the
// file; the file name copy for the template stays in the vfx mempool.
vfx_effect_template* vfx_effect_manager::load_template(const char* filename, const char* extension, int byte_alignment, int& approx_gpu_size)
{
    char full_filename[256];
    char message[256];
    VLIB_CRITICAL_FAILURE();
    mempool* pool = vfx_system::get_mempool();
    cf_add_ext(full_filename, sizeof(full_filename), filename, extension, false);
    char* peg_filename = (char*)pool->alloc((unsigned int)strlen(full_filename) + 1, 0);
    strcpy(peg_filename, full_filename);
    bm_load_peg(peg_filename, pool, true);
    cfile* p_cfile = cf_open(full_filename, "rb", PLATFORM_PC, false);
    if (p_cfile == nullptr)
    {
        // Notes (decompiled behaviour, kept as is): the message is formatted but never printed.
        sprintf(message, "Failed to open vfx: %s", full_filename);
        return nullptr;
    }
    const unsigned int size = cf_size(p_cfile);
    unsigned char* data_p = (unsigned char*)pool->alloc(size, (unsigned int)byte_alignment);
    if (data_p == nullptr)
    {
        sprintf(message, "Failed to allocate memory for vfx: %s", full_filename);
        keen::debug::nativePrintArgs(message);
        return nullptr;
    }
    cf_read(data_p, size, p_cfile, false);
    cf_close(p_cfile);
    return load_template(data_p, nullptr, peg_filename, approx_gpu_size);
}

// 0x00CA44F0 (:92): unloads the peg and swaps the last template into the freed handle.
// Notes (decompiled behaviour, kept as is): the template itself is not released here (vfx_template_release does), and
// the handle is written to the moved template only.
void vfx_effect_manager::release_template(vfx_effect_template* template_p)
{
    if (template_p->get_peg_filename() != nullptr)
    {
        bm_unload_peg(template_p->get_peg_filename());
    }
    const int handle = template_p->get_handle();
    effect_templates.remove_unsorted(handle);
    effect_templates[handle]->set_handle(handle);
}

// 0x00CAEE50 (:68)
vfx_effect* vfx_effect_manager::get_new_effect()
{
    if (effects_freelist.size() < 1)
    {
        return nullptr;
    }
    return effects_freelist.remove_back();
}

// 0x00CAEE80 (:76): the handle of the effect, or -1 when the list is full.
int vfx_effect_manager::register_effect(vfx_effect* effect_p)
{
    return effects_registered.add(effect_p);
}

// 0x00CAEED0 (:83): the last registered effect takes the freed handle; the effect goes back to the free list.
void vfx_effect_manager::unregister_effect(vfx_effect* effect_p)
{
    const int handle = effect_p->get_handle();
    effects_registered.remove_unsorted(handle);
    effects_registered[handle]->set_handle(handle);
    effects_freelist.add(effect_p);
}

// 0x00CBA630 (:183): the pool is not used (the manager allocates from the vfx mempool).
vfx_effect_template* vfx_template_load_from_file(mempool* pool, const char* filename, const char* filename_extension, int byte_alignment, int& approx_gpu_size)
{
    return vfx_system::get_effect_manager()->load_template(filename, filename_extension, byte_alignment, approx_gpu_size);
}

// 0x00CBA650 (:191)
vfx_effect_template* vfx_template_load_from_memory(const unsigned char* data_p, unsigned char* gpu_data, int& approx_gpu_size)
{
    return vfx_system::get_effect_manager()->load_template(data_p, gpu_data, nullptr, approx_gpu_size);
}

// 0x00CA4550 (:198)
void vfx_template_release(vfx_effect_template* effect_template_p)
{
    effect_template_p->release();
    vfx_system::get_effect_manager()->release_template(effect_template_p);
}
