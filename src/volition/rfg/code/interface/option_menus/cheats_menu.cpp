// Original: volition/rfg/code/interface/option_menus/cheats_menu.cpp
//
// The cheats menu: eleven on/off rows, each unlocked by an achievement or by the profile.

#include "volition/rfg/code/interface/option_menus/option_menu_system.h"

#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/option_menus/ingame_options.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/vlib/code/input/mouse.h"

#include "keen/base/trace.hpp"

// ---- file-private types (rfg.pdb)
// size 0xC
struct cheat_achievement_mapping
{
    steam_achievement_ids ids[2]; // 0x00
    const char* menu_text; // 0x08
};
static_assert(sizeof(cheat_achievement_mapping) == 0xC, "cheat_achievement_mapping");

// Magic number: the number of cheats.
static const int Num_cheats = 11;

// ---- file-static globals
// Magic numbers: the achievements of the main game and of the DLC; -1 is none.
static const cheat_achievement_mapping Cheat_achievement_mappings[Num_cheats] = // 0x01300770
{
    {{(steam_achievement_ids)0x18, (steam_achievement_ids)0x3B}, "MENU_SUPER_HAMMER"},
    {{(steam_achievement_ids)0x19, (steam_achievement_ids)0x39}, "MENU_SUPER_DEBRIS"},
    {{(steam_achievement_ids)0x0D, (steam_achievement_ids)0x37}, "MENU_SUPER_TOUGHNESS"},
    {{(steam_achievement_ids)0x07, (steam_achievement_ids)0x36}, "MENU_SUPER_SPRINTING"},
    {{(steam_achievement_ids)0x16, (steam_achievement_ids)0x38}, "MENU_SUPER_JETPACK"},
    {{(steam_achievement_ids)0x1A, (steam_achievement_ids)0x3C}, "MENU_UNLIMITED_AMMO"},
    {{(steam_achievement_ids)0x17, (steam_achievement_ids)0x34}, "MENU_COOL_TURRETS"},
    {{(steam_achievement_ids)0x0B, (steam_achievement_ids)-1}, "MENU_FREE_UPGRADES"},
    {{(steam_achievement_ids)0x06, (steam_achievement_ids)-1}, "MENU_MAX_MORALE"},
    {{(steam_achievement_ids)0x08, (steam_achievement_ids)0x35}, "MENU_MAX_TECHNOLOGY"},
    {{(steam_achievement_ids)0x13, (steam_achievement_ids)0x3A}, "MENU_ALWAYS_YELLOW_ALERT"},
};
static const char* kOptionRowGroup = {"row_group"}; // 0x016630B4
static const char* kOptionHighlightArrows = {"row_highlight_arrows"}; // 0x016630B8
static const char* kOptionLeftArrow = {"left_arrow"}; // 0x016630BC
static const char* kOptionRowHighlightBG = {"row_highlight_bg"}; // 0x016630C4
static const char* cDoc_name = {"rfg_ui_options_template"}; // 0x01663D70
static int Doc_handle_cheats = -1; // 0x01663D74
static int Control_id_cheats = -1; // 0x01663D78
static int Input_id_cheats = -1; // 0x01663D7C
static int Menu_handle_cheats = -1; // 0x01663D80
static int Message_box_handle_cheats = -1; // 0x01663D84
static int Mb_choice = -1; // 0x01663D88
static const char* cCheats_choices[2] = {"MENU_CHEAT_OFF", "MENU_CHEAT_ON"}; // 0x01663D8C
static bool Assets_loaded_cheats; // 0x02C036DC
static bool Change_made; // 0x02C036DD
static bool Mb_value; // 0x02C036DE
static bool Unlock_all_cheats; // 0x02C036DF
static bool Cheats_choices[Num_cheats]; // 0x02C036E0
extern bool Active; // 0x02C03754, ui_document_hud_tool_tips.cpp's

static void option_change_occurred(int choice, void* new_value);

// 0x0088F520 (:122)
static bool cheats_menu_cancel_message_cb(int mbox_handle, int, msgbox_choice choice)
{
    if (Message_box_handle_cheats != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        if (gameseq_get_state() == GS_CHEATS)
        {
            gameseq_pop_state();
        }
    }
    else if ((int)choice < 1 || (int)choice > 2)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Message_box_handle_cheats = -1;
    return true;
}

// 0x008FCF00 (:173)
static void cheats_selection_hint(hint_type hint)
{
    if (hint == HT_SELECTION)
    {
        if (Change_made)
        {
            for (int i = 0; i < Num_cheats; i++)
            {
                stats_set_cheat((cheat_indexes)i, Cheats_choices[i]);
            }
        }
    }
    else if (hint == HT_ABORT)
    {
        if (Change_made)
        {
            Message_box_handle_cheats = ui_add_msgbox(MBT_ACCEPT_CANCEL, "MENU_WARNING", "MENU_LOSE_CHANGES_CONFIRM", cheats_menu_cancel_message_cb, false, false, nullptr, nullptr, false);
            return;
        }
    }
    else
    {
        if (hint != HT_MISC)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
        return;
    }
    if (gameseq_get_state() == GS_CHEATS)
    {
        gameseq_pop_state();
    }
}

