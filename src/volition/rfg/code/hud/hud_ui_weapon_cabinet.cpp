// Original: volition/rfg/code/hud/hud_ui_weapon_cabinet.cpp
//
// The weapon cabinet and ammo crate screen: a carousel of the unlocked weapons and the player's three slots. The list is
// saved: a byte count, then each weapon's id and unlocked flag, as in rfg.exe.

#include "volition/rfg/code/hud/hud_ui_weapon_cabinet.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/hud/hud_ui_handbook.h"
#include "volition/rfg/code/hud/hud_ui_upgrades.h"
#include "volition/rfg/code/interface/ingame/ovl_inventory.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_meta_data.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/varray.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <string.h>
#include <wchar.h>

// size 0x8
struct user_ammo_data
{
    unsigned int weapon_id; // 0x00
    int ammo; // 0x04
};

// ---- file-static globals
static const char* cDocname_cabinet = "rfg_ui_safehouse_weapons"; // 0x01661150
static const char* cBack_cabinet = "MENU_DONE"; // 0x01661154
static int Doc_handle_cabinet = -1; // 0x01661158
static int Data_weapon_info_handle; // 0x0166115C
static int Data_icon_handle; // 0x01661160
static int Data_inventory_handle; // 0x01661164
static int Data_anim_handle; // 0x01661168
static int Data_hints_handle_cabinet; // 0x0166116C
static int Input_id_cabinet = -1; // 0x01661170
static int Selected_slot = -1; // 0x01661174
static int Singularity_bomb_index = -1; // 0x01661178
// Magic number: the most weapons a load accepts (the array's size).
static farray<ui_weapon_list_item, 64> Weapon_list; // 0x01661ED0
static farray<user_ammo_data, 4> Player_ammo; // 0x016620E0
static const char* kWeapon3 = "weapon_plate_03"; // 0x0166227C
static const char* kWeapon2 = "weapon_plate_02"; // 0x01662280
static const char* kWeapon1 = "weapon_plate_01"; // 0x01662284
static const char* kHintImg = "hint_button"; // 0x01662288
static const char* kHintText = "hint_text"; // 0x0166228C
// Magic numbers: the share of a crate's ammo by tenths of district morale.
static const float ammo_scale[11] = {0.2f, 0.3f, 0.35f, 0.4f, 0.45f, 0.5f, 0.55f, 0.6f, 0.7f, 0.8f, 1.0f}; // 0x01662710
static bool Exit_cabinet; // 0x02B9DD21
static bool Loaded_cabinet; // 0x02B9DD22
static bool Start_tween_up; // 0x02B9DD23
static unsigned int Control_id_cabinet; // 0x02B9DD28
static int Selected_index; // 0x02B9DD2C
static bool Start_tween_down; // 0x02B9DD30
static bool Wait_for_tween; // 0x02B9DD31
static bool Ammo_box; // 0x02B9DD32
static bool Updated_carousel; // 0x02B9DD33
static bool Player_had_singularity_bomb; // 0x02B9DD34
static bool Ammo_crate_allow_swap_all; // 0x02B9DD35: Ghidra lists only this file's read, so it stays false

// 0x0084B430 (:68)
bool ui_weapon_list_item::operator==(const ui_weapon_list_item& wli) const
{
    return wli.weapon_id == weapon_id;
}

// 0x0084B440 (:122)
void unused_dcf_show_weapon_cabinet()
{
    gameseq_push_state(GS_INGAME_WEAPON_CABINET, true, false);
}

// 0x0084B450 (:135)
static void hud_ui_init_audio_info_cabinet()
{
    audiolib_cue_precache(Ga_cue_ids.weapon_cabinet_scroll_up);
    audiolib_cue_precache(Ga_cue_ids.weapon_cabinet_scroll_down);
}

// 0x0084B470 (:142)
static bool singularity_bomb_confirm(int mbox_handle, int selection_index, msgbox_choice choice)
{
    (void)mbox_handle;
    (void)selection_index;
    if (choice != MBC_OK)
    {
        if ((int)choice < 1 || (int)choice > 2)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
        return true;
    }
    ui_menu_play_sound(MENU_SOUND_PRESS_SELECT);
    hud_ui_reload_items();
    Exit_cabinet = true;
    return true;
}

// 0x0085A0F0 (:164)
ui_weapon_list_item hud_ui_weapon_cabinet_get_weapon(int relative_offset, bool skip_singularity)
{
    ui_weapon_list_item result;
    // Deviation: rfg.exe leaves the empty list's unlocked flag uninitialized.
    result.weapon_id = 0;
    result.unlocked = false;
    const int count = Weapon_list.size();
    if (count <= 0)
    {
        return result;
    }
    int index = Selected_index;
    while (relative_offset != 0)
    {
        if (relative_offset < 1)
        {
            relative_offset++;
            index--;
            if (index < 0)
            {
                index = count - 1;
            }
            if (skip_singularity && index == Singularity_bomb_index)
            {
                index--;
                if (index < 0)
                {
                    index = count - 1;
                }
            }
        }
        else
        {
            relative_offset--;
            index++;
            if (index >= count)
            {
                index = 0;
            }
            if (skip_singularity && index == Singularity_bomb_index)
            {
                index++;
                if (index >= count)
                {
                    index = 0;
                }
            }
        }
    }
    return Weapon_list[index];
}

