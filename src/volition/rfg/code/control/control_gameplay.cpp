// Original: volition/rfg/code/control/control_gameplay.cpp
//
// The gameplay control mode (player_minimal.md P6). control_gameplay_process runs every frame through
// Control_mode_funcs: it sets the player's control permissions from its state (Interface_control_callbacks, then
// Player_state_control_callbacks), handles the actions (weapon menu, action button, attacks, zoom, jump, crouch,
// sprint, reload, fire, the vehicle controls), and then moves the player and the camera through the state's movement
// and camera callbacks. Compiled in rfg_control_unity.cpp after control.cpp, whose statics it uses.
// The callbacks of every player and interface state (stub_impl.md S5: turrets, vehicles, cover, satellite, races,
// spectating, ...). Not ported: set_control_permissions_rhino, maybe_ignore_camera_controls,
// vehicle_uses_tank_fire_buttons and get_driving_analog_value (no caller reached).
// Multiple-op float expressions are computed in double and rounded once, as the SSE2 code does; the camera input
// curve is x87 (__CIpow).

#include "volition/rfg/code/control/control_gameplay.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/camera/camera_free.h"
#include "volition/rfg/code/camera/camera_lookaround.h"
#include "volition/rfg/code/camera/camera_satellite.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/control/control.h"
#include "volition/rfg/code/cutscene/cutscene.h"
#include "volition/rfg/code/gameplay/activities/activity_vrail.h"
#include "volition/rfg/code/gameplay/gameplay.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/havok/api/hkcharactercontroller.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/interface/ingame/interface.h"
#include "volition/rfg/code/interface/ingame/ovl_inventory.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_player.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_move.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/ladder.h"
#include "volition/rfg/code/objects/humans/player_cover.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_flyer.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle/walker.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/time/timestamp.h"

#include "keen/base/systemtimer.hpp"

#include <math.h>

// ---- TU-private types (rfg.pdb; no header names them)
// The control callbacks of one player state (Player_state_control_callbacks) or interface mode
// (Interface_control_callbacks), at the end of this file.
// size 0xC
struct control_callback_info
{
    void (*set_control_permissions)(player*); // 0x00
    void (*do_player_movement)(); // 0x04
    void (*do_camera_movement)(); // 0x08
};

// Permission bits (player::common_controls_allowed, controls_general_allowed, controls_on_foot_allowed,
// general_actions_allowed), named after the controls control_gameplay_process lets them through (inferred).
static const unsigned int CCA_GENERAL = 0x1; // common: weapon menu, action button, backpack, detonate, satellite
static const unsigned int CCA_ON_FOOT = 0x2; // common: the on-foot controls
static const unsigned int CCA_DRIVING = 0x4; // common: the vehicle controls
static const unsigned int CGA_WEAPON_MENU = 0x1; // general
static const unsigned int CGA_ACTION = 0x2; // general
static const unsigned int CGA_BACKPACK = 0x40; // general
static const unsigned int CGA_DETONATE = 0x80; // general
static const unsigned int CGA_SATELLITE = 0x100; // general
static const unsigned int CFA_FIRE = 0x1; // on foot: reload and fire the equipped weapon
static const unsigned int CFA_MELEE = 0x2; // on foot: the melee attack button
static const unsigned int CFA_ZOOM = 0x4; // on foot
static const unsigned int CFA_MELEE_WEAPON = 0x8; // on foot: the attack buttons swing a melee weapon
static const unsigned int CFA_THROW = 0x10; // on foot: the attack button throws
static const unsigned int CFA_JUMP = 0x20; // on foot: jump and cover
static const unsigned int CFA_CROUCH = 0x40; // on foot
static const unsigned int CFA_SPRINT = 0x80; // on foot

// ---- file-static globals
// hkVector4dComparison_maskToComparison (0x012D1CE0): Havok header static (hkvector4dcomparison.h) emitted in this TU, not declared.
static int Action_button_hold_time = 750; // 0x0164ADA0, ms the action button must be held when Action_button_hold
static int s_backpackActivationTimeFrame = 250; // 0x0164ADB0, ms: a shorter press of the shared sprint/backpack button toggles the backpack
static float Driving_deadzone = 0.2f; // 0x0164ADB4
static float Control_handbrake_min_speed_sq = 25.0f; // 0x0164ADB8
static float Control_vehicle_face_button_max = 0.3f; // 0x0164ADBC
static float Control_vehicle_analog_min = 0.05f; // 0x0164ADC0
static float Camera_first_person_input_exponent = 1.9f; // 0x0164ADC8, the camera stick response curve
static float Camera_auto_level_speed = 1.5f; // 0x0164ADCC
static int Message_box_handle = -1; // 0x0164ADD4
static timestamp Action_hold_timestamp; // 0x0164B69C, invalid until set (constructor)
static timestamp Camera_auto_level_timestamp; // 0x0164B6A0, invalid until set (constructor)
static float Control_tread_dead_zone_turn_mag; // 0x01E25760
static float Player_normal_movement_last_a; // 0x01E25768
static float Control_tread_side_zone_dot; // 0x01E25774
// float Player_normal_movement_last_v (0x01E2A9A0) is used here too; placed in control.cpp.
static float Control_tread_accl_dead_zone_dot; // 0x01E2A9A4
// float Player_normal_movement_last_h (0x01E2A9A8) is used here too; placed in control.cpp.
static float Control_tread_steer_dead_zone_dot; // 0x01E2A9AC
// bool Player_input_disabled (0x01E2A9B9) is used here too; placed in control.cpp.
static bool Player_camera_input_disabled; // 0x01E2A9BA
static bool Action_button_hold; // 0x01E2A9BB
static bool Action_button_lock; // 0x01E2A9C2
// bool s_backpackButtonDownLastFrame (0x01E2A9C3) is used here too; placed in control.cpp.
// unsigned int s_timeBackpackButtonPressedDown (0x01E2A9C4) is used here too; placed in control.cpp.
bool Camera_auto_level; // 0x01E2A9C8
// The vehicle control tweaks; tweak_table.xtbl sets them.
static tweak_table_item Tweak_table_item_Control_handbrake_min_speed_sq("Control_handbrake_min_speed_sq", &Control_handbrake_min_speed_sq); // 0x01E2B17C
static tweak_table_item Tweak_table_item_Control_tread_side_zone_dot("Control_tread_side_zone_dot", &Control_tread_side_zone_dot); // 0x01E2B170
static tweak_table_item Tweak_table_item_Control_tread_accl_dead_zone_dot("Control_tread_accl_dead_zone_dot", &Control_tread_accl_dead_zone_dot); // 0x01E2B238
static tweak_table_item Tweak_table_item_Control_tread_steer_dead_zone_dot("Control_tread_steer_dead_zone_dot", &Control_tread_steer_dead_zone_dot); // 0x01E2A9DC
static tweak_table_item Tweak_table_item_Control_tread_dead_zone_turn_mag("Control_tread_dead_zone_turn_mag", &Control_tread_dead_zone_turn_mag); // 0x01E2B220
static tweak_table_item Tweak_table_item_Control_vehicle_face_button_max("Control_vehicle_face_button_max", &Control_vehicle_face_button_max); // 0x01E2B244
static tweak_table_item Tweak_table_item_Control_vehicle_analog_min("Control_vehicle_analog_min", &Control_vehicle_analog_min); // 0x01E2B22C

float Stick_h; // 0x01E25764
float Stick_v; // 0x01E2576C
float Stick_a; // 0x01E25770
bool Control_crouch_hold = true; // 0x0164AD31
bool Control_zoom_hold; // 0x01E2A9C9

// ---- internal functions
static bool riding_shotgun_cancel_mbox_cb(int mbox_handle, int selection_index, msgbox_choice choice); // 0x006F6AC0 (:422)
static void set_basic_weapon_permissions(player* pp); // 0x006F6B20 (:447)
static void set_control_permissions_stand(player* pp); // 0x006FBCC0 (:504)
static void set_control_permissions_jump(player* pp); // 0x006FBEB0 (:615)
static void set_control_permissions_fall(player* pp); // 0x006FBFE0 (:665)
static void set_control_permissions_based_on_player_state(player* pp); // 0x006F70C0 (:1137)
static void process_player_movement_based_on_player_state(); // 0x006F70E0 (:1146)
static bool maybe_ignore_movement_controls(); // 0x006F7110 (:1157)
static void process_player_normal_movement_internal(bool ignore_movement); // 0x006FC3E0 (:1239)
static void process_player_normal_movement(); // 0x006FC980 (:1349)
static void process_camera_movement_based_on_player_state(); // 0x00700B80 (:1578)
static void process_camera_get_values(float& h, float& v, control_axis_action h_action, control_axis_action v_action); // 0x006FD560 (:1604)
static void process_camera_get_actions_for_vehicle(control_axis_action& h_action, control_axis_action& v_action, vehicle* vp); // 0x006F7180 (:1622)
static void process_camera_normal_controls(); // 0x006FD6D0 (:1650)
static bool use_zoom_hold(player* client_pp); // 0x006FDDC0 (:1876)

static void set_control_permissions_in_turret(player* pp); // 0x006F6C50 (:707)
static void set_control_permissions_when_dead(player* pp); // 0x006F6CD0 (:727)
static void set_control_permissions_spectator(player* pp); // 0x006F6D10 (:740)
static void set_control_permissions_defusing(player* pp); // 0x006F6DA0 (:763)
static void set_control_permissions_action_node(player* pp); // 0x006F6DD0 (:772)
static void set_control_permissions_using_ammo_box(player* pp); // 0x006F6E10 (:786)
static void set_control_permissions_in_vehicle(player* pp); // 0x006FC0E0 (:816)
static void set_control_permissions_player_pathing(player* pp); // 0x006F6E50 (:938)
static void set_control_permissions_in_vehicle_transition(player* pp); // 0x006FC300 (:949)
static void set_control_permissions_pre_race_in_vehicle(player* pp); // 0x006F6E80 (:985)
static void set_control_permissions_racing_without_combat_in_vehicle(player* pp); // 0x006F6EC0 (:1001)
static void set_control_permissions_racing_without_combat_stand(player* pp); // 0x006FC3A0 (:1022)
static void set_control_permissions_multi_pre_race(player* pp); // 0x006F6F10 (:1042)
static void set_control_permissions_multi_racing(player* pp); // 0x006F6F50 (:1057)
static void set_control_permissions_multi_racing_gunner(player* pp); // 0x006F6F90 (:1076)
static void set_control_permissions_satellite(player* pp); // 0x006F7090 (:1126)
static void process_player_cover_movement(); // 0x006FC990 (:1356)
static void process_player_driving_movement(); // 0x006FCC00 (:1402)
static void process_camera_driving_controls(); // 0x006FD9B0 (:1745)
static void process_camera_satellite_controls(); // 0x006FDC60 (:1821)
static void process_camera_cover_controls(); // 0x006FDDB0 (:1856)

// 0x006F6AC0 (:422)
static bool riding_shotgun_cancel_mbox_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Message_box_handle != mbox_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        player_use_vehicle(player_get_local_player(), true);
    }
    else if ((int)choice < 1 || 2 < (int)choice)
    {
        VLIB_CRITICAL_FAILURE();
    }
    Message_box_handle = -1;
    return true;
}

