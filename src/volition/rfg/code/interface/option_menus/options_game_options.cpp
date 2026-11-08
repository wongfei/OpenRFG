// Original: volition/rfg/code/interface/option_menus/options_game_options.cpp
//
// The game options (GS_MAINMENU_OPTIONS_GAME_OPTIONS, UI.md UI6) on rfg_ui_options_template: difficulty (in game only),
// disable cover (always hidden), tool tips (hidden for DLC without them), subtitles, aim assist, map rotation, camera
// shake, text and audio language (main menu only). Accept saves (difficulty set, a changed language switched behind a
// loading box), back asks before dropping changes, the third hint resets the defaults.

#include "volition/rfg/code/interface/option_menus/option_menu_system.h"

#include "library/modules/runtime/base/include/keen/base/language.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/game_options.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/option_menus/ingame_options.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/player_autoaim.h"
#include "volition/rfg/code/objects/humans/player_cover.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/input/mouse.h"

#include <string.h>

// The language options' "none" (the system language): ~LanguageId_En_En in rfg.exe.
static const keen::LanguageId kNoPreferredLanguage = (keen::LanguageId)-1;

// ---- file-static globals
// bool s_localization_is_loading (0x01663023) is used here too; placed in localization_shared.h.
// The header's option row object names (shared statics of the interface unity; this file's copies).
static const char* kOptionRowGroup = {"row_group"}; // 0x016630B4
static const char* kOptionHighlightArrows = {"row_highlight_arrows"}; // 0x016630B8
static const char* kOptionLeftArrow = {"left_arrow"}; // 0x016630BC
static const char* kOptionRowHighlightBG = {"row_highlight_bg"}; // 0x016630C4
// keen::LanguageId s_keen_system_language_id (0x01663134) is used here too; placed in localization_shared.cpp.
// keen::LanguageId s_supported_languages[12] (0x01663138) is used here too; placed in localization_shared.cpp.
// bool Display_character (0x0166316F) is used here too; placed in rfg_lua_actions.cpp.
extern bool Display_character; // 0x0166316F, rfg_lua_actions.cpp
// int Doc_handle_ioptions (0x01664104) is used here too; placed in ingame_options.cpp.
static const char* cDoc_name_goptions = {"rfg_ui_options_template"}; // 0x01664240
static int Doc_handle_goptions = -1; // 0x01664244
static int Control_id_goptions = -1; // 0x01664248
static int Input_id_goptions = -1; // 0x0166424C
static int Menu_handle_goptions = -1; // 0x01664250
static int Message_box_handle_goptions = -1; // 0x01664254
static difficulty_levels Difficulty_choice = DIFFICULTY_LEVEL_MEDIUM; // 0x01664258
static const char* cTool_tips_enabled[2] = {"MENU_ENABLED", "MENU_DISABLED"}; // 0x0166425C
static const char* cDifficulty_choices[4] = {"MENU_DIFFICULTY_CASUAL", "MENU_DIFFICULTY_NORMAL", "MENU_DIFFICULTY_HARD", "MENU_DIFFICULTY_INSANE"}; // 0x01664264
// int Utility_doc_handle (0x01664FF8) is used here too; placed in loading.cpp.
// farray<unsigned int,16> Controller_slots (0x01667378) is used here too; placed in loading.cpp.
// farray<input_request_data,16> Input_types (0x016673C8) is used here too; placed in loading.cpp.
// farray<ui_menu*,8> Menus (0x01667698) is used here too; placed in ui_menu.cpp.
// The language rows' tags and their pointers (UI0 missed these statics; addresses from the asm).
static const char* Language_choice_pointers[12]; // 0x02BF2228
static const char* Language_choice_pointers_audio[12]; // 0x02BF43C8
static char Language_choices_audio[12][64]; // 0x02C00160
static char Language_choices[12][64]; // 0x02C01F00
// unsigned char Tool_tips_times_displayed[128] (0x02BF4490) is used here too; placed in ui_document_hud_tool_tips.cpp.
// bool s_localization_is_audio_loading (0x02C03509) is used here too; placed in localization_shared.cpp.
// bool s_langauge_has_localized_voices[12] (0x02C0350C) is used here too; placed in localization_shared.cpp.
// bool Active (0x02C03754) is used here too; placed in ui_document_hud_tool_tips.cpp.
extern bool Active; // 0x02C03754, ui_document_hud_tool_tips.cpp (the in-game options are up)
static bool Assets_loaded_goptions; // 0x02C07FC1
static bool Change_made_goptions; // 0x02C07FC2
static bool Waiting_on_save_completion_goptions; // 0x02C07FC3
static bool Options_check_for_reload_loca; // 0x02C07FC4
static int Num_language_choices; // 0x02C07FC8
static int Num_language_choices_audio; // 0x02C07FCC
// msgbox_info_set Message_box_info_set (0x02CA0340) is used here too; placed in ui_message_box.cpp.


