// Original: volition/vlib/code/vlib_core/vlib_core.h
// Implementation: volition/vlib/code/vlib_core/vlib_core.cpp
//
// vlib start-up and the platform tables. Placement of vlib_platform and vlib_endianness inferred (vlib_core.cpp is
// their main user; they may come from common/vtypes.h).
// vlib_system_init (decompiled): copies vlib_relative_path into Vlib_mempool, starts Global_time once, seeds
// Vlib_rand_instance, thread_system_init, then cfile_system_init(nullptr, nullptr) when init_cfile is set.
// Vlib_platforms holds the names vlib_find_platform matches, in vlib_platform order: "pc", "ps2", "ps3", "xbox", "xbox2",
// "xbone", "ps4", "switch". Vlib_endianness is the byte order of each: big-endian only for PS3 and Xbox 360.
// cfile sets cfile::m_flags bit 0x4 (swap) when Vlib_endianness[disk_platform] differs from the PC entry.
// Vlib_rand_instance (rand_instance, math/rand) is declared with its type in a later tier.
#pragma once

class mempool_base;
class page_pool;

enum vlib_platform
{
    PLATFORM_NONE = -1,
    PLATFORM_PC = 0,
    PLATFORM_PS2 = 1,
    PLATFORM_PS3 = 2,
    PLATFORM_XBOX = 3,
    PLATFORM_XBOX2 = 4,
    PLATFORM_XBONE = 5,
    PLATFORM_PS4 = 6,
    PLATFORM_SWITCH = 7,
    NUM_VLIB_PLATFORMS = 8,
};

enum vlib_endianness
{
    VLIB_BIG_ENDIAN = 0,
    VLIB_LITTLE_ENDIAN = 1,
};

extern const char* Vlib_platforms[8]; // 0x01519BC0
extern vlib_endianness Vlib_endianness[8]; // 0x01519BE0
extern char* Vlib_relative_path; // 0x01CE990C

bool vlib_system_init(const char* vlib_relative_path, bool init_cfile, int vlib_mempool_size); // 0x005CFD30 (:215)
void vlib_system_shutdown(); // 0x005954D0 (:283)
vlib_platform vlib_find_platform(const char* pname); // 0x005954E0 (:293)
unsigned int vlib_find_platforms(vlib_platform* platforms, const char* pnames); // 0x00595520 (:313)
void vlib_set_temp_mempool(mempool_base* pool); // 0x005955E0 (:334)
void vlib_set_temp_page_pool(page_pool* pool); // 0x005955F0 (:342)
