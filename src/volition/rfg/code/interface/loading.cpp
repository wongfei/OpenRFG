// Original: volition/rfg/code/interface/loading.cpp
//
// Partial (multiplayer.md MP1): the GS_LOAD state (gs_load_enter/process/exit/render, the level load job), the
// multiplayer switch states (gs_multi_switch_*, multi_init_job, multi_shutdown_job), loading_system_init (the state
// registration), the boot thread and the state switch requests (game_push_multi_init, game_set_*_shutdown, ...).
// orig_main.md OM3: the boot screen state (gs_boot_*) and its render thread frame (game_boot_thread_render); its vint
// document goes to the UI seam.
// Not ported (UI): the loading screen tables, images and UI data, the language selection, the DLC and Wrecking Crew
// switch states; the ones the ported functions call are external here (a seam, marked below) so the test programs
// stand in for them.
//
// rfg_interface_unity.obj is a unity build: the UI statics placed here (Controller_slots, Input_types,
// Utility_doc_handle, ...) belong to the interface files that use them first. msgbox_info_set (Message_box_info_set)
// belongs to ui_message_box.cpp, whose types are not recovered (UI), so it stays unplaced.
//
// The inlined UI helpers (an input id request, ui_relinquish_control_id, the loading screen's hide) are written as the
// helpers request_input_id and hide_loading_screen, or as the public call.

#include "volition/rfg/code/interface/loading.h"

#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_skin_instance.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_render_thread.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/gamestate_gameplay.h"
#include "volition/rfg/code/gamewide/gamewide.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/geom/cloth_simulation.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_bp.h"
#include "volition/rfg/code/hud/fullscreen_map.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_memory_manager.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/undergrowth.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_party.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/action_script/script_types.h"
#include "volition/rfg/code/video_player/video_player.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/cfile/packfile.h"
#include "volition/vlib/code/memory/page_pool.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/os.h"
#include "volition/vlib/code/os/scheduler.h"
#include "volition/vlib/code/os/thread.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include "keen/graphics/graphicscommandwriter.hpp"
#include "keen/graphics/graphicssystem.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/graphicscommandbuffer_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/rendertarget_d3d11.hpp"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_event.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/render_lib/code/rl_draw.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/util/string_util.h"
// stub_impl.md S7: the DLC and wrecking crew switches
#include "volition/rfg/code/gameplay/gameflow/squad_loadout.h"
#include "volition/rfg/code/gameplay/gameflow/squad_progression.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/hud/minimap_icon_info.h"
#include "volition/rfg/code/objects/gameplay_properties.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
// UI.md UI3: the loading screen data
#include "volition/interface_lib/code/vint_external.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/level/rfg_zonex_shared.h"
#include "volition/rfg/code/level/territory_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"

#include <string.h>

#include <stdio.h>
#include <stdlib.h>

// ---- file-private types (rfg.pdb); district_index_data is in loading.h and input_register_type in
// ui_document_manager.h since UI0

// input_request_data is in ui_document_manager.h since UI3.

// Placement inferred (a UI header that is not recovered may own it).
// size 0x8
struct selected_image_info
{
    int base_img_index; // 0x00
    int overlay_img_index; // 0x04
};

// Placement inferred (a UI header that is not recovered may own it).
enum sony_options_mode
{
    SONY_OPTIONS_LOGIN = 0,
    SONY_OPTIONS_SAVE = 1,
    SONY_OPTIONS_LOAD = 2,
    SONY_OPTIONS_DELETE = 3,
    SONY_OPTIONS_FRIENDS_UI = 4,
    SONY_OPTIONS_ADD_FRIEND_UI = 5,
    SONY_OPTIONS_APP_DEFINED_MSG = 6,
    SONY_OPTIONS_READ_MESSAGES = 7,
    NUM_SONY_OPTIONS_MODES = 8,
};

// ---- file-static globals
static const char LOADING_SCREEN_TIP_TABLE_FILE_NAME[25] = "loading_screen_tips.xtbl"; // 0x013005D4
static const char LOADING_SCREEN_TIP_ROOT_NAME_TAG[20] = "Loading_screen_tips"; // 0x013005F0
static const char LOADING_SCREEN_TITLE_TAG[5] = "Name"; // 0x01300604
static const char LOADING_SCREEN_TIP_TEXT_TAG[5] = "Text"; // 0x0130060C
static const char LOADING_SCREEN_TIP_ICON_TAG[5] = "Icon"; // 0x01300614
static const char LOADING_SCREEN_TIP_TYPE_TAG[5] = "Type"; // 0x0130061C
static const char LOADING_SCREEN_IMAGE_TABLE_FILE_NAME[20] = "loading_screen.xtbl"; // 0x01300624
static const char LOADING_SCREEN_IMAGE_ROOT_NAME_TAG[14] = "LoadingScreen"; // 0x01300638
static const char LOADING_SCREEN_IMAGE_BG_IMAGE_TAG[9] = "BG_image"; // 0x01300648
static const char LOADING_SCREEN_IMAGE_OVERLAY_PROB_TAG[13] = "Overlay_prob"; // 0x01300654
static const char LOADING_SCREEN_IMAGE_ROTATION_TAG[10] = "Image_rot"; // 0x01300664
static const char LOADING_SCREEN_IMAGE_NEWS_LAYOUT_TAG[12] = "News_layout"; // 0x01300670
static const char LOADING_SCREEN_IMAGE_LEVEL_LAYOUT_TAG[13] = "Level_layout"; // 0x0130067C
static const char LOADING_SCREEN_IMAGE_POSTER_LAYOUT_TAG[14] = "Poster_layout"; // 0x0130068C
static const char LOADING_SCREEN_IMAGE_FG_1_IMAGE_TAG[10] = "FG1_image"; // 0x0130069C
static const char LOADING_SCREEN_IMAGE_FG_2_IMAGE_TAG[10] = "FG2_image"; // 0x013006A8
static const char LOADING_SCREEN_IMAGE_FG_FILE_TAG[12] = "FG_filename"; // 0x013006B4
static const char LOADING_SCREEN_IMAGE_FG_ROT_TAG[7] = "FG_rot"; // 0x013006C0
static const char LOADING_SCREEN_IMAGE_TEXT_1_TAG[8] = "String1"; // 0x013006C8
static const char LOADING_SCREEN_IMAGE_TEXT_2_TAG[8] = "String2"; // 0x013006D0
static const char LOADING_SCREEN_IMAGE_TEXT_3_TAG[8] = "String3"; // 0x013006D8
static const char LOADING_SCREEN_IMAGE_TEXT_4_TAG[8] = "String4"; // 0x013006E0
static const char LOADING_SCREEN_IMAGE_TEXT_5_TAG[8] = "String5"; // 0x013006E8
static const char Init_load_image_name[16] = "ui_loading_init"; // 0x013006F0
static const char Init_load_text[13] = "MENU_LOADING"; // 0x01300700
static const char Init_load_install_text[16] = "MENU_INSTALLING"; // 0x01300710
static bool Async_level_load = true; // 0x01663022
// bool s_localization_is_loading (0x01663023) is used here too; placed in localization_shared.cpp.
static const char* UI_screen_level_loading = {"rfg_ui_loading"}; // 0x016630DC
static int Loading_doc_handle = -1; // 0x016630E0
static int Loading_input_id = -1; // 0x016630E4
static int Loading_data_group_handle = -1; // 0x016630E8
static int Loading_screen_msgbox = -1; // 0x016630EC
static float Loading_screen_news_image_prob = 0.06f; // 0x016630F0
static const char* UI_screen_boot = {"rfg_ui_init_load"}; // 0x016630F4
static int Boot_doc_handle = -1; // 0x016630F8
static unsigned int Boot_control_id = 1; // 0x016630FC
static int Boot_data_item_handle = -1; // 0x01663100
static const char* Boot_install_message = {"PLT_CACHE_MESSAGE"}; // 0x01663104
static int Boot_msgbox_handle = -1; // 0x01663108
static unsigned int Boot_render_thread = 0xFFFF; // 0x0166310C
game_state Gs_switch_next_state = GS_NONE; // 0x01663110, initial data ffffffff (external for ingame_options.cpp, OM7)
static const char* LOADING_SCREEN_TIP_TYPE_NAME[3] = {"Single Player", "Multiplayer", "Both"}; // 0x01663114
// keen::LanguageId s_keen_system_language_id (0x01663134) is used here too; placed in localization_shared.cpp.
// keen::LanguageId s_supported_languages[12] (0x01663138) is used here too; placed in localization_shared.cpp.
// A static of the interface unity build in rfg.exe, shared with ui_document_manager.cpp's ui_show_loading_screen and
// ui_remove_loading_screen; external here (loading.h), as s_localization_is_loading is (orig_main.md OM5/OM6).
int Loading_screen_handle = -1; // 0x01664B44
int Current_input_lock_id = -1; // 0x01664B4C
int Current_input_lock_index = -1; // 0x01664B50
bool Input_enabled_uimen = true; // 0x01664FD0
const char* cBlank_string = {""}; // 0x01664FF0
int Utility_doc_handle = -1; // 0x01664FF8
int Msg_box_data_item_handle = -1; // 0x01664FFC
bool Player_input_was_enabled = true; // 0x0166500C
bool Player_camera_input_was_enabled = true; // 0x0166500D
farray<loading_screen_tip_data,50> Tip_data; // 0x01665D88 (file static in the binary; external since UI5, the game lobby)
static farray<char const*,32> Loading_image_news_image_names; // 0x016660B8
farray<unsigned int,16> Controller_slots; // 0x01667378
farray<input_request_data,16> Input_types; // 0x016673C8
static char Loading_screen_string_data[9216]; // 0x02BE5960
static int scoreboard_hint_config[6]; // 0x02C00E54
static sony_options_mode Sony_options_mode; // 0x02C00E6C
static selected_image_info Loading_screen_images[3]; // 0x02C00E70
static volatile bool Gs_load_first_frame; // 0x02C034BE
static volatile bool Gs_load_in_progress; // 0x02C034BF
static volatile bool Gs_load_finished; // 0x02C034C4
static volatile bool Gs_load_setup_in_progress; // 0x02C034C5
static volatile bool Gs_load_setup_finished; // 0x02C034C6
static volatile bool Gs_load_success; // 0x02C034C7
static unsigned int Loading_control_id; // 0x02C034C8
static bool Loading_assets_loaded; // 0x02C034CC
static bool Loading_screen_delay_one_frame; // 0x02C034CD
static bool Boot_installing; // 0x02C034CE
static bool Boot_boot_failed; // 0x02C034CF
int Current_tip; // 0x02C034D0 (file static in the binary; external since UI5, the game lobby)
static unsigned int Num_loading_images; // 0x02C034D4
static int Boot_cpu_mark; // 0x02C034D8
static int Boot_gpu_mark; // 0x02C034DC
static bool Boot_language_peg_allocated; // 0x02C034E0
static bool Boot_has_set_ui_resolution; // 0x02C034E1
static volatile bool Boot_render_thread_exit; // 0x02C034E2
static volatile bool Gs_switch_first_frame; // 0x02C034E3
static float Boot_install_percent; // 0x02C034E4
static char Boot_install_time_left[16]; // 0x02C034E8
static volatile bool Gs_switch_finished; // 0x02C034F9
bool Gs_switch_transparent; // 0x02C034FA
bool Gs_switch_pause_beneath; // 0x02C034FB
static unsigned int Gs_switch_control_id; // 0x02C034FC
static virtual_mempool* Async_load_scratch_pool; // 0x02C03500
static unsigned int Loading_image_data_dlc_mark; // 0x02C03504
static bool Async_level_load_test_assert; // 0x02C03508
bool Fade_out; // 0x02C08097
int Loading_screen_ref_cnt; // 0x02C080A0, external as Loading_screen_handle
int Fade_ref; // 0x02C080A8
bool Paused_game; // 0x02C081AE
bool Locked_controls; // 0x02C081B4
static farray<loading_screen_layout,16> Loading_image_overlay_data; // 0x02C0EFF0
static farray<loading_screen_layout,128> Loading_image_data; // 0x02C3AA60
static string_pool Loading_screen_string_pool(Loading_screen_string_data, 0x2400, nullptr); // 0x02C4CAE0
static district_index_data Loading_screen_district; // 0x02C541D4
static keen::Mutex Loading_gameseq_multithreaded_mutex; // 0x02C7E374
// static msgbox_info_set Message_box_info_set; // 0x02CA0340 (its type is not recovered, see the file comment)

