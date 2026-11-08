// Original: volition/rfg/code/geom/anim.cpp
//
// The game's animation records (Anims, filled by vanim_parse_anim_tables from anim_files.xtbl in name order, so
// anim_find can search them by first letter) and the loading of animation data into anim_lib: single .anim_pc files
// (anim_load_animation), animation packfiles (anim_load_packfile, e.g. anims.vpp_pc) and streamed files
// (anim_file_loaded_callback). Part of rfg_geom_unity.cpp (player_minimal.md P2); vanim.cpp reads Anims and Num_anims.
// The tag name tables (vanim_tags.inl) are included here for anim_tag_state_from_enum and anim_tag_action_from_enum;
// vanim.cpp, later in the same unity file, reads them too.
// The external interface is declared in anim.h.

#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/vanim.h"

#include "keen/base/trace.hpp"
#include "volition/anim_lib/code/anim_lib.h"
#include "volition/anim_lib/code/anim_lib_internal.h"
#include "volition/rfg/code/gamewide/preload.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/cfile/packfile.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/util/string_comp.h"

#include <stdio.h>
#include <string.h>

// ---- globals
#include "volition/rfg/code/geom/vanim_tags.inl"

// ---- file-static globals
static anim Anims[4096]; // 0x0253D4C8
static int Num_anims; // 0x025EF8A8
static bool Anim_initialized; // 0x025EF8B0
static bool Anim_pool_locked; // 0x025EF8B1
static int Anim_pool_cache_size; // 0x025EF8B4
static int Anim_pool_alloc_size; // 0x025EF8B8
static cfile* Anim_cache_file; // 0x025EF8BC

// 0x007FB670. The preload loader of ".anim" files.
static void anim_animation_load_callback(const char* filename)
{
    anim_load_animation(filename);
}

// 0x007F1710. The streamed data is overlaid on the anim_lib slot unless the animation is already paged in
// (the original then formats the container name for a message it does not show).
bool anim_file_loaded_callback(const char* name, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size)
{
    char filename[128];
    cf_add_ext(filename, sizeof(filename), name, ".anim", false);
    const int handle = animlib_find_anim(filename);
    if (handle == -1) {
        return false;
    }
    if (!animlib_is_paged_in(handle)) {
        animlib_overlay_anim(filename, cpu_data, (unsigned int)cpu_data_size, false);
    }
    return true;
}

// 0x007F17D0
void anim_file_unloaded_callback(const char* name, unsigned int srid)
{
    char filename[128];
    cf_add_ext(filename, sizeof(filename), name, ".anim", false);
    const int handle = animlib_find_anim(filename);
    if (handle != -1) {
        animlib_unload_anim_data(handle);
    }
}

// 0x007F1840 (:1427). A cleared record; null when all 4096 are taken.
anim* anim_allocate()
{
    if (Num_anims > 0xFFF) {
        return nullptr;
    }
    anim* animp = &Anims[Num_anims];
    memset(animp, 0, sizeof(anim));
    Num_anims++;
    return animp;
}

// 0x007F1880
anim* anim_get_anim_from_handle(int handle)
{
    if ((unsigned int)handle < 4096) {
        return &Anims[handle];
    }
    return nullptr;
}

// 0x007F18A0 (:1446)
int anim_get_handle(const anim* s)
{
    return (int)(s - Anims);
}

// 0x007F18C0. A binary search for the first letter (lowercased; the table is in name order), then a linear search
// for the name crc from the lower bound of the remaining range.
anim* anim_find(const char* filename)
{
    if (filename == nullptr) {
        return nullptr;
    }
    char name[128];
    cf_drop_ext(name, sizeof(name), filename);
    const unsigned int crc = crc_stri(name, 0);
    int low = 0;
    int high = Num_anims - 1;
    for (int span = high; span > -1; span = high - low) {
        const int mid = (span >> 1) + low;
        if (Anims[mid].filename == nullptr) {
            return nullptr;
        }
        char c = name[0];
        if ((unsigned char)(c - 'A') < 26) {
            c = (char)(c + ' ');
        }
        const char first = Anims[mid].filename[0];
        if (c < first) {
            high = mid - 1;
        } else if (c > first) {
            low = mid + 1;
        } else {
            break;
        }
    }
    for (int i = low; i <= high; i++) {
        if (Anims[i].name_crc == crc) {
            return &Anims[i];
        }
    }
    // "One of the animation set files (male.xtbl, player.xtbl, etc.) is inconsistent with anim_files.xtbl; the latter
    // is missing animation file '%s'." is formatted and not shown.
    return nullptr;
}

// 0x007FB680
void anim_system_init()
{
    for (anim& a : Anims) {
        a.filename = nullptr;
    }
    preload_register_extension(".anim", anim_animation_load_callback);
    Anim_initialized = true;
}

// 0x007F19D0 (:1521): empty in rfg.exe.
void anim_init()
{
}

// 0x007F19E0 (:1528): empty in rfg.exe.
void anim_close()
{
}

// 0x007F19F0 (:1533)
void anim_reset()
{
    Num_anims = 0;
    Num_anim_variants = 0;
}

