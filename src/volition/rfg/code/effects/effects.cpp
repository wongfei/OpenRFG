// Original: volition/rfg/code/effects/effects.cpp
//
// The effect system (decompiled; header comment in effects.h).
//
// Split TU (VFX.md 2.6): this file is compiled twice. The rfg library builds the table part (radial blur and visual
// effect tables, the effect pool's preload marks and async lock, start-up and shutdown), which every program needs.
// The effect library (rfg_effects, OPENRFG_EFFECTS) builds the playback part: template loading, the backpack
// templates, the multiplayer preload and the effects_play family, which the programs without it keep as stand-ins.
// Deviation: Effects_preload_srid and the singleplayer preload marks are file statics in rfg.exe; they have external
// linkage here because both parts use them.

#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/code_effects/bullet_sparks.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/util/special_allocators.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/bmpman/peg_assemble.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_comp.h"

#ifdef OPENRFG_EFFECTS
#include "keen/base/string.hpp"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"
#include "volition/rfg/code/game_render/fullscreen_effects/fb_accum_blur.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/havok/constraint_template.h"
#include "volition/rfg/code/level/rfg_chunk_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_constraint.h"
#include "volition/render_lib/code/rl_renderable_particles_instance.h"
#include "volition/vfxlib/code/vfx_effect_manager.h"
#include "volition/vfxlib/code/vfx_effect_template.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/hash_table.h"
#endif

#include <stdio.h>
#include <string.h>

#ifndef OPENRFG_EFFECTS
// =====================================================================================================================
// The table part (rfg library)
// =====================================================================================================================

// Shared with the playback part (see the header comment).
// Deviation from rfg.exe: not static, because both parts of the split effects.cpp use these.
unsigned int Effects_preload_srid; // 0x02182CEC
unsigned int Effects_preload_mempool_sp_mark; // 0x02182CD0
unsigned int Effects_preload_gpu_mempool_sp_mark; // 0x02182CD4

unsigned int Dlc_effects_preload_srid; // 0x02182CC0
int Num_visual_effect_types_dlc_mark; // 0x02182DFC
unsigned int Num_effect_types_dlc_mark; // 0x02182DF4, marked and restored by downloadable_content.cpp

// ---- file-static globals
static radial_blur_info Radial_blur_info[200]; // 0x02106348
static int Radial_blur_types; // 0x02182CBC
static bool Effects_system_init; // 0x02182CE8
static int Effects_preload_locked; // 0x02182CC4
static unsigned int Effects_preload_mempool_mark; // 0x02182CC8
static unsigned int Effects_preload_gpu_mempool_mark; // 0x02182CCC
static unsigned int Effects_preload_mempool_peg_mark; // 0x02182CD8
static unsigned int Effects_preload_gpu_mempool_peg_mark; // 0x02182CDC

// 0x00742390 (:142)
static void radial_blur_parse_table()
{
    xml_element* root = xml_table_parse("radial_blur.xtbl");
    Radial_blur_types = 0;
    for (xml_element* entry = xml_table_find_opt(root, "Radial_blur"); entry != nullptr; entry = xml_table_find_next(root, entry, "Radial_blur"))
    {
        radial_blur_info& info = Radial_blur_info[Radial_blur_types++];
        xml_table_get_req_string(info.m_name, 0x40, entry, "Name");
        xml_table_get_req_float(info.m_strength, entry, "Strength");
        xml_table_get_req_float(info.m_duration, entry, "Duration");
        xml_table_get_req_float(info.m_radius, entry, "Radius");
        xml_table_get_req_float(info.m_distance_fade, entry, "Distance_fade");
        xml_table_get_req_int(info.m_priority, entry, "Priority");
    }
    xml_table_close(root);
}

// Marks of the preload pool (the CPU and GPU halves when it is split); inlined in the original.
static void effects_mark_preload_pool(unsigned int& cpu_mark, unsigned int& gpu_mark)
{
    if ((Effect_preload_mempool.get_flags() & 0x40) == 0)
    {
        cpu_mark = Effect_preload_mempool.static_mempool_base::mark();
    }
    else
    {
        cpu_mark = Effect_preload_mempool.mark_cpu();
        gpu_mark = Effect_preload_mempool.mark_gpu();
    }
}

// Preloads the containers of the visual effects in [first, Num_visual_effect_types) whose flags match (singleplayer
// effects, multiplayer flag 0x8 as given), marking them loaded (flags 0x3); inlined in the original.
static void effects_preload_visual_effects(int first, bool multiplayer)
{
    for (int i = first; i < Num_visual_effect_types; ++i)
    {
        visual_effect_info& info = Visual_effect_info[i];
        if (info.srid != 0 && (info.flags & 0x10) != 0 && ((info.flags & 8) != 0) == multiplayer && info.stream_category == 0)
        {
            if (stream2_preload(info.srid))
            {
                info.flags |= 3;
            }
        }
    }
}

