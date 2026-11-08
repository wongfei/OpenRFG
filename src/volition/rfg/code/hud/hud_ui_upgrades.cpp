// Original: volition/rfg/code/hud/hud_ui_upgrades.cpp
//
// The upgrade store screen: the upgrades the player can buy, their prices and blueprints.

#include "volition/rfg/code/hud/hud_ui_upgrades.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/ai/ai_scripting.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/cutscene/video_cutscene.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/hud/hud_ui_weapon_cabinet.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_meta_data.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/varray.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <math.h>
#include <string.h>

// Magic numbers: 62 upgrade definitions, four rows on the screen.
static const int Num_upgrades = 62;
static const int Num_upgrade_rows = 4;

// ---- file-static globals
static const char* cDocname_upgrades = "rfg_ui_upgrades"; // 0x01661108
static const char* cTitle_upgrades = "MENU_UPGRADES"; // 0x0166110C
static const char* cPrice = "MENU_PRICE"; // 0x01661110
static const char* cSalvage = "MENU_CREDITS"; // 0x01661114
static const char* cUnknown = "MENU_UNKNOWN"; // 0x01661118
static const char* cBack_upgrades = "MENU_BACK"; // 0x0166111C
static const char* cNoneAvailable = "UPG_NO_UPGRADE"; // 0x01661120
static const char* NANO_RIFLE_MOVIE_NAME = "RFG_CINE_03B.bik"; // 0x01661134
static int Doc_handle_upgrades = -1; // 0x01661138
static int Msgbox_handle_upgrades = -1; // 0x0166113C
static int Data_header_handle_upgrades; // 0x01661140
static int Data_button_group_handle; // 0x01661144
static int Data_hints_handle_upgrades; // 0x01661148
static int Input_id_upgrades = -1; // 0x0166114C
static farray<upgrade_list_item, Num_upgrades> Upgrades_list; // 0x016617F8
static const char* kScrollBarOutline = "black_scrollfill"; // 0x016626F4
static const char* kRow4 = "row_button_4"; // 0x016626F8
static const char* kRow3 = "row_button_3"; // 0x016626FC
static const char* kRow2 = "row_button_2"; // 0x01662700
static const char* kRow1 = "row_button_1"; // 0x01662704
static const char* kHintImg = "hints_button"; // 0x01662708
static const char* kHintText = "hints_text"; // 0x0166270C
static int Data_button_handles[Num_upgrade_rows]; // 0x02B955A8
static const char* Streamed_image_name_upgrades; // 0x02B955F8
static bool Exit_upgrades; // 0x02B9DD07
static bool Loaded_upgrades; // 0x02B9DD10
static bool Exit_processed; // 0x02B9DD11
static bool Upgraded; // 0x02B9DD12
static bool Purchased_this_frame; // 0x02B9DD13
static unsigned int Control_id_upgrades; // 0x02B9DD14
static int Display_start_index; // 0x02B9DD18
static int Selected_row_upgrades; // 0x02B9DD1C
static bool Load_image_next_frame_upgrades; // 0x02B9DD20
static int time_we_moved_onto_an_entry_upgrades; // 0x02B9DD24
static checksum_stri Tooltips_to_display[Num_upgrades]; // 0x02BD02E0

// 0x0084AF70 (:136)
void unused_dcf_show_upgrades()
{
    gameseq_push_state(GS_INGAME_UPGRADES_HUD, true, false);
}

// 0x0084AF80 (:145)
static void hud_ui_init_audio_info_upgrades()
{
    audiolib_cue_precache(Ga_cue_ids.weapon_upgrade_menu_close);
    audiolib_cue_precache(Ga_cue_ids.weapon_upgrade_menu_open);
    audiolib_cue_precache(Ga_cue_ids.weapon_upgrade_no_purchase);
    audiolib_cue_precache(Ga_cue_ids.weapon_upgrade_purchase);
}

