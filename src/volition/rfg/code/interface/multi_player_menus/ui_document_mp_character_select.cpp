// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_character_select.cpp
//
// Customization (GS_MULTIPLAYER_PLAYER_SELECT, UI.md UI5) on rfg_ui_mp_shell: the badge fields (background icon,
// foreground icon, colour scheme), the character of each team class (anarchy, EDF, guerrilla) and the sledgehammer.
// Left/right steps the selected row, accept stores the choices in the profile (and the party data) unless something
// chosen is still locked, back asks before dropping changes. The character and sledgehammer images stream in while the
// menu is up.

#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_character_select.h"

#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_string_processing.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_game/multi_game_badge.h"
#include "volition/rfg/code/multi/multi_game/multi_game_character.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/varray.h"

// ---- file-private types (rfg.pdb)
enum mp_menu_exit_actions_char_select
{
    MMEA_CS_POP = 0,
    MMEA_CS_PUSH = 1,
    NUM_MP_CS_MENU_EXIT_ACTIONS = 2,
};

enum mp_menu_states_char_select
{
    MMS_CS_LOADING = 0,
    MMS_CS_LOADED = 1,
    MMS_CS_DIALOG = 2,
    MMS_CS_UNLOADING = 3,
    MMS_CS_UNLOADED = 4,
    NUM_MP_CS_MENU_STATES = 5,
};

// size 0x74
struct mp_customization_shell_info
{

    mp_menu_states_char_select state; // 0x00
    mp_menu_states_char_select pending_state; // 0x04
    mp_menu_states_char_select prev_state; // 0x08
    mp_menu_exit_actions_char_select exit_action; // 0x0C
    game_state exit_push_target; // 0x10
    int doc_handle; // 0x14
    int data_group_handle; // 0x18
    farray<int,16> group_item_handles; // 0x1C
    unsigned int control_id; // 0x6C
    int input_id; // 0x70
};
static_assert(sizeof(mp_customization_shell_info) == 0x74, "mp_customization_shell_info");

enum mpps_hint_choices
{
    MPPS_HINT_INVALID = -1,
    MPPS_HINT_SELECT = 0,
    MPPS_HINT_ABORT = 1,
    NUM_MPPS_HINTS = 2,
};

// The menu rows (rfg.pdb enum).
enum mp_player_selections
{
    MPS_BADGE_BACKGROUND_ICON = 0,
    MPS_BADGE_FOREGROUND_ICON = 1,
    MPS_BADGE_COLOR_SCHEME = 2,
    MPS_CHARACTER_FFA = 3,
    MPS_CHARACTER_EDF = 4,
    MPS_CHARACTER_GUR = 5,
    MPS_SLEDGEHAMMER = 6,
    NUM_PLAYER_SELECT_OPTIONS = 7,
};

static void mpps_process_data_item_hint_text(vint_data_item* vdi);
static void mpps_process_data_item_titles(vint_data_item* vdi);
static void mpps_process_data_item_selection_info(vint_data_item* vdi);
static void mpps_process_data_item_menu_type(vint_data_item* vdi);
static void mpps_process_data_item_character_select(vint_data_item* vdi);

