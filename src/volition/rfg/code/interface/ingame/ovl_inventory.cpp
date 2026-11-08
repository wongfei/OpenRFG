// Original: volition/rfg/code/interface/ingame/ovl_inventory.cpp
//
// Recovered whole: the weapon selection overlay. vint-free: the radial is drawn by the HUD from Weapon_set
// (ui_document_hud_weapons.cpp, not ported); render is empty in rfg.exe.
//
// Modes (ovl_inventory_open): the weapon menu button opens it to stay open while held (a short press switches to the
// previous weapon, the quick switch); a weapon menu direction (1-4 on the keyboard) or the cycle axis switches at once
// (ImmediateSwitch); CBA_BUTTON_INVALID only shows it (Display, after a pickup). Selection slot by direction: left 0,
// up 1, right 2, down 3.

#include "volition/rfg/code/interface/ingame/ovl_inventory.h"
#include "volition/rfg/code/interface/ingame/overlay.h"

#include "volition/rfg/code/control/control_gameplay.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/interface/ui_document_hud_weapons.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_camera.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/time/timestamp.h"

#include <math.h>

enum InventoryMode
{
    Invalid = 0,
    Display = 1,
    WeaponMenuButton = 2,
    ImmediateSwitch = 3,
};

// control_gameplay.cpp's CGA_WEAPON_MENU (player::controls_general_allowed bit 0x1).
static const unsigned int CGA_WEAPON_MENU = 0x1;
// Magic number: the cycle axis counts beyond this deflection.
static const float Cycle_axis_threshold = 0.3f;

static ovl_inventory Ovl_inventory; // 0x01665D74
int ovl_inventory::refcount; // 0x02C034A8
static InventoryMode Current_mode = Invalid; // 0x02C00094
static bool Inventory_open; // 0x02C034A3, the radial is shown (also while fading out)
static int Selected_weapon_index; // 0x02C034AC, the selection slot of the equipped weapon (-1 none)
static int Previous_weapon_index; // 0x02C034B0, the quick switch's slot
static int New_selected_weapon_index; // 0x02C034B4, the slot enter took from an ImmediateSwitch direction
static bool Aborted; // 0x02C034B8, written only
static bool Disabled; // 0x02C034B9
static bool Quick_switch; // 0x02C034BA
static bool Remain_open; // 0x02C034BB
static bool Disable_quick_switch; // 0x02C034BD
static int Amount_to_cycle; // 0x02C034C0, the cycle axis direction enter saw (ImmediateSwitch)
static timestamp Inventory_open_ts; // 0x01665D78, a release of the weapon menu button before it is a quick switch
static timestamp Cycle_weapon_ts; // 0x01665D7C
static timestamp Update_hud_timestamp; // 0x01665D80, written only
static float Fade_time; // 0x02C00E08
static float Fade_amount; // 0x02C00DBC, written only
// Magic number: fades in and out in a second.
static float Fade_rate = 1.0f; // 0x016686A0
// Initialised true in .data.
static bool Update_available_flag = true; // 0x01662CD3
static inventory_item* Available_weapons[4]; // 0x02BFE070, by selection slot

// A weapon object in a selection slot 0-3.
static bool is_radial_weapon(const inventory_item* iip)
{
    if (iip->weapon_handle == 0xFFFFFFFF)
    {
        return false;
    }
    object* objp = World.object_from_handle(iip->weapon_handle);
    return objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON && iip->selection_slot >= 0 &&
           iip->selection_slot <= 3;
}

// 0x008AA800
// Always true. Does not check the local player (the callers have).
static bool available_inventory_radial()
{
    player* pp = player_get_local_player();
    for (inventory_item*& slot : Available_weapons)
    {
        slot = nullptr;
    }
    inventory_item* iip = pp->inventory;
    do
    {
        if (iip == nullptr)
        {
            return true;
        }
        if (is_radial_weapon(iip))
        {
            Available_weapons[iip->selection_slot] = iip;
        }
        iip = iip->next;
    } while (iip != pp->inventory);
    return true;
}

