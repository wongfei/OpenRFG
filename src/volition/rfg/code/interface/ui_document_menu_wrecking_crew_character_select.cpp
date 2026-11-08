// Original: volition/rfg/code/interface/ui_document_menu_wrecking_crew_character_select.cpp
//
// Wrecking Crew's character select (GS_WRECKING_CREW_CHARACTER_SELECT, wrecking.md WC2) on rfg_ui_mp_shell: fifteen
// rows, four per player (the player's name row, then the character row; the other two are blank spacers), for the
// players of the match. Left/right steps the selected player's character; the character images stream in through a
// small queue of five names. Accept leaves when no two players share a character (else a conflict box); back leaves,
// asking first when a character changed (a cancel restores them); the third hint opens the controller options page
// for the selected player (controller_options.cpp reads Player_character_select_selection for it).

#include "volition/rfg/code/interface/ui_document_menu_wrecking_crew_character_select.h"

#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"

#include <cwchar>

class rl_renderer;

// ---- file-private types (rfg.pdb)
enum mp_menu_exit_actions_uiwccs
{
    MMEA_WCCS_POP = 0,
    MMEA_WCCS_PUSH = 1,
    NUM_MP_WCCS_MENU_EXIT_ACTIONS = 2,
};

enum mp_menu_states_uiwccs
{
    MMS_WCCS_LOADING = 0,
    MMS_WCCS_LOADED = 1,
    MMS_WCCS_DIALOG = 2,
    MMS_WCCS_UNLOADING = 3,
    MMS_WCCS_UNLOADED = 4,
    NUM_MP_WCCS_MENU_STATES = 5,
};

// size 0x74
struct mp_wreck_customization_shell_info
{
    mp_menu_states_uiwccs state; // 0x00
    mp_menu_states_uiwccs pending_state; // 0x04
    mp_menu_states_uiwccs prev_state; // 0x08
    mp_menu_exit_actions_uiwccs exit_action; // 0x0C
    game_state exit_push_target; // 0x10
    int doc_handle; // 0x14
    int data_group_handle; // 0x18
    farray<int,16> group_item_handles; // 0x1C
    unsigned int control_id; // 0x6C
    int input_id; // 0x70
};
static_assert(sizeof(mp_wreck_customization_shell_info) == 0x74, "mp_wreck_customization_shell_info");

enum wccs_hint_choices
{
    WCCS_HINT_INVALID = -1,
    WCCS_HINT_ABORT = 0,
    WCCS_HINT_ACCEPT = 1,
    WCCS_HINT_CONTROLS = 2,
    NUM_WCCS_HINTS = 3,
};

// One menu row: its text tag and whether the cursor may stop on it.
// size 0x8
struct menu_option_info_uiwccs
{
    const char* m_option; // 0x00
    bool selectability; // 0x04
};
static_assert(sizeof(menu_option_info_uiwccs) == 0x8, "menu_option_info_uiwccs");

static void wccs_process_data_item_hint_text(vint_data_item* vdi);
static void wccs_process_data_item_titles(vint_data_item* vdi);
static void wccs_process_data_item_selection_info(vint_data_item* vdi);
static void wccs_process_data_item_menu_type(vint_data_item* vdi);
static void wccs_process_data_item_character_display(vint_data_item* vdi);

