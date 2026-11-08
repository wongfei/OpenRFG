// Original: volition/rfg/code/interface/option_menus/options_main.cpp
//
// The options menu (GS_MAINMENU_OPTIONS, UI.md UI6): controls, display, game options, audio, extras (from the main
// menu) or cheats (in game), back. It is a menu on the main menu's document, or on the in-game options' document when
// opened from the Esc menu. options_init registers every option menu state.

#include "volition/rfg/code/interface/option_menus/option_menu_system.h"

#include "keen/base/trace.hpp"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/option_menus/controller_options.h"
#include "volition/rfg/code/interface/option_menus/ingame_options.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/vlib/code/input/input_internal.h"

// ---- file-static globals
// bool s_localization_is_loading (0x01663023) is used here too; placed in localization_shared.h.
// int Doc_handle_main (0x016631AC) is used here too; placed in ui_menu.cpp.
extern int Doc_handle_main; // 0x016631AC, ui_menu.cpp
// int Control_id_main (0x016631B4) is used here too; placed in main_menu.cpp.
extern int Control_id_main; // 0x016631B4, main_menu.cpp
// int Doc_handle_ioptions (0x01664104) is used here too; placed in ingame_options.cpp.
// int Menu_handle_ioptions (0x01664108) is used here too; placed in ingame_options.cpp.
// int Control_id_ioptions (0x01664114) is used here too; placed in ingame_options.cpp.
static int Menu_handle_omain = -1; // 0x01664278
static int Input_id_omain = -1; // 0x0166427C
static const char* cMenu_group_name_omain = {"text_group"}; // 0x01664280
static const char* cMenu_item_name_omain = {"text_clone"}; // 0x01664284
// int Choice_last (0x01664B38) is used here too; placed in ui_message_box.cpp.
// int Utility_doc_handle (0x01664FF8) is used here too; placed in loading.cpp.
// farray<input_request_data,16> Input_types (0x016673C8) is used here too; placed in loading.cpp.
// farray<ui_menu*,8> Menus (0x01667698) is used here too; placed in ui_menu.cpp.
// ui_button_info Button_info (0x02BF1E30) is used here too; placed in ui_message_box.cpp.
// int mp_stats_hint_config[7] (0x02BF214C) is used here too; placed in ui_message_box.cpp.
// bool Active (0x02C03754) is used here too; placed in ui_document_hud_tool_tips.cpp.
extern bool Active; // 0x02C03754, ui_document_hud_tool_tips.cpp (the in-game options are up)
// The next menu animates in (UI0 missed this static; address from the asm). extras_menu.cpp shares it.
bool Should_animate_in; // 0x02C07FC5
static int Last_menu_entered_omain; // 0x02C07FD0
// msgbox_info_set Message_box_info_set (0x02CA0340) is used here too; placed in ui_message_box.cpp.

// ---- internal functions

static void options_main_destroy_menu();

// 0x008E7900 (:112)
// An option menu pushed (the menus go; the next one animates in), extras (main menu, this menu stays) or cheats (in
// game), back.
static void options_selection_callback(int which)
{
    ui_wait_for_button_ups();
    switch (which)
    {
    case 0:
        gameseq_push_state(GS_CONTROLLER_OPTIONS, true, false);
        options_main_destroy_menu();
        Should_animate_in = true;
        Last_menu_entered_omain = 0;
        break;
    case 1:
        gameseq_push_state(GS_MAINMENU_OPTIONS_DISPLAY, true, false);
        options_main_destroy_menu();
        Should_animate_in = true;
        Last_menu_entered_omain = 1;
        break;
    case 2:
        gameseq_push_state(GS_MAINMENU_OPTIONS_GAME_OPTIONS, true, false);
        options_main_destroy_menu();
        Should_animate_in = true;
        Last_menu_entered_omain = 2;
        break;
    case 3:
        gameseq_push_state(GS_MAINMENU_OPTIONS_AUDIO, true, false);
        options_main_destroy_menu();
        Should_animate_in = true;
        Last_menu_entered_omain = 3;
        break;
    case 4:
        if (!Active)
        {
            Should_animate_in = false;
            gameseq_push_state(GS_MAINMENU_OPTIONS_EXTRAS, true, false);
            break;
        }
        Should_animate_in = true;
        gameseq_push_state(GS_CHEATS, true, false);
        options_main_destroy_menu();
        Last_menu_entered_omain = 4;
        break;
    case 5:
        if (gameseq_get_state() == GS_MAINMENU_OPTIONS)
        {
            gameseq_pop_state();
        }
        break;
    default:
        break;
    }
}

// 0x00892B80 (:186)
static void options_selection_hint_callback(hint_type hint)
{
    if (hint == HT_ABORT && gameseq_get_state() == GS_MAINMENU_OPTIONS)
    {
        Last_menu_entered_omain = 0;
        gameseq_pop_state();
    }
}

