// Original: volition/rfg/code/rfg.h
// Implementation: volition/rfg/code/rfg.cpp
//
// Volition game boot and main loop, driven by keen::rfg::Application (game/sources/application.hpp).
// The PDB records no inline code for rfg.h, so its contents are inferred: every rfg.cpp function and global with
// external linkage (a public symbol) is declared here. Functions and globals without one are in the rfg.cpp stub.
//
// Boot (render thread, Application::render steps 0-4; decompiled):
//   rfg_pre_initialize_game   refuses remote-desktop sessions, xml page pool, vlib_timer_init, then stage 1 prepare.
//   rfg_init_stage_1_prepare  vlib, cfile, packfiles (init_packfiles), xml, renderer, all mempools and stream2
//                             allocators, localization, frametime, gameseq_init, input (see docs/architecture).
//   rfg_init_stage_1_loop_*   plays the startup videos until done or skipped (loop_update returns true when done).
//   rfg_init_stage_1_done     starts the boot thread and loading screen, hud critical init, console_init,
//                             init_packfiles (when !Use_packfiles), scheduler_init, compiled render groups.
//   rfg_init_stage_2_prepare  stream2_system_init, game_stream2_register_allocators/types, havok_system_init,
//                             starts the stream2 loading thread (stream2LoadInitFunction).
//   rfg_init_stage_2_loop_*   language selection (profile read, auto-detect or the selection screen), controls_poll.
//   rfg_init_stage_2_done     on an async thread: about 90 subsystem inits in dependency order, each timed and
//                             skipped once Game_boot_exit_flag is set (docs/architecture/simulation.md).
//   rfg_init_stage_2_done_2   stops the boot thread, render thread init, cf_open_lock, Game_booting = false,
//                             srand(timer_get(us)), gameseq_set_state(GS_MAINMENU), locks Permanent_mempool and
//                             registers an empty GS_SHUTDOWN state.
// Every stage bails out through the same cleanup when Game_boot_exit_flag is set (table/soundheader/effects
// preload pools released, boot thread stopped).
//
// Frame (sim thread, keen::rfg::Application::updateSimulation):
//   do_game_main_loop_frame  pauses the game if perfmon asks, rand(), rfg_do_frame(); returns true to quit.
//   rfg_do_frame             Expire_game check; game_check_for_gameplay_shutdown; focus handling (vibration off
//                            while in background); game_input_poll; frametime_calculate(false, 0, 0);
//                            multi_process (except GS_MULTI_INIT/SHUTDOWN); game_save_process; gamelog_process;
//                            gameseq_process(GS_STEP_PROCESS) under keen::rfg::lock_render when a state change is
//                            pending; video_player_process_all; copy the state stack into Sim_snapshot; mouse_flush;
//                            game_handshake_do_frame(s_simThreadRenderer); finish an audio language change.
//                            Returns gameseq_get_state() == GS_SHUTDOWN.
// Render (main thread, Application::render step 5):
//   do_game_render_frame     resolution change, rl_renderer::begin(s_mainRenderer), game_render_do_frame (scene only
//                            when Render_snapshot.in_gameplay and no resource resize is pending), end, flip main and
//                            deferred renderers.
//
// operator new/new[]/delete/delete[] (plain and nothrow) are replaced in rfg.cpp:5877-6025 (0x005D28A0..0x005D2930).
#pragma once

#include "keen/base/language.hpp"
#include "keen/sound/soundsystem.hpp"

namespace keen
{
    class GraphicsCommandWriter;
    class MemoryAllocator;
    class TlsfMemoryAllocator;
    class ZoneMemoryAllocator;
    struct GameFrameworkSystem;
    struct MemoryBlock;
    struct UserAccountId;
}
namespace kaiko
{
    struct GameSessionInteractionParameter;
}
namespace unit4
{
    class LocalGameSession;
}
template<typename T, int N> class farray;
class color;
class mempool;
class mempool_base;
class page_mempool;
class page_pool;
class rfg_split_mempool;
class rl_init_params;
class rl_renderer;
class split_memmgr;
class stream2_dynamic_allocator;
class stream2_mempool_allocator;
class stream2_slot_allocator;
class virtual_mempool;
class vconsole_config;
struct _EXCEPTION_POINTERS;

