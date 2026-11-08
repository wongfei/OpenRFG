// Original: volition/rfg/code/interface/option_menus/extras_menu.cpp
//
// The extras menu: credits, save import, cheats. The Wrecking Crew map unlocks at the end are stand-ins (wrecking.md).

#include "volition/rfg/code/interface/option_menus/option_menu_system.h"

#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_save.h"
#include "volition/rfg/code/interface/credits.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/option_menus/ingame_options.h"
#include "volition/rfg/code/interface/ui_character.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_document_save_load.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/os/frametime.h"

// Magic numbers: the items of the menu, in the order extras_create_menu adds them.
enum extras_items
{
    EXTRAS_ITEM_CREDITS = 0,
    EXTRAS_ITEM_IMPORT = 1,
    EXTRAS_ITEM_CHEATS = 2,
    EXTRAS_ITEM_BACK = 3,
};

// ---- file-static globals
extern int Doc_handle_main; // 0x016631AC, ui_menu.cpp's
extern int Control_id_main; // 0x016631B4, main_menu.cpp's
extern bool Active; // 0x02C03754, ui_document_hud_tool_tips.cpp's
static int Menu_handle_extras = -1; // 0x016640D0
static const char* cMenu_group_name_extras = {"text_group"}; // 0x016640D4
static const char* cMenu_item_name_extras = {"text_clone"}; // 0x016640D8
static const char* Gamestop_maps[4] = {"wc3", "wc5", "wc7", "wc9"}; // 0x016640DC
static const char* PC_mapmayhem_maps[6] = {"wcdlc1", "wcdlc2", "wcdlc3", "wcdlc4", "wcdlc5", "wcdlc6"}; // 0x016640EC
extern bool Should_animate_in; // 0x02C07FC5 (options_main.cpp)

// 0x008C39C0 (:107)
static void extras_show_menu_items(bool show)
{
    if (show)
    {
        if (Active)
        {
            ui_menu_enable_menu_item(Menu_handle_extras, EXTRAS_ITEM_CHEATS, true);
            ui_menu_show_menu_item(Menu_handle_extras, EXTRAS_ITEM_CHEATS, true);
        }
        else
        {
            ui_menu_enable_menu_item(Menu_handle_extras, EXTRAS_ITEM_CREDITS, true);
            ui_menu_show_menu_item(Menu_handle_extras, EXTRAS_ITEM_CREDITS, true);
            if (Num_imported_save_file_infos != 0)
            {
                ui_menu_enable_menu_item(Menu_handle_extras, EXTRAS_ITEM_IMPORT, true);
                ui_menu_show_menu_item(Menu_handle_extras, EXTRAS_ITEM_IMPORT, true);
            }
        }
        ui_menu_enable_menu_item(Menu_handle_extras, EXTRAS_ITEM_BACK, true);
        ui_menu_show_menu_item(Menu_handle_extras, EXTRAS_ITEM_BACK, true);
        return;
    }
    for (int i = EXTRAS_ITEM_CREDITS; i <= EXTRAS_ITEM_BACK; i++)
    {
        ui_menu_enable_menu_item(Menu_handle_extras, i, false);
        ui_menu_show_menu_item(Menu_handle_extras, i, false);
    }
}

// 0x008D6A60 (:217), declared here for the callback above it in the file.
static void extras_destroy_menu();

// 0x008E6F00 (:146)
static void extras_selection_callback(int which)
{
    ui_wait_for_button_ups();
    switch (which)
    {
    case EXTRAS_ITEM_CREDITS:
        extras_destroy_menu();
        credits_show(false, 0xFFFFFFFF);
        break;
    case EXTRAS_ITEM_IMPORT:
        extras_destroy_menu();
        hud_ui_save_load_show(SLUT_IMPORT_SELECT);
        break;
    case EXTRAS_ITEM_CHEATS:
        extras_destroy_menu();
        gameseq_push_state(GS_CHEATS, true, false);
        Should_animate_in = true;
        break;
    case EXTRAS_ITEM_BACK:
        if (gameseq_get_state() == GS_MAINMENU_OPTIONS_EXTRAS)
        {
            gameseq_pop_state();
        }
        break;
    default:
        break;
    }
}

