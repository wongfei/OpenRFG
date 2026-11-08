// Original: volition/rfg/code/rfg.cpp
//
// Partial:
// - The stream2 Pegs callbacks (peg_load_wrapper, peg_unload_wrapper) and get_instanced_peg_name.
// - multiplayer.md MP1, the original game loop: do_game_main_loop_frame, rfg_do_frame, the second boot stage
//   (rfg_init_stage_2_prepare, rfg_init_stage_2_done, rfg_init_stage_2_done_2, stream2LoadInitFunction,
//   sim_scratch_system_init) with the helpers they call (gamestate_init, the localized packfiles, the effects and
//   sound header preloads, the bitmap materials, the standard file access and the table precache), the keen getters and
//   setters the multiplayer code reads.
// - multiplayer.md MP11, the render thread's frame: do_game_render_frame, rfg_update_resource_sizes (without the frame
//   buffer allocator's recreation) and rfg_set_simulationThreadCommandWriter, with the main and deferred renderers.
// - orig_main.md OM1: rfg_init_keen_memory_allocators with the keen allocators it creates and the mempool_init_alloc*
//   pool allocations.
// - orig_main.md OM3: the first boot stage and its loops (rfg_pre_initialize_game,
//   rfg_init_stage_1_*, rfg_init_stage_2_loop_*), the table precache, rfg_shutdown_game, the console callbacks, the
//   error viewer, the interaction parameter and display string helpers, and the globals rfg.cpp defines (the pools,
//   the stream2 allocators, the boot flags, the fonts), and the global operator new/delete (0x005D28A0-0x005D2930,
//   final_push.md step 4).
// Not ported: the memory tracking (empty in the retail build except the two callbacks), the peg and packfile reload
// helpers of the language switch, the minidump and the debug drawing.
//
// Build: the render library compiles only the Pegs callbacks (OPENRFG_RFG_PEG_CALLBACKS_ONLY); the rfg_loop library
// compiles the whole file.
//
// Notes (decompiled behaviour, kept as is):
// - rfg_init_stage_2_done is run on its own thread by keen::rfg::Application while the render thread keeps drawing the
//   boot screen; each block of systems is skipped once Game_boot_exit_flag is set, and the bail-out releases the
//   preload pools.
// - localized_packfile_init returns false (no packfile added) for an audio language outside the table.

#include "volition/rfg/code/rfg.h"

#include "keen/base/language.hpp"
#include "keen/base/tlsfmemoryallocator.hpp"
#include "keen/base/zonememoryallocator.hpp"
#include "keen/useraccount/useraccounttypes.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/camera/camera_satellite.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/code_effects/distortion_effect.h"
#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/code_effects/glare.h"
#include "volition/rfg/code/code_effects/glass.h"
#include "volition/rfg/code/code_effects/los_blocker.h"
#include "volition/rfg/code/code_effects/material_effects.h"
#include "volition/rfg/code/code_effects/material_preload_shared.h"
#include "volition/rfg/code/code_effects/tracers.h"
#include "volition/rfg/code/code_effects/xray_effect.h"
#include "volition/rfg/code/control/control.h"
#include "volition/rfg/code/cutscene/video_cutscene.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/destruction/rfg_load_balance.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/object_decal.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/game_render/fullscreen_effects/fb_accum_blur.h"
#include "volition/rfg/code/gameplay/air_bombing.h"
#include "volition/rfg/code/gameplay/gameplay.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_handshake.h"
#include "volition/rfg/code/gamewide/game_render_thread.h"
#include "volition/rfg/code/gamewide/game_save.h"
#include "volition/rfg/code/gamewide/game_stream2_register.h"
#include "volition/rfg/code/gamewide/game_time_of_day.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/gamestate_gameplay.h"
#include "volition/rfg/code/gamewide/gamestate_quick_pause.h"
#include "volition/rfg/code/gamewide/gamewide.h"
#include "volition/rfg/code/gamewide/preload.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/gamewide/verify_gamesave.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character.h"
#include "volition/rfg/code/geom/cloth_simulation.h"
#include "volition/rfg/code/geom/decal_info.h"
#include "volition/rfg/code/geom/foliage_mesh.h"
#include "volition/rfg/code/geom/game_rig.h"
#include "volition/rfg/code/geom/material.h"
#include "volition/rfg/code/geom/mesh_morph.h"
#include "volition/rfg/code/geom/static_mesh.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_memory.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/hud/fullscreen_map.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/hud/hud_ui_activity_briefing.h"
#include "volition/rfg/code/hud/hud_ui_activity_complete.h"
#include "volition/rfg/code/hud/hud_ui_district_liberation.h"
#include "volition/rfg/code/hud/hud_ui_handbook.h"
#include "volition/rfg/code/hud/hud_ui_mission_briefing.h"
#include "volition/rfg/code/hud/hud_ui_mission_complete.h"
#include "volition/rfg/code/hud/hud_ui_upgrades.h"
#include "volition/rfg/code/hud/hud_ui_weapon_cabinet.h"
#include "volition/rfg/code/hud/in_world_gps.h"
#include "volition/rfg/code/interface/ingame/interface.h"
#include "volition/rfg/code/interface/ingame/overlay.h"
#include "volition/rfg/code/interface/ingame/ovl_base.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/option_menus/sony_options.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_document_save_load.h"
#include "volition/rfg/code/interface/ui_document_stats.h"
#include "volition/rfg/code/level/rfg_chunk_shared.h"
#include "volition/rfg/code/level/rfg_stream_layout_shared.h"
#include "volition/rfg/code/level/rfg_terrainx.h"
#include "volition/rfg/code/level/rfg_zonex.h"
#include "volition/rfg/code/level/territory_shared.h"
#include "volition/rfg/code/level/undergrowth.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/objects/gameplay_properties.h"
#include "volition/rfg/code/objects/humans/human_fire.h"
#include "volition/rfg/code/objects/humans/human_heads_and_props.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"
#include "volition/rfg/code/objects/lod_properties.h"
#include "volition/rfg/code/objects/movers/mover_fader.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_debris.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_skybox.h"
#include "volition/rfg/code/objects/object_trigger_region.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/objects/salvage.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/os/dc_menu.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/scripting/action_script/action_script.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/scripting/objective_highlight.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/rfg/code/scripting/target_outline.h"
#include "volition/rfg/code/traffic/traffic.h"
#include "volition/rfg/code/util/special_allocators.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/rfg/code/video_player/video_player.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/rfg/code/weather/weather.h"
#include "volition/stream2/code/stream2.h"
#include "volition/stream2/code/stream2_container.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/input/descent_physics.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/os/perfmon.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/string_util.h"

#include <string.h>

#if !defined(OPENRFG_RFG_PEG_CALLBACKS_ONLY)
#include "game/sources/application.hpp"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/hkbasesystem.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/rfg/code/gamewide/game_options.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/interface/option_menus/option_menu_system.h"
#include "volition/vlib/code/cfile/packfile.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/os.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/util/fastxml.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <stdio.h>
#include <stdlib.h>
#endif
#if !defined(OPENRFG_RFG_PEG_CALLBACKS_ONLY)
#include <malloc.h>
#include <new>
#endif

#if !defined(OPENRFG_RFG_PEG_CALLBACKS_ONLY)
#include "keen/base/memorysystem.hpp"
#include "keen/gameframework/gameframework.hpp"
#endif

class rl_renderer;
enum game_options_performance_mode;

#if !defined(OPENRFG_RFG_PEG_CALLBACKS_ONLY)
// One game packfile of the boot list (Game_packfiles). Placement inferred (should_register is defined in rfg.cpp).
// size 0x8
struct packfile_info
{
    // preload && Game_preload, or registered && Use_packfiles; never for localized packfiles, which
    // localized_packfile_init adds per audio language as "<name>_<lang>.vpp_pc" (decompiled).
    bool should_register() const; // 0x005D1A30 (:469)

    const char* name; // 0x00
    unsigned char registered : 1; // 0x04:0
    unsigned char preload : 1; // 0x04:1
    unsigned char stage1_install : 1; // 0x04:2
    unsigned char stage2_install : 1; // 0x04:3
    unsigned char localized : 1; // 0x04:4
    unsigned char territory : 1; // 0x04:5
    unsigned char encrypted : 1; // 0x04:6
};

// ---- globals defined here (the others of rfg.h are still the test programs', see the file comment)
mempool_base* Table_preload_pool_ptr; // 0x01D59C8C
int Expire_game; // 0x01D59C2C
keen::SoundSystem::System* Rfg_soundSystem; // 0x01D59C70

// ---- file-static globals
static bool Game_booting = true; // 0x0163F11D, cleared by rfg_init_stage_2_done_2
static keen::UserAccountId Error_viewer_user; // 0x01D59C20
static bool Error_viewer_last_event_was_remote_game_session; // 0x01D59C66
static unit4::LocalGameSession* Local_game_sesion_ptr; // 0x01D59C74
static unsigned int Error_viewer_event_id; // 0x01D59C78
static unsigned int Error_view_currently_handled_event_id; // 0x01D59C7C
static unsigned int Error_viewer_reponse; // 0x01D59C80
static mempool_base* Soundheader_preload_pool_ptr; // 0x01D59C90
static mempool_base* Effects_preload_pool_ptr; // 0x01D59C94
static int Standard_enabled; // 0x01D59CB4
static mempool RFG_sim_scratch_pool; // 0x01D64C88 (:621)
// Boot packfile list (.rdata), in order: name, then the flag bits registered, preload, stage1_install,
// stage2_install, localized, territory, encrypted.
static const packfile_info Game_packfiles[21] = // 0x012C6F00
{
    { "/gamebuild\\effects_mp", 1, 1, 0, 1, 0, 0, 0 },
    { "/gamebuild\\items", 1, 1, 0, 1, 0, 0, 0 },
    { "/gamebuild\\items_mp", 1, 1, 0, 1, 0, 0, 0 },
    { "/gamebuild\\chunks", 1, 0, 0, 1, 0, 0, 0 },
    { "/gamebuild\\activities", 1, 0, 0, 1, 0, 0, 1 },
    { "/gamebuild\\cloth_sim", 1, 0, 0, 0, 0, 0, 0 },
    { "/gamebuild\\decals", 1, 0, 0, 1, 0, 0, 0 },
    { "/gamebuild\\humans", 1, 0, 0, 0, 0, 0, 0 },
    { "/gamebuild\\misc", 1, 0, 1, 0, 0, 0, 1 },
    { "/gamebuild\\missions", 1, 0, 0, 1, 0, 0, 1 },
    { "/gamebuild\\skybox", 1, 0, 0, 0, 0, 0, 0 },
    { "/gamebuild\\interface", 1, 0, 0, 1, 0, 0, 1 },
    { "/gamebuild\\personas", 1, 0, 0, 0, 1, 0, 0 },
    { "/gamebuild\\vehicles_r", 1, 0, 0, 0, 0, 0, 0 },
    { "/gamebuild\\sounds_r", 1, 0, 0, 0, 0, 0, 0 },
    { "/gamebuild\\voices", 1, 0, 0, 0, 1, 0, 0 },
    { "/gamebuild\\terr01_l0", 0, 0, 0, 1, 0, 1, 0 },
    { "/gamebuild\\terr01_l1", 0, 0, 0, 1, 0, 1, 0 },
    { "/gamebuild\\terr01_precache", 0, 0, 0, 1, 0, 1, 0 },
    { "/gamebuild\\zonescript_terr01", 0, 0, 0, 1, 0, 0, 1 },
    { "/gamebuild\\steam", 1, 0, 1, 0, 0, 0, 1 },
};
// The simulation thread's renderer (game_handshake_do_frame).
static rl_renderer s_simThreadRenderer; // 0x01D64CF0 (:450)
// Renderers: main thread (do_game_render_frame) and deferred (handshake_deferred_render).
static rl_renderer s_mainRenderer; // 0x01D60D20 (:449)
static rl_renderer s_deferredRenderer; // 0x01D64370 (:451)
static game_options_performance_mode s_perfMode; // 0x01D59CA8
static int resource_size_update_counter; // 0x01D59CAC, non-zero while render targets are resized
#endif

