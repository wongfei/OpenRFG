// Original: volition/rfg/code/interface/ui_document_menu_wrecking_crew.cpp
//
// The Wrecking Crew main menu (GS_WRECKING_CREW_MAIN_MENU, wrecking.md WC2) on rfg_ui_mp_shell: ten rows (mode,
// players, map, weapon, backpack, walker, stress, rounds, barrels, no time limit) stepped with left/right, the rows a
// mode or a challenge does not use greyed out and skipped. Start begins the match (wrecking_crew_match_begin), the X
// hint goes to character select, the Y hint to the leaderboards (scoreboard), back leaves through GS_WC_SHUTDOWN. The
// map row streams the map's menu image in. ui_wrecking_crew_show (the restart path) enters the menu through
// GS_WC_INIT.

#include "volition/rfg/code/interface/ui_document_menu_wrecking_crew.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/thirdparty/sw_api/sw_api.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_mode_walker.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/game_options.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_core/include/multi_player_uid.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"

#include <cstring>

extern int Doc_handle_main; // 0x016631AC, ui_menu.cpp

// ---- file-private types (rfg.pdb)
enum wc_menu_states
{
    MMS_WC_LOADING = 0,
    MMS_WC_LOADED = 1,
    MMS_WC_DIALOG = 2,
    MMS_WC_UNLOADING = 3,
    MMS_WC_UNLOADED = 4,
    NUM_MP_WC_MENU_STATES = 5,
};

enum mp_menu_exit_actions_uiwre
{
    MMEA_WC_POP = 0,
    MMEA_WC_PUSH = 1,
    NUM_MP_WC_MENU_EXIT_ACTIONS = 2,
};

// size 0x74
struct wc_shell_info
{
    wc_menu_states state; // 0x00
    wc_menu_states pending_state; // 0x04
    wc_menu_states prev_state; // 0x08
    mp_menu_exit_actions_uiwre exit_action; // 0x0C
    game_state exit_push_target; // 0x10
    int doc_handle; // 0x14
    int data_group_handle; // 0x18
    farray<int,16> group_item_handles; // 0x1C
    unsigned int control_id; // 0x6C
    int input_id; // 0x70
};
static_assert(sizeof(wc_shell_info) == 0x74, "wc_shell_info");

// size 0x8
struct menu_option_info_uiwre
{
    const char* m_option; // 0x00
    const char* m_description; // 0x04
};
static_assert(sizeof(menu_option_info_uiwre) == 0x8, "menu_option_info_uiwre");

enum wcmm_hint_choices
{
    WCMM_HINT_INVALID = -1,
    WCMM_HINT_ABORT = 0,
    WCMM_HINT_CHARACTER = 1,
    WCMM_HINT_LEADERBOARD = 2,
    WCMM_HINT_START_GAME = 3,
    NUM_WCMM_HINTS = 4,
};

// The menu rows (Wrecking_crew_main_menu_options' order; rfg.exe uses the bare numbers).
enum
{
    WCMM_ROW_MODE = 0,
    WCMM_ROW_PLAYERS = 1,
    WCMM_ROW_MAP = 2,
    WCMM_ROW_WEAPON = 3,
    WCMM_ROW_BACKPACK = 4,
    WCMM_ROW_WALKER = 5,
    WCMM_ROW_STRESS = 6,
    WCMM_ROW_ROUNDS = 7,
    WCMM_ROW_BARRELS = 8,
    WCMM_ROW_NO_TIME_LIMIT = 9,
    NUM_WCMM_ROWS = 10,
};

static void wcmm_process_data_item_hint_text(vint_data_item* vdi);
static void wcmm_process_data_item_titles(vint_data_item* vdi);
static void wcmm_process_data_item_selection_info(vint_data_item* vdi);
static void wcmm_process_data_item_menu_type(vint_data_item* vdi);
static void wcmm_process_data_item_info_text(vint_data_item* vdi);
static void wcmm_process_data_item_game_info(vint_data_item* vdi);
static void wcmm_process_data_item_press_start(vint_data_item* vdi);

// ---- file-static globals
// The rows' option and description tags (const data, 0x01301230).
static const menu_option_info_uiwre Wrecking_crew_main_menu_options[NUM_WCMM_ROWS] = {
    {"MENU_WRECKING_CREW_MODE", "MENU_WRECKING_CREW_MODE_DESCRIPTION"},
    {"MENU_WRECKING_CREW_PLAYERS", "MENU_WRECKING_CREW_PLAYERS_DESCRIPTION"},
    {"MENU_WRECKING_CREW_MAP", "MENU_WRECKING_CREW_MAP_DESCRIPTION"},
    {"MENU_WRECKING_CREW_WEAPON", "MENU_WRECKING_CREW_WEAPON_DESCRIPTION"},
    {"MENU_WRECKING_CREW_BACKPACK", "MENU_WRECKING_CREW_BACKPACK_DESCRIPTION"},
    {"MENU_WRECKING_CREW_WALKER", "MENU_WRECKING_CREW_WALKER_DESCRIPTION"},
    {"MENU_WRECKING_CREW_STRESS", "MENU_WRECKING_CREW_STRESS_DESCRIPTION"},
    {"MENU_WRECKING_CREW_ROUNDS", "MENU_WRECKING_CREW_ROUNDS_DESCRIPTION"},
    {"MENU_WRECKING_CREW_BARRELS", "MENU_WRECKING_CREW_BARRELS_DESCRIPTION"},
    {"MENU_WRECKING_CREW_NO_TIME_LIMIT", "MENU_WRECKING_CREW_NO_TIME_LIMIT_DESCRIPTION"},
}; // 0x01301230
static wcmm_hint_choices Current_mouse_hint_choice_uiwre = WCMM_HINT_INVALID; // 0x01664B60
static mp_shell_data_item_info Game_options_data_items_uiwre[7] = {
    {MPSDIT_HINT_INFO, wcmm_process_data_item_hint_text},
    {MPSDIT_TITLES, wcmm_process_data_item_titles},
    {MPSDIT_SELECTION_INFO, wcmm_process_data_item_selection_info},
    {MPSDIT_MENU_TYPE, wcmm_process_data_item_menu_type},
    {MPSDIT_INFO_TEXT, wcmm_process_data_item_info_text},
    {MPSDIT_GAME_INFO, wcmm_process_data_item_game_info},
    {MPSDIT_PRESS_START, wcmm_process_data_item_press_start},
}; // 0x01664B64
static const char* Game_options_string_uiwre = {"MENU_WRECKING_CREW"}; // 0x01664B9C
static const char* Character_select_desc_uiwre = {"MENU_CHARACTER_SELECT"}; // 0x01664BA0
static const char* Back_string_uiwre = {"MENU_BACK"}; // 0x01664BA4
static const char* Leaderboard_string = {"MENU_LEADERBOARDS"}; // 0x01664BA8
// The modes' descriptions, by wrecking_crew_modes.
static const char* Wrecking_crew_mode_descriptions[5] = {
    "MENU_WRECKING_CREW_MODE_ESCELATION_DESCRIPTION",
    "MENU_WRECKING_CREW_MODE_BARREL_DASH_DESCRIPTION",
    "MENU_WRECKING_CREW_MODE_RAMPAGE_DESCRIPTION",
    "MENU_WRECKING_CREW_MODE_TOTAL_CHAOS_DESCRIPTION",
    "MENU_WRECKING_CREW_MODE_WALKER_DESCRIPTION",
}; // 0x01664BAC
static int Message_box_handle_uiwre = -1; // 0x01664BC0
// const char* Mp_shell_doc_name (0x0166320C) is used here too; placed in multi_player_menu_system.cpp.
// int Mp_shell_doc_handle (0x0166325C) is used here too; placed in multi_player_menu_system.cpp.
// int Mp_msgbox_handle (0x016632E0) is used here too; placed in ui_document_hud_multiplayer.cpp.
// int Utility_doc_handle (0x01664FF8) is used here too; placed in loading.cpp.
// bink_data Bink_info_main (0x01666148) is used here too; placed in main_menu.cpp.
static wc_shell_info Menu_wrecking_crew_info; // 0x01667498
// farray<unsigned int,16> Controller_slots (0x01667378) is used here too; placed in loading.cpp.
// farray<input_request_data,16> Input_types (0x016673C8) is used here too; placed in loading.cpp.
// int Audio_info[12] (0x02BF21D0) is used here too; placed in ui_menu.cpp.
static int wcmm_hint_config[4]; // 0x02C00EC8
// multi_player_uid Playercard_player_uid (0x02BE9E14) is used here too; placed in multi_player_menu_system.cpp.
// mp_shell_data_item_info* Mp_shell_data_item_infos (0x02C0359C) is used here too; placed in multi_player_menu_system.cpp.
// int Num_mp_shell_data_item_infos (0x02C035A0) is used here too; placed in multi_player_menu_system.cpp.
// mp_message_box_type Mp_msgbox_type (0x02C035A8) is used here too; placed in multi_player_menu_system.cpp.
// vint_data_item* Hint_data_item (0x02C035B0) is used here too; placed in multi_player_menu_system.cpp.
// int Num_hints_added (0x02C035B4) is used here too; placed in multi_player_menu_system.cpp.
static bool Pending_goto_leaderboards; // 0x02C080B1
static int wcmm_num_hints; // 0x02C080B4
static int Wrecking_crew_menu_selection; // 0x02C080B8
static mp_mouse_arrow_click_info Arrow_click_state; // 0x02C080BC

