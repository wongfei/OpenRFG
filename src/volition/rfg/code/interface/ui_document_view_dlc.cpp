// Original: volition/rfg/code/interface/ui_document_view_dlc.cpp
//
// The DLC screen (rfg_ui_ps3_dlc): the downloaded levels and the checks before one starts (new or load, autosave warning,
// difficulty), and the level's territory table load on the way out.

#include "volition/rfg/code/interface/ui_document_view_dlc.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/cutscene/video_cutscene.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_save.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/hud/fog_of_war.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_document_save_load.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2.h"
#include "volition/stream2/code/stream2_os.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/page_pool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <stdio.h>
#include <string.h>

// ---- file-private types (rfg.pdb)
enum dlc_exit_state
{
    DLC_EXIT_STATE_START = 0,
    DLC_EXIT_STATE_LOADING_TERRITORY = 1,
    DLC_EXIT_STATE_READY_TO_EXIT = 2,
};

enum dlc_load_game_state
{
    DLC_LOAD_GAME_STATE_START = 0,
    DLC_LOAD_GAME_STATE_LOADING_TERRITORY = 1,
    DLC_LOAD_GAME_STATE_READY = 2,
};

enum dlc_selection_states
{
    DLC_SS_NONE = -1,
    DLC_SS_CHECK_NEW_OR_LOAD = 0,
    DLC_SS_CHECK_PROFILE = 1,
    DLC_SS_CHECK_STORAGE_DEVICE = 2,
    DLC_SS_CHECK_MAX_SAVES = 3,
    DLC_SS_CHECK_FREE_SPACE = 4,
    DLC_SS_CHECK_AUTOSAVE = 5,
    DLC_SS_CHECK_NETWORK_CONNECTION = 6,
    DLC_SS_CHECK_INTERNET_CONNECTION = 7,
    DLC_SS_CHECK_INTERNET_PERMISSION = 8,
    DLC_SS_SELECT_DIFFICULTY = 9,
    DLC_SS_TRIGGER_SELECTION = 10,
    DLC_SS_NUM_STATES = 11,
};

enum dlc_selection_types
{
    DLC_ST_NONE = -1,
    DLC_ST_NEW_GAME = 0,
    DLC_ST_LOAD_GAME = 1,
    DLC_ST_OPEN_STORE = 2,
    NUM_DLC_SELECTION_TYPES = 3,
};

// Magic number: the rows the document has.
static const int Max_dlc_rows = 9;

// ---- file-static globals
extern int Doc_handle_main; // 0x016631AC, ui_menu.cpp's
extern dlc_level_info* Downloaded_levels; // 0x02BF2170, loading.cpp's
extern bool Gs_switch_transparent; // 0x02C034FA, loading.cpp's
extern bool Gs_switch_pause_beneath; // 0x02C034FB, loading.cpp's
extern bool Active_uichar; // 0x02C07FDF, ui_character.cpp's
void ui_character_shutdown_internal(); // 0x008B0670 (ui_character.cpp)
static const char* cDocname_uidlc = {"rfg_ui_ps3_dlc"}; // 0x01664F30
static const char* cPage_title = {"MENU_DOWNLOADABLE_CONTENT"}; // 0x01664F34
static const char* cDescription_caption = {"MENU_DLC_DESCRIPTION_CAPTION"}; // 0x01664F38
static const char* cBack_button_uidlc = {"MENU_BACK"}; // 0x01664F40
static const char* cNo_dlc = {"MENU_NO_DLC"}; // 0x01664F48
static const char* cDLCName = {""}; // 0x01664F4C
static const char* cNone_found = {"MENU_NONE_FOUND"}; // 0x01664F50
static int Doc_handle_uidlc = -1; // 0x01664F54
static int Data_header_handle_uidlc = 16; // 0x01664F58
static int Data_row_group_handle_uidlc = 16; // 0x01664F5C
static int Data_hints_handle_uidlc = 16; // 0x01664F60
static int Message_box_handle_uidlc = -1; // 0x01664F64
static int Input_id_uidlc = -1; // 0x01664F68
static unsigned int Dlc_async_file_req = 0xFFFFFFFF; // 0x01664F6C
static int Dlc_delayed_process_input_id = -1; // 0x01664F70
static dlc_selection_types Dlc_delayed_selection_type = DLC_ST_NONE; // 0x01664F74
static dlc_selection_states Dlc_delayed_selection_state = DLC_SS_NONE; // 0x01664F78
static int Refresh_msgbox_handle = -1; // 0x01664F7C
static unsigned int Refresh_thread = 0xFFFF; // 0x01664F80
static int Data_row_handles_uidlc[Max_dlc_rows]; // 0x02BE7DDC
static bool Exit_uidlc; // 0x02C0815E
static bool Loaded_uidlc; // 0x02C0815F
static unsigned int Control_id_uidlc; // 0x02C08160
static bool In_purchase_screen; // 0x02C08164
static bool Dlc_delay_selection_a_frame; // 0x02C08165
static bool Dlc_accepted_no_profile_warning; // 0x02C08166
static bool Dlc_prompted_for_device; // 0x02C08167
static int Display_start_index_uidlc; // 0x02C08168
static int Selected_row_uidlc; // 0x02C0816C
static dlc_exit_state Dlc_exit_state; // 0x02C08170
static dlc_load_game_state Dlc_load_game_state; // 0x02C08174
static unsigned char* Dlc_temp_buffer; // 0x02C08178
static virtual_mempool* DLC_temp_mempool; // 0x02C0817C
static bool Dlc_accepted_no_device_warning; // 0x02C08180
static bool Dlc_accepted_no_space_warning; // 0x02C08181
static bool Dlc_accepted_max_save_warning; // 0x02C08182
static bool Dlc_accepted_autosave_warning; // 0x02C08183
static bool Dlc_waiting_for_device; // 0x02C08184
static bool Dlc_showing_profile_load; // 0x02C08185
static bool Dlc_selected_difficulty; // 0x02C08186
static bool Dlc_selected_new_or_load; // 0x02C08187
static const char* Streamed_image_name_uidlc; // 0x02C08188
static bool Load_image_next_frame_uidlc; // 0x02C0818C
static bool Refresh_dlc; // 0x02C0818D
static bool Refresh_dlc_after_selection; // 0x02C0818E