// ---- internal functions
static bool soundheader_preload_inited(); // 0x005D1D10 (:1542)
static void soundheader_preload_packfile_finish(); // 0x005D1D20 (:1547)
static void effects_preload_packfile_init(); // 0x005D4AF0 (:1563)
static bool effects_preload_inited(); // 0x005D1D60 (:1586)
static void effects_preload_packfile_finish(); // 0x005D4BE0 (:1591)
static void get_instanced_peg_name(char* new_name, int new_name_size, const char* fname, unsigned int srid); // 0x005D1E50 (:1979)
static void font_init(); // 0x005D1FF0 (:2016)
static void setup_bitmap_materials(); // 0x005D2050 (:2045)
static void run_preps(); // 0x005D20D0 (:2055)
static bool game_packfile_startup_add(const char* packfile_name, bool installed_packfile, bool encrypted); // 0x005D21C0 (:2191)
// Called from rfg_init_stage_1_prepare when Use_packfiles is set (always: the stage sets it) and from
// rfg_init_stage_1_done when it is not (decompiled). Adds every Game_packfiles entry whose should_register() condition
// holds (packfile_add "<name>.vpp_pc"), with a temporary standard search while Use_packfiles is set; the first failure
// stops the list ("Error can't find a packfile.").
static bool init_packfiles(); // 0x005D4DF0 (:2208)
static bool localized_packfile_init(); // 0x005D4F20 (:2266)
static bool localized_packfile_deinit(keen::LanguageId previous_language); // 0x005D2230 (:2309)
// Registers every gameseq state (gs_boot_*, gs_load_*, gamestate_gp_*, ...); called near the end of
// rfg_init_stage_2_done.
static void gamestate_init(); // 0x005D23A0 (:2713)

// 0x005D1E50 (:1979)
// The bmpman name of a streamed peg: the file name, or for a container with unique data (an instanced container) the
// name without its extension followed by the srid in hex (str_x2s without its "0x").
static void get_instanced_peg_name(char* new_name, int new_name_size, const char* fname, unsigned int srid)
{
    if (srid != 0 && stream2_container_data_is_unique(srid))
    {
        cf_get_ext(fname); // the extension is looked up and its length taken, but neither is used
        cf_drop_ext(new_name, new_name_size, fname);
        char srid_text[0x20];
        str_x2s(srid_text, 0x20, (void*)srid);
        strcat(new_name, srid_text + 2);
        return;
    }
    strcpy(new_name, fname); // unbounded in the original
}

// 0x005D1F10 (:2000)
bool peg_load_wrapper(const char* fname, unsigned int srid, unsigned char* cpu_preload, int cpu_size, unsigned char* gpu_preload, int gpu_size)
{
    char peg_name[256];
    get_instanced_peg_name(peg_name, 0x100, fname, srid);
    return bm_load_peg(peg_name, cpu_preload, (unsigned int)cpu_size, gpu_preload, (unsigned int)gpu_size, true);
}

// 0x005D1F90 (:2009)
void peg_unload_wrapper(const char* fname, unsigned int srid)
{
    char peg_name[256];
    get_instanced_peg_name(peg_name, 0x100, fname, srid);
    bm_unload_peg(peg_name);
}

#if !defined(OPENRFG_RFG_PEG_CALLBACKS_ONLY)

// The standard (loose file) search of the packfile build, counted (game_enable_standard_access inlined).
static void standard_access_enable()
{
    if (Use_packfiles)
    {
        Standard_enabled++;
        cf_add_search(CF_SEARCH_STANDARD, Disc_build ? CF_IO_MEDIA_DVD : CF_IO_MEDIA_HDD, true);
    }
}

static void standard_access_disable()
{
    if (Use_packfiles)
    {
        Standard_enabled--;
        cf_remove_search(CF_SEARCH_STANDARD, Disc_build ? CF_IO_MEDIA_DVD : CF_IO_MEDIA_HDD);
    }
}

// The bail-out of the boot stages once Game_boot_exit_flag is set: the precached tables, the sound headers and the
// effects preload packfile are released and the boot thread stops.
static void boot_exit_cleanup()
{
    if (Table_preload_pool_ptr != nullptr && Use_packfiles && xml_precaching_enabled())
    {
        xml_disable_precache();
        Table_preload_pool_ptr->clear();
        salloc_pregame_split_unlock((rfg_split_mempool*)Table_preload_pool_ptr);
    }
    if (Soundheader_preload_pool_ptr != nullptr && Use_packfiles && audiolib_wavebank_precaching_enabled())
    {
        audiolib_disable_wavebank_precache();
        Soundheader_preload_pool_ptr->clear();
        salloc_pregame_split_unlock((rfg_split_mempool*)Soundheader_preload_pool_ptr);
    }
    if (Effects_preload_pool_ptr != nullptr)
    {
        effects_preload_packfile_finish();
    }
    if (game_boot_thread_running())
    {
        game_boot_thread_stop();
    }
}

// 0x005D1CC0 (:1495)
void game_disable_table_precache()
{
    if (Use_packfiles && xml_precaching_enabled())
    {
        xml_disable_precache();
        Table_preload_pool_ptr->clear();
        salloc_pregame_split_unlock((rfg_split_mempool*)Table_preload_pool_ptr);
    }
}

// 0x005D1D00 (:1510)
bool game_table_precache_inited()
{
    return Table_preload_pool_ptr != nullptr;
}

// 0x005D1D10 (:1542)
static bool soundheader_preload_inited()
{
    return Soundheader_preload_pool_ptr != nullptr;
}

// 0x005D1D20 (:1547)
static void soundheader_preload_packfile_finish()
{
    if (Use_packfiles && audiolib_wavebank_precaching_enabled())
    {
        audiolib_disable_wavebank_precache();
        Soundheader_preload_pool_ptr->clear();
        salloc_pregame_split_unlock((rfg_split_mempool*)Soundheader_preload_pool_ptr);
    }
}

// 0x005D4AF0 (:1563)
// The effects packfile is read whole into a 16 MB pregame pool while the effects table loads (a temporary packfile);
// when that fails it is added as a normal packfile.
static void effects_preload_packfile_init()
{
    if (Use_packfiles)
    {
        standard_access_enable();
    }
    else if (!Game_preload)
    {
        return;
    }
    char filename[68];
    sprintf(filename, "%s.%s", "/gamebuild\\effects", "vpp_pc");
    // Magic number: 16 MB, the effects packfile.
    Effects_preload_pool_ptr = salloc_pregame_split_lock(0x1000000, 0);
    if (!packfile_temp_add(filename, Effects_preload_pool_ptr, false, false))
    {
        Effects_preload_pool_ptr->clear();
        salloc_pregame_split_unlock((rfg_split_mempool*)Effects_preload_pool_ptr);
        Effects_preload_pool_ptr = nullptr;
        packfile_add(filename, nullptr, false, false);
    }
    standard_access_disable();
}

// 0x005D1D60 (:1586)
static bool effects_preload_inited()
{
    return Effects_preload_pool_ptr != nullptr;
}

// 0x005D4BE0 (:1591)
// The temporary effects packfile is dropped and the packfile added normally.
static void effects_preload_packfile_finish()
{
    if (Use_packfiles)
    {
        standard_access_enable();
    }
    else if (!Game_preload)
    {
        return;
    }
    char filename[68];
    sprintf(filename, "%s.%s", "/gamebuild\\effects", "vpp_pc");
    if (Effects_preload_pool_ptr != nullptr)
    {
        packfile_clear_temp_packfiles();
        Effects_preload_pool_ptr->clear();
        salloc_pregame_split_unlock((rfg_split_mempool*)Effects_preload_pool_ptr);
        Effects_preload_pool_ptr = nullptr;
        packfile_add(filename, nullptr, false, false);
    }
    standard_access_disable();
}

// 0x005D2050 (:2045)
// Each bitmap's user data is its physical material: the one named by a two-letter "_xx" suffix before the extension,
// else the default material.
static void setup_bitmap_materials()
{
    for (int i = 0; i < Bm_bitmap_count; i++)
    {
        const char* name = Bm_bitmaps[i].filename_ptr;
        const size_t len = strcspn(name, ".");
        unsigned char material = Rfg_physical_material_default_index;
        if (len > 3 && name[len - 3] == '_')
        {
            char suffix[4];
            suffix[0] = name[len - 2];
            suffix[1] = name[len - 1];
            suffix[2] = '\0';
            material = rfg_get_physical_material_index_by_suffix(suffix);
        }
        Bm_bitmaps[i].user_data = material;
    }
}

// 0x005D21C0 (:2191)
static bool game_packfile_startup_add(const char* packfile_name, bool installed_packfile, bool encrypted)
{
    char filename[256];
    cf_add_ext(filename, 0x40, packfile_name, "vpp_pc", false);
    return packfile_add(filename, nullptr, installed_packfile, encrypted);
}

// 0x005D4F20 (:2266)
// The localized packfiles (personas, voices) of the audio language: "<name>_<language>.vpp_pc", lower case with '_'
// for '-', or "<name>.vpp_pc" when the localized one does not exist.
static bool localized_packfile_init()
{
    if (!Use_packfiles)
    {
        return true;
    }
    const keen::LanguageId language = localization_get_audio_language();
    // Magic number: 0x31, the number of languages.
    if ((int)language >= 0x31 || language == (keen::LanguageId)~(int)keen::LanguageId_En_En)
    {
        return false;
    }
    standard_access_enable();
    for (const packfile_info& info : Game_packfiles)
    {
        if (!info.localized)
        {
            continue;
        }
        char filename[256];
        keen::formatString(filename, 0x100, "%s_%s.%s", info.name, keen::getLanguageIdString(localization_get_audio_language()), "vpp_pc");
        keen::changeStringToLowercase(filename);
        keen::replaceStringCharacters(filename, '-', '_');
        if (!cf_exist(filename))
        {
            cf_add_ext(filename, 0x40, info.name, "vpp_pc", false);
        }
        if (!game_packfile_startup_add(filename, false, info.encrypted))
        {
            game_disable_standard_access();
            keen::debug::nativePrintArgs("Error can't find a packfile.");
            return false;
        }
    }
    game_disable_standard_access();
    return true;
}

// 0x005D2230 (:2309)
// Removes the localized packfiles of previous_language, last first. Original: the "<name>.vpp_pc" fallback of
// localized_packfile_init is not tried, so a packfile that fell back fails the removal (false).
static bool localized_packfile_deinit(keen::LanguageId previous_language)
{
    if (!Use_packfiles)
    {
        return true;
    }
    // Magic number: 0x31, the number of languages.
    if ((int)previous_language >= 0x31 || previous_language == (keen::LanguageId)~(int)keen::LanguageId_En_En)
    {
        return false;
    }
    for (int i = (int)(sizeof(Game_packfiles) / sizeof(Game_packfiles[0])) - 1; i >= 0; i--)
    {
        const packfile_info& info = Game_packfiles[i];
        if (!info.localized)
        {
            continue;
        }
        char filename[256];
        char name[256];
        keen::formatString(filename, 0x100, "%s_%s.%s", info.name, keen::getLanguageIdString(previous_language), "vpp_pc");
        keen::changeStringToLowercase(filename);
        keen::replaceStringCharacters(filename, '-', '_');
        cf_drop_path(name, 0x100, filename);
        if (!packfile_remove(name))
        {
            keen::debug::nativePrintArgs("Failed unloading localized packfile '%s'!\n", name);
            return false;
        }
    }
    return true;
}

// 0x005D50B0 (:2345)
// The effects packfile and the localized packfiles depend on the audio language: both are reloaded.
void rfg_reload_packfiles_for_audio_language_switch(keen::LanguageId previous_language)
{
    packfile_remove("/gamebuild\\effects.vpp_pc");
    localized_packfile_deinit(previous_language);
    localized_packfile_init();
    effects_preload_packfile_init();
    effects_preload_packfile_finish();
}

// 0x005D23A0 (:2713)
static void gamestate_init()
{
    main_menu_init();
    gamestate_gp_init();
    gamestate_quick_pause_init();
    fullscreen_map_init();
    hud_ui_mission_briefing_init();
    hud_ui_activity_briefing_init();
    hud_ui_mission_complete_init();
    hud_ui_activity_complete_init();
    sony_options_init();
    hud_ui_upgrades_init();
    hud_ui_handbook_init();
    ui_stats_init();
    hud_ui_district_liberation_init();
    hud_ui_weapon_cabinet_init();
    video_cutscene_system_init();
    verify_gamesave_system_init();
    hud_ui_save_load_init();
}

// 0x005D2400 (:2762)
// The simulation thread's scratch pool (128 KB from RFG_scratch_pool).
void sim_scratch_system_init()
{
    // Magic number: 128 KB.
    void* memory = RFG_scratch_pool.alloc(0x20000, 0);
    RFG_sim_scratch_pool.init(memory, 0x20000, "Sim Scratch Pool", 4, 0);
    memory_scratch_thread_init(&RFG_sim_scratch_pool);
}