// 0x00890E90 (:178)
static void extras_selection_hint_callback(hint_type hint)
{
    if (hint == HT_ABORT && gameseq_get_state() == GS_MAINMENU_OPTIONS_EXTRAS)
    {
        gameseq_pop_state();
    }
}

// 0x008E6FF0 (:189)
static void extras_create_menu()
{
    Menu_handle_extras = ui_menu_add(Active ? Doc_handle_ioptions : Doc_handle_main, "", nullptr, extras_selection_hint_callback, nullptr, Should_animate_in, -1, -1, false);
    ui_menu_add_function_item(Menu_handle_extras, "MENU_EXTRAS_CREDITS", extras_selection_callback, false, false, false);
    ui_menu_add_function_item(Menu_handle_extras, "STEAM_SAVE_DATA_IMPORT_NAME", extras_selection_callback, false, false, false);
    ui_menu_add_function_item(Menu_handle_extras, "MENU_CHEATS", extras_selection_callback, false, false, false);
    ui_menu_add_function_item(Menu_handle_extras, "MENU_BACK", extras_selection_callback, false, false, false);
    char text[128];
    char image[256];
    controls_fill_options_button_hint(text, 0x80, localization_get_display_string_req("MENU_SELECT_WITH_BUTTON", nullptr));
    ui_menu* menu = ui_menu_get_via_handle(Menu_handle_extras);
    if (menu != nullptr)
    {
        controls_get_button_image_name(image, 0x40, CBA_MENU_SELECT, Player_controller_index);
        menu->add_hint(HT_SELECTION, CBA_MENU_SELECT, text, image);
    }
    controls_fill_options_button_hint(text, 0x80, localization_get_display_string_req("MENU_BACK_WITH_BUTTON", nullptr));
    menu = ui_menu_get_via_handle(Menu_handle_extras);
    if (menu != nullptr)
    {
        controls_get_button_image_name(image, 0x40, CBA_MENU_ABORT, Player_controller_index);
        menu->add_hint(HT_ABORT, CBA_MENU_ABORT, text, image);
    }
    extras_show_menu_items(false);
    extras_show_menu_items(true);
    // Deviation from rfg.exe: ui_menu_set_selected_item, where it sets m_selected directly.
    ui_menu_set_selected_item(Menu_handle_extras, 0);
}

// 0x008D6A60 (:217)
static void extras_destroy_menu()
{
    if (Menu_handle_extras != -1)
    {
        ui_menu_destroy(Menu_handle_extras);
        Menu_handle_extras = -1;
    }
}

// 0x008E7320 (:225)
static void extras_menu_process_mouse_input()
{
    unsigned int control_id;
    int doc_handle;
    if (gameseq_get_state() == GS_MAINMENU_IDLE || Doc_handle_main == -1)
    {
        control_id = (unsigned int)Control_id_ioptions;
        doc_handle = Doc_handle_ioptions;
    }
    else
    {
        control_id = (unsigned int)Control_id_main;
        doc_handle = Doc_handle_main;
    }
    const unsigned int top_control = Controller_slots.size() < 1 ? 1 : Controller_slots[Controller_slots.size() - 1];
    if (control_id != top_control)
    {
        return;
    }
    const char* hint_names[2] = {"hints", "hints2"};
    const int hint_actions[2] = {0, 1}; // Magic numbers: 0 selects, 1 goes back.
    vint_object_base* hint_objects[2] = {nullptr, nullptr};
    ui_pc_collect_vint_obj_handles_by_name(hint_names, 2, hint_objects, doc_handle);
    const int clicked = ui_mouse_process_clickables(hint_objects, 2, true);
    int row = ui_menu_process_mouseover_extend(doc_handle, Menu_handle_extras, cMenu_group_name_extras, cMenu_item_name_extras, false);
    if (row < 0)
    {
        if (clicked >= 0)
        {
            if (hint_actions[clicked] == 0)
            {
                row = ui_menu_get_selected_item(Menu_handle_extras);
            }
            else if (hint_actions[clicked] == 1)
            {
                ui_wait_for_button_ups();
                if (gameseq_get_state() == GS_MAINMENU_OPTIONS_EXTRAS)
                {
                    gameseq_pop_state();
                }
            }
        }
        if (row < 0)
        {
            return;
        }
    }
    if (mouse_just_released(0))
    {
        ui_menu_selection_clicked(Menu_handle_extras, row, false, false);
    }
}