// Globals (not static) of the interface unity build placed here (placement inferred).
volatile bool Gs_switch_in_progress; // 0x02C034F8
bool min_pause; // 0x02C08090, the video players are paused while the window is minimized
// The input id counter of rfg.exe's interface unity (ui_register_for_processed_input, ui_show_loading_screen and the
// inlined request below share it); external for ui_document_manager.cpp.
int cur_id = -1; // 0x01668594

// ---- internal functions
// Seam (multiplayer.md MP1): the boot screen, DLC switch and loading screen UI functions below are file statics in
// rfg.exe; they are not ported (UI) and are external here so the test programs can stand in for them.
void boot_mempools_set_thread_ownership(); // 0x008885A0 (:431)
static int game_boot_thread(void*); // 0x008885B0 (:437)
bool handle_language_selection(); // 0x008F70F0 (:619)
void gs_boot_enter(game_state prev_state); // 0x00909A70 (:676), seam
void gs_boot_exit(game_state new_state); // 0x008F0B20 (:741), seam
void gs_boot_handshake(rl_renderer*); // 0x008DE330 (:776), seam
void gs_boot_process(); // 0x00912B40 (:781), seam
void gs_boot_render(rl_renderer* pRenderer); // 0x008F7330 (:842), seam
static loading_screen_tip_type loading_screen_lookup_tip_type(const char* name); // 0x00888730 (:871)
// LTCG: arguments in EAX (not mapped yet).
static bool loading_screen_parse_tip(xml_element* xml_root, loading_screen_tip_data& tip_def); // 0x00888760 (:884)
void loading_screen_parse_tips_table(); // 0x008BF650 (:919), seam
static bool loading_screen_parse_image(xml_element* xml_root, loading_screen_layout& image_data); // 0x00888830 (:945)
void loading_screen_initialize_ui_data(); // 0x008CEA00 (:1230), seam
void loading_screen_process_ui_data(); // 0x008F0C80 (:1308), seam
static void level_load_mempools_set_thread_ownership(); // 0x008AADE0 (:1421)
static void world_load_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2); // 0x008AAEE0 (:1470)
// LTCG: arguments in EAX (not mapped yet).
bool loading_screen_stream_image_data(loading_screen_layout* img_data, bool load); // 0x008AAF30 (:1502), seam
static int loading_screen_find_image_layout(const char* image_name); // 0x008AB020 (:1549)
void loading_screen_select_loading_images(); // 0x008AB090 (:1566), seam
static void gs_load_enter(game_state prev_state); // 0x00909C70 (:1830)
static void gs_load_exit(game_state new_state); // 0x008F1320 (:1901)
static void gs_load_process(); // 0x00912E70 (:1970)
static void gs_load_render(rl_renderer*); // 0x008F73B0 (:2198)
static void dlc_init_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2); // 0x008AB5F0 (:2244)
static void dlc_shutdown_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2); // 0x008AB680 (:2276)
void gs_dlc_switch_enter(game_state prev_state); // 0x0090A180 (:2342), seam
void gs_dlc_switch_exit(game_state new_state); // 0x008F1490 (:2364), seam
void gs_dlc_switch_process(); // 0x00913300 (:2377), seam
void gs_dlc_switch_handshake(rl_renderer*); // 0x00888D90 (:2451), seam
void gs_dlc_switch_render(rl_renderer*); // 0x008F73F0 (:2455), seam
static void multi_switch_mempools_set_thread_ownership(); // 0x008AB720 (:2467)
static void multi_init_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2); // 0x008AB7A0 (:2484)
static void multi_shutdown_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2); // 0x008AB960 (:2587)
static void gs_multi_switch_enter(game_state prev_state); // 0x0090A1B0 (:2606)
static void gs_multi_switch_exit(game_state new_state); // 0x008F1540 (:2642)
static void gs_multi_switch_process(); // 0x00913440 (:2662)
static void gs_multi_switch_handshake(rl_renderer*); // 0x00888DA0 (:2734)
static void gs_multi_switch_render(rl_renderer*); // 0x008F7410 (:2738)
static void wc_init_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2); // 0x008AB990 (:2749)
static void wc_shutdown_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2); // 0x008ABA00 (:2778)
void gs_wc_switch_enter(game_state prev_state); // 0x0090A260 (:2802), seam
void gs_wc_switch_exit(game_state new_state); // 0x008F1610 (:2824), seam
void gs_wc_switch_process(); // 0x00913580 (:2837), seam
void gs_wc_switch_handshake(rl_renderer*); // 0x00888DB0 (:2914), seam
void gs_wc_switch_render(rl_renderer*); // 0x008F7430 (:2918), seam

// The input lock id request of the UI (an inline helper whose name is not in the PDB; the counter cur_id starts at
// 1): the request is added to Input_types, and its id returned (-1 when the table is full).
static int request_input_id(bool locked, int* index)
{
    if (cur_id == -1)
    {
        cur_id = 1;
    }
    // Magic number: 16, the size of Input_types.
    if (Input_types.size() >= 16)
    {
        VLIB_CRITICAL_FAILURE();
        return -1;
    }
    input_request_data request;
    request.type = IRT_UNLIMITED_INPUT;
    request.locked = locked;
    request.id = cur_id;
    const int added = Input_types.add(request);
    if (index != nullptr)
    {
        *index = added;
    }
    return cur_id++;
}

// The loading screen box of ui_show_loading_screen's ref count (inlined ui_hide_loading_screen): every reference goes,
// and the box is hidden.
static void hide_loading_screen()
{
    while (Loading_screen_ref_cnt > 0)
    {
        Loading_screen_ref_cnt = 0;
        if (Loading_screen_handle != -1 && ui_is_msgbox_active(Loading_screen_handle))
        {
            ui_hide_msgbox(Loading_screen_handle, false);
        }
        Loading_screen_handle = -1;
    }
}

// 0x00888590
district_index_data::district_index_data()
{
    territory_index = 0;
    district_index = 0;
}

// 0x008AADE0 (:1421)
// The level load runs on a scheduler job (Async_level_load): the pools and systems it fills belong to that thread
// meanwhile.
static void level_load_mempools_set_thread_ownership()
{
    Common_mempool.set_thread_ownership();
    Level_mempool.set_thread_ownership();
    Stream_human_allocator.set_thread_ownership();
    Stream_player_allocator.set_thread_ownership();
    Stream_vehicle_allocator.set_thread_ownership();
    Stream_large_layer_allocator.set_thread_ownership();
    Stream_small_layer_allocator.set_thread_ownership();
    Stream_human_voice_persona_allocator.set_thread_ownership();
    Human_voice_always_loaded_pool.set_thread_ownership();
    Stream_large_weapon_allocator.set_thread_ownership();
    Stream_small_weapon_allocator.set_thread_ownership();
    Permanent_mempool.set_thread_ownership();
    script_mempools_set_thread_ownership();
    packfile_set_thread_ownership();
    Stream_interface_allocator.set_thread_ownership();
    Stream_interface_slot_allocator.set_thread_ownership();
    undergrowth_set_thread_ownership();
    vanim_set_thread_ownership();
    cloth_sim_set_thread_ownership();
    rfg_shape_set_thread_ownership();
    rfg_mover_set_thread_ownership();
    stream2_set_thread_ownership();
    audiolib_set_thread_ownership();
    World.set_thread_ownership();
    rl_skin_bone_manager_set_thread_ownership();
    havok_bpo_set_thread_ownership();
    player_profile_set_thread_ownership("level load");
    if (multi_game_interface_memory_initialized())
    {
        Stream_mp_interface_slot_allocator.set_thread_ownership();
    }
    if (wrecking_crew_is_active())
    {
        wrecking_crew_set_thread_ownership();
    }
}

// 0x008AAEE0 (:1470)
static void world_load_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2)
{
    havok_mark_for_write();
    scheduler_get_this_context_controller();
    level_load_mempools_set_thread_ownership();
    cf_open_lock(false);
    if (Async_level_load_test_assert)
    {
        Async_level_load_test_assert = false;
    }
    Gs_load_success = World.load_territory();
    cf_open_lock(true);
    havok_unmark_for_write();
    Gs_load_finished = true;
}