// ---- internal functions

static void game_options_menu_create();

// The language of an audio row: the index-th supported language with localized voices (none: the first supported
// language).
static keen::LanguageId game_options_audio_language(int index)
{
    int found = 0;
    for (int i = 0; i < 12; i++)
    {
        if (s_langauge_has_localized_voices[i])
        {
            if (found == index)
            {
                return s_supported_languages[i];
            }
            found++;
        }
    }
    return s_supported_languages[0];
}

// A row's selection (a character list only, within its range).
static void game_options_set_char_list(int index, int value)
{
    ui_menu* menu = ui_menu_get_via_handle(Menu_handle_goptions);
    if (menu == nullptr)
    {
        return;
    }
    ui_menu_item_base* item = menu->get_menu_item_via_index(index);
    if (item != nullptr && item->get_menu_item_type() == MI_CHAR_LIST)
    {
        static_cast<ui_menu_item_char_list*>(item)->set_selected_char(value);
    }
}

// 0x00892920 (:327)
static void option_change_occurred_goptions(int choice, void* new_value)
{
    const unsigned int value = (unsigned int)(uintptr_t)new_value;
    switch (choice)
    {
    case 0:
        switch (value)
        {
        case 0:
            Difficulty_choice = DIFFICULTY_LEVEL_EASY;
            break;
        case 1:
            Difficulty_choice = DIFFICULTY_LEVEL_MEDIUM;
            break;
        case 2:
            Difficulty_choice = DIFFICULTY_LEVEL_HARD;
            break;
        case 3:
            Difficulty_choice = DIFFICULTY_LEVEL_INSANE;
            break;
        default:
            return;
        }
        Change_made_goptions = true;
        return;
    case 1:
        Change_made_goptions = true;
        Player_disable_cover = value == 1;
        return;
    case 2:
        if (value == 0)
        {
            Tool_tips_enabled = true;
        }
        else if (value == 1)
        {
            Tool_tips_enabled = false;
        }
        else
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            return;
        }
        Change_made_goptions = true;
        return;
    case 3:
        Change_made_goptions = true;
        Subtitles_enabled = value == 0;
        return;
    case 4:
        Change_made_goptions = true;
        Auto_aim_enabled = value == 0;
        return;
    case 5:
        Change_made_goptions = true;
        Minimap_rot0 = value == 1;
        return;
    case 6:
        Change_made_goptions = true;
        Camera_shake_enabled = value == 0;
        return;
    case 7:
        // The system language is stored as "none".
        if (value < 12)
        {
            Preferred_text_language = s_supported_languages[value];
            if (Preferred_text_language != localization_query_system_language())
            {
                Change_made_goptions = true;
                return;
            }
        }
        Preferred_text_language = kNoPreferredLanguage;
        Change_made_goptions = true;
        return;
    case 8:
        if (value < localization_get_number_supported_audio_languages())
        {
            Preferred_audio_language = localization_get_supported_audio_language(value);
            if (Preferred_audio_language != localization_query_system_language())
            {
                Change_made_goptions = true;
                return;
            }
        }
        Preferred_audio_language = kNoPreferredLanguage;
        Change_made_goptions = true;
        return;
    default:
        return;
    }
}