// 0x008AA870
// The slots of the equipped (in a vehicle: the last equipped) and the previous weapon. A non-inventory weapon (a
// carried object) selects no slot.
static void ovl_inventory_update_weapon_indices(player* pp)
{
    if (pp == nullptr)
    {
        return;
    }
    inventory_item* current = human_in_vehicle(pp) == nullptr ? pp->get_equipped_inv_item() : pp->get_last_equipped_weapon();
    if (current == nullptr)
    {
        current = pp->get_desired_inv_item();
        if (current == nullptr && pp->inventory != nullptr)
        {
            // the first weapon in a selection slot
            inventory_item* iip = pp->inventory;
            do
            {
                if (is_radial_weapon(iip))
                {
                    current = iip;
                    break;
                }
                iip = iip->next;
            } while (iip != pp->inventory && iip != nullptr);
        }
    }
    inventory_item* previous = human_in_vehicle(pp) == nullptr ? pp->get_last_equipped_weapon() : pp->second_last_equipped_weapon;
    for (int i = 0; i < 4; i++)
    {
        if (current != nullptr && current == Available_weapons[i])
        {
            Selected_weapon_index = i;
        }
        if (previous != nullptr && previous == Available_weapons[i])
        {
            Previous_weapon_index = i;
        }
    }
    if (current != nullptr && current->info != nullptr && current->info->w_info != nullptr && current->info->w_info->flags.non_inventory)
    {
        Selected_weapon_index = -1;
    }
}

// 0x008AA9E0
static void update_radial()
{
    available_inventory_radial();
    ovl_inventory_update_weapon_indices(player_get_local_player());
}

// 0x00888180
ovl_inventory::ovl_inventory()
{
    refcount++;
}

// 0x00888190
ovl_inventory::~ovl_inventory()
{
    refcount--;
}

// 0x008AAA00
void ovl_inventory::enter() const
{
    player* pp = player_get_local_player();
    Aborted = false;
    Update_available_flag = false;
    New_selected_weapon_index = -1;
    if (Current_mode == Display)
    {
        Remain_open = false;
        Quick_switch = false;
    }
    else if (Current_mode == WeaponMenuButton)
    {
        Remain_open = true;
        Quick_switch = false;
    }
    else if (Current_mode == ImmediateSwitch)
    {
        Quick_switch = false;
        Remain_open = false;
        const float cycle_axis = controls_get_analog(CAA_CYCLE_WEAPONS, Player_controller_index);
        if (fabsf(cycle_axis) <= Cycle_axis_threshold)
        {
            if (controls_is_button_just_pressed(CBA_WEAPON_MENU_DOWN, Player_controller_index))
            {
                New_selected_weapon_index = 3;
            }
            else if (controls_is_button_just_pressed(CBA_WEAPON_MENU_LEFT, Player_controller_index))
            {
                New_selected_weapon_index = 0;
            }
            else if (controls_is_button_just_pressed(CBA_WEAPON_MENU_UP, Player_controller_index))
            {
                New_selected_weapon_index = 1;
            }
            else if (controls_is_button_just_pressed(CBA_WEAPON_MENU_RIGHT, Player_controller_index))
            {
                New_selected_weapon_index = 2;
            }
        }
        else
        {
            Amount_to_cycle = cycle_axis <= 0.0f ? -1 : 1;
        }
    }
    if (pp == nullptr || pp->is_killed())
    {
        Aborted = true;
        return;
    }
    if (available_inventory_radial())
    {
        if (player_is_zoomed(pp))
        {
            player_set_zoom_state(pp, PZS_OFF, false);
        }
        Selected_weapon_index = 0;
        Previous_weapon_index = 0;
        ovl_inventory_update_weapon_indices(pp);
        Inventory_open = true;
        // Magic number: a release of the weapon menu button within 500 ms is a quick switch.
        Inventory_open_ts.set(500);
    }
}

// 0x008AABB0
void ovl_inventory::exit() const
{
    Inventory_open = false;
    Quick_switch = false;
    Remain_open = false;
    Weapon_set.m_show_weapons = false;
    Current_mode = Invalid;
}

// 0x00888220
// The next (or previous) occupied slot after the selected one, or -1 (none, or within 50 ms of the last cycle).
int ovl_inventory::cycle(bool positive) const
{
    Amount_to_cycle = 0;
    if (Cycle_weapon_ts.valid() && !Cycle_weapon_ts.elapsed())
    {
        return -1;
    }
    int slot = Selected_weapon_index;
    int empty_slots = 0;
    for (;;)
    {
        slot += positive ? 1 : -1;
        if (slot < 0)
        {
            slot = 3;
        }
        else if (slot > 3)
        {
            slot = 0;
        }
        if (Available_weapons[slot] != nullptr)
        {
            break;
        }
        // Magic number: gives up after 3 empty slots.
        if (++empty_slots > 2)
        {
            return -1;
        }
    }
    // Magic number: 50 ms between cycles.
    Cycle_weapon_ts.set(50);
    return slot;
}