// 0x0087CBC0 (:220)
void hud_ui_weapon_cabinet_add_inventory_items()
{
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    Player_had_singularity_bomb = false;
    inventory_item* first = pp->inventory;
    inventory_item* item = first;
    while (item != nullptr)
    {
        const weapon* wp = World.object_from_handle<weapon>(item->weapon_handle);
        if (wp != nullptr)
        {
            if (strcmp(wp->w_info->name, "singularity_bomb") == 0)
            {
                Player_had_singularity_bomb = true;
            }
            // Magic number: slots 0 to 3 are the selectable weapons.
            if ((unsigned int)item->selection_slot < 4 && !hud_ui_weapon_cabinet_unlock_weapon(wp->w_info, true))
            {
                hud_ui_weapon_cabinet_add_weapon(wp->w_info, true);
            }
        }
        item = item->next;
        if (item == first)
        {
            break;
        }
    }
}

// 0x0085A170 (:248)
void weapon_cabinet_get_ammo_box_ammo(const weapon_info* w_info, int& max_ammo, int& user_ammo, int& base_ammo, int& morale_ammo, int& final_ammo)
{
    final_ammo = 0;
    morale_ammo = 0;
    base_ammo = 0;
    user_ammo = 0;
    max_ammo = 0;
    if (w_info == nullptr)
    {
        return;
    }
    max_ammo = upgrade_weapon_has_extra_ammo(w_info) ? w_info->max_rounds_upgrade : w_info->max_rounds;
    if (w_info->ammo_box_restock != 0)
    {
        if (player_get_local_player()->current_team == HUMAN_TEAM_MARAUDER)
        {
            morale_ammo = max_ammo;
        }
        else
        {
            // The asm's index: the negated truncation of the percent times -10.
            const float morale = gfm_get_district_morale_percent(nullptr);
            const float scale = ammo_scale[-(int)((double)morale * -10.0)];
            const float restock = (float)(int)w_info->ammo_box_restock;
            const float difficulty = difficulty_get_value(DIFFICULTY_PLAYER_AMMO_CRATES, NUM_DIFFICULTY_LEVELS);
            // x87 in the asm: the product is not rounded to float.
            morale_ammo = (int)((double)restock * (double)scale * (double)difficulty);
        }
        if (w_info->ammo_box_restock == 0)
        {
            morale_ammo = 0;
        }
    }
    for (int i = 0; i < Player_ammo.size(); i++)
    {
        if ((unsigned int)w_info->unique_id == Player_ammo[i].weapon_id)
        {
            user_ammo = Player_ammo[i].ammo;
        }
    }
    if (w_info->flags.never_in_cabinet)
    {
        final_ammo = user_ammo;
        return;
    }
    if (w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE)
    {
        final_ammo = -1;
        return;
    }
    if (Ammo_box)
    {
        const int total = user_ammo + base_ammo + morale_ammo;
        final_ammo = total > max_ammo ? max_ammo : total;
        return;
    }
    final_ammo = max_ammo;
}

// 0x0084B4B0 (:319)
void hud_ui_weapon_cabinet_create_weapon_info_data()
{
    vint_data_item* item = vint_data_item::create_new("SafehouseWeaponSetInfo");
    if (item != nullptr)
    {
        static const vint_variant_type types[7] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING};
        item->set_template(types, 7);
        Data_weapon_info_handle = (int)item->get_handle();
        ui_add_data_item(Doc_handle_cabinet, Data_weapon_info_handle);
    }
}

// 0x0084B540 (:345)
void hud_ui_weapon_cabinet_create_icon_data()
{
    vint_data_item* item = vint_data_item::create_new("SafehouseWeaponSetIcons");
    if (item != nullptr)
    {
        static const vint_variant_type types[14] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_BOOL};
        item->set_template(types, 14);
        Data_icon_handle = (int)item->get_handle();
        ui_add_data_item(Doc_handle_cabinet, Data_icon_handle);
    }
}

// 0x0084B5E0 (:378)
void hud_ui_weapon_cabinet_create_inventory_data()
{
    vint_data_item* item = vint_data_item::create_new("SafehouseWeaponSlotInfo");
    if (item != nullptr)
    {
        static const vint_variant_type types[9] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT};
        item->set_template(types, 9);
        Data_inventory_handle = (int)item->get_handle();
        ui_add_data_item(Doc_handle_cabinet, Data_inventory_handle);
    }
}

// 0x0084B670 (:406)
void hud_ui_weapon_cabinet_create_animation_data()
{
    vint_data_item* item = vint_data_item::create_new("SafehouseWeaponAnim");
    if (item != nullptr)
    {
        static const vint_variant_type types[2] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL};
        item->set_template(types, 2);
        Data_anim_handle = (int)item->get_handle();
        ui_add_data_item(Doc_handle_cabinet, Data_anim_handle);
    }
}