// ---- boot flags and state
extern bool Use_packfiles; // 0x01D59C55, see init_packfiles in the rfg.cpp stub
extern bool Disc_build; // 0x01D59C56
extern bool Game_preload; // 0x01D59C57
extern bool Game_boot_exit_flag; // 0x01D59C64, set to abort boot (window closed)
extern bool Modified_table_data; // 0x01D59C65
extern bool Demo; // 0x0163F11E
extern int Expire_game; // 0x01D59C2C, rfg_do_frame exits after 200 frames when set
extern int Load_timer_indent; // 0x01D59C84, indentation of the boot timing log
extern volatile bool Show_language_selection; // 0x01D59C67
extern volatile bool Profiles_read; // 0x01D59C98
extern volatile bool Boot_screen_doc_loaded; // 0x01D59C99
extern keen::LanguageId User_selected_language; // 0x0163F184
extern unsigned int Lparam_super_overbrighten; // 0x0163F178, 255: the full opacity of faded objects (placement inferred)
extern bool RL_test_batching_splat; // 0x01D59C9B
extern double Processor_speed; // 0x01D59CA0
extern char Stamp[80]; // 0x0163F188
extern int General_font; // 0x0163F154, font handles loaded by font_init (placement inferred)
extern int Foreign_font; // 0x0163F158
extern int Header_font; // 0x0163F15C
extern int Body_font; // 0x0163F160
extern int Number_font; // 0x0163F164
extern int Latin_num_font; // 0x0163F168
extern int Latin_header_font; // 0x0163F16C
extern int Credits_body_font; // 0x0163F170
extern int Credits_header_font; // 0x0163F174

// ---- keen systems handed to Volition
extern keen::GameFrameworkSystem* Rfg_gameframework_system; // 0x01D59C6C
extern keen::SoundSystem::System* Rfg_soundSystem; // 0x01D59C70
extern keen::MemoryAllocator* Rfg_cpu_scratch_keen_allocator; // 0x01D59C68
extern keen::TlsfMemoryAllocator Rfg_gpuCpuMemoryAllocator; // 0x01640578
extern keen::TlsfMemoryAllocator Rfg_sys_mempool_provider_keen_allocator; // 0x01640658
extern keen::TlsfMemoryAllocator Rfg_vram_mempool_provider_keen_allocator; // 0x01640738
extern keen::ZoneMemoryAllocator Rfg_mempool_provider_keen_allocator; // 0x01640818
extern keen::ZoneMemoryAllocator Rfg_gpu_scratch_keen_allocator; // 0x01640900
extern keen::ZoneMemoryAllocator Rfg_frameBufferAllocator; // 0x016409E8
extern keen::TlsfMemoryAllocator Rfg_audio_bank_keen_allocator; // 0x01640AD0
extern keen::TlsfMemoryAllocator Rfg_audio_scratch_keen_allocator; // 0x01640BB0
// Memory system block 7 (rfg_init_keen_memory_allocators): staging memory for GPU data. stream2_container::alloc
// places the GPU parts of the primitives it loads at its start (they are uploaded by the load callbacks).
extern keen::MemoryBlock s_d3dMemoryBlock; // 0x01D59400