// 0x008E78A0 (:491)
static bool game_options_menu_default_controls_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Message_box_handle_goptions != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        game_options_reset_defaults();
        Change_made_goptions = true;
    }
    else if ((int)choice < 1 || (int)choice > 2)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Message_box_handle_goptions = -1;
    return true;
}

// 0x00892AF0 (:517)
static bool game_options_menu_cancle_message_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Message_box_handle_goptions != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        game_options_load_from_cache();
        if (gameseq_get_state() == GS_MAINMENU_OPTIONS_GAME_OPTIONS)
        {
            gameseq_pop_state();
        }
    }
    else if ((int)choice < 1 || (int)choice > 2)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Message_box_handle_goptions = -1;
    return true;
}

// 0x009030E0 (:547)
// Accept: nothing changed and saved before: leave; else the difficulty set and the options saved (the language checked
// in process). Back: a box if changed. Misc: the defaults box (or a box saying they are already set).
static void game_options_selection_hint(hint_type hint)
{
    if (hint == HT_SELECTION)
    {
        if (!game_options_are_changed() && !Change_made_goptions && player_profile_last_save_successful(Player_controller_index != 0xFFFFFFFFu ? (int)Player_controller_index : 0))
        {
            if (gameseq_get_state() == GS_MAINMENU_OPTIONS_GAME_OPTIONS)
            {
                gameseq_pop_state();
            }
            return;
        }
        difficulty_set_level(Difficulty_choice);
        game_options_save();
        Waiting_on_save_completion_goptions = true;
        Options_check_for_reload_loca = true;
        return;
    }
    if (hint == HT_ABORT)
    {
        if (!game_options_are_changed() && difficulty_get_level() == Difficulty_choice)
        {
            if (gameseq_get_state() == GS_MAINMENU_OPTIONS_GAME_OPTIONS)
            {
                gameseq_pop_state();
            }
            return;
        }
        Message_box_handle_goptions = ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_WARNING", "MENU_LOSE_CHANGES_CONFIRM", game_options_menu_cancle_message_cb, false, false, nullptr, nullptr, false);
        return;
    }
    if (hint != HT_MISC)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
    if (!game_options_are_changes_different_from_defaults() && difficulty_get_level() == DIFFICULTY_LEVEL_MEDIUM)
    {
        ui_add_msgbox(MBT_OK, "MENU_WARNING", "MENU_DEFAULTS_NOT_CHANGED", nullptr, false, false, nullptr, nullptr, false);
        return;
    }
    Message_box_handle_goptions = ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_WARNING", "MENU_RESET_DEFAULT_CONFIRM", game_options_menu_default_controls_cb, false, false, nullptr, nullptr, false);
}

// A menu hint: the tag's localized text with its button markup, and the button's image.
static void game_options_add_hint(hint_type ht, control_button_action action, const char* tag)
{
    char text[128];
    char image[256];
    // Magic number: 0x80, the hint text buffer's size.
    controls_fill_options_button_hint(text, 0x80, localization_get_display_string_req(tag, nullptr));
    ui_menu* menu = ui_menu_get_via_handle(Menu_handle_goptions);
    if (menu != nullptr)
    {
        // Original: 0x40 passed as the image buffer's size.
        controls_get_button_image_name(image, 0x40, action, Player_controller_index);
        menu->add_hint(ht, action, text, image);
    }
}

// Magic numbers: the menu's rows.
enum
{
    GOPTIONS_ROW_DIFFICULTY = 0,
    GOPTIONS_ROW_DISABLE_COVER = 1,
    GOPTIONS_ROW_TOOL_TIPS = 2,
    GOPTIONS_ROW_TEXT_LANGUAGE = 7,
    GOPTIONS_ROW_AUDIO_LANGUAGE = 8,
};