// 0x0084B6D0 (:427)
void hud_ui_weapon_cabinet_create_hints_data()
{
    vint_data_item* item = vint_data_item::create_new("SafehouseWeaponHints");
    if (item != nullptr)
    {
        static const vint_variant_type types[6] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 6);
        Data_hints_handle_cabinet = (int)item->get_handle();
        ui_add_data_item(Doc_handle_cabinet, Data_hints_handle_cabinet);
    }
}

// 0x0084B750 (:451)
void hud_ui_weapon_cabinet_create_data_items()
{
    hud_ui_weapon_cabinet_create_weapon_info_data();
    hud_ui_weapon_cabinet_create_icon_data();
    hud_ui_weapon_cabinet_create_inventory_data();
    hud_ui_weapon_cabinet_create_animation_data();
    hud_ui_weapon_cabinet_create_hints_data();
}

// 0x0084B7C0 (:460)
// Answers -1 for the back hint, else -2.
static int hud_ui_weapon_cabinet_process_mouse()
{
    if (ui_mouse_over_hint_combo(kHintImg, kHintText, Doc_handle_cabinet, true))
    {
        return -1;
    }
    if (mouse_just_released(0))
    {
        vint_document* doc = ui_get_vint_document(Doc_handle_cabinet);
        if (ui_is_mouse_over_vint_object(kWeapon1, doc))
        {
            Selected_slot = 0;
        }
        else if (ui_is_mouse_over_vint_object(kWeapon2, doc))
        {
            Selected_slot = 1;
        }
        else if (ui_is_mouse_over_vint_object(kWeapon3, doc))
        {
            Selected_slot = 2;
        }
    }
    return -2;
}

// The weapon at the carousel's selection, or the id 0 weapon's info for an empty list.
static const weapon_info* hud_ui_weapon_cabinet_selected_info()
{
    const unsigned int id = Weapon_list.size() < 1 ? 0 : Weapon_list[Selected_index].weapon_id;
    return weapons_get_weapon_info_from_weapon_unique_id(id);
}