// 0x006F6B20 (:447)
// LTCG: pp in ESI. The permissions of the equipped weapon: firearm, thrown weapon, mine or melee.
static void set_basic_weapon_permissions(player* pp)
{
    if (!pp->char_inst->is_playing_action((animation_action)0x170))
    {
        const weapon_info* wi = human_weapons_get_weapon_info(pp);
        if (wi == nullptr)
        {
            pp->controls_on_foot_allowed |= CFA_MELEE_WEAPON; // unarmed
        }
        else if (wi->weapon_class == WEAPON_CLASS_THROWN_MINE)
        {
            pp->controls_on_foot_allowed |= CFA_MELEE;
            if (!Multiplayer || can_fire_charge(pp->get_handle(), false))
            {
                pp->controls_on_foot_allowed |= CFA_THROW;
            }
        }
        else
        {
            const weapon_ammo_type ammo_type = wi->ammo_type;
            if (ammo_type == WEAPON_AMMO_TYPE_THROWN)
            {
                if (can_fire_charge(pp->get_handle(), false))
                {
                    pp->controls_on_foot_allowed |= CFA_ZOOM;
                    if (wi->ammo_type == WEAPON_AMMO_TYPE_THROWN)
                    {
                        pp->controls_on_foot_allowed |= CFA_THROW;
                    }
                    else
                    {
                        pp->controls_on_foot_allowed |= CFA_FIRE;
                    }
                }
                else if (wi->flags.non_inventory)
                {
                    pp->controls_on_foot_allowed |= CFA_THROW;
                }
                pp->controls_on_foot_allowed |= CFA_MELEE;
            }
            else if (wi->is_firearm())
            {
                pp->controls_on_foot_allowed |= CFA_FIRE | CFA_MELEE | CFA_ZOOM;
            }
            else if (ammo_type == WEAPON_AMMO_TYPE_MELEE)
            {
                pp->controls_on_foot_allowed |= CFA_MELEE_WEAPON;
            }
        }
    }
    pp->controls_general_allowed |= CGA_WEAPON_MENU;
}

// The rhino backpack locks the controls (the end of every on-foot permission function).
static void set_control_permissions_rhino_backpack(player* pp)
{
    if (pp->backpack != nullptr && pp->backpack->get_backpack_abilities()->rhino)
    {
        pp->common_controls_allowed = 0x41;
        pp->controls_general_allowed = 0x12;
        pp->controls_on_foot_allowed = 0;
        pp->controls_driving_allowed = 0;
        pp->general_actions_allowed = 0;
    }
}

// 0x006FBCC0 (:504)
static void set_control_permissions_stand(player* pp)
{
    pp->common_controls_allowed = CCA_GENERAL | CCA_ON_FOOT;
    pp->controls_general_allowed = 0xC0;
    pp->controls_on_foot_allowed = 0x100;
    pp->controls_driving_allowed = 0;
    pp->general_actions_allowed = 0;
    if (Multiplayer)
    {
        pp->controls_general_allowed = 0xD0;
        pp->general_actions_allowed = 6;
    }
    if (pp->cur_movement_state == HMS_SPRINT)
    {
        pp->controls_general_allowed |= CGA_WEAPON_MENU | CGA_ACTION;
        if (Player_dismiss_enabled)
        {
            pp->controls_general_allowed |= 4;
        }
        if (Player_recruit_enabled)
        {
            pp->controls_general_allowed |= 8;
        }
        pp->controls_on_foot_allowed = 0xA2;
        const weapon_info* wi = human_weapons_get_weapon_info(pp);
        if (wi != nullptr && wi->ammo_type == WEAPON_AMMO_TYPE_MELEE)
        {
            pp->controls_on_foot_allowed |= CFA_MELEE_WEAPON;
        }
        pp->general_actions_allowed |= 0x7FF88;
    }
    else if (pp->ragdoll_state == HUMAN_RAGDOLL_STATE_NONE || pp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT)
    {
        pp->controls_general_allowed |= CGA_WEAPON_MENU;
        if (!pp->hflags.playing_equip_anim && !pp->hflags.playing_unequip_anim && pp->desired_equipped_inv_item == pp->get_equipped_inv_item())
        {
            pp->controls_general_allowed |= CGA_ACTION;
            if (Player_dismiss_enabled)
            {
                pp->controls_general_allowed |= 4;
            }
            if (Player_recruit_enabled)
            {
                pp->controls_general_allowed |= 8;
            }
            pp->controls_on_foot_allowed |= Multiplayer ? 0x60 : 0xE0;
            set_basic_weapon_permissions(pp);
            pp->general_actions_allowed |= 0x7FFF8;
        }
        else
        {
            // Equipping: crouching only, and not with a non-inventory item.
            const inventory_item* item = pp->desired_equipped_inv_item;
            if (item == nullptr || item->info == nullptr || !item->info->w_info->flags.non_inventory)
            {
                pp->controls_on_foot_allowed |= CFA_CROUCH;
            }
            else
            {
                pp->controls_on_foot_allowed &= ~CFA_CROUCH;
            }
        }
    }
    if (player_has_non_inventory(pp))
    {
        pp->controls_on_foot_allowed &= ~CFA_CROUCH;
    }
    set_control_permissions_rhino_backpack(pp);
}

// 0x006FBEB0 (:615)
static void set_control_permissions_jump(player* pp)
{
    pp->controls_driving_allowed = 0;
    pp->common_controls_allowed = CCA_GENERAL | CCA_ON_FOOT;
    pp->controls_general_allowed = 0xC2;
    pp->controls_on_foot_allowed = 0x60;
    pp->general_actions_allowed = 0x120;
    if (!Multiplayer)
    {
        pp->controls_on_foot_allowed = 0xE0;
    }
    else
    {
        pp->controls_general_allowed = 0xD2;
        pp->general_actions_allowed = 0x126;
    }
    if (pp->cur_movement_state == HMS_SPRINT)
    {
        pp->controls_on_foot_allowed |= CFA_MELEE;
        const weapon_info* wi = human_weapons_get_weapon_info(pp);
        if (wi != nullptr && wi->ammo_type == WEAPON_AMMO_TYPE_MELEE)
        {
            pp->controls_on_foot_allowed |= CFA_MELEE_WEAPON;
        }
    }
    else
    {
        set_basic_weapon_permissions(pp);
    }
    pp->controls_on_foot_allowed &= ~CFA_ZOOM;
    pp->controls_general_allowed |= CGA_WEAPON_MENU;
    if (player_has_non_inventory(pp))
    {
        pp->controls_on_foot_allowed &= ~CFA_THROW;
    }
    const weapon_info* wi = human_weapons_get_weapon_info(pp);
    if (wi != nullptr && (wi->projectile_info.flags & 0x80) != 0)
    {
        pp->controls_on_foot_allowed &= ~CFA_THROW;
    }
    set_control_permissions_rhino_backpack(pp);
}

// 0x006FBFE0 (:665)
static void set_control_permissions_fall(player* pp)
{
    pp->controls_driving_allowed = 0;
    pp->common_controls_allowed = CCA_GENERAL | CCA_ON_FOOT;
    pp->controls_general_allowed = 0xC2;
    pp->controls_on_foot_allowed = 0x60;
    pp->general_actions_allowed = 0x120;
    if (!Multiplayer)
    {
        pp->controls_on_foot_allowed = 0xE0;
    }
    else
    {
        pp->controls_general_allowed = 0xD2;
        pp->general_actions_allowed = 0x126;
    }
    set_basic_weapon_permissions(pp);
    pp->controls_on_foot_allowed &= ~CFA_ZOOM;
    pp->controls_general_allowed |= CGA_WEAPON_MENU;
    if (player_has_non_inventory(pp))
    {
        pp->controls_on_foot_allowed &= ~CFA_THROW;
    }
    const weapon_info* wi = human_weapons_get_weapon_info(pp);
    if (wi != nullptr && (wi->projectile_info.flags & 0x80) != 0)
    {
        pp->controls_on_foot_allowed &= ~CFA_THROW;
    }
    set_control_permissions_rhino_backpack(pp);
}

// 0x006F6C50 (:707)
// On a turret: the general controls, fire (zoom unless the turret forces fine aim), and the turret's actions.
static void set_control_permissions_in_turret(player* pp)
{
    pp->common_controls_allowed = CCA_GENERAL | CCA_ON_FOOT;
    pp->controls_general_allowed = 0xC2;
    pp->controls_on_foot_allowed = CFA_FIRE;
    pp->controls_driving_allowed = 0;
    pp->general_actions_allowed = 0x10;
    if (Multiplayer)
    {
        pp->controls_general_allowed = 0xD2;
        pp->general_actions_allowed = 0x16;
    }
    turret* tp = human_combat_get_manned_turret(pp);
    if (tp != nullptr && !tp->t_info->flags.force_fine_aim)
    {
        pp->controls_on_foot_allowed |= CFA_ZOOM;
    }
}

// 0x006F6CD0 (:727)
// Dead: the general controls and the action button (respawn) only.
static void set_control_permissions_when_dead(player* pp)
{
    pp->common_controls_allowed = CCA_GENERAL;
    pp->controls_general_allowed = 0x22;
    pp->controls_on_foot_allowed = 0;
    pp->controls_driving_allowed = 0;
    pp->general_actions_allowed = 0;
    if (Multiplayer)
    {
        pp->controls_general_allowed = 0x32;
    }
}

// 0x006F6D10 (:740)
static void set_control_permissions_spectator(player* pp)
{
    pp->common_controls_allowed = 0;
    pp->controls_general_allowed = 0;
    pp->controls_on_foot_allowed = 0;
    pp->controls_driving_allowed = 0;
    pp->general_actions_allowed = 0;
}

// 0x006F6D40 (:749): on a ladder only the general controls, the ladder movement and the general actions (more of
// both in multiplayer). Magic numbers: the permission masks.
static void set_control_permissions_on_ladder(player* pp)
{
    pp->common_controls_allowed = CCA_GENERAL | CCA_ON_FOOT;
    pp->controls_general_allowed = 0xC2;
    pp->controls_on_foot_allowed = 0x20;
    pp->controls_driving_allowed = 0;
    pp->general_actions_allowed = 0x100;
    if (Multiplayer)
    {
        pp->controls_general_allowed = 0xD2;
        pp->general_actions_allowed = 0x106;
    }
}

// 0x006F6DA0 (:763)
static void set_control_permissions_defusing(player* pp)
{
    pp->common_controls_allowed = 0;
    pp->controls_general_allowed = 0;
    pp->controls_on_foot_allowed = 0;
    pp->controls_driving_allowed = 0;
    pp->general_actions_allowed = 0;
}

// 0x006F6DD0 (:772)
static void set_control_permissions_action_node(player* pp)
{
    pp->common_controls_allowed = 0;
    pp->controls_general_allowed = CGA_DETONATE;
    pp->controls_on_foot_allowed = 0;
    pp->controls_driving_allowed = 0;
    pp->general_actions_allowed = 0;
    if (Multiplayer)
    {
        pp->controls_general_allowed = 0x90;
    }
}

// 0x006F6E10 (:786)
static void set_control_permissions_using_ammo_box(player* pp)
{
    pp->common_controls_allowed = 0;
    pp->controls_general_allowed = CGA_DETONATE;
    pp->controls_on_foot_allowed = 0;
    pp->controls_driving_allowed = 0;
    pp->general_actions_allowed = 0;
    if (Multiplayer)
    {
        pp->controls_general_allowed = 0x90;
    }
}