// 0x00909C70 (:1830)
// Partial: when the loading document cannot be loaded, rfg.exe shows a "MENU_LOADING" message box instead, built by
// inlined ui_message_box.cpp code over its file statics (Message_box_info_set); that is UI and not ported:
// VLIB_CRITICAL_FAILURE if it is reached.
static void gs_load_enter(game_state prev_state)
{
    Gs_load_first_frame = true;
    Loading_assets_loaded = false;
    Loading_screen_delay_one_frame = true;
    World.clear_load_aborted();
    ui_force_unloads_complete();
    Fade_ref++;
    if (Fade_ref == 1)
    {
        Fade_out = true;
    }
    game_render_thread_wait_for_prev_done(false);
    Loading_control_id = ui_request_control_id();
    Loading_doc_handle = ui_load_document(UI_screen_level_loading, DFIT_NONE, 0.0f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (Loading_doc_handle != -1)
    {
        Loading_screen_msgbox = -1;
    }
    else
    {
        Loading_assets_loaded = true;
        VLIB_CRITICAL_FAILURE(); // the loading message box, see the function comment
        Loading_screen_msgbox = -1;
    }
    Loading_input_id = request_input_id(false, nullptr);
    loading_screen_select_loading_images();
    // The loading images stream in (their peg containers).
    for (unsigned int i = 0; i < Num_loading_images; i++)
    {
        loading_screen_layout* base = &Loading_image_data[Loading_screen_images[i].base_img_index];
        if (base != nullptr && !loading_screen_stream_image_data(base, true))
        {
            VLIB_CRITICAL_FAILURE();
            break;
        }
        const int overlay = Loading_screen_images[i].overlay_img_index;
        if (overlay >= 0 && overlay < Loading_image_overlay_data.size() && !loading_screen_stream_image_data(&Loading_image_overlay_data[overlay], true))
        {
            VLIB_CRITICAL_FAILURE();
            break;
        }
    }
    gamemusic_interactive_stop();
}

// 0x008F1320 (:1901)
static void gs_load_exit(game_state new_state)
{
    game_render_thread_wait_for_prev_done(false);
    if (new_state == GS_SHUTDOWN)
    {
        World.abort_load();
        return;
    }
    for (unsigned int i = 0; i < Num_loading_images; i++)
    {
        loading_screen_layout* base = &Loading_image_data[Loading_screen_images[i].base_img_index];
        if (base != nullptr && !loading_screen_stream_image_data(base, false))
        {
            VLIB_CRITICAL_FAILURE();
        }
        const int overlay = Loading_screen_images[i].overlay_img_index;
        if (overlay >= 0 && overlay < Loading_image_overlay_data.size() && !loading_screen_stream_image_data(&Loading_image_overlay_data[overlay], false))
        {
            VLIB_CRITICAL_FAILURE();
            break;
        }
    }
    ui_unload_document(Loading_doc_handle, DFOT_NONE, 0.5f);
    if (Loading_screen_msgbox != -1)
    {
        ui_hide_msgbox(Loading_screen_msgbox, false);
    }
    ui_relinquish_control_id(Loading_control_id);
    Loading_control_id = 0;
    ui_relinquish_input_id(Loading_input_id);
    Loading_screen_district.territory_index = 0;
    Loading_screen_district.district_index = 0;
    Fade_ref = 0;
    Fade_out = false;
    Loading_input_id = -1;
}

// 0x00912E70 (:1970)
// The level load in frames: first the loading screen document and images stream in; then the territory loads, on a
// scheduler job (Async_level_load, the default) or here, and is set up (world::setup_territory) over the following
// frames; then GS_GAMEPLAY, or back to the main menu when the load failed or the gameplay is shutting down.
static void gs_load_process()
{
    gamemusic_update(true);
    if (Gs_load_first_frame)
    {
        Gs_load_first_frame = false;
    }
    else if (Loading_doc_handle == -1 || Loading_assets_loaded)
    {
        loading_screen_process_ui_data();
        if (!Async_level_load)
        {
            if (!Gs_load_finished)
            {
                if (!World.load_territory())
                {
                    gamestate_gp_exit(GS_NONE);
                    gameseq_set_state(GS_MAINMENU, true);
                }
                Gs_load_finished = true;
            }
            else if (!Gs_load_setup_finished)
            {
                Gs_load_setup_finished = World.setup_territory();
            }
            else
            {
                Gs_load_first_frame = false;
                Gs_load_in_progress = false;
                Gs_load_finished = false;
                Gs_load_setup_in_progress = false;
                Gs_load_setup_finished = false;
                fullscreen_map_init_streaming_info();
                if (!Gameplay_shutdown_in_progress)
                {
                    gameseq_set_state(GS_GAMEPLAY, true);
                }
                else
                {
                    gamestate_gp_exit(GS_NONE);
                    gameseq_set_state(GS_MAINMENU, true);
                }
            }
            if (Game_boot_exit_flag)
            {
                gameseq_set_state(GS_SHUTDOWN, true);
            }
        }
        else if (!Gs_load_in_progress)
        {
            // The load job: the render lib and Havok are released to it, with a 256 KB scratch buffer.
            rl_unlock();
            havok_unmark_for_write();
            // Magic number: 256 KB.
            Async_load_scratch_pool = Common_page_pool.get_virtual_mempool("Async Level Load Scratch Pool", 0x40000);
            cpu_job job;
            job.trigger = nullptr;
            job.flags = 0;
            job.scratch_buffer = nullptr;
            job.scratch_buffer_size = 0;
            job.thread_id = get_current_thread_id();
            job.job_func = world_load_job;
            job.type = ccjt_havok;
            job.scratch_buffer = (unsigned char*)Async_load_scratch_pool->alloc(0x40000, 0);
            job.scratch_buffer_size = 0x40000;
            scheduler_add_job(nullptr, &job, nullptr);
            Gs_load_in_progress = true;
            os_sleep(10);
        }
        else if (!Gs_load_finished)
        {
            os_sleep(10);
        }
        else if (!Gs_load_setup_in_progress)
        {
            level_load_mempools_set_thread_ownership();
            havok_mark_for_write();
            Async_load_scratch_pool->clear();
            Common_page_pool.release_virtual_mempool(Async_load_scratch_pool);
            Async_load_scratch_pool = nullptr;
            Gs_load_setup_in_progress = true;
        }
        else if (!Gs_load_setup_finished && Gs_load_success)
        {
            Gs_load_setup_finished = World.setup_territory();
        }
        else
        {
            Gs_load_first_frame = false;
            Gs_load_in_progress = false;
            Gs_load_finished = false;
            Gs_load_setup_in_progress = false;
            Gs_load_setup_finished = false;
            rl_lock();
            if (Game_boot_exit_flag)
            {
                gameseq_set_state(GS_SHUTDOWN, true);
            }
            else if (!Gs_load_success || Gameplay_shutdown_in_progress)
            {
                gamestate_gp_exit(GS_NONE);
                gameseq_set_state(GS_MAINMENU, true);
            }
            else
            {
                fullscreen_map_init_streaming_info();
                gameseq_set_state(GS_GAMEPLAY, true);
                main_menu_stop_title_music();
            }
        }
    }
    else
    {
        // The loading screen document and its images are still streaming.
        stream2_do_frame(true);
        Loading_assets_loaded = true;
        switch (ui_get_doc_asset_state(Loading_doc_handle))
        {
        case ST_UNLOADED:
            VLIB_CRITICAL_FAILURE();
            // falls through
        case ST_ERROR:
            ui_unload_document(Loading_doc_handle, DFOT_NONE, 0.5f);
            Loading_assets_loaded = true;
            Loading_doc_handle = -1;
            break;
        case ST_LOADING:
            Loading_assets_loaded = false;
            ui_process(Loading_control_id, false);
            return;
        case ST_LOADED:
            break;
        default:
            VLIB_CRITICAL_FAILURE();
            break;
        }
        if (Loading_assets_loaded)
        {
            for (int i = 0; i < (int)Num_loading_images; i++)
            {
                const loading_screen_layout* base = &Loading_image_data[Loading_screen_images[i].base_img_index];
                if (base == nullptr)
                {
                    continue;
                }
                // The images of the layout: the background, its foreground images, then the overlay's.
                const char* names[6];
                int count = 0;
                names[count++] = base->bg_image;
                for (int j = 0; j < base->num_fg_images; j++)
                {
                    names[count++] = base->fg_images[j].filename;
                }
                const loading_screen_layout* checked = base;
                const int overlay = Loading_screen_images[i].overlay_img_index;
                if (overlay >= 0 && overlay < Loading_image_overlay_data.size())
                {
                    checked = &Loading_image_overlay_data[overlay];
                    names[count++] = checked->bg_image;
                    for (int j = 0; j < checked->num_fg_images; j++)
                    {
                        names[count++] = checked->fg_images[j].filename;
                    }
                }
                // Notes (decompiled behaviour, kept as is): each image is checked by the last layout's background
                // container, not by its own name.
                for (int j = 0; j < count; j++)
                {
                    if (names[j] == nullptr || checked->bg_image == nullptr)
                    {
                        continue;
                    }
                    const unsigned int srid = stream2_container_find(checked->bg_image);
                    if (srid != 0)
                    {
                        const stream2_status_val status = stream2_get_status(srid);
                        if (status != ST_LOADED && status == ST_LOADING)
                        {
                            Loading_assets_loaded = false;
                            ui_process(Loading_control_id, false);
                            return;
                        }
                    }
                }
                if (!Loading_assets_loaded)
                {
                    break;
                }
            }
        }
        if (!Loading_assets_loaded)
        {
            ui_process(Loading_control_id, false);
            return;
        }
        loading_screen_initialize_ui_data();
        loading_screen_process_ui_data();
    }
    if (Loading_assets_loaded)
    {
        ui_add_for_processing(Loading_doc_handle);
    }
    ui_process(Loading_control_id, false);
}

// 0x008F73B0 (:2198)
static void gs_load_render(rl_renderer*)
{
    if (Loading_assets_loaded)
    {
        if (!Loading_screen_delay_one_frame)
        {
            ui_add_for_rendering(Loading_doc_handle);
        }
        Loading_screen_delay_one_frame = false;
    }
    ui_render(Loading_control_id, false);
}

// 0x008AB720 (:2467)
static void multi_switch_mempools_set_thread_ownership()
{
    Stream_human_voice_persona_allocator.set_thread_ownership();
    Human_voice_always_loaded_pool.set_thread_ownership();
    Effect_preload_mempool.set_thread_ownership();
    Stream_player_allocator.set_thread_ownership();
    packfile_set_thread_ownership();
    Stream_interface_allocator.set_thread_ownership();
    Stream_interface_slot_allocator.set_thread_ownership();
    stream2_set_thread_ownership();
    player_profile_set_thread_ownership("multi switch");
    Remote_file_cache.set_thread_ownership();
    if (multi_game_interface_memory_initialized())
    {
        Stream_mp_interface_slot_allocator.set_thread_ownership();
    }
}

// 0x008AB7A0 (:2484)
// Entering multiplayer, on a scheduler job: the profile settles, the tables are precached again, the game unpaused,
// the DLC bundles with multiplayer content mounted (their localization and interface), then multi_init (system link
// when the next state is the system link menu).
static void multi_init_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2)
{
    scheduler_get_this_context_controller();
    multi_switch_mempools_set_thread_ownership();
    const int controller = Player_controller_index != (unsigned int)-1 ? (int)Player_controller_index : 0;
    while (player_profile_busy(controller))
    {
        player_profile_process();
    }
    cf_open_lock(false);
    game_enable_table_precache();
    while (game_is_paused())
    {
        if (min_pause)
        {
            min_pause = false;
            video_player_unpause_all();
        }
        game_unpause();
    }
    bool interface_marked = false;
    dlc_get_bundles_sync();
    if (Use_packfiles)
    {
        game_enable_standard_access();
    }
    for (int i = 0; i < (int)dlc_count_callback(); i++)
    {
        if (dlc_bundle_contains_mp(i))
        {
            dlc_add_bundle(dlc_bundle_index_to_id(i), i);
        }
    }
    dlc_interface_shutdown();
    Level_mempool.clear();
    for (int i = 0; i < (int)dlc_count_callback(); i++)
    {
        if (!dlc_bundle_contains_mp(i))
        {
            continue;
        }
        const int id = dlc_bundle_index_to_id(i);
        dlc_localization_init(id);
        if (!interface_marked)
        {
            char project[256];
            sprintf(project, "dlc%02d_%s", id, "rfg.vint_proj");
            if (cf_exist(project))
            {
                dlc_interface_mark();
                interface_marked = true;
            }
        }
        dlc_interface_init(id);
    }
    if (Use_packfiles)
    {
        game_disable_standard_access();
    }
    multi_init(Gs_switch_next_state == GS_MULTIPLAYER_SYSLINK);
    game_disable_table_precache();
    cf_open_lock(true);
    Gs_switch_finished = true;
}

// 0x008AB960 (:2587)
static void multi_shutdown_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2)
{
    scheduler_get_this_context_controller();
    cf_open_lock(false);
    multi_switch_mempools_set_thread_ownership();
    multi_shutdown();
    cf_open_lock(true);
    Gs_switch_finished = true;
}

// 0x0090A1B0 (:2606)
static void gs_multi_switch_enter(game_state prev_state)
{
    ui_force_unloads_complete();
    Gs_switch_first_frame = true;
    Gs_switch_control_id = ui_request_control_id();
    hide_loading_screen();
    const game_state state = gameseq_get_state();
    const char* display_string = state == GS_MULTI_INIT ? "MENU_MSJS_JOIN_IN_PROGRESS" : (state == GS_MULTI_SHUTDOWN ? "MENU_DISCONNECT" : "MENU_LOADING");
    ui_show_loading_screen(false, display_string);
    if (gameseq_get_state() == GS_MULTI_SHUTDOWN)
    {
        multi_game_lobby_clear_pushed_to_game();
        multi_game_party_clear_left_party_by_choice();
    }
}

// 0x008F1540 (:2642)
static void gs_multi_switch_exit(game_state new_state)
{
    ui_relinquish_control_id(Gs_switch_control_id);
    Gs_switch_control_id = 0;
    if (!multi_core_platform_has_pending_invite() || !Multiplayer)
    {
        hide_loading_screen();
    }
}