// 0x0089F400 (:185)
void unused_dcf_refresh_dlc()
{
    if (Console_command != 0)
    {
        Refresh_dlc = true;
    }
}

// 0x008B70E0 (:193)
static bool start_dlc_territory_table_load()
{
    char filename[64];
    sprintf(filename, "dlc%02d_%s", dlc_get_current_id(), "territories.xtbl");
    if (!cf_exist(filename))
    {
        return false;
    }
    unsigned int file_size;
    if (!cf_get_file_size(&file_size, filename, false))
    {
        return false;
    }
    stream2_os_file request;
    request.chain_num_reads = 0;
    request.chain_size = nullptr;
    request.chain_dest = nullptr;
    request.priority = 0;
    request.category = 0;
    request.preopened = nullptr;
    strcpy(request.filename, filename);
    request.size = file_size;
    request.offset = 0;
    const unsigned int buffer_size = file_size + 4; // Magic number: room past the end of the text.
    DLC_temp_mempool = Common_page_pool.get_virtual_mempool("dlc_temp_mempool", buffer_size);
    if (DLC_temp_mempool == nullptr)
    {
        return false;
    }
    Dlc_temp_buffer = (unsigned char*)DLC_temp_mempool->alloc(buffer_size, 0);
    request.dest = Dlc_temp_buffer;
    if (Dlc_temp_buffer == nullptr)
    {
        Common_page_pool.release_virtual_mempool(DLC_temp_mempool);
        DLC_temp_mempool = nullptr;
        return false;
    }
    memset(Dlc_temp_buffer, 0, buffer_size);
    Dlc_async_file_req = stream2_os_read(request);
    return true;
}

// 0x0089F420 (:236)
static bool finished_dlc_territory_table_load()
{
    if (Dlc_async_file_req == 0xFFFFFFFF)
    {
        return true;
    }
    const stream2_os_req_status status = stream2_os_poll(Dlc_async_file_req);
    if (status == SOS_BUSY)
    {
        return false;
    }
    stream2_os_release_handle(Dlc_async_file_req);
    Dlc_async_file_req = 0xFFFFFFFF;
    if (status == SOS_DONE_SUCCESS)
    {
        dlc_territories_init(Dlc_temp_buffer);
    }
    Dlc_temp_buffer = nullptr;
    if (DLC_temp_mempool != nullptr)
    {
        DLC_temp_mempool->clear();
        Common_page_pool.release_virtual_mempool(DLC_temp_mempool);
        DLC_temp_mempool = nullptr;
    }
    return true;
}

// 0x008EF6F0 (:281)
static void dlc_clear_delayed_selection()
{
    Dlc_delayed_selection_type = DLC_ST_NONE;
    Dlc_delayed_selection_state = DLC_SS_NONE;
    if (Dlc_showing_profile_load)
    {
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
        Dlc_showing_profile_load = false;
    }
    if (Refresh_dlc_after_selection)
    {
        Refresh_dlc = true;
    }
}

// 0x008EF770 (:297)
static bool dlc_difficulty_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Message_box_handle_uidlc != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        difficulty_set_level((difficulty_levels)selection_index);
        Dlc_selected_difficulty = true;
    }
    else if ((int)choice < 1 || (int)choice > 2)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    else
    {
        dlc_clear_delayed_selection();
    }
    Message_box_handle_uidlc = -1;
    ui_wait_for_button_ups();
    return true;
}