// 0x007452B0 (:169)
void visual_effects_parse_table(int dlc_id)
{
    // Parses vfx.xtbl into Effect_preload_mempool (DLC: DLC_effect_preload_mempool), saves the built peg and
    // preloads the preload-category containers: multiplayer-flagged ones first, then the others (DLC: only the
    // multiplayer-flagged ones), then the effect peg container itself.
    const bool is_dlc = dlc_id != -1;
    rfg_split_mempool* pool;
    mempool_base* tmp_pool;
    if (is_dlc)
    {
        pool = &DLC_effect_preload_mempool;
        tmp_pool = &Common_mempool;
    }
    else
    {
        pool = &Effect_preload_mempool;
        tmp_pool = salloc_pregame_split_lock(0xA00000, 0);
    }
    pasm_data peg_data;
    int cpu_mark;
    int gpu_mark;
    visual_effects_parse_table_shared(&peg_data, cpu_mark, gpu_mark, tmp_pool, pool, false, PLATFORM_PC, false, dlc_id);
    char peg_filename[64];
    if (is_dlc)
    {
        cf_add_ext(peg_filename, sizeof(peg_filename), "dlc_vfx_containers", ".cpeg_pc", false);
    }
    else
    {
        sprintf(peg_filename, "%s\\%s", "/gamebuild", "vfx_containers");
        cf_add_ext(peg_filename, sizeof(peg_filename), "vfx_containers", ".cpeg_pc", false);
    }
    if (peg_data.cpu_data != nullptr)
    {
        bm_save_peg(peg_filename, (peg_header*)peg_data.cpu_data);
        if ((pool->get_flags() & 0x40) == 0)
        {
            pool->restore_to_mark((unsigned int)cpu_mark);
        }
        else
        {
            pool->restore_to_mark_cpu((unsigned int)cpu_mark);
            pool->restore_to_mark_gpu((unsigned int)gpu_mark);
        }
    }
    if (is_dlc)
    {
        stream2_container_start(peg_filename, &DLC_effect_preload_mempool, 4, false, false);
        stream2_container_add_prim(peg_filename, "effects dlc", 1, 0, true, 0, nullptr);
        Dlc_effects_preload_srid = stream2_container_finish(false, false);
        effects_preload_visual_effects(Num_visual_effect_types_dlc_mark, true);
        stream2_preload(Dlc_effects_preload_srid);
        return;
    }
    stream2_container_start(peg_filename, &Effect_preload_mempool, 3, false, false);
    stream2_container_add_prim(peg_filename, "effects preload", 1, 0, true, 0, nullptr);
    Effects_preload_srid = stream2_container_finish(false, false);
    effects_mark_preload_pool(Effects_preload_mempool_mark, Effects_preload_gpu_mempool_mark);
    effects_preload_visual_effects(0, true);
    effects_mark_preload_pool(Effects_preload_mempool_sp_mark, Effects_preload_gpu_mempool_sp_mark);
    effects_preload_visual_effects(0, false);
    effects_mark_preload_pool(Effects_preload_mempool_peg_mark, Effects_preload_gpu_mempool_peg_mark);
    stream2_preload(Effects_preload_srid);
    salloc_pregame_split_unlock(static_cast<rfg_split_mempool*>(tmp_pool));
    Effect_preload_mempool.lock();
}

// 0x007477B0 (:335)
// The table loads of effects_system_init (no caller in rfg.exe).
static void effects_parse_table()
{
    mempool_mark_scope mark_scope(&Common_mempool);
    radial_blur_parse_table();
    visual_effects_parse_table(-1);
    effects_parse_table_shared(-1);
}

// 0x00742460 (:346)
// Releases the preloaded visual effect containers and the effect peg container.
static void effects_release_preloaded()
{
    for (int i = 0; i < Num_visual_effect_types; ++i)
    {
        visual_effect_info& info = Visual_effect_info[i];
        if ((info.flags & 2) != 0)
        {
            const unsigned int container_srid = stream2_container_find(info.vfx_filename);
            if (container_srid != 0)
            {
                stream2_release(container_srid);
            }
            info.flags &= ~1;
        }
    }
    stream2_release(Effects_preload_srid);
}

// 0x00747860 (:676)
void effects_system_init()
{
    if (Effects_system_init)
    {
        return;
    }
    Effects_system_init = true;
    {
        mempool_mark_scope mark_scope(&Common_mempool);
        radial_blur_parse_table();
        visual_effects_parse_table(-1);
        effects_parse_table_shared(-1);
    }
    bullet_spark_system_init();
}

// 0x007427E0 (:696)
void effects_system_shutdown()
{
    if (!Effects_system_init)
    {
        return;
    }
    Effects_system_init = false;
    effects_release_preloaded();
}

// 0x00742840 (:725)
radial_blur_info* effects_get_radial_blur_info(const char* name)
{
    if (name != nullptr)
    {
        for (int i = 0; i < Radial_blur_types; ++i)
        {
            if (v_stricmp(name, Radial_blur_info[i].m_name) == 0)
            {
                return &Radial_blur_info[i];
            }
        }
    }
    return nullptr;
}