// ---- globals
// bool s_localization_is_loading (0x01663023) is used here too; placed in localization_shared.h.
// const char* Mp_shell_doc_name (0x0166320C) is used here too; placed in multi_player_menu_system.cpp.
// int Mp_shell_doc_handle (0x0166325C) is used here too; placed in multi_player_menu_system.cpp.
// int Mp_msgbox_handle (0x016632E0) is used here too; placed in ui_document_hud_multiplayer.cpp.
static wccs_hint_choices Current_mouse_hint_choice_uiwccs = WCCS_HINT_INVALID; // 0x01664BC4
static mp_shell_data_item_info Game_options_data_items_uiwccs[5] = {
    {MPSDIT_HINT_INFO, wccs_process_data_item_hint_text},
    {MPSDIT_TITLES, wccs_process_data_item_titles},
    {MPSDIT_SELECTION_INFO, wccs_process_data_item_selection_info},
    {MPSDIT_MENU_TYPE, wccs_process_data_item_menu_type},
    {MPSDIT_WC_CHARACTER, wccs_process_data_item_character_display},
}; // 0x01664BC8
static const char* Game_options_string_uiwccs = {"MENU_CHARACTER_SELECT"}; // 0x01664BF0
static const char* Accept_string_uiwccs = {"MENU_ACCEPT"}; // 0x01664BF4
static const char* Back_string_uiwccs = {"MENU_BACK"}; // 0x01664BF8
// The selected row (a player's character row; the player is the row / 4). External: controller_options.cpp reads it
// to edit that player's controls.
int Player_character_select_selection = 1; // 0x01664BFC
// Four rows per player: name (the tags' "WRECKINC" typo is the original's), character, two blank spacers (the last
// player has one). The character rows' selectability is set on enter from the number of players.
static menu_option_info_uiwccs Wrecking_crew_character_select_options[15] = {
    {"MENU_WRECKINC_CREW_PLAYER_ONE", false},
    {"MENU_WRECKING_CREW_CHARACTER_SELECT_OPTION", true},
    {"", false},
    {"", false},
    {"MENU_WRECKINC_CREW_PLAYER_TWO", false},
    {"MENU_WRECKING_CREW_CHARACTER_SELECT_OPTION", true},
    {"", false},
    {"", false},
    {"MENU_WRECKINC_CREW_PLAYER_THREE", false},
    {"MENU_WRECKING_CREW_CHARACTER_SELECT_OPTION", true},
    {"", false},
    {"", false},
    {"MENU_WRECKINC_CREW_PLAYER_FOUR", false},
    {"MENU_WRECKING_CREW_CHARACTER_SELECT_OPTION", true},
    {"", false},
}; // 0x01664C00
// The streamed image waiting to be released once its replacement is requested (-1: none).
static int Target_image_to_stream_out = -1; // 0x01664C78
static int Message_box_handle_uiwccs = -1; // 0x01664C7C
// int Utility_doc_handle (0x01664FF8) is used here too; placed in loading.cpp.
// bink_data Bink_info_main (0x01666148) is used here too; placed in main_menu.cpp.
static mp_wreck_customization_shell_info Menu_wrecking_crew_character_select_info; // 0x01667510
// farray<unsigned int,16> Controller_slots (0x01667378) is used here too; placed in loading.cpp.
// farray<input_request_data,16> Input_types (0x016673C8) is used here too; placed in loading.cpp.
// int Audio_info[12] (0x02BF21D0) is used here too; placed in ui_menu.cpp.
static int wccs_hint_config[3]; // 0x02BF53E8
// Deviation: rfg.pdb gives the queue 4 names, but wccs_stream_in_image accepts a fifth (count < 5, so index 4 is
// written past the array in rfg.exe). Sized 5 here so that write stays inside the queue.
static const char* Streamed_image_name_uiwccs[5]; // 0x02C00E1C
// mp_shell_data_item_info* Mp_shell_data_item_infos (0x02C0359C) is used here too; placed in multi_player_menu_system.cpp.
// int Num_mp_shell_data_item_infos (0x02C035A0) is used here too; placed in multi_player_menu_system.cpp.
// mp_message_box_type Mp_msgbox_type (0x02C035A8) is used here too; placed in multi_player_menu_system.cpp.
// vint_data_item* Hint_data_item (0x02C035B0) is used here too; placed in multi_player_menu_system.cpp.
// int Num_hints_added (0x02C035B4) is used here too; placed in multi_player_menu_system.cpp.
static bool Delayed_streamed_out; // 0x02C080B2
static int wccs_num_hints; // 0x02C080C0
static mp_mouse_arrow_click_info Arrow_click_state_uiwccs; // 0x02C080C4
static int Streamed_image_count; // 0x02C080C8
static const char* Target_image_to_stream_in; // 0x02C080CC
// The players' infos as they were on enter (only character_index is copied, compared and restored).
wc_player_info Old_wc_player_infos[4]; // 0x02C3D120
// msgbox_info_set Message_box_info_set (0x02CA0340) is used here too; placed in ui_message_box.cpp.

// ---- image streaming

// 0x0089D220 (:242)
// An image not already queued is requested and queued (five at most). One queued but waiting to be released is kept.
void wccs_stream_in_image(const char* image_name)
{
    for (int i = 0; i < Streamed_image_count; i++)
    {
        if (v_stricmp(image_name, Streamed_image_name_uiwccs[i]) == 0)
        {
            if (i == Target_image_to_stream_out && Delayed_streamed_out)
            {
                keen::debug::nativePrintArgs("Really this shouldn't happen, but we'll handle it");
                Delayed_streamed_out = false;
            }
            return;
        }
    }
    // Magic number: 5 queued names at most (see Streamed_image_name_uiwccs).
    if (Streamed_image_count < 5)
    {
        if (image_name != nullptr)
        {
            const unsigned int srid = stream2_container_find(image_name);
            if (srid != 0)
            {
                stream2_request(srid);
            }
        }
        Streamed_image_name_uiwccs[Streamed_image_count] = image_name;
        Streamed_image_count++;
    }
}

// 0x0089D2B0 (:268)
// Queued (by pointer) and not waiting to be released.
bool wccs_is_a_streamed_image(const char* target_name)
{
    for (int i = 0; i < Streamed_image_count; i++)
    {
        if (target_name == Streamed_image_name_uiwccs[i])
        {
            return i != Target_image_to_stream_out || !Delayed_streamed_out;
        }
    }
    return false;
}