// 0x008EF7F0 (:420)
static bool dlc_new_or_load_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Message_box_handle_uidlc != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        if (selection_index != 0)
        {
            Dlc_delayed_selection_type = DLC_ST_LOAD_GAME;
        }
        Dlc_selected_new_or_load = true;
    }
    else if ((int)choice > 0 && (int)choice < 3)
    {
        dlc_clear_delayed_selection();
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Message_box_handle_uidlc = -1;
    return true;
}

// 0x008EF870 (:558)
static bool dlc_autosave_cb(int mbox_handle, int, msgbox_choice choice)
{
    if (Message_box_handle_uidlc != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        Dlc_accepted_autosave_warning = true;
    }
    else if ((int)choice > 0 && (int)choice < 3)
    {
        dlc_clear_delayed_selection();
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Message_box_handle_uidlc = -1;
    return true;
}

// 0x008EF8E0 (:584)
static bool dlc_accept_abort(int mbox_handle, int, msgbox_choice)
{
    if (Message_box_handle_uidlc != mbox_handle)
    {
        return false;
    }
    dlc_clear_delayed_selection();
    Message_box_handle_uidlc = -1;
    return true;
}

// Hides the check's message box once the check has passed. rfg.exe inlines this.
static void dlc_hide_passed_check(dlc_selection_states state)
{
    if (Dlc_delayed_selection_state == state && Message_box_handle_uidlc != -1)
    {
        ui_hide_msgbox(Message_box_handle_uidlc, false);
        Message_box_handle_uidlc = -1;
        Dlc_delayed_selection_state = DLC_SS_NONE;
    }
}

// True when the check's message box should be added now. Hides another check's box first. rfg.exe inlines this.
static bool dlc_check_needs_msgbox(dlc_selection_states state)
{
    if (Dlc_delayed_selection_state == DLC_SS_NONE || Dlc_delayed_selection_state == state)
    {
        if (Message_box_handle_uidlc != -1)
        {
            return false;
        }
    }
    else if (Message_box_handle_uidlc != -1)
    {
        ui_hide_msgbox(Message_box_handle_uidlc, false);
        Message_box_handle_uidlc = -1;
    }
    return !ui_is_msgbox_active(-2); // Magic number: -2, any message box.
}

// 0x00908660 (:599)
// One pass over the checks, in the order of dlc_selection_states; a check still waiting stops the pass.
static void dlc_process_delayed_selection()
{
    if (Dlc_delay_selection_a_frame)
    {
        Dlc_delay_selection_a_frame = false;
        return;
    }
    if (Dlc_delayed_selection_type == DLC_ST_NONE)
    {
        if (Dlc_delayed_process_input_id != -1)
        {
            ui_relinquish_input_id(Dlc_delayed_process_input_id);
            Dlc_delayed_process_input_id = -1;
        }
        return;
    }
    if (multi_core_platform_ui_is_active() || gameseq_state_is_active(GS_SONY_OPTIONS) || player_profile_busy(Player_controller_index != 0xFFFFFFFF ? Player_controller_index : 0))
    {
        return;
    }
    bool waiting = false;
    for (int check = 0; check < DLC_SS_NUM_STATES && !waiting; check++)
    {
        switch (check)
        {
        case DLC_SS_CHECK_NEW_OR_LOAD:
            if (Dlc_delayed_selection_type == DLC_ST_NEW_GAME && !Dlc_selected_new_or_load)
            {
                if (Message_box_handle_uidlc == -1 && !ui_is_msgbox_active(-2))
                {
                    Message_box_handle_uidlc = ui_add_msgbox(MBT_ACCEPT_CANCEL, "", "", dlc_new_or_load_cb, false, false, nullptr, nullptr, false);
                    ui_msgbox_add_caption_item(Message_box_handle_uidlc, "MENU_NEW_GAME", false, false);
                    ui_msgbox_add_caption_item(Message_box_handle_uidlc, "MENU_LOAD_GAME", false, false);
                }
                waiting = true;
                Dlc_delayed_selection_state = DLC_SS_CHECK_NEW_OR_LOAD;
            }
            break;
        case DLC_SS_CHECK_FREE_SPACE:
            if (!Dlc_accepted_no_profile_warning && !Dlc_accepted_no_device_warning && Dlc_delayed_selection_type == DLC_ST_NEW_GAME && Dlc_delayed_selection_state == DLC_SS_CHECK_FREE_SPACE && Message_box_handle_uidlc != -1)
            {
                ui_hide_msgbox(Message_box_handle_uidlc, false);
                Message_box_handle_uidlc = -1;
                Dlc_delayed_selection_state = DLC_SS_NONE;
                Dlc_accepted_no_device_warning = false;
                Dlc_accepted_no_space_warning = false;
                Dlc_accepted_autosave_warning = false;
                Dlc_accepted_max_save_warning = false;
                Dlc_selected_difficulty = false;
            }
            break;
        case DLC_SS_CHECK_AUTOSAVE:
            if (!Dlc_accepted_no_profile_warning && !Dlc_accepted_no_device_warning && !Dlc_accepted_no_space_warning && Dlc_delayed_selection_type == DLC_ST_NEW_GAME)
            {
                if (!game_save_is_dlc_autosave_present())
                {
                    if (Dlc_delayed_selection_state == DLC_SS_CHECK_AUTOSAVE && Message_box_handle_uidlc != -1)
                    {
                        ui_hide_msgbox(Message_box_handle_uidlc, false);
                        Message_box_handle_uidlc = -1;
                        Dlc_delayed_selection_state = DLC_SS_NONE;
                        Dlc_accepted_no_device_warning = false;
                        Dlc_accepted_no_space_warning = false;
                        Dlc_accepted_autosave_warning = false;
                        Dlc_accepted_max_save_warning = false;
                        Dlc_selected_difficulty = false;
                    }
                }
                else if (!Dlc_accepted_autosave_warning)
                {
                    if (dlc_check_needs_msgbox(DLC_SS_CHECK_AUTOSAVE))
                    {
                        Message_box_handle_uidlc = ui_add_msgbox(MBT_ACCEPT_CANCEL_SELECT, "MENU_MBOX_AUTOSAVE_WARNING_TITLE", "MENU_MBOX_AUTOSAVE_WARNING", dlc_autosave_cb, false, false, nullptr, nullptr, false);
                    }
                    waiting = true;
                    Dlc_delayed_selection_state = DLC_SS_CHECK_AUTOSAVE;
                }
            }
            break;
        case DLC_SS_CHECK_NETWORK_CONNECTION:
            if (Dlc_delayed_selection_type == DLC_ST_OPEN_STORE)
            {
                if (multi_core_platform_is_connected_to_network())
                {
                    dlc_hide_passed_check(DLC_SS_CHECK_NETWORK_CONNECTION);
                }
                else
                {
                    if (dlc_check_needs_msgbox(DLC_SS_CHECK_NETWORK_CONNECTION))
                    {
                        Message_box_handle_uidlc = ui_add_msgbox(MBT_OK, "MENU_MBOX_NO_CONNECTION", "NETWORK_NO_NETWORK_CONNECTION", dlc_accept_abort, false, false, nullptr, nullptr, false);
                    }
                    waiting = true;
                    Dlc_delayed_selection_state = DLC_SS_CHECK_NETWORK_CONNECTION;
                }
            }
            break;
        case DLC_SS_CHECK_INTERNET_CONNECTION:
            if (Dlc_delayed_selection_type == DLC_ST_OPEN_STORE)
            {
                if (multi_core_platform_is_connected_to_internet())
                {
                    dlc_hide_passed_check(DLC_SS_CHECK_INTERNET_CONNECTION);
                }
                else
                {
                    if (dlc_check_needs_msgbox(DLC_SS_CHECK_INTERNET_CONNECTION))
                    {
                        Message_box_handle_uidlc = ui_add_msgbox(MBT_OK, "MENU_MBOX_MEMBERSHIP", "NETWORK_LIVE_SIGN_IN_NO_PROMPT", dlc_accept_abort, false, false, nullptr, nullptr, false);
                    }
                    waiting = true;
                    Dlc_delayed_selection_state = DLC_SS_CHECK_INTERNET_CONNECTION;
                }
            }
            break;
        case DLC_SS_SELECT_DIFFICULTY:
            if (Dlc_delayed_selection_type == DLC_ST_NEW_GAME)
            {
                if (Dlc_selected_difficulty)
                {
                    dlc_hide_passed_check(DLC_SS_SELECT_DIFFICULTY);
                }
                else
                {
                    if (dlc_check_needs_msgbox(DLC_SS_SELECT_DIFFICULTY))
                    {
                        Message_box_handle_uidlc = ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_DIFFICULTY", "", dlc_difficulty_cb, false, false, nullptr, nullptr, false);
                        ui_msgbox_add_caption_item(Message_box_handle_uidlc, "MENU_DIFFICULTY_CASUAL", false, false);
                        ui_msgbox_add_caption_item(Message_box_handle_uidlc, "MENU_DIFFICULTY_NORMAL", false, false);
                        ui_msgbox_add_caption_item(Message_box_handle_uidlc, "MENU_DIFFICULTY_HARD", false, false);
                        ui_msgbox_add_caption_item(Message_box_handle_uidlc, "MENU_DIFFICULTY_INSANE", false, false);
                        const int menu_handle = ui_msgbox_get_menu_handle(Message_box_handle_uidlc);
                        ui_menu_enable_menu_item(menu_handle, 3, true);
                        ui_menu_set_selected_item(menu_handle, difficulty_get_level());
                    }
                    waiting = true;
                    Dlc_delayed_selection_state = DLC_SS_SELECT_DIFFICULTY;
                }
            }
            break;
        case DLC_SS_TRIGGER_SELECTION:
            if (Dlc_delayed_selection_type == DLC_ST_NEW_GAME)
            {
                fog_of_war_clear();
                stats_reset_save_game_stats();
                difficulty_set_level_reload_overwrite(DIFFICULTY_LEVEL_INVALID);
                dlc_level_info* level = Downloaded_levels + Selected_row_uidlc + Display_start_index_uidlc;
                if (level->intro_video[0] == '\0')
                {
                    dlc_load_level(level);
                }
                else
                {
                    human_voice_level_init_post();
                    video_cutscene_play(level->intro_video, 0xFFFFFFFF, nullptr, true, true, nullptr, nullptr, level);
                    ui_fade_to_black_fade_out(true);
                }
            }
            else if (Dlc_delayed_selection_type == DLC_ST_LOAD_GAME)
            {
                if (Dlc_load_game_state == DLC_LOAD_GAME_STATE_START)
                {
                    dlc_level_shutdown_post();
                    dlc_level_init_pre();
                    Dlc_load_game_state = start_dlc_territory_table_load() ? DLC_LOAD_GAME_STATE_LOADING_TERRITORY : DLC_LOAD_GAME_STATE_READY;
                    waiting = true;
                    Dlc_delayed_selection_state = DLC_SS_TRIGGER_SELECTION;
                    break;
                }
                if (Dlc_load_game_state == DLC_LOAD_GAME_STATE_LOADING_TERRITORY)
                {
                    if (finished_dlc_territory_table_load())
                    {
                        Dlc_load_game_state = DLC_LOAD_GAME_STATE_READY;
                    }
                    waiting = true;
                    Dlc_delayed_selection_state = DLC_SS_TRIGGER_SELECTION;
                    break;
                }
                if (Dlc_load_game_state == DLC_LOAD_GAME_STATE_READY)
                {
                    hud_ui_save_load_show(SLUT_LOAD);
                }
            }
            else if (Dlc_delayed_selection_type == DLC_ST_OPEN_STORE)
            {
                dlc_open_marketplace();
            }
            dlc_clear_delayed_selection();
            break;
        default:
            break;
        }
    }
}

// 0x008CA7D0 (:1434)
void ui_view_dlc_process_input()
{
    int top_id = Current_input_lock_id;
    if (top_id == -1)
    {
        top_id = Input_types.size() - 1 < 0 ? 1 : Input_types[Input_types.size() - 1].id;
    }
    if (Input_id_uidlc != top_id)
    {
        return;
    }
    const char* hints[3] = {"hint_1_group", "row_highlight_button", "row_highlight_main"};
    const int hint = ui_mouse_process_hints(hints, 3, Doc_handle_uidlc, true);
    const int num_levels = Num_downloaded_levels;
    const int selected = Selected_row_uidlc + Display_start_index_uidlc;
    // Magic numbers: ui button 11 goes back and hint 0 is the back hint; ui button 10 starts the selected level.
    if (ui_get_button_info(11, Input_id_uidlc) > 0.0f || hint == 0 || multi_core_platform_has_pending_invite())
    {
        audiolib_cue_play(Audio_info[5], nullptr, nullptr);
        Exit_uidlc = true;
    }
    else if (ui_get_button_info(10, Input_id_uidlc) > 0.0f || hint > 0)
    {
        if (selected < num_levels)
        {
            audiolib_cue_play(Audio_info[4], nullptr, nullptr);
            Dlc_selected_difficulty = false;
            Dlc_accepted_no_profile_warning = false;
            Dlc_accepted_no_device_warning = false;
            Dlc_accepted_no_space_warning = false;
            Dlc_accepted_max_save_warning = false;
            Dlc_accepted_autosave_warning = false;
            Dlc_waiting_for_device = false;
            Dlc_prompted_for_device = false;
            Dlc_showing_profile_load = false;
            Dlc_delayed_selection_type = DLC_ST_NEW_GAME;
            Dlc_delayed_selection_state = DLC_SS_NONE;
            Dlc_selected_new_or_load = false;
            dlc_set_current_id(Downloaded_levels[Selected_row_uidlc + Display_start_index_uidlc].dlc_bundle_id);
            if (Dlc_delayed_process_input_id == -1)
            {
                Dlc_delayed_process_input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
            }
        }
        else
        {
            audiolib_cue_play(Audio_info[11], nullptr, nullptr);
        }
    }
    if (Selected_row_uidlc + Display_start_index_uidlc < 0)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        Display_start_index_uidlc = 0;
        Selected_row_uidlc = 0;
    }
}