// 0x005D2460 (:4328)
// The stream2 loading thread's start: its own Havok memory router (the thread builds Havok objects).
void stream2LoadInitFunction()
{
    hkMemoryRouter* memory_router = havok_create_thread_memory_router();
    hkBaseSystem::initThread(memory_router);
}

// 0x005D53E0 (:4334)
void rfg_init_stage_2_prepare()
{
    timer_get(TIMER_MILLISECONDS);
    Load_timer_indent += 2;
    stream2_system_init(nullptr);
    game_stream2_register_allocators();
    game_stream2_register_types(PLATFORM_PC);
    Load_timer_indent -= 2;
    if (Game_boot_exit_flag)
    {
        boot_exit_cleanup();
        return;
    }
    timer_get(TIMER_MILLISECONDS);
    Load_timer_indent += 2;
    havok_system_init();
    Load_timer_indent -= 2;
    stream2_container_create_loading_thread(stream2LoadInitFunction);
}

// The boot timers of rfg_init_stage_2_done: timer_get before each system with the indent raised.
#define BOOT_TIMED(call)               \
    do                                 \
    {                                  \
        timer_get(TIMER_MILLISECONDS); \
        Load_timer_indent += 2;        \
        call;                          \
        Load_timer_indent -= 2;        \
    } while (0)

// The systems of rfg_init_stage_2_done in blocks; each block runs only while the boot is not being abandoned.
static bool boot_stage_2_systems(keen::SoundSystem::System* pSoundSystem)
{
    BOOT_TIMED(localized_packfile_init());
    BOOT_TIMED(gamelog_system_init());
    BOOT_TIMED(game_save_system_init());
    BOOT_TIMED(tweak_table_init());
    BOOT_TIMED(World.restore_system_init());
    if (Game_boot_exit_flag)
    {
        return false;
    }
    BOOT_TIMED(dc_menu_system_init());
    timer_get(TIMER_MILLISECONDS);
    rand_regenerate((unsigned int)timer_get(TIMER_MICROSECONDS));
    if (Game_boot_exit_flag)
    {
        return false;
    }
    {
        // The always loaded bitmaps.
        char base[0x44];
        char filename[0x44];
        timer_get(TIMER_MILLISECONDS);
        Load_timer_indent += 2;
        cf_drop_ext(base, 0x40, "always_loaded");
        cf_add_ext(filename, 0x40, base, Peg_platform_ext[0], false); // the first entry (all are ".cpeg_pc")
        bm_load_peg(filename, &Permanent_mempool, true);
        Load_timer_indent -= 2;
    }
    BOOT_TIMED(gameaudio_system_init_stage_2(&Rfg_audio_scratch_keen_allocator, &Rfg_audio_bank_keen_allocator, pSoundSystem));
    if (Game_boot_exit_flag)
    {
        return false;
    }
    BOOT_TIMED(ui_init_stage_2());
    BOOT_TIMED(human_voice_system_init());
    BOOT_TIMED(character_system_init(-1));
    BOOT_TIMED(game_rig_system_init());
    BOOT_TIMED(static_mesh_system_init());
    BOOT_TIMED(foliage_mesh_system_init());
    if (Game_boot_exit_flag)
    {
        return false;
    }
    BOOT_TIMED(material_system_init());
    BOOT_TIMED(decal_info_system_init());
    BOOT_TIMED(object_effect_system_init(&Permanent_mempool));
    BOOT_TIMED(fb_accum_blur_system_init());
    BOOT_TIMED(mesh_morph_system_init());
    BOOT_TIMED(object_debris_system_init());
    if (Game_boot_exit_flag)
    {
        return false;
    }
    BOOT_TIMED(projectile_system_init());
    timer_get(TIMER_MILLISECONDS);
    Load_timer_indent += 2;
    if (Use_packfiles || Game_preload)
    {
        effects_preload_packfile_init();
    }
    effects_system_init();
    if (Use_packfiles || Game_preload)
    {
        effects_preload_packfile_finish();
    }
    Load_timer_indent -= 2;
    BOOT_TIMED(camera_system_init());
    BOOT_TIMED(rfg_materials_system_init(false, nullptr));
    BOOT_TIMED(object_system_init());
    BOOT_TIMED(weather_system_init());
    if (Game_boot_exit_flag)
    {
        return false;
    }
    BOOT_TIMED(undergrowth_system_init());
    BOOT_TIMED(dc_menu_system_init());
    if (Game_boot_exit_flag)
    {
        return false;
    }
    // Magic numbers: the free camera's descent physics (speed 10, acceleration 1, damping 0.1 and 0.1).
    BOOT_TIMED(descent_physics_init(10.0f, 1.0f, 0.1f, 0.1f); descent_physics_mouse_disable(); descent_physics_keyboard_disable(); descent_physics_auto_level_lock());
    BOOT_TIMED(control_system_init());
    BOOT_TIMED(interface_system_init());
    BOOT_TIMED(hud_system_init());
    BOOT_TIMED(overlay_system_init());
    BOOT_TIMED(tracers_system_init());
    BOOT_TIMED(glare_system_init());
    BOOT_TIMED(glass_system_init());
    BOOT_TIMED(material_preload_shared_system_init());
    BOOT_TIMED(xray_effect_system_init());
    BOOT_TIMED(distortion_effect_system_init());
    BOOT_TIMED(los_blocker_system_init());
    if (Game_boot_exit_flag)
    {
        return false;
    }
    BOOT_TIMED(explosion_system_init());
    BOOT_TIMED(material_effects_system_init());
    BOOT_TIMED(object_item_system_init());
    BOOT_TIMED(salvage_system_init());
    BOOT_TIMED(weapons_system_init());
    if (Game_boot_exit_flag)
    {
        return false;
    }
    BOOT_TIMED(air_bomb_init());
    BOOT_TIMED(camera_satellite_system_init());
    BOOT_TIMED(human_fire_system_init());
    BOOT_TIMED(vehicle_ai_system_init());
    BOOT_TIMED(traffic_system_init());
    BOOT_TIMED(rfg_pf_system_init());
    if (Game_boot_exit_flag)
    {
        return false;
    }
    BOOT_TIMED(multi_game_system_init(); multi_game_mode_system_init(); multi_game_stats_system_init());
    BOOT_TIMED(script_system_init());
    BOOT_TIMED(gem_system_init());
    BOOT_TIMED(rfg_dyn_system_init());
    BOOT_TIMED(object_mover_system_init());
    BOOT_TIMED(rfg_shape_system_init());
    if (Game_boot_exit_flag)
    {
        return false;
    }
    BOOT_TIMED(lopm_system_init(&Permanent_mempool, -1));
    BOOT_TIMED(lpm_system_init(&Permanent_mempool));
    BOOT_TIMED(gpm_system_init(&Permanent_mempool, -1));
    BOOT_TIMED(object_decal_system_init());
    if (Game_boot_exit_flag)
    {
        return false;
    }
    BOOT_TIMED(setup_bitmap_materials());
    BOOT_TIMED(human_hap_system_init(); human_info_system_init());
    BOOT_TIMED(turret_system_init());
    BOOT_TIMED(trigger_region_system_init());
    if (Game_boot_exit_flag)
    {
        return false;
    }
    BOOT_TIMED(gamewide_system_init());
    BOOT_TIMED(nav_cell_system_init());
    BOOT_TIMED(game_clock_init());
    timer_get(TIMER_MILLISECONDS);
    Load_timer_indent += 2;
    if (!Use_packfiles && !Game_preload)
    {
        preload_all(-1);
    }
    else
    {
        char anims[0x44];
        sprintf(anims, "%s.%s", "/gamebuild\\anims", "vpp_pc");
        anim_load_packfile(anims);
    }
    cloth_sim_system_init(-1);
    Load_timer_indent -= 2;
    if (Game_boot_exit_flag)
    {
        return false;
    }
    BOOT_TIMED(vehicle_system_init());
    BOOT_TIMED(player_system_init());
    BOOT_TIMED(rfgl_system_init());
    BOOT_TIMED(chunk_system_init());
    BOOT_TIMED(terrain_system_init());
    BOOT_TIMED(zone_system_init());
    if (Game_boot_exit_flag)
    {
        return false;
    }
    BOOT_TIMED(gameplay_system_init());
    BOOT_TIMED(world_system_init());
    BOOT_TIMED(game_time_of_day_system_init());
    BOOT_TIMED(object_skybox_system_init());
    BOOT_TIMED(public_address_kiosk_system_init());
    if (Game_boot_exit_flag)
    {
        return false;
    }
    BOOT_TIMED(gamestate_init());
    BOOT_TIMED(mover_fader_system_init());
    BOOT_TIMED(target_outline_system_init());
    BOOT_TIMED(objective_highlight_system_init());
    BOOT_TIMED(mission_system_init());
    BOOT_TIMED(stream_layout_system_init(&Common_mempool, &Permanent_mempool, PLATFORM_PC, -1, nullptr));
    BOOT_TIMED(multi_game_backpack_system_init(Permanent_mempool));
    if (Game_boot_exit_flag)
    {
        return false;
    }
    BOOT_TIMED(stats_system_init(-1));
    BOOT_TIMED(wrecking_crew_system_init());
    BOOT_TIMED(havok_ragdoll_system_init());
    BOOT_TIMED(territory_shared_system_init(&Permanent_mempool, -1, nullptr));
    BOOT_TIMED(in_world_gps_system_init());
    if (Use_packfiles)
    {
        game_disable_table_precache();
    }
    return !Game_boot_exit_flag;
}

// 0x005D56A0 (:4418)
void rfg_init_stage_2_done(keen::SoundSystem::System* pSoundSystem)
{
    // The boot thread's own Havok memory router (the systems create Havok objects).
    hkMemoryRouter* memory_router = havok_create_thread_memory_router();
    hkBaseSystem::initThread(memory_router);
    if (boot_stage_2_systems(pSoundSystem))
    {
        console_run_script("gamestart.txt");
        havok_destroy_thread_memory_router(memory_router);
        return;
    }
    // Abandoned: the boot exit cleanup (with the table and sound header pools released when they exist).
    if (Table_preload_pool_ptr != nullptr)
    {
        game_disable_table_precache();
    }
    if (Soundheader_preload_pool_ptr != nullptr)
    {
        soundheader_preload_packfile_finish();
    }
    if (Effects_preload_pool_ptr != nullptr)
    {
        effects_preload_packfile_finish();
    }
    if (game_boot_thread_running())
    {
        game_boot_thread_stop();
    }
}

// 0x005D2480 (:5200)
void rfg_init_stage_2_done_2()
{
    game_boot_thread_stop();
    game_render_thread_init();
    cf_open_lock(true);
    Game_booting = false;
    srand((unsigned int)timer_get(TIMER_MICROSECONDS));
    gameseq_set_state(GS_MAINMENU, false);
    Permanent_mempool.lock();
    // GS_SHUTDOWN: a state without callbacks.
    state_info callbacks;
    gameseq_register_state(GS_SHUTDOWN, callbacks);
}

// 0x005D2500 (:5273)
bool rfg_do_frame()
{
    // While the window is in the background the controller vibration is off.
    static bool Lost_focus; // 0x01D651F1
    static bool Vibe_state; // 0x01D651F0
    // Magic number: an expiring build ends after 200 frames.
    if (Expire_game != 0 && Framecount > 200)
    {
        gamewide_shutdown();
        _exit(1);
    }
    game_check_for_gameplay_shutdown();
    if (os_foreground())
    {
        if (Lost_focus)
        {
            Lost_focus = false;
            input_set_vibration_unwanted(Vibe_state);
        }
    }
    else
    {
        mouse_update_visibility();
        if (!Lost_focus)
        {
            Vibe_state = input_get_vibration_unwanted();
            input_set_vibration_unwanted(false);
            Lost_focus = true;
        }
    }
    game_input_poll();
    frametime_calculate(false, 0.0f, 0.0f);
    const game_state state = gameseq_get_state();
    if (state != GS_MULTI_INIT && gameseq_get_state() != GS_MULTI_SHUTDOWN)
    {
        multi_process(game_booted_to_main_menu());
    }
    game_save_process();
    gamelog_process();
    // A state change may destroy what the render thread draws: it waits meanwhile.
    if (gameseq_might_process_exit())
    {
        keen::rfg::lock_render();
    }
    gameseq_process(GS_STEP_PROCESS, nullptr, false);
    if (keen::rfg::is_render_locked())
    {
        keen::rfg::unlock_render();
    }
    video_player_process_all();
    game_handshake_copy_gameseq(Sim_snapshot.state_stack, Sim_snapshot.stack_top_index);
    mouse_flush();
    game_handshake_do_frame(s_simThreadRenderer);
    localization_try_finish_audio_language_change();
    return gameseq_get_state() == GS_SHUTDOWN;
}