// 0x0085A300 (:489)
void hud_ui_weapon_cabinet_process_input()
{
    if (ui_get_current_input_id() != Input_id_cabinet || ui_is_msgbox_active(-2)) // Magic number: -2, any message box.
    {
        return;
    }
    const int count = Weapon_list.size();
    Selected_slot = -1;
    const weapon_info* w_info = hud_ui_weapon_cabinet_selected_info();
    const int mouse_result = hud_ui_weapon_cabinet_process_mouse();
    // Magic numbers: ui button ids 11 back, 24 the weapon menu button, 1 up, 2 down, 25 to 27 the slots.
    if ((!controls_is_using_weapon_menu_button() && ui_get_button_info(11, Input_id_cabinet) > 0.0f) || (controls_is_using_weapon_menu_button() && ui_get_button_info(24, Input_id_cabinet) > 0.0f) || mouse_result == -1)
    {
        bool has_bomb = false;
        player* pp = player_get_local_player();
        inventory_item* item = pp->inventory;
        while (item != nullptr)
        {
            const weapon* wp = item->weapon_handle != 0xFFFFFFFF ? World.object_from_handle<weapon>(item->weapon_handle) : nullptr;
            if (wp != nullptr && strcmp(wp->w_info->name, "singularity_bomb") == 0)
            {
                has_bomb = true;
                break;
            }
            item = item->next;
            if (item == player_get_local_player()->inventory)
            {
                break;
            }
        }
        if (Player_had_singularity_bomb && !has_bomb)
        {
            ui_add_msgbox(MBT_ACCEPT_CANCEL_SELECT, "MENU_WARNING", "MENU_WPN_CAB_SING_BOMB", singularity_bomb_confirm, false, false, nullptr, nullptr, false);
        }
        else
        {
            ui_menu_play_sound(MENU_SOUND_PRESS_SELECT);
            hud_ui_reload_items();
            Exit_cabinet = true;
        }
    }
    else if (!Wait_for_tween && Updated_carousel)
    {
        if (ui_get_button_info(2, Input_id_cabinet) > 0.0f || controls_get_analog(CAA_SCROLL_BAR, Player_controller_index) < 0.0f)
        {
            Selected_index--;
            if (Selected_index < 0)
            {
                Selected_index = count - 1;
            }
            const weapon_info* next = hud_ui_weapon_cabinet_selected_info();
            if (next != nullptr && !Player_had_singularity_bomb && next->weapon_class == WEAPON_CLASS_SINGULARITY_BOMB)
            {
                Selected_index--;
                if (Selected_index < 0)
                {
                    Selected_index = count - 1;
                }
            }
            audiolib_cue_play(Ga_cue_ids.weapon_cabinet_scroll_down, nullptr, nullptr);
            Start_tween_down = true;
            Updated_carousel = false;
        }
        else if (ui_get_button_info(1, Input_id_cabinet) > 0.0f || controls_get_analog(CAA_SCROLL_BAR, Player_controller_index) > 0.0f)
        {
            Selected_index++;
            if (Selected_index >= count)
            {
                Selected_index = 0;
            }
            const weapon_info* next = hud_ui_weapon_cabinet_selected_info();
            if (next != nullptr && !Player_had_singularity_bomb && next->weapon_class == WEAPON_CLASS_SINGULARITY_BOMB)
            {
                Selected_index++;
                if (Selected_index >= count)
                {
                    Selected_index = 0;
                }
            }
            audiolib_cue_play(Ga_cue_ids.weapon_cabinet_scroll_up, nullptr, nullptr);
            Start_tween_up = true;
            Updated_carousel = false;
        }
        else if (ui_get_button_info(25, Input_id_cabinet) > 0.0f)
        {
            Selected_slot = 0;
        }
        else if (ui_get_button_info(26, Input_id_cabinet) > 0.0f)
        {
            Selected_slot = 1;
        }
        else if (ui_get_button_info(27, Input_id_cabinet) > 0.0f)
        {
            Selected_slot = 2;
        }
    }
    bool can_take = Ammo_crate_allow_swap_all;
    if (!can_take)
    {
        int max_ammo;
        int user_ammo;
        int base_ammo;
        int morale_ammo;
        int final_ammo = 0;
        weapon_cabinet_get_ammo_box_ammo(w_info, max_ammo, user_ammo, base_ammo, morale_ammo, final_ammo);
        can_take = final_ammo != 0;
    }
    // Deviation: for an empty list rfg.exe tests a stale stack slot here; the list is never empty on this screen, as
    // hud_ui_weapon_cabinet_enter leaves at once.
    if (can_take && Weapon_list.size() > 0 && Weapon_list[Selected_index].unlocked)
    {
        if (Selected_slot < 0 || w_info == nullptr)
        {
            return;
        }
        player* pp = player_get_local_player();
        if (pp == nullptr)
        {
            return;
        }
        int current_slot = -1;
        for (int i = 0; i < 3; i++)
        {
            const inventory_item* item = inv_item_get_item_in_slot(pp, i);
            if (item != nullptr && item->info != nullptr && w_info == item->info->w_info)
            {
                current_slot = i;
                break;
            }
        }
        if (current_slot != Selected_slot)
        {
            const inv_item_info* swapped = nullptr;
            inventory_item* item = human_get_inventory_item(pp, Selected_slot);
            if (item != nullptr)
            {
                swapped = item->info;
                human_delete_item_from_inventory(pp, item, false, false);
            }
            if (current_slot > -1)
            {
                item = human_get_inventory_item(pp, current_slot);
                if (item != nullptr)
                {
                    human_delete_item_from_inventory(pp, item, false, false);
                }
                if (swapped != nullptr)
                {
                    human_add_item_to_inventory(pp, swapped, 0, -1, current_slot, true, false, true);
                }
            }
            pp->unhide_weapon();
            pp->hide_weapon();
            human_add_item_to_inventory(pp, w_info->weapon_inv_item_info, 0, -1, Selected_slot, true, false, true);
            ui_menu_play_sound(MENU_SOUND_PRESS_SELECT);
            return;
        }
        ui_menu_play_sound(MENU_SOUND_INVALID_INPUT);
        Selected_slot = -1;
        return;
    }
    if (Selected_slot >= 0)
    {
        ui_menu_play_sound(MENU_SOUND_INVALID_INPUT);
        Selected_slot = -1;
    }
}