// 0x006FC0E0 (:816)
// In a vehicle: the driver drives (an automobile with all its controls, a walker with its own; a hostage exit waiting
// only lets the brakes work, once), a passenger has the on-foot controls; a ready firearm or thrown weapon fires (from
// the driver seat with the driving fire buttons), a turret seat adds zoom and its secondary weapon. The weapon menu
// is allowed unless a trigger is down.
static void set_control_permissions_in_vehicle(player* pp)
{
    pp->common_controls_allowed = CCA_GENERAL;
    pp->controls_general_allowed = 0x82;
    if (Player_dismiss_enabled)
    {
        pp->controls_general_allowed = 0x86;
    }
    if (Player_recruit_enabled)
    {
        pp->controls_general_allowed |= 8;
    }
    pp->controls_on_foot_allowed = 0;
    pp->controls_driving_allowed = 0;
    pp->general_actions_allowed = 0x80;
    vehicle* vp = nullptr;
    if (pp->vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(pp->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            vp = (vehicle*)objp;
        }
    }
    bool driver = false;
    if (vp == nullptr || vehicle_passenger_in_seat(vp, VSI_DRIVER) != pp)
    {
        pp->common_controls_allowed |= CCA_ON_FOOT;
    }
    else
    {
        driver = true;
        if (pp->pflags.is_waiting_for_hostage_vehicle_exit)
        {
            pp->controls_driving_allowed = 0x10;
            pp->pflags.is_waiting_for_hostage_vehicle_exit = 0;
        }
        else
        {
            pp->common_controls_allowed |= CCA_DRIVING;
            pp->controls_driving_allowed = 0x11;
            if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO)
            {
                pp->controls_driving_allowed = 0x7F;
            }
            if (vp->get_obj_type() == OT_VEHICLE && vp->get_sub_type() == OT_SUB_VEHICLE_WALKER)
            {
                pp->controls_driving_allowed |= 0x200;
            }
        }
    }
    const bool ready = !pp->hflags.playing_equip_anim && !pp->hflags.playing_unequip_anim && pp->desired_equipped_inv_item == pp->get_equipped_inv_item();
    weapon* wp = human_weapons_get_weapon(pp);
    if (ready && wp != nullptr && wp->w_info != nullptr)
    {
        const weapon_info* wi = wp->w_info;
        bool trigger_down = false;
        if (wi->is_firearm())
        {
            if (driver)
            {
                pp->controls_driving_allowed |= 0x80;
            }
            else
            {
                pp->controls_on_foot_allowed |= CFA_FIRE;
            }
            trigger_down = wp->is_trigger_pulled(); // inlined in the binary
        }
        else if (wi->ammo_type == WEAPON_AMMO_TYPE_THROWN)
        {
            if (driver)
            {
                pp->controls_driving_allowed |= 0x100;
            }
            else
            {
                pp->controls_on_foot_allowed |= CFA_THROW;
            }
        }
        if (wp->turret_handle != 0xFFFFFFFF)
        {
            object* objp = World.object_from_handle(wp->turret_handle);
            if (objp != nullptr && objp->get_obj_type() == OT_TURRET)
            {
                pp->controls_on_foot_allowed |= CFA_ZOOM;
                weapon* secondary = ((turret*)objp)->get_secondary_weapon_inst();
                if (secondary != nullptr)
                {
                    pp->controls_on_foot_allowed |= CFA_MELEE;
                    trigger_down = trigger_down || secondary->is_trigger_pulled();
                }
            }
        }
        if (!trigger_down)
        {
            pp->controls_general_allowed |= CGA_WEAPON_MENU;
        }
    }
    else
    {
        pp->controls_general_allowed |= CGA_WEAPON_MENU;
    }
    if (Multiplayer)
    {
        pp->controls_general_allowed |= 0x10;
    }
}

// 0x006F6E50 (:938)
static void set_control_permissions_player_pathing(player* pp)
{
    pp->common_controls_allowed = CCA_GENERAL;
    pp->controls_general_allowed = 0x82;
    pp->controls_on_foot_allowed = 0;
    pp->controls_driving_allowed = 0;
    pp->general_actions_allowed = 0;
}

// 0x006FC300 (:949)
static void set_control_permissions_in_vehicle_transition(player* pp)
{
    pp->common_controls_allowed = CCA_GENERAL;
    pp->controls_general_allowed = 0x82;
    pp->controls_on_foot_allowed = 0;
    pp->controls_driving_allowed = 0;
    pp->general_actions_allowed = 0x80;
    if (Multiplayer)
    {
        pp->controls_general_allowed = 0x92;
    }
    set_control_permissions_rhino_backpack(pp); // inlined in the binary
}

// 0x006F6E80 (:985)
static void set_control_permissions_pre_race_in_vehicle(player* pp)
{
    pp->common_controls_allowed = CCA_DRIVING;
    pp->controls_general_allowed = 0;
    pp->controls_on_foot_allowed = 0;
    pp->controls_driving_allowed = 0x21;
    pp->general_actions_allowed = 0;
    if (Multiplayer)
    {
        pp->controls_general_allowed = 0x10;
    }
}

// 0x006F6EC0 (:1001)
static void set_control_permissions_racing_without_combat_in_vehicle(player* pp)
{
    pp->common_controls_allowed = CCA_GENERAL | CCA_DRIVING;
    pp->controls_general_allowed = CGA_ACTION;
    pp->controls_on_foot_allowed = 0;
    pp->controls_driving_allowed = 0x7F;
    pp->general_actions_allowed = 0x80;
    if (Multiplayer)
    {
        pp->controls_general_allowed = 0x12;
    }
}

// 0x006FC3A0 (:1022)
// Standing, without the weapon menu and the dismiss/recruit buttons.
static void set_control_permissions_racing_without_combat_stand(player* pp)
{
    set_control_permissions_stand(pp);
    pp->controls_general_allowed &= ~0xDu;
    pp->general_actions_allowed = 0x80;
    if (Multiplayer)
    {
        pp->controls_general_allowed |= 0x10;
    }
}

// 0x006F6F10 (:1042)
static void set_control_permissions_multi_pre_race(player* pp)
{
    pp->common_controls_allowed = CCA_DRIVING;
    pp->controls_general_allowed = 0;
    pp->controls_on_foot_allowed = 0;
    pp->controls_driving_allowed = 0x21;
    pp->general_actions_allowed = 0;
    if (Multiplayer)
    {
        pp->controls_general_allowed = 0x10;
    }
}

// 0x006F6F50 (:1057)
static void set_control_permissions_multi_racing(player* pp)
{
    pp->common_controls_allowed = CCA_DRIVING;
    pp->controls_general_allowed = 0;
    pp->controls_on_foot_allowed = 0;
    pp->controls_driving_allowed = 0x7F;
    pp->general_actions_allowed = 0;
    if (Multiplayer)
    {
        pp->controls_general_allowed = 0x10;
    }
}

// 0x006F6F90 (:1076)
static void set_control_permissions_multi_racing_gunner(player* pp)
{
    pp->common_controls_allowed = CCA_GENERAL | CCA_ON_FOOT;
    pp->controls_general_allowed = CGA_ACTION;
    pp->controls_on_foot_allowed = 0;
    pp->controls_driving_allowed = 0;
    pp->general_actions_allowed = 0x20;
    const bool ready = !pp->hflags.playing_equip_anim && !pp->hflags.playing_unequip_anim && pp->desired_equipped_inv_item == pp->get_equipped_inv_item();
    const weapon_info* wi = human_weapons_get_weapon_info(pp);
    if (ready && wi != nullptr)
    {
        if (wi->is_firearm()) // inlined in the binary
        {
            pp->controls_on_foot_allowed |= CFA_FIRE;
        }
        else if (wi->ammo_type == WEAPON_AMMO_TYPE_THROWN)
        {
            pp->controls_on_foot_allowed |= CFA_THROW;
        }
    }
    pp->controls_general_allowed |= CGA_WEAPON_MENU;
    if (Multiplayer)
    {
        pp->controls_general_allowed |= 0x10;
    }
}

// 0x006F7090 (:1126)
static void set_control_permissions_satellite(player* pp)
{
    pp->common_controls_allowed = CCA_GENERAL;
    pp->controls_general_allowed = CGA_SATELLITE;
    pp->controls_on_foot_allowed = 0;
    pp->controls_driving_allowed = 0;
    pp->general_actions_allowed = 0;
}

// 0x006F70C0 (:1137)
static void set_control_permissions_based_on_player_state(player* pp)
{
    extern const control_callback_info Player_state_control_callbacks[17];
    if (Player_state_control_callbacks[pp->cur_state].set_control_permissions != nullptr)
    {
        Player_state_control_callbacks[pp->cur_state].set_control_permissions(pp);
    }
}

// 0x006F70E0 (:1146)
static void process_player_movement_based_on_player_state()
{
    extern const control_callback_info Player_state_control_callbacks[17];
    if (Player_state_control_callbacks[player_get_local_player()->cur_state].do_player_movement != nullptr)
    {
        Player_state_control_callbacks[player_get_local_player()->cur_state].do_player_movement();
    }
}

// 0x006F7110 (:1157)
static bool maybe_ignore_movement_controls()
{
    player* pp = player_get_local_player();
    if (wrecking_crew_is_active() && !wrecking_crew_can_player_move())
    {
        return true;
    }
    if (Multiplayer && Multi_match_info.match_state != MGMS_GAMEPLAY)
    {
        return true;
    }
    return pp->hflags.locked_controller;
}

// 0x007009C0 (:1207): the forward/back stick climbs and slides (none while the player may not move); in multiplayer
// it is the command sent to the server, with the camera.
static void process_player_ladder_movement()
{
    player* pp = player_get_local_player();
    float v = controls_get_scaled_analog(CAA_WALK_FORWARD_BACKWARD, Player_controller_index);
    if (maybe_ignore_movement_controls()) // inlined
    {
        v = 0.0f;
    }
    if (Multiplayer)
    {
        pp->mp_current_command.forward = v;
        pp->mp_current_command.sideways = 0.0f;
        pp->mp_camera_orient = Rfg_camera.real_orient;
        pp->mp_camera_pos = Rfg_camera.real_pos;
        float p;
        float b;
        float hd;
        Rfg_camera.real_orient.extract_angles(p, b, hd);
        float last_p;
        float last_hd;
        Rfg_camera.last_orient.extract_angles(last_p, b, last_hd);
        pp->mp_camera_angular_velocity.x = (p - last_p) / Frametime;
        pp->mp_camera_angular_velocity.y = (hd - last_hd) / Frametime;
    }
    ladder_process_human(pp, v);
}

// 0x006F7180 (:1622)
// LTCG: h_action in EDI, v_action in ESI, vp in ECX.
static void process_camera_get_actions_for_vehicle(control_axis_action& h_action, control_axis_action& v_action, vehicle* vp)
{
    const int sub_type = vp->get_sub_type();
    if (sub_type == 4)
    {
        if (vehicle_is_tank(vp))
        {
            h_action = CAA_TANK_CAMERA_ROTATE;
            v_action = CAA_TANK_CAMERA_ELEVATE;
            return;
        }
        h_action = CAA_DRIVE_CAMERA_ROTATE;
        v_action = CAA_DRIVE_CAMERA_ELEVATE;
        return;
    }
    if ((unsigned int)(sub_type - 5) < 2)
    {
        h_action = CAA_CAMERA_ROTATE;
        v_action = CAA_CAMERA_ELEVATE;
        return;
    }
    VLIB_CRITICAL_FAILURE();
}

// 0x006F7200 (:1904)
void control_gameplay_system_init()
{
}

// 0x006FDE10 (:1915)
void control_gameplay_level_init_post()
{
    Player_input_disabled = false;
    Player_normal_movement_last_h = 0.0f;
    Player_normal_movement_last_v = 0.0f;
    s_backpackButtonDownLastFrame = false;
    s_timeBackpackButtonPressedDown = 0;
}

// Clears the player's current action object (the action callback ran).
static void control_gameplay_clear_action_object(player* pp)
{
    pp->action_object.handle = 0xFFFFFFFF;
    pp->action_object.action_type = GENERAL_ACTION_TYPE_INVALID;
    pp->action_object.action_icon = USE_OBJECT_ICON_INVALID;
    pp->action_object.action_callback = nullptr;
    pp->action_object.dot_prod = -1.0f;
    pp->action_object.dist_squared = -1.0f;
}

// 0x006F7210 (:1922)
// The action button: the current action object, entering or leaving a vehicle or turret, putting a carried object
// down, using the equipped item, or talking.
bool control_gameplay_just_pressed_action(player* pp)
{
    if (Action_button_hold)
    {
        if (!Action_hold_timestamp.valid())
        {
            Action_hold_timestamp.set(Action_button_hold_time);
            return false;
        }
        if (!Action_hold_timestamp.elapsed())
        {
            return false;
        }
    }
    if (pp->action_object.action_callback != nullptr)
    {
        const bool result = pp->action_object.action_callback(pp, true);
        control_gameplay_clear_action_object(pp); // inlined in the binary
        Action_button_lock = true;
        return result;
    }
    if (human_in_vehicle_interaction(pp, nullptr))
    {
        Action_button_lock = true;
        if (!activity_vrail_is_riding_shotgun_running())
        {
            return player_use_vehicle(pp, true);
        }
        Message_box_handle = ui_add_msgbox(MBT_ACCEPT_CANCEL_SELECT, "MENU_WARNING", "MENU_RS_CANCEL_MESSAGE", riding_shotgun_cancel_mbox_cb, false, false, nullptr, nullptr, false);
        return true;
    }
    if (human_combat_get_manned_turret(pp) != nullptr)
    {
        Action_button_lock = true;
        return player_use_turret(pp, true);
    }
    if (player_has_non_inventory(pp) && pp->cur_state == HS_STANDARD)
    {
        Action_button_lock = true;
        player_set_down_non_inventory(pp);
        return true;
    }
    if ((pp->general_actions_allowed & 5) != 0 && player_use_equipped_item(pp))
    {
        Action_button_lock = true;
        return true;
    }
    if ((pp->general_actions_allowed & 3) != 0 && player_start_dialogue(pp))
    {
        Action_button_lock = true;
        return true;
    }
    return false;
}

