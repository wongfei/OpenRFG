// Original: volition/rfg/code/objects/humans/human_combat_turret.cpp
//
// A human's use of turrets (header comment in human_combat_turret.h). A human's combat.reserved_turret names the turret
// it reserved or mans; the turret's current_operator says who mans it. Compiled in rfg_objects_unity.cpp.

#include "volition/rfg/code/objects/humans/human_combat_turret.h"

#include "volition/rfg/code/camera/camera_free.h"
#include "volition/rfg/code/camera/camera_view.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/interface/ui_document_hud_rail_driver.h"
#include "volition/rfg/code/interface/ui_document_hud_sniper.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/objects/turret_info.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"

// Magic numbers: how far the operator stands behind the turret.
static const float TURRET_OPERATOR_DISTANCE = 0.75f;
static const int TURRET_ABANDON_HOLD_MS = 200; // the operator keeps its turret pose this long after leaving

// The human using a turret, nullptr unless the handle is a human (inlined in the original).
static human* turret_operator_human(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    return objp != nullptr && objp->get_obj_type() == OT_HUMAN ? (human*)objp : nullptr;
}

// A turret that still works and has ammo (inlined in the original; the same test as turret::is_functional(false)).
static bool turret_has_loaded_weapon(const turret* tp)
{
    return tp->is_functional(false);
}

// 0x00A55640 (:42)
turret* human_combat_get_reserved_turret(const human* hp)
{
    if (hp->combat.reserved_turret == 0xFFFFFFFF)
    {
        return nullptr;
    }
    turret* tp = World.object_from_handle<turret>(hp->combat.reserved_turret);
    if (tp == nullptr)
    {
        return nullptr;
    }
    const human* op = turret_operator_human(tp->current_operator);
    if (op != nullptr && op != hp)
    {
        return nullptr;
    }
    return turret_has_loaded_weapon(tp) ? tp : nullptr;
}

// 0x00A556F0 (:64)
turret* human_combat_get_manned_turret(const human* hp)
{
    if (hp->combat.reserved_turret == 0xFFFFFFFF)
    {
        return nullptr;
    }
    turret* tp = World.object_from_handle<turret>(hp->combat.reserved_turret);
    if (tp == nullptr || turret_operator_human(tp->current_operator) != hp)
    {
        return nullptr;
    }
    return tp;
}

// 0x00A55750 (:89): vehicle and automated turrets cannot be taken over.
bool human_combat_can_man_turret(const human* hp, const turret* tp)
{
    if (!turret_has_loaded_weapon(tp) || tp->turret_flags.vehicle_turret || tp->t_info->flags.automated)
    {
        return false;
    }
    const human* op = turret_operator_human(tp->current_operator);
    return op == nullptr || op == hp;
}

// 0x00A2FB00 (:112)
human* human_combat_turret_get_operator(const turret* tp)
{
    return turret_operator_human(tp->current_operator);
}

// 0x00A557F0 (:157): the operator, else the first live human that reserved the turret.
human* human_combat_turret_get_reserving_human(const turret* tp)
{
    human* op = turret_operator_human(tp->current_operator);
    if (op != nullptr)
    {
        return op;
    }
    const int num = World.get_num_objects<human>();
    for (int i = 0; i < num; i++)
    {
        human* hp = World.get_object<human>(i);
        if (hp != nullptr && !hp->is_killed() && hp->combat.reserved_turret == tp->get_handle())
        {
            return hp;
        }
    }
    return nullptr;
}

// 0x00AC8570 (:188)
bool human_combat_reserve_turret(human* hp, turret* tp)
{
    if (hp->combat.reserved_turret != tp->get_handle())
    {
        human_combat_abandon_turret(hp);
    }
    human* reserving = human_combat_turret_get_reserving_human(tp);
    if (reserving != nullptr && reserving != hp)
    {
        return false;
    }
    hp->combat.reserved_turret = tp->get_handle();
    return true;
}