// 0x0087F5E0 (:153)
bool hud_ui_upgrades_msgbox_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    (void)selection_index;
    if (Msgbox_handle_upgrades != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        const int row = Selected_row_upgrades + Display_start_index;
        if (row >= Upgrades_list.size())
        {
            return true;
        }
        const int upgrade_id = Upgrades_list[row].index;
        player_get_meta_data()->purchase_upgrade(upgrade_id, false, false);
        hud_ui_upgrades_update_list();
        if (row >= Upgrades_list.size() - 1)
        {
            if (Display_start_index > 0)
            {
                Display_start_index--;
            }
            else if (Selected_row_upgrades > 0)
            {
                Selected_row_upgrades--;
            }
        }
        Upgraded = true;
        audiolib_cue_play(Ga_cue_ids.weapon_upgrade_purchase, nullptr, nullptr);
        const upgrade_definition_data* data = upgrade_get_data_by_index(upgrade_id);
        if (data != nullptr)
        {
            Tooltips_to_display[upgrade_id] = checksum_stri(data->get_post_purchase_tool_tip_crc());
            Purchased_this_frame = true;
        }
    }
    else if (choice == MBC_ABORT)
    {
        ui_menu_play_sound(MENU_SOUND_PRESS_SELECT);
    }
    else if (choice != MBC_FORCED_EXIT)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    Msgbox_handle_upgrades = -1;
    ui_wait_for_button_ups();
    return true;
}

// 0x00859680 (:219)
void hud_ui_upgrades_entry_set_viewed(int index, int curr_time, int start_time)
{
    // Magic number: an upgrade counts as seen after a second on it.
    if (index >= Upgrades_list.size() || curr_time - start_time <= 1000)
    {
        return;
    }
    upgrade_list_item& item = Upgrades_list[index];
    if (item.index > -1 && item.index < Num_upgrades)
    {
        player_meta_data* meta_data = player_get_meta_data();
        meta_data->clear_new_upgrade_notify(item.index);
        item.is_new = meta_data->is_next_upgrade_new_to_the_store(item.index);
    }
}

// 0x008596F0 (:234)
void hud_ui_upgrades_process_scroll_bar()
{
    if (!mouse_just_pressed(0) && !mouse_just_released(0) && !mouse_is_pressed(0))
    {
        return;
    }
    const int count = Upgrades_list.size();
    if (count <= Num_upgrade_rows)
    {
        return;
    }
    const int max_start = count - Num_upgrade_rows;
    vint_document* doc = ui_get_vint_document(Doc_handle_upgrades);
    if (doc == nullptr)
    {
        return;
    }
    vint_object_base* outline = vint_object_base::find_object(kScrollBarOutline, doc);
    if (outline == nullptr || !ui_is_mouse_over_vint_object(outline))
    {
        return;
    }
    float x = 0.0f;
    float y = 0.0f;
    float height = 0.0f;
    ui_get_vint_obj_position(outline, x, y);
    ui_get_vint_obj_size(outline, x, height);
    const float fraction = (float)(((double)Mouse_scaled_y - (double)y) / (double)height);
    const float position = (float)((double)fraction * (double)(float)count + 0.5);
    int start = (int)floor((double)position);
    int row = 0;
    if (start > max_start)
    {
        row = start - max_start - 1;
        start = max_start;
    }
    if (start != Display_start_index || row != Selected_row_upgrades)
    {
        audiolib_cue_play(Ga_cue_ids.ui_sound_move_up, nullptr, nullptr);
        Display_start_index = start;
        Selected_row_upgrades = row;
    }
}

// 0x008598A0 (:291)
// result: -2 the back hint, 1 a row was clicked, 2 the mouse is over a row, else 0. choice: the row, from 1.
void hud_ui_upgrades_process_mouse_input(int& result, int& choice)
{
    choice = -1;
    if (!mouse_just_released(0) && !mouse_just_moved() && !mouse_just_scrolled_wheel())
    {
        result = 0;
        return;
    }
    if (ui_mouse_over_hint_combo(kHintImg, kHintText, Doc_handle_upgrades, true))
    {
        result = -2;
        return;
    }
    vint_document* doc = ui_get_vint_document(Doc_handle_upgrades);
    result = 0;
    if (ui_is_mouse_over_vint_object(kRow1, doc))
    {
        choice = 1;
        result = 2;
    }
    else if (ui_is_mouse_over_vint_object(kRow2, doc))
    {
        choice = 2;
        result = 2;
    }
    else if (ui_is_mouse_over_vint_object(kRow3, doc))
    {
        choice = 3;
        result = 2;
    }
    else if (ui_is_mouse_over_vint_object(kRow4, doc))
    {
        choice = 4;
        result = 2;
    }
    if (Upgrades_list.size() < choice)
    {
        choice = Upgrades_list.size();
        result = 0;
        return;
    }
    if (mouse_just_released(0) && choice > 0)
    {
        result = 1;
        return;
    }
    if (result != 2)
    {
        hud_ui_upgrades_process_scroll_bar();
        result = 0;
    }
}