// 0x005D2700 (:5461)
bool do_game_main_loop_frame()
{
    if (perfmon_needs_pause() && !game_is_paused())
    {
        game_pause(true);
    }
    rand(); // one draw of the CRT generator per frame
    return rfg_do_frame();
}

// 0x005D2680 (:5431)
// The render thread's frame (keen::rfg::Application::render, after the simulation frame's handshake): the scene is
// drawn only from a gameplay snapshot and while no render target resize is pending; the deferred renderer (the
// handshake's state render steps) is flipped with the main one.
void do_game_render_frame(keen::GraphicsCommandWriter* pCommandWriter)
{
    if (gameseq_get_state() == GS_RESOLUTION_CHANGE)
    {
        resolution_change_render_thread();
    }
    s_mainRenderer.begin(pCommandWriter);
    game_render_begin_frame();
    const bool render_scene = Render_snapshot.in_gameplay && resource_size_update_counter == 0;
    game_render_do_frame(s_mainRenderer, render_scene, s_deferredRenderer);
    game_render_end_frame();
    s_mainRenderer.end();
    s_mainRenderer.flip();
    s_deferredRenderer.flip();
}

// 0x005D95B0 (:4107)
// Run first by every render frame (keen::rfg::Application::render): a performance mode change recreates the render
// targets three frames later; otherwise a pending level of detail or screen size change is applied
// (check_device_status) and the size dependent targets are updated.
// Not ported: the recreation of the frame buffer allocator on a performance mode change (Rfg_frameBufferAllocator
// over s_rfgFrameBufferMemoryBlock, keen's first stage); the programs that run this have no game options, so the
// performance mode never changes.
void rfg_update_resource_sizes(keen::GraphicsCommandWriter* pGraphicsCommandWriter)
{
    bool force = false;
    const game_options_performance_mode mode = game_options_get_performance_mode();
    if (mode != s_perfMode)
    {
        // Magic number: the targets are recreated on the third frame after the change.
        resource_size_update_counter = 3;
        s_perfMode = mode;
    }
    else if (resource_size_update_counter < 1)
    {
        goto update;
    }
    resource_size_update_counter--;
    if (resource_size_update_counter != 0)
    {
        return;
    }
    force = true;
update:
    if (resource_size_update_counter == 0)
    {
        if (rl_is_initialized())
        {
            rl_render_lib::get_instance()->check_device_status();
        }
        s_mainRenderer.updateResourceSizes(pGraphicsCommandWriter, force, mode == GOPM_HIGH_QUALITY);
    }
}

// 0x005D2DF0 (:6703)
// keen::rfg::Application::render hands the simulation thread its command writer for the frame (the handshake's render
// setup records into it) and takes it back with null before do_game_render_frame.
void rfg_set_simulationThreadCommandWriter(keen::GraphicsCommandWriter* pCommandWriter)
{
    if (pCommandWriter != nullptr)
    {
        s_simThreadRenderer.begin(pCommandWriter);
        return;
    }
    s_simThreadRenderer.end();
}

// 0x005D2730 (:5501)
void game_enable_standard_access()
{
    standard_access_enable();
}

// 0x005D2760 (:5527)
void game_disable_standard_access()
{
    standard_access_disable();
}

// 0x005D2B30 (:6479)
// Undoes the escaping of player names: the private use characters U+FDD0-U+FDD4 back to '%', '[', '\', '{' and ' '.
void rfg_reverse_escape_characters(wchar_t* pString)
{
    for (; *pString != L'\0'; pString++)
    {
        switch (*pString)
        {
        case 0xFDD0:
            *pString = L'%';
            break;
        case 0xFDD1:
            *pString = L'[';
            break;
        case 0xFDD2:
            *pString = L'\\';
            break;
        case 0xFDD3:
            *pString = L'{';
            break;
        case 0xFDD4:
            *pString = L' ';
            break;
        }
    }
}

// 0x005D2D00 (:6568)
unsigned int rfg_get_error_viewer_response()
{
    return Error_viewer_reponse;
}

// 0x005D2D10 (:6573)
unsigned int rfg_get_currently_handled_error_viewer_event_id()
{
    return Error_view_currently_handled_event_id;
}

// 0x005D2D20 (:6580)
void rfg_set_local_game_session(unit4::LocalGameSession* pGameSession)
{
    Local_game_sesion_ptr = pGameSession;
}

// 0x005D2D30 (:6585)
unit4::LocalGameSession* rfg_get_local_game_session()
{
    return Local_game_sesion_ptr;
}

// 0x005D2D60 (:6653)
keen::MemoryAllocator* rfg_get_cpu_scratch_allocator()
{
    return Rfg_cpu_scratch_keen_allocator;
}

// 0x005D2D70 (:6658)
keen::MemoryAllocator* rfg_get_gpu_scratch_allocator()
{
    return &Rfg_gpu_scratch_keen_allocator;
}

// 0x005D2DE0 (:6698)
void rfg_set_SoundSystem(keen::SoundSystem::System* pSoundSystem)
{
    Rfg_soundSystem = pSoundSystem;
}

#endif // !OPENRFG_RFG_PEG_CALLBACKS_ONLY

#if !defined(OPENRFG_RFG_PEG_CALLBACKS_ONLY)
// ---- orig_main.md OM1: the keen allocators over the memory system's blocks and the pool allocations of the first boot
// stage.

keen::GameFrameworkSystem* Rfg_gameframework_system; // 0x01D59C6C
keen::MemoryAllocator* Rfg_cpu_scratch_keen_allocator; // 0x01D59C68
keen::TlsfMemoryAllocator Rfg_vram_mempool_provider_keen_allocator; // 0x01640738
keen::ZoneMemoryAllocator Rfg_mempool_provider_keen_allocator; // 0x01640818
keen::ZoneMemoryAllocator Rfg_gpu_scratch_keen_allocator; // 0x01640900
keen::ZoneMemoryAllocator Rfg_frameBufferAllocator; // 0x016409E8
keen::TlsfMemoryAllocator Rfg_audio_bank_keen_allocator; // 0x01640AD0
keen::TlsfMemoryAllocator Rfg_audio_scratch_keen_allocator; // 0x01640BB0
keen::MemoryBlock s_d3dMemoryBlock; // 0x01D59400
unsigned char* pStream_audio_slot_allocator_memory; // 0x01D59C88
static keen::MemoryBlock s_levelMemoryBlock; // 0x01D58CE0, the level memory manager's block (rfg_init_stage_1_prepare)
static keen::MemoryBlock s_rfgFrameBufferMemoryBlock; // 0x01D58FF4
static keen::MemoryBlock s_deferredCommandBufferMemoryBlock; // 0x01D59C18, stays invalid on PC

// 0x005D96B0 (:6595)
// Locks the memory system's RFG blocks (generated_sources/memorylayoutdefinition.cpp) and puts the keen allocators over
// them. Creation flags: 0x55 for the VRAM provider, 7 for the others and 4 for the frame buffers (whose block is empty
// in rfg.exe's layout, so that create fails and the allocator stays unusable until rfg_update_resource_sizes recreates
// it). Block 0 (RFG_SysMempoolProvider) and block 4 (RFG_Graphics) are not locked here.
void rfg_init_keen_memory_allocators(keen::GameFrameworkSystem* pGameFramework)
{
    Rfg_gameframework_system = pGameFramework;
    Rfg_cpu_scratch_keen_allocator = keen::GameFramework::getDefaultAllocator(pGameFramework);
    keen::MemorySystem* pMemorySystem = keen::GameFramework::getMemorySystem(pGameFramework);
    const keen::MemoryBlock vramBlock = keen::memory::lockMemoryBlock(pMemorySystem, 1);
    const keen::MemoryBlock mempoolBlock = keen::memory::lockMemoryBlock(pMemorySystem, 2);
    const keen::MemoryBlock gpuScratchBlock = keen::memory::lockMemoryBlock(pMemorySystem, 3);
    s_rfgFrameBufferMemoryBlock = keen::memory::lockMemoryBlock(pMemorySystem, 5);
    Rfg_vram_mempool_provider_keen_allocator.create(vramBlock, 0x55, "RFG_VramMempoolProvider");
    Rfg_mempool_provider_keen_allocator.create(mempoolBlock, 7, "RFG_MempoolProvider");
    Rfg_gpu_scratch_keen_allocator.create(gpuScratchBlock, 7, "RFG_GpuScratchMem");
    Rfg_frameBufferAllocator.create(s_rfgFrameBufferMemoryBlock, 4, "RFG_FrameBuffers");
    s_levelMemoryBlock = keen::memory::lockMemoryBlock(pMemorySystem, 6);
    s_d3dMemoryBlock = keen::memory::lockMemoryBlock(pMemorySystem, 7);
    Rfg_audio_scratch_keen_allocator.create(keen::memory::lockMemoryBlock(pMemorySystem, 9), 7, "RFG_Audio");
    Rfg_audio_bank_keen_allocator.create(keen::memory::lockMemoryBlock(pMemorySystem, 8), 7, "RFG_Audio_Bank");
    s_deferredCommandBufferMemoryBlock.pStart = nullptr;
    s_deferredCommandBufferMemoryBlock.size = 0;
}

// 0x005D8590 (:2132)
// The vlib pools of the first boot stage come from the mempool provider (a zone: never freed), 16 byte aligned.
unsigned char* mempool_init_alloc(int bytes)
{
    return (unsigned char*)Rfg_mempool_provider_keen_allocator.allocate(bytes, 0x10, 0, nullptr);
}

// 0x005D85B0 (:2137)
unsigned char* mempool_init_alloc_aligned(int bytes, int align)
{
    return (unsigned char*)Rfg_mempool_provider_keen_allocator.allocate(bytes, align, 0, nullptr);
}

// 0x005D85D0 (:2142)
// Magic number: VRAM pools are 4 KB aligned.
unsigned char* vram_mempool_init_alloc(int bytes)
{
    return (unsigned char*)Rfg_vram_mempool_provider_keen_allocator.allocate(bytes, 0x1000, 0, nullptr);
}

// 0x005D85F0 (:2147)
unsigned char* vram_mempool_init_alloc_aligned(int bytes, int align)
{
    return (unsigned char*)Rfg_vram_mempool_provider_keen_allocator.allocate(bytes, align, 0, nullptr);
}

// 0x005D8610 (:2152)
// The audio slot allocator's memory, from the audio bank allocator; the address is also kept in
// pStream_audio_slot_allocator_memory.
unsigned char* audio_mempool_init_alloc_aligned(int bytes, int align)
{
    pStream_audio_slot_allocator_memory = (unsigned char*)Rfg_audio_bank_keen_allocator.allocate(bytes, align, 0, nullptr);
    return pStream_audio_slot_allocator_memory;
}
#endif // !OPENRFG_RFG_PEG_CALLBACKS_ONLY

#if !defined(OPENRFG_RFG_PEG_CALLBACKS_ONLY)
// ---- orig_main.md OM3: the first boot stage, its loops, the shutdown and the globals rfg.cpp defines for them. The
// test programs do not link rfg_loop: they define these globals themselves (src/stubs/rfg/rfg.cpp,
// src/stubs/rfg/rfg_multi.cpp) and stand in for the first stage.
//
// Deviations (orig_main.md section 3):
// - 5: -allowremote skips the remote desktop refusal of rfg_pre_initialize_game (off by default).
// Notes (decompiled behaviour, kept as is):
// - rfg_init_stage_1_prepare registers the packfiles (init_packfiles) when Use_packfiles is set, which it sets itself;
//   rfg_init_stage_1_done calls init_packfiles again only when Use_packfiles is clear (never in this build).
// - Every step is skipped once Game_boot_exit_flag is set; the bail-out releases the preload pools and stops the boot
//   screen thread (boot_exit_cleanup; the original repeats the same code at each place).
// - rfg_get_num_tracked_chunk_ids returns 12 for an array of three ids.

#include "keen/base/crc32.hpp"
#include "keen/base/platformtype.hpp"
#include "keen/base/stringencoding.hpp"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/gr/gr_font.h"
#include "volition/render_lib/code/rl_compiled_render_group.h"
#include "volition/render_lib/code/rl_draw.h"
#include "volition/render_lib/code/rl_texture.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/interface/ui_startup_video.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_matchmaking_generic.h"
#include "volition/rfg/code/os/dlmalloc/dmalloc.h"
#include "volition/rfg/code/util/special_allocators.h"
#include "volition/stream2/code/stream2_allocator.h"
#include "volition/stream2/code/stream2_os.h"
#include "volition/vlib/code/cfile/vdir.h"
#include "volition/vlib/code/input/key.h"
#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/memory/page_mempool.h"
#include "volition/vlib/code/memory/page_pool.h"
#include "volition/vlib/code/memory/split_memmgr.h"
#include "volition/vlib/code/os/cmdline.h"
#include "volition/vlib/code/os/scheduler.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/zlib_vlib.h"
#include "volition/vlib/code/vlib_core/vlib_core.h"