// ---- file-static globals
// bool s_localization_is_loading (0x01663023) is used here too; placed in localization_shared.h.
// const char* Mp_shell_doc_name (0x0166320C) is used here too; placed in multi_player_menu_system.cpp.
// int Mp_shell_doc_handle (0x0166325C) is used here too; placed in multi_player_menu_system.cpp.
// int Mp_msgbox_handle (0x016632E0) is used here too; placed in ui_document_hud_multiplayer.cpp.
static mp_shell_data_item_info Game_options_data_items[5] = {
    {MPSDIT_HINT_INFO, mpps_process_data_item_hint_text},
    {MPSDIT_TITLES, mpps_process_data_item_titles},
    {MPSDIT_SELECTION_INFO, mpps_process_data_item_selection_info},
    {MPSDIT_MENU_TYPE, mpps_process_data_item_menu_type},
    {MPSDIT_CHARACTER_SELECT, mpps_process_data_item_character_select},
}; // 0x01663354
static const char* Game_options_string = {"MENU_CUSTOMIZATION"}; // 0x0166337C
static const char* Character_select_desc = {""}; // 0x01663380
static const char* Accept_string = {"MENU_ACCEPT"}; // 0x01663384
static const char* Back_string = {"MENU_BACK"}; // 0x01663388
static const char* Player_selection_menu_options[7] = {"MENU_BADGE_BACKGROUND_ICON", "MENU_BADGE_FOREGROUND_ICON", "MENU_BADGE_COLOR_SCHEME", "MENU_ANARCHY_SELECT", "MENU_EDF_SELECT", "MENU_GUR_SELECT", "MENU_SLEDGEHAMMER"}; // 0x0166338C
static mpps_hint_choices Current_mouse_hint_choice_char_select = MPPS_HINT_INVALID; // 0x016633A8
// This file's copies of the header's arrow object names (the same strings as multi_player_menu_system.cpp's).
static const char* kLeftArrow = {"row_highlight_left"}; // 0x016685FC
static const char* kRightArrow = {"row_highlight_right"}; // 0x01668600
// int Utility_doc_handle (0x01664FF8) is used here too; placed in loading.cpp.
// bink_data Bink_info_main (0x01666148) is used here too; placed in main_menu.cpp.
static mp_customization_shell_info Mp_player_select_info; // 0x016663A0
// farray<unsigned int,16> Controller_slots (0x01667378) is used here too; placed in loading.cpp.
// farray<input_request_data,16> Input_types (0x016673C8) is used here too; placed in loading.cpp.
static int Player_save_sledge; // 0x02BE9E00
// multi_player_uid Playercard_player_uid (0x02BE9E14) is used here too; placed in multi_player_menu_system.cpp.
static unsigned int Player_save_badge; // 0x02BF2174
// int Audio_info[12] (0x02BF21D0) is used here too; placed in ui_menu.cpp.
static unsigned int Player_badge; // 0x02BF22EC
static int Player_sledge; // 0x02C00090
static unsigned int Player_save_select_bundle; // 0x02C0015C
// int mpps_hint_config[2] (0x02C00E00) is used here too; placed in multi_player_menu_system.cpp.
static unsigned int Player_select_bundle; // 0x02C02B4C
// mp_shell_data_item_info* Mp_shell_data_item_infos (0x02C0359C) is used here too; placed in multi_player_menu_system.cpp.
// int Num_mp_shell_data_item_infos (0x02C035A0) is used here too; placed in multi_player_menu_system.cpp.
static bool Exiting_char_select; // 0x02C035A6
static bool Right_arrow_clicked; // 0x02C035A7
// mp_message_box_type Mp_msgbox_type (0x02C035A8) is used here too; placed in multi_player_menu_system.cpp.
// vint_data_item* Hint_data_item (0x02C035B0) is used here too; placed in multi_player_menu_system.cpp.
// int Num_hints_added (0x02C035B4) is used here too; placed in multi_player_menu_system.cpp.
// The three character images (by mp_character_selection_types) and the sledgehammer's.
static const char* Image_names[4]; // 0x02C035C8
static int Player_select_selection; // 0x02C035D8
static int mpps_num_hints; // 0x02C035DC
static bool Left_arrow_clicked; // 0x02C035E0
// msgbox_info_set Message_box_info_set (0x02CA0340) is used here too; placed in ui_message_box.cpp.

// ---- internal functions

// 0x008E05D0 (:209)
// LTCG: raw_text in ESI, unicode_buf in EDI. A tag's localized text (or "!!" and the tag), coloured when asked.
static void get_colorized_localized_text(wchar_t* unicode_buf, int unicode_buf_size, const char* raw_text, bool colorize, const color& text_color)
{
    wchar_t hex[32];
    if (colorize)
    {
        char hex_text[32];
        str_color2s_hex(hex_text, sizeof(hex_text), text_color, false);
        copy_to_wide(hex, hex_text, 0x20);
    }
    const wchar_t* text = raw_text != nullptr ? localization_get_display_string_req(raw_text, nullptr) : nullptr;
    wchar_t missing[256];
    if (text == nullptr)
    {
        wchar_t raw[256];
        copy_to_wide(raw, raw_text, 0x100);
        _snprintf_safe(missing, 0x100, L"!!%ls", raw);
        text = missing;
    }
    if (!colorize)
    {
        _snprintf_safe(unicode_buf, (unsigned int)unicode_buf_size, L"%ls", text);
        return;
    }
    _snprintf_safe(unicode_buf, (unsigned int)unicode_buf_size, L"\r[color:#%ls]%ls[color:#FFFFFFFF]", hex, text);
}

// 0x0088A950 (:236)
// LTCG: unicode_buf in ESI, raw_text in EDI. Already display text: only coloured when asked.
static void get_colorized_localized_text(wchar_t* unicode_buf, int unicode_buf_size, const wchar_t* raw_text, bool colorize, const color& text_color)
{
    if (colorize)
    {
        char hex_text[32];
        wchar_t hex[32];
        str_color2s_hex(hex_text, sizeof(hex_text), text_color, false);
        copy_to_wide(hex, hex_text, 0x20);
        _snprintf_safe(unicode_buf, (unsigned int)unicode_buf_size, L"\r[color:#%ls]%ls[color:#FFFFFFFF]", hex, raw_text);
        return;
    }
    _snprintf_safe(unicode_buf, (unsigned int)unicode_buf_size, L"%ls", raw_text);
}