// 0x0085A850 (:685)
void hud_ui_weapon_cabinet_process_weapon_info_data()
{
    vint_data_item* item = vint_data_item::find(Data_weapon_info_handle);
    if (item == nullptr || Start_tween_down || Start_tween_up || Wait_for_tween)
    {
        return;
    }
    human_teams team = HUMAN_TEAM_NONE;
    bool has_ammo = true;
    const char* display_name = nullptr;
    char ammo_text[128];
    char count_text[32];
    wchar_t upgrades_wide[256];
    char upgrades[768];
    ammo_text[0] = '\0';
    count_text[0] = '\0';
    upgrades[0] = '\0';
    upgrades_wide[0] = L'\0';
    const weapon_info* w_info = hud_ui_weapon_cabinet_selected_info();
    if (w_info != nullptr)
    {
        if (w_info->weapon_inv_item_info != nullptr)
        {
            display_name = w_info->weapon_inv_item_info->display_name;
            int max_ammo;
            int user_ammo;
            int base_ammo;
            int morale_ammo;
            int final_ammo = 0;
            weapon_cabinet_get_ammo_box_ammo(w_info, max_ammo, user_ammo, base_ammo, morale_ammo, final_ammo);
            const bool unlocked = Weapon_list.size() > 0 && Weapon_list[Selected_index].unlocked;
            if (!unlocked)
            {
                has_ammo = false;
                _snprintf_safe(ammo_text, 0x80, "MENU_WC_UPGRADE_AVAILABLE");
            }
            else if (final_ammo < 0)
            {
                _snprintf_safe(ammo_text, 0x80, "");
            }
            else
            {
                if (final_ammo == 0)
                {
                    has_ammo = false;
                }
                _snprintf_safe(ammo_text, 0x80, Ammo_box ? "[format][scale:0.5][image:ui_hud_guerilla_logo_white][/format] %i" : "%i", final_ammo);
            }
        }
        // Magic number: 62 upgrade definitions.
        for (unsigned int i = 0; i < 62; i++)
        {
            const upgrade_definition_data* data = upgrade_get_data_by_index(i);
            if (data == nullptr)
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                continue;
            }
            if (data->get_weapon_info() != w_info)
            {
                continue;
            }
            player_meta_data* meta_data = player_get_meta_data();
            if (meta_data == nullptr)
            {
                continue;
            }
            const upgrade_level* level = meta_data->get_current_upgrade_level(data->get_id());
            // Magic number: upgrade type 2 only shows past its first level.
            if (level == nullptr || (data->get_upgrade_type() == 2 && level->value <= 1))
            {
                continue;
            }
            unsigned int text_crc = level->name_crc;
            if (text_crc == localization_generate_hash_from_key("UPG_CHARGES_COUNT_NAME"))
            {
                text_crc = level->description_crc;
            }
            if (localization_get_display_string_opt(text_crc) != nullptr)
            {
                if (upgrades_wide[0] != L'\0')
                {
                    wcsncat(upgrades_wide, L", ", 0x100);
                }
                wcsncat(upgrades_wide, localization_get_display_string_opt(text_crc), 0x100);
            }
        }
        vint_encode_wide_text(upgrades, 0x300, upgrades_wide);
        team = w_info->default_team;
    }
    // The carousel's position, counting the singularity bomb out when it is hidden.
    int position = Selected_index + 1;
    int total = Weapon_list.size();
    if (Singularity_bomb_index > -1)
    {
        total--;
        if (Selected_index > Singularity_bomb_index)
        {
            position = Selected_index;
        }
    }
    _snprintf_safe(count_text, 0x20, "%i / %i", position, total);
    item->set_element(0, vint_variant(display_name));
    item->set_element(1, vint_variant(ammo_text));
    item->set_element(2, vint_variant(upgrades));
    item->set_element(3, vint_variant((int)team));
    item->set_element(4, vint_variant(count_text));
    item->set_element(5, vint_variant(has_ammo));
    item->set_element(6, vint_variant(Ammo_box ? "MENU_GH_AMMO_BOX" : "TOOLTIP_LOCKER"));
}

// 0x0085AC80 (:795)
void hud_ui_weapon_cabinet_process_icon_data()
{
    vint_data_item* item = vint_data_item::find(Data_icon_handle);
    if (item == nullptr || Start_tween_down || Wait_for_tween)
    {
        return;
    }
    // Magic numbers: seven icons, three either side of the selection.
    for (int i = 0; i < 7; i++)
    {
        const int relative_offset = i - 3;
        const weapon_info* w_info = weapons_get_weapon_info_from_weapon_unique_id(hud_ui_weapon_cabinet_get_weapon(relative_offset, true).weapon_id);
        const char* icon = nullptr;
        bool unlocked = true;
        if (w_info != nullptr)
        {
            icon = w_info->icon_name;
            unlocked = hud_ui_weapon_cabinet_get_weapon(relative_offset, true).unlocked;
        }
        item->set_element(i * 2, vint_variant(icon));
        item->set_element(i * 2 + 1, vint_variant(unlocked));
    }
    Updated_carousel = true;
}

// 0x0085AD80 (:829)
void hud_ui_weapon_cabinet_process_inventory_data()
{
    vint_data_item* item = vint_data_item::find(Data_inventory_handle);
    player* pp = player_get_local_player();
    if (item == nullptr || pp == nullptr)
    {
        return;
    }
    // Magic numbers: four slots, 32 characters each.
    char ammo[4][32];
    const char* icons[4] = {nullptr, nullptr, nullptr, nullptr};
    memset(ammo, 0, sizeof(ammo));
    inventory_item* entry = pp->inventory;
    while (entry != nullptr)
    {
        const weapon* wp = entry->weapon_handle != 0xFFFFFFFF ? World.object_from_handle<weapon>(entry->weapon_handle) : nullptr;
        if (wp != nullptr && wp->w_info != nullptr && (unsigned int)entry->selection_slot < 4)
        {
            const unsigned int slot = (unsigned int)entry->selection_slot;
            int max_ammo;
            int user_ammo;
            int base_ammo;
            int morale_ammo;
            int final_ammo = 0;
            weapon_cabinet_get_ammo_box_ammo(wp->w_info, max_ammo, user_ammo, base_ammo, morale_ammo, final_ammo);
            if (final_ammo < 0)
            {
                _snprintf_safe(ammo[slot], 0x20, "");
            }
            else
            {
                _snprintf_safe(ammo[slot], 0x20, "%i", final_ammo);
            }
            icons[slot] = wp->w_info->icon_name;
        }
        entry = entry->next;
        if (entry == pp->inventory)
        {
            break;
        }
    }
    for (int i = 0; i < 4; i++)
    {
        item->set_element(i * 2, vint_variant(ammo[i]));
        item->set_element(i * 2 + 1, vint_variant(icons[i]));
    }
    item->set_element(8, vint_variant(Selected_slot + 1));
}