// 0x0088F580 (:216)
// The asm passes the cheat in EAX. An achievement already awarded unlocks the cheat in the profile.
static bool cheats_menu_is_cheat_unlocked(cheat_indexes index, int dlc_id)
{
    player_profile_data* profile = player_profile_get("cheats", Player_controller_index != 0xFFFFFFFF ? Player_controller_index : 0);
    if (profile == nullptr)
    {
        return false;
    }
    const int set = dlc_id != -1 ? 1 : 0;
    if (!profile->new_data.cheats_unlocked[set][index])
    {
        const int achievement = (int)Cheat_achievement_mappings[index].ids[set];
        if (achievement == -1 || !stats_has_achievement_been_awarded(achievement))
        {
            return false;
        }
    }
    profile->new_data.cheats_unlocked[set][index] = true;
    return true;
}

// 0x0088F5F0 (:240)
void cheats_menu_unlock_cheat_for_achievement(steam_achievement_ids achievement_id)
{
    player_profile_data* profile = player_profile_get("cheats", Player_controller_index != 0xFFFFFFFF ? Player_controller_index : 0);
    if (profile == nullptr)
    {
        return;
    }
    for (int i = 0; i < Num_cheats; i++)
    {
        if (Cheat_achievement_mappings[i].ids[0] == achievement_id)
        {
            profile->new_data.cheats_unlocked[0][i] = true;
        }
        if (Cheat_achievement_mappings[i].ids[1] == achievement_id)
        {
            profile->new_data.cheats_unlocked[1][i] = true;
        }
    }
}

// 0x0090ED70 (:260)
static void cheats_menu_create()
{
    Menu_handle_cheats = ui_menu_add(Doc_handle_cheats, "MENU_CHEATS", nullptr, cheats_selection_hint, "OM", true, -1, -1, false);
    const int dlc_id = dlc_get_current_id();
    for (int i = 0; i < Num_cheats; i++)
    {
        const bool unlocked = Unlock_all_cheats || cheats_menu_is_cheat_unlocked((cheat_indexes)i, dlc_id);
        const char* caption = unlocked ? Cheat_achievement_mappings[i].menu_text : "???";
        ui_menu_add_char_list_item(Menu_handle_cheats, caption, cCheats_choices, 2, option_change_occurred, Cheats_choices[i] ? 1 : 0, true);
        ui_menu_enable_menu_item(Menu_handle_cheats, i, unlocked);
        ui_menu_show_menu_item(Menu_handle_cheats, i, unlocked);
    }
    char text[128];
    char image[256];
    controls_fill_options_button_hint(text, 0x80, localization_get_display_string_req("MENU_ACCEPT_WITH_BUTTON", nullptr));
    ui_menu* menu = ui_menu_get_via_handle(Menu_handle_cheats);
    if (menu != nullptr)
    {
        controls_get_button_image_name(image, 0x40, CBA_MENU_SELECT, Player_controller_index);
        menu->add_hint(HT_SELECTION, CBA_MENU_SELECT, text, image);
    }
    controls_fill_options_button_hint(text, 0x80, localization_get_display_string_req("MENU_CANCEL_WITH_BUTTON", nullptr));
    menu = ui_menu_get_via_handle(Menu_handle_cheats);
    if (menu != nullptr)
    {
        controls_get_button_image_name(image, 0x40, CBA_MENU_ABORT, Player_controller_index);
        menu->add_hint(HT_ABORT, CBA_MENU_ABORT, text, image);
    }
}

// 0x008C2DF0 (:288)
static bool cheats_warning_cb(int mbox_handle, int, msgbox_choice choice)
{
    if (mbox_handle != Message_box_handle_cheats)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        if ((unsigned int)Mb_choice < 12 && Mb_choice < Num_cheats)
        {
            Cheats_choices[Mb_choice] = Mb_value;
            Change_made = true;
        }
    }
    else if ((int)choice < 1 || (int)choice > 2)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    else if ((unsigned int)Mb_choice < 12) // Magic number: 12, as the asm tests, one past the last cheat.
    {
        ui_menu_set_char_list_index(Menu_handle_cheats, Mb_choice, 0);
    }
    Message_box_handle_cheats = -1;
    return true;
}

// 0x008FCF90 (:335)
// The first cheat turned on in a game warns first. rfg.exe inlines ui_add_msgbox here.
static void option_change_occurred(int choice, void* new_value)
{
    if (cheats_are_enabled() || Change_made)
    {
        if ((unsigned int)choice < 12 && choice < Num_cheats)
        {
            Change_made = true;
            Cheats_choices[choice] = new_value != nullptr;
        }
        return;
    }
    Mb_value = new_value != nullptr;
    Mb_choice = choice;
    Message_box_handle_cheats = ui_add_msgbox(MBT_ACCEPT_CANCEL_SELECT, "MENU_WARNING", "MENU_CHEATS_WARNING", cheats_warning_cb, false, false, nullptr, nullptr, false);
}