// ---- memory pools defined in rfg.cpp (types: Tier 2). Owned by the sim thread after updateSimulation claims them.
extern mempool_base* g_pCommonMempool; // 0x0163F17C
extern mempool_base* g_pPermanentMempool; // 0x0163F180
extern mempool_base* Table_preload_pool_ptr; // 0x01D59C8C
extern farray<mempool_base*, 24> Memtrack_pools; // 0x01640F58
extern rfg_split_mempool Permanent_mempool; // 0x01D64B20
extern rfg_split_mempool Interface_mempool; // 0x01D59CB8
extern rfg_split_mempool Interface_perm_mempool; // 0x01D60C78
extern rfg_split_mempool Interface_font_mempool; // 0x01D60A80
extern rfg_split_mempool Effect_preload_mempool; // 0x01D64BC8
extern rfg_split_mempool MP_effect_preload_mempool; // 0x01D59DC0
extern rfg_split_mempool MP_item_preload_mempool; // 0x01D60970
extern rfg_split_mempool MP_allocators_mempool; // 0x01D5D328
extern rfg_split_mempool MP_multi_purpose_mempool; // 0x01D59ED0
extern rfg_split_mempool Item_preload_pool; // 0x01D5A188
extern rfg_split_mempool Decal_preload_mempool; // 0x01D5A080
extern rfg_split_mempool Chunk_preload_pool; // 0x01D5D3D0
extern rfg_split_mempool Animation_mempool; // 0x01D5D500
extern rfg_split_mempool Time_of_day_mempool; // 0x01D60828
extern virtual_mempool Common_mempool; // 0x01D5A128
extern virtual_mempool Cutscene_mempool; // 0x01D649C8
extern mempool Anim_blend_tree_mempool; // 0x01D59D70
extern mempool Vint_perm_interface_mempool; // 0x01D59E80
extern mempool Remote_file_cache; // 0x01D59F78
extern mempool Havok_mempool; // 0x01D59FC8
extern mempool DL_Mempool; // 0x01D5A030
extern mempool Shadow_mempool; // 0x01D5D478
extern mempool World_state_mempool; // 0x01D5D5A8
extern mempool Level_mempool; // 0x01D608D0
extern mempool Audio_mempool; // 0x01D60920
extern mempool RFG_scratch_pool; // 0x01D60A18
extern mempool Human_voice_always_loaded_pool; // 0x01D60C28
extern mempool Script_mempool; // 0x01D64870
extern mempool Rig_mempool; // 0x01D648C0
extern mempool Level_memmgr_header_pool; // 0x01D64910
extern mempool Cloth_sim_mempool; // 0x01D64978
extern mempool Localization_mempool; // 0x01D64AD0
extern page_mempool Undergrowth_mempool; // 0x01D60760
extern page_mempool Cloth_sim_runtime_mempool; // 0x01D607D0
extern page_mempool Material_effects_mempool; // 0x01D61220
extern page_pool Common_page_pool; // 0x01D5A230
extern page_pool Gpu_profiler_page_pool; // 0x01D5D5F8
extern page_pool Perfmon_page_pool; // 0x01D61278
extern split_memmgr Level_memmgr; // 0x01D60B28

// ---- stream2 allocators defined in rfg.cpp (stream2_allocator.h), registered by game_stream2_register_allocators
// in game_stream_allocators order (game_stream2_prims.h). The interface, fullscreen map and multiplayer interface
// allocators are defined in their own (deferred) modules.
extern unsigned char* pStream_audio_slot_allocator_memory; // 0x01D59C88
extern stream2_dynamic_allocator Stream_world_allocator; // 0x01D59D5C, GSA_WORLD
extern stream2_mempool_allocator Stream_chunk_preload_allocator; // 0x01640C90, GSA_CHUNK_PRELOAD
extern stream2_mempool_allocator Stream_effect_preload_allocator; // 0x01640C9C (:649), GSA_EFFECT_PRELOAD
extern stream2_mempool_allocator Stream_effect_cutscene_allocator; // 0x01640CA8, GSA_EFFECT_CUTSCENE
extern stream2_mempool_allocator Stream_item_preload_allocator; // 0x01640CB4 (:651), GSA_ITEM_PRELOAD
extern stream2_mempool_allocator Stream_decal_preload_allocator; // 0x01640CC0 (:652), GSA_DECAL_PRELOAD
extern stream2_mempool_allocator Stream_cloth_sim_preload_allocator; // 0x01640CCC (:653), GSA_CLOTH_SIM_PRELOAD
extern stream2_mempool_allocator Stream_tod_allocator; // 0x01640CD8 (:654), GSA_TOD
extern stream2_mempool_allocator Stream_mp_effect_preload_allocator; // 0x01640CE4, GSA_MP_EFFECT_PRELOAD
extern stream2_mempool_allocator Stream_mp_item_preload_allocator; // 0x01640CF0, GSA_MP_ITEM_PRELOAD
extern stream2_slot_allocator Stream_player_allocator; // 0x01640CFC (:657), GSA_PLAYER
extern stream2_slot_allocator Stream_human_allocator; // 0x01640D20 (:658), GSA_HUMAN
extern stream2_slot_allocator Stream_large_weapon_allocator; // 0x01640D44 (:659), GSA_LARGE_WEAPON
extern stream2_slot_allocator Stream_small_weapon_allocator; // 0x01640D68 (:660), GSA_SMALL_WEAPON
extern stream2_slot_allocator Stream_vehicle_allocator; // 0x01640D8C (:662), GSA_VEHICLE
extern stream2_slot_allocator Stream_large_layer_allocator; // 0x01640DB0 (:663), GSA_LARGE_LAYER
extern stream2_slot_allocator Stream_small_layer_allocator; // 0x01640DD4 (:664), GSA_SMALL_LAYER
extern stream2_slot_allocator Stream_human_voice_persona_allocator; // 0x01640DF8 (:665), GSA_HUMAN_VOICE_PERSONA
extern stream2_mempool_allocator Stream_always_loaded_human_voice_persona_allocator; // 0x01640E1C (:666), GSA_ALWAYS_LOADED_HUMAN_VOICE_PERSONA
extern stream2_mempool_allocator Stream_material_effects_allocator; // 0x01640E28 (:667), GSA_MATERIAL_EFFECT
extern stream2_mempool_allocator Stream_permanent_allocator; // 0x01640E34 (:668), GSA_PERMANENT
extern stream2_mempool_allocator Stream_dlc_effect_preload_allocator; // 0x01640E40, GSA_DLC_EFFECT_PRELOAD
extern stream2_mempool_allocator Stream_dlc_item_preload_allocator; // 0x01640E4C, GSA_DLC_ITEM_PRELOAD
extern stream2_slot_allocator Stream_audio_slot_allocator; // 0x01640E58 (:672), GSA_AUDIO