// 0x0089D2F0 (:291)
// Released and taken off the queue (the last name moves into its slot).
void wccs_stream_out_image(const char* image_name)
{
    for (int i = 0; i < Streamed_image_count; i++)
    {
        if (image_name == Streamed_image_name_uiwccs[i])
        {
            if (image_name != nullptr)
            {
                const unsigned int srid = stream2_container_find(image_name);
                if (srid != 0 && stream2_get_status(srid) != ST_UNLOADED)
                {
                    stream2_release(srid);
                }
            }
            Streamed_image_count--;
            Streamed_image_name_uiwccs[i] = Streamed_image_name_uiwccs[Streamed_image_count];
            return;
        }
    }
    keen::debug::nativePrintArgs("Why are we unloading an image that is not loaded?");
}

// 0x0089D370 (:311)
// The old image is released (and the new one requested) once every other queued image has loaded.
void wccs_mark_image_to_stream_out(const char* stream_out_image_name, const char* stream_in_image_name)
{
    Target_image_to_stream_in = stream_in_image_name;
    for (int i = 0; i < Streamed_image_count; i++)
    {
        if (stream_out_image_name == Streamed_image_name_uiwccs[i])
        {
            Target_image_to_stream_out = i;
            Delayed_streamed_out = true;
            return;
        }
    }
    keen::debug::nativePrintArgs("Why are we attempting to unload an image that is not loaded?");
}

// 0x0089D3C0 (:331)
// A queued image that failed (or has no container) is requested again and the frame ends there; with all of them
// fine, a pending swap runs.
void wccs_process_image_stream_system()
{
    for (int i = 0; i < Streamed_image_count; i++)
    {
        if (Target_image_to_stream_out == i)
        {
            continue;
        }
        const char* name = Streamed_image_name_uiwccs[i];
        const unsigned int srid = name != nullptr ? stream2_container_find(name) : 0;
        if (srid != 0 && stream2_get_status(srid) != ST_ERROR)
        {
            continue;
        }
        // Original: the container is looked up again (none: nothing requested).
        if (name != nullptr)
        {
            const unsigned int retry = stream2_container_find(name);
            if (retry != 0)
            {
                stream2_request(retry);
            }
        }
        return;
    }
    if (Delayed_streamed_out)
    {
        wccs_stream_out_image(Streamed_image_name_uiwccs[Target_image_to_stream_out]);
        Delayed_streamed_out = false;
        Target_image_to_stream_out = -1;
        wccs_stream_in_image(Target_image_to_stream_in);
    }
}

// 0x0089D470 (:355)
void wccs_drop_streamed_images()
{
    while (Streamed_image_count > 0)
    {
        wccs_stream_out_image(Streamed_image_name_uiwccs[0]);
    }
}

// ---- internal functions

// 0x008B62B0 (:363)
// OK: the characters chosen on enter come back and the menu leaves.
static bool wccs_cancel_message_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Message_box_handle_uiwccs != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        for (int i = 0; i < 4; i++)
        {
            Wc_player_infos[i].character_index = Old_wc_player_infos[i].character_index;
        }
        Menu_wrecking_crew_character_select_info.exit_action = MMEA_WCCS_POP;
        Menu_wrecking_crew_character_select_info.pending_state = MMS_WCCS_UNLOADING;
    }
    else if ((int)choice < 1 || (int)choice > 2)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Message_box_handle_uiwccs = -1;
    return true;
}

// 0x0089D500 (:400)
// No two players of the match share a character.
static bool wccs_are_all_character_choices_valid()
{
    for (int i = 0; i < Wrecking_crew_options.num_players; i++)
    {
        for (int j = i + 1; j < Wrecking_crew_options.num_players; j++)
        {
            if (Wc_player_infos[i].character_index == Wc_player_infos[j].character_index)
            {
                return false;
            }
        }
    }
    return true;
}

// 0x0089D550 (:413)
// LTCG: which_player in ESI. No other player of the match has this player's character.
static bool wccs_is_valid_character_choice(int which_player)
{
    for (int i = 0; i < Wrecking_crew_options.num_players; i++)
    {
        if (i != which_player && Wc_player_infos[which_player].character_index == Wc_player_infos[i].character_index)
        {
            return false;
        }
    }
    return true;
}

static void wccs_load_menu_option_data_items();
static void wccs_unload_ui_information();

// 0x008F68D0 (:424)
// Loaded: the data items and the rows' data group; unloading: the exit action.
static void wccs_do_state_change()
{
    do
    {
        Menu_wrecking_crew_character_select_info.state = Menu_wrecking_crew_character_select_info.pending_state;
        if (Menu_wrecking_crew_character_select_info.pending_state == MMS_WCCS_LOADED)
        {
            // wccs_load_ui_information, inlined.
            multi_player_menu_system_shell_load_data_items(Game_options_data_items_uiwccs, 5);
            vint_data_group* group = vint_data_group::create_new("MpShellMenuInfo");
            if (group != nullptr)
            {
                Menu_wrecking_crew_character_select_info.data_group_handle = group->get_handle();
                ui_add_data_group(Menu_wrecking_crew_character_select_info.doc_handle, Menu_wrecking_crew_character_select_info.data_group_handle);
                wccs_load_menu_option_data_items();
            }
        }
        else
        {
            if (Menu_wrecking_crew_character_select_info.pending_state != MMS_WCCS_UNLOADING)
            {
                return;
            }
            wccs_unload_ui_information();
            if (Menu_wrecking_crew_character_select_info.exit_action == MMEA_WCCS_POP)
            {
                gameseq_pop_state();
            }
            else if (Menu_wrecking_crew_character_select_info.exit_action == MMEA_WCCS_PUSH)
            {
                gameseq_push_state(Menu_wrecking_crew_character_select_info.exit_push_target, true, true);
            }
            Menu_wrecking_crew_character_select_info.pending_state = MMS_WCCS_UNLOADED;
        }
    } while (Menu_wrecking_crew_character_select_info.state != Menu_wrecking_crew_character_select_info.pending_state);
}