// 0x008CA9F0 (:1560)
void ui_view_dlc_create_header_data()
{
    vint_data_item* item = vint_data_item::create_new("DlcHeaderData");
    if (item != nullptr)
    {
        const vint_variant_type types[14] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING};
        item->set_template(types, 14);
        Data_header_handle_uidlc = item->get_handle();
        ui_add_data_item(Doc_handle_uidlc, Data_header_handle_uidlc);
    }
}

// 0x008CAAA0 (:1592)
void ui_view_dlc_create_row_data()
{
    vint_data_group* group = vint_data_group::create_new("DlcRowData");
    if (group != nullptr)
    {
        Data_row_group_handle_uidlc = group->get_handle();
        ui_add_data_group(Doc_handle_uidlc, Data_row_group_handle_uidlc);
    }
}

// 0x008CAAD0 (:1605)
void ui_view_dlc_create_hints_data()
{
    vint_data_item* item = vint_data_item::create_new("DlcHintData");
    if (item != nullptr)
    {
        const vint_variant_type types[7] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 7);
        Data_hints_handle_uidlc = item->get_handle();
        ui_add_data_item(Doc_handle_uidlc, Data_hints_handle_uidlc);
    }
}

// 0x008CAB60 (:1631)
void ui_view_dlc_create_data_items()
{
    ui_view_dlc_create_header_data();
    ui_view_dlc_create_row_data();
    ui_view_dlc_create_hints_data();
}

