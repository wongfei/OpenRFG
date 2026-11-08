// Original: volition/rfg/code/geom/rfg_material.cpp
//
// Standalone materials (VFX.md FX4): the GSP_MATERIAL stream2 callbacks load and unload the materials
// (material_shared.cpp) and keep them by (file name, srid) for material_find: the material effect layers (matfx_*
// containers), the head materials and the high-res weapon materials. Compiled in rfg_effects (OPENRFG_EFFECTS); the
// programs without it keep the stand-ins of tests/stubs/rfg/geom/geom.cpp (no material is loaded or found).
// The external interface is declared in material.h.

#include "volition/rfg/code/geom/material.h"
#include "volition/rfg/code/geom/material_shared.h"

#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/cfile.h"

#include "keen/base/trace.hpp"

#include <stdio.h>

// ---- globals and file statics
static bool Material_system_init; // 0x025EF8DB
static stream2_data_hash<material*,256,unsigned short> Material_filename_hash; // 0x02616C40

// ---- functions
// 0x00808AD0 (:38): the shared loader is inlined.
// Original: a material of a container without a srid is loaded but not kept (and still reported as loaded).
bool material_load(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size)
{
    material* mat_p = material_load(cpu_data, cpu_data_size);
    if (mat_p == nullptr)
    {
        // The original formats into a thread-local buffer.
        char message[256];
        sprintf(message, "Failed to load rl data for %s", filename);
        keen::debug::nativePrintArgs(message);
        return false;
    }
    Material_filename_hash.insert(filename, srid, mat_p);
    return true;
}

// 0x0080B550 (:64)
void material_unload(const char* filename, unsigned int srid)
{
    material* mat_p = nullptr;
    Material_filename_hash.get(filename, srid, mat_p);
    if (mat_p == nullptr)
    {
        return;
    }
    material_unload(mat_p);
    Material_filename_hash.remove(filename, srid);
}

// 0x007F4800 (:79)
void material_system_init()
{
    if (!Material_system_init)
    {
        Material_system_init = true;
    }
}

// 0x007F4820 (:88): not called in rfg.exe.
void material_system_shutdown()
{
    if (Material_system_init)
    {
        Material_system_init = false;
    }
}

// 0x0080B5C0 (:97): by file name (with or without the .mat_pc extension).
material* material_find(const char* filename, unsigned int srid)
{
    char name[64];
    cf_add_ext(name, sizeof(name), filename, "mat_pc", false);
    material* mat_p = nullptr;
    Material_filename_hash.get(name, srid, mat_p);
    return mat_p;
}

// 0x007F4840 (:109): empty; not called in rfg.exe.
void material_level_shutdown()
{
}