// 0x0089D590 (:473)
static void wccs_set_state(mp_menu_states_uiwccs new_state)
{
    Menu_wrecking_crew_character_select_info.pending_state = new_state;
}

// 0x0089D5A0 (:482)
static void wccs_init_state(game_state prev_state)
{
    Menu_wrecking_crew_character_select_info.state = MMS_WCCS_UNLOADED;
    Menu_wrecking_crew_character_select_info.pending_state = MMS_WCCS_UNLOADED;
    Menu_wrecking_crew_character_select_info.prev_state = MMS_WCCS_UNLOADED;
    Menu_wrecking_crew_character_select_info.exit_action = MMEA_WCCS_POP;
    Menu_wrecking_crew_character_select_info.exit_push_target = prev_state;
}

// 0x008B6340 (:493)
static void wccs_init_info()
{
    Menu_wrecking_crew_character_select_info.group_item_handles.clear();
    Menu_wrecking_crew_character_select_info.data_group_handle = -1;
}

// 0x008C8630 (:501)
// Fifteen rows: {index, option, value, unused flag, valid}.
static void wccs_load_menu_option_data_items()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Menu_wrecking_crew_character_select_info.data_group_handle);
    // Magic number: 15, the rows of Wrecking_crew_character_select_options.
    for (int i = 0; i < 15; i++)
    {
        vint_data_item* item = group->add_data_item(nullptr);
        if (item == nullptr)
        {
            continue;
        }
        vint_variant_type types[5] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL};
        item->set_template(types, 5);
        Menu_wrecking_crew_character_select_info.group_item_handles.add((int)item->get_handle());
    }
}

// 0x008C8700 (:532)
static void wccs_load_data_groups()
{
    vint_data_group* group = vint_data_group::create_new("MpShellMenuInfo");
    if (group != nullptr)
    {
        Menu_wrecking_crew_character_select_info.data_group_handle = group->get_handle();
        ui_add_data_group(Menu_wrecking_crew_character_select_info.doc_handle, Menu_wrecking_crew_character_select_info.data_group_handle);
        wccs_load_menu_option_data_items();
    }
}

// 0x008DB760 (:567)
static void wccs_load_ui_information()
{
    multi_player_menu_system_shell_load_data_items(Game_options_data_items_uiwccs, 5);
    wccs_load_data_groups();
}

// 0x008EE070 (:576)
static void wccs_unload_ui_information()
{
    multi_player_menu_system_shell_unload_data_items();
    wccs_init_info();
    multi_player_menu_system_close_message_box();
}

// 0x008EE100 (:585)
// Accept, back, and "<controller setup> (<the selected row's previous row>)": the name row above the selected
// character row, so the hint names the player whose controls it opens.
static void wccs_process_data_item_hint_text(vint_data_item* vdi)
{
    multi_player_menu_system_begin_hints(vdi);
    wccs_hint_config[1] = -1;
    wccs_hint_config[2] = -1;
    wccs_hint_config[0] = WCCS_HINT_ACCEPT;
    wccs_num_hints = 1;
    multi_player_menu_system_add_hint(localization_get_display_string_req(Accept_string_uiwccs, nullptr), CBA_MENU_SELECT);
    wccs_hint_config[wccs_num_hints] = WCCS_HINT_ABORT;
    wccs_num_hints++;
    multi_player_menu_system_add_hint(localization_get_display_string_req(Back_string_uiwccs, nullptr), CBA_MENU_ABORT);
    // Original: rfg.exe reads the row before the selection (0x01664BF8 + selection * 8); with the selection on row 0
    // that is Back_string_uiwccs. The cursor only stops on character rows (1, 5, 9, 13).
    const char* player_tag = Player_character_select_selection > 0 ? Wrecking_crew_character_select_options[Player_character_select_selection - 1].m_option : Back_string_uiwccs;
    const wchar_t* player_text = localization_get_display_string_req(player_tag, nullptr);
    const wchar_t* setup_text = localization_get_display_string_req("MENU_WRECKING_CREW_CONTROLLER_SETUP_OPTION", nullptr);
    wchar_t text[128];
    // Original: the unbounded wide sprintf; a missing tag passes a null string.
    sprintf(text, L"%ls (%ls)", setup_text, player_text);
    multi_player_menu_system_add_hint(text, CBA_MENU_X);
    wccs_hint_config[wccs_num_hints] = WCCS_HINT_CONTROLS;
    wccs_num_hints++;
    multi_player_menu_system_end_hints();
}