// 0x00AB6220 (:213): a player takes the turret from an NPC that reserved it, and precaches its weapons' cues when it
// was free or a player's; an NPC with a weapon still in hand answers false.
bool human_combat_man_turret(human* hp, turret* tp)
{
    if (hp == nullptr || tp == nullptr || hp->cur_state == HS_JUMP || hp->cur_state == HS_FALL)
    {
        return false;
    }
    if (hp->combat.reserved_turret != tp->get_handle())
    {
        human_combat_abandon_turret(hp);
    }
    // A player can sit at an empty vehicle turret.
    if (!tp->is_functional(hp->get_sub_type() == OT_SUB_HUMAN_PLAYER && tp->turret_flags.vehicle_turret))
    {
        return false;
    }
    human* op = human_combat_turret_get_operator(tp);
    if (op != nullptr)
    {
        return op == hp;
    }
    const bool vehicle_turret = tp->turret_flags.vehicle_turret;
    player* pp = World.object_cast<player>(hp);
    if (pp != nullptr)
    {
        human* reserving = human_combat_turret_get_reserving_human(tp);
        if (reserving == nullptr || World.object_cast<player>(reserving) != nullptr)
        {
            weapon* wp = tp->get_weapon_inst();
            if (wp != nullptr)
            {
                wp->precache_audio_cues();
            }
            wp = tp->get_secondary_weapon_inst();
            if (wp != nullptr)
            {
                wp->precache_audio_cues();
            }
        }
        else
        {
            human_combat_abandon_turret(reserving);
        }
    }
    hp->combat.reserved_turret = tp->get_handle();
    if (!vehicle_turret)
    {
        human_controller_set_vel(hp, ZERO_VECTOR);
    }
    tp->attach_new_operator(hp);
    if (pp == nullptr)
    {
        if (!vehicle_turret && hp->equipped_inv_item != nullptr)
        {
            return false;
        }
    }
    else
    {
        human_unequip_inventory_item(hp);
        hp->set_equipped_inv_item(nullptr);
        if (!vehicle_turret && object_is_local_player(hp))
        {
            camera_start_turret_mode(true);
        }
        if (tp->t_info->flags.center_on_player_entry)
        {
            Rfg_camera.lookaround_params.heading = tp->orient.extract_heading();
            Rfg_camera.real_orient.set(Rfg_camera.lookaround_params.pitch, 0.0f, Rfg_camera.lookaround_params.heading);
        }
    }
    return true;
}

// 0x00AC0B20 (:346)
void human_combat_abandon_turret(human* hp)
{
    if (hp == nullptr)
    {
        return;
    }
    turret* tp = human_combat_get_manned_turret(hp);
    if (tp != nullptr)
    {
        player* pp = nullptr;
        if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            pp = (player*)hp;
            // Zoomed in: leave the zoom, unless the turret forces fine aim. The original reads the manned turret again.
            if (pp->zoom_state != PZS_OFF && !tp->t_info->flags.force_fine_aim)
            {
                const player_zoom_state previous = pp->zoom_state;
                pp->previous_zoom_state = previous;
                pp->zoom_state = PZS_OFF;
                if (pp == Player)
                {
                    pp->get_equipped_weapon(); // Original: the result is not used
                    if (previous == PZS_SCOPE)
                    {
                        pp->set_opacity_plus_child_items(1.0f);
                        pp->set_casts_transparent_shadows_plus_child_items(false);
                        hud_ui_rail_driver_set_visible(false);
                        hud_ui_sniper_set_visible(false);
                    }
                    camera_set_view_player(PZS_OFF, false);
                }
            }
        }

        tp->turret_flags.walking_right = false;
        tp->turret_flags.walking_left = false;
        const bool vehicle_turret = tp->turret_flags.vehicle_turret;
        if (vehicle_turret)
        {
            v_animate* vp = hp->char_inst->vp;
            vanim_set_state_blend_pose(vp, index_alias(ANIMATION_STATE_WALK_LEFT, (vanim_aliases)-1), 0.0f, 0);
            vanim_set_state_blend_pose(hp->char_inst->vp, index_alias(ANIMATION_STATE_TURRET_LEFT, (vanim_aliases)-1), 0.0f, 0);
            vanim_set_state_blend_pose(hp->char_inst->vp, index_alias(ANIMATION_STATE_TURRET_SLIGHT_LEFT, (vanim_aliases)-1), 0.0f, 0);
            vanim_set_state_blend_pose(hp->char_inst->vp, index_alias(ANIMATION_STATE_TURRET_MIDDLE, (vanim_aliases)-1), 0.0f, 0);
            vanim_set_state_blend_pose(hp->char_inst->vp, index_alias(ANIMATION_STATE_TURRET_SLIGHT_RIGHT, (vanim_aliases)-1), 0.0f, 0);
            vanim_set_state_blend_pose(hp->char_inst->vp, index_alias(ANIMATION_STATE_TURRET_RIGHT, (vanim_aliases)-1), 0.0f, 0);
            vanim_set_state_blend_pose(hp->char_inst->vp, index_alias(ANIMATION_STATE_WALK_RIGHT, (vanim_aliases)-1), 0.0f, 0);
        }
        else
        {
            hp->turret_hold_animations.set(TURRET_ABANDON_HOLD_MS);
            hp->move_speed = -1.0f;
            hp->cur_state = HS_STANDARD;
            hp->cur_movement_state = HMS_IDLE;
            hp->cur_stance = HST_STAND;
        }
        if (pp != nullptr && !vehicle_turret)
        {
            // 0x1FD with alias 0x3DB: the step back from the turret; it blocks movement while it plays.
            vanim_play_action(hp->char_inst->vp, index_alias(ANIMATION_ACTION_UNEQUIP, ANIM_ALIAS_THROW), 0x4020, 1.0f, 1.0f);
            hp->block_steering_vector = hp->orient.fvec;
            hp->block_movement_anim = ANIM_ALIAS_THROW;
            hp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
            hp->block_steering_heading_offset = 0.0f;
        }
        hp->set_equipped_inv_item(nullptr);
        tp->detach_operator(hp);
        if (pp != nullptr)
        {
            camera_stop_turret_mode(true);
            if (!vehicle_turret)
            {
                weapon* wp = tp->get_weapon_inst();
                if (wp != nullptr)
                {
                    float pitch;
                    float bank;
                    float heading;
                    wp->orient.extract_angles(pitch, bank, heading);
                    Rfg_camera.lookaround_params.heading = heading;
                    Rfg_camera.lookaround_params.pitch = pitch;
                }
                human_equip_inventory_item(hp, hp->last_equipped_weapon);
            }
            else if (hp->vehicle_handle != 0xFFFFFFFF)
            {
                object* objp = World.object_from_handle(hp->vehicle_handle);
                if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
                {
                    camera_free_set_target(objp);
                }
            }
            human_set_combat_ready(hp, true, 0);
        }
        if (!vehicle_turret)
        {
            vector dir = hp->orient.fvec;
            dir.y = 0.0f;
            dir.normalize_safe();
            hp->orient.make(dir);
        }
    }

    hp->combat.reserved_turret = 0xFFFFFFFF;
    if (tp == nullptr)
    {
        return;
    }
    if (tp->turret_flags.vehicle_turret)
    {
        // The driver takes the gun back when the gunner leaves.
        vehicle* vp = World.object_from_handle<vehicle>(tp->get_parent());
        player* local = Player;
        if (hp != local && vehicle_human_in_vehicle(vp, local) && local->vehicle_seat_idx == VSI_DRIVER
            && !human_in_vehicle_transition(local, nullptr, VI_INTERACTION_NONE))
        {
            human_combat_man_turret(local, tp);
        }
    }
}