// 0x0087F700 (:375)
void hud_ui_upgrades_process_input()
{
    if (ui_get_current_input_id() != Input_id_upgrades || ui_is_msgbox_active(-2)) // Magic number: -2, any message box.
    {
        return;
    }
    const int old_row = Selected_row_upgrades + Display_start_index;
    const int curr_time = timer_get(TIMER_MILLISECONDS);
    int mouse_result = 0;
    int mouse_choice = 0;
    hud_ui_upgrades_process_mouse_input(mouse_result, mouse_choice);
    if ((mouse_result == 2 || (mouse_result == 1 && mouse_choice != -1)) && Selected_row_upgrades != mouse_choice - 1)
    {
        Selected_row_upgrades = mouse_choice - 1;
        hud_ui_upgrades_entry_set_viewed(old_row, curr_time, time_we_moved_onto_an_entry_upgrades);
    }
    // Magic numbers: ui button ids 11 and 29 back, 10 accept, 1 up, 2 down.
    if (ui_get_button_info(11, Input_id_upgrades) > 0.0f || ui_get_button_info(29, Input_id_upgrades) > 0.0f || mouse_result == -2)
    {
        audiolib_cue_play(Ga_cue_ids.weapon_upgrade_menu_close, nullptr, nullptr);
        Exit_upgrades = true;
        Exit_processed = false;
        int delay_ms = 1000; // Magic number: the post purchase tips show a second apart.
        for (int i = 0; i < Num_upgrades; i++)
        {
            if (Tooltips_to_display[i] != checksum_stri::INVALID)
            {
                ui_tool_tips_add_tip(Tooltips_to_display[i], delay_ms, nullptr);
                delay_ms += 1000;
            }
        }
        hud_ui_upgrades_entry_set_viewed(old_row, curr_time, time_we_moved_onto_an_entry_upgrades);
        Upgrades_list.clear();
        ui_unload_document(Doc_handle_upgrades, DFOT_NONE, 0.0f);
        Doc_handle_upgrades = -1;
        hud_ui_reload_items();
        time_we_moved_onto_an_entry_upgrades = timer_get(TIMER_MILLISECONDS);
    }
    else if (ui_get_button_info(10, Input_id_upgrades) > 0.0f || mouse_result == 1)
    {
        if (old_row < Upgrades_list.size() && Upgrades_list[old_row].available)
        {
            ui_menu_play_sound(MENU_SOUND_PRESS_SELECT);
            const wchar_t* name = localization_get_display_string_req(Upgrades_list[old_row].name_crc);
            if (name == nullptr)
            {
                return;
            }
            // Magic numbers: the sizes of rfg.exe's buffers on the stack.
            wchar_t text[72];
            char encoded[218];
            _snprintf_safe(text, 0x48, L"%ls: %ls \n%ls:\r %i", localization_get_display_string_req("MENU_UPGRADE", nullptr), name, localization_get_display_string_req("MENU_PRICE", nullptr), Upgrades_list[old_row].price);
            vint_encode_wide_text(encoded, 0xDA, text);
            Msgbox_handle_upgrades = ui_add_msgbox(MBT_ACCEPT_CANCEL_SELECT, "MENU_CONFIRM_PURCHASE", encoded, hud_ui_upgrades_msgbox_cb, true, false, nullptr, nullptr, false);
        }
        else
        {
            audiolib_cue_play(Ga_cue_ids.weapon_upgrade_no_purchase, nullptr, nullptr);
        }
        time_we_moved_onto_an_entry_upgrades = timer_get(TIMER_MILLISECONDS);
    }
    else if (ui_get_button_info(1, Input_id_upgrades) > 0.0f || controls_get_analog(CAA_SCROLL_BAR, Player_controller_index) > 0.0f)
    {
        if (Upgrades_list.size() > 1)
        {
            ui_menu_play_sound(MENU_SOUND_MOVE_UP);
            if (Selected_row_upgrades > 0)
            {
                Selected_row_upgrades--;
            }
            else if (Display_start_index > 0)
            {
                Display_start_index--;
            }
            else
            {
                const int last = Upgrades_list.size() - 1;
                Selected_row_upgrades = last > 2 ? Num_upgrade_rows - 1 : last;
                const int start = Upgrades_list.size() - Num_upgrade_rows;
                Display_start_index = start > 0 ? start : 0;
            }
        }
        hud_ui_upgrades_entry_set_viewed(old_row, curr_time, time_we_moved_onto_an_entry_upgrades);
    }
    else if (ui_get_button_info(2, Input_id_upgrades) > 0.0f || controls_get_analog(CAA_SCROLL_BAR, Player_controller_index) < 0.0f)
    {
        if (Upgrades_list.size() > 1)
        {
            ui_menu_play_sound(MENU_SOUND_MOVE_DOWN);
            if (old_row >= Upgrades_list.size() - 1)
            {
                Selected_row_upgrades = 0;
                Display_start_index = 0;
            }
            else if (Selected_row_upgrades + 1 < Num_upgrade_rows)
            {
                Selected_row_upgrades++;
            }
            else
            {
                Display_start_index++;
            }
        }
        hud_ui_upgrades_entry_set_viewed(old_row, curr_time, time_we_moved_onto_an_entry_upgrades);
    }
    const int row = Selected_row_upgrades + Display_start_index;
    if (row < Upgrades_list.size() || Upgrades_list.size() == 0)
    {
        if (row < 0)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            Display_start_index = 0;
            Selected_row_upgrades = 0;
        }
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        Display_start_index = Upgrades_list.size() - Num_upgrade_rows;
        if (Display_start_index < 0)
        {
            Display_start_index = 0;
        }
        Selected_row_upgrades = row - Display_start_index;
    }
    if (time_we_moved_onto_an_entry_upgrades == 0 || row != old_row)
    {
        time_we_moved_onto_an_entry_upgrades = timer_get(TIMER_MILLISECONDS);
    }
}