// 0x0089D5D0 (:612)
static void wccs_process_data_item_titles(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(Game_options_string_uiwccs));
    vdi->set_element(1, vint_variant(Wrecking_crew_character_select_options[Player_character_select_selection].m_option));
}

// 0x008B6360 (:619)
// {true, then each player's character image: once loaded and queued (not waiting to be released) for the players of
// the match, else none}.
static void wccs_process_data_item_character_display(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(true));
    for (int i = 0; i < 4; i++)
    {
        const char* bitmap_name = Wrecking_crew_characters[Wc_player_infos[i].character_index]->bitmap_name;
        stream2_status_val status = ST_ERROR;
        if (bitmap_name != nullptr)
        {
            const unsigned int srid = stream2_container_find(bitmap_name);
            if (srid != 0)
            {
                status = stream2_get_status(srid);
            }
        }
        const bool show = i < Wrecking_crew_options.num_players && status == ST_LOADED && wccs_is_a_streamed_image(bitmap_name);
        vdi->set_element(i + 1, vint_variant(show ? bitmap_name : ""));
    }
}

// 0x0089D640 (:634)
static void wccs_process_data_item_selection_info(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(Player_character_select_selection));
    vdi->set_element(1, vint_variant((const char*)nullptr));
    vdi->set_element(2, vint_variant(true));
}

// 0x0089D6D0 (:641)
static void wccs_process_data_item_menu_type(vint_data_item* vdi)
{
    // Magic number: menu type 2 (the customization layout of rfg_ui_mp_shell).
    vdi->set_element(0, vint_variant(2));
}

// 0x008EE2C0 (:646)
// Each row: {index + 1, option, value, false, valid}. The rows of the match's players have their option text; a
// character row's value is the character's name (or "!!" and the tag) and valid says no other player has it. The
// other players' rows are blank.
static void wccs_process_data_group_menu_options()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Menu_wrecking_crew_character_select_info.data_group_handle);
    if (group == nullptr)
    {
        return;
    }
    const int count = (int)group->get_num_data_items();
    for (int i = 0; i < count; i++)
    {
        vint_data_item* item = vint_data_item::find((unsigned int)Menu_wrecking_crew_character_select_info.group_item_handles[i]);
        if (item == nullptr)
        {
            continue;
        }
        // Magic number: four rows per player.
        const int player = (unsigned int)i >> 2;
        item->set_element(0, vint_variant(i + 1));
        item->set_element(3, vint_variant(false));
        if (player >= Wrecking_crew_options.num_players)
        {
            item->set_element(1, vint_variant(""));
            item->set_element(2, vint_variant(""));
            item->set_element(4, vint_variant(true));
            continue;
        }
        item->set_element(1, vint_variant(Wrecking_crew_character_select_options[i].m_option));
        wchar_t text[128];
        char encoded[384];
        switch (i)
        {
        case 1:
        case 5:
        case 9:
        case 13:
        {
            const char* display_name = Wrecking_crew_characters[Wc_player_infos[player].character_index]->display_name;
            if (localization_does_tag_exist(display_name))
            {
                // Magic number: 0x80 wide characters, the last one cleared.
                wcsncpy(text, localization_get_display_string_req(display_name, nullptr), 0x80);
                text[127] = L'\0';
            }
            else
            {
                _snprintf_safe(encoded, 0x80, "!!%s", display_name);
                copy_to_wide(text, encoded, 0x80);
            }
            item->set_element(4, vint_variant(wccs_is_valid_character_choice(player)));
            break;
        }
        default:
            copy_to_wide(text, "", 0x80);
            item->set_element(4, vint_variant(true));
            break;
        }
        // Magic number: 0x180 encoded bytes (the buffer's size).
        vint_encode_wide_text(encoded, 0x180, text);
        item->set_element(2, vint_variant((const char*)encoded));
    }
}

// 0x008EE5E0 (:727)
static void wccs_process_data_items()
{
    multi_player_menu_system_shell_process_data_items();
    wccs_process_data_group_menu_options();
}

