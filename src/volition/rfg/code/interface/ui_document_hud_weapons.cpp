// Original: volition/rfg/code/interface/ui_document_hud_weapons.cpp
//
// The weapon documents (stub_impl.md S7): rfg_ui_hud_weapon_sel (the weapon slots and their select buttons) and
// rfg_ui_hud_reticles (the reticle, ammunition, heat, charges, the backpack line).

#include "volition/rfg/code/interface/ui_document_hud_weapons.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/camera/camera_first_person.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gameplay/activities/activity_convoy.h"
#include "volition/rfg/code/gameplay/gameplay.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/ingame/ovl_inventory.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_camera.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/string_util.h"

// ---- globals
weapon_info_set Weapon_set; // 0x02C4CB3C

// ---- file-static globals
static const char* cUI_Child_Hud_Weapon_Select = {"rfg_ui_hud_weapon_sel"}; // 0x01664AD0
static const char* cUI_Child_Hud_Reticles = {"rfg_ui_hud_reticles"}; // 0x01664AD4
static int Doc_handles_uiwpn[2] = {-1, -1}; // 0x01664AD8
static int Data_handles_uiwpn[6] = {-1, -1, -1, -1, -1, -1}; // 0x01664AE0
static timestamp Hit_indicator_delay; // 0x01667168, initial data ffffffff
static bool Assets_loaded_uiwpn; // 0x02C0806D
static bool Assets_only; // 0x02C0806E
static bool Data_items_created_uiwpn; // 0x02C0806F
static bool Active_uiwpn; // 0x02C0807C
static int Last_locking_status; // the heat seeking lock of the last frame (hud_ui_weapons_process_weapon_fire_info)

// 0x00898C70
weapon_info_set::weapon_info_set()
{
}

// 0x00898C80
static void ui_init_weapon_set()
{
    Weapon_set.m_weapon_slot = 0;
    Weapon_set.m_show_weapons = false;
    Weapon_set.m_show_reticules = true;
    Weapon_set.m_last_weapon_info = 0xFFFFFFFF;
    Weapon_set.m_current_weapon_info = 0xFFFFFFFF;
    Weapon_set.m_weapon_screen_pos.x = 0.0f;
    Weapon_set.m_weapon_screen_pos.y = 0.0f;
}

// 0x00898CC0
static vint_data_item* hud_ui_weapons_add_weapon_slot_data_items_info(int& data_handle, int& doc_handle)
{
    vint_data_item* item = vint_data_item::create_new("WeaponSlotInfo");
    doc_handle = 0;
    if (item != nullptr)
    {
        const vint_variant_type types[12] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 12);
        data_handle = item->get_handle();
    }
    return item;
}

// 0x00898D50
static vint_data_item* hud_ui_weapons_add_weapon_button_data_items_info(int& data_handle, int& doc_handle)
{
    vint_data_item* item = vint_data_item::create_new("WeaponButtonInfo");
    doc_handle = 0;
    if (item != nullptr)
    {
        const vint_variant_type types[4] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        item->set_template(types, 4);
        data_handle = item->get_handle();
    }
    return item;
}

// 0x00898DC0
static vint_data_item* hud_ui_weapons_add_weapon_show_data_items_info(int& data_handle, int& doc_handle)
{
    vint_data_item* item = vint_data_item::create_new("WeaponShowInfo");
    doc_handle = 0;
    if (item != nullptr)
    {
        // Original bug kept: 2 elements here, hud_ui_weapons_process_weapon_select_info sets 4.
        const vint_variant_type types[2] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT};
        item->set_template(types, 2);
        data_handle = item->get_handle();
    }
    return item;
}

// 0x00898E20
static vint_data_item* hud_ui_weapons_add_weapon_fire_data_items_info(int& data_handle, int& doc_handle)
{
    vint_data_item* item = vint_data_item::create_new("WeaponFire");
    doc_handle = 1;
    if (item != nullptr)
    {
        const vint_variant_type types[25] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING,
            VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL,
            VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT};
        item->set_template(types, 25);
        data_handle = item->get_handle();
    }
    return item;
}