// 0x009037B0 (:619)
// Insane only once the game is completed (or in DLC); disable cover always hidden; difficulty only in game, the
// languages only from the main menu; tool tips hidden where the DLC has none.
static void game_options_menu_create()
{
    Menu_handle_goptions = ui_menu_add(Doc_handle_goptions, "MENU_GAME_OPTIONS", nullptr, game_options_selection_hint, "OM", true, -1, -1, false);
    int num_difficulties = 3;
    if (dlc_get_current_id() > -1)
    {
        num_difficulties = 4;
    }
    else
    {
        player_profile_data* profile = player_profile_get("options_game_options", Player_controller_index != 0xFFFFFFFFu ? (int)Player_controller_index : 0);
        if (profile != nullptr && profile->game_completed)
        {
            num_difficulties = 4;
        }
    }
    Difficulty_choice = difficulty_get_level();
    ui_menu_add_char_list_item(Menu_handle_goptions, "MENU_OVERALL_DIFFICULTY", cDifficulty_choices, num_difficulties, option_change_occurred_goptions, 0, true);
    ui_menu_add_char_list_item(Menu_handle_goptions, "MENU_DISABLE_COVER", cTool_tips_enabled, 2, option_change_occurred_goptions, 0, true);
    ui_menu_add_char_list_item(Menu_handle_goptions, "MENU_TOOL_TIPS", cTool_tips_enabled, 2, option_change_occurred_goptions, 0, true);
    ui_menu_add_char_list_item(Menu_handle_goptions, "MENU_SUBTITLE", cTool_tips_enabled, 2, option_change_occurred_goptions, 0, true);
    ui_menu_add_char_list_item(Menu_handle_goptions, "MENU_AIM_ASSIST", cTool_tips_enabled, 2, option_change_occurred_goptions, 0, true);
    ui_menu_add_char_list_item(Menu_handle_goptions, "MENU_MAP_ROTATION", cTool_tips_enabled, 2, option_change_occurred_goptions, 0, true);
    ui_menu_add_char_list_item(Menu_handle_goptions, "MENU_CAMERA_SHAKE", cTool_tips_enabled, 2, option_change_occurred_goptions, 0, true);
    ui_menu_add_char_list_item(Menu_handle_goptions, "LANGUAGE_SETTINGS_TEXT_LANGUAGE", Language_choice_pointers, Num_language_choices, option_change_occurred_goptions, 0, true);
    ui_menu_add_char_list_item(Menu_handle_goptions, "LANGUAGE_SETTINGS_AUDIO_LANGUAGE", Language_choice_pointers_audio, Num_language_choices_audio, option_change_occurred_goptions, 0, true);
    game_options_set_menu_selections();
    ui_menu_show_menu_item(Menu_handle_goptions, GOPTIONS_ROW_DISABLE_COVER, false);
    ui_menu_enable_menu_item(Menu_handle_goptions, GOPTIONS_ROW_DISABLE_COVER, false);
    if (!Active)
    {
        ui_menu_show_menu_item(Menu_handle_goptions, GOPTIONS_ROW_DIFFICULTY, false);
        ui_menu_enable_menu_item(Menu_handle_goptions, GOPTIONS_ROW_DIFFICULTY, false);
    }
    else
    {
        ui_menu_enable_menu_item(Menu_handle_goptions, GOPTIONS_ROW_TEXT_LANGUAGE, false);
        ui_menu_enable_menu_item(Menu_handle_goptions, GOPTIONS_ROW_AUDIO_LANGUAGE, false);
    }
    if (dlc_get_tooltips_disabled())
    {
        ui_menu_show_menu_item(Menu_handle_goptions, GOPTIONS_ROW_TOOL_TIPS, false);
        ui_menu_enable_menu_item(Menu_handle_goptions, GOPTIONS_ROW_TOOL_TIPS, false);
    }
    game_options_add_hint(HT_SELECTION, CBA_MENU_SELECT, "MENU_ACCEPT_WITH_BUTTON");
    game_options_add_hint(HT_ABORT, CBA_MENU_ABORT, "MENU_CANCEL_WITH_BUTTON");
    game_options_add_hint(HT_MISC, CBA_MENU_Y, "MENU_DEFAULT_SETTINGS");
}

// 0x008D6F10 (:687)
static void game_options_menu_reload()
{
    ui_menu_destroy(Menu_handle_goptions);
    game_options_menu_create();
}