// 0x00742900 (:1135)
// The stream allocator borrows the effect preload pool: release the effect peg container (once).
bool __effects_pool_lock_async()
{
    if (Effects_preload_locked != 0)
    {
        return false;
    }
    Effects_preload_locked = 1;
    if (stream2_get_status(Effects_preload_srid) == ST_LOADED)
    {
        stream2_release(Effects_preload_srid);
    }
    return true;
}

// 0x00742940 (:1149)
// Once the peg container is out: unload the peg and hand out the pool from the peg mark.
rfg_split_mempool* __effects_pool_lock_async_poll()
{
    if (stream2_get_status(Effects_preload_srid) == ST_LOADED)
    {
        return nullptr;
    }
    bm_unload_peg("vfx_containers");
    Effect_preload_mempool.unlock();
    if ((Effect_preload_mempool.get_flags() & 0x40) != 0)
    {
        Effect_preload_mempool.restore_to_mark_cpu(Effects_preload_mempool_peg_mark);
        Effect_preload_mempool.restore_to_mark_gpu(Effects_preload_gpu_mempool_peg_mark);
        return &Effect_preload_mempool;
    }
    Effect_preload_mempool.restore_to_mark(Effects_preload_mempool_peg_mark);
    return &Effect_preload_mempool;
}

// 0x007429C0 (:1176)
void __effects_pool_unlock_async()
{
    if (Effects_preload_locked != 0)
    {
        stream2_request(Effects_preload_srid);
    }
}

// 0x007429E0 (:1186)
// Done once the peg container is back (or failed).
bool __effects_pool_unlock_async_poll()
{
    if (Effects_preload_locked != 0 && Effects_preload_srid != 0)
    {
        const stream2_status_val status = stream2_get_status(Effects_preload_srid);
        if (status != ST_ERROR)
        {
            if (status != ST_LOADED)
            {
                return false;
            }
            Effect_preload_mempool.lock();
            Effects_preload_locked = 0;
        }
    }
    return true;
}

// 0x00742A20 (:1547)
bool effects_get_enabled()
{
    return true;
}

#else // OPENRFG_EFFECTS
// =====================================================================================================================
// The playback part (rfg_effects)
// =====================================================================================================================

static const int NUM_BACKPACK_EFFECTS = 15;
static const unsigned int BACKPACK_VFX_DATA_SIZE = 0x8000; // the copy buffer of one backpack template
static const unsigned int TEMPLATE_FILENAME_HASH_SIZE = 1500;
// effects_play refuses an effect unless this many parametric particle constant blocks are free.
static const int MIN_FREE_PARAMETRIC_CONSTANTS = 20;
// The console's replace and play-last effects play this far in front of the camera or player.
static const float CONSOLE_EFFECT_DISTANCE = 10.0f;

// ---- file-local types (rfg.pdb)
// size 0x8000
struct vfx_data_hack
{
    unsigned char data[BACKPACK_VFX_DATA_SIZE]; // 0x00
};
static_assert(sizeof(vfx_data_hack) == 0x8000, "vfx_data_hack");

// The table part's (see the header comment).
extern unsigned int Effects_preload_srid; // 0x02182CEC
extern unsigned int Effects_preload_mempool_sp_mark; // 0x02182CD0
extern unsigned int Effects_preload_gpu_mempool_sp_mark; // 0x02182CD4

bool Effects_lod_enabled = true; // 0x01650931
char effect_last_played_console[256]; // 0x02182CF0

// ---- file-static globals
static unsigned int Effects_replace_objh = 0xFFFFFFFF; // 0x01650AA0
// The backpack templates: copied into s_backpackVFXData and loaded from the copy, once (is_backpack_effect).
static const char* s_backpackEffectNames[NUM_BACKPACK_EFFECTS] = { // 0x01650AA4
    "mp_thrust_activate",
    "mp_thrust_loop",
    "mp_thrust_dust",
    "mp_thrust_down",
    "mp_thrust_down_activate",
    "mp_fleetfoot_contrail",
    "mp_fleetfoot_player_contrail",
    "mp_rhino_engine_blast",
    "mp_rhino_shield",
    "mp_rhino_activate",
    "mp_rhino_arc",
    "mp_shockwave_passive_a",
    "mp_shockwave_active_a",
    "mp_backpack_heal_a",
    "mp_backpack_tremor_a",
};
static vfx_effect_template* s_backpackEffects[NUM_BACKPACK_EFFECTS]; // 0x0218286C
static bool s_backpackVFXDataLoaded[NUM_BACKPACK_EFFECTS]; // 0x02182CAC
static unsigned int MP_preload_mempool_mark; // 0x02182CE0
static unsigned int MP_preload_gpu_mempool_mark; // 0x02182CE4
// alignas(16): rfg.exe's array is 16-aligned (0x0210A810). The template's mesh data is loaded in place from the copy and
// rl_submesh_data_load rejects submeshes off a 16-byte boundary; unaligned, the rhino templates (the only backpack ones
// with meshes) failed to load, and stream2 then skipped the rest of every container holding them (ns_base: the terrain
// detail textures).
alignas(16) static vfx_data_hack s_backpackVFXData[NUM_BACKPACK_EFFECTS]; // 0x0210A810
// Every other loaded template, by container file name.
static str_data_hash_table<vfx_effect_template*, TEMPLATE_FILENAME_HASH_SIZE, 64, unsigned short> Template_filename_hash; // 0x021A3248