// 0x006F7380 (:2030)
bool control_gameplay_continued_press_action(player* pp)
{
    if (Action_button_lock)
    {
        return false;
    }
    if (!Action_button_hold)
    {
        if (pp->action_object.action_callback != nullptr)
        {
            const bool result = pp->action_object.action_callback(pp, true);
            control_gameplay_clear_action_object(pp); // inlined in the binary
            Action_button_lock = true;
            return result;
        }
        return false;
    }
    if (Action_hold_timestamp.valid() && Action_hold_timestamp.elapsed())
    {
        return control_gameplay_just_pressed_action(pp);
    }
    return false;
}

// 0x006F7420 (:2133)
void control_gameplay_release_action(player* pp)
{
    Action_button_lock = false;
    Action_hold_timestamp.invalidate();
}

// 0x00700C50 (:2171)
void control_gameplay_process()
{
    extern const control_callback_info Interface_control_callbacks[NUM_INT_MODES];
    if (game_is_paused())
    {
        return;
    }
    if (Multiplayer && multi_is_dedicated_server())
    {
        return;
    }
    player* pp = player_get_local_player();
    bool do_camera = true;
    if (pp == nullptr || Player_input_disabled)
    {
        process_player_normal_movement_internal(true);
    }
    else
    {
        pp->mp_current_command.vcontrols.controls = 0;
        pp->mp_current_command.vcontrols.accelerate = 0.0f;
        pp->mp_current_command.vcontrols.steering = 0.0f;
        pp->common_controls_allowed = 0;
        pp->controls_general_allowed = 0;
        pp->controls_on_foot_allowed = 0;
        pp->controls_driving_allowed = 0;
        pp->general_actions_allowed = 0;
        if (Interface_mode != INT_MODE_NONE && Interface_control_callbacks[Interface_mode].set_control_permissions != nullptr)
        {
            Interface_control_callbacks[Interface_mode].set_control_permissions(pp);
        }
        bool do_movement = !pp->is_killed();
        if (hud_ui_multiplayer_splash_screen_is_active())
        {
            const control_button_action select = ui_get_control_button_value(BC_SELECT);
            if (select != CBA_BUTTON_INVALID)
            {
                controls_mask_button(select, Player_controller_index);
            }
        }
        if (pp->backpack != nullptr && pp->backpack->get_backpack_abilities()->charge)
        {
            do_camera = false;
        }
        // Fire stops unless a fire control started it this frame (also without controls).
        bool stop_primary_fire = true;
        bool stop_secondary_fire = true;
        if (cutscene_is_playing() || pp->pflags.script_controlled || pp->pflags.disable_controls)
        {
            do_movement = false;
            do_camera = false;
        }
        else if (!pp->hflags.locked_controller)
        {
            if ((pp->common_controls_allowed & CCA_GENERAL) != 0)
            {
                // the weapon menu (a held direction or the cycle axis opens it)
                const float cycle = controls_get_analog(CAA_CYCLE_WEAPONS, Player_controller_index);
                bool menu_direction = false;
                if (!controls_is_using_weapon_menu_button() && (controls_is_button_pressed(CBA_WEAPON_MENU_DOWN, Player_controller_index) || controls_is_button_pressed(CBA_WEAPON_MENU_UP, Player_controller_index) || controls_is_button_pressed(CBA_WEAPON_MENU_LEFT, Player_controller_index) || controls_is_button_pressed(CBA_WEAPON_MENU_RIGHT, Player_controller_index)))
                {
                    menu_direction = true;
                }
                if (human_in_vehicle(pp) != nullptr || human_combat_is_manning_turret(pp) || (pp->controls_general_allowed & CGA_WEAPON_MENU) == 0 || (!controls_is_button_just_pressed(CBA_GAC_WEAPON_MENU, Player_controller_index) && fabsf(cycle) <= 0.3f && !menu_direction))
                {
                    if ((pp->controls_general_allowed & CGA_ACTION) != 0)
                    {
                        // the action button
                        if (controls_is_button_just_pressed(control_gameplay_get_action_button(), Player_controller_index))
                        {
                            if (control_gameplay_just_pressed_action(pp))
                            {
                                controls_mask_button(control_gameplay_get_action_button(), Player_controller_index);
                                return;
                            }
                        }
                        else if (controls_is_button_down(control_gameplay_get_action_button(), Player_controller_index))
                        {
                            if (control_gameplay_continued_press_action(pp))
                            {
                                do_movement = false;
                                controls_mask_button(control_gameplay_get_action_button(), Player_controller_index);
                            }
                        }
                        else
                        {
                            control_gameplay_release_action(pp); // inlined in the binary
                        }
                        // quick exit from a vehicle being entered
                        if ((pp->cur_state == HS_PLAYER_PATHING || pp->cur_state == HS_VEHICLE_ENTERING) && human_can_quick_exit_vehicle(pp))
                        {
                            const float forward = controls_get_analog(CAA_WALK_FORWARD_BACKWARD, Player_controller_index);
                            const float sideways = controls_get_analog(CAA_WALK_TURN_LEFT_RIGHT, Player_controller_index);
                            vehicle* vp = World.object_from_handle<vehicle>(pp->vehicle_handle);
                            if (0.5f < fabsf(forward) || 0.5f < fabsf(sideways) || (vp != nullptr && vp->m_flags.m_on_fire))
                            {
                                player_use_vehicle(pp, true);
                            }
                        }
                        if (controls_is_button_just_pressed(CBA_VDC_CHANGE_SEAT, Player_controller_index) && human_in_vehicle_interaction(pp, nullptr) && !activity_vrail_is_riding_shotgun_running())
                        {
                            player_switch_seat(pp);
                        }
                    }
                }
                else
                {
                    vehicle* vp = human_in_vehicle(pp);
                    if (vp == nullptr || vp->get_sub_type() != 6)
                    {
                        ovl_inventory_open(controls_is_button_just_pressed(CBA_GAC_WEAPON_MENU, Player_controller_index) ? CBA_GAC_WEAPON_MENU : CBA_WEAPON_MENU_DOWN, 1.0f);
                    }
                }
                // the backpack: in single player its button may share the sprint button, and then only a short press
                // toggles it
                bool backpack_down = s_backpackButtonDownLastFrame;
                if (!Multiplayer && !wrecking_crew_is_active() && controls_get_button_for_action(CBA_GAC_TOGGLE_BACKPACK_ACTIVATION, Player_controller_index) == controls_get_button_for_action(CBA_OFC_SPRINT, Player_controller_index))
                {
                    const unsigned int now = keen::SystemTimer::getCurrentMilliseconds();
                    backpack_down = controls_is_button_down(CBA_GAC_TOGGLE_BACKPACK_ACTIVATION, Player_controller_index);
                    if (backpack_down)
                    {
                        if (s_timeBackpackButtonPressedDown == 0)
                        {
                            s_timeBackpackButtonPressedDown = now;
                        }
                    }
                    else if ((unsigned int)s_backpackActivationTimeFrame <= now - s_timeBackpackButtonPressedDown)
                    {
                        s_timeBackpackButtonPressedDown = 0;
                    }
                    if ((pp->controls_general_allowed & CGA_BACKPACK) != 0 && !backpack_down && s_backpackButtonDownLastFrame && s_timeBackpackButtonPressedDown != 0 && now - s_timeBackpackButtonPressedDown < (unsigned int)s_backpackActivationTimeFrame)
                    {
                        player_toggle_backpack_activation(pp, false, 300);
                    }
                }
                else if ((pp->controls_general_allowed & CGA_BACKPACK) != 0 && controls_is_button_just_pressed(CBA_GAC_TOGGLE_BACKPACK_ACTIVATION, Player_controller_index))
                {
                    player_toggle_backpack_activation(pp, false, 300);
                }
                s_backpackButtonDownLastFrame = backpack_down;
                // remote charges
                if (controls_is_button_just_pressed(CBA_OFC_CHARGE_DETONATE, Player_controller_index) && (pp->controls_general_allowed & CGA_DETONATE) != 0 && detonate_charges(pp->get_handle()))
                {
                    pp->mp_flags.set_detonated_charges(1);
                    multi_game_player_set_mp_flag(pp, PMPF_DETONATED_CHARGES, 1);
                    if (pp->char_inst->vp->default_group_index == ANIMATION_GROUP_REMOTE_CHARGE && (pp->common_controls_allowed & CCA_ON_FOOT) != 0 && !player_has_non_inventory(pp))
                    {
                        pp->char_inst->play_action(ANIMATION_ACTION_STAND_FIRE, ANIM_ALIAS_FIRE, 1.0f, 0x2000);
                    }
                }
                // the satellite camera
                if ((pp->controls_general_allowed & CGA_SATELLITE) != 0)
                {
                    if (controls_is_button_just_pressed(CBA_SAT_FIRE, Player_controller_index))
                    {
                        camera_satellite_fire_weapon();
                    }
                    if (controls_is_button_down(CBA_SAT_HOLD, Player_controller_index))
                    {
                        camera_satellite_hold_position_this_frame();
                    }
                    if (controls_process_button_just_pressed(CBA_SAT_RETURN, Player_controller_index))
                    {
                        camera_satellite_return_to_target();
                    }
                }
            }
            pp->pflags.passenger_brake = 0;
            if ((pp->common_controls_allowed & CCA_ON_FOOT) != 0)
            {
                weapon* wp = nullptr;
                bool skip_fire = false;
                // melee attacks
                if ((pp->controls_on_foot_allowed & CFA_MELEE_WEAPON) != 0)
                {
                    if (controls_is_button_just_pressed(CBA_OFC_ATTACK_PRIMARY, Player_controller_index))
                    {
                        human_melee_attack(pp, MELEE_ATTACK_PRIMARY, false, false);
                    }
                    else if (controls_is_button_just_pressed(CBA_OFC_ATTACK_MELEE, Player_controller_index))
                    {
                        human_melee_attack(pp, MELEE_ATTACK_TERTIARY, false, false);
                    }
                }
                if (!human_in_vehicle_interaction(pp, nullptr) && (pp->controls_on_foot_allowed & CFA_MELEE) != 0 && controls_process_button_just_pressed(CBA_OFC_ATTACK_MELEE, Player_controller_index))
                {
                    // a firearm's butt strike
                    if (pp->cur_state != HS_COVER)
                    {
                        human_melee_attack(pp, MELEE_ATTACK_FIREARM, false, false);
                    }
                }
                else if ((pp->controls_on_foot_allowed & CFA_ZOOM) != 0)
                {
                    // zoom (fine aim or scope)
                    controls_get_analog(CAA_CAMERA_ROTATE, Player_controller_index);
                    controls_get_analog(CAA_CAMERA_ELEVATE, Player_controller_index);
                    if (pp->cur_state != HS_COVER || human_is_leaning_from_cover(pp))
                    {
                        const bool in_vehicle = human_in_vehicle_interaction(pp, nullptr);
                        bool zoom = false;
                        if (use_zoom_hold(pp))
                        {
                            if (controls_is_button_down(in_vehicle ? CBA_VDC_TURRET_ZOOM : CBA_OFC_ZOOM, Player_controller_index))
                            {
                                if (!pp->pflags.no_fineaim && pp->get_equipped_weapon() != nullptr)
                                {
                                    zoom = true;
                                }
                            }
                            else if (player_is_zoomed(pp) && (pp->cur_state != HS_COVER || !Player_cover_force_fine_aim))
                            {
                                player_set_zoom_state(pp, PZS_OFF, false);
                            }
                        }
                        else if (controls_process_button_just_pressed(in_vehicle ? CBA_VDC_TURRET_ZOOM : CBA_OFC_ZOOM, Player_controller_index) && !pp->pflags.no_fineaim)
                        {
                            if (player_is_zoomed(pp))
                            {
                                player_set_zoom_state(pp, PZS_OFF, false);
                            }
                            else if (pp->get_equipped_weapon() != nullptr)
                            {
                                zoom = true;
                            }
                        }
                        if (zoom)
                        {
                            if (pp->get_equipped_weapon()->w_info->flags.can_scope)
                            {
                                player_set_zoom_state(pp, PZS_SCOPE, false);
                            }
                            else if (pp->get_equipped_weapon()->w_info->flags.can_fine_aim)
                            {
                                player_set_zoom_state(pp, PZS_FINE_AIM, false);
                                pp->hflags.moving_and_transitioning_states = 1;
                                pp->pflags.moving_to_a_stop = 1;
                            }
                        }
                    }
                }
                // jetpack and jump
                const bool can_move = wrecking_crew_can_player_move();
                bool jetpack = false;
                if ((pp->controls_on_foot_allowed & CFA_JUMP) != 0 && can_move && ((pp->backpack != nullptr && pp->backpack->get_backpack_info()->jetpack) || (!Multiplayer && !wrecking_crew_is_active() && upgrade_get_backpack_type() == UPGRADE_DEFINITION_JETPACK)))
                {
                    jetpack = controls_is_button_down(CBA_OFC_JUMP, Player_controller_index);
                    if ((human_is_in_air(pp) || Multiplayer || wrecking_crew_is_active()) && controls_is_button_down(CBA_GAC_TOGGLE_BACKPACK_ACTIVATION, Player_controller_index) && !pp->pflags.do_sprint)
                    {
                        jetpack = true;
                        const character_controller* cc = pp->rdd.is_loaded() ? pp->rdd.get_data()->controller : nullptr;
                        if ((pp->hflags.is_falling && !pp->hflags.is_on_ladder) || pp->movement_mode == HMM_JUMP || cc->flags.sliding)
                        {
                            pp->pflags.jetpack_ready = 1;
                        }
                    }
                }
                player_update_jetpack(pp, jetpack);
                if ((pp->controls_on_foot_allowed & CFA_JUMP) != 0)
                {
                    if (can_move && (controls_process_button_just_pressed(CBA_OFC_JUMP, Player_controller_index) || (controls_is_button_just_pressed(CBA_GAC_TOGGLE_BACKPACK_ACTIVATION, Player_controller_index) && jetpack)) && (!pp->last_supported.elapsed() || jetpack))
                    {
                        if (pp->cur_state == HS_COVER)
                        {
                            player_exit_cover(pp, true);
                        }
                        else
                        {
                            pp->pflags.crouching_state = 0;
                            human_jump(pp, false, false, false);
                        }
                    }
                    if ((pp->controls_on_foot_allowed & CFA_JUMP) != 0 && controls_is_button_just_pressed(CBA_OFC_COVER, Player_controller_index) && pp->entering_cover.elapsed())
                    {
                        pp->pflags.use_cover = 1;
                    }
                }
                // crouch (a toggle unless held)
                if (!Control_crouch_hold)
                {
                    if ((pp->controls_on_foot_allowed & CFA_CROUCH) != 0 && controls_is_button_just_pressed(CBA_OFC_CROUCH, Player_controller_index) && !maybe_ignore_movement_controls())
                    {
                        pp->pflags.crouching_state ^= 1;
                    }
                    const bool crouching = human_move_is_crouching(pp);
                    if (pp->pflags.crouching_state && !crouching)
                    {
                        human_move_start_crouching(pp);
                    }
                    else if (!pp->pflags.crouching_state && crouching)
                    {
                        human_move_stop_crouching(pp);
                    }
                }
                else if ((pp->controls_on_foot_allowed & CFA_CROUCH) == 0)
                {
                    human_move_stop_crouching(pp);
                }
                else if (controls_is_button_down(CBA_OFC_CROUCH, Player_controller_index) && !human_move_is_crouching(pp) && !maybe_ignore_movement_controls())
                {
                    human_move_start_crouching(pp);
                }
                else if (!controls_is_button_down(CBA_OFC_CROUCH, Player_controller_index) && human_move_is_crouching(pp))
                {
                    human_move_stop_crouching(pp);
                }
                if (human_move_is_crouching(pp) && human_is_jumping(pp))
                {
                    human_move_stop_crouching(pp);
                }
                // sprint
                wp = human_weapons_get_weapon(pp);
                pp->pflags.do_sprint = 0;
                if ((pp->controls_on_foot_allowed & CFA_SPRINT) != 0 && controls_is_button_down(CBA_OFC_SPRINT, Player_controller_index) && pp->entering_cover.elapsed() && (pp->pflags.do_sprint || (pp->cur_state != HS_FALL && pp->cur_state != HS_JUMP)) && !Multiplayer && !wrecking_crew_is_active() && !vanim_is_playing_action(pp->char_inst->vp, (animation_action)0x3DD))
                {
                    pp->pflags.do_sprint = 1;
                    pp->pflags.quick_turn = 0;
                    if (!pp->pflags.do_sprint)
                    {
                        // Original: dead code (do_sprint was just set).
                        pp->sprint_start_timestamp.set(0);
                    }
                    if (wp != nullptr)
                    {
                        wp->flags.delay_fire = false;
                        wp->one_shot_delay_timestamp.set(0);
                    }
                    if (pp->cur_state == HS_COVER)
                    {
                        player_exit_cover(pp, true);
                    }
                }
                // reload and fire
                if ((pp->controls_on_foot_allowed & CFA_FIRE) != 0 && controls_is_button_just_pressed(CBA_OFC_RELOAD, Player_controller_index) && wp != nullptr && wp->can_reload())
                {
                    human_weapons_firearm_reload(pp, true, nullptr);
                }
                if (wp != nullptr && (pp->controls_on_foot_allowed & CFA_FIRE) != 0 && pp->sprint_delay_timestamp.elapsed())
                {
                    const bool automatic = wp->w_info->trigger_type != WEAPON_TRIGGER_TYPE_SINGLE;
                    bool fire = (automatic && controls_is_button_down(CBA_OFC_ATTACK_PRIMARY, Player_controller_index)) || controls_is_button_just_pressed(CBA_OFC_ATTACK_PRIMARY, Player_controller_index) || wp->flags.delay_fire;
                    if (!fire && human_in_vehicle_interaction(pp, nullptr))
                    {
                        fire = (automatic && controls_is_button_down(CBA_VDC_FIREARM, Player_controller_index)) || controls_is_button_just_pressed(CBA_VDC_FIREARM, Player_controller_index) || wp->flags.delay_fire;
                    }
                    if (fire)
                    {
                        // A projectile that detonates in the air: the trigger detonates the player's flying one.
                        if (wp->w_info->ammo_type == WEAPON_AMMO_TYPE_PROJECTILE && (wp->w_info->projectile_info.flags & 0x10000) != 0)
                        {
                            for (int i = 0; i < World.get_num_objects<projectile>(); i++)
                            {
                                projectile* proj = World.get_object<projectile>(i);
                                if (proj->w_info == wp->w_info && pp->get_handle() == proj->owner_handle)
                                {
                                    multi_game_player_set_mp_flag(pp, PMPF_IN_AIR_DETONATE, 1);
                                    pp->mp_flags.set_in_air_detonate(1);
                                    if (proj->mid_air_detonate(nullptr, nullptr, nullptr))
                                    {
                                        skip_fire = true;
                                        break;
                                    }
                                }
                            }
                        }
                        if (!skip_fire)
                        {
                            wp->flags.delay_fire = false;
                            player_firearm_attack_start(false);
                            Camera_auto_level_timestamp.set(1000);
                            stop_primary_fire = false;
                        }
                    }
                }
                // throwing, and the empty-weapon handling of the attack button
                if (controls_is_button_just_pressed(CBA_OFC_ATTACK_PRIMARY, Player_controller_index) && pp->sprint_delay_timestamp.elapsed())
                {
                    if ((pp->controls_on_foot_allowed & CFA_THROW) != 0)
                    {
                        human_weapons_start_throw(pp, wp, nullptr, false);
                    }
                    else if (wp != nullptr)
                    {
                        if (wp->get_rounds_in_reserve() + wp->get_rounds_in_magazine() == 0 && !wp->has_unlimited_magazine_ammo() && wp->w_info != nullptr && wp->w_info->ammo_type == WEAPON_AMMO_TYPE_THROWN)
                        {
                            if (!player_weapon_autoswap(pp))
                            {
                                audiolib_cue_play(wp->w_info->no_ammo_sound, nullptr, nullptr);
                            }
                        }
                        else if (wp->w_info != nullptr && wp->w_info->ammo_type == WEAPON_AMMO_TYPE_THROWN && !can_fire_charge(pp->get_handle(), false))
                        {
                            audiolib_cue_play(wp->w_info->no_ammo_sound, nullptr, nullptr);
                        }
                    }
                }
                // a turret's secondary weapon
                turret* tp = human_combat_get_manned_turret(pp);
                weapon* secondary = tp != nullptr ? tp->get_secondary_weapon_inst() : nullptr;
                if (secondary != nullptr && (pp->controls_on_foot_allowed & CFA_MELEE) != 0 && ((secondary->w_info->trigger_type != WEAPON_TRIGGER_TYPE_SINGLE && controls_is_button_down(CBA_VDC_TANK_FIRE_SECONDARY, Player_controller_index)) || controls_is_button_just_pressed(CBA_VDC_TANK_FIRE_SECONDARY, Player_controller_index) || secondary->flags.delay_fire))
                {
                    secondary->flags.delay_fire = false;
                    player_firearm_attack_start(true);
                    Camera_auto_level_timestamp.set(1000);
                    stop_secondary_fire = false;
                }
                if (pp->vehicle_handle != 0xFFFFFFFF)
                {
                    pp->pflags.passenger_brake = controls_is_button_down(CBA_VDC_BRAKE_REVERSE, Player_controller_index) ? 1 : 0;
                }
            }
            else if ((pp->common_controls_allowed & CCA_DRIVING) != 0)
            {
                player_update_jetpack(pp, false);
                if (controls_is_button_just_pressed(CBA_VDC_CYCLE_CAMERA_MODE, Player_controller_index))
                {
                    camera_free_cycle_camera();
                }
                vehicle* vp = World.object_from_handle<vehicle>(pp->vehicle_handle);
                if (vp == nullptr)
                {
                    return;
                }
                if (gameplay_can_player_attack(nullptr, DBT_NONE))
                {
                    const bool tank_buttons = vehicle_is_tank(vp) || vp->get_sub_type() == 6;
                    if ((pp->controls_driving_allowed & 0x80) != 0)
                    {
                        if (controls_is_button_down(tank_buttons ? CBA_VDC_TANK_FIRE_PRIMARY : CBA_VDC_FIREARM, Player_controller_index))
                        {
                            if (human_combat_is_manning_turret(pp))
                            {
                                player_firearm_attack_start(false);
                            }
                            stop_primary_fire = false;
                        }
                        if ((pp->controls_driving_allowed & 0x80) != 0 && tank_buttons && controls_is_button_down(CBA_VDC_TANK_FIRE_SECONDARY, Player_controller_index) && human_combat_is_manning_turret(pp))
                        {
                            turret* tp = human_combat_get_manned_turret(pp);
                            if (tp != nullptr && tp->get_secondary_weapon_inst() != nullptr)
                            {
                                player_firearm_attack_start(true);
                                stop_secondary_fire = false;
                            }
                        }
                    }
                    if ((pp->controls_driving_allowed & 0x200) != 0)
                    {
                        walker* wkp = (walker*)vp;
                        if (controls_is_button_down(CBA_VDC_MECH_MELEE1, Player_controller_index))
                        {
                            walker_play_attack(wkp);
                        }
                        if (controls_is_button_down(CBA_VDC_MECH_MELEE2, Player_controller_index))
                        {
                            walker_play_attack2(wkp);
                        }
                        if (controls_is_button_down(CBA_VDC_MECH_MELEE3, Player_controller_index))
                        {
                            walker_play_attack3(wkp);
                        }
                        if (controls_is_button_down(CBA_VDC_MECH_MELEE4, Player_controller_index))
                        {
                            walker_play_attack4(wkp);
                        }
                        if (controls_is_button_down(CBA_VDC_MECH_CHARGE, Player_controller_index))
                        {
                            walker_charge(wkp);
                        }
                    }
                }
                if ((pp->controls_driving_allowed & 1) != 0)
                {
                    if (vp->get_sub_type() == 4)
                    {
                        automobile_steer((automobile*)vp, Stick_h, Stick_v);
                    }
                    else if (vp->get_sub_type() == 5)
                    {
                        flyer_steer((flyer*)vp);
                    }
                    else if (vp->get_sub_type() == 6)
                    {
                        walker_steer((walker*)vp, Stick_v, Stick_h);
                    }
                }
                if (vp->get_sub_type() == 4)
                {
                    // The car's handbrake, accelerator and brake. Permissions: 2 accelerate, 4 brake or reverse, 8 handbrake.
                    automobile* ap = World.object_cast<automobile>(vp);
                    const bool is_tank = vehicle_is_tank(ap);
                    const bool has_handbrake = automobile_has_handbrake(ap);
                    if (is_tank)
                    {
                        automobile_set_handbrake(ap, false);
                    }
                    else if ((pp->controls_driving_allowed & 8) != 0)
                    {
                        if (controls_is_button_down(CBA_VDC_HANDBRAKE, Player_controller_index))
                        {
                            automobile_set_handbrake(ap, true);
                            pp->mp_current_command.vcontrols.handbrake = true;
                        }
                        else
                        {
                            automobile_set_handbrake(ap, false);
                        }
                    }
                    const float accelerate = controls_get_button_analog(CBA_VDC_ACCELERATE, Player_controller_index);
                    const float brake = controls_get_button_analog(CBA_VDC_BRAKE_REVERSE, Player_controller_index);
                    const bool accelerating = (pp->controls_driving_allowed & 2) != 0 && accelerate >= Control_vehicle_analog_min;
                    const bool braking = (pp->controls_driving_allowed & 4) != 0 && brake >= Control_vehicle_analog_min;
                    vector vel;
                    havok_body_get_linear_velocity(ap->havok_handle, vel);
                    if (!has_handbrake && automobile_get_handbrake_on(ap) && !is_tank)
                    {
                        automobile_clear_accelerator(ap);
                        automobile_set_brake_pedal(ap);
                    }
                    else if (braking && accelerating && !is_tank)
                    {
                        // Both pedals: no accelerator, the brake is the pedal against the direction of travel, and only rises.
                        automobile_set_forward_or_reverse_accelerator(ap->get_handle(), 0.0f);
                        const float pedal = 0.0f <= ap->orient.fvec * vel ? brake : accelerate;
                        const float current = automobile_get_brake_value(ap->get_handle());
                        automobile_set_brake_pedal_input(ap->get_handle(), current - pedal >= 0.0f ? current : pedal);
                    }
                    else
                    {
                        // A tank always takes the stick; so does a car with neither pedal down that may use both.
                        float input = 0.0f;
                        if (is_tank)
                        {
                            input = Stick_a;
                        }
                        else if (accelerating)
                        {
                            input = accelerate;
                        }
                        else if (braking)
                        {
                            input = -brake;
                        }
                        else if ((pp->controls_driving_allowed & 2) != 0 && (pp->controls_driving_allowed & 4) != 0)
                        {
                            input = Stick_a;
                        }
                        if (automobile_get_handbrake_on(ap) && vel.mag_squared() < Control_handbrake_min_speed_sq)
                        {
                            input = 0.0f;
                        }
                        // The input curve keeps the sign (x87 in the binary).
                        automobile_set_forward_or_reverse_accelerator(ap->get_handle(), (float)((double)input * (double)input * (double)fl_sign(input)));
                    }
                }
            }
        }
        if (stop_primary_fire)
        {
            player_firearm_attack_stop(false);
        }
        if (stop_secondary_fire)
        {
            player_firearm_attack_stop(true);
        }
        if (do_movement && Interface_control_callbacks[Interface_mode].do_player_movement != nullptr)
        {
            Interface_control_callbacks[Interface_mode].do_player_movement();
        }
        else
        {
            process_player_normal_movement_internal(true);
        }
    }
    if (do_camera && Interface_control_callbacks[Interface_mode].do_camera_movement != nullptr)
    {
        Interface_control_callbacks[Interface_mode].do_camera_movement();
    }
}