#include <io.h>
#include <windows.h>

// ---- globals
bool Use_packfiles; // 0x01D59C55
bool Disc_build; // 0x01D59C56
bool Game_preload; // 0x01D59C57
bool Game_boot_exit_flag; // 0x01D59C64
bool Modified_table_data; // 0x01D59C65
bool Demo = true; // 0x0163F11E, .data (read by nothing that is ported)
int Load_timer_indent; // 0x01D59C84
volatile bool Show_language_selection; // 0x01D59C67
volatile bool Profiles_read; // 0x01D59C98
volatile bool Boot_screen_doc_loaded; // 0x01D59C99
keen::LanguageId User_selected_language = (keen::LanguageId)~(int)keen::LanguageId_En_En; // 0x0163F184, none yet
unsigned int Lparam_super_overbrighten = 255; // 0x0163F178
// The font handles (font_init, ui_initialize_fonts); -1 until the fonts load.
int General_font = -1; // 0x0163F154
int Foreign_font = -1; // 0x0163F158
int Header_font = -1; // 0x0163F15C
int Body_font = -1; // 0x0163F160
int Number_font = -1; // 0x0163F164
int Latin_num_font = -1; // 0x0163F168
int Latin_header_font = -1; // 0x0163F16C
int Credits_body_font = -1; // 0x0163F170
int Credits_header_font = -1; // 0x0163F174
keen::TlsfMemoryAllocator Rfg_sys_mempool_provider_keen_allocator; // 0x01640658, never created on PC (destroyed at shutdown)
// The pools of the first boot stage.
rfg_split_mempool Permanent_mempool; // 0x01D64B20
rfg_split_mempool Interface_mempool; // 0x01D59CB8
rfg_split_mempool Interface_perm_mempool; // 0x01D60C78
rfg_split_mempool Interface_font_mempool; // 0x01D60A80
rfg_split_mempool Effect_preload_mempool; // 0x01D64BC8
rfg_split_mempool Item_preload_pool; // 0x01D5A188
rfg_split_mempool Decal_preload_mempool; // 0x01D5A080
rfg_split_mempool Chunk_preload_pool; // 0x01D5D3D0
rfg_split_mempool Animation_mempool; // 0x01D5D500
rfg_split_mempool Time_of_day_mempool; // 0x01D60828
virtual_mempool Common_mempool; // 0x01D5A128
// Nothing in rfg.exe initializes this pool: only its constructor runs, and only the real-time cutscene code, which is
// dead, reads it.
virtual_mempool Cutscene_mempool; // 0x01D649C8
mempool Anim_blend_tree_mempool; // 0x01D59D70
mempool Vint_perm_interface_mempool; // 0x01D59E80
mempool Remote_file_cache; // 0x01D59F78
mempool Havok_mempool; // 0x01D59FC8
mempool DL_Mempool; // 0x01D5A030
mempool World_state_mempool; // 0x01D5D5A8
mempool Level_mempool; // 0x01D608D0
mempool Audio_mempool; // 0x01D60920
mempool RFG_scratch_pool; // 0x01D60A18
mempool Human_voice_always_loaded_pool; // 0x01D60C28
mempool Script_mempool; // 0x01D64870
mempool Rig_mempool; // 0x01D648C0
mempool Level_memmgr_header_pool; // 0x01D64910
mempool Cloth_sim_mempool; // 0x01D64978
mempool Localization_mempool; // 0x01D64AD0
page_mempool Undergrowth_mempool; // 0x01D60760
page_mempool Cloth_sim_runtime_mempool; // 0x01D607D0
page_mempool Material_effects_mempool; // 0x01D61220
// The multiplayer pools (multi_core_memory_alloc and multi_game_memory_alloc set them up).
rfg_split_mempool MP_allocators_mempool; // 0x01D5D328
rfg_split_mempool MP_multi_purpose_mempool; // 0x01D59ED0
rfg_split_mempool MP_effect_preload_mempool; // 0x01D59DC0
rfg_split_mempool MP_item_preload_mempool; // 0x01D60970
page_pool Common_page_pool; // 0x01D5A230
split_memmgr Level_memmgr("level_memmgr"); // 0x01D60B28
mempool_base* g_pCommonMempool = &Common_mempool; // 0x0163F17C
mempool_base* g_pPermanentMempool = &Permanent_mempool; // 0x0163F180
// The stream2 allocators (game_stream2_register_allocators registers them).
stream2_dynamic_allocator Stream_world_allocator; // 0x01D59D5C
stream2_mempool_allocator Stream_chunk_preload_allocator; // 0x01640C90
stream2_mempool_allocator Stream_effect_preload_allocator; // 0x01640C9C
stream2_mempool_allocator Stream_effect_cutscene_allocator; // 0x01640CA8
stream2_mempool_allocator Stream_item_preload_allocator; // 0x01640CB4
stream2_mempool_allocator Stream_decal_preload_allocator; // 0x01640CC0
stream2_mempool_allocator Stream_cloth_sim_preload_allocator; // 0x01640CCC
stream2_mempool_allocator Stream_tod_allocator; // 0x01640CD8
stream2_mempool_allocator Stream_mp_effect_preload_allocator; // 0x01640CE4
stream2_mempool_allocator Stream_mp_item_preload_allocator; // 0x01640CF0
stream2_slot_allocator Stream_player_allocator; // 0x01640CFC
stream2_slot_allocator Stream_human_allocator; // 0x01640D20
stream2_slot_allocator Stream_large_weapon_allocator; // 0x01640D44
stream2_slot_allocator Stream_small_weapon_allocator; // 0x01640D68
stream2_slot_allocator Stream_vehicle_allocator; // 0x01640D8C
stream2_slot_allocator Stream_large_layer_allocator; // 0x01640DB0
stream2_slot_allocator Stream_small_layer_allocator; // 0x01640DD4
stream2_slot_allocator Stream_human_voice_persona_allocator; // 0x01640DF8
stream2_mempool_allocator Stream_always_loaded_human_voice_persona_allocator; // 0x01640E1C
stream2_mempool_allocator Stream_material_effects_allocator; // 0x01640E28
stream2_mempool_allocator Stream_permanent_allocator; // 0x01640E34
stream2_mempool_allocator Stream_dlc_effect_preload_allocator; // 0x01640E40
stream2_mempool_allocator Stream_dlc_item_preload_allocator; // 0x01640E4C
stream2_slot_allocator Stream_audio_slot_allocator; // 0x01640E58

// ---- file-static globals
static bool Boot_language_select = true; // 0x0163F11F, .data
static rfg_split_mempool Special_reserve; // 0x01D64A28 (:644)
// The console string conversion buffers of game_draw_console_string.
static char console_string_utf8_buffer[1024]; // 0x01D59000
static wchar_t console_string_draw_buffer[1024]; // 0x01D584E0
static const unsigned int s_trackedChunkIds[3] = {0, 0, 0}; // 0x012C6FA8
// The render command line options (final_push.md step 5; file statics, inferred: rfg.pdb has no external name for
// them). Their dynamic initializers (0x01061620..0x010616E0, between Common_page_pool's and Memtrack_last's) link them
// into cmdline.cpp's parameter list; nothing in rfg.exe reads them (cmdline_init and the readers have no callers), so
// -w, -h, -nod3d9, -nod3d11 and -display do nothing in retail.
static cmdline_param Force_w("-w", "", true); // 0x01D5A018
static cmdline_param Force_h("-h", "", true); // 0x01D59E64
static cmdline_param Disable_d3d9_arg("-nod3d9", "", false); // 0x01D64960
static cmdline_param Disable_d3d10_arg("-nod3d11", "", false); // 0x01D64C6C
static cmdline_param Force_monitor("-display", "", true); // 0x01D64CD8

// 0x005D1DB0 (:1866)
// The memory tracking of the render library's initialization: nothing in the retail build.
void game_track_mem_usage_start(const char*)
{
}

// 0x005D1DD0 (:1905)
void game_track_mem_usage_end(bool)
{
}

// 0x005D20D0 (:2055)
// The loose-file build (Use_packfiles clear) finds its working directory from rfg.exe or the data folder.
static void run_preps()
{
    _finddata64i32_t data;
    if (_findfirst64i32("rfg.exe", &data) < 0)
    {
        SetCurrentDirectoryW(L"../");
        if (_findfirst64i32("rfg.exe", &data) < 0)
        {
            SetCurrentDirectoryW(L"projects/rfg/main");
        }
    }
    if (_findfirst64i32("./data", &data) < 0)
    {
        SetCurrentDirectoryW(L"projects/rfg/main");
    }
    Game_preload = false;
}

// 0x005D4DF0 (:2208)
static bool init_packfiles()
{
    standard_access_enable();
    for (const packfile_info& info : Game_packfiles)
    {
        if (!info.should_register())
        {
            continue;
        }
        char filename[256];
        cf_add_ext(filename, 0x40, info.name, "vpp_pc", false);
        if (!packfile_add(filename, nullptr, false, info.encrypted))
        {
            standard_access_disable();
            keen::debug::nativePrintArgs("Error can't find a packfile.");
            return false;
        }
    }
    standard_access_disable();
    return true;
}

// orig_main.md OM4: with the real scheduler (the test programs keep the stand-in of tests/stubs/vlib/vlib_noboot.cpp).
// The calling thread's pool for the destruction's temporary data, cached per thread.
__declspec(thread) mempool_base* This_context_virtual_mempool; // TLS +0x848

// 0x005D2160 (:2166)
// Before threading and on the sim thread Common_mempool; on a scheduler thread its controller's "cpu virtual pool".
// Notes (decompiled behaviour, kept as is): the controller lookup runs after Common_mempool was chosen too (the sim
// thread is no controller's, so it keeps Common_mempool); any other thread gets null.
mempool_base* get_this_context_virtual_mempool()
{
    if (This_context_virtual_mempool != nullptr)
    {
        return This_context_virtual_mempool;
    }
    if (scheduler_is_pre_threading() || scheduler_is_sim_thread())
    {
        This_context_virtual_mempool = &Common_mempool;
    }
    cpu_controller* controller = scheduler_get_this_context_controller();
    if (controller != nullptr)
    {
        This_context_virtual_mempool = &controller->get_cpu_virtual_mempool();
        return This_context_virtual_mempool;
    }
    return This_context_virtual_mempool;
}

// 0x005D1A30 (:469)
bool packfile_info::should_register() const
{
    return ((preload && Game_preload) || (registered && Use_packfiles)) && !localized;
}

// 0x005D4860 (:1398)
// The table packfile (table.vpp_pc) is read whole into Common_mempool, its condensed directory and data into a 25 MB
// pregame pool, and the xml reader takes the tables from there (the precache, released at the end of the second
// stage). Its CRC is the table hash of the multiplayer matchmaking.
void game_enable_table_precache()
{
    if (!Use_packfiles)
    {
        return;
    }
    mempool_mark_scope scope(&Common_mempool);
    standard_access_enable();
    // Magic number: 25 MB.
    Table_preload_pool_ptr = salloc_pregame_split_lock(0x1900000, 0);
    char filename[0x44];
    sprintf(filename, "%s.%s", "/gamebuild\\table", "vpp_pc");
    if (!cf_exist(filename))
    {
        standard_access_disable();
        return;
    }
    unsigned int size;
    cf_get_file_size(&size, filename, true);
    stream2_os_file request;
    request.chain_read = false;
    request.encrypted = false;
    request.filename[0] = '\0';
    request.dest = nullptr;
    request.chain_num_reads = 0;
    request.chain_size = nullptr;
    request.chain_dest = nullptr;
    request.priority = 0;
    request.category = 0;
    request.preopened = nullptr;
    strcpy(request.filename, filename);
    request.offset = 0;
    request.size = size;
    request.dest = (unsigned char*)Common_mempool.alloc(size, 0);
    if (request.dest == nullptr)
    {
        standard_access_disable();
    }
    else
    {
        const unsigned int handle = stream2_os_read(request);
        while (stream2_os_poll(handle) == SOS_BUSY)
        {
            os_sleep(1);
        }
        const stream2_os_req_status status = stream2_os_poll(handle);
        stream2_os_release_handle(handle);
        if (status != SOS_DONE_SUCCESS)
        {
            game_disable_standard_access();
            Table_preload_pool_ptr->clear();
            salloc_pregame_split_unlock((rfg_split_mempool*)Table_preload_pool_ptr);
            return;
        }
        v_packfile packfile;
        if (packfile_read_condensed(request.dest, size, Table_preload_pool_ptr, &packfile))
        {
            // Magic number: 350 table files.
            if (!xml_enable_precache(&packfile, Table_preload_pool_ptr, 0x15E))
            {
                Table_preload_pool_ptr->clear();
                salloc_pregame_split_unlock((rfg_split_mempool*)Table_preload_pool_ptr);
            }
            multi_core_matchmake_internal_set_mod_table_hash(keen::getCrc32Value(packfile.data, packfile.data_size));
            game_disable_standard_access();
            return;
        }
        game_disable_standard_access();
    }
    // Original: the pool is released while Table_preload_pool_ptr keeps pointing at it.
    Table_preload_pool_ptr->clear();
    salloc_pregame_split_unlock((rfg_split_mempool*)Table_preload_pool_ptr);
}