// A language row's tag: "LANGUAGE_SETTINGS_LANGUAGE_NAME_<ID>", '-' as '_'.
static void game_options_make_language_tag(char* tag, keen::LanguageId language)
{
    // Magic number: 0x40, the tag buffers' size.
    keen::formatString(tag, 0x40, "language_settings_language_name_%s", keen::getLanguageIdString(language));
    keen::replaceStringCharacters(tag, "-", '_');
    keen::changeStringToUppercase(tag);
}

// ---- external interface

// 0x008928C0 (:180)
// Cover on, tool tips on (or DLC), subtitles, aim assist on, map rotation on, camera shake on, the system languages.
bool game_options_are_changes_different_from_defaults()
{
    if (!Player_disable_cover && (Tool_tips_enabled || dlc_get_current_id() > -1) && Subtitles_enabled && Auto_aim_enabled && !Minimap_rot0 && Camera_shake_enabled && Preferred_text_language == kNoPreferredLanguage && Preferred_audio_language == kNoPreferredLanguage)
    {
        return false;
    }
    return true;
}

// 0x008C4410 (:204)
// The rows from the current options; a language row is the preferred language's (none: the system language's, else
// the first).
void game_options_set_menu_selections()
{
    if ((int)Difficulty_choice > -1)
    {
        game_options_set_char_list(0, Difficulty_choice);
    }
    game_options_set_char_list(1, Player_disable_cover ? 1 : 0);
    game_options_set_char_list(2, Tool_tips_enabled ? 0 : 1);
    game_options_set_char_list(3, Subtitles_enabled ? 0 : 1);
    game_options_set_char_list(4, Auto_aim_enabled ? 0 : 1);
    game_options_set_char_list(5, Minimap_rot0 ? 1 : 0);
    game_options_set_char_list(6, Camera_shake_enabled ? 0 : 1);
    int text_index = -1;
    int system_index = -1;
    for (int i = 0; i < 12; i++)
    {
        const keen::LanguageId language = s_supported_languages[i];
        if (Preferred_text_language == language)
        {
            text_index = i;
            break;
        }
        if (keen::findSupportedLanguage(s_keen_system_language_id, s_supported_languages, 12) == language)
        {
            system_index = i;
        }
    }
    if (text_index < 0)
    {
        text_index = system_index < 0 ? 0 : system_index;
    }
    game_options_set_char_list(GOPTIONS_ROW_TEXT_LANGUAGE, text_index);
    int audio_index = -1;
    system_index = -1;
    for (int i = 0; i < (int)localization_get_number_supported_audio_languages(); i++)
    {
        const keen::LanguageId language = game_options_audio_language(i);
        if (Preferred_audio_language == language)
        {
            audio_index = i;
            break;
        }
        if (keen::findSupportedLanguage(s_keen_system_language_id, s_supported_languages, 12) == language)
        {
            system_index = i;
        }
    }
    if (audio_index < 0)
    {
        audio_index = system_index < 0 ? 0 : system_index;
    }
    // A row whose language has no localized voices becomes the first.
    const keen::LanguageId audio_language = game_options_audio_language(audio_index);
    bool has_voices = false;
    for (int i = 0; i < 12; i++)
    {
        if (s_supported_languages[i] == audio_language)
        {
            has_voices = s_langauge_has_localized_voices[i];
            break;
        }
    }
    if (!has_voices)
    {
        audio_index = 0;
    }
    game_options_set_char_list(GOPTIONS_ROW_AUDIO_LANGUAGE, audio_index);
}

// 0x008E77C0 (:285)
// The defaults; the menu remade on the same row.
void game_options_reset_defaults()
{
    Player_disable_cover = false;
    Tool_tips_enabled = true;
    Preferred_text_language = kNoPreferredLanguage;
    Preferred_audio_language = kNoPreferredLanguage;
    Subtitles_enabled = true;
    Difficulty_choice = DIFFICULTY_LEVEL_MEDIUM;
    Auto_aim_enabled = true;
    Minimap_rot0 = false;
    Camera_shake_enabled = true;
    ui_menu* menu = ui_menu_get_via_handle(Menu_handle_goptions);
    const int selected = menu != nullptr ? menu->get_selected_item() : -1;
    game_options_set_menu_selections();
    game_options_menu_reload();
    ui_menu_set_selected_item(Menu_handle_goptions, selected);
}