// 0x00898EF0
static vint_data_item* hud_ui_weapons_add_weapon_posn_data_items_info(int& data_handle, int& doc_handle)
{
    vint_data_item* item = vint_data_item::create_new("WeaponPosn");
    doc_handle = 1;
    if (item != nullptr)
    {
        const vint_variant_type types[2] = {VINT_PROP_TYPE_FLOAT, VINT_PROP_TYPE_FLOAT};
        item->set_template(types, 2);
        data_handle = item->get_handle();
    }
    return item;
}

// 0x00898F40
static vint_data_item* hud_ui_weapons_add_safehouse_data_items_info(int& data_handle, int& doc_handle)
{
    vint_data_item* item = vint_data_item::create_new("SafehouseInfo");
    doc_handle = 1;
    if (item != nullptr)
    {
        vint_variant_type types[8] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
        int num_elements;
        if (Multiplayer)
        {
            // The backpack's name and hint (hud_ui_multiplayer_process_backpack_data_item).
            types[4] = VINT_PROP_TYPE_STRING;
            types[5] = VINT_PROP_TYPE_STRING;
            num_elements = 6;
        }
        else
        {
            types[4] = VINT_PROP_TYPE_VECTOR2F;
            types[5] = VINT_PROP_TYPE_VECTOR2F;
            types[6] = VINT_PROP_TYPE_INT;
            types[7] = VINT_PROP_TYPE_INT;
            num_elements = 8;
        }
        item->set_template(types, num_elements);
        data_handle = item->get_handle();
    }
    return item;
}

// 0x008B2BE0
static void hud_ui_weapons_process_weapon_slot_info(float dtime)
{
    if (Data_handles_uiwpn[0] == -1)
    {
        return;
    }
    vint_data_item* item = vint_data_item::find(Data_handles_uiwpn[0]);
    player* pp = item != nullptr ? player_get_local_player() : nullptr;
    if (pp == nullptr)
    {
        return;
    }
    int ammo[4] = {};
    const char* icons[4] = {};
    const char* names[4] = {};
    inventory_item* inv = pp->inventory;
    while (inv != nullptr)
    {
        if (inv->weapon_handle != 0xFFFFFFFF)
        {
            weapon* wp = (weapon*)World.object_from_handle(inv->weapon_handle);
            if (wp != nullptr && wp->get_obj_type() == OT_ITEM && wp->get_sub_type() == OT_SUB_ITEM_WEAPON && (unsigned int)inv->selection_slot < 4)
            {
                const unsigned int slot = inv->selection_slot;
                if (!wp->uses_ammo())
                {
                    // Magic number: -2 shows no ammunition at all.
                    ammo[slot] = -2;
                }
                else if (wp->has_unlimited_ammo() || wp->has_unlimited_magazine_ammo())
                {
                    // Magic number: -1 shows as unlimited.
                    ammo[slot] = -1;
                }
                else
                {
                    ammo[slot] = wp->get_rounds_in_reserve() + wp->get_rounds_in_magazine();
                }
                const weapon_info* w_info = wp->w_info;
                if (w_info != nullptr)
                {
                    icons[slot] = w_info->icon_name;
                    if (w_info->weapon_inv_item_info != nullptr)
                    {
                        names[slot] = w_info->weapon_inv_item_info->display_name;
                    }
                }
            }
        }
        inv = inv->next;
        if (inv == pp->inventory)
        {
            break;
        }
    }
    for (int slot = 0; slot < 4; slot++)
    {
        item->set_element(slot * 3, vint_variant(ammo[slot]));
        item->set_element(slot * 3 + 1, vint_variant(icons[slot]));
        item->set_element(slot * 3 + 2, vint_variant(names[slot]));
    }
}

// 0x00898FE0
static void hud_ui_weapons_process_weapon_select_buttons(float dtime)
{
    if (Data_handles_uiwpn[5] == -1)
    {
        return;
    }
    vint_data_item* item = vint_data_item::find(Data_handles_uiwpn[5]);
    if (item == nullptr)
    {
        return;
    }
    static const control_button_action buttons[4] = {CBA_WEAPON_MENU_DOWN, CBA_WEAPON_MENU_LEFT, CBA_WEAPON_MENU_UP, CBA_WEAPON_MENU_RIGHT};
    for (int i = 0; i < 4; i++)
    {
        char button[0x80];
        char image[0x80];
        if (!controls_get_button_image_name(button, 0x80, buttons[i], Player_controller_index))
        {
            button[0] = '\0';
        }
        keen::formatString(image, 0x80, "[image:%s]", button);
        item->set_element(i, vint_variant((const char*)image));
    }
}