// 0x0084B870 (:884)
void hud_ui_weapon_cabinet_process_animation_data()
{
    vint_data_item* item = vint_data_item::find(Data_anim_handle);
    if (item == nullptr)
    {
        return;
    }
    if (!Start_tween_down && !Start_tween_up)
    {
        item->set_element(0, vint_variant(false));
        item->set_element(1, vint_variant(false));
        return;
    }
    item->set_element(0, vint_variant(true));
    item->set_element(1, vint_variant(Start_tween_down));
    Start_tween_down = false;
    Start_tween_up = false;
    Wait_for_tween = true;
}

// 0x0084B960 (:904)
void hud_ui_weapon_cabinet_proccess_hints()
{
    vint_data_item* item = vint_data_item::find(Data_hints_handle_cabinet);
    if (item == nullptr)
    {
        return;
    }
    char button[32];
    char image[128];
    // Magic number: BC_ABORT + 13, the weapon menu button.
    const button_choices back_button = (button_choices)(controls_is_using_weapon_menu_button() ? BC_ABORT + 13 : BC_ABORT);
    controls_get_button_image_name(button, 0x20, ui_get_control_button_value(back_button), Player_controller_index);
    keen::formatString(image, 0x80, "[image:%s]", button);
    item->set_element(0, vint_variant(image));
    item->set_element(1, vint_variant(cBack_cabinet));
    static const button_choices slots[4] = {BC_SLOT0, BC_SLOT1, BC_SLOT2, BC_SLOT3};
    for (int i = 0; i < 4; i++)
    {
        controls_get_button_image_name(button, 0x20, ui_get_control_button_value(slots[i]), Player_controller_index);
        keen::formatString(image, 0x80, "[image:%s]", button);
        item->set_element(2 + i, vint_variant(image));
    }
}

// 0x0085AF80 (:941)
void hud_ui_weapon_cabinet_process_info()
{
    hud_ui_weapon_cabinet_process_weapon_info_data();
    hud_ui_weapon_cabinet_process_icon_data();
    hud_ui_weapon_cabinet_process_inventory_data();
    hud_ui_weapon_cabinet_process_animation_data();
    hud_ui_weapon_cabinet_proccess_hints();
}

// 0x0087FEC0 (:957)
void hud_ui_weapon_cabinet_init()
{
    state_info info;
    info.enter = hud_ui_weapon_cabinet_enter;
    info.exit = hud_ui_weapon_cabinet_exit;
    info.process = hud_ui_weapon_cabinet_process;
    info.handshake = hud_ui_weapon_cabinet_handshake;
    info.render = hud_ui_weapon_cabinet_render;
    info.transparent = true;
    info.pause_beneath = true;
    gameseq_register_state(GS_INGAME_WEAPON_CABINET, info);
}

