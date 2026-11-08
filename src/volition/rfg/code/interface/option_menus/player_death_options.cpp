// Original: volition/rfg/code/interface/option_menus/player_death_options.cpp
//
// The death menu (GS_PLAYER_DEATH_OPTIONS, UI.md UI6), single player: continue (reload the level, the last save in
// memory, or the tutorial again), load game, quit (with its confirmation box). On rfg_ui_ingame_options, tinted red;
// the game is paused and blurred under it (the music keeps playing).

#include "volition/rfg/code/interface/option_menus/player_death_options.h"

#include "keen/base/trace.hpp"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_save.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/gamewide/gamestate_gameplay.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/vlib/code/cfile/vdir.h"
#include "volition/vlib/code/input/input_internal.h"

// ---- file-static globals
// bool s_localization_is_loading (0x01663023) is used here too; placed in localization_shared.h.
static int Doc_handle_dthoptions = -1; // 0x01664288
static int Menu_handle_dthoptions = -1; // 0x0166428C
static int Control_id_dthoptions = -1; // 0x01664290
static int Input_id_dthoptions = -1; // 0x01664294
// This file's copy of the header's menu group name.
static const char* kTextGroup = {"text_group"}; // 0x016686F0
// int Utility_doc_handle (0x01664FF8) is used here too; placed in loading.cpp.
// farray<unsigned int,16> Controller_slots (0x01667378) is used here too; placed in loading.cpp.
// farray<input_request_data,16> Input_types (0x016673C8) is used here too; placed in loading.cpp.
// farray<ui_menu*,8> Menus (0x01667698) is used here too; placed in ui_menu.cpp.
// screen_blur Screen_blur_info (0x02BE9E04) is used here too; placed in ui_document_manager.cpp.
static bool Assets_loaded_dthoptions; // 0x02C07FC6
static bool Render_ok; // 0x02C07FC7
// save_load_ui_type Save_load_ui_type (0x02C0812C) is used here too; placed in main_menu.cpp.
// msgbox_info_set Message_box_info_set (0x02CA0340) is used here too; placed in ui_message_box.cpp.

// ---- internal functions

// 0x00892FF0 (:101)
// Single player: the level reloaded (the debug setting), else the save in memory, or the tutorial territory again.
static void player_death_options_continue_cb(int delta)
{
    if (Multiplayer)
    {
        return;
    }
    if (Reload_level_on_death)
    {
        vdir_restore(PLATFORM_PC, true);
        World.set_pending_reload();
        return;
    }
    if (mission_get_mode() != MISSION_MODE_TUTORIAL)
    {
        World.set_pending_load_from_memory();
        return;
    }
    World.set_pending_territory_load("", false, false, false, World.dlc_bundle_id, false);
}

// 0x008D7070 (:117)
static void player_death_destroy_menu()
{
    if (Menu_handle_dthoptions != -1)
    {
        ui_menu_destroy(Menu_handle_dthoptions);
        Menu_handle_dthoptions = -1;
    }
}

// 0x008E8050 (:125)
static void player_death_options_load_cb(int delta)
{
    if (game_save_is_saving_enbaled())
    {
        player_death_options_show_menu(false);
        Save_load_ui_type = SLUT_LOAD;
        gameseq_push_state(GS_SAVE_LOAD_SCREEN, true, false);
        player_death_destroy_menu();
    }
}

// 0x00893050 (:136)
// Accepted: the frontend music and the main menu (through the DLC switch in single player DLC).
static bool player_death_confirm_main_menu(int _handle, int _selection, msgbox_choice _choice)
{
    if (_choice == MBC_OK)
    {
        gamemusic_frontend_start(false);
        if (World.dlc_bundle_id != -1 && !Multiplayer && !wrecking_crew_is_active())
        {
            game_set_dlc_shutdown(GS_MAINMENU);
            return true;
        }
        gameseq_set_state(GS_MAINMENU, false);
    }
    return true;
}

// 0x00903F60 (:152)
static void player_death_option_mainmenu_cb(int delta)
{
    ui_add_msgbox(MBT_ACCEPT_CANCEL_SELECT, "MENU_WARNING", "MENU_QUIT_CONFIRM", player_death_confirm_main_menu, false, false, nullptr, nullptr, false);
}