// 0x008B2DF0
static void hud_ui_weapons_process_weapon_select_info(float dtime)
{
    if (Data_handles_uiwpn[1] == -1)
    {
        return;
    }
    vint_data_item* item = vint_data_item::find(Data_handles_uiwpn[1]);
    player* pp = item != nullptr ? player_get_local_player() : nullptr;
    if (pp == nullptr)
    {
        return;
    }
    bool show;
    if (((!controls_is_using_weapon_menu_button() || !controls_is_button_pressed(CBA_GAC_WEAPON_MENU, Player_controller_index)) && (controls_is_using_weapon_menu_button() || !ovl_inventory_is_open()) && !controls_button_is_masked(CBA_GAC_WEAPON_MENU, Player_controller_index)) || (pp->controls_general_allowed & 1) == 0)
    {
        show = false;
    }
    else
    {
        show = true;
    }
    if (Multiplayer && !multi_gameplay_mode_player_can_swap_weapons(pp))
    {
        show = false;
    }
    item->set_element(0, vint_variant(Weapon_set.m_show_weapons));
    item->set_element(1, vint_variant(Weapon_set.m_weapon_slot));
    item->set_element(2, vint_variant(Weapon_set.m_prev_weapon_slot));
    item->set_element(3, vint_variant(show));
}

// 0x008DA620
static void hud_ui_weapons_process_safehouse_info(float dtime)
{
    if (Data_handles_uiwpn[4] != -1)
    {
        vint_data_item* item = vint_data_item::find(Data_handles_uiwpn[4]);
        if (item != nullptr && Multiplayer)
        {
            hud_ui_multiplayer_process_backpack_data_item(item);
        }
    }
}

// 0x008B2F50: 0 nothing, 1 not an enemy, 2 an enemy.
int hud_ui_weapons_determine_object_targeted(const player* const pp)
{
    if (pp == nullptr || (pp->aim_target == 0xFFFFFFFF && pp->heat_seeking_target == 0xFFFFFFFF))
    {
        return 0;
    }
    const human_teams team = pp->current_team;
    unsigned int handle = pp->heat_seeking_target;
    if (!upgrade_has_heat_seeking_rockets() || handle == 0xFFFFFFFF)
    {
        handle = pp->aim_target;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr)
    {
        return 0;
    }
    if (objp->get_obj_type() == OT_HUMAN)
    {
        bool stealthed = false;
        if (objp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            multi_object_backpack* backpack = ((player*)objp)->backpack;
            if (backpack != nullptr && backpack->is_fully_stealthed())
            {
                stealthed = true;
            }
        }
        // Magic number: 3 is the first disposition that is not hostile.
        if ((int)human_teams_get_disposition((human*)objp, team) < 3 && !human_teams_are_allies((human*)objp, team))
        {
            return stealthed ? 0 : 2;
        }
    }
    else
    {
        if (objp->get_obj_type() != OT_VEHICLE)
        {
            return 0;
        }
        vehicle* vp = (vehicle*)objp;
        human* hp = vehicle_passenger_in_seat(vp, VSI_DRIVER);
        if (hp == nullptr)
        {
            hp = vehicle_get_human_in_gunner_seat(vp);
        }
        if (activity_convoy_vehicle_destroy(vp))
        {
            return 2;
        }
        if (hp == nullptr)
        {
            return 0;
        }
        if (!human_teams_are_friendly(hp, team) && !human_teams_are_allies(hp, team))
        {
            return 2;
        }
    }
    return 1;
}

// 0x008991F0 (LTCG: pp in ECX)
static void hud_ui_weapons_determine_weapon_selection(const player* const pp)
{
    Weapon_set.m_last_weapon_info = Weapon_set.m_current_weapon_info;
    Weapon_set.m_current_weapon_info = 0xFFFFFFFF;
    if (pp != nullptr)
    {
        weapon* wp = pp->get_equipped_weapon();
        if (wp != nullptr)
        {
            Weapon_set.m_current_weapon_info = weapons_get_weapon_info_handle(wp->w_info);
        }
    }
}