// 0x0084AFC0 (:548)
void hud_ui_upgrades_create_header_data()
{
    vint_data_item* item = vint_data_item::create_new("HeaderData");
    if (item != nullptr)
    {
        // Original bug, kept: process_header sets an eleventh element, a bool, past this template.
        static const vint_variant_type types[10] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_UINT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_UINT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_FLOAT};
        item->set_template(types, 10);
        Data_header_handle_upgrades = (int)item->get_handle();
        ui_add_data_item(Doc_handle_upgrades, Data_header_handle_upgrades);
    }
}

// 0x0084B060 (:577)
void hud_ui_upgrades_create_button_data()
{
    vint_data_group* group = vint_data_group::create_new("ButtonData");
    if (group == nullptr)
    {
        return;
    }
    Data_button_group_handle = (int)group->get_handle();
    ui_add_data_group(Doc_handle_upgrades, Data_button_group_handle);
    for (int i = 0; i < Num_upgrade_rows; i++)
    {
        vint_data_item* item = group->add_data_item(nullptr);
        if (item != nullptr)
        {
            static const vint_variant_type types[9] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_UINT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL};
            item->set_template(types, 9);
            Data_button_handles[i] = (int)item->get_handle();
        }
    }
}

// 0x0084B120 (:617)
void hud_ui_upgrades_create_hints_data()
{
    vint_data_item* item = vint_data_item::create_new("HintData");
    if (item != nullptr)
    {
        static const vint_variant_type types[3] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 3);
        Data_hints_handle_upgrades = (int)item->get_handle();
        ui_add_data_item(Doc_handle_upgrades, Data_hints_handle_upgrades);
    }
}

// 0x0084B190 (:639)
void hud_ui_upgrades_create_data_items()
{
    hud_ui_upgrades_create_header_data();
    hud_ui_upgrades_create_button_data();
    hud_ui_upgrades_create_hints_data();
}