// 0x0089F4B0 (:1638)
void ui_view_dlc_process_header_data()
{
    vint_data_item* item = vint_data_item::find(Data_header_handle_uidlc);
    const int num_levels = Num_downloaded_levels;
    const int selected = Selected_row_uidlc + Display_start_index_uidlc;
    if (item == nullptr)
    {
        return;
    }
    item->set_element(0, vint_variant(cPage_title));
    const bool has_levels = Downloaded_levels != nullptr && num_levels >= 1;
    const dlc_level_info* level = has_levels ? Downloaded_levels + selected : nullptr;
    item->set_element(1, vint_variant(has_levels ? (const char*)level->display_name : cNone_found));
    item->set_element(2, vint_variant(cDescription_caption));
    item->set_element(3, vint_variant(has_levels ? (const char*)level->description : cNo_dlc));
    item->set_element(4, vint_variant(""));
    item->set_element(5, vint_variant(0));
    item->set_element(6, vint_variant(-1));
    if (has_levels)
    {
        const char* name = level->image_file;
        const unsigned int srid = stream2_container_find(name);
        const bool ready = srid != 0 && stream2_get_status(srid) == ST_LOADED && !Refresh_dlc;
        item->set_element(13, vint_variant(ready ? name : (const char*)nullptr));
    }
    item->set_element(7, vint_variant(num_levels > Max_dlc_rows));
    item->set_element(8, vint_variant((float)Display_start_index_uidlc / (float)(num_levels - Max_dlc_rows)));
    item->set_element(9, vint_variant(cDLCName));
    item->set_element(10, vint_variant(""));
    item->set_element(11, vint_variant(""));
    item->set_element(12, vint_variant(true));
}