// 0x008B3090
static void hud_ui_weapons_process_weapon_fire_info(float dtime)
{
    if (Data_handles_uiwpn[2] == -1)
    {
        return;
    }
    vint_data_item* item = vint_data_item::find(Data_handles_uiwpn[2]);
    if (item == nullptr)
    {
        return;
    }
    // Original quirk kept: compared before this frame's selection is determined, so a change shows one frame later.
    const bool weapon_changed = Weapon_set.m_current_weapon_info != Weapon_set.m_last_weapon_info;
    int magazine = 0;
    int reserve = 0;
    int magazine_size = 0;
    int target = 0;
    int locking = 0;
    int charges = 0;
    int detonatable = 0;
    int mines = 0;
    int total_charges = 0;
    int free_charges = 0;
    float spread = 0.0f;
    float heat = 0.0f;
    bool low_ammo = false;
    bool reloading = false;
    bool overheated = false;
    bool can_fire = false;
    bool fine_aim = false;
    bool autoaim_turret = false;
    bool on_turret = false;
    bool secondary_can_fire = false;
    bool show_ammo = false;
    int rockets = -1; // Magic number: -1 hides the multi rocket count
    const char* weapon_name = nullptr;
    const char* icon_name = nullptr;
    const char* reticule_name = nullptr;
    player* pp = player_get_local_player();
    hud_ui_weapons_determine_weapon_selection(pp);

    weapon* wp = nullptr;
    if (pp != nullptr)
    {
        if (!Camera_third_person_lock)
        {
            projectile_count_charges_proxy_mines(pp->get_handle(), charges, detonatable, mines);
        }
        total_charges = charges;
        if (Multiplayer)
        {
            total_charges = mines + charges;
        }
        free_charges = total_charges - detonatable;
        wp = pp->get_equipped_weapon();
        if (wp == nullptr && pp->desired_equipped_inv_item != nullptr && pp->hflags.playing_equip_anim != 0)
        {
            wp = World.object_from_handle<weapon>(pp->desired_equipped_inv_item->weapon_handle);
        }
    }
    if (wp != nullptr)
    {
        magazine = wp->has_unlimited_magazine_ammo() ? -1 : wp->get_rounds_in_magazine();
        reserve = wp->has_unlimited_ammo() ? -1 : wp->get_rounds_in_reserve();
        magazine_size = wp->w_info != nullptr ? wp->get_magazine_size() : 0;
        const weapon_class_type weapon_class = wp->w_info->weapon_class;
        on_turret = wp->turret_handle != 0xFFFFFFFF;
        show_ammo = !(weapon_class == WEAPON_CLASS_SLEDGEHAMMER || weapon_class == WEAPON_CLASS_GUTTER || weapon_class == WEAPON_CLASS_MELEE || weapon_class == WEAPON_CLASS_REPAIR || (on_turret && wp->has_unlimited_magazine_ammo()));
        overheated = wp->is_overheated();
        heat = wp->get_overheat_percent() * 100.0f;
        if (upgrade_get_multi_rockets() > 1 && (wp->w_info->projectile_info.flags & 1) != 0)
        {
            rockets = wp->is_queueing_multi_rockets() ? wp->secondary_rockets_to_fire + 1 : 0;
        }
        if (wp->turret_handle != 0xFFFFFFFF)
        {
            object* turret_obj = World.object_from_handle(wp->turret_handle);
            if (turret_obj != nullptr && turret_obj->get_obj_type() == OT_TURRET)
            {
                turret* tp = (turret*)turret_obj;
                autoaim_turret = tp->should_autoaim();
                vehicle* vp = tp->get_vehicle();
                if (vp != nullptr && vp->info != nullptr && vp->info->flags.is_walker_flamer)
                {
                    autoaim_turret = false;
                }
                weapon* secondary = tp->get_secondary_weapon_inst();
                if (secondary != nullptr)
                {
                    const bool out_of_ammo = secondary->get_rounds_in_reserve() + secondary->get_rounds_in_magazine() < 1 && !secondary->has_unlimited_magazine_ammo() && !secondary->has_unlimited_ammo();
                    const bool cant_fire = !secondary->can_fire() && (secondary->w_info->trigger_type != WEAPON_TRIGGER_TYPE_AUTOMATIC || !secondary->is_trigger_pulled());
                    secondary_can_fire = !out_of_ammo && !cant_fire && !secondary->is_overheated() && pp->pflags.no_reticule == 0;
                    if (wp->w_info->flags.use_secondary_weapon_heat)
                    {
                        overheated = secondary->is_overheated();
                        heat = secondary->get_overheat_percent() * 100.0f;
                        can_fire = wp->can_fire();
                    }
                }
            }
        }
        if (Weapon_set.m_current_weapon_info != 0xFFFFFFFF)
        {
            const weapon_info* selected = weapons_get_weapon_info(Weapon_set.m_current_weapon_info);
            if (selected != nullptr && !autoaim_turret && !Camera_third_person_lock && !human_is_dead(pp))
            {
                // Magic number: in a DLC bundle the thrown charge has its own reticle.
                weapon_name = World.dlc_bundle_id != -1 && selected->weapon_class == WEAPON_CLASS_THROWN_CHARGE ? "charge_placer" : selected->name;
                icon_name = selected->icon_name;
                reticule_name = selected->reticule_name;
            }
        }
        // Magic number: a quarter of the magazine is low.
        low_ammo = !wp->has_unlimited_magazine_ammo() && (double)magazine / (double)magazine_size < 0.25;
        reloading = wp->is_reloading();
    }
    if (wp != nullptr && wp->w_info != nullptr)
    {
        const int total_rounds = magazine + reserve;
        const int reserve_rounds = reserve;
        bool skip_target = false;
        const bool out_of_ammo = total_rounds < 1 && !wp->has_unlimited_magazine_ammo() && !wp->has_unlimited_ammo();
        const bool cant_fire = !wp->can_fire() && (wp->w_info->trigger_type != WEAPON_TRIGGER_TYPE_AUTOMATIC || !wp->is_trigger_pulled());
        can_fire = !out_of_ammo && !wp->is_overheated() && !cant_fire && pp->pflags.no_reticule == 0 && pp->sprint_delay_timestamp.elapsed();
        if (!gameplay_can_player_attack(nullptr, DBT_NONE))
        {
            can_fire = false;
        }
        if (wp->w_info->ammo_type == WEAPON_AMMO_TYPE_THROWN && (magazine == -1 || reserve_rounds != -1))
        {
            if (magazine != -1)
            {
                magazine = total_rounds;
            }
            if (reserve_rounds != -1 && (wp->w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE || wp->w_info->weapon_class == WEAPON_CLASS_THROWN_MINE))
            {
                reserve = upgrade_get_max_charges();
            }
        }
        if (wp->w_info->weapon_class == WEAPON_CLASS_GRINDER)
        {
            spread = wp->get_grinder_speed_percent() * 100.0f;
            if (player_is_scoped(pp) || player_is_fine_aimed(pp))
            {
                reticule_name = wp->w_info->fine_aim_reticule_name;
                fine_aim = true;
            }
        }
        else
        {
            spread = 100.0f;
            if (wp->w_info->flags.use_secondary_weapon_heat)
            {
                double fired = 0.0;
                if (wp->time_since_fired() != -1)
                {
                    fired = (double)((((float)wp->time_since_fired()) / (wp->get_weapon_info_refire_delay() * 1000.0f)) * 100.0f);
                }
                spread = (float)fired;
                if (100.0f - spread < 0.0f)
                {
                    spread = 100.0f;
                }
            }
            if (pp->cur_state == HS_COVER)
            {
                // Magic numbers: the movement submodes of firing from cover.
                const bool not_firing_submode = (int)pp->movement_submode < 0x14 || (int)pp->movement_submode > 0x17;
                if ((!can_fire || not_firing_submode) && pp->hflags.side_firing == 0)
                {
                    spread = 0.0f;
                    skip_target = true;
                }
            }
            if (player_is_scoped(pp) || player_is_fine_aimed(pp))
            {
                reticule_name = wp->w_info->fine_aim_reticule_name;
                // Magic number: the fine aim reticle's negative spread.
                spread = -125.0f;
                fine_aim = true;
            }
        }
        if (!skip_target && (pp->heat_seeking_target != 0xFFFFFFFF || on_turret || vec_dist(pp->aim_pos, pp->pos) <= wp->w_info->red_range))
        {
            target = hud_ui_weapons_determine_object_targeted(pp);
            if (upgrade_has_heat_seeking_rockets() && (wp->w_info->projectile_info.flags & 1) != 0)
            {
                const unsigned int handle = pp->heat_seeking_target != 0xFFFFFFFF ? pp->heat_seeking_target : pp->aim_target;
                object* objp = World.object_from_handle(handle);
                if (objp != nullptr && (objp->get_obj_type() == OT_HUMAN || objp->get_obj_type() == OT_VEHICLE))
                {
                    locking = pp->heat_seeking_target_locking_status();
                }
            }
        }
        if (!fine_aim && spread < 0.0f)
        {
            spread = 0.0f;
        }
    }

    item->set_element(0, vint_variant(weapon_changed));
    item->set_element(1, vint_variant(low_ammo));
    item->set_element(2, vint_variant(can_fire));
    item->set_element(3, vint_variant(reloading));
    item->set_element(4, vint_variant(weapon_name));
    item->set_element(5, vint_variant(icon_name));
    item->set_element(6, vint_variant(reticule_name));
    item->set_element(7, vint_variant((int)spread));
    item->set_element(8, vint_variant(magazine));
    item->set_element(9, vint_variant(reserve));
    item->set_element(10, vint_variant(magazine_size));
    item->set_element(11, vint_variant(target));
    item->set_element(12, vint_variant(pp != nullptr && pp->pflags.local_player_hit_someone_this_frame != 0));
    item->set_element(13, vint_variant(overheated));
    item->set_element(14, vint_variant((int)heat));
    item->set_element(15, vint_variant(on_turret));
    item->set_element(16, vint_variant(secondary_can_fire));
    // Magic number: 3 means no change of the lock.
    item->set_element(17, vint_variant(locking != Last_locking_status ? locking : 3));
    item->set_element(18, vint_variant(show_ammo));
    item->set_element(19, vint_variant(rockets));
    item->set_element(20, vint_variant(total_charges));
    item->set_element(21, vint_variant(free_charges));
    const camera_mode mode = camera_get_mode();
    // Magic numbers: camera modes 8 and 10, the gyro aiming cameras.
    if (input_get_gyro_enabled(GYRO_MODE_THIRD_PERSON) && ((int)mode == 8 || (int)mode == 10) && !game_is_paused())
    {
        item->set_element(22, vint_variant(true));
        // Magic number: the gyro's value in pixels of the reticle.
        const float x = input_gyro_get_camera_orientation_value(GYRO_MODE_THIRD_PERSON, GYRO_CAMERA_AXIS_X, Player_controller_index) * 250.0f;
        item->set_element(23, vint_variant(camera_first_person_get_gyro_base_sensitivity_heading() * x));
        const float y = input_gyro_get_camera_orientation_value(GYRO_MODE_THIRD_PERSON, GYRO_CAMERA_AXIS_Y, Player_controller_index) * 250.0f;
        item->set_element(24, vint_variant(camera_first_person_get_gyro_base_sensitivity_pitch() * y));
    }
    else
    {
        item->set_element(22, vint_variant(false));
        item->set_element(23, vint_variant(0.0f));
        item->set_element(24, vint_variant(0.0f));
    }
    // Magic number: 2 is locked.
    if (locking == 2 && Last_locking_status != 2)
    {
        audiolib_cue_play(Ga_cue_ids.rocket_lockon, nullptr, nullptr);
    }
    Last_locking_status = locking;
    if (pp != nullptr)
    {
        if (pp->pflags.local_player_hit_someone_this_frame != 0 && (!Hit_indicator_delay.valid() || Hit_indicator_delay.elapsed()))
        {
            if (Multiplayer)
            {
                audiolib_cue_play(Ga_cue_ids.multiplayer_player_hits_player, nullptr, nullptr);
            }
            // Magic number: a hit sound every 250 ms at most.
            Hit_indicator_delay.set(250);
        }
        pp->pflags.local_player_hit_someone_this_frame = 0;
    }
}