// 0x008DDC20
// Selects a slot (ImmediateSwitch: the direction enter saw or the cycle; open: a just pressed direction or the cycle
// axis) and equips its weapon; a release of the weapon menu button soon after it opened is a quick switch to the
// previous weapon. Fades the radial in while open and out after; returns false (closed) once faded out.
bool ovl_inventory::process() const
{
    if (Disabled)
    {
        ovl_inventory_close();
        return false;
    }
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return false;
    }
    if (Update_available_flag)
    {
        update_radial();
        Update_available_flag = false;
    }
    const bool controls_allowed = Inventory_open && control_gameplay_general_controls_allowed() && (!Multiplayer || !multi_game_camera_is_running()) &&
                                  !pp->is_killed() && (pp->controls_general_allowed & CGA_WEAPON_MENU) != 0;

    int new_index = -1;
    bool from_input = true;
    if (Current_mode == ImmediateSwitch)
    {
        if (Amount_to_cycle != 0)
        {
            new_index = cycle(Amount_to_cycle > 0);
            Amount_to_cycle = 0;
            from_input = false;
        }
        else if (New_selected_weapon_index != -1)
        {
            new_index = New_selected_weapon_index;
            New_selected_weapon_index = -1;
            from_input = false;
        }
    }
    if (from_input && controls_allowed)
    {
        const float cycle_axis = controls_get_analog(CAA_CYCLE_WEAPONS, Player_controller_index);
        if (fabsf(cycle_axis) > Cycle_axis_threshold)
        {
            new_index = cycle(0.0f < cycle_axis);
            Disable_quick_switch = true;
        }
        else if (Remain_open || Current_mode == ImmediateSwitch)
        {
            if (controls_is_button_just_pressed(CBA_WEAPON_MENU_UP, Player_controller_index) || controls_is_button_just_pressed(CBA_WEAPON_MENU_UP_DPAD, Player_controller_index))
            {
                new_index = 1;
            }
            else if (controls_is_button_just_pressed(CBA_WEAPON_MENU_RIGHT, Player_controller_index) || controls_is_button_just_pressed(CBA_WEAPON_MENU_RIGHT_DPAD, Player_controller_index))
            {
                new_index = 2;
            }
            else if (controls_is_button_just_pressed(CBA_WEAPON_MENU_DOWN, Player_controller_index) || controls_is_button_just_pressed(CBA_WEAPON_MENU_DOWN_DPAD, Player_controller_index))
            {
                new_index = 3;
            }
            else if (controls_is_button_just_pressed(CBA_WEAPON_MENU_LEFT, Player_controller_index) || controls_is_button_just_pressed(CBA_WEAPON_MENU_LEFT_DPAD, Player_controller_index))
            {
                new_index = 0;
            }
            if (new_index >= 0)
            {
                Disable_quick_switch = true;
            }
        }
    }
    // rfg.exe reaches this through two labels; every path that selects from input sets Disable_quick_switch first.
    if (Disable_quick_switch)
    {
        Inventory_open_ts.invalidate();
    }

    // the directions belong to the radial while the weapon menu button is held
    if (controls_allowed && controls_is_button_down(CBA_GAC_WEAPON_MENU, Player_controller_index))
    {
        controls_mask_button(CBA_WEAPON_MENU_UP, Player_controller_index);
        controls_mask_button(CBA_WEAPON_MENU_RIGHT, Player_controller_index);
        controls_mask_button(CBA_WEAPON_MENU_DOWN, Player_controller_index);
        controls_mask_button(CBA_WEAPON_MENU_LEFT, Player_controller_index);
        controls_mask_button(CBA_WEAPON_MENU_UP_DPAD, Player_controller_index);
        controls_mask_button(CBA_WEAPON_MENU_RIGHT_DPAD, Player_controller_index);
        controls_mask_button(CBA_WEAPON_MENU_DOWN_DPAD, Player_controller_index);
        controls_mask_button(CBA_WEAPON_MENU_LEFT_DPAD, Player_controller_index);
    }

    if (new_index >= 0 && new_index != Selected_weapon_index)
    {
        inventory_item* iip = Available_weapons[new_index];
        if (iip != nullptr && player_equip_item(player_get_local_player(), iip, nullptr, false))
        {
            Inventory_open_ts.invalidate();
            if (Selected_weapon_index == -1)
            {
                update_radial();
            }
            else
            {
                Previous_weapon_index = Selected_weapon_index;
            }
            Selected_weapon_index = new_index;
        }
        Fade_time = 1.0f;
        Disable_quick_switch = true;
        Remain_open = true;
        controls_mask_button(CBA_GAC_WEAPON_MENU, Player_controller_index);
        // Magic number: 200 ms.
        Update_hud_timestamp.set(200);
    }

    if (controls_allowed && controls_is_button_down(CBA_GAC_WEAPON_MENU, Player_controller_index))
    {
        controls_mask_button(CBA_GAC_WEAPON_MENU, Player_controller_index);
        if (!Remain_open && !Disable_quick_switch)
        {
            Inventory_open_ts.set(500);
        }
        Aborted = false;
        Quick_switch = false;
        Fade_time = 1.0f;
        Remain_open = available_inventory_radial();
    }
    else
    {
        Remain_open = false;
        Aborted = false;
        if (!Disable_quick_switch)
        {
            // the weapon menu button released within Inventory_open_ts: the quick switch
            if (!Inventory_open_ts.elapsed() && Inventory_open_ts.valid() && !Quick_switch && (!Multiplayer || multi_gameplay_mode_player_can_swap_weapons(pp)))
            {
                inventory_item* iip = Available_weapons[Previous_weapon_index];
                Quick_switch = true;
                if (iip == nullptr)
                {
                    const inv_item_info* item_info = pp->h_info->get_default_last_equip_item();
                    iip = human_has_item(pp, item_info);
                    if (iip == nullptr && item_info != nullptr && item_info->w_info != nullptr)
                    {
                        iip = human_has_weapon_of_type(pp, item_info->w_info->weapon_class);
                    }
                }
                if (iip != nullptr && iip->info != nullptr && pp->get_equipped_inv_item() != iip && player_equip_item(pp, iip, nullptr, false))
                {
                    // Original quirk kept: the previous slot is selected even when the default weapon was equipped.
                    const int previous = Previous_weapon_index;
                    if (Selected_weapon_index == -1)
                    {
                        update_radial();
                    }
                    else
                    {
                        Previous_weapon_index = Selected_weapon_index;
                    }
                    Selected_weapon_index = previous;
                }
                Disable_quick_switch = true;
            }
            // rfg.exe tests Remain_open again here (to refresh the radial); it was just cleared.
        }
    }

    // the fade
    if (pp->is_killed())
    {
        Aborted = true;
        Remain_open = false;
    }
    if (Remain_open)
    {
        Fade_time = Frametime * Fade_rate + Fade_time;
        Inventory_open = true;
        if (1.0f - Fade_time < 0.0f)
        {
            Fade_time = 1.0f;
        }
    }
    else
    {
        Fade_time = Fade_time - Frametime * Fade_rate;
        if (!(0.0f - Fade_time < 0.0f))
        {
            Fade_time = 0.0f;
        }
        Inventory_open = 0.0f < Fade_time;
    }
    Fade_amount = (Fade_time - 0.5f) * 2.0f;
    if (0.0f <= 0.0f - Fade_amount)
    {
        Fade_amount = 0.0f;
    }

    Weapon_set.m_weapon_slot = Selected_weapon_index;
    Weapon_set.m_prev_weapon_slot = Available_weapons[Previous_weapon_index] != nullptr ? Previous_weapon_index : Selected_weapon_index;
    Weapon_set.m_quick_switch = Quick_switch;
    Weapon_set.m_show_weapons = 0.0f < Fade_time && !wrecking_crew_is_active() && !multi_game_camera_is_running() && !hud_ui_multiplayer_splash_screen_is_active();
    return 0.0f < Fade_time;
}