// 0x00913440 (:2662)
// The switch runs multi_init_job (or multi_shutdown_job) on a scheduler job on its second frame and waits for it; then
// GS_MULTI_INIT is replaced by the next state (pushed with its flags), GS_MULTI_SHUTDOWN sets it.
static void gs_multi_switch_process()
{
    if (Gs_switch_first_frame)
    {
        Gs_switch_first_frame = false;
    }
    else if (Gs_switch_in_progress)
    {
        if (Gs_switch_finished)
        {
            Gs_switch_first_frame = false;
            Gs_switch_in_progress = false;
            Gs_switch_finished = false;
            multi_switch_mempools_set_thread_ownership();
            if (Game_boot_exit_flag)
            {
                gameseq_set_state(GS_SHUTDOWN, true);
                gamewide_shutdown();
                exit(0);
            }
            if (gameseq_get_state() == GS_MULTI_INIT)
            {
                gameseq_pop_state();
                gameseq_push_state(Gs_switch_next_state, Gs_switch_transparent, Gs_switch_pause_beneath);
            }
            else
            {
                gameseq_set_state(Gs_switch_next_state, false);
            }
        }
        else
        {
            os_sleep(10);
        }
    }
    else
    {
        cpu_job job;
        job.trigger = nullptr;
        job.flags = 0;
        job.scratch_buffer = nullptr;
        job.scratch_buffer_size = 0;
        job.thread_id = get_current_thread_id();
        job.type = ccjt_havok;
        const game_state state = gameseq_get_state();
        if (state == GS_MULTI_INIT)
        {
            dlc_clear_temp_srid();
            job.job_func = multi_init_job;
        }
        else if (state == GS_MULTI_SHUTDOWN)
        {
            dlc_clear_temp_srid();
            job.job_func = multi_shutdown_job;
        }
        else
        {
            // Notes (decompiled behaviour, kept as is): the job is added without a function then.
            keen::debug::nativePrintArgs("If we get here, we're doomed");
        }
        Gs_switch_in_progress = true;
        scheduler_add_job(nullptr, &job, nullptr);
        os_sleep(10);
    }
    ui_process(Gs_switch_control_id, false);
}

// 0x00888DA0 (:2734)
static void gs_multi_switch_handshake(rl_renderer*)
{
}

// 0x008F7410 (:2738)
static void gs_multi_switch_render(rl_renderer*)
{
    ui_render(Gs_switch_control_id, false);
}

// 0x009136D0 (:2930)
// The loading tables, then the switch states: GS_LOAD, GS_BOOT, GS_MULTI_INIT/_SHUTDOWN, GS_DLC_INIT/_SHUTDOWN and
// GS_WC_INIT/_SHUTDOWN, each opaque, streaming by itself (user flag 1) and the first two hiding the cursor.
void loading_system_init()
{
    Loading_gameseq_multithreaded_mutex.create("Loading system gameseq mutex");
    Loading_screen_string_pool.clear();
    loading_screen_parse_tips_table();
    loading_screen_parse_images_table(-1);
    state_info callbacks;
    callbacks.transparent = false;
    callbacks.pause_beneath = false;
    callbacks.user_flags = 1;
    callbacks.hide_cursor = true;
    callbacks.enter = gs_load_enter;
    callbacks.exit = gs_load_exit;
    callbacks.process = gs_load_process;
    callbacks.handshake = nullptr;
    callbacks.render = gs_load_render;
    gameseq_register_state(GS_LOAD, callbacks);
    callbacks.enter = gs_boot_enter;
    callbacks.exit = gs_boot_exit;
    callbacks.process = gs_boot_process;
    callbacks.handshake = gs_boot_handshake;
    callbacks.render = gs_boot_render;
    gameseq_register_state(GS_BOOT, callbacks);
    callbacks.enter = gs_multi_switch_enter;
    callbacks.exit = gs_multi_switch_exit;
    callbacks.process = gs_multi_switch_process;
    callbacks.handshake = gs_multi_switch_handshake;
    callbacks.render = gs_multi_switch_render;
    gameseq_register_state(GS_MULTI_INIT, callbacks);
    gameseq_register_state(GS_MULTI_SHUTDOWN, callbacks);
    callbacks.enter = gs_dlc_switch_enter;
    callbacks.exit = gs_dlc_switch_exit;
    callbacks.process = gs_dlc_switch_process;
    callbacks.handshake = gs_dlc_switch_handshake;
    callbacks.render = gs_dlc_switch_render;
    gameseq_register_state(GS_DLC_INIT, callbacks);
    gameseq_register_state(GS_DLC_SHUTDOWN, callbacks);
    callbacks.enter = gs_wc_switch_enter;
    callbacks.exit = gs_wc_switch_exit;
    callbacks.process = gs_wc_switch_process;
    callbacks.handshake = gs_wc_switch_handshake;
    callbacks.render = gs_wc_switch_render;
    gameseq_register_state(GS_WC_INIT, callbacks);
    gameseq_register_state(GS_WC_SHUTDOWN, callbacks);
}

// 0x00888DC0 (:3033)
void loading_system_shutdown()
{
    Loading_gameseq_multithreaded_mutex.destroy();
}

// 0x00888DD0 (:3039)
void game_boot_thread_start()
{
    Boot_render_thread = thread_spawn(game_boot_thread, nullptr, 0, THREAD_PRIOR_NORMAL, "Boot_render", 0);
}

// 0x00888DF0 (:3052)
void game_boot_thread_stop()
{
    Boot_render_thread_exit = true;
    // Deviation: the programs whose first boot stage is a stand-in (the headless programs on the original loop) never
    // start the boot screen thread; rfg.exe (and OpenRFG) always has it when rfg_init_stage_2_done_2 stops it.
    if (Boot_render_thread != 0xFFFF)
    {
        thread_wait_until_done(Boot_render_thread);
    }
    // Magic number: 0xFFFF, no thread.
    Boot_render_thread = 0xFFFF;
    Interface_mempool.set_thread_ownership();
    vint_set_thread_ownership();
}

// 0x00888E20 (:3062)
bool game_boot_thread_running()
{
    return Boot_render_thread != 0xFFFF;
}

// 0x008885B0 (:437)
// The boot screen thread: GS_BOOT's process step every frame until the boot ends.
static int game_boot_thread(void*)
{
    Interface_mempool.set_thread_ownership();
    vint_set_thread_ownership();
    gameseq_set_state(GS_BOOT, true);
    do
    {
        frametime_calculate(false, 0.0f, 0.0f);
        Loading_gameseq_multithreaded_mutex.lock(0);
        gameseq_process(GS_STEP_PROCESS, nullptr, true);
        Loading_gameseq_multithreaded_mutex.unlock();
    } while (!Boot_render_thread_exit);
    return 0;
}

// ---- orig_main.md OM3: the boot screen (GS_BOOT) and its frame on the render thread, for the program that runs the
// original boot (OpenRFG). Its document is vint (not ported): the UI layer loads it at once and gives no document info
// (ui_get_doc_info null), so Boot_screen_doc_loaded is set and the critical pegs and the "InitLoad" data item are
// skipped (orig_main.md 2.9). The test programs never start the boot screen thread and keep the seams above.

// A critical peg's file name: the resource name, with the platform's peg extension when it has none.
static void boot_critical_peg_name(char* name, const vint_critical_resources* resource)
{
    // Original: the resource name is the format string.
    _snprintf_safe(name, 0x80, resource->name);
    if (strstr(name, ".") == nullptr)
    {
        strcat(name, Peg_platform_ext[0]);
    }
}

// 0x008AAC40 (:500)
// The render thread's boot screen frame: clear the back buffer to black (depth 1, stencil 0), then the state machine's
// handshake and render steps under the boot thread's mutex.
void game_boot_thread_render(keen::GraphicsCommandWriter* pCommandWriter, rl_renderer* pRenderer)
{
    if (Boot_render_thread_exit)
    {
        return;
    }
    game_render_begin_frame();
    pRenderer->begin(pCommandWriter);
    rl_draw::get_instance()->set_renderer(pRenderer);
    keen::GraphicsCommandBuffer* pBuffer = pCommandWriter->getBuffer();
    ID3D11DepthStencilView* pDepthView = pBuffer->pCurrentRenderTarget->pDepthBufferView;
    if (pDepthView != nullptr)
    {
        pBuffer->pContext->ClearDepthStencilView(pDepthView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }
    const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    for (unsigned int i = 0; i < pBuffer->pCurrentRenderTarget->colorBufferCount; i++)
    {
        pBuffer->pContext->ClearRenderTargetView(pBuffer->pCurrentRenderTarget->renderTargetViews[i], black);
    }
    Loading_gameseq_multithreaded_mutex.lock(0);
    gameseq_process(GS_STEP_HANDSHAKE, pRenderer, true);
    gameseq_process(GS_STEP_RENDER, pRenderer, true);
    Loading_gameseq_multithreaded_mutex.unlock();
    pRenderer->end();
    game_render_end_frame();
    rl_draw::get_instance()->set_renderer(nullptr);
}

// 0x00909A70 (:676)
// The boot document; its critical pegs go into the interface pool (marked, so gs_boot_exit can drop them), and the
// "InitLoad" data item feeds it the loading text, the install progress and the language state.
void gs_boot_enter(game_state)
{
    Boot_doc_handle = ui_load_document(UI_screen_boot, DFIT_NONE, 0.5f, DLT_PERMANENTLY_LOAD, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    Boot_screen_doc_loaded = true;
    Boot_has_set_ui_resolution = false;
    vint_doc_info* pInfo = ui_get_doc_info(Boot_doc_handle);
    if (pInfo == nullptr || pInfo->m_doc == nullptr)
    {
        return;
    }
    vint_document* pDocument = pInfo->m_doc;
    // Magic number: 0x40, a split pool.
    if ((Interface_mempool.get_flags() & 0x40) == 0)
    {
        Boot_cpu_mark = Interface_mempool.static_mempool_base::mark();
        Boot_gpu_mark = 0;
    }
    else
    {
        Boot_cpu_mark = Interface_mempool.mark_cpu();
        Boot_gpu_mark = Interface_mempool.mark_gpu();
    }
    for (int which = 0; which < (int)pDocument->get_num_critical_resources(); which++)
    {
        const vint_critical_resources* pResource = pDocument->get_critical_resource(which);
        if (pResource != nullptr && pResource->type == VINT_CRIT_RSC_PEG)
        {
            char name[0x80];
            boot_critical_peg_name(name, pResource);
            if (!bm_load_peg(name, &Interface_mempool, true))
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "C:/unit4projects/rfg/root/code/volition/rfg/code/interface/loading.cpp", 0x2C4);
                return;
            }
        }
    }
    vint_data_item* pItem = vint_data_item::create_new("InitLoad");
    if (pItem != nullptr)
    {
        // The image, the loading text, installing, the install percentage, failed, the time left, the install message,
        // localization loading.
        const vint_variant_type types[8] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_FLOAT,
                                            VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL};
        pItem->set_template(types, 8);
        Boot_data_item_handle = (int)pItem->get_handle();
        ui_add_data_item(Boot_doc_handle, Boot_data_item_handle);
    }
}

// 0x008F0B20 (:741)
// The boot document's critical pegs are unloaded, the document goes and the interface pool returns to its mark.
void gs_boot_exit(game_state)
{
    vint_doc_info* pInfo = ui_get_doc_info(Boot_doc_handle);
    if (pInfo == nullptr || pInfo->m_doc == nullptr)
    {
        return;
    }
    vint_document* pDocument = pInfo->m_doc;
    for (int which = 0; which < (int)pDocument->get_num_critical_resources(); which++)
    {
        const vint_critical_resources* pResource = pDocument->get_critical_resource(which);
        if (pResource != nullptr && pResource->type == VINT_CRIT_RSC_PEG)
        {
            char name[0x80];
            boot_critical_peg_name(name, pResource);
            bm_unload_peg(name);
        }
    }
    ui_unload_document(Boot_doc_handle, DFOT_NONE, 0.0f);
    if ((Interface_mempool.get_flags() & 0x40) != 0)
    {
        Interface_mempool.restore_to_mark_cpu(Boot_cpu_mark);
        Interface_mempool.restore_to_mark_gpu(Boot_gpu_mark);
        return;
    }
    Interface_mempool.split_mempool::restore_to_mark(Boot_cpu_mark);
}