// 0x0087CC90 (:971)
void hud_ui_weapon_cabinet_enter(game_state prev_state)
{
    (void)prev_state;
    if (Weapon_list.size() == 0)
    {
        Doc_handle_cabinet = -1;
        ui_add_primary_message(L"HM_NO_WEAPONS", 3.0f, nullptr, nullptr, nullptr); // Magic number: 3 seconds.
        gameseq_pop_state();
        return;
    }
    Exit_cabinet = false;
    Loaded_cabinet = false;
    Start_tween_up = false;
    Start_tween_down = false;
    Wait_for_tween = false;
    Updated_carousel = false;
    Singularity_bomb_index = -1;
    Selected_index = 0;
    Selected_slot = -1;
    Input_id_cabinet = -1;
    ui_set_screen_blur(true, false);
    Doc_handle_cabinet = ui_load_document(cDocname_cabinet, DFIT_ALPHA, 0.5f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (Doc_handle_cabinet == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        gameseq_pop_state();
        return;
    }
    hud_ui_init_audio_info_cabinet();
    hud_ui_unload_items();
    Control_id_cabinet = ui_request_control_id();
    ui_wait_for_button_ups();
    hud_ui_upgrades_update_list();
    hud_ui_weapon_cabinet_add_inventory_items();
    if (!Player_had_singularity_bomb)
    {
        for (int i = 0; i < Weapon_list.size(); i++)
        {
            const weapon_info* w_info = weapons_get_weapon_info_from_weapon_unique_id(Weapon_list[i].weapon_id);
            if (w_info != nullptr && w_info->weapon_class == WEAPON_CLASS_SINGULARITY_BOMB)
            {
                Singularity_bomb_index = i;
                break;
            }
        }
    }
}

// 0x0085AFA0 (:1037)
void hud_ui_weapon_cabinet_exit(game_state new_state)
{
    (void)new_state;
    ui_unload_document(Doc_handle_cabinet, DFOT_NONE, 0.0f);
    ui_relinquish_control_id(Control_id_cabinet);
    ui_relinquish_input_id(Input_id_cabinet);
    ui_set_screen_blur(false, false);
    if (!hud_ui_is_loaded())
    {
        hud_ui_reload_items();
    }
    if (Loaded_cabinet)
    {
        player* pp = player_get_local_player();
        if (pp != nullptr)
        {
            // The reload sound of the equipped weapon, else the last equipped one, else the first that uses ammo.
            bool played = false;
            const weapon* equipped = pp->get_equipped_weapon();
            if (equipped == nullptr || !equipped->uses_ammo() || equipped->info == nullptr)
            {
                const inventory_item* last = pp->last_equipped_weapon;
                equipped = last != nullptr && last->weapon_handle != 0xFFFFFFFF ? World.object_from_handle<weapon>(last->weapon_handle) : nullptr;
                if (equipped != nullptr && (!equipped->uses_ammo() || equipped->info == nullptr))
                {
                    equipped = nullptr;
                }
            }
            if (equipped != nullptr)
            {
                audiolib_cue_play(equipped->info->pickup_snd_2, nullptr, nullptr);
                played = true;
            }
            inventory_item* entry = pp->inventory;
            while (entry != nullptr)
            {
                weapon* wp = entry->weapon_handle != 0xFFFFFFFF ? World.object_from_handle<weapon>(entry->weapon_handle) : nullptr;
                if (wp != nullptr && wp->uses_ammo() && wp->info != nullptr)
                {
                    if (!played)
                    {
                        audiolib_cue_play(wp->info->pickup_snd_2, nullptr, nullptr);
                        played = true;
                    }
                    int max_ammo;
                    int user_ammo;
                    int base_ammo;
                    int morale_ammo;
                    int final_ammo = 0;
                    weapon_cabinet_get_ammo_box_ammo(wp->w_info, max_ammo, user_ammo, base_ammo, morale_ammo, final_ammo);
                    wp->set_rounds_in_magazine(0);
                    wp->set_rounds_in_reserve(0);
                    wp->add_ammo(final_ammo);
                }
                entry = entry->next;
                if (entry == pp->inventory)
                {
                    break;
                }
            }
        }
        ovl_inventory_open(CBA_BUTTON_INVALID, 3.0f); // Magic number: 3 seconds.
        ovl_inventory_update_avail_weapons();
    }
    ui_tool_tips_remove_tip(Ammo_box ? "Ammo_Crate_Weapons" : "Locker");
    if (Ammo_box)
    {
        ui_tool_tips_add_tip("Ammo_Boxes", 0, nullptr);
    }
    else
    {
        ui_tool_tips_add_tip("Locker_Ammo", 0, nullptr);
        ui_add_secondary_message(localization_get_display_string_req("HM_FULL_AMMO", nullptr), -1.0f, false, false);
    }
    gamelog_maybe_record_loadout_change(player_get_local_player(), (gamelog_loadout_change_types)((Ammo_box ? 0 : 1) + GLLCT_AMMO_BOX));
}

// 0x0084BBD0 (:1161)
void hud_ui_weapon_cabinet_handshake(rl_renderer*)
{
}

// 0x0085B240 (:1167)
void hud_ui_weapon_cabinet_process()
{
    const bool msgbox_active = ui_is_msgbox_active(-2); // Magic number: -2, any message box.
    if (!msgbox_active)
    {
        if (Exit_cabinet)
        {
            if (hud_ui_is_loaded())
            {
                gameseq_pop_state();
            }
        }
        else
        {
            if (!Loaded_cabinet)
            {
                switch (ui_get_doc_asset_state(Doc_handle_cabinet))
                {
                case ST_UNLOADED:
                    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                    ui_unload_document(Doc_handle_cabinet, DFOT_NONE, 0.5f);
                    Doc_handle_cabinet = -1;
                    gameseq_pop_state();
                    break;
                case ST_ERROR:
                    ui_unload_document(Doc_handle_cabinet, DFOT_NONE, 0.5f);
                    Doc_handle_cabinet = -1;
                    gameseq_pop_state();
                    break;
                case ST_LOADING:
                    break;
                case ST_LOADED:
                    Loaded_cabinet = true;
                    Input_id_cabinet = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
                    hud_ui_weapon_cabinet_create_data_items();
                    ui_tool_tips_add_tip(Ammo_box ? "Ammo_Crate_Weapons" : "Locker", 0, nullptr);
                    break;
                default:
                    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                    break;
                }
            }
            if (Loaded_cabinet)
            {
                hud_ui_weapon_cabinet_process_input();
                hud_ui_weapon_cabinet_process_info();
                ui_add_for_processing(Doc_handle_cabinet);
            }
        }
    }
    ui_process(Control_id_cabinet, msgbox_active);
}

// 0x0084BBE0 (:1232)
void hud_ui_weapon_cabinet_render(rl_renderer*)
{
    if (Loaded_cabinet)
    {
        ui_add_for_rendering(Doc_handle_cabinet);
    }
    ui_render(Control_id_cabinet, false);
}

// 0x0087FF10 (:1242)
void hud_ui_weapon_cabinet_level_init_pre()
{
    Weapon_list.clear();
    notify_info info;
    info.data = nullptr;
    info.callback = hud_ui_weapon_cabinet_pick_up_cb;
    info.type = EVENT_WEAPON_ACQUIRED;
    gem_register_for_event(info);
}

// 0x0084BC10 (:1253)
void hud_ui_weapon_cabinet_tween_end_cb()
{
    Wait_for_tween = false;
}

// 0x008696A0 (:1259)
bool hud_ui_weapon_cabinet_show(bool from_ammo_box)
{
    if (game_is_paused() || gameseq_get_pending_state() != GS_NONE)
    {
        return false;
    }
    Ammo_box = from_ammo_box;
    Player_ammo.clear();
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        inventory_item* entry = pp->inventory;
        while (entry != nullptr)
        {
            const weapon* wp = entry->weapon_handle != 0xFFFFFFFF ? World.object_from_handle<weapon>(entry->weapon_handle) : nullptr;
            if (wp != nullptr && wp->w_info != nullptr && wp->uses_ammo())
            {
                user_ammo_data data;
                data.weapon_id = wp->w_info->unique_id;
                data.ammo = wp->get_rounds_in_reserve() + wp->get_rounds_in_magazine();
                Player_ammo.add(data);
            }
            entry = entry->next;
            if (entry == pp->inventory)
            {
                break;
            }
        }
    }
    gameseq_push_state(GS_INGAME_WEAPON_CABINET, true, false);
    return true;
}