// 0x00859A00 (:647)
void hud_ui_upgrades_process_header()
{
    vint_data_item* item = vint_data_item::find(Data_header_handle_upgrades);
    if (item == nullptr)
    {
        return;
    }
    const player_meta_data* meta_data = player_get_meta_data();
    unsigned int name_crc = 0;
    unsigned int description_crc = 0;
    // Magic number: 72, the buffers' sizes in the asm. Both start as a space.
    char price[72] = " ";
    char salvage[72] = " ";
    const char* icon = nullptr;
    const char* blueprint = nullptr;
    item->set_element(0, vint_variant(cTitle_upgrades));
    const int count = Upgrades_list.size();
    if (count > 0)
    {
        const upgrade_list_item& selected = Upgrades_list[Selected_row_upgrades + Display_start_index];
        if (selected.price < 0)
        {
            price[0] = '\0';
        }
        else if (!selected.locked)
        {
            _snprintf_safe(price, 0x48, "%i", selected.price);
        }
        else
        {
            _snprintf_safe(price, 0x48, cUnknown);
        }
        name_crc = selected.name_crc;
        description_crc = selected.description_crc;
        icon = selected.icon_name;
        if (selected.blueprint_name != nullptr && ui_get_image_status(selected.blueprint_name) == ST_LOADED)
        {
            blueprint = selected.blueprint_name;
        }
    }
    _snprintf_safe(salvage, 0x48, "%i", meta_data->get_salvage());
    const float scroll = (float)(Selected_row_upgrades + Display_start_index) / (float)(count - 1);
    item->set_element(1, vint_variant(name_crc));
    item->set_element(2, vint_variant(icon));
    item->set_element(3, vint_variant(blueprint));
    item->set_element(4, vint_variant(cPrice));
    item->set_element(5, vint_variant(price));
    item->set_element(6, vint_variant(description_crc));
    item->set_element(7, vint_variant(cSalvage));
    item->set_element(8, vint_variant(salvage));
    item->set_element(9, vint_variant(scroll));
    item->set_element(10, vint_variant(Upgrades_list.size() > Num_upgrade_rows));
}

// 0x00859D70 (:708)
void hud_ui_upgrades_process_buttons()
{
    const int count = Upgrades_list.size();
    const int selected_index = Selected_row_upgrades + Display_start_index;
    // Kept between the rows, as in rfg.exe: a row past the list repeats the last row's two flags.
    bool highlighted = false;
    bool even = false;
    for (int i = 0; i < Num_upgrade_rows; i++)
    {
        vint_data_item* item = vint_data_item::find(Data_button_handles[i]);
        if (item == nullptr)
        {
            continue;
        }
        const int index = Display_start_index + i;
        bool visible = false;
        if (index < count)
        {
            even = index % 2 == 0;
            visible = true;
            highlighted = Selected_row_upgrades == i;
            if (Upgrades_list[index].price < 0)
            {
                highlighted = false;
                even = false;
            }
        }
        // Original bug, kept: rows past the list read past its end (the array's capacity holds them unless it is full).
        item->set_element(0, vint_variant(i + 1));
        item->set_element(1, vint_variant(visible));
        item->set_element(2, vint_variant(Upgrades_list.elements()[index].locked));
        item->set_element(3, vint_variant(Upgrades_list.elements()[index].available));
        item->set_element(4, vint_variant(highlighted));
        item->set_element(5, vint_variant(even));
        item->set_element(6, vint_variant(Upgrades_list.elements()[index].name_crc));
        item->set_element(7, vint_variant(Upgrades_list.elements()[index].is_new));
        item->set_element(8, vint_variant(Upgrades_list.elements()[selected_index].is_new));
    }
}

// 0x0084B210 (:760)
void hud_ui_upgades_proccess_hints()
{
    vint_data_item* item = vint_data_item::find(Data_hints_handle_upgrades);
    if (item == nullptr)
    {
        return;
    }
    char button[72];
    char image[128];
    controls_get_button_image_name(button, 0x48, CBA_MENU_SELECT, Player_controller_index);
    keen::formatString(image, 0x80, "[image:%s]", button);
    item->set_element(0, vint_variant(image));
    controls_get_button_image_name(button, 0x48, CBA_MENU_ABORT, Player_controller_index);
    keen::formatString(image, 0x80, "[image:%s]", button);
    item->set_element(1, vint_variant(image));
    item->set_element(2, vint_variant(cBack_upgrades));
}

// 0x00859FD0 (:783)
void hud_ui_upgrades_process_info()
{
    hud_ui_upgrades_process_header();
    hud_ui_upgrades_process_buttons();
    hud_ui_upgades_proccess_hints();
}