// 0x008E7470 (:269)
static void extras_process_input()
{
    extras_menu_process_mouse_input();
}

// 0x009174E0 (:280)
void extras_menu_init()
{
    state_info info;
    info.enter = extras_menu_enter;
    info.exit = extras_menu_exit;
    info.process = extras_menu_process;
    info.handshake = extras_menu_handshake;
    info.render = extras_menu_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_MAINMENU_OPTIONS_EXTRAS, info);
    credits_init();
}

// 0x00890EB0 (:296)
void extras_menu_enter(game_state prev_state)
{
    (void)prev_state;
    Should_animate_in = false;
}

// 0x008D6A80 (:301)
void extras_menu_exit(game_state new_state)
{
    (void)new_state;
    extras_destroy_menu();
}

// 0x00915CF0 (:306)
void extras_menu_process()
{
    bool main_menu;
    unsigned int control_id;
    int doc_handle;
    if (gameseq_get_state() == GS_MAINMENU_IDLE || Doc_handle_main == -1)
    {
        main_menu = false;
        control_id = (unsigned int)Control_id_ioptions;
        doc_handle = Doc_handle_ioptions;
    }
    else
    {
        main_menu = true;
        control_id = (unsigned int)Control_id_main;
        doc_handle = Doc_handle_main;
    }
    if (Utility_doc_handle != -1 && ui_is_msgbox_active(-2)) // Magic number: -2, any message box.
    {
        ui_process(control_id, false);
        return;
    }
    if (Menu_handle_extras == -1 && gameseq_get_state() == GS_MAINMENU_OPTIONS_EXTRAS && gameseq_get_pending_state() == GS_NONE)
    {
        extras_create_menu();
    }
    if (gameseq_get_state() == GS_MAINMENU_OPTIONS_EXTRAS && !ui_menu_get_tween_on_select_end(Menu_handle_extras))
    {
        extras_process_input();
    }
    if (main_menu)
    {
        ui_character_process(Frametime_real);
    }
    ui_add_for_processing(doc_handle);
    ui_process(control_id, false);
}

// 0x00890EC0 (:333)
void extras_menu_handshake(rl_renderer*)
{
}

// 0x00900CA0 (:337)
void extras_menu_render(rl_renderer*)
{
    if (gameseq_get_state() != GS_MAINMENU_IDLE && Doc_handle_main != -1)
    {
        ui_add_for_rendering(Doc_handle_main);
        ui_render((unsigned int)Control_id_main, false);
    }
    else
    {
        ui_add_for_rendering(Doc_handle_ioptions);
        ui_render((unsigned int)Control_id_ioptions, false);
    }
}

// 0x00890ED0 (:354)
void extras_menu_init_wc_maps()
{
    for (int i = 0; i < 6; i++)
    {
        if (wrecking_crew_is_map_valid(PC_mapmayhem_maps[i]))
        {
            wrecking_crew_unlock_map(PC_mapmayhem_maps[i]);
        }
    }
    for (int i = 0; i < 4; i++)
    {
        if (wrecking_crew_is_map_valid(Gamestop_maps[i]))
        {
            wrecking_crew_unlock_map(Gamestop_maps[i]);
        }
    }
}