// 0x008F6980 (:736)
// The document loaded: the menu loads; failed or missing: the menu goes.
static void wccs_process_loading()
{
    switch (ui_get_doc_asset_state(Menu_wrecking_crew_character_select_info.doc_handle))
    {
    case ST_LOADING:
        return;
    case ST_LOADED:
        wccs_set_state(MMS_WCCS_LOADED);
        return;
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        [[fallthrough]];
    case ST_ERROR:
        multi_player_menu_system_shell_unload_doc(Menu_wrecking_crew_character_select_info.doc_handle);
        wccs_set_state(MMS_WCCS_UNLOADING);
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
}

// 0x0089D710 (:761)
// LTCG: val in ECX, min in EDX, max in ESI. Below min wraps to max, above max to min. Inlined into its callers.
static void int_wrap_uiwccs(int& val, int min, int max)
{
    if (val < min)
    {
        val = max;
    }
    else if (val > max)
    {
        val = min;
    }
}

// 0x008B6480 (:770)
// LTCG: val in ESI, min in EDI. Left steps down, right up (each with its cue), then the value wraps.
static void int_process_uiwccs(int& val, int min, int max, bool left_action, bool right_action)
{
    // Magic numbers: Audio_info[7] previous, [8] next.
    if (left_action)
    {
        val--;
        audiolib_cue_play(Audio_info[7], nullptr, nullptr);
    }
    else if (right_action)
    {
        val++;
        audiolib_cue_play(Audio_info[8], nullptr, nullptr);
    }
    int_wrap_uiwccs(val, min, max);
}

// 0x0089D720 (:784)
// A player's character differs from the one on enter.
static bool wccs_has_options_changed()
{
    for (int i = 0; i < 4; i++)
    {
        if (Old_wc_player_infos[i].character_index != Wc_player_infos[i].character_index)
        {
            return true;
        }
    }
    return false;
}

// 0x008B64D0 (:800)
// LTCG: new_selection in EAX. A selectable row becomes the selection (wrapping), with the cue of its direction.
static void wccs_input_set_selection(int new_selection, bool& events_consumed)
{
    const int old_selection = Player_character_select_selection;
    const int count = Menu_wrecking_crew_character_select_info.group_item_handles.size();
    const int selection = new_selection % count;
    if (!Wrecking_crew_character_select_options[selection].selectability)
    {
        return;
    }
    Player_character_select_selection = selection;
    if (selection < 0)
    {
        Player_character_select_selection = count - 1;
    }
    if (selection > count - 1)
    {
        Player_character_select_selection = 0;
    }
    // Magic numbers: Audio_info[1] down, [0] up, [11] unavailable.
    if (old_selection < selection)
    {
        audiolib_cue_play(Audio_info[1], nullptr, nullptr);
    }
    else if (selection < old_selection)
    {
        audiolib_cue_play(Audio_info[0], nullptr, nullptr);
    }
    else
    {
        audiolib_cue_play(Audio_info[11], nullptr, nullptr);
    }
    events_consumed = true;
}

// 0x008DB7A0 (:833)
// The hints (with no message box up), then the row arrows, else the rows under the mouse.
static void wccs_input_process_mouse(bool& events_consumed)
{
    const int doc_handle = Menu_wrecking_crew_character_select_info.doc_handle;
    int hint = -1;
    if (Utility_doc_handle == -1 || !ui_is_msgbox_active(-2))
    {
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
            Current_mouse_hint_choice_uiwccs = (wccs_hint_choices)wccs_hint_config[0];
            return;
        }
        if (hint >= 0)
        {
            Current_mouse_hint_choice_uiwccs = (wccs_hint_choices)wccs_hint_config[hint];
            return;
        }
    }
    multi_player_menu_system_handle_arrow_clicks(Arrow_click_state_uiwccs, Menu_wrecking_crew_character_select_info.doc_handle);
    if (Arrow_click_state_uiwccs != MP_ARROW_CLICK_NONE)
    {
        return;
    }
    const int selection = multi_player_menu_system_handle_menu_mouse_input(Player_character_select_selection, Menu_wrecking_crew_character_select_info.group_item_handles.size(), Menu_wrecking_crew_character_select_info.input_id, Menu_wrecking_crew_character_select_info.doc_handle, events_consumed);
    if (selection >= 0 && selection != Player_character_select_selection)
    {
        wccs_input_set_selection(selection, events_consumed);
    }
}

// 0x008DB8A0 (:859)
// The mouse's hint and arrow state for this frame; true when it consumed the input.
static bool wccs_process_input()
{
    bool events_consumed = false;
    Arrow_click_state_uiwccs = MP_ARROW_CLICK_NONE;
    Current_mouse_hint_choice_uiwccs = WCCS_HINT_INVALID;
    wccs_input_process_mouse(events_consumed);
    return events_consumed;
}

// 0x0089D750 (:1050)
static void wccs_ui_push(game_state new_state)
{
    Menu_wrecking_crew_character_select_info.exit_action = MMEA_WCCS_PUSH;
    Menu_wrecking_crew_character_select_info.exit_push_target = new_state;
    Menu_wrecking_crew_character_select_info.pending_state = MMS_WCCS_UNLOADING;
}

// 0x0089D770 (:1057)
static void wccs_ui_pop()
{
    Menu_wrecking_crew_character_select_info.exit_action = MMEA_WCCS_POP;
    Menu_wrecking_crew_character_select_info.pending_state = MMS_WCCS_UNLOADING;
}