// ---- boot
int rfg_pre_initialize_game(keen::GameFrameworkSystem* pKeenGameFrameworkSystem, keen::GraphicsCommandWriter* pGraphicsCommandWriter); // 0x005D9660 (:5553), 0 or -1
void rfg_init_keen_memory_allocators(keen::GameFrameworkSystem* pGameFramework); // 0x005D96B0 (:6595)
void rfg_init_stage_1_prepare(keen::GameFrameworkSystem* pKeenGameFrameworkSystem, keen::GraphicsCommandWriter* pGraphicsCommandWriter); // 0x005D8630 (:2840)
void rfg_update_resource_sizes(keen::GraphicsCommandWriter* pGraphicsCommandWriter); // 0x005D95B0 (:4107)
bool rfg_init_stage_1_loop_update(); // 0x005D2440 (:4159)
void rfg_init_stage_1_loop_render(keen::GraphicsCommandWriter* pCommandWriter); // 0x005D50E0 (:4173)
void rfg_init_stage_1_done(); // 0x005D5150 (:4188)
void stream2LoadInitFunction(); // 0x005D2460 (:4328)
void rfg_init_stage_2_prepare(); // 0x005D53E0 (:4334)
bool rfg_init_stage_2_loop_update(); // 0x005D54E0 (:4366)
void rfg_init_stage_2_loop_render(keen::GraphicsCommandWriter* pCommandWriter); // 0x005D5670 (:4409)
void rfg_init_stage_2_done(keen::SoundSystem::System* pSoundSystem); // 0x005D56A0 (:4418)
void rfg_init_stage_2_done_2(); // 0x005D2480 (:5200)
void sim_scratch_system_init(); // 0x005D2400 (:2762)
void rfg_shutdown_game(); // 0x005D6420 (:5656)

// ---- frame
bool rfg_do_frame(); // 0x005D2500 (:5273)
void do_game_render_frame(keen::GraphicsCommandWriter* pCommandWriter); // 0x005D2680 (:5431)
bool do_game_main_loop_frame(); // 0x005D2700 (:5461)

// ---- keen glue (setters called by keen::rfg::Application)
void rfg_set_SoundSystem(keen::SoundSystem::System* pSoundSystem); // 0x005D2DE0 (:6698)
void rfg_set_simulationThreadCommandWriter(keen::GraphicsCommandWriter* pCommandWriter); // 0x005D2DF0 (:6703)
void rfg_set_local_game_session(unit4::LocalGameSession* pGameSession); // 0x005D2D20 (:6580)
unit4::LocalGameSession* rfg_get_local_game_session(); // 0x005D2D30 (:6585)
bool rfg_is_gameframework_fully_in_foreground(); // 0x005D2D40 (:6590)
keen::MemoryAllocator* rfg_get_cpu_scratch_allocator(); // 0x005D2D60 (:6653)
keen::MemoryAllocator* rfg_get_gpu_scratch_allocator(); // 0x005D2D70 (:6658)
keen::MemoryAllocator* rfg_get_audio_scratch_allocator(); // 0x005D2D80 (:6663)
keen::MemoryAllocator* rfg_get_audio_bank_allocator(); // 0x005D2D90 (:6668)
void rfg_get_system_allocator_info(unsigned int& total_size, unsigned int& availalble_size); // 0x005D2DA0 (:6673)
const unsigned int* rfg_get_tracked_chunk_ids(); // 0x005D2DC0 (:6688)
unsigned int rfg_get_num_tracked_chunk_ids(); // 0x005D2DD0 (:6693)
void rfg_process_game_session_interaction_param(kaiko::GameSessionInteractionParameter& param); // 0x005D2BA0 (:6517)
void rfg_set_error_viewer_event_id(unsigned int event_id, const keen::UserAccountId& user, bool is_remote_game_session); // 0x005D2C00 (:6533)
void rfg_clear_error_viewer_event_id(); // 0x005D2C90 (:6541)
unsigned int rfg_get_error_viewer_event_id(); // 0x005D2CB0 (:6547)
bool rfg_get_error_viewer_last_event_was_for_remote_game_session(); // 0x005D2CC0 (:6552)
const keen::UserAccountId& rfg_get_error_viewer_user(); // 0x005D2CD0 (:6557)
void rfg_set_error_viewer_response(unsigned int response); // 0x005D2CE0 (:6562)
unsigned int rfg_get_error_viewer_response(); // 0x005D2D00 (:6568)
unsigned int rfg_get_currently_handled_error_viewer_event_id(); // 0x005D2D10 (:6573)
void rfg_shorten_display_string(const char* pString, char* pOutString, unsigned int outStringCapacity, unsigned int maxNumCharacters, bool useRfgEscapeSequences); // 0x005D2980 (:6331)
void rfg_reverse_escape_characters(wchar_t* pString); // 0x005D2B30 (:6479)