// 0x0089F8D0 (:1689)
void ui_view_dlc_process_row_data()
{
    vint_data_group* group = vint_data_group::find(Data_row_group_handle_uidlc);
    if (group != nullptr && Downloaded_levels != nullptr)
    {
        const int num_items = (int)group->get_num_data_items();
        int rows_wanted = Num_downloaded_levels < 1 ? 1 : Num_downloaded_levels;
        if (rows_wanted > Max_dlc_rows)
        {
            rows_wanted = Max_dlc_rows;
        }
        for (int i = num_items - 1; i >= rows_wanted; i--)
        {
            group->remove_data_item(Data_row_handles_uidlc[i]);
        }
        for (int i = num_items; i < rows_wanted; i++)
        {
            vint_data_item* item = group->add_data_item(nullptr);
            if (item != nullptr)
            {
                const vint_variant_type types[7] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_UINT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL};
                item->set_template(types, 7);
                Data_row_handles_uidlc[i] = item->get_handle();
            }
        }
    }
    char image[64];
    char button[128];
    controls_get_button_image_name(image, 0x40, CBA_MENU_SELECT, Player_controller_index);
    if (Num_downloaded_levels < 1)
    {
        vint_data_item* item = vint_data_item::find(Data_row_handles_uidlc[0]);
        keen::formatString(button, 0x80, "[image:%s]", image);
        if (item != nullptr)
        {
            item->set_element(0, vint_variant(1));
            item->set_element(1, vint_variant(cNo_dlc));
            item->set_element(2, vint_variant(0u));
            item->set_element(3, vint_variant(true));
            item->set_element(4, vint_variant(false));
            item->set_element(5, vint_variant((const char*)button));
            item->set_element(6, vint_variant(true));
        }
        return;
    }
    // Original bug, kept: with no group this calls through a null pointer, as the test above lets it.
    for (unsigned int i = 0; i < group->get_num_data_items(); i++)
    {
        vint_data_item* item = vint_data_item::find(Data_row_handles_uidlc[i]);
        if (item == nullptr)
        {
            continue;
        }
        const int level_index = Display_start_index_uidlc + (int)i;
        keen::formatString(button, 0x80, "[image:%s]", image);
        item->set_element(0, vint_variant((int)i + 1));
        item->set_element(1, vint_variant("MENU_START_GAME"));
        item->set_element(2, vint_variant(0u));
        item->set_element(3, vint_variant(Selected_row_uidlc == (int)i));
        item->set_element(4, vint_variant(level_index % 2 == 0));
        item->set_element(5, vint_variant((const char*)button));
        item->set_element(6, vint_variant(true));
    }
}