// 0x0087CDE0 (:1290)
event_result hud_ui_weapon_cabinet_pick_up_cb(const event_data& in_event, notify_info* info)
{
    (void)info;
    if (in_event.get_type() != EVENT_WEAPON_ACQUIRED)
    {
        return EVENT_IGNORED;
    }
    const weapon_acquired_event& acquired = (const weapon_acquired_event&)in_event;
    if (!hud_ui_weapon_cabinet_unlock_weapon(weapons_get_weapon_info(acquired.w_handle), true))
    {
        hud_ui_weapon_cabinet_add_weapon(weapons_get_weapon_info(acquired.w_handle), true);
    }
    return EVENT_OK;
}

// 0x00877200 (:1303)
void hud_ui_weapon_cabinet_add_weapon(const weapon_info* w_info, bool unlocked)
{
    if (w_info == nullptr || w_info->weapon_class == WEAPON_CLASS_SLEDGEHAMMER || w_info->flags.non_inventory)
    {
        return;
    }
    if (w_info->name != nullptr)
    {
        hud_ui_handbook_unlock_entry(crc_stri(w_info->name, 0));
    }
    ui_weapon_list_item item;
    item.weapon_id = w_info->unique_id;
    item.unlocked = unlocked;
    Weapon_list.add_unique(item);
}

// 0x0085B3B0 (:1325)
bool hud_ui_weapon_cabinet_unlock_weapon(const weapon_info* w_info, bool unlocked)
{
    if (w_info == nullptr || w_info->weapon_class == WEAPON_CLASS_SLEDGEHAMMER || w_info->flags.non_inventory)
    {
        return false;
    }
    for (int i = 0; i < Weapon_list.size(); i++)
    {
        if (Weapon_list[i].weapon_id == (unsigned int)w_info->unique_id)
        {
            Weapon_list[i].unlocked = unlocked;
            if (w_info->name != nullptr)
            {
                hud_ui_handbook_unlock_entry(crc_stri(w_info->name, 0));
            }
            return true;
        }
    }
    return false;
}

// 0x0085B420 (:1354): on a new campaign, the unlocked weapons again.
void hud_ui_weapon_cabinet_reset()
{
    Weapon_list.clear();
    player_meta_data* meta_data = player_get_meta_data();
    if (meta_data != nullptr)
    {
        meta_data->add_unlocked_weapons_to_weapons_cabinet();
    }
}

// 0x0085B440 (:1363)
void hud_ui_weapon_cabinet_save(cfile* save_file)
{
    if (save_file == nullptr)
    {
        return;
    }
    cf_write_uint8((unsigned char)Weapon_list.size(), save_file);
    for (int i = 0; i < Weapon_list.size(); i++)
    {
        cf_write_uint32(Weapon_list[i].weapon_id, save_file);
        cf_write_bool(Weapon_list[i].unlocked, save_file);
    }
}

// 0x00877260 (:1378)
// Original: a count over the array's size leaves the rest unread, and the save's section check then fails the load.
void hud_ui_weapon_cabinet_load(cfile* save_file)
{
    if (save_file == nullptr)
    {
        return;
    }
    const unsigned int count = cf_read_uint8(save_file);
    if (count > 64)
    {
        return;
    }
    for (unsigned int i = 0; i < count; i++)
    {
        ui_weapon_list_item item;
        item.weapon_id = cf_read_uint32(save_file);
        item.unlocked = cf_read_bool(save_file);
        Weapon_list.add_unique(item);
    }
}

// 0x0085B4A0 (:1400)
bool hud_ui_weapon_cabinet_is_weapon_purchased(unsigned int weapon_unique_id)
{
    for (int i = 0; i < Weapon_list.size(); i++)
    {
        if (Weapon_list[i].weapon_id == weapon_unique_id && Weapon_list[i].unlocked)
        {
            return true;
        }
    }
    return false;
}