// 0x00747760 (:133)
// Destroys every effect object (no caller in rfg.exe).
static void effects_kill_all()
{
    const int num = World.get_num_objects<object_effect>();
    for (int i = 0; i < num; ++i)
    {
        World.destroy_object(World.get_object<object_effect>(i), false);
    }
}

// 0x007424C0 (:393)
// The name up to its first '.'.
void get_effect_name_without_extension(char* effect_name_without_extension, unsigned int capacity, const char* src_effect_name)
{
    keen::copyString(effect_name_without_extension, capacity, src_effect_name);
    for (unsigned int i = 0; i < capacity; ++i)
    {
        if (effect_name_without_extension[i] == '.')
        {
            effect_name_without_extension[i] = '\0';
            return;
        }
    }
}

// 0x00742500 (:406)
// The backpack template slot of a file name (extension ignored), or 0xFFFFFFFF.
unsigned int get_backpack_array_index(const char* effect_name)
{
    char name[128];
    get_effect_name_without_extension(name, sizeof(name), effect_name);
    for (unsigned int i = 0; i < NUM_BACKPACK_EFFECTS; ++i)
    {
        if (keen::compareString(name, s_backpackEffectNames[i]) == 0)
        {
            return i;
        }
    }
    return 0xFFFFFFFF;
}

// 0x007425A0 (:422)
bool is_backpack_effect(const char* effect_name)
{
    char name[128];
    get_effect_name_without_extension(name, sizeof(name), effect_name);
    for (unsigned int i = 0; i < NUM_BACKPACK_EFFECTS; ++i)
    {
        if (keen::compareString(name, s_backpackEffectNames[i]) == 0)
        {
            return true;
        }
    }
    return false;
}

// 0x007477F0 (:438)
bool is_backpack_effect(vfx_effect_template* template_p)
{
    return is_backpack_effect(template_p->get_effect_name());
}

// 0x0074B800 (:443)
// The stream2 load callback of GSP_VFX. A backpack template is copied into its slot's buffer (it must fit in
// 0x8000 bytes) and loaded from the copy, only the first time; every other template is loaded in place and kept by
// file name. False only for a template that fails to load: stream2 then skips the rest of the container.
bool effects_template_load_cb(const char* filename, unsigned int srid, unsigned char* cpu_data_p, int cpu_size, unsigned char* gpu_data_p, int gpu_size)
{
    int load_size = 0;
    if (is_backpack_effect(filename))
    {
        // An already loaded backpack template (another container has it too) is a success.
        const unsigned int index = get_backpack_array_index(filename);
        if (index != 0xFFFFFFFF && !s_backpackVFXDataLoaded[index])
        {
            if ((unsigned int)cpu_size < BACKPACK_VFX_DATA_SIZE)
            {
                memmove(s_backpackVFXData[index].data, cpu_data_p, cpu_size);
                vfx_effect_template* template_p = vfx_template_load_from_memory(s_backpackVFXData[index].data, gpu_data_p, load_size);
                if (template_p != nullptr)
                {
                    s_backpackEffects[index] = template_p;
                    s_backpackVFXDataLoaded[index] = true;
                    return true;
                }
            }
            return false;
        }
    }
    else if (Template_filename_hash.get_ptr(filename) == nullptr)
    {
        vfx_effect_template* template_p = vfx_template_load_from_memory(cpu_data_p, gpu_data_p, load_size);
        if (template_p == nullptr)
        {
            return false;
        }
        Template_filename_hash.insert_key_data(filename, template_p);
    }
    return true;
}

// 0x00747810 (:494)
// Backpack templates stay loaded (effects_system_release_backpack_effects).
void effects_template_unload_cb(const char* filename, unsigned int srid)
{
    vfx_effect_template** template_pp = Template_filename_hash.get_ptr(filename);
    if (template_pp != nullptr && *template_pp != nullptr)
    {
        vfx_template_release(*template_pp);
        Template_filename_hash.remove(filename);
    }
}