// ---- internal functions

// 0x008B6140
// LTCG: val in ESI, min in EDI. Left steps down, right up (with the previous/next sounds), then the value wraps:
// -1 when it went under min (now max), 1 when over max (now min), else 0.
static int int_process(int* val, int min, int max, bool left_action, bool right_action)
{
    // Magic numbers: Audio_info[7] previous, [8] next.
    if (left_action)
    {
        --*val;
        audiolib_cue_play(Audio_info[7], nullptr, nullptr);
    }
    else if (right_action)
    {
        ++*val;
        audiolib_cue_play(Audio_info[8], nullptr, nullptr);
    }
    if (*val < min)
    {
        *val = max;
        return -1;
    }
    if (*val > max)
    {
        *val = min;
        return 1;
    }
    return 0;
}

// 0x0089D190
// LTCG: val in ECX, min in EDX, max in ESI. No caller in rfg.exe.
static int int_wrap(int* val, int min, int max)
{
    if (*val < min)
    {
        *val = max;
        return -1;
    }
    if (*val > max)
    {
        *val = min;
        return 1;
    }
    return 0;
}

// The map's menu image is released when it is loaded or loading. Original bug kept: for the old map (old_map_index)
// the loading test reads the current map's image (map_index) instead.
static bool wcmm_map_image_loaded_or_loading(int map_index, int loading_map_index)
{
    return ui_get_image_status(wrecking_crew_get_map_image_name(map_index)) == ST_LOADED ||
           ui_get_image_status(wrecking_crew_get_map_image_name(loading_map_index)) == ST_LOADING;
}

// 0x0089C920
static bool wcmm_generic_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    Message_box_handle_uiwre = -1;
    return true;
}

static void wcmm_load_ui_information();
static void wcmm_unload_ui_information();

// 0x008F63F0
// Loading: the map's image is requested; loaded: the data items; unloading: the map images released, the box closed,
// then back to the main menu (through GS_WC_SHUTDOWN) or the pushed state.
static void wcmm_do_state_change()
{
    do
    {
        Menu_wrecking_crew_info.state = Menu_wrecking_crew_info.pending_state;
        if (Menu_wrecking_crew_info.pending_state == MMS_WC_LOADING)
        {
            ui_request_image(wrecking_crew_get_map_image_name(Wrecking_crew_options.map_index));
        }
        else if (Menu_wrecking_crew_info.pending_state == MMS_WC_LOADED)
        {
            wcmm_load_ui_information();
        }
        else
        {
            if (Menu_wrecking_crew_info.pending_state != MMS_WC_UNLOADING)
            {
                return;
            }
            if (wcmm_map_image_loaded_or_loading(Wrecking_crew_options.map_index, Wrecking_crew_options.map_index))
            {
                ui_release_image(wrecking_crew_get_map_image_name(Wrecking_crew_options.map_index));
            }
            if (Wrecking_crew_options.old_map_index != -1)
            {
                if (wcmm_map_image_loaded_or_loading(Wrecking_crew_options.old_map_index, Wrecking_crew_options.map_index))
                {
                    ui_release_image(wrecking_crew_get_map_image_name(Wrecking_crew_options.old_map_index));
                }
                Wrecking_crew_options.old_map_index = -1;
            }
            if (Message_box_handle_uiwre != -1)
            {
                ui_hide_msgbox(Message_box_handle_uiwre, false);
                Message_box_handle_uiwre = -1;
            }
            wcmm_unload_ui_information();
            if (Menu_wrecking_crew_info.exit_action == MMEA_WC_POP)
            {
                game_set_wc_shutdown(GS_MAINMENU);
            }
            else if (Menu_wrecking_crew_info.exit_action == MMEA_WC_PUSH)
            {
                gameseq_push_state(Menu_wrecking_crew_info.exit_push_target, true, true);
            }
            Menu_wrecking_crew_info.pending_state = MMS_WC_UNLOADED;
        }
    } while (Menu_wrecking_crew_info.state != Menu_wrecking_crew_info.pending_state);
}

// 0x0089C930
static void wcmm_set_state(wc_menu_states new_state)
{
    Menu_wrecking_crew_info.pending_state = new_state;
}

// 0x0089C940
static void wcmm_init_state(game_state prev_state)
{
    Menu_wrecking_crew_info.state = MMS_WC_UNLOADED;
    Menu_wrecking_crew_info.pending_state = MMS_WC_UNLOADED;
    Menu_wrecking_crew_info.prev_state = MMS_WC_UNLOADED;
    Menu_wrecking_crew_info.exit_action = MMEA_WC_POP;
    Menu_wrecking_crew_info.exit_push_target = prev_state;
}