// 0x008DE330 (:776)
void gs_boot_handshake(rl_renderer*)
{
    ui_handshake();
}

// 0x008BF5D0 (:519)
// The language box's answer: item 0 is the system language, the others count the supported languages that are not
// selectable. Original: every supported language is selectable on PC (both tests read the same table), so only item 0
// can be chosen.
bool language_select_msg_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Boot_msgbox_handle != mbox_handle || choice != MBC_OK)
    {
        return false;
    }
    keen::LanguageId language = (keen::LanguageId)~(int)keen::LanguageId_En_En;
    if (selection_index == 0)
    {
        language = localization_auto_detect_language();
    }
    else
    {
        // Magic number: 0x31, the number of languages.
        for (int l = keen::LanguageId_En_En; l < 0x31; l++)
        {
            if (localization_is_language_supported((keen::LanguageId)l) && !localization_is_language_selectable((keen::LanguageId)l))
            {
                selection_index--;
                if (selection_index == 0)
                {
                    language = (keen::LanguageId)l;
                    break;
                }
            }
        }
    }
    User_selected_language = language;
    Boot_msgbox_handle = -1;
    return true;
}

// 0x00888620 (:568)
// The language's name image in interface-languages (Japanese and Arabic drawn larger).
const char* get_language_img_name(float& scale, keen::LanguageId language)
{
    scale = 1.0f;
    switch (language)
    {
    case keen::LanguageId_En_En:
    case keen::LanguageId_En_Us:
        return "ui_english";
    case keen::LanguageId_De_De:
        return "ui_german";
    case keen::LanguageId_Fr_Fr:
        return "ui_french";
    case keen::LanguageId_It_It:
        return "ui_italian";
    case keen::LanguageId_Es_Es:
        return "ui_spanish";
    case keen::LanguageId_Pl_Pl:
        return "ui_polish";
    case keen::LanguageId_Ru_Ru:
        return "ui_russian";
    case keen::LanguageId_Ja_Jp:
        // Magic numbers: the names' scales.
        scale = 1.2f;
        return "ui_japanese";
    case keen::LanguageId_Cs_Cz:
        return "ui_czech";
    case keen::LanguageId_Ar_Eg:
        scale = 1.35f;
        return "ui_arabic";
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return nullptr;
    }
}

// 0x008F70F0 (:619)
// Once the profiles are read and ask for it (never on PC, UI.md UI0): the language names' peg (without it, the system
// language) and a box with the system language first, then each supported language that is not selectable (none on
// PC, see language_select_msg_cb).
bool handle_language_selection()
{
    if (!Profiles_read || !Show_language_selection)
    {
        return false;
    }
    char peg[0x80] = "interface-languages";
    strcat(peg, Peg_platform_ext[0]);
    if (!bm_load_peg(peg, &Interface_mempool, true))
    {
        keen::debug::nativePrintArgs("Could not allocate the language names PEG");
        User_selected_language = localization_auto_detect_language();
        return true;
    }
    Boot_language_peg_allocated = true;
    Boot_msgbox_handle = ui_add_msgbox(MBT_ACCEPT_SELECT, "", "", language_select_msg_cb, false, false, nullptr, nullptr, false);
    float scale = 1.0f;
    char caption[0x80];
    const char* image = get_language_img_name(scale, localization_auto_detect_language());
    _snprintf_safe(caption, sizeof(caption), "[scale:%f][image:%s]", (double)scale, image);
    ui_msgbox_add_caption_item(Boot_msgbox_handle, caption, false, true);
    for (int l = keen::LanguageId_En_En; l < 0x31; l++)
    {
        if (localization_is_language_supported((keen::LanguageId)l) && !localization_is_language_selectable((keen::LanguageId)l))
        {
            image = get_language_img_name(scale, (keen::LanguageId)l);
            _snprintf_safe(caption, sizeof(caption), "[scale:%f][image:%s]", (double)scale, image);
            ui_msgbox_add_caption_item(Boot_msgbox_handle, caption, false, true);
        }
    }
    return true;
}

// 0x00912B40 (:781)
// Until a language is selected, the language selection message box; then the language pegs go. The data item gets the
// loading text (and the UI the screen size once), the install state and the localization state.
void gs_boot_process()
{
    // Magic number: 0x31, the number of languages.
    if ((int)User_selected_language < 0x31 && User_selected_language != (keen::LanguageId)~(int)keen::LanguageId_En_En)
    {
        if (Boot_language_peg_allocated)
        {
            char name[0x80] = "interface-languages";
            strcat(name, Peg_platform_ext[0]);
            bm_unload_peg(name);
            Boot_language_peg_allocated = false;
        }
    }
    else if (Boot_msgbox_handle == -1 && !handle_language_selection())
    {
        os_sleep(10);
    }
    vint_data_item* pItem = vint_data_item::find((unsigned int)Boot_data_item_handle);
    if (pItem != nullptr)
    {
        const char* text = Boot_installing ? Init_load_install_text : Init_load_text;
        pItem->set_element(0, vint_variant(Init_load_image_name));
        if (!s_localization_is_loading)
        {
            pItem->set_element(1, vint_variant(text));
            if (!Boot_has_set_ui_resolution)
            {
                const keen::RenderTarget* pBackBuffer = keen::graphics::getBackBufferRenderTarget(rl_get_graphics_system());
                const unsigned int height = keen::graphics::getRenderTargetHeight(pBackBuffer);
                const unsigned int width = keen::graphics::getRenderTargetWidth(keen::graphics::getBackBufferRenderTarget(rl_get_graphics_system()));
                ui_update_resolution((int)width, (int)height);
                Boot_has_set_ui_resolution = true;
            }
        }
        pItem->set_element(2, vint_variant(Boot_installing));
        pItem->set_element(3, vint_variant(Boot_install_percent));
        pItem->set_element(4, vint_variant(Boot_boot_failed));
        pItem->set_element(5, vint_variant((const char*)Boot_install_time_left));
        pItem->set_element(6, vint_variant(Boot_install_message));
        pItem->set_element(7, vint_variant((bool)s_localization_is_loading));
    }
    ui_add_for_processing(Boot_doc_handle);
    ui_process(Boot_control_id, false);
    frontend_music_update();
}

// 0x008F7330 (:842)
// The boot document over the cleared screen (the primitive state's blend and depth bits set for 2D, then restored).
void gs_boot_render(rl_renderer* pRenderer)
{
    const rl_primitive_state saved = pRenderer->get_macro_state();
    rl_primitive_state state = saved;
    // Magic numbers: the low bits of the state (base pass: 0x05 with bits 0x1A cleared; otherwise 0x02 with 0x1D).
    if (pRenderer->get_is_base_pass())
    {
        state.set_bits((state.get_bits() & 0xFFFFFFE5) | 5);
    }
    else
    {
        state.set_bits((state.get_bits() & 0xFFFFFFE2) | 2);
    }
    pRenderer->set_macro_state(state, false);
    ui_add_for_rendering(Boot_doc_handle);
    ui_render(Boot_control_id, false);
    pRenderer->set_macro_state(saved, false);
}

// 0x008882D0 (.h:89)
loading_screen_layout::loading_screen_layout()
{
    name = nullptr;
    name_crc = 0;
    layout_type = LSLT_NONE;
    overlay_prob = 0;
    rotation = 0.0f;
    bg_image = nullptr;
    string1 = nullptr;
    string2 = nullptr;
    string3 = nullptr;
    string4 = nullptr;
    string5 = nullptr;
    num_fg_images = 0;
    fg_images[0].filename = nullptr;
    fg_images[0].rotation = 0.0f;
    fg_images[1].filename = nullptr;
    fg_images[1].rotation = 0.0f;
}

// stub_impl.md S7: the DLC and wrecking crew switches. Placement of the two globals inferred; the DLC tables and the
// switches read them.
int Num_downloaded_levels; // 0x02C08190
dlc_level_info* Downloaded_levels; // 0x02BF2170

// Port helper: the profile's work finishes first, as rfg.exe inlines it in the init jobs.
static void loading_wait_for_player_profile()
{
    while (player_profile_busy(Player_controller_index != 0xFFFFFFFFu ? (int)Player_controller_index : 0))
    {
        player_profile_process();
    }
}

// 0x008AB5F0 (:2244)
static void dlc_init_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2)
{
    scheduler_get_this_context_controller();
    loading_wait_for_player_profile(); // inlined
    level_load_mempools_set_thread_ownership();
    cf_open_lock(false);
    dlc_free_single_player_levels();
    Num_downloaded_levels = 0;
    dlc_get_bundles_sync();
    dlc_get_single_player_levels(Num_downloaded_levels, Downloaded_levels);
    cf_open_lock(true);
    Gs_switch_finished = true;
}

// 0x008AB680 (:2276)
// Everything a DLC changed goes back to the base game's tables.
static void dlc_shutdown_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2)
{
    scheduler_get_this_context_controller();
    level_load_mempools_set_thread_ownership();
    cf_open_lock(false);
    dlc_free_single_player_levels();
    Num_downloaded_levels = 0;
    dlc_set_current_id(-1);
    dlc_interface_shutdown();
    loading_screen_dlc_restore();
    dlc_items_init(-1);
    squad_loadout_system_init(-1);
    squad_progression_system_init(-1);
    tweak_table_load(-1);
    dlc_anim_init(-1);
    dlc_upgrade_shutdown();
    gpm_dlc_shutdown();
    stats_system_init(-1);
    mission_unlock_load_table(-1);
    unlock_default_missions();
    minimap_load_icon_table(-1);
    parse_melee_table(-1, false);
    fullscreen_map_terr_info_init(-1);
    cf_open_lock(true);
    Gs_switch_finished = true;
}

// Port helper: the exit of the DLC and wrecking crew switches. rfg.exe inlines the control id's release and one
// reference of the loading screen in both.
static void gs_switch_release_control_and_loading_screen()
{
    for (int i = 0; i < Controller_slots.size(); i++)
    {
        if (Controller_slots[i] == Gs_switch_control_id)
        {
            Controller_slots.remove(i);
            break;
        }
    }
    Gs_switch_control_id = 0;
    if (Loading_screen_ref_cnt > 0)
    {
        Loading_screen_ref_cnt--;
        if (Loading_screen_ref_cnt < 1)
        {
            if (Loading_screen_handle != -1 && ui_is_msgbox_active(Loading_screen_handle))
            {
                ui_hide_msgbox(Loading_screen_handle, false);
            }
            Loading_screen_handle = -1;
        }
    }
}

// 0x0090A180 (:2342)
void gs_dlc_switch_enter(game_state prev_state)
{
    Gs_switch_first_frame = true;
    Gs_switch_control_id = ui_request_control_id();
    gameseq_get_state(); // Original: the state is read and not used.
    ui_show_loading_screen(false, "MENU_LOADING");
}

// 0x008F1490 (:2364)
void gs_dlc_switch_exit(game_state new_state)
{
    gs_switch_release_control_and_loading_screen(); // inlined
}