// ---- tables, packfiles and pegs
bool rfg_test_table_checksum(const unsigned char* data, unsigned int data_size); // 0x005D1CB0 (:1351)
void game_enable_table_precache(); // 0x005D4860 (:1398)
void game_disable_table_precache(); // 0x005D1CC0 (:1495)
bool game_table_precache_inited(); // 0x005D1D00 (:1510)
void rfg_reload_packfiles_for_audio_language_switch(keen::LanguageId previous_language); // 0x005D50B0 (:2345)
bool peg_load_wrapper(const char* fname, unsigned int srid, unsigned char* cpu_preload, int cpu_size, unsigned char* gpu_preload, int gpu_size); // 0x005D1F10 (:2000)
void peg_unload_wrapper(const char* fname, unsigned int srid); // 0x005D1F90 (:2009)
int game_load_peg(const char* fname, mempool* pool); // 0x005D2790 (:5716)
void game_unload_peg(const char* fname); // 0x005D2810 (:5724)
void game_find_peg_from_source(char* target, unsigned int target_size, const char* source); // 0x005D2880 (:5739)

// ---- memory
unsigned char* mempool_init_alloc(int bytes); // 0x005D8590 (:2132)
unsigned char* mempool_init_alloc_aligned(int bytes, int align); // 0x005D85B0 (:2137)
unsigned char* vram_mempool_init_alloc(int bytes); // 0x005D85D0 (:2142)
unsigned char* vram_mempool_init_alloc_aligned(int bytes, int align); // 0x005D85F0 (:2147)
unsigned char* audio_mempool_init_alloc_aligned(int bytes, int align); // 0x005D8610 (:2152)
mempool_base* get_this_context_virtual_mempool(); // 0x005D2160 (:2166)
void game_check_free_physical_mem(const char* where, bool report_untracked_only); // 0x005D1D90 (:1801)
void game_track_mem_usage_start(const char* where); // 0x005D1DB0 (:1866)
void game_track_mem_usage_end(bool quiet); // 0x005D1DD0 (:1905)
bool game_set_new_protect(bool on); // 0x005D2940 (:6033)

// ---- console callbacks (vconsole_config, see vconsole.h)
void update_console(vconsole_config& config); // 0x005D4810 (:1336)
void game_draw_console_rect(int x, int y, int w, int h, color& rect_color); // 0x005D1DE0 (:1931)
void game_draw_console_string(const char* str, int x, int y, int len, int& w, int& h, color& string_color); // 0x005D4CD0 (:1943)
void game_draw_console_line(int x1, int y1, int x2, int y2, color& line_color); // 0x005D1E10 (:1966)

// ---- misc
int CreateMiniDump(_EXCEPTION_POINTERS* ExceptionInfo); // 0x005D1A90 (:1275)
void unused_dcf_get_my_ip(); // 0x005D1A70 (:877)
void game_enable_standard_access(); // 0x005D2730 (:5501)
void game_disable_standard_access(); // 0x005D2760 (:5527)
void db_test(); // 0x005D2950 (:6044)
void memory_use_debug_graph_render(); // 0x005D2960 (:6289)
void rfg_debug_render(void* user_data_p, rl_renderer*); // 0x005D2970 (:6298)