// A hint text: the tag's localized text with its button markup, and the button's image.
static void options_main_add_hint(hint_type ht, control_button_action action, const char* tag)
{
    char text[128];
    char image[256];
    // Magic number: 0x80, the hint text buffer's size.
    controls_fill_options_button_hint(text, 0x80, localization_get_display_string_req(tag, nullptr));
    ui_menu* menu = ui_menu_get_via_handle(Menu_handle_omain);
    if (menu != nullptr)
    {
        // Original: 0x40 passed as the image buffer's size.
        controls_get_button_image_name(image, 0x40, action, Player_controller_index);
        menu->add_hint(ht, action, text, image);
    }
}

// 0x008E7A30 (:198)
// On the in-game options' document when they are up, else the main menu's; cheats in game (shown only once some are
// unlocked), extras otherwise (no tween out); the last entered item selected.
static void options_main_create_menu()
{
    const bool ingame = Active;
    Menu_handle_omain = ui_menu_add(ingame ? Doc_handle_ioptions : Doc_handle_main, "", nullptr, options_selection_hint_callback, nullptr, Should_animate_in, -1, Input_id_omain, false);
    ui_menu_add_function_item(Menu_handle_omain, "MENU_CONTROLS", options_selection_callback, true, false, true);
    ui_menu_add_function_item(Menu_handle_omain, "MENU_DISPLAY", options_selection_callback, true, false, true);
    ui_menu_add_function_item(Menu_handle_omain, "MENU_GAME_OPTIONS", options_selection_callback, true, false, true);
    ui_menu_add_function_item(Menu_handle_omain, "MENU_AUDIO", options_selection_callback, true, false, true);
    ui_menu_add_function_item(Menu_handle_omain, ingame ? "MENU_CHEATS" : "MENU_EXTRAS", options_selection_callback, ingame, false, true);
    ui_menu_add_function_item(Menu_handle_omain, "MENU_BACK", options_selection_callback, true, false, false);
    options_main_add_hint(HT_SELECTION, CBA_MENU_SELECT, "MENU_SELECT_WITH_BUTTON");
    options_main_add_hint(HT_ABORT, CBA_MENU_ABORT, "MENU_BACK_WITH_BUTTON");
    ui_menu_set_selected_item(Menu_handle_omain, Last_menu_entered_omain);
    if (ingame && !cheats_menu_cheats_unlocked())
    {
        // Magic number: item 4, cheats.
        ui_menu_enable_menu_item(Menu_handle_omain, 4, false);
        ui_menu_show_menu_item(Menu_handle_omain, 4, false);
    }
}

// 0x008D6F30 (:241)
// This menu, and the in-game options' menu under it.
static void options_main_destroy_menu()
{
    if (Active && Menu_handle_ioptions != -1)
    {
        ui_menu_destroy(Menu_handle_ioptions);
        Menu_handle_ioptions = -1;
    }
    if (Menu_handle_omain != -1)
    {
        ui_menu_destroy(Menu_handle_omain);
        Menu_handle_omain = -1;
    }
}

// The document the menu is on: the main menu's (unless idle or gone), else the in-game options'.
static bool options_menu_on_main_menu()
{
    return gameseq_get_state() != GS_MAINMENU_IDLE && Doc_handle_main != -1;
}

// 0x008E7EA0 (:254)
// The select and back hints, then the menu items; a released click selects.
static void options_menu_process_mouse_input()
{
    if (gameseq_get_state() != GS_MAINMENU_OPTIONS)
    {
        return;
    }
    // Original: the state was just checked to be GS_MAINMENU_OPTIONS, so this is always the main menu's document
    // (when it exists).
    const int doc_handle = options_menu_on_main_menu() ? Doc_handle_main : Doc_handle_ioptions;
    const char* names[2] = {"hints", "hints2"};
    const int hints[2] = {HT_SELECTION, HT_ABORT};
    vint_object_base* objects[2] = {nullptr, nullptr};
    ui_pc_collect_vint_obj_handles_by_name(names, 2, objects, doc_handle);
    const int clicked = ui_mouse_process_clickables(objects, 2, true);
    int index = ui_menu_process_mouseover_extend(doc_handle, Menu_handle_omain, cMenu_group_name_omain, cMenu_item_name_omain, false);
    if (index < 0)
    {
        if (clicked > -1)
        {
            if (hints[clicked] == HT_SELECTION)
            {
                index = ui_menu_get_selected_item(Menu_handle_omain);
            }
            else if (hints[clicked] == HT_ABORT)
            {
                ui_wait_for_button_ups();
                if (gameseq_get_state() == GS_MAINMENU_OPTIONS)
                {
                    gameseq_pop_state();
                }
            }
        }
        if (index < 0)
        {
            return;
        }
    }
    if (Mouse_buttons[0].just_released)
    {
        ui_menu_selection_clicked(Menu_handle_omain, index, false, false);
    }
}

// ---- external interface