// 0x008B6120
static void wcmm_init_info()
{
    Menu_wrecking_crew_info.group_item_handles.clear();
    Menu_wrecking_crew_info.data_group_handle = -1;
}

// 0x008C8520
// Ten rows: {index, option, value} (the template has three elements; the fourth, disabled, is set past it).
static void wcmm_load_menu_option_data_items()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Menu_wrecking_crew_info.data_group_handle);
    for (int i = 0; i < NUM_WCMM_ROWS; i++)
    {
        vint_data_item* item = group->add_data_item(nullptr);
        if (item == nullptr)
        {
            continue;
        }
        vint_variant_type types[3] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 3);
        Menu_wrecking_crew_info.group_item_handles.add((int)item->get_handle());
    }
}

// 0x008C85E0
// No caller in rfg.exe besides the inlined copy in wcmm_load_ui_information.
static void wcmm_load_data_groups()
{
    vint_data_group* group = vint_data_group::create_new("MpShellMenuInfo");
    if (group != nullptr)
    {
        Menu_wrecking_crew_info.data_group_handle = group->get_handle();
        ui_add_data_group(Menu_wrecking_crew_info.doc_handle, Menu_wrecking_crew_info.data_group_handle);
        wcmm_load_menu_option_data_items();
    }
}

// 0x008DB5D0
static void wcmm_load_ui_information()
{
    multi_player_menu_system_shell_load_data_items(Game_options_data_items_uiwre, 7);
    wcmm_load_data_groups();
}

// 0x008ED580
static void wcmm_unload_ui_information()
{
    multi_player_menu_system_shell_unload_data_items();
    wcmm_init_info();
    multi_player_menu_system_close_message_box();
}

// 0x008ED610
// Back, character select, then the leaderboards when Steam has them.
static void wcmm_process_data_item_hint_text(vint_data_item* vdi)
{
    multi_player_menu_system_begin_hints(vdi);
    wcmm_hint_config[1] = -1;
    wcmm_hint_config[2] = -1;
    wcmm_hint_config[3] = -1;
    wcmm_hint_config[0] = WCMM_HINT_ABORT;
    wcmm_num_hints = 1;
    multi_player_menu_system_add_hint(localization_get_display_string_req(Back_string_uiwre, nullptr), CBA_MENU_ABORT);
    wcmm_hint_config[wcmm_num_hints] = WCMM_HINT_CHARACTER;
    wcmm_num_hints++;
    multi_player_menu_system_add_hint(localization_get_display_string_req(Character_select_desc_uiwre, nullptr), CBA_MENU_X);
    if (SW_HasLeaderboards() != 0)
    {
        wcmm_hint_config[wcmm_num_hints] = WCMM_HINT_LEADERBOARD;
        wcmm_num_hints++;
        multi_player_menu_system_add_hint(localization_get_display_string_req(Leaderboard_string, nullptr), CBA_MENU_Y);
    }
    multi_player_menu_system_end_hints();
}

// 0x0089C970
static void wcmm_process_data_item_titles(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(Game_options_string_uiwre));
    vdi->set_element(1, vint_variant(Wrecking_crew_main_menu_options[Wrecking_crew_menu_selection].m_option));
}

// 0x0089CD40
// {shown, description, image, false, ""}: the mode's (or the challenge's) description, the map's name and image (once
// streamed in), else the row's description.
static void wcmm_process_data_item_info_text(vint_data_item* vdi)
{
    if (Wrecking_crew_menu_selection == WCMM_ROW_MODE)
    {
        vdi->set_element(0, vint_variant(true));
        if (Wrecking_crew_options.current_challenge_id == -1)
        {
            vdi->set_element(1, vint_variant(Wrecking_crew_mode_descriptions[Wrecking_crew_options.mode]));
        }
        else
        {
            vdi->set_element(1, vint_variant("MENU_WC_MODE_CHALLENGE_DESCRIPTION"));
        }
        vdi->set_element(2, vint_variant(""));
    }
    else if (Wrecking_crew_menu_selection == WCMM_ROW_MAP)
    {
        vdi->set_element(0, vint_variant(true));
        vdi->set_element(1, vint_variant(wrecking_crew_get_map_display_name(Wrecking_crew_options.map_index)));
        if (ui_get_image_status(wrecking_crew_get_map_image_name(Wrecking_crew_options.map_index)) == ST_LOADED)
        {
            vdi->set_element(2, vint_variant(wrecking_crew_get_map_image_name(Wrecking_crew_options.map_index)));
        }
        else
        {
            vdi->set_element(2, vint_variant(""));
        }
    }
    else
    {
        vdi->set_element(0, vint_variant(true));
        vdi->set_element(1, vint_variant(Wrecking_crew_main_menu_options[Wrecking_crew_menu_selection].m_description));
        vdi->set_element(2, vint_variant(""));
    }
    vdi->set_element(3, vint_variant(false));
    vdi->set_element(4, vint_variant(""));
}

// 0x0089D0C0
static void wcmm_process_data_item_selection_info(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(Wrecking_crew_menu_selection));
    vdi->set_element(1, vint_variant((const char*)nullptr));
    vdi->set_element(2, vint_variant(true));
}

// 0x0089D150
static void wcmm_process_data_item_menu_type(vint_data_item* vdi)
{
    // Magic number: menu type 4 (the wrecking crew layout of rfg_ui_mp_shell).
    vdi->set_element(0, vint_variant(4));
}

// 0x0089CAF0
// On the map row: {true, "", the map's name, then eleven empty texts}; else hidden.
static void wcmm_process_data_item_game_info(vint_data_item* vdi)
{
    if (Wrecking_crew_menu_selection != WCMM_ROW_MAP)
    {
        vdi->set_element(0, vint_variant(false));
        return;
    }
    vdi->set_element(0, vint_variant(true));
    vdi->set_element(1, vint_variant(""));
    vdi->set_element(2, vint_variant(wrecking_crew_get_map_display_name(Wrecking_crew_options.map_index)));
    // Magic number: elements 3 to 13, the game info item's other fields.
    for (int i = 3; i <= 13; i++)
    {
        vdi->set_element(i, vint_variant(""));
    }
}

// 0x0089C9E0
// {true, the start button's image, ready, start game}.
static void wcmm_process_data_item_press_start(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(true));
    char image[32];
    char text[128];
    controls_get_button_image_name(image, sizeof(image), CBA_MENU_START, Player_controller_index);
    keen::formatString(text, sizeof(text), "[image:%s]", image);
    vdi->set_element(1, vint_variant((const char*)text));
    vdi->set_element(2, vint_variant("MENU_MP_WORD_READY"));
    vdi->set_element(3, vint_variant("MENU_START_GAME"));
}