// 0x008AFDB0 (:303)
bool game_options_reset_tips_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Message_box_handle_goptions != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        memset(Tool_tips_times_displayed, 0, sizeof(Tool_tips_times_displayed));
    }
    else if ((int)choice < 1 || (int)choice > 2)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Message_box_handle_goptions = -1;
    return true;
}

// 0x0090FA00 (:699)
// The language rows' tags, the document (none: back out), a control id and an input id; the menu under it hidden.
void game_options_menu_enter(game_state prev_state)
{
    Num_language_choices = 12;
    for (int i = 0; i < Num_language_choices; i++)
    {
        game_options_make_language_tag(Language_choices[i], s_supported_languages[i]);
        Language_choice_pointers[i] = Language_choices[i];
    }
    Num_language_choices_audio = (int)localization_get_number_supported_audio_languages();
    for (int i = 0; i < Num_language_choices_audio; i++)
    {
        game_options_make_language_tag(Language_choices_audio[i], game_options_audio_language(i));
        Language_choice_pointers_audio[i] = Language_choices_audio[i];
    }
    Assets_loaded_goptions = false;
    Change_made_goptions = false;
    Doc_handle_goptions = ui_load_document(cDoc_name_goptions, DFIT_NONE, 0.0f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (Doc_handle_goptions == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        gameseq_pop_state();
        return;
    }
    Control_id_goptions = (int)ui_request_control_id();
    Input_id_goptions = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    if (Active)
    {
        ingame_options_show_menu(false);
        return;
    }
    main_menu_show_menu(false);
}

// 0x008F40A0 (:749)
void game_options_menu_exit(game_state new_state)
{
    if (Message_box_handle_goptions != -1)
    {
        ui_hide_msgbox(Message_box_handle_goptions, true);
        Message_box_handle_goptions = -1;
    }
    ui_menu_destroy(Menu_handle_goptions);
    Menu_handle_goptions = -1;
    // Magic number: a 0.5 s fade out.
    ui_unload_document(Doc_handle_goptions, DFOT_NONE, 0.5f);
    Doc_handle_goptions = -1;
    ui_relinquish_input_id(Input_id_goptions);
    Input_id_goptions = -1;
    ui_relinquish_control_id((unsigned int)Control_id_goptions);
    Control_id_goptions = 0;
    Display_character = true;
    if (Active)
    {
        ingame_options_show_menu(true);
        return;
    }
    main_menu_show_menu(true);
}

// 0x00903DD0 (:780)
// The default hints, then the rows; a released click selects (on the arrows: left or right).
void game_options_menu_process_mouse_input()
{
    options_default_hint_objects hints;
    options_menu_get_default_hint_elements(Doc_handle_goptions, hints);
    const int hint = options_menu_process_mouse_on_default_hints(hints);
    if (hint > -1)
    {
        game_options_selection_hint((hint_type)hint);
        return;
    }
    const int index = ui_menu_process_mouseover(Doc_handle_goptions, Menu_handle_goptions, kOptionRowGroup);
    if (index < 0 || !mouse_just_released(0))
    {
        return;
    }
    vint_doc_info* info = ui_get_doc_info(Doc_handle_goptions);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    bool left_arrow = false;
    bool right_arrow = false;
    if (ui_vint_object_visible(kOptionHighlightArrows, info->m_doc))
    {
        left_arrow = ui_is_mouse_over_vint_object(kOptionLeftArrow, info->m_doc);
        if (!left_arrow)
        {
            // Original: right is anywhere else on the highlighted row.
            right_arrow = ui_is_mouse_over_vint_object(kOptionRowHighlightBG, info->m_doc);
        }
    }
    ui_menu_selection_clicked(Menu_handle_goptions, index, left_arrow, right_arrow);
}

// 0x00916200 (:812)
// A save in progress: a changed language switches behind a loading box ("LOADING" for text, "MENU_LOADING" for audio
// only); once no language is loading the box goes and the menu leaves. With a box up only the document runs; the menu
// once the document is loaded, then the mouse.
void game_options_menu_process()
{
    if (Waiting_on_save_completion_goptions)
    {
        bool leave = Multiplayer;
        if (!Multiplayer)
        {
            if (Options_check_for_reload_loca)
            {
                const bool text_changed = !(Preferred_text_language == Current_language || (Preferred_text_language == kNoPreferredLanguage && Current_language == localization_query_system_language()));
                const bool audio_changed = !(Preferred_audio_language == Current_audio_language || (Preferred_audio_language == kNoPreferredLanguage && Current_audio_language == localization_query_system_language()));
                if (text_changed || audio_changed)
                {
                    if (Message_box_handle_goptions != -1)
                    {
                        ui_hide_msgbox(Message_box_handle_goptions, true);
                        Message_box_handle_goptions = -1;
                    }
                    Message_box_handle_goptions = ui_add_msgbox(MBT_LOADING, text_changed ? "LOADING" : "MENU_LOADING", "", nullptr, false, false, nullptr, nullptr, false);
                    ui_menu_remove_all_hints(Menu_handle_goptions);
                    game_options_start_language_switch(Preferred_text_language, Preferred_audio_language);
                }
                Options_check_for_reload_loca = false;
            }
            if (!s_localization_is_loading)
            {
                if (!s_localization_is_audio_loading)
                {
                    if (Message_box_handle_goptions != -1)
                    {
                        ui_hide_msgbox(Message_box_handle_goptions, !player_profile_last_save_successful(Player_controller_index != 0xFFFFFFFFu ? (int)Player_controller_index : 0));
                        Message_box_handle_goptions = -1;
                    }
                    leave = true;
                }
                else
                {
                    ui_update_msgbox_caption(Message_box_handle_goptions, "MENU_LOADING", false);
                }
            }
        }
        if (leave)
        {
            Waiting_on_save_completion_goptions = false;
            if (gameseq_get_state() == GS_MAINMENU_OPTIONS_GAME_OPTIONS)
            {
                gameseq_pop_state();
            }
        }
    }
    if (Utility_doc_handle != -1 && ui_is_msgbox_active(-2))
    {
        if (Assets_loaded_goptions)
        {
            ui_add_for_processing(Doc_handle_goptions);
        }
        ui_process((unsigned int)Control_id_goptions, false);
        return;
    }
    if (!Assets_loaded_goptions)
    {
        switch (ui_get_doc_asset_state(Doc_handle_goptions))
        {
        case ST_LOADING:
            break;
        case ST_LOADED:
            Assets_loaded_goptions = true;
            break;
        case ST_UNLOADED:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            [[fallthrough]];
        case ST_ERROR:
            if (gameseq_get_state() == GS_MAINMENU_OPTIONS_GAME_OPTIONS)
            {
                gameseq_pop_state();
            }
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
        if (!Assets_loaded_goptions)
        {
            ui_process((unsigned int)Control_id_goptions, false);
            return;
        }
        game_options_menu_create();
    }
    if (Utility_doc_handle == -1 || !ui_is_msgbox_active(-2))
    {
        game_options_menu_process_mouse_input();
    }
    ui_add_for_processing(Doc_handle_goptions);
    ui_process((unsigned int)Control_id_goptions, false);
}

// 0x00892B60 (:930)
// The stream frame this state's user flag keeps out of the handshake (unless audio localization is loading).
void game_options_menu_handshake(rl_renderer*)
{
    if (!s_localization_is_audio_loading)
    {
        stream2_do_frame(true);
    }
}

// 0x00903EE0 (:938)
void game_options_menu_render(rl_renderer*)
{
    if (Assets_loaded_goptions)
    {
        ui_add_for_rendering(Doc_handle_goptions);
    }
    ui_render((unsigned int)Control_id_goptions, false);
}