// 0x00888210
void ovl_inventory::render() const
{
}

// 0x008881E0
void parse_inventory_data(xml_element*, const char*)
{
}

// 0x008881F0
void ovl_inventory_system_init()
{
    Update_hud_timestamp.set(0);
}

// 0x008BF570
void ovl_inventory_close()
{
    Aborted = true;
    overlay_close(&Ovl_inventory);
}

// 0x00888200
void ovl_inventory_update_avail_weapons()
{
    Update_available_flag = true;
}

// 0x008DDB80
bool ovl_inventory_open(control_button_action cba, float duration)
{
    Disable_quick_switch = false;
    if (Disabled || wrecking_crew_is_active() || multi_game_camera_is_running() || hud_ui_multiplayer_splash_screen_is_active())
    {
        return false;
    }
    if (cba == CBA_GAC_WEAPON_MENU)
    {
        Current_mode = WeaponMenuButton;
        Remain_open = true;
    }
    else
    {
        Current_mode = cba == CBA_BUTTON_INVALID ? Display : ImmediateSwitch;
        Disable_quick_switch = true;
        Remain_open = false;
    }
    Fade_time = duration;
    Fade_amount = 0.0f;
    return overlay_open(&Ovl_inventory);
}

// 0x008AABD0
bool ovl_inventory_is_open()
{
    return overlay_is_open(&Ovl_inventory);
}

// 0x008882A0
bool ovl_inventory_is_closing_or_closed()
{
    return !Remain_open;
}

// 0x008882B0
void ovl_inventory_disable(bool disable)
{
    Disabled = disable;
}

// 0x008882C0
bool ovl_inventory_is_disabled()
{
    return Disabled;
}