// 0x00899230
static void hud_ui_weapons_process_weapon_posn(float dtime)
{
    if (Data_handles_uiwpn[3] != -1)
    {
        vint_data_item* item = vint_data_item::find(Data_handles_uiwpn[3]);
        if (item != nullptr)
        {
            item->set_element(0, vint_variant(Weapon_set.m_weapon_screen_pos.x));
            item->set_element(1, vint_variant(Weapon_set.m_weapon_screen_pos.y));
        }
    }
}

// 0x008DA650
static bool hud_ui_weapons_render_doc(hud_doc_info info)
{
    player* pp = player_get_local_player();
    if (info == HDI_HEALTH)
    {
        return !(Multiplayer && multi_game_spectating());
    }
    if (info != HDI_BASE)
    {
        return true;
    }
    if (Rfg_camera.mode == CAMERA_SLEW_MODE)
    {
        return false;
    }
    if (Multiplayer && (hud_ui_multiplayer_splash_screen_is_active() || Multi_match_info.match_state != MGMS_GAMEPLAY || multi_game_spectating()))
    {
        return false;
    }
    if (pp != nullptr && (pp->is_killed() || player_is_scoped(pp)))
    {
        return false;
    }
    if (multi_game_camera_is_running())
    {
        return false;
    }
    if (wrecking_crew_is_active() && !wrecking_crew_can_player_move_camera() && !gameplay_can_player_attack(nullptr, DBT_NONE))
    {
        return false;
    }
    return true;
}