// 0x006F7440 (:3087)
void control_gameplay_render()
{
}

// 0x006F7450 (:3124)
bool control_gameplay_general_controls_allowed()
{
    return (player_get_local_player()->common_controls_allowed & CCA_GENERAL) != 0;
}

// 0x006F7460 (:3129)
bool control_gameplay_on_foot_controls_allowed()
{
    return (player_get_local_player()->common_controls_allowed & CCA_ON_FOOT) != 0;
}

// 0x006F7480 (:3134)
bool control_gameplay_driving_controls_allowed()
{
    return (player_get_local_player()->common_controls_allowed & CCA_DRIVING) != 0;
}

// 0x006F74A0 (:3141)
void control_gameplay_reset_movement_controls()
{
    Player_normal_movement_last_h = 0.0f;
    Player_normal_movement_last_v = 0.0f;
    s_backpackButtonDownLastFrame = false;
    s_timeBackpackButtonPressedDown = 0;
}

// 0x006F74C0 (:3150)
void control_gameplay_clear_crouch()
{
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        human_move_stop_crouching(pp);
        if (pp->cur_stance != HST_CROUCH)
        {
            pp->pflags.crouching_state = 0;
        }
    }
}

// 0x006F74F0 (:3163)
// The movement destination: 5 m along the camera relative input direction.
void calculate_normal_movement(player* pp, float forward, float sideways, vector& xz_fvec, vector& xz_rvec, bool set_turnto_target, vector& new_dest_point)
{
    vector dir;
    if (forward == 0.0f && sideways == 0.0f)
    {
        dir = ZERO_VECTOR;
    }
    else
    {
        dir = xz_fvec * forward + xz_rvec * sideways;
    }
    if (dir.is_null())
    {
        dir = ZERO_VECTOR;
    }
    else
    {
        dir.normalize();
    }
    // Original: the results of these weapon queries are unused (a set_turnto_target branch compiled away).
    if (!pp->combat.combat_ready_timestamp.valid() || pp->combat.combat_ready_timestamp.elapsed())
    {
        if (!human_weapons_is_firearm_equipped(pp))
        {
            human_weapons_is_thrown_weapon_equipped(pp);
        }
    }
    new_dest_point = pp->pos + dir * 5.0f;
}