// 0x005D1DE0 (:1931)
void game_draw_console_rect(int x, int y, int w, int h, color& rect_color)
{
    if (rect_color.alpha != 0)
    {
        gr_set_state(GM_FILTER);
        gr_set_color(rect_color);
        gr_rect(x, y, w, h, GM_FILTER);
    }
}

// 0x005D4CD0 (:1943)
// The console's text in Body_font, scaled with the screen height (0.54 at 768 lines); w and h return the scaled size.
void game_draw_console_string(const char* str, int x, int y, int len, int& w, int& h, color& string_color)
{
    // Magic numbers: 0.54 at a 768 line screen.
    const float scale = ((float)rl_render_lib::get_instance()->get_screen_info()->get_screen_height() * 0.54f) / 768.0f;
    if (len > 0)
    {
        keen::copyString(console_string_utf8_buffer, sizeof(console_string_utf8_buffer), str, str + len);
        len = (int)keen::getStringLengthUTF8(console_string_utf8_buffer);
    }
    // Original: the whole string is converted, not the copy of its first len bytes.
    keen::convertUTF8ToUTF16((uint16*)console_string_draw_buffer, 0x800, str);
    gr_get_string_size(w, h, console_string_draw_buffer, len, Body_font);
    if (string_color.alpha != 0)
    {
        gr_set_color(string_color);
        gr_string_scale(x, y, console_string_draw_buffer, scale, Body_font, GM_FILTER);
    }
    w = (int)((float)w * scale);
    h = (int)((float)h * scale);
}

// 0x005D4810 (:1336)
// The console follows the screen size.
void update_console(vconsole_config& config)
{
    const rl_screen_info* screen = rl_render_lib::get_instance()->get_screen_info();
    if (config.screen_width == screen->get_screen_width() && config.screen_height == screen->get_screen_height())
    {
        return;
    }
    config.screen_width = rl_render_lib::get_instance()->get_screen_info()->get_screen_width();
    config.screen_height = rl_render_lib::get_instance()->get_screen_info()->get_screen_height();
}

// 0x005D2980 (:6331)
// Copies a UTF-8 string of at most maxNumCharacters characters. With useRfgEscapeSequences the characters the
// Volition text formatter interprets (% [ \ { and space) become private use characters U+EDD0-U+EDD4; without it a
// space becomes a no-break space. A string cut by the capacity or the character count ends in "...".
void rfg_shorten_display_string(const char* pString, char* pOutString, unsigned int outStringCapacity, unsigned int maxNumCharacters, bool useRfgEscapeSequences)
{
    unsigned int characters = 0;
    unsigned char* out = (unsigned char*)pOutString;
    const unsigned char* in = (const unsigned char*)pString;
    if (*in == 0)
    {
        *pOutString = '\0';
        return;
    }
    for (;;)
    {
        unsigned char encoded[4];
        int in_length;
        const unsigned char lead = *in;
        if ((char)lead < 0)
        {
            if ((lead & 0xE0) == 0xC0)
            {
                in_length = 2;
            }
            else if ((lead & 0xF0) == 0xE0)
            {
                in_length = 3;
            }
            else if ((lead & 0xF8) == 0xF0)
            {
                in_length = 4;
            }
            else
            {
                exit(1); // original: an invalid lead byte ends the process
            }
        }
        else
        {
            in_length = 1;
        }
        memcpy(encoded, in, in_length);
        int out_length = in_length;
        if (!useRfgEscapeSequences)
        {
            if (lead == ' ')
            {
                // U+00A0
                encoded[0] = 0xC2;
                encoded[1] = 0xA0;
                out_length = 2;
            }
        }
        else
        {
            // U+EDD0 (%), U+EDD1 ([), U+EDD2 (\), U+EDD3 ({), U+EDD4 (space)
            unsigned char escaped = 0;
            switch (lead)
            {
            case '%': escaped = 0x90; break;
            case '[': escaped = 0x91; break;
            case '\\': escaped = 0x92; break;
            case '{': escaped = 0x93; break;
            case ' ': escaped = 0x94; break;
            default: break;
            }
            if (escaped != 0)
            {
                encoded[0] = 0xEF;
                encoded[1] = 0xB7;
                encoded[2] = escaped;
                out_length = 3;
            }
        }
        if ((unsigned int)((out + out_length) - (unsigned char*)pOutString) >= outStringCapacity || characters == maxNumCharacters)
        {
            break;
        }
        memcpy(out, encoded, out_length);
        out += out_length;
        characters++;
        in += in_length;
        if (*in == 0)
        {
            *out = 0;
            return;
        }
    }
    // Cut: back to a character boundary that leaves room for "...".
    *out = 0;
    for (unsigned int length = (unsigned int)(out - (unsigned char*)pOutString); (outStringCapacity - 4 < length || (*out & 0xC0) == 0x80) && (int)length > 0; length--)
    {
        out--;
    }
    out[0] = '.';
    out[1] = '.';
    out[2] = '.';
    out[3] = 0;
}

// 0x005D2BA0 (:6517)
// A user name parameter of a session interaction is shortened to 20 characters with the Volition escapes.
void rfg_process_game_session_interaction_param(kaiko::GameSessionInteractionParameter& param)
{
    if (param.type == kaiko::GameSessionInteractionParameterType_UserNameString)
    {
        kaiko::GameSessionInteractionParameter shortened = param;
        // Magic numbers: a 64 byte buffer, 20 characters.
        rfg_shorten_display_string(param.value.stringValue, shortened.value.stringValue, 0x40, 0x14, true);
        param = shortened;
    }
}

// 0x005D1E10 (:1966)
void game_draw_console_line(int x1, int y1, int x2, int y2, color& line_color)
{
    if (line_color.alpha != 0)
    {
        gr_set_color(line_color);
        gr_line(x1, y1, x2, y2, GM_FILTER, true);
    }
}

// 0x005D2C00 (:6533)
// The error viewer event a session interaction shows (ids above 999999), its user and whether it came from the remote
// game session; its response is cleared.
void rfg_set_error_viewer_event_id(unsigned int event_id, const keen::UserAccountId& user, bool is_remote_game_session)
{
    Error_viewer_event_id = event_id;
    Error_viewer_reponse = 0;
    Error_viewer_last_event_was_remote_game_session = is_remote_game_session;
    // Original: a memmove when the user overlaps the global.
    memmove(&Error_viewer_user, &user, sizeof(Error_viewer_user));
}

// 0x005D2C90 (:6541)
void rfg_clear_error_viewer_event_id()
{
    Error_view_currently_handled_event_id = Error_viewer_event_id;
    Error_viewer_event_id = 0;
}

// 0x005D2CB0 (:6547)
unsigned int rfg_get_error_viewer_event_id()
{
    return Error_viewer_event_id;
}

// 0x005D2CC0 (:6552)
bool rfg_get_error_viewer_last_event_was_for_remote_game_session()
{
    return Error_viewer_last_event_was_remote_game_session;
}

// 0x005D2CD0 (:6557)
const keen::UserAccountId& rfg_get_error_viewer_user()
{
    return Error_viewer_user;
}

// 0x005D2CE0 (:6562)
void rfg_set_error_viewer_response(unsigned int response)
{
    Error_viewer_reponse = response;
    Error_view_currently_handled_event_id = 0;
}

// 0x005D2D40 (:6590)
// Whether the game window is in front without a system message over it (the platform UI then gates the gameplay
// input).
bool rfg_is_gameframework_fully_in_foreground()
{
    return keen::GameFramework::getCurrentProcessState(Rfg_gameframework_system) == keen::ApplicationProcessState_Foreground;
}

// 0x005D2DC0 (:6688)
const unsigned int* rfg_get_tracked_chunk_ids()
{
    return s_trackedChunkIds;
}

// 0x005D2DD0 (:6693)
unsigned int rfg_get_num_tracked_chunk_ids()
{
    // Original: 12, more than s_trackedChunkIds holds.
    return 0xC;
}

// 0x005D9660 (:5553)
// Deviation 5 (orig_main.md section 3, off by default): -allowremote on the command line skips the remote desktop
// refusal, for development over RDP.
static bool allow_remote_session()
{
    for (int i = 1; i < __argc; i++)
    {
        if (strcmp(__argv[i], "-allowremote") == 0)
        {
            return true;
        }
    }
    return false;
}

int rfg_pre_initialize_game(keen::GameFrameworkSystem* pKeenGameFrameworkSystem, keen::GraphicsCommandWriter* pGraphicsCommandWriter)
{
    if (GetSystemMetrics(SM_REMOTESESSION) != 0 && !allow_remote_session())
    {
        // OpenRFG deviation: the message box is replaced by a log line.
        // MessageBoxA(nullptr, "Application may not be launched remotely.", "Error!", 0);
        keen::debug::nativePrintArgs("[error] Application may not be launched remotely.\n");
        return -1;
    }
    xml_set_page_pool(&Common_page_pool);
    vlib_timer_init();
    rfg_init_stage_1_prepare(pKeenGameFrameworkSystem, pGraphicsCommandWriter);
    return 0;
}

// The boot timers: timer_get before each system with the indent raised (BOOT_TIMED for one statement).
#define BOOT_TIMER_BEGIN()             \
    do                                 \
    {                                  \
        timer_get(TIMER_MILLISECONDS); \
        Load_timer_indent += 2;        \
    } while (0)
#define BOOT_TIMER_END() Load_timer_indent -= 2