// 0x00906130 (:872)
// Up/down moves to the next selectable row; on a character row left/right steps the player's character (its image
// swapped through the queue) and the controls hint opens the controller options. Back leaves (asking when a
// character changed); a pending invite restores the characters and leaves; accept leaves unless two players share a
// character.
static void wccs_process_loaded()
{
    wccs_process_data_items();
    // Magic number: four rows per player. Original: taken before up/down moves the selection, so a left/right on the
    // same frame as a move steps the previous row's player.
    const int player = Player_character_select_selection / 4;
    const int input_id = Menu_wrecking_crew_character_select_info.input_id;
    // Magic numbers: buttons 3/7 left (pad/keys), 4/8 right, 1/5 up, 2/6 down, 11 back, 10 select, 14 controls (X).
    bool left = ui_get_button_info(3, input_id) > 0.0f || ui_get_button_info(7, input_id) > 0.0f;
    bool right = ui_get_button_info(4, input_id) > 0.0f || ui_get_button_info(8, input_id) > 0.0f;
    const bool up = ui_get_button_info(1, input_id) > 0.0f || ui_get_button_info(5, input_id) > 0.0f;
    const bool down = ui_get_button_info(2, input_id) > 0.0f || ui_get_button_info(6, input_id) > 0.0f;
    bool abort = ui_get_button_info(11, input_id) > 0.0f;
    bool accept = ui_get_button_info(10, input_id) > 0.0f;
    bool controls = ui_get_button_info(14, input_id) > 0.0f;
    if (wccs_process_input())
    {
        return;
    }
    abort = abort || Current_mouse_hint_choice_uiwccs == WCCS_HINT_ABORT;
    controls = controls || Current_mouse_hint_choice_uiwccs == WCCS_HINT_CONTROLS;
    accept = accept || Current_mouse_hint_choice_uiwccs == WCCS_HINT_ACCEPT;
    right = right || Arrow_click_state_uiwccs == MP_ARROW_CLICK_RIGHT;
    left = left || Arrow_click_state_uiwccs == MP_ARROW_CLICK_LEFT;
    const int count = Menu_wrecking_crew_character_select_info.group_item_handles.size();
    // Magic numbers: Audio_info[1] / [0] moved up / down, [11] nowhere to go.
    if (up)
    {
        const int old_selection = Player_character_select_selection;
        int selection = old_selection;
        do
        {
            selection += count - 1;
        } while (!Wrecking_crew_character_select_options[selection % count].selectability);
        Player_character_select_selection = selection;
        audiolib_cue_play(selection != old_selection ? Audio_info[1] : Audio_info[11], nullptr, nullptr);
    }
    else if (down)
    {
        const int old_selection = Player_character_select_selection;
        int selection = old_selection;
        do
        {
            selection++;
        } while (!Wrecking_crew_character_select_options[selection % count].selectability);
        Player_character_select_selection = selection;
        audiolib_cue_play(selection != old_selection ? Audio_info[0] : Audio_info[11], nullptr, nullptr);
    }
    Player_character_select_selection %= count;
    wccs_process_image_stream_system();
    switch (Player_character_select_selection)
    {
    case 0:
    case 2:
    case 3:
    case 4:
    case 6:
    case 7:
    case 8:
    case 10:
    case 11:
    case 12:
    case 14:
        break;
    case 1:
    case 5:
    case 9:
    case 13:
    {
        wc_player_info& info = Wc_player_infos[player];
        const char* old_image = Wrecking_crew_characters[info.character_index]->bitmap_name;
        int_process_uiwccs(info.character_index, 0, Wrecking_crew_characters.size() - 1, left, right);
        if (left || right)
        {
            const char* new_image = Wrecking_crew_characters[info.character_index]->bitmap_name;
            bool shared = false;
            // Original: all four players are checked, not only the match's.
            for (int i = 0; i < 4; i++)
            {
                if (i != player && old_image == Wrecking_crew_characters[Wc_player_infos[i].character_index]->bitmap_name)
                {
                    shared = true;
                    break;
                }
            }
            if (shared)
            {
                // Another player still shows the old image: the new one is only added.
                wccs_stream_in_image(new_image);
            }
            else
            {
                wccs_mark_image_to_stream_out(old_image, new_image);
            }
        }
        if (controls)
        {
            wccs_ui_push(GS_CONTROLLER_OPTIONS);
        }
        break;
    }
    default:
        // Not reachable: the selection is below the 15 rows.
        if (left || right)
        {
            audiolib_cue_play(Audio_info[11], nullptr, nullptr);
        }
        break;
    }
    if (abort)
    {
        if (wccs_has_options_changed())
        {
            Message_box_handle_uiwccs = ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_WARNING", "MENU_LOSE_CHANGES_CONFIRM", wccs_cancel_message_cb, false, false, nullptr, nullptr, false);
        }
        else
        {
            wccs_ui_pop();
        }
        // Magic number: Audio_info[5] back.
        audiolib_cue_play(Audio_info[5], nullptr, nullptr);
    }
    else if (multi_core_platform_has_pending_invite())
    {
        for (int i = 0; i < 4; i++)
        {
            Wc_player_infos[i].character_index = Old_wc_player_infos[i].character_index;
        }
        wccs_ui_pop();
    }
    if (!accept)
    {
        return;
    }
    if (wccs_are_all_character_choices_valid())
    {
        wccs_ui_pop();
        return;
    }
    ui_add_msgbox(MBT_OK, "MENU_WRECKING_CREW_CHARACTER_CONFLICT_TITLE", "MENU_WRECKING_CREW_CHARACTER_CONFLICT", nullptr, false, false, nullptr, nullptr, false);
}