// 0x0088A9E0 (:285)
static bool mpps_confirm_abort(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (choice == MBC_OK)
    {
        Exiting_char_select = true;
    }
    return true;
}

static void mpps_load_menu_option_data_items();
static void mpps_unload_ui_information();

// 0x008F2060 (:294)
// Loaded: the data items and the menu rows' data group; unloading: the exit action.
static void mpps_do_state_change()
{
    do
    {
        Mp_player_select_info.state = Mp_player_select_info.pending_state;
        if (Mp_player_select_info.pending_state == MMS_CS_LOADED)
        {
            // mpps_load_ui_information, inlined.
            multi_player_menu_system_shell_load_data_items(Game_options_data_items, 5);
            vint_data_group* group = vint_data_group::create_new("MpShellMenuInfo");
            if (group != nullptr)
            {
                Mp_player_select_info.data_group_handle = group->get_handle();
                ui_add_data_group(Mp_player_select_info.doc_handle, Mp_player_select_info.data_group_handle);
                mpps_load_menu_option_data_items();
            }
        }
        else
        {
            if (Mp_player_select_info.pending_state != MMS_CS_UNLOADING)
            {
                return;
            }
            mpps_unload_ui_information();
            if (Mp_player_select_info.exit_action == MMEA_CS_POP)
            {
                gameseq_pop_state();
            }
            else if (Mp_player_select_info.exit_action == MMEA_CS_PUSH)
            {
                gameseq_push_state(Mp_player_select_info.exit_push_target, true, true);
            }
            Mp_player_select_info.pending_state = MMS_CS_UNLOADED;
        }
    } while (Mp_player_select_info.state != Mp_player_select_info.pending_state);
}

// 0x0088A9F0 (:343)
// LTCG: new_state in EAX.
static void mpps_set_state(mp_menu_states_char_select new_state)
{
    Mp_player_select_info.pending_state = new_state;
}

// 0x0088AA00 (:352)
static void mpps_init_state(game_state prev_state)
{
    Mp_player_select_info.state = MMS_CS_UNLOADED;
    Mp_player_select_info.pending_state = MMS_CS_UNLOADED;
    Mp_player_select_info.prev_state = MMS_CS_UNLOADED;
    Mp_player_select_info.exit_action = MMEA_CS_POP;
    Mp_player_select_info.exit_push_target = prev_state;
}

// 0x008ACB60 (:363)
static void mpps_init_info()
{
    Mp_player_select_info.group_item_handles.clear();
    Mp_player_select_info.data_group_handle = -1;
    Exiting_char_select = false;
}

// 0x008BFD40 (:372)
// Seven rows: {index, option, value}.
static void mpps_load_menu_option_data_items()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Mp_player_select_info.data_group_handle);
    for (int i = 0; i < NUM_PLAYER_SELECT_OPTIONS; i++)
    {
        vint_data_item* item = group->add_data_item(nullptr);
        if (item == nullptr)
        {
            continue;
        }
        vint_variant_type types[3] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 3);
        Mp_player_select_info.group_item_handles.add((int)item->get_handle());
    }
}

// 0x008CFEA0 (:401)
static void mpps_load_data_groups()
{
    vint_data_group* group = vint_data_group::create_new("MpShellMenuInfo");
    if (group != nullptr)
    {
        Mp_player_select_info.data_group_handle = group->get_handle();
        ui_add_data_group(Mp_player_select_info.doc_handle, Mp_player_select_info.data_group_handle);
        mpps_load_menu_option_data_items();
    }
}

// 0x0088AA30 (:420)
// LTCG: index in ECX.
static const char* get_character_raw_display_name(int index)
{
    return multi_game_player_get_display_name((mp_character_selection_types)index, Player_select_bundle);
}

// 0x0088AA40 (:427)
// LTCG: index in EAX. The characters' images (0-2), then the sledgehammer's.
static const char* get_image_name(int index)
{
    if ((unsigned int)index < NUM_MP_CHARACTER_SELECTION_TYPES)
    {
        return multi_game_player_get_bitmap_name((mp_character_selection_types)index, Player_select_bundle);
    }
    return multi_game_character_get_weapon_image(Player_sledge);
}

// 0x008ACB80 (:438)
// LTCG: index in EAX.
static bool render_image_is_loaded(int index)
{
    if (Image_names[index] == nullptr)
    {
        return false;
    }
    const unsigned int srid = stream2_container_find(Image_names[index]);
    return srid != 0 && stream2_get_status(srid) == ST_LOADED;
}