// The pools of the first stage, from the mempool provider (mempool_init_alloc, a zone that is never freed).
static void boot_stage_1_pools()
{
    unsigned char* memory = mempool_init_alloc(0xB9000);
    DL_Mempool.init(memory, 0xB9000, "dlmalloc", 4, 8);
    // The original reads the pool's max_size directly.
    const unsigned int dl_size = DL_Mempool.space_max();
    dl_system_init(DL_Mempool.alloc_all(0), dl_size);
    World_state_mempool.init(mempool_init_alloc(0x500000), 0x500000, "world_state", 4, 8);
    Vint_perm_interface_mempool.init(mempool_init_alloc(0x273848), 0x273848, "vint_perm", 4, 8);
    // Magic numbers: the level pool is 15.5 MB with packfiles, 15 MB without.
    const unsigned int level_size = Use_packfiles ? 0xF7D000 : 0xF00000;
    Level_mempool.init(mempool_init_alloc(level_size), level_size, "level", 4, 8);
    Audio_mempool.init(mempool_init_alloc(0x500000), 0x500000, "audio", 4, 8);
    Rig_mempool.init(mempool_init_alloc(0x74000), 0x74000, "rigs", 4, 8);
    Animation_mempool.init_rfg_split_mempool("animation", mempool_init_alloc(0xB42800), 0xB42800, 4, nullptr, 0, 1, 0);
    Anim_blend_tree_mempool.init(mempool_init_alloc(0xC800), 0xC800, "anim_blend_tree", 4, 0);
    Cloth_sim_mempool.init(mempool_init_alloc(0x19000), 0x19000, "cloth sim", 4, 8);
    Stream_cloth_sim_preload_allocator.init(&Cloth_sim_mempool);
    Cloth_sim_runtime_mempool.init(mempool_init_alloc(0x34800), 0x34800, 0x400, "cloth sim runtime", 4, 0);
    Havok_mempool.init(mempool_init_alloc(0x4B00000), 0x4B00000, "Havok", 4, 8);
    Script_mempool.init(mempool_init_alloc(0x64000), 0x64000, "Script mempool", 4, 8);
    Remote_file_cache.init(mempool_init_alloc(0x10000), 0x10000, "Remote file cache", 4, 8);
    // The level memory manager over memory system block 6: tiny buckets of 16 byte pages up to 4 KB and 1 KB pages up
    // to 256 KB.
    tiny_bucket_info tiny_buckets[2];
    tiny_buckets[0].size = 0x240000;
    tiny_buckets[0].page_size = 0x10;
    tiny_buckets[0].largest_alloc = 0x1000;
    tiny_buckets[0].hash_pool = &Level_memmgr_header_pool;
    tiny_buckets[0].num_hash_entries = 0x2000;
    tiny_buckets[1].size = 0x1600000;
    tiny_buckets[1].page_size = 0x400;
    tiny_buckets[1].largest_alloc = 0x40000;
    tiny_buckets[1].hash_pool = &Level_memmgr_header_pool;
    tiny_buckets[1].num_hash_entries = 0x800;
    Level_memmgr_header_pool.init(mempool_init_alloc(0x95010), 0x95010, "Level memmgr headers", 4, 8);
    Level_memmgr.init(s_levelMemoryBlock.pStart, (unsigned int)s_levelMemoryBlock.size, 2, tiny_buckets, Level_memmgr_header_pool, 0x800, Level_memmgr_header_pool, 0xC00);
    Stream_world_allocator.init(&Level_memmgr, &Level_memmgr);
    RFG_scratch_pool.init(mempool_init_alloc(0x130000), 0x130000, "thread scratch space", 4, 8);
    Special_reserve.init_rfg_split_mempool("special reserve mempool", mempool_init_alloc(4), 4, 4, nullptr, 0, 4, 8);
    Chunk_preload_pool.init_rfg_split_mempool("chunk_preload", mempool_init_alloc(0x4B000), 0x4B000, 4, nullptr, 0, 4, 8);
    Stream_chunk_preload_allocator.init(&Chunk_preload_pool);
    Permanent_mempool.init_rfg_split_mempool("Permanent mempool", mempool_init_alloc(0xF00000), 0xF00000, 4, nullptr, 0, 4, 8);
    Stream_permanent_allocator.init(&Permanent_mempool);
    Interface_mempool.init_rfg_split_mempool("interface", mempool_init_alloc(0x980000), 0x980000, 4, nullptr, 0, 4, 8);
    Interface_perm_mempool.init_rfg_split_mempool("interface_perm", mempool_init_alloc(0x3000000), 0x3000000, 4, nullptr, 0, 4, 8);
    Interface_font_mempool.init_rfg_split_mempool("interface_font", mempool_init_alloc(0x1700000), 0x1700000, 4, nullptr, 0, 4, 8);
    memory = mempool_init_alloc(0x144C000);
    Effect_preload_mempool.init_rfg_split_mempool("effects preload", memory, 0x144C000, 4, nullptr, 0, bm_get_peg_align_value(), 8);
    Stream_effect_preload_allocator.init(&Effect_preload_mempool);
    Item_preload_pool.init_rfg_split_mempool("item_preload", mempool_init_alloc(0x8000000), 0x8000000, 4, nullptr, 0, 4, 8);
    Stream_item_preload_allocator.init(&Item_preload_pool);
    memory = mempool_init_alloc(0x2C9000);
    Decal_preload_mempool.init_rfg_split_mempool("decal preload", memory, 0x2C9000, 4, nullptr, 0, bm_get_peg_align_value(), 8);
    Stream_decal_preload_allocator.init(&Decal_preload_mempool);
    memory = mempool_init_alloc(0x210000);
    Time_of_day_mempool.init_rfg_split_mempool("time of day", memory, 0x210000, 4, nullptr, 0, bm_get_peg_align_value(), 8);
    Stream_tod_allocator.init(&Time_of_day_mempool);
    Localization_mempool.init(mempool_init_alloc(0x100000), 0x100000, "Localization Pool", 4, 0);
    // The stream2 slot allocators: {CPU size, GPU size, slots} per bracket (the CPU halves from the mempool provider,
    // the GPU halves from the VRAM provider, 2 KB aligned).
    int player_slots[3] = {0x1254000, 0, 1};
    Stream_player_allocator.init(mempool_init_alloc_aligned, 0x800, vram_mempool_init_alloc_aligned, 0x800, player_slots, 1, "Player slots");
    int human_slots[3] = {0x900000, 0, 0x10};
    Stream_human_allocator.init(mempool_init_alloc_aligned, 0x800, vram_mempool_init_alloc_aligned, 0x800, human_slots, 1, "human slots");
    int voice_slots[3] = {0xF800, 0, 0x40};
    Stream_human_voice_persona_allocator.init(mempool_init_alloc_aligned, 4, vram_mempool_init_alloc_aligned, 4, voice_slots, 1, "human voice slots");
    Human_voice_always_loaded_pool.init(mempool_init_alloc(0x3FC00), 0x3FC00, "always loaded personas", 4, 8);
    Stream_always_loaded_human_voice_persona_allocator.init(&Human_voice_always_loaded_pool);
    int large_weapon_slots[3] = {0x1100000, 0, 3};
    Stream_large_weapon_allocator.init(mempool_init_alloc_aligned, 0x800, vram_mempool_init_alloc_aligned, 0x800, large_weapon_slots, 1, "large weapon slots");
    int small_weapon_slots[3] = {0x800000, 0, 1};
    Stream_small_weapon_allocator.init(mempool_init_alloc_aligned, 0x800, vram_mempool_init_alloc_aligned, 0x800, small_weapon_slots, 1, "small weapon slots");
    int vehicle_slots[3] = {0x4400000, 0, 7};
    Stream_vehicle_allocator.init(mempool_init_alloc_aligned, 0x800, vram_mempool_init_alloc_aligned, 0x800, vehicle_slots, 1, "Vehicle slots");
    // The audio slots, both halves from the audio bank allocator (16 byte aligned CPU half, no GPU alignment).
    int audio_slots[3] = {0x300000, 0, 7};
    Stream_audio_slot_allocator.init(audio_mempool_init_alloc_aligned, 0x10, audio_mempool_init_alloc_aligned, 0, audio_slots, 1, "audio slots");
    int small_layer_slots[3] = {0x8000, 0, 3};
    Stream_small_layer_allocator.init(mempool_init_alloc_aligned, 0x800, vram_mempool_init_alloc_aligned, 0x800, small_layer_slots, 1, "Small Layer Slots");
    int large_layer_slots[3] = {0x11000, 0, 1};
    Stream_large_layer_allocator.init(mempool_init_alloc_aligned, 0x800, vram_mempool_init_alloc_aligned, 0x800, large_layer_slots, 1, "Large Layer Slots");
    Undergrowth_mempool.init(mempool_init_alloc(0x80000), 0x80000, 0x20, "Undergrowth Paged Mempool", 4, 0);
    Material_effects_mempool.init(mempool_init_alloc(0x40000), 0x40000, 0x20, "Material Effects Paged Mempool", 4, 0);
    Stream_material_effects_allocator.init(&Material_effects_mempool);
}

// 0x005D8630 (:2840)
// The first boot stage on the main thread (render step 0): vlib, the common pools, zlib, cfile and the packfiles, the
// xml reader, keys and mouse, the renderers, the video player, the pools of the game, the bitmaps, localization, the
// frame time, multiplayer, stream2's request pool, the table precache, the UI, the state machine, input and controls,
// audio and the start-up videos.
void rfg_init_stage_1_prepare(keen::GameFrameworkSystem* pKeenGameFrameworkSystem, keen::GraphicsCommandWriter* pGraphicsCommandWriter)
{
    BOOT_TIMER_BEGIN();
    // Magic number: a 2 MB vlib pool (vlib_system_init does not use it).
    vlib_system_init("vlib", false, 0x200000);
    BOOT_TIMER_END();
    if (Game_boot_exit_flag)
    {
        boot_exit_cleanup();
        return;
    }
    // Magic number: a 12.75 MB page budget for the common pool.
    Common_page_pool.init("common page pool", 0xCC0000, true);
    Common_mempool.init(&Common_page_pool, "common_mempool", 0, false, 0);
    vlib_set_temp_mempool(&Common_mempool);
    vlib_set_temp_page_pool(&Common_page_pool);
    Use_packfiles = true;
    Disc_build = false;
    if (Game_boot_exit_flag || (zlib_init(), Game_boot_exit_flag))
    {
        boot_exit_cleanup();
        return;
    }
    if (!Use_packfiles)
    {
        BOOT_TIMER_BEGIN();
        run_preps();
        BOOT_TIMER_END();
        if (Game_boot_exit_flag)
        {
            boot_exit_cleanup();
            return;
        }
    }
    // The renderers' parameters: high shadows, no antialiasing, very high particles, no soft shadows, the post effects
    // on, then the display options of the options cache.
    rl_init_params params;
    params.m_vsync_enabled = 0;
    params.m_lod.m_aa_lod = RL_AA_LOD_OFF;
    params.m_lod.m_particle_lod = RL_PARTICLE_LOD_VERY_HIGH;
    params.m_lod.m_shadow_lod = RL_SHADOW_LOD_HIGH;
    params.m_lod.m_bloom_enabled = true;
    params.m_lod.m_motion_blur_enabled = true;
    params.m_lod.m_depth_of_field_enabled = true;
    params.m_lod.m_distortion_enabled = true;
    params.m_lod.m_soft_shadows = false;
    params.m_memtracker_start_cb = game_track_mem_usage_start;
    params.m_memtracker_end_cb = game_track_mem_usage_end;
    params.m_pFileSystem = keen::GameFramework::getFileSystem(pKeenGameFrameworkSystem);
    params.m_pDefaultAllocator = Rfg_cpu_scratch_keen_allocator;
    params.m_occlusionQueryGPUMemory.pStart = nullptr;
    params.m_occlusionQueryGPUMemory.size = 0;
    if (keen::getHostPlatform() != keen::PlatformType_Win32)
    {
        params.m_pVramAllocator = &Rfg_vram_mempool_provider_keen_allocator;
        params.m_pFramebufferAllocator = &Rfg_frameBufferAllocator;
    }
    params.m_pBufferAllocator = &Rfg_vram_mempool_provider_keen_allocator;
    params.m_pKeenGraphicsSystem = keen::GameFramework::getGraphicsSystem(pKeenGameFrameworkSystem);
    bool vsync_enabled = false;
    // Magic number: a 45 degree field of view unless the options have one.
    int camera_fov = 0x2D;
    if (game_options_load_display_options(params.m_screen_info, params.m_lod, vsync_enabled, camera_fov))
    {
        params.m_vsync_enabled = vsync_enabled ? 1 : 0;
        params.m_uses_screen_info = 1;
    }
    else
    {
        // Magic numbers: 60/1 Hz.
        params.m_screen_info.set_refresh_rate(0x3C, 1);
    }
    BOOT_TIMER_BEGIN();
    packfile_config config;
    // Magic numbers: 16,384 files, 512 packfiles, 256 temporary packfiles.
    config.max_files = 0x4000;
    config.max_packfiles = 0x200;
    config.max_temp_packfiles = 0x100;
    bool cfile_ready;
    if (!Use_packfiles)
    {
        // Magic numbers: the loose-file build's pools.
        config.pool_size = 0x19000;
        config.temp_pool_size = 0x64000;
        vdir_config vdir;
        vdir.pool_size = 0x800000;
        cfile_ready = cfile_system_init(&vdir, &config, false);
        if (cfile_ready)
        {
            cf_add_search(CF_SEARCH_STANDARD, CF_IO_MEDIA_HDD, true);
            cf_add_search(CF_SEARCH_VDIR, CF_IO_MEDIA_HDD, true);
            cf_add_search(CF_SEARCH_PACKFILE, CF_IO_MEDIA_HDD, false);
        }
    }
    else
    {
        // Magic numbers: 400 KB of packfile directory and 600 KB for the temporary packfiles.
        config.pool_size = 0x64000;
        config.temp_pool_size = 0x96000;
        cfile_ready = cfile_system_init(nullptr, &config, Disc_build);
        if (cfile_ready)
        {
            cf_add_search(CF_SEARCH_PACKFILE, Disc_build ? CF_IO_MEDIA_DVD : CF_IO_MEDIA_HDD, true);
            cf_add_search(CF_SEARCH_PACKFILE, CF_IO_MEDIA_MEMORY, false);
            cf_add_search(CF_SEARCH_PACKFILE, CF_IO_MEDIA_HDD, false);
        }
    }
    if (!cfile_ready)
    {
        // Original: no bail-out cleanup here.
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "C:/unit4projects/rfg/root/code/volition/rfg/code/rfg.cpp", Use_packfiles ? 0xC09 : 0xC36);
        return;
    }
    BOOT_TIMER_END();
    if (Game_boot_exit_flag)
    {
        boot_exit_cleanup();
        return;
    }
    if (Use_packfiles)
    {
        BOOT_TIMER_BEGIN();
        if (!init_packfiles())
        {
            keen::debug::nativePrintArgs("Error initializing game packfiles! I have to roll over and die now. Good night, and good luck.");
        }
        BOOT_TIMER_END();
        if (Game_boot_exit_flag)
        {
            boot_exit_cleanup();
            return;
        }
    }
    BOOT_TIMER_BEGIN();
    xml_system_init(&Common_mempool);
    BOOT_TIMER_END();
    BOOT_TIMER_BEGIN();
    quaternion_init();
    BOOT_TIMER_END();
    BOOT_TIMER_BEGIN();
    key_init();
    BOOT_TIMER_END();
    BOOT_TIMER_BEGIN();
    mouse_init();
    BOOT_TIMER_END();
    BOOT_TIMER_BEGIN();
    game_render_system_init(params, pGraphicsCommandWriter, s_deferredCommandBufferMemoryBlock);
    s_mainRenderer.initialize(params);
    s_simThreadRenderer.initialize(params);
    s_deferredRenderer.initialize(params);
    BOOT_TIMER_END();
    timer_get(TIMER_MILLISECONDS);
    BOOT_TIMER_BEGIN();
    video_player_system_init();
    BOOT_TIMER_END();
    if (Game_boot_exit_flag)
    {
        boot_exit_cleanup();
        return;
    }
    boot_stage_1_pools();
    if (Game_boot_exit_flag)
    {
        boot_exit_cleanup();
        return;
    }
    // Magic numbers: a 90% safe area.
    gr_set_safe_area(0.9f, 0.9f);
    BOOT_TIMER_BEGIN();
    bm_init("startup", "misc-static.tga", &Permanent_mempool, rl_texture_register, rl_texture_unregister, nullptr, nullptr);
    BOOT_TIMER_END();
    BOOT_TIMER_BEGIN();
    localization_system_init();
    BOOT_TIMER_END();
    if (Game_boot_exit_flag)
    {
        boot_exit_cleanup();
        return;
    }
    BOOT_TIMER_BEGIN();
    frametime_init();
    // Magic numbers: the frame time between 1/300 s and 1/10 s.
    frametime_set_cap(0.0033333334f, 0.1f);
    BOOT_TIMER_END();
    if (Game_boot_exit_flag)
    {
        boot_exit_cleanup();
        return;
    }
    BOOT_TIMER_BEGIN();
    multi_system_init();
    BOOT_TIMER_END();
    // Magic numbers: 768 stream2 requests with packfiles, 1024 without.
    stream2_os_system_init(&Permanent_mempool, Use_packfiles ? 0x300 : 0x400);
    if (Use_packfiles)
    {
        BOOT_TIMER_BEGIN();
        game_enable_table_precache();
        BOOT_TIMER_END();
    }
    BOOT_TIMER_BEGIN();
    ui_init();
    BOOT_TIMER_END();
    BOOT_TIMER_BEGIN();
    gameseq_init();
    BOOT_TIMER_END();
    BOOT_TIMER_BEGIN();
    input_init();
    BOOT_TIMER_END();
    BOOT_TIMER_BEGIN();
    controls_system_init();
    BOOT_TIMER_END();
    if (!Game_boot_exit_flag && (gameaudio_system_init_stage_1(), !Game_boot_exit_flag))
    {
        ui_startup_video_init();
        return;
    }
    boot_exit_cleanup();
}