// 0x00904290 (:163)
static void player_death_options_hint_callback(hint_type hint)
{
}

// ---- external interface

// 0x00918C90 (:196)
void player_death_options_init()
{
    state_info info;
    info.enter = player_death_options_enter;
    info.exit = player_death_options_exit;
    info.process = player_death_options_process;
    info.handshake = player_death_options_handshake;
    info.render = player_death_options_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_PLAYER_DEATH_OPTIONS, info);
}

// 0x0090FCC0 (:211)
// The document fades in (none: back out at once), single player paused and blurred (the music on), the controls
// locked, a control id and an input id, the HUD hidden.
void player_death_options_enter(game_state prev_state)
{
    Assets_loaded_dthoptions = false;
    Render_ok = false;
    // Magic number: a 0.5 s alpha fade in.
    Doc_handle_dthoptions = ui_load_document("rfg_ui_ingame_options", DFIT_ALPHA, 0.5f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (Doc_handle_dthoptions == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        gameseq_pop_state();
        return;
    }
    if (!Multiplayer)
    {
        ui_set_screen_blur(true, false);
    }
    hud_display_firearm_interface(false);
    player_lock_local_player_controls();
    Control_id_dthoptions = (int)ui_request_control_id();
    Input_id_dthoptions = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    hud_ui_unload_items();
}

// 0x009042A0 (:245)
// With no box up: the select hint, then the menu items; a released click selects.
void player_death_options_process_mouse_input()
{
    if (Utility_doc_handle != -1 && ui_is_msgbox_active(-2))
    {
        return;
    }
    const char* names[1] = {"hints"};
    const int hints[1] = {HT_SELECTION};
    vint_object_base* objects[1] = {nullptr};
    ui_pc_collect_vint_obj_handles_by_name(names, 1, objects, Doc_handle_dthoptions);
    const int clicked = ui_mouse_process_clickables(objects, 1, true);
    if (clicked >= 0 && hints[clicked] != HT_SELECTION)
    {
        return;
    }
    int index = ui_menu_process_mouseover(Doc_handle_dthoptions, Menu_handle_dthoptions, kTextGroup);
    if (index < 0)
    {
        if (clicked > -1 && hints[clicked] == HT_SELECTION)
        {
            index = ui_menu_get_selected_item(Menu_handle_dthoptions);
        }
        if (index < 0)
        {
            return;
        }
    }
    if (Mouse_buttons[0].just_released)
    {
        ui_menu_selection_clicked(Menu_handle_dthoptions, index, false, false);
    }
}

// 0x00904370 (:285)
// Continue, load (disabled without saving), quit; the select hint; the pause text and box tinted red.
void player_death_options_create_menu()
{
    Menu_handle_dthoptions = ui_menu_add(Doc_handle_dthoptions, "MENU_YOU_DIED", nullptr, player_death_options_hint_callback, "_ingame", true, -1, Input_id_dthoptions, false);
    ui_menu* menu = ui_menu_get_via_handle(Menu_handle_dthoptions);
    if (menu != nullptr)
    {
        menu->add_function_item("MENU_CONTINUE", player_death_options_continue_cb, true, false, false);
        menu->add_function_item("MENU_LOAD_GAME", player_death_options_load_cb, true, false, false);
        menu->add_function_item("MENU_QUIT_GAME", player_death_option_mainmenu_cb, false, false, false);
        for (int hint = HT_SELECTION; hint < 4; hint++)
        {
            menu->remove_hint((hint_type)hint);
        }
    }
    char text[128];
    char image[256];
    // Magic number: 0x80, the hint text buffer's size.
    controls_fill_options_button_hint(text, 0x80, localization_get_display_string_req("MENU_SELECT_WITH_BUTTON", nullptr));
    menu = ui_menu_get_via_handle(Menu_handle_dthoptions);
    if (menu != nullptr)
    {
        // Original: 0x40 passed as the image buffer's size.
        controls_get_button_image_name(image, 0x40, CBA_MENU_SELECT, Player_controller_index);
        menu->add_hint(HT_SELECTION, CBA_MENU_SELECT, text, image);
    }
    if (!game_save_is_saving_enbaled())
    {
        ui_menu_enable_menu_item(Menu_handle_dthoptions, 1, false);
    }
    vint_doc_info* info = ui_get_doc_info(Doc_handle_dthoptions);
    if (info != nullptr && info->m_doc != nullptr)
    {
        vint_object_base* object = vint_object_base::find_object("paused_text", info->m_doc);
        if (object != nullptr)
        {
            object->set_property_value("tint", "0.5,0,0", 0);
        }
        object = vint_object_base::find_object("paused_box", info->m_doc);
        if (object != nullptr)
        {
            object->set_property_value("tint", "1.0,0,0", 0);
        }
    }
}

// 0x009164A0 (:324)
// The document loading (failed: continue at once), then the menu, shown once on top, the mouse.
void player_death_options_process()
{
    if (Assets_loaded_dthoptions)
    {
        if (Menu_handle_dthoptions == -1 && gameseq_get_state() == GS_PLAYER_DEATH_OPTIONS)
        {
            player_death_options_create_menu();
        }
        if (gameseq_get_state() == GS_PLAYER_DEATH_OPTIONS && gameseq_get_pending_state() == GS_NONE)
        {
            player_death_options_show_menu(true);
        }
        Render_ok = true;
        if (Menu_handle_dthoptions != -1)
        {
            if (!ui_menu_get_tween_on_select_end(Menu_handle_dthoptions) && !ui_menu_is_fading_in(Menu_handle_dthoptions))
            {
                player_death_options_process_mouse_input();
            }
            ui_add_for_processing(Doc_handle_dthoptions);
            ui_process((unsigned int)Control_id_dthoptions, false);
        }
        return;
    }
    switch (ui_get_doc_asset_state(Doc_handle_dthoptions))
    {
    case ST_LOADING:
        break;
    case ST_LOADED:
        Assets_loaded_dthoptions = true;
        break;
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        [[fallthrough]];
    case ST_ERROR:
        // Magic number: a 0.5 s fade out.
        ui_unload_document(Doc_handle_dthoptions, DFOT_NONE, 0.5f);
        Doc_handle_dthoptions = -1;
        player_death_options_continue_cb(0);
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        break;
    }
    if (!Assets_loaded_dthoptions)
    {
        ui_process((unsigned int)Control_id_dthoptions, false);
        return;
    }
    player_death_options_create_menu();
}

// 0x008930A0 (:387)
void player_death_options_handshake(rl_renderer*)
{
}

// 0x00904640 (:392)
void player_death_options_render(rl_renderer*)
{
    if (Render_ok)
    {
        ui_add_for_rendering(Doc_handle_dthoptions);
    }
    ui_render((unsigned int)Control_id_dthoptions, false);
}

// 0x008F4190 (:402)
// The menu, document and ids go, the controls come back, the blur ends (single player, the music on).
void player_death_options_exit(game_state new_state)
{
    ui_menu_destroy(Menu_handle_dthoptions);
    // Magic number: a 0.5 s fade out.
    ui_unload_document(Doc_handle_dthoptions, DFOT_NONE, 0.5f);
    ui_relinquish_control_id((unsigned int)Control_id_dthoptions);
    ui_relinquish_input_id(Input_id_dthoptions);
    Control_id_dthoptions = -1;
    Menu_handle_dthoptions = -1;
    Doc_handle_dthoptions = -1;
    player_unlock_local_player_controls();
    if (!Multiplayer)
    {
        ui_set_screen_blur(false, false);
    }
}

// 0x008930B0 (:422)
int player_death_options_get_control_id()
{
    return Control_id_dthoptions;
}

// 0x008930C0 (:428)
int player_death_options_get_doc_handle()
{
    return Doc_handle_dthoptions;
}

// 0x008D7090 (:435)
void player_death_options_show_menu(bool show)
{
    vint_doc_info* info = ui_get_doc_info(Doc_handle_dthoptions);
    if (info != nullptr && info->m_doc != nullptr)
    {
        vint_object_base* group = vint_object_base::find_object("screen_group", info->m_doc);
        if (group != nullptr)
        {
            group->set_property_value("visible", vint_variant(show), 0);
        }
    }
}