// 0x00A55880 (:474): an NPC walks to the operation position first, in the equip action.
void human_combat_turret_do_frame_pre(turret* tp)
{
    human* hp = turret_operator_human(tp->current_operator);
    weapon* wp = hp != nullptr ? tp->get_weapon_inst() : nullptr;
    if (wp == nullptr)
    {
        return;
    }
    if (!tp->turret_flags.vehicle_turret && hp->cur_state != HS_TURRET && !hp->char_inst->is_playing_action(ANIM_ALIAS_EQUIP))
    {
        hp->char_inst->set_animation_group(wp->w_info->weapon_anim_group, index_alias(-1, (vanim_aliases)-1), 0.3f, false);
        hp->char_inst->play_action(ANIMATION_ACTION_EQUIP, ANIM_ALIAS_ENTER, 1.0f, 0x4040);
        vector muzzle_pos;
        matrix muzzle_orient;
        wp->get_muzzle_transform(muzzle_pos, &muzzle_orient);
        const vector dir = muzzle_orient.fvec.copy_flatten_normalize_safe();
        matrix dest_orient;
        dest_orient.make(dir);
        vector world_pos;
        world_pos.x = tp->pos.x - dir.x * TURRET_OPERATOR_DISTANCE;
        world_pos.y = tp->pos.y - dir.y * TURRET_OPERATOR_DISTANCE;
        world_pos.z = tp->pos.z - dir.z * TURRET_OPERATOR_DISTANCE;
        hp->char_inst->set_action_dest_position(hp->pos, hp->orient, world_pos, ANIM_ALIAS_ENTER, -1.0f, false);
        hp->char_inst->set_action_dest_orient(hp->orient, dest_orient, ANIM_ALIAS_ENTER, 0.25f);
        hp->move_speed = -1.0f;
        hp->cur_state = HS_TURRET;
        hp->cur_movement_state = HMS_IDLE;
        return;
    }
    if (hp->cur_state != HS_VEHICLE_DRIVING)
    {
        const animation_group group = wp->w_info->weapon_anim_group;
        if (hp->char_inst->get_animation_group() != group)
        {
            hp->char_inst->set_animation_group(group, index_alias(-1, (vanim_aliases)-1), 0.3f, false);
        }
    }
}

// 0x00A2FB30 (:516): keeps a standing operator at its place behind the turret, facing it.
void human_combat_turret_do_frame_post(turret* tp)
{
    if (tp->turret_flags.vehicle_turret)
    {
        return;
    }
    human* hp = turret_operator_human(tp->current_operator);
    if (hp == nullptr || hp->char_inst->is_playing_action(ANIM_ALIAS_ENTER) || hp->char_inst->is_playing_action(ANIM_ALIAS_EQUIP))
    {
        return;
    }
    vector to_turret;
    to_turret.x = tp->pos.x - hp->pos.x;
    to_turret.y = tp->pos.y - hp->pos.y;
    to_turret.z = tp->pos.z - hp->pos.z;
    const vector dir = to_turret.copy_flatten_normalize_safe();
    hp->hflags.use_current_velocity = true;
    hp->velocity.x = (tp->pos.x - dir.x * TURRET_OPERATOR_DISTANCE - hp->pos.x) / Frametime;
    hp->velocity.y = 0.0f;
    hp->velocity.z = (tp->pos.z - dir.z * TURRET_OPERATOR_DISTANCE - hp->pos.z) / Frametime;
    hp->orient.make(dir);
}