// 0x008DA700
void hud_ui_weapons_create_data_items()
{
    for (int i = 0; i < 6; i++)
    {
        int data_handle = -1;
        int doc = 0;
        if (player_get_local_player() != nullptr)
        {
            switch (i)
            {
                case 0: hud_ui_weapons_add_weapon_slot_data_items_info(data_handle, doc); break;
                case 1: hud_ui_weapons_add_weapon_show_data_items_info(data_handle, doc); break;
                case 2: hud_ui_weapons_add_weapon_fire_data_items_info(data_handle, doc); break;
                case 3: hud_ui_weapons_add_weapon_posn_data_items_info(data_handle, doc); break;
                case 4: hud_ui_weapons_add_safehouse_data_items_info(data_handle, doc); break;
                case 5: hud_ui_weapons_add_weapon_button_data_items_info(data_handle, doc); break;
            }
            ui_add_data_item(Doc_handles_uiwpn[doc], data_handle);
        }
        Data_handles_uiwpn[i] = data_handle;
    }
}

// 0x008992C0
void hud_ui_weapons_set_weapon_select_active(bool active)
{
    Weapon_set.m_show_weapons = active;
}

// 0x008992D0
bool hud_ui_weapons_is_weapon_select_active()
{
    return Weapon_set.m_show_weapons;
}