// 0x006F7710 (:3216)
void control_gameplay_enter()
{
}

// 0x006F7720 (:3220)
void control_gameplay_exit()
{
}

// 0x006F7730 (:3225)
control_button_action control_gameplay_get_action_button()
{
    player* pp = player_get_local_player();
    if (pp != nullptr && human_in_vehicle(pp) != nullptr && human_combat_get_manned_turret(pp) == nullptr)
    {
        return CBA_VDC_ACTION;
    }
    return CBA_OFC_ACTION;
}

// 0x006F7770 (:3249)
void control_enable_player_input(bool enable)
{
    Player_input_disabled = !enable;
}

// 0x006F7780 (:3254)
void control_enable_player_camera_input(bool enable)
{
    Player_camera_input_disabled = !enable;
}

// 0x006F7790 (:3259)
bool control_player_input_enabled()
{
    return !Player_input_disabled;
}

// 0x006F77A0 (:3264)
bool control_player_camera_input_enabled()
{
    return !Player_camera_input_disabled;
}

// 0x006FC3E0 (:1239)
// The player's walk input relative to the camera: the stick (or WASD) mapped to a circle, the charge backpack and
// sprinting forcing forward, the camera auto level, then the movement request.
static void process_player_normal_movement_internal(bool ignore_movement)
{
    player* pp = player_get_local_player();
    if (pp == nullptr || !pp->rdd.is_loaded())
    {
        return;
    }
    float h = controls_get_analog(CAA_WALK_TURN_LEFT_RIGHT, Player_controller_index);
    const float raw_v = controls_get_analog(CAA_WALK_FORWARD_BACKWARD, Player_controller_index);
    const float len_sq = (float)((double)h * (double)h + (double)raw_v * (double)raw_v);
    float v = raw_v;
    if (!(1.0f >= len_sq))
    {
        const float len = (float)sqrt((double)len_sq);
        h = h / len;
        v = raw_v / len;
    }
    if (maybe_ignore_movement_controls() || ignore_movement)
    {
        h = 0.0f;
        v = 0.0f;
    }
    if ((double)h * (double)h + (double)v * (double)v < (double)0.0001f)
    {
        h = 0.0f;
        v = 0.0f;
    }
    if (pp->ragdoll_state != HUMAN_RAGDOLL_STATE_NONE && pp->ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT)
    {
        return;
    }
    if (pp->backpack != nullptr && pp->backpack->get_backpack_abilities()->charge)
    {
        h = 0.0f;
        v = 1.0f;
    }
    if (pp->pflags.do_sprint && h == 0.0f && v == 0.0f)
    {
        // sprinting without input: on along the steering direction, relative to the camera
        if (pp->steering_vector.is_null())
        {
            h = 0.0f;
            v = 1.0f;
        }
        else
        {
            vector dir(pp->orient.fvec.x, 0.0f, pp->orient.fvec.z);
            dir.rotate_around_y_axis(-Rfg_camera.lookaround_params.heading);
            h = dir.x;
            v = dir.z;
        }
    }
    Stick_h = h;
    Stick_v = v;
    Player_normal_movement_last_v = v;
    Player_normal_movement_last_h = h;
    vector xz_fvec = Rfg_camera.ideal_orient.fvec.copy_flatten_normalize_safe();
    vector xz_rvec = Rfg_camera.ideal_orient.rvec.copy_flatten_normalize_safe();
    const bool stopping = h == 0.0f && v == 0.0f && !pp->hflags.block_forced_movement;
    if (Multiplayer)
    {
        pp->mp_current_command.forward = v;
        pp->mp_current_command.sideways = h;
        pp->mp_camera_orient = Rfg_camera.real_orient;
        pp->mp_camera_pos = Rfg_camera.real_pos;
        float p;
        float b;
        float hd;
        Rfg_camera.real_orient.extract_angles(p, b, hd);
        float last_p;
        float last_hd;
        Rfg_camera.last_orient.extract_angles(last_p, b, last_hd);
        pp->mp_camera_angular_velocity.x = (float)((double)(float)((double)p - (double)last_p) / (double)Frametime);
        pp->mp_camera_angular_velocity.y = (float)((double)(float)((double)hd - (double)last_hd) / (double)Frametime);
    }
    if (Camera_auto_level)
    {
        // Running straight on (the stick past 0.8) with the camera untouched levels the camera after a second.
        const float abs_v = (float)fabs((double)v);
        if ((double)0.8f > (double)abs_v || Rfg_camera.lookaround_params.rot_p != 0.0f || Rfg_camera.lookaround_params.rot_h != 0.0f)
        {
            Camera_auto_level_timestamp.set(1000);
        }
        else if (Camera_auto_level_timestamp.elapsed())
        {
            const float pitch = Rfg_camera.lookaround_params.pitch;
            Rfg_camera.lookaround_params.pitch = (float)((double)pitch - (double)Camera_auto_level_speed * (double)pitch * (double)abs_v * (double)Frametime_uncompressed);
        }
    }
    vector dest;
    calculate_normal_movement(pp, v, h, xz_fvec, xz_rvec, false, dest);
    float speed_sq = (float)((double)Stick_h * (double)Stick_h + (double)Stick_v * (double)Stick_v);
    if (!pp->sprint_delay_timestamp.elapsed())
    {
        speed_sq = 1.0f;
    }
    player_move_set_anim_direct(pp, h, v, pp->pflags.do_sprint, stopping);
    player_process_normal_movement(pp, dest, speed_sq);
}