// 0x00913300 (:2377)
// As gs_multi_switch_process, with the DLC jobs.
void gs_dlc_switch_process()
{
    if (Gs_switch_first_frame)
    {
        Gs_switch_first_frame = false;
    }
    else if (Gs_switch_in_progress)
    {
        if (Gs_switch_finished)
        {
            level_load_mempools_set_thread_ownership();
            Gs_switch_first_frame = false;
            Gs_switch_in_progress = false;
            Gs_switch_finished = false;
            if (Game_boot_exit_flag)
            {
                gameseq_set_state(GS_SHUTDOWN, true);
                gamewide_shutdown();
                exit(0);
            }
            if (gameseq_get_state() == GS_DLC_INIT)
            {
                gameseq_pop_state();
                gameseq_push_state(Gs_switch_next_state, Gs_switch_transparent, Gs_switch_pause_beneath);
            }
            else
            {
                gameseq_set_state(Gs_switch_next_state, false);
            }
        }
        else
        {
            os_sleep(10);
        }
    }
    else
    {
        cpu_job job;
        job.trigger = nullptr;
        job.flags = 0;
        job.scratch_buffer = nullptr;
        job.scratch_buffer_size = 0;
        job.thread_id = get_current_thread_id();
        job.type = ccjt_havok;
        const game_state state = gameseq_get_state();
        if (state == GS_DLC_INIT)
        {
            dlc_clear_temp_srid();
            job.job_func = dlc_init_job;
        }
        else if (state == GS_DLC_SHUTDOWN)
        {
            dlc_clear_temp_srid();
            job.job_func = dlc_shutdown_job;
        }
        else
        {
            // Notes (decompiled behaviour, kept as is): the job is added without a function then.
            keen::debug::nativePrintArgs("If we get here, we're doomed");
        }
        Gs_switch_in_progress = true;
        scheduler_add_job(nullptr, &job, nullptr);
        os_sleep(10);
    }
    ui_process(Gs_switch_control_id, false);
}

// 0x00888D90 (:2451)
void gs_dlc_switch_handshake(rl_renderer*)
{
}

// 0x008F73F0 (:2455)
void gs_dlc_switch_render(rl_renderer*)
{
    ui_render(Gs_switch_control_id, false);
}

// 0x008AB990 (:2749)
static void wc_init_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2)
{
    scheduler_get_this_context_controller();
    level_load_mempools_set_thread_ownership();
    loading_wait_for_player_profile(); // inlined
    cf_open_lock(false);
    dlc_get_bundles_sync();
    wrecking_crew_init();
    cf_open_lock(true);
    Gs_switch_finished = true;
}

// 0x008ABA00 (:2778)
static void wc_shutdown_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2)
{
    scheduler_get_this_context_controller();
    level_load_mempools_set_thread_ownership();
    cf_open_lock(false);
    dlc_exit_wc();
    wrecking_crew_shutdown();
    cf_open_lock(true);
    Gs_switch_finished = true;
}

// 0x0090A260 (:2802)
void gs_wc_switch_enter(game_state prev_state)
{
    Gs_switch_first_frame = true;
    Gs_switch_control_id = ui_request_control_id();
    gameseq_get_state(); // Original: the state is read and not used.
    ui_show_loading_screen(false, "MENU_LOADING");
}

// 0x008F1610 (:2824)
void gs_wc_switch_exit(game_state new_state)
{
    gs_switch_release_control_and_loading_screen(); // inlined
}

// 0x00913580 (:2837)
// As gs_multi_switch_process, with the wrecking crew jobs; the stream thread's ownership comes back too.
void gs_wc_switch_process()
{
    if (Gs_switch_first_frame)
    {
        Gs_switch_first_frame = false;
    }
    else if (Gs_switch_in_progress)
    {
        if (Gs_switch_finished)
        {
            level_load_mempools_set_thread_ownership();
            Gs_switch_first_frame = false;
            Gs_switch_in_progress = false;
            Gs_switch_finished = false;
            stream2_set_thread_ownership();
            if (Game_boot_exit_flag)
            {
                gameseq_set_state(GS_SHUTDOWN, true);
                gamewide_shutdown();
                exit(0);
            }
            if (gameseq_get_state() == GS_WC_INIT)
            {
                gameseq_pop_state();
                gameseq_push_state(Gs_switch_next_state, Gs_switch_transparent, Gs_switch_pause_beneath);
            }
            else
            {
                gameseq_set_state(Gs_switch_next_state, false);
            }
        }
        else
        {
            os_sleep(10);
        }
    }
    else
    {
        cpu_job job;
        job.trigger = nullptr;
        job.flags = 0;
        job.scratch_buffer = nullptr;
        job.scratch_buffer_size = 0;
        job.thread_id = get_current_thread_id();
        job.type = ccjt_havok;
        const game_state state = gameseq_get_state();
        if (state == GS_WC_INIT)
        {
            dlc_clear_temp_srid();
            job.job_func = wc_init_job;
        }
        else if (state == GS_WC_SHUTDOWN)
        {
            dlc_clear_temp_srid();
            job.job_func = wc_shutdown_job;
        }
        else
        {
            // Notes (decompiled behaviour, kept as is): the job is added without a function then.
            keen::debug::nativePrintArgs("If we get here, we're doomed");
        }
        Gs_switch_in_progress = true;
        scheduler_add_job(nullptr, &job, nullptr);
        os_sleep(10);
    }
    ui_process(Gs_switch_control_id, false);
}

// 0x00888DB0 (:2914)
void gs_wc_switch_handshake(rl_renderer*)
{
}

// 0x008F7430 (:2918)
void gs_wc_switch_render(rl_renderer*)
{
    ui_render(Gs_switch_control_id, false);
}

// 0x00888E30 (:3068)
void game_push_multi_init(game_state next_state, bool transparent, bool pause_beneath)
{
    if (Multiplayer)
    {
        gameseq_push_state(next_state, transparent, pause_beneath);
        return;
    }
    Gs_switch_next_state = next_state;
    Gs_switch_transparent = transparent;
    Gs_switch_pause_beneath = pause_beneath;
    gameseq_push_state(GS_MULTI_INIT, true, true);
}

// 0x00888E80 (:3082)
void game_set_multi_shutdown(game_state next_state)
{
    if (!Multiplayer)
    {
        gameseq_set_state(next_state, false);
        return;
    }
    if (gameseq_get_state() != GS_LOAD && gameseq_get_pending_state() != GS_LOAD)
    {
        Gs_switch_next_state = next_state;
        gameseq_set_state(GS_MULTI_SHUTDOWN, true);
    }
}

// 0x00888ED0 (:3103)
void game_push_dlc_init(game_state next_state, bool transparent, bool pause_beneath)
{
    Gs_switch_next_state = next_state;
    Gs_switch_transparent = transparent;
    Gs_switch_pause_beneath = pause_beneath;
    gameseq_push_state(GS_DLC_INIT, true, true);
}

// 0x00888F10 (:3112)
void game_set_dlc_shutdown(game_state next_state)
{
    if (gameseq_get_state() != GS_LOAD && gameseq_get_pending_state() != GS_LOAD)
    {
        Gs_switch_next_state = next_state;
        gameseq_set_state(GS_DLC_SHUTDOWN, true);
    }
}

// 0x00888F40 (:3124)
void game_push_wc_init(game_state next_state, bool transparent, bool pause_beneath)
{
    Gs_switch_next_state = next_state;
    Gs_switch_transparent = transparent;
    Gs_switch_pause_beneath = pause_beneath;
    gameseq_push_state(GS_WC_INIT, true, true);
}

// 0x00888F80 (:3133)
void game_set_wc_shutdown(game_state next_state)
{
    if (gameseq_get_state() != GS_LOAD && gameseq_get_pending_state() != GS_LOAD)
    {
        Gs_switch_next_state = next_state;
        gameseq_set_state(GS_WC_SHUTDOWN, true);
    }
}

// ---- UI.md UI3: the loading screen's data (tips, image layouts, the data group of rfg_ui_loading) and the boot
// screen's install setters.

// 0x00888730 (:871)
// LTCG: the name in EDI.
static loading_screen_tip_type loading_screen_lookup_tip_type(const char* name)
{
    for (int i = LSTT_SINGLE_PLAYER; i < LSTT_NUM_TYPES; i++)
    {
        if (v_stricmp(LOADING_SCREEN_TIP_TYPE_NAME[i], name) == 0)
        {
            return (loading_screen_tip_type)i;
        }
    }
    return LSTT_INVALID;
}

// 0x00888760 (:884)
// LTCG: xml_root in EAX. A tip's title, text, icon and type. Original bug kept: an unknown type (-1) is accepted.
static bool loading_screen_parse_tip(xml_element* xml_root, loading_screen_tip_data& tip_def)
{
    if (xml_root == nullptr)
    {
        return false;
    }
    xml_element* element = xml_find(xml_root, LOADING_SCREEN_TITLE_TAG);
    if (element == nullptr || (tip_def.title = Loading_screen_string_pool.add_unique(element->text)) == nullptr)
    {
        return false;
    }
    element = xml_find(xml_root, LOADING_SCREEN_TIP_TEXT_TAG);
    if (element == nullptr || (tip_def.text = Loading_screen_string_pool.add_unique(element->text)) == nullptr)
    {
        return false;
    }
    element = xml_find(xml_root, LOADING_SCREEN_TIP_ICON_TAG);
    if (element == nullptr || (tip_def.icon_image = Loading_screen_string_pool.add_unique(element->text)) == nullptr)
    {
        return false;
    }
    element = xml_find(xml_root, LOADING_SCREEN_TIP_TYPE_TAG);
    if (element == nullptr)
    {
        return false;
    }
    tip_def.type = loading_screen_lookup_tip_type(element->text);
    return tip_def.type != LSTT_NUM_TYPES;
}

// 0x008BF650 (:919)
void loading_screen_parse_tips_table()
{
    xml_element* root = xml_table_parse(LOADING_SCREEN_TIP_TABLE_FILE_NAME);
    for (xml_element* cur = xml_find(root, LOADING_SCREEN_TIP_ROOT_NAME_TAG); cur != nullptr && Tip_data.size() < Tip_data.capacity();
         cur = xml_find_next(root, cur, LOADING_SCREEN_TIP_ROOT_NAME_TAG))
    {
        loading_screen_tip_data tip;
        if (loading_screen_parse_tip(cur, tip))
        {
            Tip_data.add(tip);
        }
    }
    xml_table_close(root);
}

// A layout's image name (the extension dropped) in the string pool.
static char* loading_screen_image_name(const xml_element* filename)
{
    char name[256];
    cf_drop_ext(name, sizeof(name), filename->text);
    return Loading_screen_string_pool.add_unique(name);
}

// A foreground image of a layout: FG_filename (an element with the name as its first child) and FG_rot (degrees,
// not converted, original).
static bool loading_screen_parse_fg_image(xml_element* fg, loading_screen_layout& image_data)
{
    xml_element* filename = xml_find(fg, LOADING_SCREEN_IMAGE_FG_FILE_TAG);
    if (filename == nullptr || filename->elements == nullptr)
    {
        return false;
    }
    loading_screen_image& image = image_data.fg_images[image_data.num_fg_images];
    image.filename = loading_screen_image_name(filename->elements);
    if (image.filename == nullptr)
    {
        return false;
    }
    xml_element* rotation = xml_find(fg, LOADING_SCREEN_IMAGE_FG_ROT_TAG);
    if (rotation == nullptr)
    {
        return false;
    }
    image.rotation = xml_get_float(rotation, 0.0f);
    image_data.num_fg_images++;
    return true;
}

// An optional string of a layout (null when missing, false when it cannot be stored).
static bool loading_screen_parse_string(xml_element* layout, const char* tag, char*& out)
{
    xml_element* element = xml_find(layout, tag);
    out = nullptr;
    if (element != nullptr)
    {
        out = Loading_screen_string_pool.add_unique(element->text);
        if (out == nullptr)
        {
            return false;
        }
    }
    return true;
}