// 0x008ACBB0 (:443)
// LTCG: index in ESI. The image once streamed in, else none.
static const char* get_render_image_name(int index)
{
    return render_image_is_loaded(index) ? Image_names[index] : "";
}

// 0x0088AA70 (:452)
// LTCG: player_selection in EAX.
static bool item_is_locked(mp_player_selections player_selection)
{
    const int player_level = multi_game_xp_get_my_level();
    switch (player_selection)
    {
    case MPS_BADGE_BACKGROUND_ICON:
        return !multi_game_badge_field_is_valid(Player_badge, "Background Icon", player_level);
    case MPS_BADGE_FOREGROUND_ICON:
        return !multi_game_badge_field_is_valid(Player_badge, "Foreground Icon", player_level);
    case MPS_BADGE_COLOR_SCHEME:
        return !multi_game_badge_field_is_valid(Player_badge, "Color Scheme", player_level);
    case MPS_CHARACTER_FFA:
    case MPS_CHARACTER_EDF:
    case MPS_CHARACTER_GUR:
        return multi_game_player_is_locked((mp_character_selection_types)(player_selection - MPS_CHARACTER_FFA), Player_select_bundle);
    case MPS_SLEDGEHAMMER:
        return multi_game_xp_weapon_is_locked(Player_sledge);
    default:
        return false;
    }
}

// 0x0088AB30 (:477)
// The level is read before each check (as the original does; only the badge checks use it).
static bool any_selected_resource_locked()
{
    multi_game_xp_get_my_level();
    if (multi_game_player_is_locked(MP_SEL_FFA, Player_select_bundle))
    {
        return true;
    }
    multi_game_xp_get_my_level();
    if (multi_game_player_is_locked(MP_SEL_EDF, Player_select_bundle))
    {
        return true;
    }
    multi_game_xp_get_my_level();
    if (multi_game_player_is_locked(MP_SEL_GUR, Player_select_bundle))
    {
        return true;
    }
    multi_game_xp_get_my_level();
    if (multi_game_xp_weapon_is_locked(Player_sledge))
    {
        return true;
    }
    return !multi_game_badge_field_is_valid(Player_badge, "Background Icon", multi_game_xp_get_my_level()) ||
           !multi_game_badge_field_is_valid(Player_badge, "Foreground Icon", multi_game_xp_get_my_level()) ||
           !multi_game_badge_field_is_valid(Player_badge, "Color Scheme", multi_game_xp_get_my_level());
}

// 0x008ACBF0 (:488)
// A changed image: the old one released (if it was streamed), the new one requested.
static void update_image(int index)
{
    const char* name = get_image_name(index);
    if (name == nullptr || v_stricmp(name, Image_names[index]) == 0)
    {
        return;
    }
    if (Image_names[index] != nullptr)
    {
        const unsigned int srid = stream2_container_find(Image_names[index]);
        if (srid != 0 && stream2_get_status(srid) != ST_UNLOADED)
        {
            stream2_release(srid);
        }
    }
    Image_names[index] = name;
    const unsigned int srid = stream2_container_find(name);
    if (srid != 0)
    {
        stream2_request(srid);
    }
}

// 0x008CFED0 (:515)
static void mpps_load_ui_information()
{
    multi_player_menu_system_shell_load_data_items(Game_options_data_items, 5);
    mpps_load_data_groups();
}

// 0x008E0730 (:524)
static void mpps_unload_ui_information()
{
    multi_player_menu_system_shell_unload_data_items();
    mpps_init_info();
    multi_player_menu_system_close_message_box();
}

// 0x008E07C0 (:533)
static void mpps_process_data_item_hint_text(vint_data_item* vdi)
{
    multi_player_menu_system_begin_hints(vdi);
    mpps_hint_config[1] = -1;
    mpps_hint_config[0] = MPPS_HINT_SELECT;
    mpps_num_hints = 1;
    multi_player_menu_system_add_hint(localization_get_display_string_req(Accept_string, nullptr), CBA_MENU_SELECT);
    mpps_hint_config[mpps_num_hints] = MPPS_HINT_ABORT;
    mpps_num_hints++;
    multi_player_menu_system_add_hint(localization_get_display_string_req(Back_string, nullptr), CBA_MENU_ABORT);
    multi_player_menu_system_end_hints();
}

// 0x0088AC20 (:554)
static void mpps_process_data_item_titles(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(Game_options_string));
    vdi->set_element(1, vint_variant(Character_select_desc));
}

// 0x0088AC90 (:560)
static void mpps_process_data_item_selection_info(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(Player_select_selection));
    vdi->set_element(1, vint_variant((const char*)nullptr));
    vdi->set_element(2, vint_variant(true));
}