// 0x00745620 (:520)
// Builds the multiplayer effect peg into MP_effect_preload_mempool and loads it, releases the preloaded containers of
// the visual effects that are not multiplayer ones (flag 0x8), marks the pool and preloads the multiplayer visual
// effects that are not in the preload category (flag 0x10).
void mp_preload_visual_effects()
{
    timer_get(TIMER_MILLISECONDS);
    Load_timer_indent += 2;
    rfg_split_mempool* tmp_pool = salloc_pregame_split_lock(0xA00000, 0);
    pasm_data peg_data;
    peg_data.cpu_data = nullptr;
    peg_data.cpu_size = 0;
    peg_data.gpu_data = nullptr;
    peg_data.gpu_size = 0;
    int cpu_mark;
    int gpu_mark;
    visual_effects_parse_table_shared(&peg_data, cpu_mark, gpu_mark, tmp_pool, &MP_effect_preload_mempool, true, PLATFORM_PC, false, -1);
    char peg_filename[64];
    cf_add_ext(peg_filename, sizeof(peg_filename), "vfx_mp_containers", ".cpeg_pc", false);
    if (peg_data.cpu_data == nullptr)
    {
        bm_load_peg(peg_filename, &MP_effect_preload_mempool, true);
    }
    else
    {
        bm_load_peg(peg_filename, peg_data.cpu_data, peg_data.cpu_size, peg_data.gpu_data, peg_data.gpu_size, true);
    }
    for (int i = 0; i < Num_visual_effect_types; ++i)
    {
        visual_effect_info& info = Visual_effect_info[i];
        if (info.srid != 0 && (info.flags & 2) != 0 && (info.flags & 8) == 0)
        {
            info.flags &= ~2;
            stream2_release(info.srid);
        }
    }
    if ((MP_effect_preload_mempool.get_flags() & 0x40) == 0)
    {
        MP_preload_mempool_mark = MP_effect_preload_mempool.static_mempool_base::mark();
    }
    else
    {
        MP_preload_mempool_mark = MP_effect_preload_mempool.mark_cpu();
        MP_preload_gpu_mempool_mark = MP_effect_preload_mempool.mark_gpu();
    }
    for (int i = 0; i < Num_visual_effect_types; ++i)
    {
        visual_effect_info& info = Visual_effect_info[i];
        if ((info.flags & 8) != 0 && (info.flags & 0x10) == 0)
        {
            info.srid = stream2_container_find(info.vfx_filename);
            if (stream2_preload(info.srid))
            {
                info.flags |= 3;
            }        }
    }
    salloc_pregame_split_unlock(tmp_pool);
    Load_timer_indent -= 2;
}

// 0x00742640 (:593)
// The reverse: releases (and deletes, when unreferenced) the multiplayer containers, unloads their peg, takes the
// effect peg container out, rewinds the effect pool to the singleplayer mark and preloads the singleplayer preload
// containers and the effect peg again.
void mp_unload_visual_effects()
{
    timer_get(TIMER_MILLISECONDS);
    Load_timer_indent += 2;
    for (int i = 0; i < Num_visual_effect_types; ++i)
    {
        visual_effect_info& info = Visual_effect_info[i];
        if (info.srid != 0 && (info.flags & 2) != 0 && (info.flags & 0x10) == 0)
        {
            info.flags &= ~3;
            stream2_release(info.srid);
            stream2_container* container_p = stream2_get_container(info.srid);
            if (container_p != nullptr && container_p->get_ref_count() == 0)
            {
                stream2_container_delete(info.srid);
            }
            info.srid = 0;
        }
    }
    char peg_filename[64];
    cf_add_ext(peg_filename, sizeof(peg_filename), "vfx_mp_containers", ".cpeg_pc", false);
    bm_unload_peg(peg_filename);
    Effect_preload_mempool.unlock();
    if (stream2_get_status(Effects_preload_srid) == ST_LOADED)
    {
        stream2_release(Effects_preload_srid);
    }
    else
    {
        bm_unload_peg("vfx_containers");
    }
    if ((Effect_preload_mempool.get_flags() & 0x40) == 0)
    {
        Effect_preload_mempool.restore_to_mark(Effects_preload_mempool_sp_mark);
    }
    else
    {
        Effect_preload_mempool.restore_to_mark_cpu(Effects_preload_mempool_sp_mark);
        Effect_preload_mempool.restore_to_mark_gpu(Effects_preload_gpu_mempool_sp_mark);
    }
    for (int i = 0; i < Num_visual_effect_types; ++i)
    {
        visual_effect_info& info = Visual_effect_info[i];
        if (info.srid != 0 && (info.flags & 0x10) != 0 && (info.flags & 10) == 0 && info.stream_category == 0) // 10: flags 0x2 and 0x8
        {
            if (stream2_preload(info.srid))
            {
                info.flags |= 3;
            }
        }
    }
    stream2_preload(Effects_preload_srid);
    Effect_preload_mempool.lock();
    Load_timer_indent -= 2;
}

// 0x00742800 (:706)
void effects_system_release_backpack_effects()
{
    for (int i = 0; i < NUM_BACKPACK_EFFECTS; ++i)
    {
        if (s_backpackEffects[i] != nullptr)
        {
            vfx_template_release(s_backpackEffects[i]);
        }
    }
}

// 0x00742820 (:717)
void effects_level_init_pre()
{
}

// 0x00742830 (:721)
void effects_level_shutdown()
{
}