// 0x0090F140 (:374)
void cheats_menu_enter(game_state prev_state)
{
    (void)prev_state;
    Assets_loaded_cheats = false;
    Change_made = false;
    Doc_handle_cheats = ui_load_document(cDoc_name, DFIT_NONE, 0.0f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (Doc_handle_cheats == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        gameseq_pop_state();
        return;
    }
    Control_id_cheats = (int)ui_request_control_id();
    Input_id_cheats = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    for (int i = 0; i < Num_cheats; i++)
    {
        Cheats_choices[i] = stats_cheat_enabled((cheat_indexes)i);
    }
    if (!Active)
    {
        main_menu_show_menu(false);
        return;
    }
    vint_document* doc = ui_get_vint_document(Doc_handle_cheats);
    if (doc != nullptr)
    {
        vint_object_base* group = vint_object_base::find_object("screen_group", doc);
        if (group != nullptr)
        {
            group->set_property_value("visible", vint_variant(false), 0);
        }
    }
}

// 0x008F3B90 (:405)
void cheats_menu_exit(game_state new_state)
{
    (void)new_state;
    ui_menu_destroy(Menu_handle_cheats);
    Menu_handle_cheats = -1;
    ui_unload_document(Doc_handle_cheats, DFOT_NONE, 0.5f);
    Doc_handle_cheats = -1;
    ui_relinquish_input_id(Input_id_cheats);
    Input_id_cheats = -1;
    ui_relinquish_control_id((unsigned int)Control_id_cheats);
    Control_id_cheats = 0;
    if (Active)
    {
        ingame_options_show_menu(true);
    }
    else
    {
        main_menu_show_menu(true);
    }
}

// 0x008FD320 (:427)
static void cheats_menu_process_mouse_input()
{
    int top_id = Current_input_lock_id;
    if (top_id == -1)
    {
        top_id = Input_types.size() - 1 < 0 ? 1 : Input_types[Input_types.size() - 1].id;
    }
    if (top_id != Input_id_cheats)
    {
        return;
    }
    options_default_hint_objects hints;
    options_menu_get_default_hint_elements(Doc_handle_cheats, hints);
    const int hint = options_menu_process_mouse_on_default_hints(hints);
    if (hint >= 0)
    {
        cheats_selection_hint((hint_type)hint);
        return;
    }
    const int row = ui_menu_process_mouseover(Doc_handle_cheats, Menu_handle_cheats, kOptionRowGroup);
    if (row < 0 || !mouse_just_released(0))
    {
        return;
    }
    vint_document* doc = ui_get_vint_document(Doc_handle_cheats);
    if (doc == nullptr)
    {
        return;
    }
    bool left_arrow = false;
    bool right_arrow = false;
    if (ui_vint_object_visible(kOptionHighlightArrows, doc))
    {
        left_arrow = ui_is_mouse_over_vint_object(kOptionLeftArrow, doc);
        if (!left_arrow)
        {
            right_arrow = ui_is_mouse_over_vint_object(kOptionRowHighlightBG, doc);
        }
    }
    ui_menu_selection_clicked(Menu_handle_cheats, row, left_arrow, right_arrow);
}

// 0x00915A00 (:464)
void cheats_menu_process()
{
    if (!Assets_loaded_cheats)
    {
        switch (ui_get_doc_asset_state(Doc_handle_cheats))
        {
        case ST_UNLOADED:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            [[fallthrough]];
        case ST_ERROR:
            if (gameseq_get_state() == GS_CHEATS)
            {
                gameseq_pop_state();
            }
            break;
        case ST_LOADING:
            break;
        case ST_LOADED:
            Assets_loaded_cheats = true;
            break;
        default:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
        if (!Assets_loaded_cheats)
        {
            ui_process((unsigned int)Control_id_cheats, false);
            return;
        }
        cheats_menu_create();
    }
    if (Utility_doc_handle == -1 || !ui_is_msgbox_active(-2)) // Magic number: -2, any message box.
    {
        cheats_menu_process_mouse_input();
    }
    ui_add_for_processing(Doc_handle_cheats);
    ui_process((unsigned int)Control_id_cheats, false);
}

// 0x0088F650 (:503)
void cheats_menu_handshake(rl_renderer*)
{
}

// 0x008FD460 (:508)
void cheats_menu_render(rl_renderer*)
{
    if (Assets_loaded_cheats)
    {
        ui_add_for_rendering(Doc_handle_cheats);
    }
    ui_render((unsigned int)Control_id_cheats, false);
}

// 0x0088F660 (:525)
// True when any cheat is unlocked: the first one found ends the unlocking of the rest in the profile, as the asm's flag does.
bool cheats_menu_cheats_unlocked()
{
    bool unlocked = Unlock_all_cheats;
    const int dlc_id = dlc_get_current_id();
    for (int i = 0; i < Num_cheats; i++)
    {
        unlocked = unlocked || cheats_menu_is_cheat_unlocked((cheat_indexes)i, dlc_id);
    }
    return unlocked;
}
