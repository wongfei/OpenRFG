// Original: volition/vlib/code/vlib_core/vlib_core.cpp
//
// Platform tables, vlib_system_init and the temp pool setters. The headless programs do not call vlib_system_init
// (their start-up calls cfile_system_init directly); the original boot does (orig_main.md OM3).

#include "volition/vlib/code/vlib_core/vlib_core.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/dynamic_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/thread.h"
#include "volition/vlib/code/os/timer.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <string.h>

const char* Vlib_platforms[8] = { "pc", "ps2", "ps3", "xbox", "xbox2", "xbone", "ps4", "switch" }; // 0x01519BC0
vlib_endianness Vlib_endianness[8] = // 0x01519BE0
{
    VLIB_LITTLE_ENDIAN, // pc
    VLIB_LITTLE_ENDIAN, // ps2
    VLIB_BIG_ENDIAN, // ps3
    VLIB_LITTLE_ENDIAN, // xbox
    VLIB_BIG_ENDIAN, // xbox2
    VLIB_LITTLE_ENDIAN, // xbone
    VLIB_LITTLE_ENDIAN, // ps4
    VLIB_LITTLE_ENDIAN, // switch
};
char* Vlib_relative_path; // 0x01CE990C

// 0x005CFD30 (:215)
// The relative path is copied into Vlib_mempool, the global timer starts, the global random table is filled from the
// generator and the thread system starts (cfile too when asked). vlib_mempool_size is not used.
bool vlib_system_init(const char* vlib_relative_path, bool init_cfile, int)
{
    const size_t length = vlib_relative_path != nullptr ? strlen(vlib_relative_path) : 0;
    Vlib_relative_path = (char*)Vlib_mempool.alloc((unsigned int)length + 1, 4);
    strcpy(Vlib_relative_path, vlib_relative_path != nullptr ? vlib_relative_path : "");
    // Inlined in rfg.exe (the statics are shared through the vlib unity build).
    vlib_timer_init();
    Vlib_rand_instance.m_next_rand = 0;
    // Magic number: the 2048 entries of the random table.
    for (int i = 0; i < 2048; i++)
    {
        Vlib_rand_instance.m_rand_array[i] = Vlib_rand_instance.genrand();
    }
    if (!thread_system_init())
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "C:/unit4projects/rfg/root/code/volition/vlib/code/vlib_core/vlib_core.cpp", 0xF1);
        return false;
    }
    if (init_cfile && !cfile_system_init(nullptr, nullptr, false))
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "C:/unit4projects/rfg/root/code/volition/vlib/code/vlib_core/vlib_core.cpp", 0xF9);
        return false;
    }
    return true;
}

// 0x005954D0 (:283)
void vlib_system_shutdown()
{
}

// 0x005954E0 (:293)
vlib_platform vlib_find_platform(const char* pname)
{
    for (int platform = PLATFORM_PC; platform < NUM_VLIB_PLATFORMS; ++platform)
    {
        if (keen::compareStringNoCase(Vlib_platforms[platform], pname) == 0)
        {
            return (vlib_platform)platform;
        }
    }
    return PLATFORM_NONE;
}

// 0x00595520 (:313)
unsigned int vlib_find_platforms(vlib_platform* platforms, const char* pnames)
{
    // pnames is a list of platform names separated by spaces, commas or tabs. Unknown names are skipped.
    char names[256];
    strcpy(names, pnames);
    unsigned int count = 0;
    for (char* token = strtok(names, " ,\t"); token != nullptr; token = strtok(nullptr, " ,\t"))
    {
        for (int platform = PLATFORM_PC; platform < NUM_VLIB_PLATFORMS; ++platform)
        {
            if (keen::compareStringNoCase(Vlib_platforms[platform], token) == 0)
            {
                platforms[count++] = (vlib_platform)platform;
                break;
            }
        }
    }
    return count;
}

// 0x005955E0 (:334)
void vlib_set_temp_mempool(mempool_base* pool)
{
    Vlib_temp_mempool = pool;
}

// 0x005955F0 (:342)
void vlib_set_temp_page_pool(page_pool* pool)
{
    Vlib_temp_page_pool = pool;
}