// 0x007478B0 (:738)
// A backpack template by slot, any other by file name.
vfx_effect_template* effects_template_find(const char* name)
{
    const unsigned int backpack_index = get_backpack_array_index(name);
    if (backpack_index != 0xFFFFFFFF)
    {
        return s_backpackEffects[backpack_index];
    }
    vfx_effect_template** template_pp = Template_filename_hash.get_ptr(name);
    return template_pp != nullptr ? *template_pp : nullptr;
}

// 0x007457D0 (:751)
// The effect whose visual effect's file is this file (path and extension ignored), or 0xFFFFFFFF.
unsigned int effects_get_handle_from_vfx_filename(const char* name)
{
    char wanted[64];
    char visual_name[64];
    cf_drop_path(wanted, sizeof(wanted), name);
    cf_drop_ext(wanted, sizeof(wanted), wanted);
    for (unsigned int i = 0; i < Num_effect_types; ++i)
    {
        const unsigned int visual_handle = Effect_info[i].visual_handle;
        if (visual_handle < (unsigned int)Num_visual_effect_types)
        {
            cf_drop_path(visual_name, sizeof(visual_name), Visual_effect_info[visual_handle].vfx_filename);
            cf_drop_ext(visual_name, sizeof(visual_name), visual_name);
            if (v_stricmp(wanted, visual_name) == 0)
            {
                return i;
            }
        }
    }
    return 0xFFFFFFFF;
}

// 0x00747910 (:792)
unsigned int effects_play(unsigned int effect_handle, const vector& pos, const matrix& orient, bool looping, unsigned int host_handle, int host_tag, unsigned int source_handle, float effect_volume, bool attach_to_camera, object_effect_category category)
{
    if (effect_handle < Num_effect_types)
    {
        return effects_play(&Effect_info[effect_handle], pos, orient, looping, host_handle, host_tag, source_handle, effect_volume, attach_to_camera, category);
    }
    return 0xFFFFFFFF;
}

// 0x007458B0 (:817)
// Not on a dedicated server. The visual is left out (the sound still plays) when the effect's LOD rejects it: view
// dependent and not visible, or beyond its spawn distance from the camera; with the LOD passing (or off) the effect
// is refused when fewer than 20 parametric constant blocks are free. Then the effect object is created (not saved,
// destroyed when its zone streams out) with the effect's flags, info and category.
unsigned int effects_play(const effect_info* info, const vector& pos, const matrix& orient, bool looping, unsigned int host_handle, int host_tag, unsigned int source_handle, float effect_volume, bool attach_to_camera, object_effect_category category)
{
    if (Multiplayer && multi_is_dedicated_server())
    {
        return 0xFFFFFFFF;
    }
    if (info == nullptr)
    {
        return 0xFFFFFFFF;
    }
    unsigned int visual_handle = info->visual_handle;
    if (visual_handle < (unsigned int)Num_visual_effect_types)
    {
        const visual_effect_info& visual_info = Visual_effect_info[visual_handle];
        bool spawn = true;
        if (visual_info.m_lod.enabled && Effects_lod_enabled)
        {
            spawn = false;
            if (!visual_info.m_lod.spawn_view_dependent || game_render_is_object_visible(pos, visual_info.radius))
            {
                const float spawn_distance = visual_info.m_lod.spawn_distance;
                const vector to_camera(pos.x - Rfg_camera.real_pos.x, pos.y - Rfg_camera.real_pos.y, pos.z - Rfg_camera.real_pos.z);
                spawn = to_camera.mag_squared() <= spawn_distance * spawn_distance;
            }
        }
        if (spawn)
        {
            if (rl_renderable_particles_instance::get_parametric_constant_pool_available() < MIN_FREE_PARAMETRIC_CONSTANTS)
            {
                return 0xFFFFFFFF;
            }
            visual_handle = info->visual_handle;
        }
        else
        {
            visual_handle = 0xFFFFFFFF;
            if (info->sound_handle == -1 && info->sound_alr_handle == -1)
            {
                return 0xFFFFFFFF;
            }
        }
    }
    object_effect::props props(visual_handle, info->sound_handle, info->sound_alr_handle, pos, orient, looping, host_handle, host_tag, source_handle, effect_volume, attach_to_camera);
    object_effect* effect_p = World.create_object<object_effect>(props, 0xFFFFFFFF);
    if (effect_p == nullptr)
    {
        return 0xFFFFFFFF;
    }
    effect_p->obj_flags.no_save = 1;
    effect_p->obj_flags.destroy_on_stream = 1;
    effect_p->sound_effect.kill_oneshot_when_done = info->flags.sound_kill_oneshot_when_done;
    effect_p->sound_effect.follow_effect_pos = info->flags.sound_follows_effect_pos;
    effect_p->visual_effect.kill_particles_on_stop = info->flags.vfx_kill_particles_on_stop;
    effect_p->visual_effect.kill_particles_fade_time_s = info->vfx_kill_particles_fade_time_s;
    effect_p->stop_when_host_destroyed = info->flags.stop_when_host_destroyed;
    effect_p->set_effect_info(const_cast<effect_info*>(info));
    effect_p->set_category(category);
    return effect_p->get_handle();
}