// 0x006FC980 (:1349)
static void process_player_normal_movement()
{
    process_player_normal_movement_internal(false);
}

// The local player's network input in multiplayer: the movement and the camera transform and turn rate (inlined in
// the movement functions of the binary).
static void control_gameplay_store_mp_input(player* pp, float forward, float sideways)
{
    pp->mp_current_command.forward = forward;
    pp->mp_current_command.sideways = sideways;
    pp->mp_camera_orient = Rfg_camera.real_orient;
    pp->mp_camera_pos = Rfg_camera.real_pos;
    float p;
    float b;
    float hd;
    Rfg_camera.real_orient.extract_angles(p, b, hd);
    float last_p;
    float last_hd;
    Rfg_camera.last_orient.extract_angles(last_p, b, last_hd);
    pp->mp_camera_angular_velocity.x = (float)((double)(float)((double)p - (double)last_p) / (double)Frametime);
    pp->mp_camera_angular_velocity.y = (float)((double)(float)((double)hd - (double)last_hd) / (double)Frametime);
}

// 0x006FC990 (:1356)
// In cover: the walk stick (mapped into the unit circle) moves along the cover; none while the player may not move.
static void process_player_cover_movement()
{
    player* pp = player_get_local_player();
    if (!pp->rdd.is_loaded())
    {
        return;
    }
    float h = controls_get_analog(CAA_WALK_TURN_LEFT_RIGHT, Player_controller_index);
    float v = controls_get_analog(CAA_WALK_FORWARD_BACKWARD, Player_controller_index);
    const float len_sq = (float)((double)v * (double)v + (double)h * (double)h);
    if (1.0f < len_sq)
    {
        const float len = (float)sqrt((double)len_sq);
        h = h / len;
        v = v / len;
    }
    if (maybe_ignore_movement_controls()) // inlined in the binary
    {
        h = 0.0f;
        v = 0.0f;
    }
    if (Multiplayer)
    {
        control_gameplay_store_mp_input(pp, v, h);
    }
    player_process_cover_movement(pp, h, v);
}

// 0x006FCC00 (:1402)
// Driving: the vehicle's steer actions (a tank's own, a flyer and a walker steer with the walk stick) clamped to
// -1..1; no input at all keeps half of last frame's (fading out). Normal steering maps the stick onto the square;
// the tread controls (tank style, CAMERA_FREE_CONTROL_TYPE_TREAD and its camera relative variant) turn the stick
// direction into a turn and an accelerator through the side, steer and accelerator dead zones. The accelerator is
// rescaled past Driving_deadzone.
static void process_player_driving_movement()
{
    float h = 0.0f;
    float v = 0.0f;
    float a = 0.0f;
    player* pp = player_get_local_player();
    if (pp->vehicle_handle == 0xFFFFFFFF)
    {
        return;
    }
    object* objp = World.object_from_handle(pp->vehicle_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
    {
        return;
    }
    vehicle* vp = (vehicle*)objp;
    if (!maybe_ignore_movement_controls()) // inlined in the binary
    {
        const int sub_type = vp->get_sub_type();
        if (sub_type == OT_SUB_VEHICLE_AUTO)
        {
            if (vehicle_is_tank(vp))
            {
                h = controls_get_analog(CAA_TANK_STEER, Player_controller_index);
                v = controls_get_analog(CAA_TANK_STEER_VERT, Player_controller_index);
            }
            else
            {
                h = controls_get_analog(CAA_DRIVE_STEER, Player_controller_index);
                v = controls_get_analog(CAA_DRIVE_STEER_VERT, Player_controller_index);
            }
        }
        else if (sub_type == OT_SUB_VEHICLE_FLYER || sub_type == OT_SUB_VEHICLE_WALKER)
        {
            h = controls_get_analog(CAA_WALK_TURN_LEFT_RIGHT, Player_controller_index);
            v = controls_get_analog(CAA_WALK_FORWARD_BACKWARD, Player_controller_index);
        }
        else
        {
            VLIB_CRITICAL_FAILURE();
        }
        if ((float)((double)h - -1.0) < 0.0f)
        {
            h = -1.0f;
        }
        if ((float)((double)h - 1.0) >= 0.0f)
        {
            h = 1.0f;
        }
        if ((float)((double)v - -1.0) < 0.0f)
        {
            v = -1.0f;
        }
        if ((float)((double)v - 1.0) >= 0.0f)
        {
            v = 1.0f;
        }
        // The accelerator clamps the same way; it is always 0 here (no action feeds it).
        // Magic number: 0.1 (a float constant) is the dead zone of the fade-out.
        if ((double)fabsf(h) < (double)0.1f && (double)fabsf(v) < (double)0.1f && (double)fabsf(a) < (double)0.1f)
        {
            h = (float)((double)Player_normal_movement_last_h * 0.5);
            v = (float)((double)Player_normal_movement_last_v * 0.5);
            if ((double)fabsf(h) - (double)0.1f < 0.0)
            {
                h = 0.0f;
            }
            if ((double)fabsf(v) - (double)0.1f < 0.0)
            {
                v = 0.0f;
            }
            a = (float)((double)Player_normal_movement_last_a * 0.5);
            if ((double)fabsf(a) - (double)0.1f < 0.0)
            {
                a = 0.0f;
            }
        }
    }
    Player_normal_movement_last_a = a;
    Player_normal_movement_last_v = v;
    Player_normal_movement_last_h = h;
    if (Rfg_camera.free_params.control_type != CAMERA_FREE_CONTROL_TYPE_TREAD && Rfg_camera.free_params.control_type != CAMERA_FREE_CONTROL_TYPE_TREAD_CAM_REL)
    {
        controls_map_stick_to_square(h, v, h, v);
    }
    else
    {
        vector dir(h, 0.0f, v);
        const float len_sq = (float)((double)v * (double)v + (double)h * (double)h);
        if (1.0f < len_sq)
        {
            const float len = (float)sqrt((double)len_sq);
            dir.x = h / len;
            dir.z = v / len;
        }
        if (Rfg_camera.free_params.control_type == CAMERA_FREE_CONTROL_TYPE_TREAD_CAM_REL)
        {
            // the stick relative to the camera, turned into the vehicle's frame
            matrix cam_orient;
            cam_orient.make_quick(Rfg_camera.real_orient.fvec.copy_flatten_normalize());
            matrix veh_orient;
            veh_orient.make_quick(vp->orient.fvec.copy_flatten_normalize());
            dir.unrotate(cam_orient);
            dir.rotate(veh_orient);
        }
        float mag = dir.mag_and_normalize_safe();
        float turn = 0.0f;
        float accel = 0.0f;
        if ((double)0.1f <= (double)mag)
        {
            // the sign bit: a vehicle at -0 counts as reversing
            const float travel = signbit(vp->forward_velocity) ? -1.0f : 1.0f;
            const float side = (float)((double)RIGHT_VECTOR.y * (double)dir.y + (double)dir.x * (double)RIGHT_VECTOR.x + (double)RIGHT_VECTOR.z * (double)dir.z);
            if (Control_tread_side_zone_dot < side || side < -Control_tread_side_zone_dot)
            {
                // to the side: turn on the spot, moving the way the vehicle goes
                accel = travel * mag;
                turn = Control_tread_side_zone_dot < side ? 1.0f : -1.0f;
            }
            else
            {
                mag = fl_sign(dir.z) * mag;
                if (travel * mag <= 0.0f)
                {
                    // against the travel direction: brake, steering only outside the dead zones
                    if (side <= Control_tread_accl_dead_zone_dot)
                    {
                        if (side <= Control_tread_steer_dead_zone_dot)
                        {
                            if (-Control_tread_steer_dead_zone_dot < side)
                            {
                                turn = 0.0f;
                                accel = mag;
                            }
                            else if (side <= -Control_tread_accl_dead_zone_dot)
                            {
                                turn = fl_parameter(side, -Control_tread_side_zone_dot, -Control_tread_accl_dead_zone_dot, -1.0f, -Control_tread_dead_zone_turn_mag, true);
                                accel = 0.0f;
                            }
                            else
                            {
                                turn = fl_parameter(side, -Control_tread_accl_dead_zone_dot, -Control_tread_steer_dead_zone_dot, -Control_tread_dead_zone_turn_mag, 0.0f, true);
                                accel = mag;
                            }
                        }
                        else
                        {
                            turn = fl_parameter(side, Control_tread_steer_dead_zone_dot, Control_tread_accl_dead_zone_dot, 0.0f, Control_tread_dead_zone_turn_mag, true);
                            accel = mag;
                        }
                    }
                    else
                    {
                        turn = fl_parameter(side, Control_tread_accl_dead_zone_dot, Control_tread_side_zone_dot, Control_tread_dead_zone_turn_mag, 1.0f, true);
                        accel = 0.0f;
                    }
                }
                else
                {
                    turn = fl_parameter(side, -Control_tread_side_zone_dot, Control_tread_side_zone_dot, -1.0f, 1.0f, true);
                    accel = mag;
                }
            }
        }
        h = turn;
        v = accel;
        a = accel; // the tread path rescales its own accelerator below
    }
    if ((double)fabsf(a) - (double)0.1f < 0.0)
    {
        a = 0.0f;
    }
    if ((double)0.1f <= (double)fabsf(a))
    {
        const float sign = signbit(a) ? -1.0f : 1.0f;
        Stick_a = (float)(((double)a - (double)sign * (double)Driving_deadzone) / (1.0 - (double)Driving_deadzone));
    }
    else
    {
        Stick_a = 0.0f;
    }
    Stick_h = h;
    Stick_v = v;
    if (Multiplayer)
    {
        control_gameplay_store_mp_input(player_get_local_player(), v, h);
    }
}

// 0x00700B80 (:1578)
static void process_camera_movement_based_on_player_state()
{
    extern const control_callback_info Player_state_control_callbacks[17];
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        process_camera_normal_controls();
        return;
    }
    if (Player_state_control_callbacks[pp->cur_state].do_camera_movement != nullptr)
    {
        camera_free_set_swing_rate(pp->cur_movement_state == HMS_SPRINT ? 0.0f : 1.0f);
        Player_state_control_callbacks[pp->cur_state].do_camera_movement();
    }
    if (pp->cur_state == HS_VEHICLE_DRIVING && pp->vehicle_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(pp->vehicle_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE && !vehicle_is_tank((vehicle*)objp) && objp->get_sub_type() != 6)
        {
            Rfg_camera.free_params.flags.look_back = controls_is_button_down(CBA_VDC_LOOK_BACK, Player_controller_index);
        }
    }
}

// 0x006FD560 (:1604)
// LTCG: h in ESI, v in EDI, h_action in ECX, v_action on the stack. The camera input: the actions' analog values
// through the response curve |x|^1.9, clamped to -1..1; none with a locked controller or between multiplayer rounds.
static void process_camera_get_values(float& h, float& v, control_axis_action h_action, control_axis_action v_action)
{
    h = controls_get_analog(h_action, Player_controller_index);
    v = controls_get_analog(v_action, Player_controller_index);
    const int h_sign = h > 0.0f ? 1 : -1;
    h = (float)pow((double)fabsf(h), (double)Camera_first_person_input_exponent) * (float)h_sign; // x87 __CIpow, FIMUL
    const int v_sign = v > 0.0f ? 1 : -1;
    v = (float)pow((double)fabsf(v), (double)Camera_first_person_input_exponent) * (float)v_sign;
    if ((float)((double)h - -1.0) < 0.0f)
    {
        h = -1.0f;
    }
    if ((float)((double)h - 1.0) >= 0.0f)
    {
        h = 1.0f;
    }
    if ((float)((double)v - -1.0) < 0.0f)
    {
        v = -1.0f;
    }
    if ((float)((double)v - 1.0) >= 0.0f)
    {
        v = 1.0f;
    }
    player* pp = player_get_local_player();
    if ((Multiplayer && Multi_match_info.match_state != MGMS_GAMEPLAY) || (pp != nullptr && pp->hflags.locked_controller))
    {
        h = 0.0f;
        v = 0.0f;
    }
}