// 0x00880750 (:797)
void hud_ui_upgrades_init()
{
    state_info info;
    info.enter = hud_ui_upgrades_enter;
    info.exit = hud_ui_upgrades_exit;
    info.process = hud_ui_upgrades_process;
    info.handshake = hud_ui_upgrades_handshake;
    info.render = hud_ui_upgrades_render;
    info.transparent = true;
    info.pause_beneath = true;
    gameseq_register_state(GS_INGAME_UPGRADES_HUD, info);
}

// 0x0087FBE0 (:812)
void hud_ui_upgrades_enter(game_state prev_state)
{
    (void)prev_state;
    Exit_upgrades = false;
    Loaded_upgrades = false;
    Purchased_this_frame = false;
    Display_start_index = 0;
    Selected_row_upgrades = 0;
    Input_id_upgrades = -1;
    Streamed_image_name_upgrades = nullptr;
    ui_set_screen_blur(true, false);
    Doc_handle_upgrades = ui_load_document(cDocname_upgrades, DFIT_NONE, 0.0f, DLT_NORMAL, true, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (Doc_handle_upgrades == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        gameseq_pop_state();
        return;
    }
    hud_ui_unload_items();
    hud_ui_init_audio_info_upgrades();
    Control_id_upgrades = ui_request_control_id();
    ui_wait_for_button_ups();
    hud_ui_upgrades_update_list();
    for (int i = 0; i < Num_upgrades; i++)
    {
        Tooltips_to_display[i] = checksum_stri::INVALID;
    }
    // The guerrilla tech stops talking while the store is open.
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        npc* np = World.get_object<npc>(i);
        if (np->h_info->flags.guerrilla_tech)
        {
            np->stop_voice(VOICE_LINE_PRIORITY_CRITICAL);
            break;
        }
    }
}

// 0x0084B350 (:872)
void hud_ui_upgrades_exit(game_state new_state)
{
    if (Streamed_image_name_upgrades != nullptr)
    {
        ui_release_image(Streamed_image_name_upgrades);
        Streamed_image_name_upgrades = nullptr;
    }
    if (Doc_handle_upgrades != -1)
    {
        ui_unload_document(Doc_handle_upgrades, DFOT_NONE, 0.5f);
    }
    if (Msgbox_handle_upgrades != -1)
    {
        ui_hide_msgbox(Msgbox_handle_upgrades, false);
    }
    ui_relinquish_control_id(Control_id_upgrades);
    ui_relinquish_input_id(Input_id_upgrades);
    ui_set_screen_blur(false, false);
    if (Upgraded)
    {
        if (new_state != GS_MAINMENU)
        {
            World.save_and_continue(nullptr, false, true);
        }
        Upgraded = false;
    }
    ui_kill_subtitle();
}

// 0x0085A000 (:909)
void hud_ui_upgrades_handshake(rl_renderer*)
{
    if (ui_is_msgbox_active(-2)) // Magic number: -2, any message box.
    {
        Purchased_this_frame = true;
        return;
    }
    if (Purchased_this_frame)
    {
        Purchased_this_frame = false;
        return;
    }
    if (!Loaded_upgrades)
    {
        return;
    }
    if (Upgrades_list.size() < 1)
    {
        if (Streamed_image_name_upgrades != nullptr)
        {
            ui_release_image(Streamed_image_name_upgrades);
            Streamed_image_name_upgrades = nullptr;
        }
        Load_image_next_frame_upgrades = false;
        return;
    }
    const char* blueprint = Upgrades_list[Display_start_index + Selected_row_upgrades].blueprint_name;
    if (Load_image_next_frame_upgrades)
    {
        if (blueprint != nullptr)
        {
            ui_request_image(blueprint);
        }
        Load_image_next_frame_upgrades = false;
        Streamed_image_name_upgrades = blueprint;
    }
    else if (Streamed_image_name_upgrades != blueprint)
    {
        if (Streamed_image_name_upgrades != nullptr)
        {
            if (blueprint != nullptr && v_stricmp(blueprint, Streamed_image_name_upgrades) == 0)
            {
                return;
            }
            ui_release_image(Streamed_image_name_upgrades);
            Streamed_image_name_upgrades = nullptr;
        }
        if (blueprint != nullptr)
        {
            Load_image_next_frame_upgrades = true;
        }
    }
}