// 0x008992E0
void hud_ui_weapons_set_quick_switch(bool quick_switch)
{
    Weapon_set.m_quick_switch = quick_switch;
}

// 0x008992F0
void hud_ui_weapons_set_current_weapon_slot(int slot)
{
    Weapon_set.m_weapon_slot = slot;
}

// 0x00899300
void hud_ui_weapons_set_previous_weapon_slot(int slot)
{
    Weapon_set.m_prev_weapon_slot = slot;
}

// 0x00899310
void hud_ui_weapons_add_weapon_ammo_slot_update(int slot)
{
    Weapon_set.m_update_ammo_slot = Weapon_set.m_update_ammo_slot | 1 << (slot & 0x1F);
}

// 0x00899330
void hud_ui_weapons_enter()
{
    Assets_loaded_uiwpn = false;
    Assets_only = false;
    Data_items_created_uiwpn = false;
    Active_uiwpn = false;
    ui_init_weapon_set();
}

// 0x00910E10
void hud_ui_weapons_load_items(bool reload_flag)
{
    if (!Active_uiwpn)
    {
        if (reload_flag)
        {
            ui_reload_doc_assets(Doc_handles_uiwpn[0]);
            ui_reload_doc_assets(Doc_handles_uiwpn[1]);
            Assets_only = true;
        }
        else
        {
            Doc_handles_uiwpn[0] = ui_load_document(cUI_Child_Hud_Weapon_Select, DFIT_ALPHA, 0.5f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
            Doc_handles_uiwpn[1] = ui_load_document(cUI_Child_Hud_Reticles, DFIT_ALPHA, 0.5f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
        }
        Assets_loaded_uiwpn = false;
        Active_uiwpn = true;
    }
}

// 0x008F5AB0
void hud_ui_weapons_unload_items(bool assets_only)
{
    if (assets_only)
    {
        if (Active_uiwpn)
        {
            ui_unload_doc_assets(Doc_handles_uiwpn[0]);
            ui_unload_doc_assets(Doc_handles_uiwpn[1]);
            Assets_loaded_uiwpn = false;
            Active_uiwpn = false;
        }
        return;
    }
    if (Doc_handles_uiwpn[0] != -1)
    {
        ui_unload_document(Doc_handles_uiwpn[0], DFOT_NONE, 0.5f);
    }
    if (Doc_handles_uiwpn[1] != -1)
    {
        ui_unload_document(Doc_handles_uiwpn[1], DFOT_NONE, 0.5f);
    }
    Active_uiwpn = false;
    Data_items_created_uiwpn = false;
    Assets_loaded_uiwpn = false;
    Doc_handles_uiwpn[1] = -1;
    Doc_handles_uiwpn[0] = -1;
}

// 0x008F5B60
void hud_ui_weapons_process(float dtime)
{
    if (!Assets_loaded_uiwpn && Active_uiwpn)
    {
        Assets_loaded_uiwpn = true;
        for (int i = 0; i < 2; i++)
        {
            switch (ui_get_doc_asset_state(Doc_handles_uiwpn[i]))
            {
                case ST_UNLOADED:
                case ST_ERROR:
                    hud_ui_weapons_unload_items(false);
                    break;
                case ST_LOADING:
                    Assets_loaded_uiwpn = false;
                    break;
                default:
                    break;
            }
        }
        if (!Assets_loaded_uiwpn)
        {
            return;
        }
        if (!Data_items_created_uiwpn)
        {
            hud_ui_weapons_create_data_items();
            Data_items_created_uiwpn = true;
        }
        for (int handle : Doc_handles_uiwpn)
        {
            ui_fade_in_doc(handle, DFIT_ALPHA, 0.5f);
        }
    }
    if (Assets_loaded_uiwpn || (game_is_paused() && Data_items_created_uiwpn))
    {
        hud_ui_weapons_process_weapon_slot_info(dtime);
        hud_ui_weapons_process_weapon_select_info(dtime);
        if (Data_handles_uiwpn[4] != -1)
        {
            vint_data_item* item = vint_data_item::find(Data_handles_uiwpn[4]);
            if (item != nullptr && Multiplayer && player_get_local_player() != nullptr)
            {
                // The element that shows the backpack line; hud_ui_weapons_process_safehouse_info is not called here.
                item->set_element(0, vint_variant(false));
            }
        }
        hud_ui_weapons_process_weapon_fire_info(dtime);
        hud_ui_weapons_process_weapon_posn(dtime);
        hud_ui_weapons_process_weapon_select_buttons(dtime);
    }
    farray<int,16> handles;
    handles.add(Doc_handles_uiwpn[0]);
    handles.add(Doc_handles_uiwpn[1]);
    ui_add_for_processing(handles);
}

// 0x008EC080
void hud_ui_weapons_render()
{
    if (Assets_loaded_uiwpn)
    {
        farray<int,16> handles;
        for (int i = HDI_HEALTH; i < 2; i++)
        {
            if (hud_ui_weapons_render_doc((hud_doc_info)i))
            {
                handles.add(Doc_handles_uiwpn[i]);
            }
        }
        ui_add_for_rendering(handles);
    }
}

// 0x008C6D90
void hud_ui_weapons_get_doc_handles(farray<int,16>& handles)
{
    for (int handle : Doc_handles_uiwpn)
    {
        handles.add(handle);
    }
}