// The Steam controller's raw camera input, scaled and kept in -1..1 (shared by the free camera branches).
static void process_camera_get_steam_values(float& h, float& v, control_axis_action h_action, control_axis_action v_action)
{
    // 0.025: the Steam pad's raw axis units per camera input unit (x87 FMUL of a double constant)
    h = (float)((double)controls_get_raw_analog_uncapped(h_action, Player_controller_index) * (double)0.025f);
    v = (float)((double)controls_get_raw_analog_uncapped(v_action, Player_controller_index) * (double)0.025f);
    const float abs_h = fabsf(h);
    const float abs_v = fabsf(v);
    if (1.0f < abs_h || 1.0f < abs_v)
    {
        const float m = abs_h - abs_v < 0.0f ? abs_v : abs_h;
        h = h / m;
        v = v / m;
    }
}

// 0x006FD6D0 (:1650)
// The camera input of the walking, jumping and falling states, by camera mode.
static void process_camera_normal_controls()
{
    player* pp = player_get_local_player();
    if (pp != nullptr && pp->backpack != nullptr && pp->backpack->get_backpack_abilities()->charge)
    {
        return;
    }
    switch (Rfg_camera.mode)
    {
    case CAMERA_FREE_MODE:
    {
        control_axis_action h_action = CAA_CAMERA_ROTATE;
        control_axis_action v_action = CAA_CAMERA_ELEVATE;
        if (pp != nullptr && pp->vehicle_handle != 0xFFFFFFFF)
        {
            object* objp = World.object_from_handle(pp->vehicle_handle);
            if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
            {
                process_camera_get_actions_for_vehicle(h_action, v_action, (vehicle*)objp);
            }
        }
        float h;
        float v;
        process_camera_get_values(h, v, h_action, v_action);
        if (controls_get_current_config_setting() == CCS_STEAM)
        {
            process_camera_get_steam_values(h, v, h_action, v_action); // inlined in the binary
        }
        camera_free_set_rotation(-h, h_action);
        camera_free_set_elevation(v, v_action);
        return;
    }
    case CAMERA_LOOKAROUND_MODE:
    case CAMERA_FIRST_PERSON_MODE:
    case CAMERA_THIRD_PERSON_MODE:
    {
        float h;
        float v;
        process_camera_get_values(h, v, CAA_CAMERA_ROTATE, CAA_CAMERA_ELEVATE);
        camera_lookaround_set_rotation(-v, h);
        return;
    }
    case CAMERA_TURRET_MODE:
    {
        control_axis_action h_action = CAA_CAMERA_ROTATE;
        control_axis_action v_action = CAA_CAMERA_ELEVATE;
        if (pp != nullptr && pp->vehicle_handle != 0xFFFFFFFF)
        {
            object* objp = World.object_from_handle(pp->vehicle_handle);
            if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
            {
                process_camera_get_actions_for_vehicle(h_action, v_action, (vehicle*)objp);
            }
        }
        float h;
        float v;
        process_camera_get_values(h, v, h_action, v_action);
        if (controls_get_current_config_setting() == CCS_STEAM)
        {
            h = controls_get_raw_analog_uncapped(h_action, Player_controller_index);
            v = controls_get_raw_analog_uncapped(v_action, Player_controller_index);
        }
        camera_turret_set_rotation(-v, h, h_action, v_action);
        return;
    }
    default:
        return;
    }
}

// 0x006FD9B0 (:1745)
// The driver's camera: the free camera turns at the submode's rotation speed (a third of the input except in a
// walker), the turret mode turns the vehicle's turret.
static void process_camera_driving_controls()
{
    player* pp = player_get_local_player();
    if (pp == nullptr || pp->vehicle_handle == 0xFFFFFFFF)
    {
        return;
    }
    object* objp = World.object_from_handle(pp->vehicle_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
    {
        return;
    }
    vehicle* vp = (vehicle*)objp;
    float h = 0.0f;
    float v = 0.0f;
    player* lp = player_get_local_player();
    if ((Multiplayer && Multi_match_info.match_state != MGMS_GAMEPLAY) || (lp != nullptr && lp->hflags.locked_controller))
    {
        return;
    }
    control_axis_action h_action = CAA_CAMERA_ROTATE;
    control_axis_action v_action = CAA_CAMERA_ELEVATE;
    if (Rfg_camera.mode == CAMERA_FREE_MODE)
    {
        process_camera_get_actions_for_vehicle(h_action, v_action, vp);
        process_camera_get_values(h, v, h_action, v_action);
        if (controls_get_current_config_setting() == CCS_STEAM)
        {
            process_camera_get_steam_values(h, v, h_action, v_action); // inlined in the binary
        }
        if (vp->get_sub_type() != OT_SUB_VEHICLE_WALKER)
        {
            h = (float)((double)h * (double)0.3f); // Magic number: 0.3 (a float constant)
        }
        camera_free_set_rotation(-(Rfg_camera.free_params.submode_params->y_axis_rotation_speed * h), h_action);
        camera_free_set_elevation(v, v_action);
    }
    else if (Rfg_camera.mode == CAMERA_TURRET_MODE)
    {
        process_camera_get_actions_for_vehicle(h_action, v_action, vp);
        process_camera_get_values(h, v, h_action, v_action);
        if (controls_get_current_config_setting() == CCS_STEAM)
        {
            h = controls_get_raw_analog_uncapped(h_action, Player_controller_index);
            v = controls_get_raw_analog_uncapped(v_action, Player_controller_index);
        }
        camera_turret_set_rotation(-v, h, h_action, v_action);
    }
    else
    {
        wrecking_crew_is_active(); // the result is unused in rfg.exe
    }
}

// 0x006FDC60 (:1821)
// The satellite view: the camera stick (inverted as the axis is), or the walk stick when the camera stick is idle.
static void process_camera_satellite_controls()
{
    control_axis_action h_action = CAA_CAMERA_ROTATE;
    control_axis_action v_action = CAA_CAMERA_ELEVATE;
    float h;
    float v;
    player* pp = player_get_local_player();
    if ((Multiplayer && Multi_match_info.match_state != MGMS_GAMEPLAY) || (pp != nullptr && pp->hflags.locked_controller))
    {
        v = 0.0f;
        h = 0.0f;
    }
    else
    {
        h = controls_get_analog(CAA_CAMERA_ROTATE, Player_controller_index);
        v = controls_get_analog(CAA_CAMERA_ELEVATE, Player_controller_index);
        if (Player_controller_index == 0)
        {
            const control_config& config = Control_config[0][controls_get_current_config_setting()];
            if (config.axis_bindings[CAA_CAMERA_ELEVATE].axis[0] != 0xFFFFFFFF && config.axis_bindings[CAA_CAMERA_ELEVATE].inverted[0])
            {
                v = -v;
            }
        }
        // Magic number: 0.1 (a float constant), the idle zone of the camera stick.
        if ((double)fabsf(h) <= (double)0.1f && (double)fabsf(v) <= (double)0.1f)
        {
            h_action = CAA_WALK_TURN_LEFT_RIGHT;
            v_action = CAA_WALK_FORWARD_BACKWARD;
            h = controls_get_analog(CAA_WALK_TURN_LEFT_RIGHT, Player_controller_index);
            v = controls_get_analog(CAA_WALK_FORWARD_BACKWARD, Player_controller_index);
        }
    }
    camera_satellite_control(v, h, h_action, v_action);
}

// 0x006FDDB0 (:1856)
static void process_camera_cover_controls()
{
    process_camera_normal_controls();
}

// 0x006FDDC0 (:1876)
// Zoom is held (not toggled) when the option says so or the zoom is on a trigger or shoulder button (4-7).
static bool use_zoom_hold(player* client_pp)
{
    const int button = controls_get_button_for_action(CBA_OFC_ZOOM, Player_controller_index);
    return Control_zoom_hold || button == 6 || button == 7 || button == 4 || button == 5;
}

// ---- file-static tables (after the functions they point to)
// 0x012D1D20, rfg.exe .rdata (dumped in player_minimal.md P6). Indexed by interface_modes: the normal, racing standing
// and PDA... modes control the player through its state; the race modes have their own permissions.
extern const control_callback_info Interface_control_callbacks[NUM_INT_MODES] = {
    {set_control_permissions_based_on_player_state, process_player_movement_based_on_player_state, process_camera_movement_based_on_player_state}, // INT_MODE_NORMAL
    {nullptr, nullptr, nullptr}, // INT_MODE_NO_CONTROL
    {set_control_permissions_based_on_player_state, process_player_movement_based_on_player_state, nullptr}, // INT_MODE_MULTIPLAYER_REMOTE_CLIENT
    {set_control_permissions_based_on_player_state, process_player_movement_based_on_player_state, process_camera_movement_based_on_player_state}, // INT_MODE_PDA
    {set_control_permissions_pre_race_in_vehicle, nullptr, nullptr}, // INT_MODE_PRE_RACING_IN_VEHICLE
    {set_control_permissions_racing_without_combat_in_vehicle, process_player_movement_based_on_player_state, process_camera_movement_based_on_player_state}, // INT_MODE_RACING_WO_COMBAT_IN_VEHICLE
    {set_control_permissions_racing_without_combat_stand, process_player_movement_based_on_player_state, process_camera_movement_based_on_player_state}, // INT_MODE_RACING_WO_COMBAT_STANDING
    {set_control_permissions_multi_pre_race, nullptr, process_camera_movement_based_on_player_state}, // INT_MODE_MULTI_PRE_RACING
    {set_control_permissions_multi_racing, process_player_movement_based_on_player_state, process_camera_movement_based_on_player_state}, // INT_MODE_MULTI_RACING
    {set_control_permissions_multi_racing_gunner, process_player_movement_based_on_player_state, process_camera_movement_based_on_player_state}, // INT_MODE_MULTI_RACING_GUNNER
};

// 0x012D1D98, rfg.exe .rdata (dumped in player_minimal.md P0). Indexed by the player's state (human_states); the
// player minimal scope uses entries 0-2 (standing, jumping, falling).
extern const control_callback_info Player_state_control_callbacks[17] = {
    {set_control_permissions_stand, process_player_normal_movement, process_camera_normal_controls}, // 0
    {set_control_permissions_jump, process_player_normal_movement, process_camera_normal_controls}, // 1
    {set_control_permissions_fall, process_player_normal_movement, process_camera_normal_controls}, // 2
    {set_control_permissions_in_turret, nullptr, process_camera_normal_controls}, // 3
    {set_control_permissions_when_dead, nullptr, nullptr}, // 4
    {set_control_permissions_on_ladder, process_player_ladder_movement, process_camera_normal_controls}, // 5
    {set_control_permissions_player_pathing, nullptr, process_camera_normal_controls}, // 6
    {set_control_permissions_in_vehicle_transition, nullptr, process_camera_normal_controls}, // 7
    {set_control_permissions_in_vehicle, process_player_normal_movement, process_camera_normal_controls}, // 8
    {set_control_permissions_in_vehicle, process_player_driving_movement, process_camera_driving_controls}, // 9
    {set_control_permissions_in_vehicle_transition, nullptr, process_camera_normal_controls}, // 10
    {set_control_permissions_spectator, nullptr, process_camera_normal_controls}, // 11
    {set_control_permissions_stand, process_player_cover_movement, process_camera_cover_controls}, // 12
    {set_control_permissions_using_ammo_box, nullptr, process_camera_normal_controls}, // 13
    {set_control_permissions_defusing, nullptr, process_camera_normal_controls}, // 14
    {set_control_permissions_action_node, nullptr, process_camera_normal_controls}, // 15
    {set_control_permissions_satellite, nullptr, process_camera_satellite_controls}, // 16
};