// 0x007F1A00. Loads a .anim_pc file into Animation_mempool (permanent overlay); returns its size, 0 when the
// animation is unknown, already paged in or missing.
int anim_load_animation(const char* filename)
{
    const int handle = animlib_find_anim(filename);
    if (handle == -1 || animlib_is_paged_in(handle)) {
        return 0;
    }
    char name[128];
    cf_add_ext(name, sizeof(name), filename, ".anim_pc", false);
    cfile* fp = cf_open(name, "rb", PLATFORM_PC, false);
    if (fp == nullptr) {
        // "Unable to load %s as an animation file.  File probably doesn't exist." (formatted, not shown)
        return 0;
    }
    const unsigned int size = cf_size(fp);
    void* data = Animation_mempool.alloc(size, 16);
    cf_read(data, size, fp, false);
    cf_close(fp);
    animlib_overlay_anim(filename, data, size, true);
    return (int)size;
}

// 0x007F1B20 (:1583). Magic number: 322 states.
const char* anim_tag_state_from_enum(animation_state anim_state)
{
    if ((int)anim_state >= 0 && (int)anim_state < 322) {
        return Animation_state_tags[anim_state];
    }
    return nullptr;
}

// 0x007F1B40 (:1592). Magic number: 661 actions after the states.
const char* anim_tag_action_from_enum(animation_action anim_action)
{
    const int index = (int)anim_action - (int)ANIMATION_ACTION_IDLE_A;
    if (index >= 0 && index < 661) {
        return Animation_action_tags[index];
    }
    return nullptr;
}

// 0x007F1B60. A condensed animation packfile (anims.vpp_pc): its data block goes to Animation_mempool, every .anim is
// overlaid on its anim_lib slot (allocated when new) and every .rig becomes an anim_lib rig. Returns the packfile size.
int anim_load_packfile(const char* filename)
{
    game_enable_standard_access();
    mempool_mark_scope mark(&Common_mempool);
    v_packfile packfile;
    if (!packfile_read_condensed(filename, &Common_mempool, &Animation_mempool, &packfile, false)) {
        game_disable_standard_access();
        return 0;
    }
    for (unsigned int i = 0; i < packfile.num_files; i++) {
        const v_packfile_entry& entry = packfile.dir[i];
        const char* ext = strrchr(entry.name, '.');
        char name[64];
        if (v_strnicmp(ext, ".anim", 5) == 0) {
            cf_add_ext(name, sizeof(name), entry.name, ".anim", false);
            if (animlib_find_anim(name) == -1) {
                animlib_get_anim_slot(name);
            }
            animlib_overlay_anim(name, packfile.data + entry.start, entry.size, true);
        } else if (v_strnicmp(ext, ".rig", 5) == 0) {
            cf_add_ext(name, sizeof(name), entry.name, ".rig", false);
            // Original bug kept: the rig's start offset is passed as its size.
            animlib_new_anim_rig(name, packfile.data + entry.start, entry.start, true);
        } else {
            keen::debug::nativePrintArgs("invalid file type in animation packfile. ??\n");
        }
    }
    game_disable_standard_access();
    return (int)packfile.file_size;
}

// 0x007F1D20 (:1684). Frees Animation_mempool for other use: what is in it goes to a temporary file. Null when it is
// locked already; before anim_system_init there is nothing to save. Magic name: "anim_temp.dat".
rfg_split_mempool* __anim_pool_lock()
{
    if (Anim_initialized) {
        if (Anim_pool_locked) {
            return nullptr;
        }
        Anim_pool_locked = true;
        Anim_pool_cache_size = (int)(Animation_mempool.space_max() - Animation_mempool.space_free());
        Anim_pool_alloc_size = Anim_pool_cache_size;
        Anim_cache_file = cf_open("anim_temp.dat", "wb", PLATFORM_PC, false);
        if (Anim_cache_file != nullptr) {
            cf_write(Animation_mempool.get_base(), Anim_pool_cache_size, Anim_cache_file);
            cf_close(Anim_cache_file);
        }
        Animation_mempool.clear();
    }
    return &Animation_mempool;
}

// 0x007F1DC0 (:1719). Starts reading the data back (the file is opened asynchronously).
void __anim_pool_unlock_async()
{
    if (Anim_initialized && Anim_pool_locked) {
        void* data = Animation_mempool.alloc(Anim_pool_alloc_size, 0);
        Anim_cache_file = cf_open("anim_temp.dat", "rb", PLATFORM_PC, true);
        if (Anim_cache_file != nullptr) {
            cf_read(data, Anim_pool_cache_size, Anim_cache_file, false);
        }
        Anim_pool_cache_size = 0;
        Anim_pool_alloc_size = 0;
    }
}

// 0x007F1E30 (:1743). True once the read is done.
bool __anim_pool_unlock_async_poll()
{
    if (Anim_initialized) {
        if (cf_sync(Anim_cache_file) == 0) {
            return false;
        }
        cf_close(Anim_cache_file);
    }
    Anim_pool_locked = false;
    return true;
}

// 0x007F1E70 (:1760). The read, waited for.
void __anim_pool_unlock()
{
    __anim_pool_unlock_async();
    while (!__anim_pool_unlock_async_poll()) {
    }
}