// 0x00888830 (:945)
// LTCG: image_data in ESI. A loading screen image: name, background, overlay probability (0..100), rotation, and one
// layout: news (two foreground images, five strings), level name (one string) or poster (one foreground image, three
// strings, no overlay). An image with an overlay probability has no foreground images.
static bool loading_screen_parse_image(xml_element* xml_root, loading_screen_layout& image_data)
{
    if (xml_root == nullptr)
    {
        return false;
    }
    xml_element* element = xml_find(xml_root, LOADING_SCREEN_TITLE_TAG);
    if (element == nullptr || (image_data.name = Loading_screen_string_pool.add_unique(element->text)) == nullptr)
    {
        return false;
    }
    image_data.name_crc = crc_stri(image_data.name, 0);
    element = xml_find(xml_root, LOADING_SCREEN_IMAGE_BG_IMAGE_TAG);
    if (element == nullptr || element->elements == nullptr || (image_data.bg_image = loading_screen_image_name(element->elements)) == nullptr)
    {
        return false;
    }
    element = xml_find(xml_root, LOADING_SCREEN_IMAGE_OVERLAY_PROB_TAG);
    if (element == nullptr)
    {
        return false;
    }
    image_data.overlay_prob = xml_get_int(element, 0);
    // Magic number: 100 %.
    if (image_data.overlay_prob < 0 || image_data.overlay_prob > 100)
    {
        return false;
    }
    element = xml_find(xml_root, LOADING_SCREEN_IMAGE_ROTATION_TAG);
    if (element == nullptr)
    {
        return false;
    }
    // Magic number: degrees to radians as rfg.exe rounds it.
    image_data.rotation = xml_get_float(element, 0.0f) * 0.017453f;
    xml_element* layout = xml_find(xml_root, LOADING_SCREEN_IMAGE_NEWS_LAYOUT_TAG);
    if (layout != nullptr)
    {
        image_data.layout_type = LSLT_NEWS_BROADCAST;
        image_data.num_fg_images = 0;
        xml_element* fg = xml_find(layout, LOADING_SCREEN_IMAGE_FG_1_IMAGE_TAG);
        if (fg != nullptr && !loading_screen_parse_fg_image(fg, image_data))
        {
            return false;
        }
        fg = xml_find(layout, LOADING_SCREEN_IMAGE_FG_2_IMAGE_TAG);
        if (fg != nullptr && !loading_screen_parse_fg_image(fg, image_data))
        {
            return false;
        }
        if (!loading_screen_parse_string(layout, LOADING_SCREEN_IMAGE_TEXT_1_TAG, image_data.string1) ||
            !loading_screen_parse_string(layout, LOADING_SCREEN_IMAGE_TEXT_2_TAG, image_data.string2) ||
            !loading_screen_parse_string(layout, LOADING_SCREEN_IMAGE_TEXT_3_TAG, image_data.string3) ||
            !loading_screen_parse_string(layout, LOADING_SCREEN_IMAGE_TEXT_4_TAG, image_data.string4) ||
            !loading_screen_parse_string(layout, LOADING_SCREEN_IMAGE_TEXT_5_TAG, image_data.string5))
        {
            return false;
        }
    }
    layout = xml_find(xml_root, LOADING_SCREEN_IMAGE_LEVEL_LAYOUT_TAG);
    if (layout != nullptr)
    {
        if (image_data.layout_type != LSLT_NONE)
        {
            return false;
        }
        image_data.layout_type = LSLT_LEVEL_NAME;
        image_data.num_fg_images = 0;
        element = xml_find(layout, LOADING_SCREEN_IMAGE_TEXT_1_TAG);
        if (element == nullptr || (image_data.string1 = Loading_screen_string_pool.add_unique(element->text)) == nullptr)
        {
            return false;
        }
    }
    layout = xml_find(xml_root, LOADING_SCREEN_IMAGE_POSTER_LAYOUT_TAG);
    if (layout != nullptr)
    {
        if (image_data.layout_type != LSLT_NONE)
        {
            return false;
        }
        image_data.layout_type = LSLT_POSTER;
        image_data.num_fg_images = 0;
        if (image_data.overlay_prob > 0)
        {
            return false;
        }
        xml_element* fg = xml_find(layout, LOADING_SCREEN_IMAGE_FG_1_IMAGE_TAG);
        if (fg != nullptr && !loading_screen_parse_fg_image(fg, image_data))
        {
            return false;
        }
        if (!loading_screen_parse_string(layout, LOADING_SCREEN_IMAGE_TEXT_1_TAG, image_data.string1) ||
            !loading_screen_parse_string(layout, LOADING_SCREEN_IMAGE_TEXT_2_TAG, image_data.string2) ||
            !loading_screen_parse_string(layout, LOADING_SCREEN_IMAGE_TEXT_3_TAG, image_data.string3))
        {
            return false;
        }
    }
    if (image_data.overlay_prob > 0 && image_data.num_fg_images != 0)
    {
        return false;
    }
    return image_data.layout_type != LSLT_NONE;
}

// 0x008BF720 (:1148)
// The image layouts of loading_screen.xtbl (or a DLC's "dlcNN_loading_screen.xtbl"): posters go to the overlays,
// the others to the images (news broadcasts also to the news names). The base table's count is the DLC mark.
void loading_screen_parse_images_table(int dlc_id)
{
    const bool is_dlc = dlc_id != -1;
    xml_element* root;
    if (is_dlc)
    {
        char filename[256];
        sprintf(filename, "dlc%02d_%s", dlc_id, LOADING_SCREEN_IMAGE_TABLE_FILE_NAME);
        root = xml_table_parse(filename);
        Loading_screen_string_pool.set_thread_ownership();
    }
    else
    {
        root = xml_table_parse(LOADING_SCREEN_IMAGE_TABLE_FILE_NAME);
    }
    for (xml_element* cur = xml_find(root, LOADING_SCREEN_IMAGE_ROOT_NAME_TAG); cur != nullptr;
         cur = xml_find_next(root, cur, LOADING_SCREEN_IMAGE_ROOT_NAME_TAG))
    {
        loading_screen_layout layout;
        memset(&layout, 0, sizeof(layout));
        if (!loading_screen_parse_image(cur, layout))
        {
            continue;
        }
        if (layout.layout_type == LSLT_POSTER)
        {
            if (Loading_image_overlay_data.size() < Loading_image_overlay_data.capacity())
            {
                Loading_image_overlay_data.add(layout);
            }
            continue;
        }
        if (layout.layout_type == LSLT_NEWS_BROADCAST)
        {
            if (Loading_image_news_image_names.size() + 1 > Loading_image_news_image_names.capacity())
            {
                VLIB_CRITICAL_FAILURE();
            }
            else
            {
                Loading_image_news_image_names.add(layout.name);
            }
        }
        if (Loading_image_data.size() < Loading_image_data.capacity())
        {
            Loading_image_data.add(layout);
        }
    }
    xml_table_close(root);
    if (!is_dlc)
    {
        Loading_image_data_dlc_mark = (unsigned int)Loading_image_data.size();
    }
}

// 0x008AAD60 (:1221)
// Drops the DLC's image layouts (back to the mark).
void loading_screen_dlc_restore()
{
    while (Loading_image_data_dlc_mark < (unsigned int)Loading_image_data.size())
    {
        Loading_image_data.remove(Loading_image_data.size() - 1);
    }
}

// 0x008CEA00 (:1230)
// The "LoadData" data group of rfg_ui_loading: one item per selected image, 23 elements each.
void loading_screen_initialize_ui_data()
{
    vint_data_group* group = vint_data_group::create_new("LoadData");
    if (group == nullptr)
    {
        return;
    }
    // Magic numbers: the element types (uint 2... in vint_variant_type: 1 int, 2 uint, 3 float, 4 string).
    const vint_variant_type types[23] = {
        VINT_PROP_TYPE_UINT, (vint_variant_type)1, (vint_variant_type)4, (vint_variant_type)3, (vint_variant_type)4,
        (vint_variant_type)3, (vint_variant_type)4, (vint_variant_type)3, (vint_variant_type)4, (vint_variant_type)4,
        (vint_variant_type)4, (vint_variant_type)4, (vint_variant_type)4, (vint_variant_type)4, (vint_variant_type)3,
        (vint_variant_type)4, (vint_variant_type)3, (vint_variant_type)4, (vint_variant_type)4, (vint_variant_type)4,
        (vint_variant_type)4, (vint_variant_type)4, (vint_variant_type)4,
    };
    for (unsigned int i = 0; i < Num_loading_images; i++)
    {
        vint_data_item* item = group->add_data_item(nullptr);
        if (item != nullptr)
        {
            item->set_template(types, 23);
        }
    }
    Loading_data_group_handle = (int)group->get_handle();
    ui_add_data_group(Loading_doc_handle, Loading_data_group_handle);
}

// The variant of a string element (null allowed).
static vint_variant loading_screen_string_variant(const char* text)
{
    vint_variant value;
    value = text;
    return value;
}

// 0x008F0C80 (:1308)
// Each selected image's data item: its index (1-based), layout type, background and rotation, the foreground images,
// the strings (a level name layout shows the district, wrecking crew map or multiplayer map name), the overlay
// poster's fields, and the scoreboard tip in multiplayer.
void loading_screen_process_ui_data()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Loading_data_group_handle);
    if (group == nullptr)
    {
        return;
    }
    vint_data_item* item = group->first_data_item();
    for (unsigned int i = 0; i < group->get_num_data_items(); i++, item = group->next_data_item(item))
    {
        if (item == nullptr)
        {
            return;
        }
        const int overlay_index = Loading_screen_images[i].overlay_img_index;
        loading_screen_layout* layout = &Loading_image_data[Loading_screen_images[i].base_img_index];
        loading_screen_layout* overlay = nullptr;
        // Original bug kept: the overlay index is bounded by the image count, not the overlay count.
        if (overlay_index >= 0 && overlay_index < Loading_image_data.size())
        {
            overlay = &Loading_image_overlay_data[overlay_index];
        }
        if (layout == nullptr)
        {
            continue;
        }
        item->set_element(0, vint_variant(i + 1));
        item->set_element(1, vint_variant((int)layout->layout_type + 1));
        item->set_element(2, loading_screen_string_variant(layout->bg_image));
        item->set_element(3, vint_variant(layout->rotation));
        item->set_element(4, loading_screen_string_variant(layout->fg_images[0].filename));
        item->set_element(5, vint_variant(layout->fg_images[0].rotation));
        item->set_element(6, loading_screen_string_variant(layout->fg_images[1].filename));
        item->set_element(7, vint_variant(layout->fg_images[1].rotation));
        item->set_element(8, loading_screen_string_variant(layout->string1));
        char level_name[384];
        if (layout->layout_type == LSLT_LEVEL_NAME)
        {
            wchar_t name[128];
            name[0] = L'\0';
            if (!Multiplayer)
            {
                if (wrecking_crew_is_active())
                {
                    const char* id_name = wrecking_crew_get_map_display_name(Wrecking_crew_options.map_index);
                    wcsncpy(name, localization_get_display_string_req(id_name, nullptr), 128);
                }
                else
                {
                    t_district* district = territory_shared_find_district(Loading_screen_district.territory_index, Loading_screen_district.district_index);
                    const wchar_t* text = district != nullptr ? localization_get_display_string_opt(district->localized_name_hash) : nullptr;
                    if (text != nullptr)
                    {
                        wcsncpy(name, text, 128);
                    }
                }
            }
            else if (Mp_server_options.get_level() != nullptr)
            {
                wcsncpy(name, localization_get_display_string_req(Mp_server_options.get_level()->display_name, nullptr), 128);
            }
            vint_encode_wide_text(level_name, sizeof(level_name), name);
            item->set_element(9, loading_screen_string_variant(level_name));
        }
        else
        {
            item->set_element(9, loading_screen_string_variant(layout->string2));
        }
        item->set_element(10, loading_screen_string_variant(layout->string3));
        item->set_element(11, loading_screen_string_variant(layout->string4));
        item->set_element(12, loading_screen_string_variant(layout->string5));
        if (overlay == nullptr)
        {
            item->set_element(13, loading_screen_string_variant(nullptr));
            item->set_element(14, vint_variant(0.0f));
            item->set_element(15, loading_screen_string_variant(nullptr));
            item->set_element(16, vint_variant(0.0f));
            for (int e = 17; e <= 21; e++)
            {
                item->set_element(e, loading_screen_string_variant(nullptr));
            }
        }
        else
        {
            item->set_element(13, loading_screen_string_variant(overlay->bg_image));
            item->set_element(14, vint_variant(overlay->rotation));
            item->set_element(15, loading_screen_string_variant(overlay->fg_images[0].filename));
            item->set_element(16, vint_variant(overlay->fg_images[0].rotation));
            item->set_element(17, loading_screen_string_variant(overlay->string1));
            item->set_element(18, loading_screen_string_variant(overlay->string2));
            item->set_element(19, loading_screen_string_variant(overlay->string3));
            item->set_element(20, loading_screen_string_variant(overlay->string4));
            item->set_element(21, loading_screen_string_variant(overlay->string5));
        }
        item->set_element(22, loading_screen_string_variant(Multiplayer ? "LOADING_TIP_SCOREBOARD" : nullptr));
    }
}