// 0x008ED760
// Each row: {index + 1, option, value, disabled}. A row is disabled when the mode (or a challenge) does not use it.
static void wcmm_process_data_group_menu_options()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Menu_wrecking_crew_info.data_group_handle);
    if (group == nullptr)
    {
        return;
    }
    // Original: the text is not cleared between rows, so a row whose value is not found shows the previous row's.
    wchar_t text[128];
    char encoded[384];
    const int count = (int)group->get_num_data_items();
    for (int i = 0; i < count; i++)
    {
        vint_data_item* item = vint_data_item::find((unsigned int)Menu_wrecking_crew_info.group_item_handles[i]);
        if (item == nullptr)
        {
            continue;
        }
        item->set_element(0, vint_variant(i + 1));
        item->set_element(1, vint_variant(Wrecking_crew_main_menu_options[i].m_option));
        item->set_element(3, vint_variant(false));
        const bool challenge = Wrecking_crew_options.current_challenge_id != -1;
        bool disable = false;
        // Magic number: 0x80, the text buffer's size in characters.
        switch (i)
        {
        case WCMM_ROW_MODE:
            if (!challenge)
            {
                const char* name = wrecking_crew_get_mode_display_name(Wrecking_crew_options.mode);
                if (name != nullptr)
                {
                    strncpy_safe(text, localization_get_display_string_req(name, nullptr), 0x80);
                }
            }
            else
            {
                const wchar_t* challenge_text = localization_get_display_string_req("HM_WC_MODE_CHALLENGE", nullptr);
                if (challenge_text != nullptr)
                {
                    strncpy_safe(text, challenge_text, 0x80);
                }
                else
                {
                    sprintf(text, L"%hs", "HM_WC_MODE_CHALLENGE");
                }
            }
            break;
        case WCMM_ROW_PLAYERS:
            _snprintf_safe(encoded, 0x80, "%d", Wrecking_crew_options.num_players);
            copy_to_wide(text, encoded, 0x80);
            break;
        case WCMM_ROW_MAP:
        {
            const wchar_t* unknown = localization_get_display_string_req("MENU_UNKNOWN", nullptr);
            strncpy_safe(text, unknown != nullptr ? unknown : L"", 0x80);
            const char* name = wrecking_crew_get_map_display_name(Wrecking_crew_options.map_index);
            if (name != nullptr)
            {
                const wchar_t* map_text = localization_get_display_string_req(name, nullptr);
                if (map_text != nullptr)
                {
                    strncpy_safe(text, map_text, 0x80);
                }
            }
            break;
        }
        case WCMM_ROW_WEAPON:
            if (!challenge && Wrecking_crew_options.weapon_index != -1)
            {
                const wrecking_crew_weapon_info* wc_info = wrecking_crew_get_weapon_info(Wrecking_crew_options.weapon_index);
                if (wc_info == nullptr)
                {
                    // Original: the rest of the rows are left as they were.
                    return;
                }
                const weapon_info* w_info = weapons_get_weapon_info(wc_info->w_info_handle);
                if (w_info != nullptr)
                {
                    strncpy_safe(text, localization_get_display_string_req(w_info->weapon_inv_item_info->display_name, nullptr), 0x80);
                }
            }
            else
            {
                strncpy_safe(text, localization_get_display_string_req("MENU_RANDOM", nullptr), 0x80);
            }
            disable = Wrecking_crew_options.mode == WCM_WALKER || challenge;
            break;
        case WCMM_ROW_BACKPACK:
            if (!challenge && Wrecking_crew_options.backpack_index != -1)
            {
                const wrecking_crew_backpack_info* wc_info = wrecking_crew_get_backpack_info(Wrecking_crew_options.backpack_index);
                if (wc_info == nullptr)
                {
                    // Original: the rest of the rows are left as they were.
                    return;
                }
                const multi_backpack_info* b_info = multi_game_backpack_get_info(wc_info->backpack_type);
                if (b_info != nullptr)
                {
                    strncpy_safe(text, localization_get_display_string_req(b_info->display_name, nullptr), 0x80);
                }
            }
            else
            {
                strncpy_safe(text, localization_get_display_string_req("MENU_RANDOM", nullptr), 0x80);
            }
            disable = Wrecking_crew_options.mode == WCM_MAYHEM || Wrecking_crew_options.mode == WCM_WALKER || challenge;
            break;
        case WCMM_ROW_WALKER:
            if (!challenge && Wrecking_crew_options.mode == WCM_WALKER)
            {
                if (Wrecking_crew_options.walker_choice == WCWC_NONE)
                {
                    strncpy_safe(text, localization_get_display_string_req("MENU_RANDOM", nullptr), 0x80);
                }
                else
                {
                    strncpy_safe(text, localization_get_display_string_req(wrecking_crew_get_walker_display_name(Wrecking_crew_options.walker_choice), nullptr), 0x80);
                }
                // Set explicitly (false) in rfg.exe.
                item->set_element(3, vint_variant(false));
            }
            else
            {
                strncpy_safe(text, localization_get_display_string_req("MENU_NONE", nullptr), 0x80);
                disable = true;
            }
            break;
        case WCMM_ROW_STRESS:
        {
            const char* tag = nullptr;
            if (challenge)
            {
                tag = "MENU_RANDOM";
            }
            else if (Wrecking_crew_options.stress_mode == WCSS_OFF)
            {
                tag = "MENU_OFF";
            }
            else if (Wrecking_crew_options.stress_mode == WCSS_LOW)
            {
                tag = "MENU_LOW";
            }
            else if (Wrecking_crew_options.stress_mode == WCSS_HIGH)
            {
                tag = "MENU_HIGH";
            }
            if (tag == nullptr)
            {
                // Original: an unknown stress mode keeps the previous text (and is never disabled).
                break;
            }
            strncpy_safe(text, localization_get_display_string_req(tag, nullptr), 0x80);
            disable = challenge;
            break;
        }
        case WCMM_ROW_ROUNDS:
            // Magic number: a challenge plays three rounds.
            sprintf(text, L"%d", challenge ? 3 : Wrecking_crew_options.rounds_to_win);
            disable = challenge;
            break;
        case WCMM_ROW_BARRELS:
            if (challenge)
            {
                strncpy_safe(text, localization_get_display_string_req("MENU_RANDOM", nullptr), 0x80);
            }
            else
            {
                // Original: an unbounded copy.
                strcpy(text, localization_get_display_string_req(Wrecking_crew_options.spawn_barrels ? "MENU_ON" : "MENU_OFF", nullptr));
            }
            disable = challenge || Wrecking_crew_options.mode == WCM_FALL_GUY;
            break;
        case WCMM_ROW_NO_TIME_LIMIT:
            // Original: an unbounded copy.
            strcpy(text, localization_get_display_string_req(!challenge && Wrecking_crew_options.free_play ? "MENU_ON" : "MENU_OFF", nullptr));
            disable = challenge || Wrecking_crew_options.mode == WCM_RAMPAGE;
            break;
        default:
            break;
        }
        if (disable)
        {
            item->set_element(3, vint_variant(true));
        }
        vint_encode_wide_text(encoded, 0x180, text);
        item->set_element(2, vint_variant((const char*)encoded));
    }
}