// ---- external interface

// 0x009116D0 (:1070)
// The shell document, a control id and an input id; every player's character image queued (all four), the choices
// kept for a cancel, the character rows of the match's players made selectable, the first one selected.
static void ui_document_menu_wrecking_crew_character_select_enter(game_state prev_state)
{
    wccs_init_state(prev_state);
    wccs_init_info();
    multi_player_menu_system_shell_load_doc(Menu_wrecking_crew_character_select_info.doc_handle, false);
    Menu_wrecking_crew_character_select_info.control_id = ui_request_control_id();
    Menu_wrecking_crew_character_select_info.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    for (int i = 0; i < 4; i++)
    {
        wccs_stream_in_image(Wrecking_crew_characters[Wc_player_infos[i].character_index]->bitmap_name);
    }
    for (int i = 0; i < 4; i++)
    {
        Old_wc_player_infos[i].character_index = Wc_player_infos[i].character_index;
        // Magic number: row 1 + 4 * i, player i's character row.
        Wrecking_crew_character_select_options[1 + 4 * i].selectability = i < Wrecking_crew_options.num_players;
    }
    Player_character_select_selection = 1;
}

// 0x008B6560 (:1107)
// The images released, the ids returned; a changed character makes player 0 the current player again.
static void ui_document_menu_wrecking_crew_character_select_exit(game_state new_state)
{
    wccs_drop_streamed_images();
    ui_relinquish_control_id(Menu_wrecking_crew_character_select_info.control_id);
    ui_relinquish_input_id(Menu_wrecking_crew_character_select_info.input_id);
    if (wccs_has_options_changed())
    {
        Wrecking_crew_current_player = 0;
    }
}

// 0x00911850 (:1128)
static void ui_document_menu_wrecking_crew_character_select_process()
{
    if (Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
    ui_add_for_processing(Menu_wrecking_crew_character_select_info.doc_handle);
    switch (Menu_wrecking_crew_character_select_info.state)
    {
    case MMS_WCCS_LOADING:
        wccs_process_loading();
        break;
    case MMS_WCCS_LOADED:
        wccs_process_loaded();
        break;
    case MMS_WCCS_UNLOADED:
        wccs_set_state(MMS_WCCS_LOADING);
        break;
    default:
        break;
    }
    ui_process(Menu_wrecking_crew_character_select_info.control_id, false);
}

// 0x008F6A30 (:1157)
static void ui_document_menu_wrecking_crew_character_select_handshake(rl_renderer*)
{
    Menu_wrecking_crew_character_select_info.prev_state = Menu_wrecking_crew_character_select_info.state;
    if (Menu_wrecking_crew_character_select_info.pending_state != Menu_wrecking_crew_character_select_info.state)
    {
        wccs_do_state_change();
    }
}

// 0x008F6A50 (:1166)
// Drawn once loaded for a whole frame.
static void ui_document_menu_wrecking_crew_character_select_render(rl_renderer*)
{
    if (Menu_wrecking_crew_character_select_info.doc_handle == -1 || Menu_wrecking_crew_character_select_info.control_id == 0)
    {
        return;
    }
    if (Menu_wrecking_crew_character_select_info.state == MMS_WCCS_LOADED && Menu_wrecking_crew_character_select_info.prev_state == MMS_WCCS_LOADED)
    {
        ui_add_for_rendering(Menu_wrecking_crew_character_select_info.doc_handle);
    }
    ui_render(Menu_wrecking_crew_character_select_info.control_id, false);
}

// 0x009118D0 (:1190)
void ui_document_menu_wrecking_crew_character_select_init()
{
    state_info info;
    info.enter = ui_document_menu_wrecking_crew_character_select_enter;
    info.exit = ui_document_menu_wrecking_crew_character_select_exit;
    info.process = ui_document_menu_wrecking_crew_character_select_process;
    info.handshake = ui_document_menu_wrecking_crew_character_select_handshake;
    info.render = ui_document_menu_wrecking_crew_character_select_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_WRECKING_CREW_CHARACTER_SELECT, info);
}

// 0x0089D790 (:1208)
// Empty in rfg.exe and never called (no xrefs).
void ui_document_menu_wrecking_crew_character_controller_options_cancel()
{
}

// 0x0089D7A0 (:1212)
// Empty in rfg.exe and never called (no xrefs).
void ui_document_menu_wrecking_crew_character_controller_options_save()
{
}

// 0x0089D7B0 (:1216)
// The selected row's player (-1 past the fourth). Never called in rfg.exe: controller_options.cpp inlines it.
int ui_document_menu_wrecking_crew_character_get_player_index()
{
    const unsigned int player = (unsigned int)(Player_character_select_selection / 4);
    return player > 3 ? -1 : (int)player;
}