// 0x00747970 (:920)
unsigned int effects_play(unsigned int effect_handle, unsigned int host_handle, int host_tag, bool looping, unsigned int source_handle, float effect_volume, bool attach_to_camera)
{
    const effect_info* info = effect_handle < Num_effect_types ? &Effect_info[effect_handle] : nullptr;
    return effects_play(info, host_handle, host_tag, looping, source_handle, effect_volume, attach_to_camera);
}

// 0x00745AF0 (:941)
// At the host's transform, or its tag's.
unsigned int effects_play(const effect_info* info, unsigned int host_handle, int host_tag, bool looping, unsigned int source_handle, float effect_volume, bool attach_to_camera)
{
    if (info == nullptr)
    {
        return 0xFFFFFFFF;
    }
    object* host_p = World.object_from_handle(host_handle);
    if (host_p == nullptr)
    {
        return 0xFFFFFFFF;
    }
    vector play_pos = host_p->pos;
    matrix play_orient(host_p->orient);
    if (host_tag != -1)
    {
        host_p->get_tag_world_coords(host_tag, play_pos, &play_orient, nullptr, nullptr);
    }
    return effects_play(info, play_pos, play_orient, looping, host_handle, host_tag, source_handle, effect_volume, attach_to_camera, EFFECTS_CATEGORY_NONE);
}