// 0x0089FCF0 (:1804)
void ui_view_dlc_process_hints_data()
{
    vint_data_item* item = vint_data_item::find(Data_hints_handle_uidlc);
    if (item == nullptr)
    {
        return;
    }
    char image[64];
    char button[128];
    controls_get_button_image_name(image, 0x40, CBA_MENU_ABORT, Player_controller_index);
    keen::formatString(button, 0x80, "[image:%s]", image);
    item->set_element(0, vint_variant((const char*)button));
    item->set_element(1, vint_variant(cBack_button_uidlc));
    item->set_element(2, vint_variant(""));
    item->set_element(3, vint_variant(""));
    item->set_element(4, vint_variant(false));
    item->set_element(5, vint_variant(""));
    item->set_element(6, vint_variant(""));
}

// 0x0089FE80 (:1830)
void ui_view_dlc_process_data_items()
{
    if (!In_purchase_screen)
    {
        ui_view_dlc_process_header_data();
        ui_view_dlc_process_row_data();
        ui_view_dlc_process_hints_data();
    }
}

// 0x00917110 (:1866)
void ui_view_dlc_init()
{
    state_info info;
    info.enter = ui_view_dlc_enter;
    info.exit = ui_view_dlc_exit;
    info.exit_notify = ui_view_dlc_exit_notify;
    info.process = ui_view_dlc_process;
    info.handshake = ui_view_dlc_handshake;
    info.render = ui_view_dlc_render;
    info.transparent = true;
    info.pause_beneath = true;
    gameseq_register_state(GS_VIEW_DLC_SCREEN, info);
}