// 0x0087FD20 (:975)
void hud_ui_upgrades_process()
{
    const bool msgbox_active = ui_is_msgbox_active(-2); // Magic number: -2, any message box.
    if (!msgbox_active)
    {
        if (Exit_upgrades)
        {
            if (!Exit_processed)
            {
                Exit_processed = true;
                const int response_line = meta_data_get_last_upgrade_voice_line();
                if (response_line > -1)
                {
                    ai_scripting_trigget_tech_response((voice_lines)response_line);
                }
                meta_data_clear_last_upgrade_voice_line();
                if (meta_data_purchased_nano_rifle())
                {
                    ui_fade_to_black_fade_out(false);
                    video_cutscene_play(NANO_RIFLE_MOVIE_NAME, 0xFFFFFFFF, nullptr, false, false, nullptr, nullptr, nullptr);
                    gameseq_pop_state();
                }
            }
            else if (hud_ui_is_loaded())
            {
                gameseq_pop_state();
            }
        }
        else
        {
            if (!Loaded_upgrades)
            {
                switch (ui_get_doc_asset_state(Doc_handle_upgrades))
                {
                case ST_UNLOADED:
                    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                    gameseq_pop_state();
                    break;
                case ST_ERROR:
                    gameseq_pop_state();
                    break;
                case ST_LOADING:
                    break;
                case ST_LOADED:
                    audiolib_cue_play(Ga_cue_ids.weapon_upgrade_menu_open, nullptr, nullptr);
                    Loaded_upgrades = true;
                    Input_id_upgrades = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
                    hud_ui_upgrades_create_data_items();
                    hud_ui_upgrades_process_info();
                    break;
                default:
                    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                    break;
                }
            }
            if (Loaded_upgrades)
            {
                hud_ui_upgrades_process_input();
                hud_ui_upgrades_process_info();
                ui_add_for_processing(Doc_handle_upgrades);
            }
        }
    }
    ui_process(Control_id_upgrades, msgbox_active);
}

// 0x0084B400 (:1063)
void hud_ui_upgrades_render(rl_renderer*)
{
    if (Loaded_upgrades)
    {
        ui_add_for_rendering(Doc_handle_upgrades);
    }
    ui_render(Control_id_upgrades, false);
}

// 0x0087C950 (:1073)
void hud_ui_upgrades_update_list()
{
    Upgrades_list.clear();
    player_meta_data* meta_data = player_get_meta_data();
    int num_visible = 0;
    for (int i = 0; i < Num_upgrades; i++)
    {
        const upgrade_level* level = meta_data->get_next_upgrade_level(i);
        if (level != nullptr)
        {
            upgrade_get_data_by_index(i);
            if (meta_data->is_next_upgrade_available(i) || level->show_locked)
            {
                num_visible++;
            }
        }
    }
    if (num_visible == 0)
    {
        upgrade_list_item none;
        none.index = (upgrade_definition_names)-1;
        none.name_crc = localization_generate_hash_from_key(cNoneAvailable);
        none.description_crc = 0;
        none.icon_name = nullptr;
        none.blueprint_name = nullptr;
        none.locked = false;
        none.available = false;
        none.is_new = false;
        none.price = -1;
        Upgrades_list.add(none);
        return;
    }
    for (int i = 0; i < Num_upgrades; i++)
    {
        const upgrade_level* level = meta_data->get_next_upgrade_level(i);
        if (level == nullptr)
        {
            continue;
        }
        const upgrade_definition_data* data = upgrade_get_data_by_index(i);
        if (!meta_data->is_next_upgrade_available(i) && !level->show_locked)
        {
            continue;
        }
        upgrade_list_item item;
        item.index = (upgrade_definition_names)i;
        item.name_crc = level->name_crc;
        item.description_crc = level->description_crc;
        item.icon_name = data->get_icon_name();
        item.blueprint_name = data->get_blueprint_name();
        const unsigned short cost = level->get_cost(data->get_id(), false);
        item.locked = level->show_locked;
        item.available = !item.locked && meta_data->get_salvage() >= (int)cost;
        item.is_new = meta_data->is_next_upgrade_new_to_the_store(i);
        item.price = cost;
        Upgrades_list.add(item);
        // Magic number: upgrade type 2 is a new weapon, listed locked in the cabinet. rfg.exe inlines the add_weapon call.
        if (data->get_upgrade_type() == 2 && data->get_weapon_info() != nullptr)
        {
            hud_ui_weapon_cabinet_add_weapon(data->get_weapon_info(), false);
        }
    }
}