// 0x005D2440 (:4159)
// True once the start-up videos are done (or were never started).
bool rfg_init_stage_1_loop_update()
{
    if (Startup_videos_initialized && !Exit_startup_videos)
    {
        ui_startup_video_process();
        return false;
    }
    return true;
}

// 0x005D50E0 (:4173)
void rfg_init_stage_1_loop_render(keen::GraphicsCommandWriter* pCommandWriter)
{
    rl_render_lib::get_instance()->check_device_status();
    if (Startup_videos_initialized && !Exit_startup_videos)
    {
        s_mainRenderer.begin(pCommandWriter);
        game_render_begin_frame();
        rl_draw::get_instance()->set_renderer(&s_mainRenderer);
        ui_startup_video_render(s_mainRenderer);
        game_render_end_frame();
        s_mainRenderer.end();
        rl_draw::get_instance()->set_renderer(nullptr);
    }
}

// 0x005D5150 (:4188)
// The end of the first stage: the loading states, the boot screen thread, the HUD's screen scale, the developer
// console, the scheduler and the compiled render groups.
void rfg_init_stage_1_done()
{
    ui_startup_video_cleanup();
    loading_system_init();
    if (Game_boot_exit_flag)
    {
        boot_exit_cleanup();
        return;
    }
    game_boot_thread_start();
    // Magic number: the boot screen thread gets 200 ms to start.
    os_sleep(200);
    BOOT_TIMER_BEGIN();
    hud_system_critical_init();
    BOOT_TIMER_END();
    BOOT_TIMER_BEGIN();
    vconsole_config console;
    console.safe_area_left = 0;
    console.screen_width = 0;
    console.screen_height = 0;
    console.save_history = false;
    console.draw_rect = game_draw_console_rect;
    console.draw_str = game_draw_console_string;
    console.update_func = update_console;
    console.safe_area_left = gr_safe_area_left();
    console.screen_width = rl_render_lib::get_instance()->get_screen_info()->get_screen_width();
    console.save_history = !Use_packfiles;
    console.screen_height = rl_render_lib::get_instance()->get_screen_info()->get_screen_height();
    console_init(&console);
    BOOT_TIMER_END();
    if (Game_boot_exit_flag)
    {
        boot_exit_cleanup();
        return;
    }
    if (!Use_packfiles)
    {
        if (Game_preload)
        {
            cf_add_search(CF_SEARCH_PACKFILE, CF_IO_MEDIA_MEMORY, false);
        }
        BOOT_TIMER_BEGIN();
        if (!init_packfiles())
        {
            keen::debug::nativePrintArgs("Error initializing game packfiles! I have to roll over and die now. Good night, and good luck.");
        }
        BOOT_TIMER_END();
        if (Game_boot_exit_flag)
        {
            boot_exit_cleanup();
            return;
        }
    }
    timer_get(TIMER_MILLISECONDS);
    BOOT_TIMER_BEGIN();
    scheduler_init(nullptr, &Common_page_pool, &RFG_scratch_pool);
    BOOT_TIMER_END();
    rl_compiled_render_group_init(&Rfg_mempool_provider_keen_allocator);
}

// 0x005D54E0 (:4366)
// The language: once the profiles are read (the selection screen, or the detected language), the selected language is
// saved, or set once the boot screen document is loaded.
bool rfg_init_stage_2_loop_update()
{
    // Magic number: 0x31, the number of languages.
    if ((int)User_selected_language > 0x30 || User_selected_language == (keen::LanguageId)~(int)keen::LanguageId_En_En)
    {
        if (!Profiles_read)
        {
            if (Boot_language_select && localization_non_selectable_language_supported() && localization_get_number_supported_languages() != 1)
            {
                player_profile_process();
                Show_language_selection = true;
            }
            else
            {
                User_selected_language = localization_auto_detect_language();
                Show_language_selection = false;
            }
            Profiles_read = true;
        }
        controls_poll();
        if (Game_boot_exit_flag)
        {
            boot_exit_cleanup();
        }
        return false;
    }
    if (Show_language_selection)
    {
        BOOT_TIMER_BEGIN();
        localization_set_and_save_selected_language(User_selected_language, User_selected_language);
        BOOT_TIMER_END();
        return true;
    }
    if (!Boot_screen_doc_loaded)
    {
        return false;
    }
    BOOT_TIMER_BEGIN();
    localization_set_language(User_selected_language);
    localization_set_audio_language(User_selected_language);
    BOOT_TIMER_END();
    return true;
}

// 0x005D5670 (:4409)
// The boot screen frame while the second stage runs.
void rfg_init_stage_2_loop_render(keen::GraphicsCommandWriter* pCommandWriter)
{
    rl_render_lib::get_instance()->check_device_status();
    if (game_boot_thread_running())
    {
        game_boot_thread_render(pCommandWriter, &s_mainRenderer);
    }
}

// 0x005D6420 (:5656)
// The pools and slot allocators of the first stage go, then the game's systems (gamewide_shutdown) and the keen
// allocators.
void rfg_shutdown_game()
{
    effects_system_release_backpack_effects();
    Level_memmgr.destroy();
    Common_mempool.destroy();
    Chunk_preload_pool.mempool_base::destroy();
    Effect_preload_mempool.mempool_base::destroy();
    Item_preload_pool.mempool_base::destroy();
    Decal_preload_mempool.mempool_base::destroy();
    Cloth_sim_mempool.mempool_base::destroy();
    Time_of_day_mempool.mempool_base::destroy();
    Stream_player_allocator.destroy();
    Stream_human_allocator.destroy();
    Stream_large_weapon_allocator.destroy();
    Stream_small_weapon_allocator.destroy();
    Stream_vehicle_allocator.destroy();
    Stream_large_layer_allocator.destroy();
    Stream_small_layer_allocator.destroy();
    Stream_human_voice_persona_allocator.destroy();
    Human_voice_always_loaded_pool.mempool_base::destroy();
    Material_effects_mempool.mempool_base::destroy();
    Permanent_mempool.mempool_base::destroy();
    Stream_audio_slot_allocator.destroy();
    Rfg_audio_bank_keen_allocator.free(pStream_audio_slot_allocator_memory);
    stream2_set_thread_ownership();
    gamewide_shutdown();
    Rfg_sys_mempool_provider_keen_allocator.destroy();
    Rfg_vram_mempool_provider_keen_allocator.destroy();
    Rfg_mempool_provider_keen_allocator.destroy();
    Rfg_gpu_scratch_keen_allocator.destroy();
    Rfg_frameBufferAllocator.destroy();
    Rfg_audio_bank_keen_allocator.destroy();
    Rfg_audio_scratch_keen_allocator.destroy();
}
#undef BOOT_TIMER_BEGIN
#undef BOOT_TIMER_END

// ---- final_push.md step 4: the global operator new and delete. rfg.exe routes every C++ allocation through
// _aligned_malloc with 16 byte alignment (vector4 and the Havok types are SIMD aligned) and frees it with _aligned_free.
// Only OpenRFG links rfg_loop and gets them; the headless and hkdiff programs keep the CRT's.
//
// Notes (decompiled behaviour, kept as is):
// - A failed allocation returns null (no std::bad_alloc).
// - The nothrow forms are not meant to be used: they print "What are you doing here?" and allocate or free nothing (a
//   nothrow new returns null). Nothing in the port uses them.
// - Not replaced (newer compilers only; the CRT's stay and agree with the forms here): the sized deletes, which forward
//   to operator delete(void*) / delete[](void*) below, and the std::align_val_t forms (alignment above 8), which use
//   _aligned_malloc / _aligned_free themselves.

// 0x005D28A0 (:5877)
void* operator new(size_t n)
{
    // Magic number: 16 byte alignment for every allocation.
    return _aligned_malloc(n, 0x10);
}

// 0x005D28C0 (:5929)
void* operator new[](size_t n)
{
    // Magic number: 16 byte alignment for every allocation.
    return _aligned_malloc(n, 0x10);
}

// 0x005D28E0 (:5976)
void operator delete(void* p) noexcept
{
    _aligned_free(p);
}

// 0x005D28F0 (:5993)
void operator delete[](void* p) noexcept
{
    _aligned_free(p);
}

// 0x005D2900 (:6008)
void* operator new(size_t n, const std::nothrow_t&) noexcept
{
    keen::debug::nativePrintArgs("What are you doing here?");
    return nullptr;
}

// 0x005D2910 (:6014)
void* operator new[](size_t n, const std::nothrow_t&) noexcept
{
    keen::debug::nativePrintArgs("What are you doing here?");
    return nullptr;
}

// 0x005D2920 (:6020)
void operator delete[](void* p, const std::nothrow_t&) noexcept
{
    keen::debug::nativePrintArgs("What are you doing here?");
}

// 0x005D2930 (:6025)
void operator delete(void* p, const std::nothrow_t&) noexcept
{
    keen::debug::nativePrintArgs("What are you doing here?");
}
#endif // !OPENRFG_RFG_PEG_CALLBACKS_ONLY