// 0x00908CD0 (:1880)
void ui_view_dlc_enter(game_state prev_state)
{
    (void)prev_state;
    Dlc_exit_state = DLC_EXIT_STATE_START;
    Dlc_load_game_state = DLC_LOAD_GAME_STATE_START;
    Control_id_uidlc = 0;
    Input_id_uidlc = -1;
    Exit_uidlc = false;
    Loaded_uidlc = false;
    Display_start_index_uidlc = 0;
    Selected_row_uidlc = 0;
    Dlc_delayed_selection_type = DLC_ST_NONE;
    Dlc_delayed_selection_state = DLC_SS_NONE;
    Dlc_accepted_no_profile_warning = false;
    Dlc_prompted_for_device = false;
    Dlc_accepted_no_device_warning = false;
    Dlc_accepted_no_space_warning = false;
    Dlc_accepted_max_save_warning = false;
    Dlc_accepted_autosave_warning = false;
    Dlc_waiting_for_device = false;
    Dlc_selected_difficulty = false;
    Dlc_selected_new_or_load = false;
    Refresh_dlc_after_selection = false;
    Streamed_image_name_uidlc = nullptr;
    Doc_handle_uidlc = ui_load_document(cDocname_uidlc, DFIT_NONE, 0.0f, DLT_NORMAL, true, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (Doc_handle_uidlc == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        gameseq_pop_state();
        return;
    }
    Input_id_uidlc = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    Control_id_uidlc = ui_request_control_id();
    ui_wait_for_button_ups();
    if (gameseq_get_state() != GS_MAINMENU_IDLE && Doc_handle_main != -1)
    {
        ui_character_shutdown_internal();
        Active_uichar = false;
        main_menu_show_menu(false);
    }
}

// 0x008B7240 (:1938)
bool ui_view_dlc_exit_notify(game_state new_state)
{
    if (new_state != GS_LOAD && new_state != GS_VIDEO_CUTSCENE_PLAY)
    {
        dlc_level_shutdown_post();
        return true;
    }
    if (Dlc_exit_state == DLC_EXIT_STATE_START)
    {
        dlc_level_shutdown_post();
        dlc_level_init_pre();
        dlc_subtitles();
        Dlc_exit_state = start_dlc_territory_table_load() ? DLC_EXIT_STATE_LOADING_TERRITORY : DLC_EXIT_STATE_READY_TO_EXIT;
    }
    else if (Dlc_exit_state == DLC_EXIT_STATE_LOADING_TERRITORY)
    {
        if (finished_dlc_territory_table_load())
        {
            Dlc_exit_state = DLC_EXIT_STATE_READY_TO_EXIT;
        }
    }
    else if (Dlc_exit_state == DLC_EXIT_STATE_READY_TO_EXIT)
    {
        return true;
    }
    return false;
}

// 0x008EF900 (:1987)
void ui_view_dlc_exit(game_state prev_state)
{
    (void)prev_state;
    if (Streamed_image_name_uidlc != nullptr)
    {
        const unsigned int srid = stream2_container_find(Streamed_image_name_uidlc);
        if (srid != 0 && stream2_get_status(srid) != ST_UNLOADED)
        {
            stream2_release(srid);
        }
        Streamed_image_name_uidlc = nullptr;
    }
    ui_unload_document(Doc_handle_uidlc, DFOT_NONE, 0.5f);
    Doc_handle_uidlc = -1;
    ui_relinquish_control_id(Control_id_uidlc);
    ui_relinquish_input_id(Input_id_uidlc);
    Downloaded_levels = nullptr;
    if (gameseq_get_state() != GS_MAINMENU_IDLE && Doc_handle_main != -1)
    {
        main_menu_show_menu(true);
    }
}

// 0x0089FEA0 (:2011)
void ui_view_dlc_handshake(rl_renderer*)
{
    if (Refresh_thread != 0xFFFF)
    {
        return;
    }
    if (Downloaded_levels == nullptr || Num_downloaded_levels < 1)
    {
        if (Streamed_image_name_uidlc != nullptr)
        {
            ui_release_image(Streamed_image_name_uidlc);
            Streamed_image_name_uidlc = nullptr;
        }
        Load_image_next_frame_uidlc = false;
        return;
    }
    const char* image_file = Downloaded_levels[Selected_row_uidlc + Display_start_index_uidlc].image_file;
    if (Load_image_next_frame_uidlc)
    {
        const unsigned int srid = stream2_container_find(image_file);
        if (srid != 0)
        {
            stream2_request(srid);
        }
        Load_image_next_frame_uidlc = false;
        Streamed_image_name_uidlc = image_file;
    }
    if (Streamed_image_name_uidlc != image_file)
    {
        if (Streamed_image_name_uidlc != nullptr)
        {
            if (v_stricmp(image_file, Streamed_image_name_uidlc) == 0)
            {
                return;
            }
            ui_release_image(Streamed_image_name_uidlc);
            Streamed_image_name_uidlc = nullptr;
        }
        Load_image_next_frame_uidlc = true;
    }
}

// 0x00912600 (:2072)
void ui_view_dlc_process()
{
    if (Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
    if (Refresh_thread != 0xFFFF)
    {
        return;
    }
    if (Exit_uidlc)
    {
        if (gameseq_get_pending_state() == GS_LOAD)
        {
            gameseq_pop_state();
        }
        else if (gameseq_get_state() != GS_LOAD && gameseq_get_pending_state() != GS_LOAD)
        {
            Gs_switch_next_state = GS_MAINMENU;
            gameseq_set_state(GS_DLC_SHUTDOWN, true);
        }
    }
    else if (!Loaded_uidlc)
    {
        switch (ui_get_doc_asset_state(Doc_handle_uidlc))
        {
        case ST_UNLOADED:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            [[fallthrough]];
        case ST_ERROR:
            ui_unload_document(Doc_handle_uidlc, DFOT_NONE, 0.5f);
            Doc_handle_uidlc = -1;
            gameseq_pop_state();
            break;
        case ST_LOADING:
            break;
        case ST_LOADED:
            Loaded_uidlc = true;
            ui_view_dlc_create_data_items();
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
    }
    else
    {
        if (Refresh_msgbox_handle != -1)
        {
            ui_hide_msgbox(Refresh_msgbox_handle, false);
            Refresh_msgbox_handle = -1;
        }
        if (!In_purchase_screen)
        {
            dlc_process_delayed_selection();
            ui_view_dlc_process_input();
            ui_view_dlc_process_data_items();
        }
        ui_add_for_processing(Doc_handle_uidlc);
    }
    ui_process(Control_id_uidlc, false);
}

// 0x008F6B50 (:2135)
void ui_view_dlc_render(rl_renderer*)
{
    if (Loaded_uidlc && gameseq_get_state() == GS_VIEW_DLC_SCREEN)
    {
        ui_add_for_rendering(Doc_handle_uidlc);
    }
    ui_render(Control_id_uidlc, false);
}

// 0x0089FF80 (:2147)
void ui_view_dlc_show()
{
    Gs_switch_next_state = GS_VIEW_DLC_SCREEN;
    Gs_switch_transparent = true;
    Gs_switch_pause_beneath = true;
    gameseq_push_state(GS_DLC_INIT, true, true);
}

// 0x0089FFB0 (:2152)
void ui_view_dlc_get_handles(int& doc, int& header, int& row_group, int& hints)
{
    doc = Doc_handle_uidlc;
    header = Data_header_handle_uidlc;
    row_group = Data_row_group_handle_uidlc;
    hints = Data_hints_handle_uidlc;
    In_purchase_screen = true;
}

// 0x0089FFF0 (:2161)
void ui_view_dlc_release_handles()
{
    In_purchase_screen = false;
    dlc_get_single_player_levels(Num_downloaded_levels, Downloaded_levels);
}

// 0x008A0010 (:2209)
bool dlc_menu_enabled()
{
    return true;
}