// 0x009198A0 (:300)
// Every option menu state; the game options menu has user flag 1.
void options_init()
{
    state_info info;
    info.enter = options_menu_enter;
    info.exit = options_menu_exit;
    info.process = options_menu_process;
    info.handshake = options_menu_handshake;
    info.render = options_menu_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_MAINMENU_OPTIONS, info);
    info.enter = controller_options_enter;
    info.exit = controller_options_exit;
    info.process = controller_options_process;
    info.handshake = controller_options_handshake;
    info.render = controller_options_render;
    gameseq_register_state(GS_CONTROLLER_OPTIONS, info);
    info.enter = display_menu_enter;
    info.exit = display_menu_exit;
    info.process = display_menu_process;
    info.handshake = display_menu_handshake;
    info.render = display_menu_render;
    gameseq_register_state(GS_MAINMENU_OPTIONS_DISPLAY, info);
    info.enter = audio_menu_enter;
    info.exit = audio_menu_exit;
    info.process = audio_menu_process;
    info.handshake = audio_menu_handshake;
    info.render = audio_menu_render;
    gameseq_register_state(GS_MAINMENU_OPTIONS_AUDIO, info);
    info.enter = xbox_live_menu_enter;
    info.exit = xbox_live_menu_exit;
    info.process = xbox_live_menu_process;
    info.handshake = nullptr;
    info.render = xbox_live_menu_render;
    gameseq_register_state(GS_MAINMENU_OPTIONS_LIVE, info);
    info.enter = xbox_live_quickmatch_menu_enter;
    info.exit = xbox_live_quickmatch_menu_exit;
    info.process = xbox_live_quickmatch_menu_process;
    info.handshake = nullptr;
    info.render = xbox_live_quickmatch_menu_render;
    gameseq_register_state(GS_MAINMENU_OPTIONS_LIVE_QUICKMATCH, info);
    info.enter = cheats_menu_enter;
    info.exit = cheats_menu_exit;
    info.process = cheats_menu_process;
    info.handshake = cheats_menu_handshake;
    info.render = cheats_menu_render;
    gameseq_register_state(GS_CHEATS, info);
    info.enter = resolution_change_enter;
    info.exit = resolution_change_exit;
    info.process = resolution_change_process;
    info.handshake = resolution_change_handshake;
    info.render = resolution_change_render;
    gameseq_register_state(GS_RESOLUTION_CHANGE, info);
    info.enter = game_options_menu_enter;
    info.exit = game_options_menu_exit;
    info.process = game_options_menu_process;
    info.handshake = game_options_menu_handshake;
    info.render = game_options_menu_render;
    info.user_flags = 1;
    gameseq_register_state(GS_MAINMENU_OPTIONS_GAME_OPTIONS, info);
    extras_menu_init();
}

// 0x008E7FC0 (:375)
void options_menu_enter(game_state prev_state)
{
    Should_animate_in = false;
    Input_id_omain = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    options_main_create_menu();
}

// 0x008D6F80 (:383)
void options_menu_exit(game_state new_state)
{
    ui_menu_destroy(Menu_handle_omain);
    ui_relinquish_input_id(Input_id_omain);
    Menu_handle_omain = -1;
    Last_menu_entered_omain = 0;
}

// 0x00918BA0 (:394)
// Nothing but the document with a box up; the menus come back (the in-game one too) when this state is on top again,
// then the mouse.
void options_menu_process()
{
    const bool on_main_menu = options_menu_on_main_menu();
    const int doc_handle = on_main_menu ? Doc_handle_main : Doc_handle_ioptions;
    const unsigned int control_id = (unsigned int)(on_main_menu ? Control_id_main : Control_id_ioptions);
    if (Utility_doc_handle != -1 && ui_is_msgbox_active(-2))
    {
        ui_process(control_id, false);
        return;
    }
    if (Menu_handle_omain == -1 && gameseq_get_state() == GS_MAINMENU_OPTIONS && gameseq_get_pending_state() == GS_NONE)
    {
        if (Active)
        {
            ingame_options_create_menu(false);
        }
        options_main_create_menu();
    }
    if (gameseq_get_state() == GS_MAINMENU_OPTIONS && !ui_menu_get_tween_on_select_end(Menu_handle_omain) && !ui_menu_is_fading_in(Menu_handle_omain))
    {
        options_menu_process_mouse_input();
    }
    ui_add_for_processing(doc_handle);
    ui_process(control_id, false);
}

// 0x00892BB0 (:437)
void options_menu_handshake(rl_renderer*)
{
}

// 0x00903F10 (:441)
void options_menu_render(rl_renderer*)
{
    if (options_menu_on_main_menu())
    {
        ui_add_for_rendering(Doc_handle_main);
        ui_render((unsigned int)Control_id_main, false);
        return;
    }
    ui_add_for_rendering(Doc_handle_ioptions);
    ui_render((unsigned int)Control_id_ioptions, false);
}

// 0x00892BC0 (:458)
// Magic number: item 4, extras.
bool options_menu_is_extras_menu(int selection)
{
    return selection == 4;
}