// 0x0088AD20 (:567)
static void mpps_process_data_item_menu_type(vint_data_item* vdi)
{
    // Magic number: menu type 2 (the customization layout of rfg_ui_mp_shell).
    vdi->set_element(0, vint_variant(2));
}

// 0x008E08A0 (:572)
// {true, then per character {image, name, locked text or none}, badge, badge locked, my name, then the sledgehammer's
// {image, name, locked text or none}}. Images only once streamed in (none while leaving).
static void mpps_process_data_item_character_select(vint_data_item* vdi)
{
    wchar_t text[128];
    char locked_text[384];
    char encoded[384];
    // Magic number: 0x80 wide characters, 0x180 encoded bytes (the buffers' sizes).
    get_colorized_localized_text(text, 0x80, "MENU_LOCKED", false, color::RED);
    vint_encode_wide_text(locked_text, 0x180, text);
    vdi->set_element(0, vint_variant(true));
    int index = 1;
    for (int i = MP_SEL_FFA; i < NUM_MP_CHARACTER_SELECTION_TYPES; i++, index += 3)
    {
        const bool locked = item_is_locked((mp_player_selections)(i + MPS_CHARACTER_FFA));
        get_colorized_localized_text(text, 0x80, multi_game_player_get_display_name((mp_character_selection_types)i, Player_select_bundle), false, color::RED);
        vint_encode_wide_text(encoded, 0x180, text);
        vdi->set_element(index, vint_variant(!Exiting_char_select ? get_render_image_name(i) : ""));
        vdi->set_element(index + 1, vint_variant((const char*)encoded));
        vdi->set_element(index + 2, vint_variant(locked ? (const char*)locked_text : (const char*)nullptr));
    }
    // The badge's locked check runs foreground, background, colour scheme (any_selected_resource_locked the other way).
    const bool badge_locked = !multi_game_badge_field_is_valid(Player_badge, "Foreground Icon", multi_game_xp_get_my_level()) ||
                              !multi_game_badge_field_is_valid(Player_badge, "Background Icon", multi_game_xp_get_my_level()) ||
                              !multi_game_badge_field_is_valid(Player_badge, "Color Scheme", multi_game_xp_get_my_level());
    vdi->set_element(index, vint_variant((int)Player_badge));
    vdi->set_element(index + 1, vint_variant(badge_locked));
    vint_encode_wide_text(encoded, 0x180, multi_game_data_get_my_name());
    vdi->set_element(index + 2, vint_variant((const char*)encoded));
    multi_game_xp_get_my_level();
    const bool sledge_locked = multi_game_xp_weapon_is_locked(Player_sledge);
    get_colorized_localized_text(text, 0x80, multi_game_character_get_weapon_display_name(Player_sledge), false, color::RED);
    vint_encode_wide_text(encoded, 0x180, text);
    vdi->set_element(index + 3, vint_variant(!Exiting_char_select ? get_render_image_name(3) : ""));
    vdi->set_element(index + 4, vint_variant((const char*)encoded));
    vdi->set_element(index + 5, vint_variant(sledge_locked ? (const char*)locked_text : (const char*)nullptr));
}

// 0x008E0CA0 (:619)
// Each row: {index + 1, option, value (red when locked)}.
static void mpps_process_data_group_menu_options()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Mp_player_select_info.data_group_handle);
    if (group == nullptr)
    {
        return;
    }
    const int count = (int)group->get_num_data_items();
    for (int i = 0; i < count; i++)
    {
        vint_data_item* item = vint_data_item::find((unsigned int)Mp_player_select_info.group_item_handles[i]);
        if (item == nullptr)
        {
            continue;
        }
        wchar_t text[128];
        char encoded[384];
        item->set_element(0, vint_variant(i + 1));
        get_colorized_localized_text(text, 0x80, Player_selection_menu_options[i], false, color::RED);
        vint_encode_wide_text(encoded, 0x180, text);
        item->set_element(1, vint_variant((const char*)encoded));
        const bool locked = item_is_locked((mp_player_selections)i);
        wchar_t value[128];
        switch (i)
        {
        case MPS_BADGE_BACKGROUND_ICON:
            multi_game_badge_get_field_value_name(value, 0x80, Player_badge, "Background Icon");
            get_colorized_localized_text(text, 0x80, value, locked, color::RED);
            break;
        case MPS_BADGE_FOREGROUND_ICON:
            multi_game_badge_get_field_value_name(value, 0x80, Player_badge, "Foreground Icon");
            get_colorized_localized_text(text, 0x80, value, locked, color::RED);
            break;
        case MPS_BADGE_COLOR_SCHEME:
            multi_game_badge_get_field_value_name(value, 0x80, Player_badge, "Color Scheme");
            get_colorized_localized_text(text, 0x80, value, locked, color::RED);
            break;
        case MPS_CHARACTER_FFA:
        case MPS_CHARACTER_EDF:
        case MPS_CHARACTER_GUR:
            get_colorized_localized_text(text, 0x80, get_character_raw_display_name(i - MPS_CHARACTER_FFA), locked, color::RED);
            break;
        case MPS_SLEDGEHAMMER:
        {
            // Original quirk: no display name leaves the option's own text in the value.
            const char* name = multi_game_character_get_weapon_display_name(Player_sledge);
            if (name != nullptr)
            {
                get_colorized_localized_text(text, 0x80, name, locked, color::RED);
            }
            break;
        }
        default:
            break;
        }
        vint_encode_wide_text(encoded, 0x180, text);
        item->set_element(2, vint_variant((const char*)encoded));
    }
}