// 0x008AAF30 (:1502)
// LTCG: img_data in EAX. Requests (or releases) the image's background container and its foreground images; a
// failed foreground request releases what was requested.
bool loading_screen_stream_image_data(loading_screen_layout* img_data, bool load)
{
    if (img_data == nullptr)
    {
        return false;
    }
    const char* name = img_data->bg_image;
    if (!load)
    {
        // Original: the background is released unless it is a container that is not loaded.
        unsigned int srid;
        if (name == nullptr || (srid = stream2_container_find(name)) == 0 || stream2_get_status(srid) != ST_UNLOADED)
        {
            ui_release_image(img_data->bg_image);
        }
        for (int i = 0; i < img_data->num_fg_images; i++)
        {
            if (ui_get_image_status(img_data->fg_images[i].filename) != ST_UNLOADED)
            {
                ui_release_image(img_data->fg_images[i].filename);
            }
        }
        return true;
    }
    if (name == nullptr)
    {
        return false;
    }
    const unsigned int srid = stream2_container_find(name);
    if (srid == 0 || !stream2_request(srid))
    {
        return false;
    }
    for (int i = 0; i < img_data->num_fg_images; i++)
    {
        if (!ui_request_image(img_data->fg_images[i].filename))
        {
            ui_release_image(img_data->bg_image);
            for (int k = i - 1; k >= 0; k--)
            {
                ui_release_image(img_data->fg_images[k].filename);
            }
            return false;
        }
    }
    return true;
}

// 0x008AB020 (:1549)
static int loading_screen_find_image_layout(const char* image_name)
{
    if (image_name == nullptr)
    {
        return -1;
    }
    const unsigned int crc = crc_stri(image_name, 0);
    for (int i = 0; i < Loading_image_data.size(); i++)
    {
        if (Loading_image_data[i].name_crc == crc && v_stricmp(Loading_image_data[i].name, image_name) == 0)
        {
            return i;
        }
    }
    return -1;
}

// The district's own loading images, or none (the news images) when the news probability wins, but in the tutorial
// and Parker.
static unsigned int loading_screen_district_images(const t_district* district, const char** names)
{
    if (v_stricmp(district->name, "Tutorial") != 0 && v_stricmp(district->name, "Parker") != 0 && fl_rand() <= Loading_screen_news_image_prob)
    {
        return 0xFFFFFFFF;
    }
    const unsigned int count = (unsigned int)district->num_load_screen_images;
    for (unsigned int i = 0; i < count; i++)
    {
        // Magic number: 128 names.
        if (i > 0x7F)
        {
            VLIB_CRITICAL_FAILURE();
            break;
        }
        names[i] = district->load_screen_images[i];
    }
    return count;
}

// 0x008AB090 (:1566)
// Up to three different images at random (CRT rand) from the candidates: the multiplayer map's load image, the
// wrecking crew map's, the district's (or the news images); each may get a poster overlay by its probability. With
// none, the "default" image.
void loading_screen_select_loading_images()
{
    unsigned int num_names = 0;
    const char* names[128];
    memset(names, 0, sizeof(names));
    // Magic number: 6 % news images in the base game, none with a DLC.
    Loading_screen_news_image_prob = World.dlc_bundle_id == -1 ? 0.06f : 0.0f;
    unsigned int found = 0;
    bool news = false;
    if (Multiplayer)
    {
        const multi_level_info* level = Mp_server_options.get_level();
        if (level != nullptr && (names[0] = level->level_load_image) != nullptr)
        {
            num_names = 1;
        }
    }
    else if (World.sl_flags.pending_new_game && !World.sl_flags.pending_single_zone && !wrecking_crew_is_active())
    {
        char territory_name[132];
        int grid_x;
        int grid_y;
        int district_flags;
        rfg_zonex_get_territory_info(World.get_pending_filename(), territory_name, grid_x, grid_y, district_flags);
        territory* terr = territory_shared_find(territory_name);
        t_district* district = terr != nullptr ? terr->find_district((unsigned short)(((grid_y & 0xF) | (grid_x & 0xF) << 4) << 8) | (unsigned short)(district_flags & 0xFF)) : nullptr;
        if (district != nullptr)
        {
            Loading_screen_district.district_index = (int)district->index;
            Loading_screen_district.territory_index = (int)terr->index;
            found = loading_screen_district_images(district, names);
            news = found == 0xFFFFFFFF;
            num_names = news ? 0 : found;
        }
    }
    else if (wrecking_crew_is_active())
    {
        if ((names[0] = wrecking_crew_get_loading_screen_image_name(Wrecking_crew_options.map_index)) != nullptr)
        {
            num_names = 1;
        }
    }
    else if (Loading_screen_district.territory_index == 0 || Loading_screen_district.district_index == 0)
    {
        world_state_buf* state = World.get_state_data();
        territory* terr = state != nullptr ? territory_shared_find(World.get_territory_name()) : nullptr;
        if (terr != nullptr)
        {
            int grid_x;
            int grid_y;
            rfg_zonex_get_grid_coords(state->player_start_pos, grid_x, grid_y);
            t_district* district = terr->find_district((unsigned short)(((grid_y & 0xF) | (grid_x & 0xF) << 4) << 8));
            if (district != nullptr)
            {
                Loading_screen_district.district_index = (int)district->index;
                Loading_screen_district.territory_index = (int)terr->index;
                found = loading_screen_district_images(district, names);
                news = found == 0xFFFFFFFF;
                num_names = news ? 0 : found;
            }
        }
    }
    else
    {
        t_district* district = territory_shared_find_district(Loading_screen_district.territory_index, Loading_screen_district.district_index);
        if (district != nullptr)
        {
            found = loading_screen_district_images(district, names);
            news = found == 0xFFFFFFFF;
            num_names = news ? 0 : found;
        }
    }
    if (news)
    {
        num_names = (unsigned int)Loading_image_news_image_names.size();
        for (unsigned int i = 0; i < num_names; i++)
        {
            names[i] = Loading_image_news_image_names[i];
        }
    }
    Num_loading_images = 0;
    // Magic number: three images at most.
    for (unsigned int n = 0; n < num_names && n < 3; n++)
    {
        const unsigned int start = (unsigned int)rand() % num_names;
        unsigned int candidate = start;
        bool unique = false;
        do
        {
            const int index = loading_screen_find_image_layout(names[candidate]);
            Loading_screen_images[Num_loading_images].base_img_index = index;
            if (index != -1)
            {
                unique = true;
                for (int k = (int)Num_loading_images - 1; k >= 0; k--)
                {
                    if (Loading_screen_images[k].base_img_index == index)
                    {
                        unique = false;
                        break;
                    }
                }
                if (!unique && ++candidate >= num_names)
                {
                    candidate = 0;
                }
            }
        } while (candidate != start && !unique);
        if (!unique)
        {
            break;
        }
        Loading_screen_images[Num_loading_images].overlay_img_index = -1;
        loading_screen_layout* layout = &Loading_image_data[Loading_screen_images[Num_loading_images].base_img_index];
        const int num_overlays = Loading_image_overlay_data.size();
        if (rand() % 100 < layout->overlay_prob)
        {
            const int first = rand() % num_overlays;
            int overlay = first;
            bool overlay_unique;
            do
            {
                overlay_unique = true;
                Loading_screen_images[Num_loading_images].overlay_img_index = overlay;
                for (int k = (int)Num_loading_images - 1; k >= 0; k--)
                {
                    if (Loading_screen_images[k].overlay_img_index == overlay)
                    {
                        overlay_unique = false;
                        // Original bug kept: the overlay index wraps at the image name count.
                        if ((unsigned int)++overlay >= num_names)
                        {
                            overlay = 0;
                        }
                        break;
                    }
                }
                if (overlay == first)
                {
                    if (!overlay_unique)
                    {
                        VLIB_CRITICAL_FAILURE();
                        Loading_screen_images[Num_loading_images].overlay_img_index = -1;
                    }
                    break;
                }
            } while (!overlay_unique);
        }
        Num_loading_images++;
    }
    if (Num_loading_images == 0)
    {
        const int index = loading_screen_find_image_layout("default");
        if (index >= 0)
        {
            Num_loading_images = 1;
            Loading_screen_images[0].base_img_index = index;
            return;
        }
        keen::debug::nativePrintArgs("Error finding the default loading screen image");
    }
}

// 0x00888FB0 (:3144)
void gs_boot_set_installing(bool installing)
{
    Boot_installing = installing;
}

// 0x00888FC0 (:3149)
void gs_boot_set_failed_boot(bool failed)
{
    Boot_boot_failed = failed;
}

// 0x00888FD0 (:3154)
// "m:ss".
void gs_boot_set_install_time(unsigned int timeleft)
{
    _snprintf_safe(Boot_install_time_left, sizeof(Boot_install_time_left), "%d:%02d", timeleft / 60, timeleft % 60);
}

// 0x00889010 (:3159)
void gs_boot_set_install_percentage(float percentage)
{
    Boot_install_percent = percentage;
}

// 0x008ABA30 (:3165)
const loading_screen_tip_data* loading_screen_tip_get_data()
{
    if (Current_tip >= 0 && Current_tip < Tip_data.size())
    {
        return &Tip_data[Current_tip];
    }
    return nullptr;
}

// 0x008BF950 (:3174)
void loading_screen_tip_randomize()
{
    Current_tip = int_rand_range(0, Tip_data.size() - 1);
    loading_screen_tip_cycle(true);
}

// 0x008ABA50 (:3182)
// The next (or previous) tip that is not of the other game type. Original: the test compares the type with
// !Multiplayer, so single player tips are skipped in multiplayer and multiplayer tips in single player.
void loading_screen_tip_cycle(bool forward)
{
    if (Tip_data.size() <= 0)
    {
        Current_tip = -1;
        return;
    }
    int tip = Current_tip;
    do
    {
        if (forward)
        {
            if (++tip >= Tip_data.size())
            {
                tip = 0;
            }
        }
        else if (--tip < 0)
        {
            tip = Tip_data.size() - 1;
        }
    } while ((unsigned int)Tip_data[tip].type == (unsigned int)!Multiplayer && tip != Current_tip);
    Current_tip = tip;
}

// 0x00889020 (:3216)
void loading_screen_set_district(int territory_index, int district_index)
{
    Loading_screen_district.territory_index = territory_index;
    Loading_screen_district.district_index = district_index;
}