// 0x007479C0 (:959)
// An effect at a script point of a destroyable rfg_mover's chunk (or at the given position and orientation), held to
// the mover by a constraint (type 5): to the dlod "<sub piece>_parent" of the script point when that dlod is a
// single piece, else to the mover's body.
unsigned int effects_play_at_script_point(unsigned int effect_handle, unsigned int script_point_ref_handle, int script_point_index, bool looping, const vector* use_pos, const matrix* use_orient)
{
    if (script_point_ref_handle == 0xFFFFFFFF)
    {
        return 0xFFFFFFFF;
    }
    rfg_mover* mover_p = (rfg_mover*)World.object_from_handle(script_point_ref_handle);
    if (mover_p == nullptr || mover_p->get_obj_type() != OT_MOVER || mover_p->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return 0xFFFFFFFF;
    }
    c_chunk_base* chunk_p = chunk_get_base_by_uid(mover_p->chunk_uid);
    if (chunk_p == nullptr || script_point_index < 0 || chunk_p->num_script_points <= script_point_index)
    {
        return 0xFFFFFFFF;
    }
    const c_script_point& script_point = chunk_p->script_points[script_point_index];
    vector local_pos;
    matrix local_orient;
    script_point.transform.get_translation(local_pos);
    script_point.transform.get_orientation(local_orient);
    vector play_pos;
    if (use_pos == nullptr)
    {
        const matrix& m = mover_p->orient;
        const float uz = m.uvec.z * local_pos.y;
        play_pos.x = mover_p->pos.x + m.rvec.x * local_pos.x + local_pos.y * m.uvec.x + local_pos.z * m.fvec.x;
        play_pos.y = mover_p->pos.y + m.rvec.y * local_pos.x + m.uvec.y * local_pos.y + m.fvec.y * local_pos.z;
        play_pos.z = mover_p->pos.z + m.rvec.z * local_pos.x + uz + m.fvec.z * local_pos.z;
    }
    else
    {
        play_pos = *use_pos;
    }
    const matrix play_orient = use_orient == nullptr ? local_orient * mover_p->orient : *use_orient;
    const unsigned int effect_objh = effects_play(effect_handle, play_pos, play_orient, looping, script_point_ref_handle, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    if (effect_objh == 0xFFFFFFFF)
    {
        return 0xFFFFFFFF;
    }
    object* effect_p = World.object_from_handle(effect_objh);
    if (effect_p == nullptr || effect_p->get_obj_type() != OT_EFFECT)
    {
        return 0xFFFFFFFF;
    }
    constraint_template ct; // reset by its constructor (rfg.exe calls reset on the local)
    ct.type = 5; // the constraint type effects use to hang on movers
    ct.body_a_transform.m_translation.x = play_pos.x - effect_p->pos.x;
    ct.body_a_transform.m_translation.y = play_pos.y - effect_p->pos.y;
    ct.body_a_transform.m_translation.z = play_pos.z - effect_p->pos.z;
    ct.body_a_transform.m_translation.rotate(effect_p->orient);
    ct.body_b_transform.m_translation.x = play_pos.x - mover_p->pos.x;
    ct.body_b_transform.m_translation.y = play_pos.y - mover_p->pos.y;
    ct.body_b_transform.m_translation.z = play_pos.z - mover_p->pos.z;
    ct.body_b_transform.m_translation.rotate(mover_p->orient);
    int host_attach_index = -1;
    if (mover_p->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        rfg_destroyable_shape* shape_p = mover_p->get_shape();
        const char* sub_piece_name = script_point.sub_piece_name;
        if (shape_p != nullptr && sub_piece_name[0] != '\0')
        {
            char dlod_name[1024];
            sprintf(dlod_name, "%s_parent", sub_piece_name);
            const unsigned int dlod_index = shape_p->get_dlod_index_from_name(dlod_name);
            if (dlod_index != 0xFFFFFFFF)
            {
                const rfg_dlod_base& dlod = shape_p->parent->base_dlods.to_pointer()[dlod_index];
                if (dlod.max_pieces == 1)
                {
                    host_attach_index = dlod.first_piece;
                }
            }
        }
    }
    obj_constraint_props constraint_props(&ct, mover_p->get_handle(), effect_objh, play_pos, play_orient, host_attach_index, -1, -1, -1);
    World.create_object<constraint>(constraint_props, 0xFFFFFFFF);
    return effect_objh;
}

// 0x007428B0 (:1059)
void effects_play_radial_blur(const radial_blur_info* blur_info, const vector& pos)
{
    if (blur_info != nullptr)
    {
        fb_accum_blur_set_radial_blur(pos, blur_info->m_strength, blur_info->m_radius, blur_info->m_duration, blur_info->m_distance_fade);
    }
}

// 0x00747F50 (:1068)
// The console's effect replacement: destroys the previous one and plays this effect 10 m in front of the camera
// (volume 0). LTCG: rfg.exe passes the effect handle in EAX and looping on the stack.
static void effects_play_replace(unsigned int effect_h, bool looping)
{
    if (Effects_replace_objh != 0xFFFFFFFF)
    {
        World.destroy_object(Effects_replace_objh, false);
    }
    const vector play_pos(Rfg_camera.real_pos.x + Rfg_camera.real_orient.fvec.x * CONSOLE_EFFECT_DISTANCE, Rfg_camera.real_pos.y + Rfg_camera.real_orient.fvec.y * CONSOLE_EFFECT_DISTANCE, Rfg_camera.real_pos.z + Rfg_camera.real_orient.fvec.z * CONSOLE_EFFECT_DISTANCE);
    if (effect_h < Num_effect_types)
    {
        const unsigned int objh = effects_play(&Effect_info[effect_h], play_pos, IDENTITY_MATRIX, looping, 0xFFFFFFFF, -1, 0xFFFFFFFF, 0.0f, false, EFFECTS_CATEGORY_NONE);
        if (objh != 0xFFFFFFFF)
        {
            console_output("effect started", nullptr, false);
            Effects_replace_objh = objh;
            return;
        }
    }
    console_output("effect failed to play", nullptr, false);
}

// 0x00748090 (:1088)
// The console's replacement by template file name.
void effects_play_file_replace(const char* filename, bool looping)
{
    const unsigned int handle = effects_get_handle_from_vfx_filename(filename);
    if (handle != 0xFFFFFFFF)
    {
        char msg[256];
        sprintf(msg, "found effect file filename %s in entry name %s", filename, Effect_info[handle].name);
        console_output(msg, nullptr, false);
    }
    effects_play_replace(handle, looping);
}

// 0x00748120 (:1214)
// The "kill_all_effects" console command.
void unused_dcf_kill_all_effects()
{
    const int num = World.get_num_objects<object_effect>();
    for (int i = 0; i < num; ++i)
    {
        World.destroy_object(World.get_object<object_effect>(i), false);
    }
}

// 0x00748130 (:1293)
// The "play_last_effect" console command: the last effect named in the console 10 m in front of the local player,
// as an effect or else as a bare visual effect object.
void unused_dcf_play_last_effect()
{
    if (Multiplayer && multi_is_dedicated_server())
    {
        return;
    }
    if (multi_game_spectating())
    {
        return;
    }
    player* player_p = player_get_local_player();
    if (effect_last_played_console[0] == '\0')
    {
        return;
    }
    unsigned int handle = effects_get_handle(effect_last_played_console);
    const vector play_pos(player_p->pos.x + player_p->orient.fvec.x * CONSOLE_EFFECT_DISTANCE, player_p->pos.y + player_p->orient.fvec.y * CONSOLE_EFFECT_DISTANCE, player_p->pos.z + player_p->orient.fvec.z * CONSOLE_EFFECT_DISTANCE);
    if (handle < Num_effect_types && effects_play(&Effect_info[handle], play_pos, IDENTITY_MATRIX, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE) != 0xFFFFFFFF)
    {
        console_output("effect started", nullptr, false);
        console_output(effect_last_played_console, nullptr, false);
        return;
    }
    handle = effects_get_visual_effect_info_handle(effect_last_played_console);
    if (handle == 0xFFFFFFFF)
    {
        console_output("effect failed to play", nullptr, false);
        return;
    }
    object_effect::props props(handle, -1, -1, play_pos, IDENTITY_MATRIX, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false);
    if (World.create_object<object_effect>(props, 0xFFFFFFFF) == nullptr)
    {
        console_output("effect failed to play", nullptr, false);
    }
}

#endif // OPENRFG_EFFECTS