// 0x008E0F30 (:691)
static void mpps_process_data_items()
{
    multi_player_menu_system_shell_process_data_items();
    mpps_process_data_group_menu_options();
}

// 0x008F2110 (:700)
// The document loaded: the menu loads; failed or missing: the menu goes.
static void mpps_process_loading()
{
    switch (ui_get_doc_asset_state(Mp_player_select_info.doc_handle))
    {
    case ST_LOADING:
        return;
    case ST_LOADED:
        mpps_set_state(MMS_CS_LOADED);
        return;
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        [[fallthrough]];
    case ST_ERROR:
        multi_player_menu_system_shell_unload_doc(Mp_player_select_info.doc_handle);
        mpps_set_state(MMS_CS_UNLOADING);
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
}

// 0x008CFF10 (:728)
// The hints (with no message box up), then the row arrows on a click, else the rows under the mouse.
static void mpps_input_process_mouse(bool& events_consumed)
{
    int hint = -1;
    Left_arrow_clicked = false;
    Right_arrow_clicked = false;
    Current_mouse_hint_choice_char_select = MPPS_HINT_INVALID;
    if (Utility_doc_handle == -1 || !ui_is_msgbox_active(-2))
    {
        if (ui_mouse_over_hint(kHintText2, Mp_player_select_info.doc_handle, true))
        {
            hint = 1;
        }
        if (ui_mouse_over_hint(kHintText1, Mp_player_select_info.doc_handle, true))
        {
            Current_mouse_hint_choice_char_select = (mpps_hint_choices)mpps_hint_config[0];
            return;
        }
        if (hint >= 0)
        {
            Current_mouse_hint_choice_char_select = (mpps_hint_choices)mpps_hint_config[hint];
            return;
        }
    }
    if (mouse_just_released(0))
    {
        vint_doc_info* info = ui_get_doc_info(Mp_player_select_info.doc_handle);
        if (info == nullptr || info->m_doc == nullptr)
        {
            return;
        }
        if (ui_is_mouse_over_vint_object(vint_object_base::find_object(kRightArrow, info->m_doc)))
        {
            Right_arrow_clicked = true;
            return;
        }
        if (ui_is_mouse_over_vint_object(vint_object_base::find_object(kLeftArrow, info->m_doc)))
        {
            Left_arrow_clicked = true;
        }
        return;
    }
    const int selection = multi_player_menu_system_handle_menu_mouse_input(Player_select_selection, Mp_player_select_info.group_item_handles.size(), Mp_player_select_info.input_id, Mp_player_select_info.doc_handle, events_consumed);
    if (selection >= 0 && selection != Player_select_selection)
    {
        multi_player_menu_system_set_menu_selection(selection, Player_select_selection, Mp_player_select_info.group_item_handles.size());
    }
}

// 0x008FB440 (:776)
// LTCG: reads ECX (only the stack slot of the flags). Left/right steps the selected row; back (changes made: asked
// first) leaves; accept (nothing locked) stores the profile choices and leaves.
static void mpps_process_loaded()
{
    if (Exiting_char_select)
    {
        Mp_player_select_info.exit_action = MMEA_CS_POP;
        Mp_player_select_info.pending_state = MMS_CS_UNLOADING;
        mpps_process_data_items();
        return;
    }
    bool events_consumed = false;
    mpps_input_process_mouse(events_consumed);
    multi_player_menu_system_handle_menu_key_input(Player_select_selection, Mp_player_select_info.group_item_handles.size(), Mp_player_select_info.input_id, events_consumed);
    if (events_consumed)
    {
        // Original: the data items are not processed on this frame.
        return;
    }
    const int input_id = Mp_player_select_info.input_id;
    // Magic numbers: buttons 10 select, 11 back, 3/7 left (pad/keys), 4/8 right.
    const float select_button = ui_get_button_info(10, input_id);
    const bool back_pressed = ui_get_button_info(11, input_id) > 0.0f;
    const bool left_pressed = ui_get_button_info(3, input_id) > 0.0f || ui_get_button_info(7, input_id) > 0.0f;
    const bool right_pressed = ui_get_button_info(4, input_id) > 0.0f || ui_get_button_info(8, input_id) > 0.0f;
    const bool select = select_button > 0.0f || Current_mouse_hint_choice_char_select == MPPS_HINT_SELECT;
    const bool abort = back_pressed || Current_mouse_hint_choice_char_select == MPPS_HINT_ABORT;
    const bool left = left_pressed || Left_arrow_clicked;
    const bool right = right_pressed || Right_arrow_clicked;
    // Magic numbers: Audio_info[7] previous, [8] next, [11] unavailable, [5] back.
    switch (Player_select_selection)
    {
    case MPS_BADGE_BACKGROUND_ICON:
    case MPS_BADGE_FOREGROUND_ICON:
    case MPS_BADGE_COLOR_SCHEME:
    {
        static const char* const field_names[3] = {"Background Icon", "Foreground Icon", "Color Scheme"};
        const char* field = field_names[Player_select_selection];
        if (left)
        {
            multi_game_badge_decrement_field(Player_badge, field);
            audiolib_cue_play(Audio_info[7], nullptr, nullptr);
        }
        else if (right)
        {
            multi_game_badge_increment_field(Player_badge, field);
            audiolib_cue_play(Audio_info[8], nullptr, nullptr);
        }
        break;
    }
    case MPS_CHARACTER_FFA:
    case MPS_CHARACTER_EDF:
    case MPS_CHARACTER_GUR:
    {
        const mp_character_selection_types type = (mp_character_selection_types)(Player_select_selection - MPS_CHARACTER_FFA);
        if (left)
        {
            multi_game_character_modify_player_selection(type, Player_select_bundle, false);
            update_image(Player_select_selection - MPS_CHARACTER_FFA);
            audiolib_cue_play(Audio_info[7], nullptr, nullptr);
        }
        else if (right)
        {
            multi_game_character_modify_player_selection(type, Player_select_bundle, true);
            update_image(Player_select_selection - MPS_CHARACTER_FFA);
            audiolib_cue_play(Audio_info[8], nullptr, nullptr);
        }
        break;
    }
    case MPS_SLEDGEHAMMER:
        if (left)
        {
            multi_game_xp_decrement_weapon_field(Player_sledge);
            update_image(Player_select_selection - MPS_CHARACTER_FFA);
            audiolib_cue_play(Audio_info[7], nullptr, nullptr);
        }
        else if (right)
        {
            multi_game_xp_increment_weapon_field(Player_sledge);
            update_image(Player_select_selection - MPS_CHARACTER_FFA);
            audiolib_cue_play(Audio_info[8], nullptr, nullptr);
        }
        break;
    default:
        if (left || right)
        {
            audiolib_cue_play(Audio_info[11], nullptr, nullptr);
        }
        break;
    }
    if (abort)
    {
        audiolib_cue_play(Audio_info[5], nullptr, nullptr);
        if (Player_select_bundle == Player_save_select_bundle && Player_badge == Player_save_badge && Player_save_sledge == Player_sledge)
        {
            Exiting_char_select = true;
        }
        else
        {
            ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_WARNING", "MENU_CHANGES_LOST", mpps_confirm_abort, false, false, nullptr, nullptr, false);
        }
    }
    if (select)
    {
        if (any_selected_resource_locked())
        {
            ui_add_msgbox(MBT_OK, "MENU_WARNING", "MENU_CAN_NOT_SAVE_LOCKED_CONTENT", nullptr, false, false, nullptr, nullptr, false);
            mpps_process_data_items();
            return;
        }
        const bool bundle_changed = Player_select_bundle != Player_save_select_bundle;
        if (bundle_changed)
        {
            multi_game_data_store_profile_player_selection(Player_select_bundle);
            Player_save_select_bundle = Player_select_bundle;
        }
        const bool badge_changed = Player_badge != Player_save_badge;
        if (badge_changed)
        {
            multi_game_save_badge_to_profile(Player_badge);
            Player_save_badge = Player_badge;
        }
        bool save = bundle_changed || badge_changed;
        if (Player_save_sledge != Player_sledge)
        {
            multi_game_data_store_profile_sledge_selection(Player_sledge);
            Player_save_sledge = Player_sledge;
            save = true;
        }
        if (save)
        {
            // No controller (-1): controller 0.
            player_profile_save("mpps_process_loaded()", Player_controller_index != 0xFFFFFFFFu ? (int)Player_controller_index : 0);
        }
        const multi_game_data* data = multi_game_data_get_my_data(ST_PARTY, false, false);
        if (data != nullptr)
        {
            multi_game_data_set_desired_hammer(data->handle, Player_sledge);
            multi_game_data_set_player_select_bundle(data->handle, Player_select_bundle);
            multi_game_data_set_badge(data->handle, Player_badge);
        }
        Exiting_char_select = true;
    }
    mpps_process_data_items();
}

// 0x0088AD60 (:1027)
static void mpps_ui_pop()
{
    Mp_player_select_info.exit_action = MMEA_CS_POP;
    Mp_player_select_info.pending_state = MMS_CS_UNLOADING;
}

// ---- external interface

// 0x0090B500 (:1039)
// The shell document, a control id and an input id; the profile's choices, their images streamed in.
void ui_document_mp_player_select_enter(game_state prev_state)
{
    mpps_init_info();
    mpps_init_state(prev_state);
    multi_player_menu_system_shell_load_doc(Mp_player_select_info.doc_handle, false);
    Mp_player_select_info.control_id = ui_request_control_id();
    Mp_player_select_info.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    Player_save_select_bundle = multi_game_character_get_profile_player_selection();
    Player_select_bundle = Player_save_select_bundle;
    multi_game_badge_get_my_badge(Player_badge);
    multi_game_badge_get_my_badge(Player_save_badge);
    Player_sledge = multi_game_xp_get_sledge_from_profile();
    // Magic number: 0xFF, no sledgehammer stored in the profile.
    if (Player_sledge == 0xFF)
    {
        Player_sledge = multi_game_xp_get_first_weapon_field();
    }
    Player_select_selection = 0;
    Player_save_sledge = Player_sledge;
    for (int i = 0; i < 4; i++)
    {
        const char* name = get_image_name(i);
        Image_names[i] = name;
        if (name != nullptr)
        {
            const unsigned int srid = stream2_container_find(name);
            if (srid != 0)
            {
                stream2_request(srid);
            }
        }
    }
}

// 0x008E0F40 (:1083)
// Note: the shell document stays loaded (the original does not unload it here).
void ui_document_mp_player_select_exit(game_state new_state)
{
    mpps_unload_ui_information();
    ui_relinquish_control_id(Mp_player_select_info.control_id);
    ui_relinquish_input_id(Mp_player_select_info.input_id);
    for (int i = 0; i < 4; i++)
    {
        if (Image_names[i] == nullptr)
        {
            continue;
        }
        const unsigned int srid = stream2_container_find(Image_names[i]);
        if (srid != 0 && stream2_get_status(srid) != ST_UNLOADED)
        {
            stream2_release(srid);
        }
        Image_names[i] = nullptr;
    }
}

// 0x009147F0 (:1104)
void ui_document_mp_player_select_process()
{
    if (Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
    ui_add_for_processing(Mp_player_select_info.doc_handle);
    switch (Mp_player_select_info.state)
    {
    case MMS_CS_LOADING:
        mpps_process_loading();
        break;
    case MMS_CS_LOADED:
        mpps_process_loaded();
        break;
    case MMS_CS_UNLOADED:
        mpps_set_state(MMS_CS_LOADING);
        break;
    default:
        break;
    }
    ui_process(Mp_player_select_info.control_id, false);
}

// 0x008F21C0 (:1133)
void ui_document_mp_player_select_handshake(rl_renderer*)
{
    Mp_player_select_info.prev_state = Mp_player_select_info.state;
    if (Mp_player_select_info.pending_state != Mp_player_select_info.state)
    {
        mpps_do_state_change();
    }
}

// 0x008FBA30 (:1141)
// Drawn once loaded for a whole frame.
void ui_document_mp_player_select_render(rl_renderer*)
{
    if (Mp_player_select_info.doc_handle == -1 || Mp_player_select_info.control_id == 0)
    {
        return;
    }
    if (Mp_player_select_info.state == MMS_CS_LOADED && Mp_player_select_info.prev_state == MMS_CS_LOADED)
    {
        ui_add_for_rendering(Mp_player_select_info.doc_handle);
    }
    ui_render(Mp_player_select_info.control_id, false);
}

// 0x00914870 (:1164)
void ui_document_mp_player_select_init()
{
    state_info info;
    info.enter = ui_document_mp_player_select_enter;
    info.exit = ui_document_mp_player_select_exit;
    info.process = ui_document_mp_player_select_process;
    info.handshake = ui_document_mp_player_select_handshake;
    info.render = ui_document_mp_player_select_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_MULTIPLAYER_PLAYER_SELECT, info);
}