// 0x008EE060
static void wcmm_process_data_items()
{
    multi_player_menu_system_shell_process_data_items();
    wcmm_process_data_group_menu_options();
}

// 0x008F65B0
// The document loaded: the menu loads (and the DLC is checked for corrupt content); failed or missing: the menu goes.
static void wcmm_process_loading()
{
    switch (ui_get_doc_asset_state(Menu_wrecking_crew_info.doc_handle))
    {
    case ST_LOADING:
        return;
    case ST_LOADED:
        wcmm_set_state(MMS_WC_LOADED);
        dlc_found_corrupt_content();
        return;
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        [[fallthrough]];
    case ST_ERROR:
        multi_player_menu_system_shell_unload_doc(Menu_wrecking_crew_info.doc_handle);
        wcmm_set_state(MMS_WC_UNLOADING);
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
}

// 0x008B61A0
// No caller in rfg.exe: a row is selectable unless it is disabled.
static bool wcmm_selection_enabled(int selection)
{
    if (vint_data_group::find((unsigned int)Menu_wrecking_crew_info.data_group_handle) != nullptr)
    {
        vint_data_item* item = vint_data_item::find((unsigned int)Menu_wrecking_crew_info.group_item_handles[selection]);
        if (item != nullptr && (bool)item->get_element(3))
        {
            return false;
        }
    }
    return true;
}

// 0x008B61F0
// LTCG: new_selection in ESI. A disabled row is not selected; otherwise the selection wraps, with the up/down sound.
static void wcmm_input_set_selection(int new_selection, bool& events_consumed)
{
    if (vint_data_group::find((unsigned int)Menu_wrecking_crew_info.data_group_handle) != nullptr)
    {
        vint_data_item* item = vint_data_item::find((unsigned int)Menu_wrecking_crew_info.group_item_handles[new_selection]);
        if (item != nullptr && (bool)item->get_element(3))
        {
            return;
        }
    }
    const int old_selection = Wrecking_crew_menu_selection;
    const int num = Menu_wrecking_crew_info.group_item_handles.size();
    Wrecking_crew_menu_selection = new_selection;
    if (new_selection < 0)
    {
        Wrecking_crew_menu_selection = num - 1;
    }
    if (new_selection > num - 1)
    {
        Wrecking_crew_menu_selection = 0;
    }
    // Magic numbers: Audio_info[1] down, [0] up, [11] unavailable (compared with the unwrapped selection).
    if (old_selection < new_selection)
    {
        audiolib_cue_play(Audio_info[1], nullptr, nullptr);
    }
    else if (old_selection > new_selection)
    {
        audiolib_cue_play(Audio_info[0], nullptr, nullptr);
    }
    else
    {
        audiolib_cue_play(Audio_info[11], nullptr, nullptr);
    }
}

// 0x008DB610
// The hints (with no message box up), the start click, the row arrows, else the rows under the mouse.
static void wcmm_input_process_mouse(bool& events_consumed)
{
    const int doc_handle = Menu_wrecking_crew_info.doc_handle;
    if (Utility_doc_handle == -1 || !ui_is_msgbox_active(-2))
    {
        int hint = -1;
        if (ui_mouse_over_hint(kHintText3, doc_handle, true))
        {
            hint = 2;
        }
        if (ui_mouse_over_hint(kHintText2, doc_handle, true))
        {
            hint = 1;
        }
        if (ui_mouse_over_hint(kHintText1, doc_handle, true))
        {
            Current_mouse_hint_choice_uiwre = (wcmm_hint_choices)wcmm_hint_config[0];
            return;
        }
        if (hint >= 0)
        {
            Current_mouse_hint_choice_uiwre = (wcmm_hint_choices)wcmm_hint_config[hint];
            return;
        }
    }
    const char* start_names[1] = {"game_start_click"};
    if (ui_mouse_process_hints(start_names, 1, Menu_wrecking_crew_info.doc_handle, true) >= 0)
    {
        Current_mouse_hint_choice_uiwre = WCMM_HINT_START_GAME;
    }
    multi_player_menu_system_handle_arrow_clicks(Arrow_click_state, Menu_wrecking_crew_info.doc_handle);
    if (Arrow_click_state != MP_ARROW_CLICK_NONE)
    {
        return;
    }
    const int selection = multi_player_menu_system_handle_menu_mouse_input(Wrecking_crew_menu_selection, Menu_wrecking_crew_info.group_item_handles.size(), Menu_wrecking_crew_info.input_id, Menu_wrecking_crew_info.doc_handle, events_consumed);
    if (selection >= 0 && selection != Wrecking_crew_menu_selection)
    {
        wcmm_input_set_selection(selection, events_consumed);
    }
}

// 0x008DB740
// No caller in rfg.exe (wcmm_process_loaded does this itself). LTCG: the flag's stack slot is ECX's low bytes, so it
// starts as whatever ECX held; here it starts false.
static bool wcmm_process_input()
{
    bool events_consumed = false;
    Current_mouse_hint_choice_uiwre = WCMM_HINT_INVALID;
    wcmm_input_process_mouse(events_consumed);
    return false;
}

// Releases the map images when the menu is left for another state.
static void wcmm_release_map_images()
{
    if (Wrecking_crew_options.map_index != -1)
    {
        ui_release_image(wrecking_crew_get_map_image_name(Wrecking_crew_options.map_index));
    }
    if (Wrecking_crew_options.old_map_index != -1)
    {
        ui_release_image(wrecking_crew_get_map_image_name(Wrecking_crew_options.old_map_index));
        Wrecking_crew_options.old_map_index = -1;
    }
}

// 0x00905420
// Up/down moves through the rows (skipping the disabled ones), left/right changes the selected row's option. X goes to
// character select, Y to the leaderboards (a signed in player only), back (or an invite) leaves, start begins the match.
static void wcmm_process_loaded()
{
    wcmm_process_data_items();
    const int input_id = Menu_wrecking_crew_info.input_id;
    // Magic numbers: buttons 3/7 left (pad/keys), 4/8 right, 1/5 up, 2/6 down, 11 back, 9 start, 14 X, 15 Y.
    bool left = ui_get_button_info(3, input_id) > 0.0f || ui_get_button_info(7, input_id) > 0.0f;
    bool right = ui_get_button_info(4, input_id) > 0.0f || ui_get_button_info(8, input_id) > 0.0f;
    const bool up = ui_get_button_info(1, input_id) > 0.0f || ui_get_button_info(5, input_id) > 0.0f;
    const bool down = ui_get_button_info(2, input_id) > 0.0f || ui_get_button_info(6, input_id) > 0.0f;
    bool abort = ui_get_button_info(11, input_id) > 0.0f;
    bool start = ui_get_button_info(9, input_id) > 0.0f;
    bool character = ui_get_button_info(14, input_id) > 0.0f;
    bool leaderboard = ui_get_button_info(15, input_id) > 0.0f;
    bool events_consumed = false;
    Current_mouse_hint_choice_uiwre = WCMM_HINT_INVALID;
    wcmm_input_process_mouse(events_consumed);
    abort = abort || Current_mouse_hint_choice_uiwre == WCMM_HINT_ABORT;
    character = character || Current_mouse_hint_choice_uiwre == WCMM_HINT_CHARACTER;
    leaderboard = leaderboard || Current_mouse_hint_choice_uiwre == WCMM_HINT_LEADERBOARD;
    start = start || Current_mouse_hint_choice_uiwre == WCMM_HINT_START_GAME;
    right = right || Arrow_click_state == MP_ARROW_CLICK_RIGHT;
    left = left || Arrow_click_state == MP_ARROW_CLICK_LEFT;

    // Magic numbers: Audio_info[4] select, [5] back, [0] up, [1] down, [11] unavailable, [7] previous, [8] next.
    if (character)
    {
        Menu_wrecking_crew_info.exit_action = MMEA_WC_PUSH;
        Menu_wrecking_crew_info.exit_push_target = GS_WRECKING_CREW_CHARACTER_SELECT;
        Menu_wrecking_crew_info.pending_state = MMS_WC_UNLOADING;
        audiolib_cue_play(Audio_info[4], nullptr, nullptr);
        wcmm_release_map_images();
        return;
    }
    if (gameseq_get_state() == GS_WRECKING_CREW_MAIN_MENU && (leaderboard || Pending_goto_leaderboards) && Message_box_handle_uiwre == -1 && SW_HasLeaderboards() != 0)
    {
        Pending_goto_leaderboards = false;
        if (!My_player_uid.is_valid())
        {
            Message_box_handle_uiwre = ui_add_msgbox(MBT_CONFIG_CONTROL, "REMOTE_GAME_SESSION_ERROR_TITLE", "GAMESESSION_INTERACTION_DESC_0x0000500F", wcmm_generic_cb, false, false, "MENU_ACCEPT", nullptr, false);
            if (Message_box_handle_uiwre != -1)
            {
                ui_msgbox_add_caption_item(Message_box_handle_uiwre, "GAMESESSION_INTERACTION_RESPONSE_0x00000001", false, false);
            }
            return;
        }
        Menu_wrecking_crew_info.exit_action = MMEA_WC_PUSH;
        Menu_wrecking_crew_info.exit_push_target = GS_WRECKING_CREW_SCOREBOARD;
        Menu_wrecking_crew_info.pending_state = MMS_WC_UNLOADING;
        audiolib_cue_play(Audio_info[4], nullptr, nullptr);
        wcmm_release_map_images();
        return;
    }

    // Up/down: the selection wraps; a disabled row is stepped over (Original bug kept: stepping up from the weapon row
    // in a challenge, reachable only with the mouse, leaves a negative selection).
    const int old_selection = Wrecking_crew_menu_selection;
    const int num = Menu_wrecking_crew_info.group_item_handles.size();
    int direction = 0;
    if (up)
    {
        Wrecking_crew_menu_selection = Wrecking_crew_menu_selection - 1 + num;
        direction = -1;
    }
    else if (down)
    {
        Wrecking_crew_menu_selection++;
        direction = 1;
    }
    Wrecking_crew_menu_selection %= num;
    if (direction != 0)
    {
        int skip = 0;
        const int sel = Wrecking_crew_menu_selection;
        if (Wrecking_crew_options.current_challenge_id != -1 && (sel == WCMM_ROW_WEAPON || sel == WCMM_ROW_NO_TIME_LIMIT))
        {
            // Magic number: a challenge disables rows 3 to 9; 7 steps over them all.
            skip = 7;
        }
        else if (Wrecking_crew_options.mode == WCM_FALL_GUY)
        {
            if (sel == WCMM_ROW_BARRELS || sel == WCMM_ROW_WALKER)
            {
                skip = 1;
            }
        }
        else if (Wrecking_crew_options.mode == WCM_MAYHEM)
        {
            // Backpack and walker.
            if (sel == WCMM_ROW_BACKPACK || sel == WCMM_ROW_WALKER)
            {
                skip = 2;
            }
        }
        else if (Wrecking_crew_options.mode == WCM_WALKER)
        {
            // Weapon and backpack.
            if (sel == WCMM_ROW_BACKPACK || sel == WCMM_ROW_WEAPON)
            {
                skip = 2;
            }
        }
        else if (sel == WCMM_ROW_WALKER || (Wrecking_crew_options.mode == WCM_RAMPAGE && sel == WCMM_ROW_NO_TIME_LIMIT))
        {
            skip = 1;
        }
        if (skip != 0)
        {
            Wrecking_crew_menu_selection = (skip * direction + Wrecking_crew_menu_selection) % num;
        }
        if (Wrecking_crew_menu_selection > old_selection)
        {
            audiolib_cue_play(Audio_info[0], nullptr, nullptr);
        }
        else if (Wrecking_crew_menu_selection < old_selection)
        {
            audiolib_cue_play(Audio_info[1], nullptr, nullptr);
        }
        else
        {
            audiolib_cue_play(Audio_info[11], nullptr, nullptr);
        }
    }

    switch (Wrecking_crew_menu_selection)
    {
    case WCMM_ROW_MODE:
    {
        if (Wrecking_crew_options.current_challenge_id != -1)
        {
            // A challenge: left goes to the last mode (walker with the behemoth, else total chaos), right to mayhem.
            int value = 0;
            const int wrapped = int_process(&value, 0, 0, left, right);
            if (wrapped < 0)
            {
                Wrecking_crew_options.current_challenge_id = -1;
                Wrecking_crew_options.mode = (wrecking_crew_modes)(WCM_TOTAL_CHAOS + (wrecking_crew_behemoth_enabled() ? 1 : 0));
            }
            else if (wrapped > 0)
            {
                Wrecking_crew_options.current_challenge_id = -1;
                Wrecking_crew_options.mode = WCM_MAYHEM;
            }
            break;
        }
        int mode = Wrecking_crew_options.mode;
        const int wrapped = int_process(&mode, WCM_MAYHEM, WCM_WALKER, left, right);
        bool to_challenge = wrapped != 0;
        if (!to_challenge)
        {
            if (mode == WCM_WALKER)
            {
                if (!wrecking_crew_behemoth_enabled())
                {
                    // Without the behemoth the walker slot is the challenge.
                    to_challenge = true;
                }
                else
                {
                    Wrecking_crew_options.weapon_index = -1;
                    Wrecking_crew_options.backpack_index = -1;
                }
            }
            else
            {
                if (mode == WCM_FALL_GUY)
                {
                    Wrecking_crew_options.spawn_barrels = true;
                }
                else if (mode == WCM_MAYHEM)
                {
                    Wrecking_crew_options.backpack_index = 0;
                }
                else if (mode == WCM_RAMPAGE)
                {
                    Wrecking_crew_options.free_play = false;
                }
                Wrecking_crew_options.walker_choice = WCWC_NONE;
            }
        }
        if (!to_challenge)
        {
            Wrecking_crew_options.mode = (wrecking_crew_modes)mode;
            // The weapon steps on until the mode allows it (or random).
            const int num_weapons = wrecking_crew_get_num_weapons();
            while (Wrecking_crew_options.weapon_index != -1 && !wrecking_crew_is_weapon_available_for_mode(Wrecking_crew_options.weapon_index, Wrecking_crew_options.mode))
            {
                int_process(&Wrecking_crew_options.weapon_index, -1, num_weapons - 1, false, true);
            }
            break;
        }
        // The challenge: random weapon and backpack, on the first map from here that has one. Original bug kept: with
        // no challenge map at all this loops forever.
        Wrecking_crew_options.weapon_index = -1;
        Wrecking_crew_options.backpack_index = -1;
        Wrecking_crew_options.current_challenge_id = wrecking_crew_get_map_challenge_id(Wrecking_crew_options.map_index);
        while (Wrecking_crew_options.current_challenge_id == -1)
        {
            Wrecking_crew_options.map_index = (Wrecking_crew_options.map_index + 1) % wrecking_crew_get_num_maps();
            Wrecking_crew_options.current_challenge_id = wrecking_crew_get_map_challenge_id(Wrecking_crew_options.map_index);
        }
        break;
    }
    case WCMM_ROW_PLAYERS:
    {
        const int old_players = Wrecking_crew_options.num_players;
        // Magic numbers: one to four players.
        int_process(&Wrecking_crew_options.num_players, 1, 4, left, right);
        const int new_players = Wrecking_crew_options.num_players;
        if (new_players <= old_players)
        {
            break;
        }
        // The new players take the first characters nobody has.
        bool used[64];
        memset(used, 0, sizeof(used));
        const int num_characters = Wrecking_crew_characters.size();
        for (int i = 0; i < old_players; i++)
        {
            used[Wc_player_infos[i].character_index] = true;
        }
        for (int i = old_players; i < new_players; i++)
        {
            for (int c = 0; c < num_characters; c++)
            {
                if (!used[c])
                {
                    Wc_player_infos[i].character_index = c;
                    used[c] = true;
                    break;
                }
            }
        }
        break;
    }
    case WCMM_ROW_MAP:
    {
        const int num_maps = wrecking_crew_get_num_maps();
        const stream2_status_val status = ui_get_image_status(wrecking_crew_get_map_image_name(Wrecking_crew_options.map_index));
        if (left || right)
        {
            if (Wrecking_crew_options.old_map_index != -1)
            {
                ui_release_image(wrecking_crew_get_map_image_name(Wrecking_crew_options.old_map_index));
            }
            Wrecking_crew_options.old_map_index = Wrecking_crew_options.map_index;
        }
        // Locked maps (and, in a challenge, maps without one) are stepped over. Original bug kept: with no left/right a
        // locked current map loops forever.
        do
        {
            do
            {
                int_process(&Wrecking_crew_options.map_index, 0, num_maps - 1, left, right);
            } while (wrecking_crew_is_map_locked(Wrecking_crew_options.map_index));
        } while (Wrecking_crew_options.current_challenge_id != -1 && wrecking_crew_get_map_challenge_id(Wrecking_crew_options.map_index) == -1);
        if (status == ST_UNLOADED || left || right)
        {
            ui_request_image(wrecking_crew_get_map_image_name(Wrecking_crew_options.map_index));
        }
        if (Wrecking_crew_options.current_challenge_id != -1)
        {
            Wrecking_crew_options.current_challenge_id = wrecking_crew_get_map_challenge_id(Wrecking_crew_options.map_index);
        }
        break;
    }
    case WCMM_ROW_WEAPON:
    {
        // -1 is random. Original bug kept: with no left/right an unavailable weapon loops forever.
        const int num_weapons = wrecking_crew_get_num_weapons();
        int_process(&Wrecking_crew_options.weapon_index, -1, num_weapons - 1, left, right);
        while (Wrecking_crew_options.weapon_index != -1 && !wrecking_crew_is_weapon_available_for_mode(Wrecking_crew_options.weapon_index, Wrecking_crew_options.mode))
        {
            int_process(&Wrecking_crew_options.weapon_index, -1, num_weapons - 1, left, right);
        }
        break;
    }
    case WCMM_ROW_BACKPACK:
    {
        // -1 is random. Original bug kept: with no left/right an unavailable backpack loops forever.
        const int num_backpacks = wrecking_crew_get_num_backpacks();
        int_process(&Wrecking_crew_options.backpack_index, -1, num_backpacks - 1, left, right);
        while (Wrecking_crew_options.backpack_index != -1 && !wrecking_crew_is_backpack_available_for_mode(Wrecking_crew_options.backpack_index, Wrecking_crew_options.mode))
        {
            int_process(&Wrecking_crew_options.backpack_index, -1, num_backpacks - 1, left, right);
        }
        break;
    }
    case WCMM_ROW_WALKER:
    {
        // WCWC_NONE (random) to WCWC_FLAMER.
        int walker = Wrecking_crew_options.walker_choice;
        int_process(&walker, WCWC_NONE, WCWC_FLAMER, left, right);
        Wrecking_crew_options.walker_choice = (wrecking_crew_walker_choices)walker;
        break;
    }
    case WCMM_ROW_STRESS:
    {
        int stress = Wrecking_crew_options.stress_mode;
        int_process(&stress, WCSS_OFF, WCSS_HIGH, left, right);
        Wrecking_crew_options.stress_mode = (wrecking_crew_stress_modes)stress;
        break;
    }
    case WCMM_ROW_ROUNDS:
        // Magic numbers: one to five round wins.
        int_process(&Wrecking_crew_options.rounds_to_win, 1, 5, left, right);
        break;
    case WCMM_ROW_BARRELS:
        // Fall Guy always has barrels.
        if (left && Wrecking_crew_options.mode != WCM_FALL_GUY)
        {
            audiolib_cue_play(Audio_info[7], nullptr, nullptr);
            Wrecking_crew_options.spawn_barrels = !Wrecking_crew_options.spawn_barrels;
        }
        else if (right && Wrecking_crew_options.mode != WCM_FALL_GUY)
        {
            audiolib_cue_play(Audio_info[8], nullptr, nullptr);
            Wrecking_crew_options.spawn_barrels = !Wrecking_crew_options.spawn_barrels;
        }
        break;
    case WCMM_ROW_NO_TIME_LIMIT:
        if (left || right)
        {
            audiolib_cue_play(left ? Audio_info[7] : Audio_info[8], nullptr, nullptr);
            Wrecking_crew_options.free_play = !Wrecking_crew_options.free_play;
        }
        break;
    default:
        if (left || right)
        {
            audiolib_cue_play(Audio_info[11], nullptr, nullptr);
        }
        break;
    }

    if (abort || multi_core_platform_has_pending_invite())
    {
        const bool invite = multi_core_platform_has_pending_invite();
        if (abort)
        {
            audiolib_cue_play(Audio_info[5], nullptr, nullptr);
        }
        // An invite starts the multiplayer front end music.
        gamemusic_frontend_start(invite);
        Menu_wrecking_crew_info.exit_action = MMEA_WC_POP;
        Menu_wrecking_crew_info.pending_state = MMS_WC_UNLOADING;
    }
    if (start)
    {
        wrecking_crew_match_begin();
    }
}

// 0x0089D1B0
// No caller in rfg.exe (wcmm_process_loaded sets the fields itself).
static void wcmm_ui_push(game_state new_state)
{
    Menu_wrecking_crew_info.exit_action = MMEA_WC_PUSH;
    Menu_wrecking_crew_info.exit_push_target = new_state;
    Menu_wrecking_crew_info.pending_state = MMS_WC_UNLOADING;
}

// 0x0089D1D0
// No caller in rfg.exe (wcmm_process_loaded sets the fields itself).
static void wcmm_ui_pop()
{
    Menu_wrecking_crew_info.exit_action = MMEA_WC_POP;
    Menu_wrecking_crew_info.pending_state = MMS_WC_UNLOADING;
}

// ---- external interface

// 0x009113A0
// The main menu hidden, the options from the cache, each player's character made valid and unique, then the shell
// document, a control id and an input id.
static void ui_document_menu_wrecking_crew_enter(game_state prev_state)
{
    main_menu_show_menu(false);
    game_options_load_from_cache();
    const int num_characters = Wrecking_crew_characters.size();
    // Original bug kept: the index equal to the character count passes as valid.
    for (int i = 0; i < 4; i++)
    {
        if (Wc_player_infos[i].character_index < 0 || Wc_player_infos[i].character_index > num_characters)
        {
            Wc_player_infos[i].character_index = 0;
        }
    }
    // A player (last first) sharing a character with an earlier one takes the first character nobody has (left as it
    // was when none is free).
    for (int i = 3; i >= 0; i--)
    {
        for (int j = 0; j < i; j++)
        {
            if (Wc_player_infos[i].character_index != Wc_player_infos[j].character_index)
            {
                continue;
            }
            for (int c = 0; c < num_characters; c++)
            {
                bool taken = false;
                for (int k = 0; k < 4; k++)
                {
                    if (Wc_player_infos[k].character_index == c)
                    {
                        taken = true;
                        break;
                    }
                }
                if (!taken)
                {
                    Wc_player_infos[i].character_index = c;
                    break;
                }
            }
            break;
        }
    }
    wcmm_init_state(prev_state);
    wcmm_init_info();
    Pending_goto_leaderboards = false;
    Message_box_handle_uiwre = -1;
    multi_player_menu_system_shell_load_doc(Menu_wrecking_crew_info.doc_handle, true);
    Menu_wrecking_crew_info.control_id = ui_request_control_id();
    Menu_wrecking_crew_info.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
}

// 0x008F6660
// The map images released, the document unloaded, the ids given back; the main menu shown again (unless idle), and the
// scoreboard shut down when leaving for the main menu.
static void ui_document_menu_wrecking_crew_exit(game_state new_state)
{
    // Original: the release check is ui_release_image's own again, so the status is read twice.
    if (wcmm_map_image_loaded_or_loading(Wrecking_crew_options.map_index, Wrecking_crew_options.map_index))
    {
        ui_release_image(wrecking_crew_get_map_image_name(Wrecking_crew_options.map_index));
    }
    if (Wrecking_crew_options.old_map_index != -1)
    {
        if (wcmm_map_image_loaded_or_loading(Wrecking_crew_options.old_map_index, Wrecking_crew_options.map_index))
        {
            ui_release_image(wrecking_crew_get_map_image_name(Wrecking_crew_options.old_map_index));
        }
        Wrecking_crew_options.old_map_index = -1;
    }
    wcmm_unload_ui_information();
    multi_player_menu_system_shell_unload_doc(Menu_wrecking_crew_info.doc_handle);
    ui_relinquish_control_id(Menu_wrecking_crew_info.control_id);
    ui_relinquish_input_id(Menu_wrecking_crew_info.input_id);
    if (gameseq_get_state() != GS_MAINMENU_IDLE && Doc_handle_main != -1)
    {
        main_menu_show_menu(true);
    }
    if (new_state == GS_MAINMENU || new_state == GS_WC_SHUTDOWN)
    {
        multi_core_platform_scoreboard_shutdown();
    }
}

// 0x00911600
static void ui_document_menu_wrecking_crew_process()
{
    if (Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
    ui_add_for_processing(Menu_wrecking_crew_info.doc_handle);
    switch (Menu_wrecking_crew_info.state)
    {
    case MMS_WC_LOADING:
        wcmm_process_loading();
        break;
    case MMS_WC_LOADED:
        wcmm_process_loaded();
        break;
    case MMS_WC_UNLOADED:
        wcmm_set_state(MMS_WC_LOADING);
        break;
    default:
        break;
    }
    ui_process(Menu_wrecking_crew_info.control_id, false);
}

// 0x008F6860
static void ui_document_menu_wrecking_crew_handshake(rl_renderer*)
{
    Menu_wrecking_crew_info.prev_state = Menu_wrecking_crew_info.state;
    if (Menu_wrecking_crew_info.pending_state != Menu_wrecking_crew_info.state)
    {
        wcmm_do_state_change();
    }
}

// 0x008F6880
// Drawn once loaded for a whole frame.
static void ui_document_menu_wrecking_crew_render(rl_renderer*)
{
    if (Menu_wrecking_crew_info.doc_handle == -1 || Menu_wrecking_crew_info.control_id == 0)
    {
        return;
    }
    if (Menu_wrecking_crew_info.state == MMS_WC_LOADED && Menu_wrecking_crew_info.prev_state == MMS_WC_LOADED)
    {
        ui_add_for_rendering(Menu_wrecking_crew_info.doc_handle);
    }
    ui_render(Menu_wrecking_crew_info.control_id, false);
}

// 0x00911680 (:1588)
void ui_document_menu_wrecking_crew_init()
{
    state_info info;
    info.enter = ui_document_menu_wrecking_crew_enter;
    info.exit = ui_document_menu_wrecking_crew_exit;
    info.process = ui_document_menu_wrecking_crew_process;
    info.handshake = ui_document_menu_wrecking_crew_handshake;
    info.render = ui_document_menu_wrecking_crew_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_WRECKING_CREW_MAIN_MENU, info);
}

// 0x0089D1F0 (:1604)
// The menu through GS_WC_INIT (which pushes it, transparent and pausing what is beneath).
void ui_wrecking_crew_show()
{
    Gs_switch_next_state = GS_WRECKING_CREW_MAIN_MENU;
    Gs_switch_transparent = true;
    Gs_switch_pause_beneath = true;
    gameseq_push_state(GS_WC_INIT, true, true);
}
